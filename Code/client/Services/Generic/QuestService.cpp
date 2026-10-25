#include <TiltedOnlinePCH.h>

#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Events/UpdateEvent.h>

#include <Services/QuestService.h>
#include <Services/ImguiService.h>
#include <Services/OverlayService.h>
#include <Services/TransportService.h>

#include <OverlayApp.hpp>

#include <PlayerCharacter.h>
#include <Forms/TESQuest.h>
#include <Games/TES.h>
#include <Games/Overrides.h>
#include <AI/Movement/PlayerControls.h>

#include <Events/EventDispatcher.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/RequestQuestSceneUpdate.h>
#include <Messages/NotifyQuestUpdate.h>
#include <Messages/NotifyQuestSceneUpdate.h>

namespace spdfmt = spdlog::fmt_lib;

static TESQuest* FindQuestByNameId(const String& name)
{
    // The mod manager owns the quest registry and is null before the engine
    // builds it; this helper is reachable from the command path, which a player
    // can invoke at any time.
    auto* pModManager = ModManager::Get();
    if (!pModManager)
        return nullptr;

    auto& questRegistry = pModManager->quests;
    auto it = std::find_if(questRegistry.begin(), questRegistry.end(), [name](auto* it) { return std::strcmp(it->idName.AsAscii(), name.c_str()); });

    return it != questRegistry.end() ? *it : nullptr;
}

using HeldStages = QuestService::HeldStages;

static void ReleaseStagesForWaitingPhases(TESQuest* apQuest, HeldStages& aHeld) noexcept;
static void ReleaseUncheckedStages(TESQuest* apQuest, HeldStages& aHeld) noexcept;

// Upstream's plain scene trace. Shared by the TESSceneEvent handler and the phase handler's scene-begin failsafe so the
// one-line formatter stays in a single place.
static void LogSceneEvent(const TESSceneEvent* apEvent) noexcept
{
    spdlog::debug("Scene {:X} {}", apEvent->sceneFormId, apEvent->type == 0 ? "begin" : "end");
}

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_joinedConnection = aDispatcher.sink<ConnectedEvent>().connect<&QuestService::OnConnected>(this);
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&QuestService::OnUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&QuestService::OnDisconnected>(this);
    m_leftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&QuestService::OnPartyLeft>(this);
    m_questUpdateConnection = aDispatcher.sink<NotifyQuestUpdate>().connect<&QuestService::OnQuestUpdate>(this);
    m_questSceneUpdateConnection = aDispatcher.sink<NotifyQuestSceneUpdate>().connect<&QuestService::OnQuestSceneUpdate>(this);
    m_playerId = 0;

    // Game events:
    // Starts, stops and stages are sent from TESQuestStartStopEvent and TESQuestStageEvent, which fire while the quest
    // changes. Outside a party they only prompt the overlay.
    // TESQuestStageItemDoneEvent fires once for each of a stage's log entries, after its fragment has run. That's too
    // late to send on, but it tells when an applied held stage is done.
    // The scene, scene action and dialogue line events are only logged (the fork also sends the scene begin/end from
    // TESSceneEvent as a RequestQuestSceneUpdate), to follow scenes when debugging held stages. OnUpdate explains why
    // they can't trigger the release of held stages.
    auto* pEventList = EventDispatcherManager::Get();
    if (!pEventList)
    {
        // The dispatcher is built by the engine during startup and this service
        // is constructed from World's initialisation, so the order is not
        // guaranteed. Without it no quest event is observed - the same
        // degradation the legacy runtime already accepts for unmapped ids -
        // whereas dereferencing a null one takes the process down.
        spdlog::warn(__FUNCTION__ ": event dispatcher not available yet, quest sync will not observe events");
        return;
    }

    pEventList->questStartStopEvent.RegisterSink(this);
    pEventList->questStageEvent.RegisterSink(this);
    pEventList->questStageItemDoneEvent.RegisterSink(this);
    pEventList->sceneEvent.RegisterSink(this);
    pEventList->sceneActionEvent.RegisterSink(this);
    pEventList->scenePhaseEvent.RegisterSink(this);
    pEventList->topicInfoEvent.RegisterSink(this);
}

void QuestService::OnConnected(const ConnectedEvent& apEvent) noexcept
{
    // TODO: this should be followed with whatever the quest leader selected
    /*
    // deselect any active quests
    auto* pPlayer = PlayerCharacter::Get();
    for (auto& objective : pPlayer->objectives)
    {
        if (auto* pQuest = objective.instance->quest)
            pQuest->SetActive(false);
    }
    */
    m_playerId = apEvent.PlayerId;
}

