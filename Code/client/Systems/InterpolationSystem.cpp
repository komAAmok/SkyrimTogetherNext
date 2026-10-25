#include <TiltedOnlinePCH.h>

#include <Systems/InterpolationSystem.h>
#include <Components.h>

#include <algorithm>
#include <cmath>
#include <iterator>

#include <AI/AIProcess.h>
#include <Misc/MiddleProcess.h>

#include <Games/References.h>
#include <World.h>

namespace
{
// Hard bound on the sample window. The render head normally leaves three or
// four samples behind it; this only matters when the play head does not
// advance, which is what a clock resync looks like from here.
constexpr size_t kMaxSamples = 8;

// How far in the past a sample may still influence the curve. The client reports at
// up to ~33 Hz, so 800 ms is the span the sample cap already covers; this only bites
// after a burst, where the same number of samples covers far less time and the cubic
// ends up fitting almost nothing.
constexpr uint64_t kMaxWindowAgeMs = 800;

// Past the newest snapshot the actor keeps moving for at most this long, then holds
// still until data arrives. Kept well under PlaybackClock's own bounds: the clock
// already eases the play head onto the right tick, this only bridges one late packet.
constexpr double kMaxExtrapolationMs = 150.0;

// Consecutive snapshots further apart than this are a teleport, not movement to
// interpolate across.
constexpr float kTeleportDistance = 1024.f;

// Larger corrections snap instead of sliding the actor into place.
constexpr float kMaxPositionCorrection = 256.f;

// Time constant of the correction fading out, ~95% of it is gone after three times this.
constexpr double kPositionCorrectionTime = 50.0;

// Uniform Catmull-Rom through p1..p2, with p0 and p3 supplying the tangents.
// Linear interpolation gives C0 continuity only, so every snapshot boundary is
// a step change in direction; this smooths those joints out.
glm::vec3 CatmullRom(const glm::vec3& aP0, const glm::vec3& aP1, const glm::vec3& aP2, const glm::vec3& aP3, const float aT) noexcept
{
    const float t2 = aT * aT;
    const float t3 = t2 * aT;

    return 0.5f * (2.f * aP1 + (aP2 - aP0) * aT + (2.f * aP0 - 5.f * aP1 + 4.f * aP2 - aP3) * t2 + (-aP0 + 3.f * aP1 - 3.f * aP2 + aP3) * t3);
}
} // namespace

