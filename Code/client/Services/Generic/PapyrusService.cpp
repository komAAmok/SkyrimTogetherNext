#include <TiltedOnlinePCH.h>

#include <Events/PapyrusFunctionRegisterEvent.h>

#include <Services/PapyrusService.h>

PapyrusService::PapyrusService(entt::dispatcher& aDispatcher) noexcept
{
    m_papyrusFunctionRegisterConnection = aDispatcher.sink<PapyrusFunctionRegisterEvent>().connect<&PapyrusService::HandlePapyrusFunctionEvent>(this);
}

const void* PapyrusService::Get(const String& acNamespace, const String& acFunction) const noexcept
{
    const String key = acNamespace + "::" + acFunction;

    const auto itor = m_functions.find(key);
    if (itor != std::end(m_functions))
        return itor->second;

    // Whatever asked for this cannot call anything now, so say so here instead
    // of leaving it to fail as a null call later. The total separates the two
    // reasons: zero means the registration hook never saw a single native, a
    // large number means only this one is absent.
    spdlog::warn("papyrus function {} is not registered, {} natives were captured; calls to it are skipped",
                 key.c_str(), m_functions.size());

    return nullptr;
}

void PapyrusService::HandlePapyrusFunctionEvent(const PapyrusFunctionRegisterEvent& acEvent) noexcept
{
    m_functions[acEvent.Namespace + "::" + acEvent.Name] = acEvent.Function;
}

void PapyrusDetail::ReportMissing(const char* acpName, bool& aReported) noexcept
{
    if (aReported)
        return;

    aReported = true;
    spdlog::error("papyrus function {} was needed but never registered; the call is skipped, so whatever depends on "
                  "it does not happen",
                  acpName);
}