void QuestService::OnUpdate(const UpdateEvent&) noexcept
{
    if (m_heldStages.empty())
        return;

    // Checked every frame rather than on scene events: an action's done event fires before its fragment runs, and the
    // action only counts as complete afterwards, with no event of its own
    Vector<uint32_t> questFormIds;
    for (const auto& [questFormId, _] : m_heldStages)
        questFormIds.push_back(questFormId);

    for (uint32_t questFormId : questFormIds)
    {
        TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(questFormId));
        if (HeldStages* pHeld = GetHeldStages(questFormId, pQuest))
        {
            ReleaseStagesForWaitingPhases(pQuest, *pHeld);
            ReleaseUncheckedStages(pQuest, *pHeld);
        }
    }
}

void QuestService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_playerId = 0;
    m_heldStages.clear();
}

void QuestService::OnPartyLeft(const PartyLeftEvent&) noexcept
{
    m_heldStages.clear();
}

BSTEventResult QuestService::OnEvent(const TESQuestStartStopEvent* apEvent,
                                     const EventDispatcher<TESQuestStartStopEvent>*)
{
    GameId Id;
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId));
    if (pQuest == nullptr || IsNonSyncableQuest(pQuest))
        return BSTEventResult::kOk; // pQuest == nullptr shouldn't happen.

    if (!m_world.Get().GetPartyService().IsInParty())
    {
        // Questing alone on a populated server is exactly when a party is worth
        // suggesting. A stop event is not progress, so only a start gets the hint.
        if (!pQuest->IsStopped())
            NotifyOverlayOfQuestUpdate(pQuest);

        return BSTEventResult::kOk;
    }

    if (!m_world.GetModSystem().GetServerModId(pQuest->formID, Id))
        return BSTEventResult::kOk; // nor should getting GameId fail.

    const auto startStop = pQuest->IsStopped() ? "stopped" : "started";
    const bool isMiscNone = (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous);
    const TiltedPhoques::String miscQuest(isMiscNone ? spdfmt::format("none/misc quest gameId {:X}", Id.LogFormat()) : "quest");
    const bool isLeader = m_world.Get().GetPartyService().IsLeader();
    const auto playerString = isLeader ? "leader" : "player";

    spdlog::info(
        __FUNCTION__ ": {} {} formId: {:X}, questStage: {}, questType: {}, flags: {:X}, {} {}, name: {}", startStop, miscQuest, apEvent->formId, pQuest->currentStage,
        static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());

    m_world.GetRunner().Queue(
        [&, formId = pQuest->formID, stageId = pQuest->currentStage, stopped = pQuest->IsStopped(), type = pQuest->type]()
        {
            GameId Id;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(formId, Id))
            {
                RequestQuestUpdate update;
                update.Id = Id;
                update.Stage = stageId;
                update.Status = stopped ? RequestQuestUpdate::Stopped : RequestQuestUpdate::Started;
                update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type);
                m_world.GetTransport().Send(update);
            }
        });

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESQuestStageEvent* apEvent, const EventDispatcher<TESQuestStageEvent>*)
{
    GameId Id;
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId));
    if (pQuest == nullptr || IsNonSyncableQuest(pQuest))
        return BSTEventResult::kOk; // pQuest == nullptr shouldn't happen.

    if (!m_world.Get().GetPartyService().IsInParty())
    {
        NotifyOverlayOfQuestUpdate(pQuest);
        return BSTEventResult::kOk;
    }

    if (!m_world.GetModSystem().GetServerModId(pQuest->formID, Id))
        return BSTEventResult::kOk; // nor should getting GameId fail.

    const auto startStop = pQuest->IsStopped() ? "stopped" : "started";
    const bool isMiscNone = (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous);
    const TiltedPhoques::String miscQuest(isMiscNone ? spdfmt::format("none/misc quest gameId {:X}", Id.LogFormat()) : "quest");
    const bool isLeader = m_world.Get().GetPartyService().IsLeader();
    const auto playerString = isLeader ? "leader" : "player";

    spdlog::info(
        __FUNCTION__ ": stage update {} formId: {:X}, questStage: {}, questType: {}, isStopped: {}, flags: {:X}, {} {}, name: {}", miscQuest, apEvent->formId, apEvent->stageId,
        static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(), pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());

    // We should not see a stage 0 Update to a started quest, unless it started a long time ago in StageWait.
    bool isResetUpdate = apEvent->stageId == 0 &&
        (pQuest->flags & (TESQuest::Flags::Enabled | TESQuest::Flags::StageWait)) != (TESQuest::Flags::Enabled | TESQuest::Flags::StageWait);
    if (isResetUpdate)
        spdlog::critical(
            __FUNCTION__ ": suspected quest reset event NEEDS TEST, quest formId: {:X}, questStage: {}, questType: {}, isStopped: {}, flags {:X}, {} {}, name: {}",
            apEvent->formId, apEvent->stageId, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(), pQuest->flags, playerString, PlayerId(),
            pQuest->fullName.value.AsAscii());

    m_world.GetRunner().Queue(
        [&, formId = apEvent->formId, stageId = apEvent->stageId, isReset = isResetUpdate, type = pQuest->type]()
        {
            GameId Id;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(formId, Id))
            {
                RequestQuestUpdate update;
                update.Id = Id;
                update.Stage = stageId;
                // FIXME EXPERIMENT. If the critical log fires, we think it is a reset. If observed in real life
                // we can debug it and see if this reset logic actually works
                update.Status = isReset ? RequestQuestUpdate::Reset : RequestQuestUpdate::StageUpdate;
                update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type);
                m_world.GetTransport().Send(update);
            }
        });

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESSceneEvent* apEvent, const EventDispatcher<TESSceneEvent>*)
{
    LogSceneEvent(apEvent);

    GameId Id;
    // Same lookup the phase-event handler already guards: the scene can be gone
    // by the time the event is dispatched, and this dereferenced it outright.
    auto pScene = Cast<BGSScene>(TESForm::GetById(apEvent->sceneFormId));
    auto pQuest = pScene ? pScene->owningQuest : nullptr;
    if (pQuest == nullptr || QuestService::IsNonSyncableQuest(pQuest) || !m_world.Get().GetPartyService().IsInParty() || !m_world.GetModSystem().GetServerModId(pQuest->formID, Id))
        return BSTEventResult::kOk;

    const auto beginEnd = apEvent->type ? "End" : "Begin";
    const bool isMiscNone = (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous);
    const TiltedPhoques::String miscQuest(isMiscNone ? spdfmt::format("none/misc quest gameId {:X},", Id.LogFormat()) : "quest");
    const bool isLeader = m_world.Get().GetPartyService().IsLeader();
    const auto playerString = isLeader ? "leader" : "player";
    const auto isPlaying = pScene->isPlaying;

    spdlog::debug(
        __FUNCTION__ "::TESSceneEvent*"
                     ": sending scene {}, scene {:X}, isPlaying {}, {} formId: {:X}, questStage: {}, "
                     "questType: {}, isStopped: {}, flags: {:X}, {} {}, name: {}",
        beginEnd, apEvent->sceneFormId, isPlaying, miscQuest, pQuest->formID, pQuest->currentStage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type),
        pQuest->IsStopped(), pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());

    // FIXME EXPERIMENT, seems to work, scene begin/end sent to server so we can choose to send a quest stage poke.
    m_world.GetRunner().Queue(
        [&, sceneId = apEvent->sceneFormId, questId = pQuest->formID, stageId = pQuest->currentStage, type = pQuest->type, sceneType = apEvent->type]()
        {
            GameId sceneGameId;
            GameId questGameId;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(sceneId, sceneGameId) && modSys.GetServerModId(questId, questGameId))
            {
                RequestQuestSceneUpdate update;
                update.SceneId = sceneGameId;
                update.QuestId = questGameId;
                update.Stage = stageId;
                update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type);
                update.SceneType = sceneType;

                m_world.GetTransport().Send(update);
            }
        });

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESSceneActionEvent* apEvent, const EventDispatcher<TESSceneActionEvent>*)
{
    const bool isLeader = m_world.GetPartyService().IsLeader();
    const auto playerType = isLeader ? "leaderId" : "playerId";

    spdlog::debug("Scene {:X} action {} done, quest: {:X}, alias: {}, {} {}", apEvent->sceneFormId, apEvent->actionIndex, apEvent->questFormId, apEvent->aliasId,
                  playerType, PlayerId());
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESScenePhaseEvent* apEvent, const EventDispatcher<TESScenePhaseEvent>*)
{
    const bool isLeader = m_world.GetPartyService().IsLeader();
    const auto playerType = isLeader ? "leaderId" : "playerId";

    // The fork's own phase trace (was: quest stage + isAnyCutscenePlaying from the old, larger event layout). The
    // upstream action breakdown is appended when the scene is resolvable.
    auto pScene = Cast<BGSScene>(TESForm::GetById(apEvent->sceneFormId));
    auto pQuest = pScene ? pScene->owningQuest : nullptr;
    const String sceneType = apEvent->type == 0 ? "Begin" : "End";
    const auto isAnyScenePlaying = pQuest == nullptr ? false : pQuest->IsAnyCutscenePlaying();

    // The per-action breakdown walks the scene's action array, so only build it when debug is actually on
    const bool cLogActions = pScene && spdlog::should_log(spdlog::level::debug);
    if (cLogActions)
    {
        // A phase without completion conditions ends once every action running in it is complete
        String actions;
        for (BGSSceneAction* pAction : pScene->actions)
        {
            if (!pAction || !pAction->IsInPhase(apEvent->phaseIndex))
                continue;

            constexpr std::array cTypeNames{"dialogue", "package", "timer"};
            const uint32_t cType = static_cast<uint32_t>(pAction->GetType());
            actions += spdfmt::format(
                " [{} {} alias {} {}]", pAction->GetIndex(), cType < cTypeNames.size() ? cTypeNames[cType] : "unknown", pAction->actorID,
                pAction->IsComplete() ? "complete" : "running");
        }

        spdlog::debug("Scene {:X} phase {} {}, actions:{}, quest {:X}, isAnyScenePlaying {}, by {} {}", apEvent->sceneFormId, apEvent->phaseIndex, sceneType, actions,
                      pScene->owningQuest ? pScene->owningQuest->formID : 0, isAnyScenePlaying, playerType, PlayerId());
    }
    else
    {
        spdlog::debug(
            "TESScenePhaseEvent event: quest {:X}, scene {:X}. phase index {}, type {}, "
            "isAnyScenePlaying {}, by {} {}",
            pQuest ? pQuest->formID : 0, apEvent->sceneFormId, apEvent->phaseIndex, sceneType, isAnyScenePlaying, playerType, PlayerId());
    }

    // Failsafe if we did not get a scene Begin event; happens if waiting
    // in Phase 0 (Phase 1 in CK), which means the first thing we'll get
    // is Phase 0 End. And on very long scenes, refreshes SceneMaster so it doesn't time out.
    const TESSceneEvent event = {nullptr, apEvent->sceneFormId, 0};
    if (apEvent->type == 0 || apEvent->phaseIndex == 0)
        OnEvent(&event, nullptr);

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESQuestStageItemDoneEvent* apEvent, const EventDispatcher<TESQuestStageItemDoneEvent>*)
{
    // Sent once the fragment of one of the stage's log entries has run, usually from a script thread
    m_world.GetRunner().Queue(
        [this, questFormId = apEvent->formId, stage = apEvent->stageId]()
        {
            auto it = m_heldStages.find(questFormId);
            if (it == m_heldStages.end())
                return;

            Vector<uint16_t>& pendingItems = it.value().PendingItems;
            auto itemIt = std::find(pendingItems.begin(), pendingItems.end(), stage);
            if (itemIt == pendingItems.end())
                return;

            pendingItems.erase(itemIt);
            spdlog::debug("Fragment of applied member quest stage {} for {:X} done, {} left", stage, questFormId, pendingItems.size());
        });

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESTopicInfoEvent* apEvent, const EventDispatcher<TESTopicInfoEvent>*)
{
    spdlog::debug(
        "Dialogue line {:X} {}, speaker: {:X}", apEvent->uiTopicInfoFormID, apEvent->eType == 0 ? "begin" : "end",
        apEvent->pSpeakerRef ? apEvent->pSpeakerRef->formID : 0);
    return BSTEventResult::kOk;
}