void InterpolationSystem::Update(Actor* apActor, InterpolationComponent& aInterpolationComponent, const double aNow) noexcept
{
    // The clock is advanced before the early-outs below on purpose: it is what turns
    // the network arrival times into the play head, and it has to keep running while
    // an actor has nothing to interpolate yet, or the first snapshot after a gap
    // would be measured against a stale clock.
    auto& clock = aInterpolationComponent.Clock;
    const double cDeltaTime = clock.Advance(aNow);

    auto& movements = aInterpolationComponent.TimePoints;

    if (movements.size() < 2)
        return;

    const double cRenderTick = clock.GetRenderTick();

    // Keep the window bounded. Newest samples win; dropping from the front is
    // what makes this safe against a play head that has stopped advancing.
    while (movements.size() > kMaxSamples)
        movements.pop_front();

    // ...and bounded by age as well as by count. Eight samples is 800 ms at the
    // old 10 Hz send interval, but a burst right after a stall fills the same eight
    // with samples spanning a few milliseconds, which leaves the cubic almost no
    // geometry to work with once the play head reaches them.
    while (movements.size() > 3 && cRenderTick > static_cast<double>(movements.front().Tick) &&
           cRenderTick - static_cast<double>(movements.front().Tick) > static_cast<double>(kMaxWindowAgeMs))
        movements.pop_front();

    // Advance the window to the active segment, but stop one sample short of
    // it: that sample is the cubic's p0, and without it every segment would
    // start with a tangent of zero. p3, the sample after the segment, is what
    // the window must never give up, hence the size > 3 guard.
    while (movements.size() > 3)
    {
        const auto third = std::next(std::next(movements.begin()));
        if (static_cast<double>(third->Tick) > cRenderTick)
            break;

        movements.pop_front();
    }

    // Walk to the segment that brackets the play head. When it has run past
    // every sample the last pair is kept, so delta comes out above 1 and the
    // curve extrapolates along the final segment instead of freezing.
    auto itFirst = movements.begin();
    auto itSecond = std::next(itFirst);

    while (std::next(itSecond) != movements.end() && static_cast<double>(itSecond->Tick) <= cRenderTick)
    {
        ++itFirst;
        ++itSecond;
    }

    const auto& first = *itFirst;
    const auto& second = *itSecond;

    // Clamping the outer samples to the segment ends degrades the curve to the
    // linear case when the window does not hold them, rather than overshooting.
    const auto& previous = itFirst == movements.begin() ? first : *std::prev(itFirst);
    const auto& following = std::next(itSecond) == movements.end() ? second : *std::next(itSecond);

    const auto cTickDelta = static_cast<double>(second.Tick - first.Tick);
    const bool cIsTeleport = glm::distance(first.Position, second.Position) > kTeleportDistance;

    auto delta = 0.f;
    bool isExtrapolating = false;

    if (cRenderTick <= static_cast<double>(second.Tick))
    {
        if (cTickDelta > 0.0)
            delta = static_cast<float>(std::clamp((cRenderTick - static_cast<double>(first.Tick)) / cTickDelta, 0.0, 1.0));

        // Stay at the old position until the teleport happens
        if (cIsTeleport && delta < 1.f)
            delta = 0.f;
    }
    else if (!cIsTeleport && cTickDelta > 0.0)
    {
        // The next snapshot is late, keep going the way the actor was moving rather
        // than freezing it. Bounded, and the clock keeps easing the play head back
        // onto the owner's timeline, so this only ever bridges one late packet.
        const double cExtrapolationTime = std::min(cRenderTick - static_cast<double>(second.Tick), kMaxExtrapolationMs);
        delta = static_cast<float>(1.0 + cExtrapolationTime / cTickDelta);
        isExtrapolating = true;
    }
    else
    {
        // A teleport is not worth following, and a zero-length segment has no
        // direction to follow: hold the last known position instead.
        delta = 1.f;
    }

    const NiPoint3 position{CatmullRom(previous.Position, first.Position, second.Position, following.Position, delta)};

    auto& positionError = aInterpolationComponent.PositionError;

    // The late snapshot arrived, so the actor is no longer where extrapolation put it
    if (aInterpolationComponent.HasNewTimePoint && aInterpolationComponent.IsExtrapolating)
        positionError = aInterpolationComponent.Position - glm::vec3(position);

    if (cIsTeleport || glm::length(positionError) > kMaxPositionCorrection)
        positionError = {};
    else
        positionError *= static_cast<float>(std::exp(-cDeltaTime / kPositionCorrectionTime));

    aInterpolationComponent.HasNewTimePoint = false;
    aInterpolationComponent.IsExtrapolating = isExtrapolating;
    aInterpolationComponent.Position = glm::vec3(position) + positionError;

    // Don't try to move a null actor
    if (!apActor)
        return;

    apActor->ForcePosition(aInterpolationComponent.Position);
    apActor->LoadAnimationVariables(second.Variables);

    if (apActor->currentProcess && apActor->currentProcess->middleProcess)
    {
        apActor->currentProcess->middleProcess->direction = second.Direction;
    }

    auto rotA = first.Rotation;
    auto rotB = second.Rotation;

    const auto deltaX = TiltedPhoques::DeltaAngle(rotA.x, rotB.x, true) * delta;
    const auto deltaY = TiltedPhoques::DeltaAngle(rotA.y, rotB.y, true) * delta;
    const auto deltaZ = TiltedPhoques::DeltaAngle(rotA.z, rotB.z, true) * delta;

    auto finalX = TiltedPhoques::Mod(rotA.x + deltaX, float(TiltedPhoques::Pi * 2));
    if (finalX > 0.f && finalX > float(TiltedPhoques::Pi / 2))
        finalX -= TiltedPhoques::Pi * 2;

    const auto finalY = TiltedPhoques::Mod(rotA.y + deltaY, float(TiltedPhoques::Pi * 2));
    const auto finalZ = TiltedPhoques::Mod(rotA.z + deltaZ, float(TiltedPhoques::Pi * 2));

    apActor->SetRotation(finalX, finalY, finalZ);
}

void InterpolationSystem::AddPoint(InterpolationComponent& aInterpolationComponent, InterpolationComponent::TimePoint aPoint, const double aArrivalTime) noexcept
{
    // A point that could not be stored is not interpolated; the next packet carries
    // a fresh one. Both containers below can allocate, so the whole body is guarded.
    try
    {
        // A duplicate carries no new timing information and would skew the estimates,
        // the clock is the single authority on that and reports it here.
        if (!aInterpolationComponent.Clock.AddSample(aArrivalTime, aPoint.Tick))
            return;

        auto& timePoints = aInterpolationComponent.TimePoints;

        // Snapshots are unreliable and can arrive out of order
        auto itor = std::begin(timePoints);
        const auto end = std::cend(timePoints);

        while (itor != end && itor->Tick < aPoint.Tick)
            ++itor;

        timePoints.insert(itor, std::move(aPoint));
        aInterpolationComponent.HasNewTimePoint = true;

        // The clock's own buffer is the same size, so a trimmed point never comes
        // back as something the clock could have kept.
        while (timePoints.size() > PlaybackClock::cMaxSamples)
            timePoints.pop_front();
    }
    catch (...)
    {
        Diagnostics::ReportFailure(__FUNCTION__, "ran out of memory in InterpolationSystem::AddPoint");
    }
}

uint64_t InterpolationSystem::GetPlaybackTick(const InterpolationComponent& acInterpolationComponent) noexcept
{
    // The render tick stays at 0 until the first snapshot arrives
    return static_cast<uint64_t>(std::max(acInterpolationComponent.Clock.GetRenderTick(), 0.0));
}

InterpolationComponent& InterpolationSystem::Setup(World& aWorld, const entt::entity aEntity) noexcept
{
    return aWorld.emplace_or_replace<InterpolationComponent>(aEntity);
}

void InterpolationSystem::Clean(World& aWorld, const entt::entity aEntity) noexcept
{
    if (aWorld.all_of<InterpolationComponent>(aEntity))
        aWorld.remove<InterpolationComponent>(aEntity);
}
