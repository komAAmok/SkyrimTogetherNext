
#include <Services/TransportService.h>

#include <Events/ConnectedEvent.h>
#include <Events/ConnectionErrorEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/UpdateEvent.h>

#include <Games/References.h>
#include <Games/TES.h>
#include <Forms/TESWorldSpace.h>
#include <Forms/TESObjectCELL.h>

#include <TimeManager.h>

#include <Forms/TESNPC.h>
#include <TiltedOnlinePCH.h>
#include <World.h>

#include <Messages/AuthenticationRequest.h>
#include <Messages/ServerMessageFactory.h>
#include <Messages/NotifySettingsChange.h>
#include <Packet.hpp>

#include <ScriptExtender.h>
#include <Services/DiscordService.h>

#include <chrono>

// #include <imgui_internal.h>

static constexpr wchar_t kMO2DllName[] = L"usvfs_x64.dll";

// How long the client waits for the whole handshake (name resolution, transport
// connect, clock sync, authentication response) before giving up and telling
// the UI. A server on the local network answers in well under a second; ten
// seconds is generous enough that a slow VPN does not trip it, and short enough
// that a wrong address does not look like a freeze.
static constexpr std::chrono::seconds kHandshakeTimeout{10};

using TiltedPhoques::Packet;

TransportService::TransportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&TransportService::HandleUpdate>(this);
    m_settingsChangeConnection = m_dispatcher.sink<NotifySettingsChange>().connect<&TransportService::HandleNotifySettingsChange>(this);
    m_connectedConnection = m_dispatcher.sink<ConnectedEvent>().connect<&TransportService::HandleConnected>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&TransportService::HandleDisconnected>(this);

    m_connected = false;
    m_localPlayerId = NULL;

    auto handlerGenerator = [this](auto& x)
    {
        using T = typename std::remove_reference_t<decltype(x)>::Type;

        m_messageHandlers[T::Opcode] = [this](UniquePtr<ServerMessage>& apMessage)
        {
            const auto pRealMessage = TiltedPhoques::CastUnique<T>(std::move(apMessage));
            m_dispatcher.trigger(*pRealMessage);
        };

        return false;
    };

    ServerMessageFactory::Visit(handlerGenerator);

    // Override authentication response
    m_messageHandlers[AuthenticationResponse::Opcode] = [this](UniquePtr<ServerMessage>& apMessage)
    {
        const auto pRealMessage = TiltedPhoques::CastUnique<AuthenticationResponse>(std::move(apMessage));
        HandleAuthenticationResponse(*pRealMessage);
    };
}

bool TransportService::Send(const ClientMessage& acMessage) const noexcept
{
    static thread_local ScratchAllocator s_allocator(1 << 18);

    struct ScopedReset
    {
        ~ScopedReset() { s_allocator.Reset(); }
    } allocatorGuard;

    if (IsConnected())
    {
        ScopedAllocator _{s_allocator};

        Buffer buffer(1 << 16);
        Buffer::Writer writer(&buffer);
        writer.WriteBits(0, 8); // Write first byte as packet needs it

        acMessage.Serialize(writer);
        TiltedPhoques::PacketView packet(reinterpret_cast<char*>(buffer.GetWriteData()), writer.Size());

        Client::Send(&packet);

        return true;
    }

    return false;
}

void TransportService::OnConsume(const void* apData, uint32_t aSize)
{
    ServerMessageFactory factory;
    TiltedPhoques::ViewBuffer buf((uint8_t*)apData, aSize);
    Buffer::Reader reader(&buf);

    auto pMessage = factory.Extract(reader);
    if (!pMessage)
    {
        spdlog::error("Couldn't parse packet from server");
        return;
    }

    m_messageHandlers[pMessage->GetOpcode()](pMessage);
}

void TransportService::ArmConnectionWatchdog(const std::string& acEndpoint) noexcept
{
    m_handshakePending = true;
    m_handshakeDeadline = std::chrono::steady_clock::now() + kHandshakeTimeout;
    m_pendingEndpoint = acEndpoint;
    m_attemptOutcomeReported = false;

    spdlog::info("connecting to {}, giving up after {}s if the handshake does not complete",
                 acEndpoint, kHandshakeTimeout.count());
}