static bool Compare(TESConditionItem::OpCode aOpCode, float aLhs, float aRhs) noexcept
{
    switch (aOpCode)
    {
    case TESConditionItem::OpCode::kEqualTo: return aLhs == aRhs;
    case TESConditionItem::OpCode::kNotEqualTo: return aLhs != aRhs;
    case TESConditionItem::OpCode::kGreaterThan: return aLhs > aRhs;
    case TESConditionItem::OpCode::kGreaterThanOrEqualTo: return aLhs >= aRhs;
    case TESConditionItem::OpCode::kLessThan: return aLhs < aRhs;
    case TESConditionItem::OpCode::kLessThanOrEqualTo: return aLhs <= aRhs;
    default: return false;
    }
}

// Whether setting the stage turns a GetStage/GetStageDone check on the quest from false to true. With aOrChecksLater,
// also whether it changes the check's result at all, or the check is on the same or a later stage.
static bool IsCheckedByStage(const TESCondition& acConditions, TESQuest* apQuest, uint16_t aStage, bool aOrChecksLater) noexcept
{
    for (const TESConditionItem* pItem = acConditions.pHead; pItem; pItem = pItem->pNext)
    {
        if (pItem->UsesGlobal() || pItem->params[0] != apQuest)
            continue;

        const TESConditionItem::OpCode cOpCode = pItem->GetOpCode();
        float checkedStage = 0.f;
        bool before = false;
        bool after = false;
        if (pItem->function == TESConditionItem::kGetStage)
        {
            // Stages only go up, GetStage returns the highest one set
            checkedStage = pItem->comparisonValue;
            before = Compare(cOpCode, apQuest->currentStage, pItem->comparisonValue);
            after = Compare(cOpCode, std::max(apQuest->currentStage, aStage), pItem->comparisonValue);
        }
        else if (pItem->function == TESConditionItem::kGetStageDone)
        {
            const uint16_t cStage = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(pItem->params[1]));
            checkedStage = cStage;
            if (cStage == aStage)
            {
                before = Compare(cOpCode, 0.f, pItem->comparisonValue);
                after = Compare(cOpCode, 1.f, pItem->comparisonValue);
            }
        }
        else
        {
            continue;
        }

        if (!before && after)
            return true;

        if (aOrChecksLater && (before != after || checkedStage >= aStage))
            return true;
    }

    return false;
}

