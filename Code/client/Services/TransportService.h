#pragma once

#include "Events/ConnectedEvent.h"
#include "Events/DisconnectedEvent.h"

#include <atomic>
#include <chrono>
#include <Client.hpp>

struct ImguiService;
struct UpdateEvent;
struct ClientMessage;
struct AuthenticationResponse;
struct NotifySettingsChange;

struct World;

using TiltedPhoques::Client;

/**
 * @brief Handles communication with the server.
 */
struct TransportService : Client
{
    TransportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~TransportService() noexcept = default;

    TP_NOCOPYMOVE(TransportService);

    bool Send(const ClientMessage& acMessage) const noexcept;

    void OnConsume(const void* apData, uint32_t aSize) override;
    void OnConnected() override;
    void OnDisconnected(EDisconnectReason aReason) override;
    void OnUpdate() override;

    [[nodiscard]] bool IsOnline() const noexcept { return m_connected; }
    void SetServerPassword(const std::string& acPassword) noexcept { m_serverPassword = acPassword; }
    const uint32_t& GetLocalPlayerId() const noexcept { return m_localPlayerId; }

    /**
     * @brief Starts the connection watchdog.
     *
     * A server that never answers, or one that accepts the transport connection
     * but never sends a clock sync or an authentication response, used to leave
     * the UI on "connecting" forever with no error and no way out. The watchdog
     * is armed here and disarmed once the handshake completes or fails.
     */
    void ArmConnectionWatchdog(const std::string& acEndpoint) noexcept;

    /**
     * @brief Ends the attempt in flight without reporting anything to the UI.
     *
     * The reconnect path restarts an attempt the player never asked about, so
     * the teardown that precedes it is a side effect rather than news. A plain
     * teardown from anywhere else must call Close() directly so the attempt
     * keeps reporting its outcome.
     */
    void AbandonAttempt() noexcept;

protected:
    // Event handlers
    void HandleUpdate(const UpdateEvent& acEvent) noexcept;
    void HandleConnected(const ConnectedEvent& acEvent) noexcept;
    void HandleDisconnected(const DisconnectedEvent& acEvent) noexcept;

    // Packet handlers
    void HandleAuthenticationResponse(const AuthenticationResponse& acMessage) noexcept;
    void HandleNotifySettingsChange(const NotifySettingsChange& acMessage) noexcept;

private:
    World& m_world;
    entt::dispatcher& m_dispatcher;
    bool m_connected;
    String m_serverPassword{};
    uint32_t m_localPlayerId;

    // Connection watchdog.
    //
    // m_handshakePending is set while an attempt is on the wire, from the moment
    // it is armed until its outcome is reported. It is the only flag that says
    // "something is in flight", so OnDisconnected reads it to decide whether the
    // teardown it is handling still owes the UI an event -- and for that reason
    // it must be cleared *after* Close(), never before, or the event that ends
    // the attempt gets swallowed. m_connected means the handshake finished and
    // the session is live; it is never set before authentication is accepted.
    //
    // m_handshakeDeadline is only meaningful while m_handshakePending is set.
    //
    // m_attemptOutcomeReported collapses the several teardowns one attempt can
    // produce into the single set of events the UI expects, and is reset when
    // the next attempt starts.
    bool m_handshakePending{false};
    bool m_attemptOutcomeReported{false};
    std::chrono::steady_clock::time_point m_handshakeDeadline{};
    String m_pendingEndpoint{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_sendServerMessageConnection;
    entt::scoped_connection m_settingsChangeConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    std::function<void(UniquePtr<ServerMessage>&)> m_messageHandlers[kServerOpcodeMax];
};