void TransportService::OnConnected()
{
    AuthenticationRequest request{};
    request.Version = BUILD_COMMIT;
    request.SKSEActive = IsScriptExtenderLoaded();
    request.MO2Active = GetModuleHandleW(kMO2DllName);

    request.Token = m_serverPassword;
    m_serverPassword = "";

    PlayerCharacter* pPlayer = PlayerCharacter::Get();

    // null if discord is not active
    // TODO: think about user opt out
    request.DiscordId = m_world.ctx().at<DiscordService>().GetUser().id;
    auto* pNpc = Cast<TESNPC>(pPlayer->baseForm);
    if (pNpc)
    {
        request.Username = pNpc->fullName.value.AsAscii();
    }
    else
    {
        request.Username = "Some dragon boi";
    }

    auto* const cpModManager = ModManager::Get();

    for (auto* pMod : cpModManager->mods)
    {
        if (!pMod->IsLoaded())
            continue;

        auto& entry = request.UserMods.ModList.emplace_back();
        entry.Id = pMod->GetId();
        entry.IsLite = pMod->IsLite();
        entry.Filename = pMod->filename;
    }

    auto& modSystem = m_world.GetModSystem();
    if (pPlayer->GetWorldSpace())
        modSystem.GetServerModId(pPlayer->GetWorldSpace()->formID, request.WorldSpaceId);

    modSystem.GetServerModId(pPlayer->parentCell->formID, request.CellId);

    request.Level = pPlayer->GetLevel();

    auto* pGameTime = TimeData::Get();
    request.PlayerTime.TimeScale = pGameTime->TimeScale->f;
    request.PlayerTime.Time = pGameTime->GameHour->f;
    request.PlayerTime.Year = pGameTime->GameYear->f;
    request.PlayerTime.Month = pGameTime->GameMonth->f;
    request.PlayerTime.Day = pGameTime->GameDay->f;

    // The transport is up and the clock is synced; what remains is the server's
    // answer to this request, so the watchdog now covers the auth round trip.
    spdlog::info("transport connected, authentication request sent");

    Send(request);
}

void TransportService::OnDisconnected(EDisconnectReason aReason)
{
    // Read before touching: a teardown can re-enter here, and the second entry
    // has to see whether an attempt was still in flight when the first ran.
    //
    // m_handshakePending is also deliberately cleared *after* the Close() that
    // may have brought us here (see the timeout branch in HandleUpdate and the
    // rejection branch at the end of HandleAuthenticationResponse). Those paths
    // raise an error event first and then tear the transport down, and this flag
    // being still set is what makes that teardown report the attempt instead of
    // being dismissed as the tail end of an attempt that already ended.
    const bool cWasAttempting = m_connected || m_handshakePending;

    m_connected = false;
    m_handshakePending = false;

    // One attempt can report its teardown more than once: Close() reports
    // kAborted synchronously for the transport, and uv_cancel wakes the name
    // resolution callback, which reports kAborted again on the next pump. Only
    // the first report of a given attempt may raise DisconnectedEvent.
    //
    // The important detail is that this must not swallow the *only* report an
    // attempt produces. An attempt that never reached the server still has to
    // tell the UI it is over: "disconnect" is the signal the overlay turns into
    // leaving the "connecting" state, and dropping it leaves the player stuck
    // there forever, which is the bug this whole mechanism exists to prevent.
    // So an attempt in flight always reports, even when an error event has
    // already been raised for it.
    if (m_attemptOutcomeReported)
    {
        spdlog::info("disconnect already reported for this attempt, ignoring the duplicate ({})",
                     static_cast<int>(aReason));
        return;
    }
    m_attemptOutcomeReported = true;

    static constexpr const char* kReasonNames[] = {"timeout",    "local problem", "kicked",
                                                   "cannot resolve address", "aborted", "normal"};
    constexpr auto kReasonCount = static_cast<int>(std::size(kReasonNames));
    const auto cReasonIndex = static_cast<int>(aReason);
    const auto cReasonName =
        cReasonIndex >= 0 && cReasonIndex < kReasonCount ? kReasonNames[cReasonIndex] : "unknown";

    if (!cWasAttempting)
    {
        // Reached for a teardown that was not part of an attempt: Close() calls
        // into the transport even when there is nothing to close, and the
        // reconnect path tears down deliberately without intending to report.
        // Re-triggering DisconnectedEvent here would have every service reset
        // itself a second time for no reason.
        spdlog::debug("Disconnected from server with no attempt in flight ({}): {}", cReasonIndex,
                      cReasonName);
        return;
    }

    spdlog::warn("Disconnected from server ({}): {}", cReasonIndex, cReasonName);

    m_dispatcher.trigger(DisconnectedEvent());
}

void TransportService::OnUpdate()
{
}

void TransportService::AbandonAttempt() noexcept
{
    // Clears the in-flight marker first so the teardown below is not reported.
    // Only the reconnect path uses this: it is about to start a replacement
    // attempt, and telling the UI the old one ended would be worse than noise --
    // it would drop the player out of "connecting" between the two tries.
    m_handshakePending = false;
    Close();
}