// Whether the phase's own lines are done, so passing its completion conditions won't cut one off. Timers and packages
// don't hold such a phase back, and packages never complete on clients that don't own the NPC.
static bool IsPhaseWaitingOnConditions(BGSScene* apScene, uint32_t aPhaseIndex) noexcept
{
    for (BGSSceneAction* pAction : apScene->actions)
    {
        if (!pAction || !pAction->IsInPhase(aPhaseIndex))
            continue;

        if (pAction->GetType() != BGSSceneAction::Type::kDialogue || pAction->IsComplete() || pAction->Loops())
            continue;

        // The player's own lines wait for this player to answer, while a member may have answered instead
        if (apScene->owningQuest && apScene->owningQuest->GetAliasedRef(pAction->actorID) == PlayerCharacter::Get())
            continue;

        return false;
    }

    return true;
}

// Whether a phase the playing scenes have yet to finish checks the stage or a later one, or checks differently with it
// (a phase waiting on GetStage == N). Scenes that only end or repeat on it don't count (MQ105's sprint trial).
static bool IsCheckedByPlayingScene(TESQuest* apQuest, uint16_t aStage) noexcept
{
    for (BGSScene* pScene : apQuest->scenes)
    {
        if (!pScene)
            continue;

        for (uint32_t i = pScene->GetNextPhaseIndex(); i < pScene->phases.length; ++i)
        {
            const BGSScenePhase* pPhase = pScene->phases[i];
            if (!pPhase)
                continue;

            // The running phase is past its start conditions
            const bool cIsRunning = pScene->currentPhaseIndex == i;
            if ((!cIsRunning && IsCheckedByStage(pPhase->startConditions, apQuest, aStage, true)) ||
                IsCheckedByStage(pPhase->completionConditions, apQuest, aStage, true))
            {
                return true;
            }
        }
    }

    return false;
}

void QuestService::HoldStage(TESQuest* apQuest, uint16_t aStage) noexcept
{
    Vector<uint16_t>& stages = m_heldStages[apQuest->formID].Stages;
    auto it = std::lower_bound(stages.begin(), stages.end(), aStage);
    if (it == stages.end() || *it != aStage)
        stages.insert(it, aStage);

    spdlog::info("Holding member quest stage {} for {:X} until no playing scene of it checks it", aStage, apQuest->formID);
}

static void ApplyHeldStages(TESQuest* apQuest, HeldStages& aHeld, size_t aCount) noexcept
{
    ScopedQuestOverride _;

    // In order, a later stage's scripts can rely on the earlier ones
    for (size_t i = 0; i < aCount; ++i)
    {
        // A scene the fragments start only plays once they've run, and the next stage may be what it waits on. Each log
        // entry reports when its fragment is done.
        const uint16_t cStage = aHeld.Stages[i];
        if (apQuest->ScriptSetStage(cStage))
            aHeld.PendingItems.insert(aHeld.PendingItems.end(), apQuest->GetStageItemCount(cStage), cStage);
    }

    aHeld.Stages.erase(aHeld.Stages.begin(), aHeld.Stages.begin() + aCount);
}

static void ReleaseStagesForWaitingPhases(TESQuest* apQuest, HeldStages& aHeld) noexcept
{
    // A phase whose own actions are done and whose completion conditions want a held stage only moves on with it.
    // Release up to the first stage that does it, the leader's scene then carries on at its own pace.
    for (BGSScene* pScene : apQuest->scenes)
    {
        if (aHeld.Stages.empty())
            break;

        if (!pScene || !pScene->isPlaying || pScene->currentPhaseIndex >= pScene->phases.length)
            continue;

        BGSScenePhase* pPhase = pScene->phases[pScene->currentPhaseIndex];
        if (!pPhase)
            continue;

        // Conditions first, they rarely mention a held stage and are cheaper than walking the phase's actions
        const auto cStageIt = std::find_if(
            aHeld.Stages.begin(), aHeld.Stages.end(),
            [&](uint16_t aStage) { return IsCheckedByStage(pPhase->completionConditions, apQuest, aStage, false); });
        if (cStageIt == aHeld.Stages.end() || !IsPhaseWaitingOnConditions(pScene, pScene->currentPhaseIndex))
            continue;

        spdlog::info(
            "Releasing held member quest stages for {:X} up to {}, scene {:X} phase {} waits on it", apQuest->formID, *cStageIt, pScene->formID,
            pScene->currentPhaseIndex);
        ApplyHeldStages(apQuest, aHeld, cStageIt - aHeld.Stages.begin() + 1);
    }
}