void TransportService::HandleUpdate(const UpdateEvent& acEvent) noexcept
{
    // A handshake that never resolves must not leave the UI stuck on
    // "connecting" with no error and no way out. The Steam status callback
    // only fires for transport failures, so a server that accepts the
    // connection and then says nothing is caught here instead.
    if (m_handshakePending && std::chrono::steady_clock::now() >= m_handshakeDeadline)
    {
        spdlog::error("Handshake with {} timed out after {}s, aborting", m_pendingEndpoint,
                      kHandshakeTimeout.count());

        // This attempt is over, so state the outcome once and cancel the
        // pending deadline. m_handshakePending and m_connected are deliberately
        // left alone: Close() below reports through OnDisconnected, and the
        // flag still being set is what makes that report count as this
        // attempt's outcome rather than as the tail of one that already ended.
        m_attemptOutcomeReported = true;

        ConnectionErrorEvent errorEvent;
        errorEvent.ErrorDetail = "{\"error\": \"no_reason\"}";
        m_dispatcher.trigger(errorEvent);

        Close();
    }

    Update();
}

void TransportService::HandleConnected(const ConnectedEvent& acEvent) noexcept
{
    m_localPlayerId = acEvent.PlayerId;
}

void TransportService::HandleDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    m_localPlayerId = NULL;
}

void TransportService::HandleAuthenticationResponse(const AuthenticationResponse& acMessage) noexcept
{
    // Whichever way the server answered, the handshake is over and the
    // watchdog must not fire on top of the answer.
    m_handshakePending = false;

    using AR = AuthenticationResponse::ResponseType;
    if (acMessage.Type == AR::kAccepted)
    {
        m_connected = true;
        m_attemptOutcomeReported = false;

        spdlog::info("authentication accepted, joined as player {}", acMessage.PlayerId);

        m_world.SetServerSettings(acMessage.Settings);

        m_dispatcher.trigger(acMessage.UserMods);
        m_dispatcher.trigger(acMessage.Settings);
        m_dispatcher.trigger(ConnectedEvent(acMessage.PlayerId));
        return; // quit the function here.
    }

    // error finding

    TiltedPhoques::String ErrorInfo;

    ErrorInfo = "{";

    switch (acMessage.Type)
    {
    case AR::kWrongVersion:
        ErrorInfo += "\"error\": \"wrong_version\", \"data\": {";
        ErrorInfo += fmt::format("\"expectedVersion\": \"{}\", \"version\": \"{}\"", acMessage.Version, BUILD_COMMIT);
        ErrorInfo += "}";
        break;
    case AR::kModsMismatch:
    {
        ErrorInfo += "\"error\": \"mods_mismatch\", \"data\": {\"mods\": [";
        bool first = true;
        for (const auto& m : acMessage.UserMods.ModList)
        {
            if (!first)
                ErrorInfo += ",";
            ErrorInfo += fmt::format("[\"{}\",\"{}\"]", m.Filename.c_str(), m.Id);
            first = false;
        }
        ErrorInfo += "]}";
        break;
    }
    case AR::kClientModsDisallowed:
    {
        ErrorInfo += "\"error\": \"client_mods_disallowed\", \"data\": { \"mods\": [";
        if (acMessage.SKSEActive)
            ErrorInfo += "\"SKSE\"";
        if (acMessage.MO2Active)
            if (acMessage.SKSEActive)
                ErrorInfo += ",";
        ErrorInfo += "\"MO2\"";
        ErrorInfo += "]}";
        break;
    }
    case AR::kWrongPassword:
    {
        ErrorInfo += "\"error\": \"wrong_password\"";
        break;
    }
    case AR::kServerFull:
    {
        ErrorInfo += "\"error\": \"server_full\"";
        break;
    }
    default: ErrorInfo += "\"error\": \"no_reason\""; break;
    }

    ErrorInfo += "}";

    ConnectionErrorEvent errorEvent;
    if (!ErrorInfo.empty())
    {
        spdlog::error(ErrorInfo.c_str());
        errorEvent.ErrorDetail = std::move(ErrorInfo);
    }

    // The server rejected us, so this attempt is over and it reports its
    // outcome once: the error carries the reason. The transport teardown that
    // follows still reports the disconnect, and that is on purpose -- the UI
    // leaves "connecting" on either signal, so raising both keeps one of them
    // from becoming a single point of failure.
    m_attemptOutcomeReported = true;

    m_dispatcher.trigger(errorEvent);

    // Also tear the transport down here rather than waiting for the server's
    // close packet, which is not guaranteed to arrive at all.
    Close();
}

void TransportService::HandleNotifySettingsChange(const NotifySettingsChange& acMessage) noexcept
{
    m_world.SetServerSettings(acMessage.Settings);
    m_dispatcher.trigger(acMessage.Settings);
}