// Applies held stages in order while no playing scene checks the next one, e.g. once the scenes that did have ended.
// Waits for the fragments of the stages it applied first.
static void ReleaseUncheckedStages(TESQuest* apQuest, HeldStages& aHeld) noexcept
{
    while (!aHeld.Stages.empty() && aHeld.PendingItems.empty() && !IsCheckedByPlayingScene(apQuest, aHeld.Stages.front()))
    {
        spdlog::info("Applying held member quest stage {} for {:X}, no playing scene checks it", aHeld.Stages.front(), apQuest->formID);
        ApplyHeldStages(apQuest, aHeld, 1);
    }
}

HeldStages* QuestService::GetHeldStages(uint32_t aQuestFormId, TESQuest* apQuest) noexcept
{
    auto it = m_heldStages.find(aQuestFormId);
    if (it == m_heldStages.end())
        return nullptr;

    // Setting a stage on a stopped quest starts it again, and scenes also end because their quest stopped
    HeldStages& held = it.value();
    if (!apQuest || apQuest->IsStopped())
    {
        if (!held.Stages.empty())
            spdlog::info("Dropping held member quest stages for {:X}, quest stopped or missing", aQuestFormId);

        m_heldStages.erase(it);
        return nullptr;
    }

    // Reached by the leader's own scripts in the meantime
    std::erase_if(
        held.Stages,
        [apQuest](uint16_t aStage)
        {
            if (!apQuest->IsStageReached(aStage))
                return false;

            spdlog::info("Dropping held member quest stage {} for {:X}, already reached here", aStage, apQuest->formID);
            return true;
        });

    if (held.Stages.empty() && held.PendingItems.empty())
    {
        m_heldStages.erase(it);
        return nullptr;
    }

    return &held;
}

void QuestService::NotifyOverlayOfQuestUpdate(TESQuest* apQuest) noexcept
{
    if (apQuest == nullptr)
        return;

    if (!m_world.GetTransport().IsOnline())
        return;

    // Nobody else is around to party with, or an invite toast is already asking.
    const auto remotePlayerView = m_world.view<PlayerComponent, RemoteComponent>();
    if (m_world.GetPartyService().HasPendingInvitations() || remotePlayerView.begin() == remotePlayerView.end())
        return;

    // Hidden background quests and minor misc tasks advance constantly and are
    // not worth prompting about.
    if (apQuest->type == TESQuest::Type::None || apQuest->type == TESQuest::Type::Miscellaneous)
        return;

    if (auto* pApp = m_world.GetOverlayService().GetOverlayApp())
        pApp->ExecuteAsync("questUpdated");
}

void QuestService::OnQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept
{
    // Quest and scene notifications. The quest log maps and the reports allocate; the
    // server resends the state a client missed on its next change.
    try
    {
        auto Id = aUpdate.Id;
        auto formId = World::Get().GetModSystem().GetGameId(aUpdate.Id);
        TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(formId));
        if (pQuest == nullptr || !m_world.Get().GetPartyService().IsInParty())
            return;

        const auto startStop = pQuest->IsStopped() ? "stopped" : "started";
        const bool isMiscNone = (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous); // If we can't get the GameId we can't sync anyway.
        const TiltedPhoques::String miscQuest(isMiscNone ? spdfmt::format("none/misc quest gameId {:X},", Id.LogFormat()) : "quest");
        const bool isLeader = m_world.Get().GetPartyService().IsLeader();
        const auto playerString = isLeader ? "leader" : "player";
        const auto isSceneMaster = aUpdate.SceneMaster == PlayerId();

        // Quest OnEvent()s send updates to the server where the Leader deduplicates them,
        // so most remote updates (QuestService::OnQuestUpdate) are coming from the Leader.
        //
        // But when Party Members are all playing a scene, the scene itself is advancing
        // the quest, so everyone is sending the same quest updates close enough together some
        // can sneak through the dedup code. That's solved client-side by not allowing rewind
        // when in a scene.
        //
        // The initiator of the scene is remembered as SceneMaster and sent in NotifyQuestUpdate.
        // Currently only used in experiments & logs, we may end up needing it.
        //
        // FIXME: Experiment: To unstick non-SceneMasters if they get stuck on an interaction,
        // the SceneMaster sends a forced quest stage update at the end of the scene. This may
        // be unnecessary now that we have fixed scene playing speed sync to be better.
        bool updateDisabled = pQuest->IsAnyCutscenePlaying() && (aUpdate.Status == NotifyQuestUpdate::StageUpdate && aUpdate.Stage < pQuest->currentStage);

        if (updateDisabled)
        {
            spdlog::debug(
                __FUNCTION__ ": suppressing quest stage update while playing a scene: gameId: {:X}, formId: {:X}, "
                             "questStage: {}, questStatus: {}, questType: {}, SceneMaster {}, "
                             "isAnyCutscenePlaying {}, player {}, formId: {:X}, name: {}",
                aUpdate.Id.LogFormat(), formId, aUpdate.Stage, aUpdate.Status, aUpdate.ClientQuestType, PlayerId() == aUpdate.SceneMaster, pQuest->IsAnyCutscenePlaying(), PlayerId(),
                formId, pQuest->fullName.value.AsAscii());
            return;
        }

        // A stage the fork's own scene-rewind guard let through above is still checked for a scene on the leader side.
        if (aUpdate.Status == NotifyQuestUpdate::StageUpdate && pQuest->IsStageReached(aUpdate.Stage))
        {
            spdlog::info("Remote quest {:X} already reached stage {}, current stage: {}", formId, aUpdate.Stage, pQuest->currentStage);
            return;
        }

        // A member stage the leader's playing scene checks can push it past its own scripts (a phase ending before its
        // lines and scripts ran), so hold it, and the stages after it, until the scene gets there or reaches it itself.
        const bool cIsLeaderStage = aUpdate.Status == NotifyQuestUpdate::StageUpdate && m_world.GetPartyService().IsLeader();
        HeldStages* pHeld = cIsLeaderStage || aUpdate.Status == NotifyQuestUpdate::Stopped ? GetHeldStages(formId, pQuest) : nullptr;
        if (cIsLeaderStage && (pHeld || IsCheckedByPlayingScene(pQuest, aUpdate.Stage)))
        {
            HoldStage(pQuest, aUpdate.Stage);
            return;
        }

        // The member reached the held stages before stopping the quest, and once it's stopped here they're dropped
        if (pHeld && !pHeld->Stages.empty())
        {
            spdlog::info("Applying {} held member quest stages for {:X} before it stops", pHeld->Stages.size(), formId);
            ApplyHeldStages(pQuest, *pHeld, pHeld->Stages.size());
        }

        bool wasUpdated = false;
        const bool isRunning = pQuest->getState() == TESQuest::State::Running;

        // The quest events fire synchronously while the stage is set, don't send them back
        ScopedQuestOverride _;

        switch (aUpdate.Status)
        {
        case NotifyQuestUpdate::Started:
            if (isRunning)
            {
                spdlog::debug(
                    __FUNCTION__ ": suppressing duplicate start {} formId: {:X}, questStage: {}, "
                                 "questType: {}, isStopped: {}, flags: {:X}, {} {}, name: {}",
                    miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(),
                    pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());
            }
            else
            {
                spdlog::debug(
                    __FUNCTION__ ": remotely started {} formId: {:X}, questStage: {}, "
                                 "questType: {}, isStopped: {}, flags: {:X}, {} {}, name: {}",
                    miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(),
                    pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());

                pQuest->ScriptSetStage(aUpdate.Stage);
                pQuest->SetActive(true);
            }
            wasUpdated = true;
            break;

        case NotifyQuestUpdate::Reset:
            spdlog::critical(
                __FUNCTION__ ": REPORT THIS LOG, experimental remote reset {} formId: {:X}, questStage: {}, questType: {}, "
                             "isStopped: {}, flags: {:X}, {} {}, name: {}",
                miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(),
                pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());

            pQuest->ScriptResetAndUpdate();
            wasUpdated = true;
            break;

        case NotifyQuestUpdate::StageUpdate: // TODO? Insert START iff needed?
            spdlog::debug(
                __FUNCTION__ ": remotely updated {} formId: {:X}, questStage: {}, questType: {}, "
                             "isStopped: {}, flags: {:X}, {} {}, name: {}",
                miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(),
                pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());

            wasUpdated = pQuest->ScriptSetStage(aUpdate.Stage);
            break;

        case NotifyQuestUpdate::Stopped:
            spdlog::debug(
                __FUNCTION__ ": remotely stopped {} formId: {:X}, questStage: {}, questType: {}, "
                             "isStopped: {}, flags: {:X}, {} {}, name: {}",
                miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), pQuest->IsStopped(),
                pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());
            wasUpdated = StopQuest(formId);
            break;

        default:
            spdlog::error(
                __FUNCTION__ ": unknown remote status {} {} formId: {:X}, questStage: {}, "
                             "questType: {}, isStopped: {}, flags: {:X}, {} {}, name: {}",
                aUpdate.Status, miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type),
                pQuest->IsStopped(), pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());
            break;
        }

        if (!wasUpdated)
            spdlog::error(
                __FUNCTION__ ": failed to remotely update status {} {} formId: {:X}, questStage: "
                             "{}, questType: {}, isStopped: {}, flags: {:X}, {} {}, name: {}",
                aUpdate.Status, miscQuest, formId, aUpdate.Stage, static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type),
                pQuest->IsStopped(), pQuest->flags, playerString, PlayerId(), pQuest->fullName.value.AsAscii());
    }
    catch (...)
    {
        Diagnostics::ReportFailure(__FUNCTION__, "ran out of memory in QuestService::OnQuestUpdate");
    }
}

void QuestService::OnQuestSceneUpdate(const NotifyQuestSceneUpdate& aUpdate) noexcept
{
    // Quest and scene notifications. The quest log maps and the reports allocate; the
    // server resends the state a client missed on its next change.
    try
    {
        ModSystem& modSystem = World::Get().GetModSystem();
        auto pQuest = Cast<TESQuest>(TESForm::GetById(modSystem.GetGameId(aUpdate.QuestId)));
        auto pScene = Cast<BGSScene>(TESForm::GetById(modSystem.GetGameId(aUpdate.SceneId)));
        const bool bIsLeader = m_world.GetPartyService().IsLeader();
        const auto playerType = bIsLeader ? "leaderId" : "playerId";

        if (!pQuest)
        {
            spdlog::error(__FUNCTION__ ": failed to find questId: {:X}, {} {}", aUpdate.QuestId.LogFormat(), playerType, PlayerId());
            return;
        }
        if (!pScene)
        {
            spdlog::error(__FUNCTION__ ": failed to find sceneId: {:X}, {} {}", aUpdate.SceneId.LogFormat(), playerType, PlayerId());
            return;
        }

        if (aUpdate.SceneType == 0) // Scene Begin
        {
            if (pScene->isPlaying)
                spdlog::debug(
                    __FUNCTION__ ": skip starting scene already playing, questId {:X}, sceneId {:X}, {} {}", aUpdate.QuestId.LogFormat(), aUpdate.SceneId.LogFormat(), playerType,
                    PlayerId());
            else
            {
                spdlog::debug(
                    __FUNCTION__ ": BLOCKED starting scene, causes more bugs than fixed, questId {:X}, sceneId {:X}, {} {}", aUpdate.QuestId.LogFormat(), aUpdate.SceneId.LogFormat(), playerType, PlayerId());
                // pScene->ScriptForceStart();
            }
        }

        else // Scene End
        {
            if (!pScene->isPlaying)
                spdlog::debug(
                    __FUNCTION__ ": skip stopping scene not playing, questId {:X}, sceneId {:X}, {} {}", aUpdate.QuestId.LogFormat(), aUpdate.SceneId.LogFormat(), playerType,
                    PlayerId());
            else
            {
                spdlog::warn(__FUNCTION__ ": FIXME/REVIEW stopping scene questId {:X}, sceneId {:X}, {} {}", aUpdate.QuestId.LogFormat(), aUpdate.SceneId.LogFormat(), playerType, PlayerId());
                pScene->ScriptStop();
            }
        }
    }
    catch (...)
    {
        Diagnostics::ReportFailure(__FUNCTION__, "ran out of memory in QuestService::OnQuestSceneUpdate");
    }
}

bool QuestService::StopQuest(uint32_t aformId)
{
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(aformId));
    if (pQuest)
    {
        if (pQuest->getState() == TESQuest::State::Stopped) // Supress duplicate or loopback quest stop
        {
            spdlog::debug(
                __FUNCTION__ ": suppressing duplicate quest stop formId: {:X}, questStage: {}, questFlags: {:X}, questType: {}, formId: {:X}, name: {}", aformId,
                pQuest->currentStage, static_cast<uint8_t>(pQuest->flags), static_cast<uint16_t>(pQuest->type), aformId, pQuest->fullName.value.AsAscii());
        }
        else
        {
            pQuest->SetActive(false);
            pQuest->SetStopped();
        }

        return true;
    }

    return false;
}

static constexpr std::array kNonSyncableQuestIds = std::to_array<uint32_t>({
    0x2BA16,   // Werewolf transformation quest
    0x20071D0, // Vampire transformation quest
    0x3AC44,   // MS13BleakFallsBarrowLeverScene
    // 0xFE014801,  // Unknown dynamic ID, kept as note, maybe lookup correct ID this game?
    0xF2593 // Skill experience quest
});

bool QuestService::IsNonSyncableQuest(TESQuest* apQuest)
{
    // Quests with no quest stages are never synced. The fork used to keep executed and
    // waiting stages in two separate lists; they are one inline-head list (TESQuest::stages),
    // so a single emptiness check covers both.
    // Most TESQues::Type:: quests should be synced, including Type::None
    // and Type::Miscellaneous, but there are a few // known exceptions to exclude.
    return apQuest->stages.Empty() || std::find(kNonSyncableQuestIds.begin(), kNonSyncableQuestIds.end(), apQuest->formID) != kNonSyncableQuestIds.end();
}

void QuestService::DebugDumpQuests()
{
    auto* pModManager = ModManager::Get();
    if (!pModManager)
    {
        spdlog::warn(__FUNCTION__ ": mod manager unavailable, no quest registry to dump");
        return;
    }

    auto& quests = pModManager->quests;
    for (TESQuest* pQuest : quests)
        spdlog::info("{:X}|{}|{}|{}", pQuest->formID, (uint8_t)pQuest->type, pQuest->priority, pQuest->idName.AsAscii());
}
