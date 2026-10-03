#include "scene/ConstructionLogistics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <ostream>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "animation/ConstructionAnimation.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"
#include "scene/SupportSystem.h"
#include "objects/Sledge.h"
#include "scene/SandSimulation.h"
#include <iostream>

namespace
{
constexpr glm::vec3 cargoSocketOffset{0.0f, 1.36f, -0.02f};
constexpr glm::vec3 physicalBlockScale{2.6f, 1.6f, 2.4f};
constexpr float placementPositionTolerance = 1.0e-3f;
constexpr float placementOrientationToleranceDegrees = 0.05f;
constexpr float placementScaleTolerance = 1.0e-3f;
constexpr std::size_t postLiftWaypointCount = 10;
constexpr std::array<glm::vec3, 2> pullingWorkerOffsets{{
    {-0.72f, 0.0f, -4.70f},
    {0.72f, 0.0f, -4.70f}}};
constexpr std::array<glm::vec3, 2> sledgeTowPointOffsets{{
    {-0.46f, 0.58f, -2.44f},
    {0.46f, 0.58f, -2.44f}}};
constexpr float workerGroundClearance = 0.015f;

bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

float headingDegreesFor(const glm::vec3& direction)
{
    return glm::degrees(std::atan2(-direction.x, -direction.z));
}

glm::vec3 formationOffset(std::size_t crewIndex)
{
    if (crewIndex >= pullingWorkerOffsets.size())
        throw std::out_of_range("Physical pulling-crew index is invalid");
    return pullingWorkerOffsets[crewIndex];
}

struct HorizontalRouteSample
{
    glm::vec2 point{0.0f};
    glm::vec2 direction{0.0f, -1.0f};
};

float horizontalRouteLength(const std::vector<glm::vec2>& points)
{
    float length = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i)
        length += glm::distance(points[i - 1], points[i]);
    return length;
}

std::vector<glm::vec2> upperTransferStageOneWaypoints()
{
    const UpperAccessLayout& access = MonumentalSite::upperAccessLayout();
    const RampDescriptor& ramp = MonumentalSite::upperRampA();
    std::vector<glm::vec2> points{access.departure,
        {access.departure.x, access.apronCenter.y}};
    // Quarter-circle: southbound deck exit turns smoothly toward the west.
    constexpr int apronSegments = 48;
    for (int i = 1; i <= apronSegments; ++i)
    {
        const float angle = glm::half_pi<float>() * i / apronSegments;
        points.push_back(access.apronCenter + access.apronRadius *
            glm::vec2{std::cos(angle), std::sin(angle)});
    }
    points.push_back({ramp.base.x, ramp.base.z});
    points.push_back({access.turnCenter.x, ramp.top.z});
    // The high landing carries a rolling 108-degree turn.
    constexpr int landingSegments = 64;
    for (int i = 1; i <= landingSegments; ++i)
    {
        const float angle = access.turnRadians * i / landingSegments;
        points.push_back(access.turnCenter + access.turnRadius *
            glm::vec2{-std::sin(angle), std::cos(angle)});
    }
    return points;
}

std::vector<glm::vec2> upperTransferWaypoints()
{
    std::vector<glm::vec2> points = upperTransferStageOneWaypoints();
    const RampDescriptor& ramp = MonumentalSite::upperRampB();
    const UpperAccessLayout& access = MonumentalSite::upperAccessLayout();
    // The first point is the as-built rolling-turn endpoint, which is also
    // the declared visible toe of Ramp B.
    points.push_back({ramp.top.x, ramp.top.z});
    points.push_back(access.targetParking);
    return points;
}

HorizontalRouteSample horizontalRouteAt(const std::vector<glm::vec2>& points,
                                        float distance)
{
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const glm::vec2 delta = points[i] - points[i - 1];
        const float segment = glm::length(delta);
        if (distance > segment && i + 1 < points.size())
        {
            distance -= segment;
            continue;
        }
        const glm::vec2 direction = delta / segment;
        // The pulling crew may walk past the parked sledge on the same deck.
        return {points[i - 1] + direction * distance, direction};
    }
    return {points.front(), {0.0f, -1.0f}};
}

glm::vec3 quarryLoadingSledgePosition()
{
    return ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::SledgeLoading, 0.0f).sledgePosition;
}

struct PostLiftRoute
{
    std::array<glm::vec3, postLiftWaypointCount> points{};
    std::array<float, postLiftWaypointCount> cumulativeDistance{};
    float length = 0.0f;
};

struct PostLiftRouteSample
{
    glm::vec3 position{0.0f};
    std::size_t segment = 0;
    float segmentProgress = 0.0f;
};

glm::vec3 rampCargoCenter(const RampDescriptor& ramp, float progress)
{
    return MonumentalSite::rampSurfacePoint(ramp, progress) +
           glm::vec3{0.0f, 0.5f * physicalBlockScale.y, 0.0f};
}

glm::vec3 targetLevelAccessPoint(const PyramidBlockPlacement& target)
{
    if (target.level == 0)
    {
        const PyramidLayoutConfig config;
        const float zStep = config.blockDepth + config.horizontalSpacing;
        const unsigned int side = config.baseBlocksPerSide - target.level;
        const float halfDepth = 0.5f * static_cast<float>(side - 1) * zStep +
                                0.5f * config.blockDepth;
        const float supportY = target.position.y - 0.5f * target.scale.y;
        return {config.origin.x,
                supportY + 0.5f * physicalBlockScale.y,
                config.origin.z + halfDepth + 4.0f};
    }

    const float targetSupportY =
        target.position.y - 0.5f * target.scale.y;
    const char* rampId = targetSupportY <= 2.35f
                             ? "MainHaulingLow"
                             : targetSupportY <= 5.25f
                                   ? "MainHaulingMiddle"
                                   : "MainHaulingRamp";
    const RampDescriptor* ramp = MonumentalSite::findRamp(rampId);
    if (ramp == nullptr)
        throw std::runtime_error("Post-lift route cannot find its target-level ramp");

    float low = 0.0f;
    float high = 1.0f;
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        const float middle = 0.5f * (low + high);
        if (MonumentalSite::rampSurfacePoint(*ramp, middle).y < targetSupportY)
            low = middle;
        else
            high = middle;
    }
    return rampCargoCenter(*ramp, 0.5f * (low + high));
}

PostLiftRoute makePostLiftRoute(const PyramidBlockPlacement& target)
{
    const PyramidLayoutConfig config;
    if (target.level >= config.baseBlocksPerSide)
        throw std::runtime_error("Post-lift target level is outside the pyramid");

    const RampDescriptor* quarryExit =
        MonumentalSite::findRamp("QuarryExitRamp");
    if (quarryExit == nullptr)
        throw std::runtime_error("Post-lift route cannot find QuarryExitRamp");

    const float xStep = config.blockWidth + config.horizontalSpacing;
    const float zStep = config.blockDepth + config.horizontalSpacing;
    const unsigned int side = config.baseBlocksPerSide - target.level;
    const float halfWidth = 0.5f * static_cast<float>(side - 1) * xStep +
                            0.5f * config.blockWidth;
    const float halfDepth = 0.5f * static_cast<float>(side - 1) * zStep +
                            0.5f * config.blockDepth;
    const float sideSign = target.position.x < config.origin.x ? -1.0f : 1.0f;
    const float supportY = target.position.y - 0.5f * target.scale.y;
    const float carriedCenterY = supportY + 0.5f * physicalBlockScale.y;
    const float frontOutsideZ = config.origin.z + halfDepth + 4.0f;
    const float sideOutsideX = config.origin.x +
                               sideSign * (halfWidth + 4.0f);
    const float approachOffset = 0.5f * target.scale.x +
                                 0.5f * physicalBlockScale.x + 0.35f;

    PostLiftRoute route;
    route.points = {{
        QuarryPulleyAnimationController::destinationLoadPosition(),
        rampCargoCenter(*quarryExit, 0.0f),
        rampCargoCenter(*quarryExit, 1.0f),
        {-10.0f, 0.89f, 42.0f},
        {0.0f, 0.89f, 45.0f},
        targetLevelAccessPoint(target),
        {sideOutsideX, carriedCenterY, frontOutsideZ},
        {sideOutsideX, carriedCenterY, target.position.z},
        {target.position.x + sideSign * approachOffset,
         carriedCenterY, target.position.z},
        target.position}};

    route.cumulativeDistance[0] = 0.0f;
    for (std::size_t index = 1; index < route.points.size(); ++index)
    {
        const float segmentLength =
            glm::distance(route.points[index - 1], route.points[index]);
        if (!std::isfinite(segmentLength) || segmentLength <= 1.0e-4f)
            throw std::runtime_error("Post-lift route contains a degenerate segment");
        route.length += segmentLength;
        route.cumulativeDistance[index] = route.length;
    }
    return route;
}

PostLiftRouteSample samplePostLiftRoute(const PostLiftRoute& route,
                                        float progress)
{
    const float distance =
        std::clamp(progress, 0.0f, 1.0f) * route.length;
    for (std::size_t index = 1; index < route.points.size(); ++index)
    {
        if (distance > route.cumulativeDistance[index] &&
            index + 1 < route.points.size())
            continue;
        const float startDistance = route.cumulativeDistance[index - 1];
        const float segmentLength =
            route.cumulativeDistance[index] - startDistance;
        const float local = std::clamp(
            (distance - startDistance) / segmentLength, 0.0f, 1.0f);
        return {glm::mix(route.points[index - 1], route.points[index], local),
                index - 1, local};
    }
    return {route.points.back(), route.points.size() - 2, 1.0f};
}

const ConstructionBlock* findActiveBlock(const ConstructionSimulation& simulation,
                                         std::uint64_t blockId)
{
    const auto found = std::find_if(
        simulation.activeBlocks.begin(), simulation.activeBlocks.end(),
        [blockId](const ConstructionBlock& block) { return block.id == blockId; });
    return found == simulation.activeBlocks.end() ? nullptr : &*found;
}

float stateBaseDuration(LogisticsState state)
{
    switch (state)
    {
    case LogisticsState::QuarryReady: return 2.0f;
    case LogisticsState::Extracting: return 5.0f;
    case LogisticsState::Staged: return 2.5f;
    case LogisticsState::SledgeLoading: return 3.5f;
    case LogisticsState::Hauling: return 52.0f;
    case LogisticsState::RampApproach: return 5.0f;
    case LogisticsState::RampAscent: return 8.0f;
    case LogisticsState::UpperTransfer:
    {
        static const float duration = 7.5f *
            horizontalRouteLength(upperTransferWaypoints()) /
            horizontalRouteLength(upperTransferStageOneWaypoints());
        return duration;
    }
    case LogisticsState::LiftPrep: return 3.0f;
    case LogisticsState::Lifting: return 6.0f;
    case LogisticsState::QuarryPlatformTransfer: return 4.0f;
    case LogisticsState::Placement: return 30.0f;
    case LogisticsState::Settled: return 2.0f;
    default: return 3.0f;
    }
}
} // namespace

ConstructionLogistics::ConstructionLogistics()
{
    reset();
}

void ConstructionLogistics::reset()
{
    state_ = LogisticsState::QuarryReady;
    activeBlockId_ = 0;
    activeBlockNumber_ = 101;
    stateProgress_ = 0.0f;
    stateTimer_ = 0.0f;
    blockPosition_ = glm::vec3{-121.0f, -6.65f, -3.0f};
    blockRotation_ = glm::vec3{0.0f};
    blockScale_ = physicalBlockScale;
    const LogisticsSnapshot staged = samplePhysicalRoute(LogisticsState::SledgeLoading, 0.0f, supportProgress_);
    sledgePosition_ = staged.sledgePosition;
    sledgeHeading_ = staged.sledgeHeading;
    sledgePitch_ = staged.sledgePitch;
    ropeTaut_ = false;
    activeWorkers_ = 4;
    liftHeight_ = 0.0f;
    justSettled_ = false;
    settledFlashTimer_ = 0.0f;
    previousPostLiftPosition_ = glm::vec3{0.0f};
    largestPostLiftDisplacement_ = 0.0f;
    hasPreviousPostLiftPosition_ = false;
}

const char* ConstructionLogistics::stateName(LogisticsState state)
{
    switch (state)
    {
    case LogisticsState::QuarryReady: return "Quarry Ready";
    case LogisticsState::Extracting: return "Extracting Block";
    case LogisticsState::Staged: return "Block Staged";
    case LogisticsState::SledgeLoading: return "Loading Sledge";
    case LogisticsState::Hauling: return "Hauling Sledge";
    case LogisticsState::RampApproach: return "Approaching Ramp";
    case LogisticsState::RampAscent: return "Ramp Ascent";
    case LogisticsState::UpperTransfer: return "Upper Access Transfer";
    case LogisticsState::LiftPrep: return "Lift Preparation";
    case LogisticsState::Lifting: return "Quarry Floor -> Receiving Platform Lift";
    case LogisticsState::QuarryPlatformTransfer: return "Quarry Platform Transfer";
    case LogisticsState::Placement: return "Maneuvering to Target Cell";
    case LogisticsState::Settled: return "Block Settled -> Permanent Structure";
    default: return "Unknown";
    }
}

const char* ConstructionLogistics::routeDescription() const
{
    switch (state_)
    {
    case LogisticsState::QuarryReady:
    case LogisticsState::Extracting:
    case LogisticsState::Staged: return "Quarry Bed -> Floor Staging -> Quarry Pulley";
    case LogisticsState::LiftPrep:
    case LogisticsState::Lifting:
    case LogisticsState::QuarryPlatformTransfer: return "Quarry Pulley -> Receiving Platform";
    case LogisticsState::SledgeLoading: return "Receiving Platform -> Physical Sledge";
    case LogisticsState::Hauling: return "Transport Corridor -> Ramp Base";
    case LogisticsState::RampApproach:
    case LogisticsState::RampAscent: return "Pyramid Construction Ramp";
    case LogisticsState::UpperTransfer: return "Upper Deck -> West Ramp -> Target Landing";
    case LogisticsState::Placement: return "Target Cell Transfer";
    case LogisticsState::Settled: return "Integrated into Pyramid Core";
    default: return "";
    }
}

LogisticsSnapshot ConstructionLogistics::snapshot() const
{
    LogisticsSnapshot s;
    s.state = state_;
    s.blockIndex = activeBlockNumber_;
    s.stateProgress = stateProgress_;
    s.blockPosition = blockPosition_;
    s.blockRotation = blockRotation_;
    s.sledgePosition = sledgePosition_;
    s.sledgeHeading = sledgeHeading_;
    s.sledgePitch = sledgePitch_;
    s.supportProgress = supportProgress_;
    s.ropeTaut = ropeTaut_;
    s.activeWorkers = activeWorkers_;
    s.liftHeight = liftHeight_;
    s.blockSettled = justSettled_;
    s.routeDescription = routeDescription();
    return s;
}

bool ConstructionLogistics::isPhysicalHaulingState(LogisticsState state)
{
    return state == LogisticsState::SledgeLoading ||
           state == LogisticsState::Hauling ||
           state == LogisticsState::RampApproach ||
           state == LogisticsState::RampAscent ||
           state == LogisticsState::UpperTransfer;
}

glm::mat4 ConstructionLogistics::physicalSledgeRoot(
    const LogisticsSnapshot& snapshot)
{
    if (!finiteVector(snapshot.sledgePosition) ||
        !std::isfinite(snapshot.sledgeHeading) ||
        !std::isfinite(snapshot.sledgePitch))
        throw std::invalid_argument("Physical sledge snapshot must be finite");
    return makeTransform(snapshot.sledgePosition,
                         {snapshot.sledgePitch, snapshot.sledgeHeading, 0.0f},
                         {1.0f, 1.0f, 1.0f});
}

glm::vec3 ConstructionLogistics::haulingDirection(
    const LogisticsSnapshot& snapshot)
{
    const glm::mat4 root = physicalSledgeRoot(snapshot);
    return glm::normalize(glm::vec3{root * glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}});
}

float ConstructionLogistics::haulingSupportHeight(
    const LogisticsSnapshot& snapshot, const glm::vec2& worldPoint)
{
    return SceneSupport::transportAt(worldPoint, snapshot.supportProgress).height;
}

std::vector<glm::vec2> ConstructionLogistics::physicalRouteWaypoints(LogisticsState state)
{
    // From the receiving deck, cross the supported bridge and east-wall exit,
    // then skirt the repositories through the open northern desert.
    const std::vector<glm::vec2> haul{{-108.95f, -10.0f},
        {-108.77f, -9.65f}, {-108.05f, -9.45f}, {-106.2f, -9.4f},
        {-102.5f, -2.0f}, {-97.0f, 25.0f},
        {-97.0f, 30.0f}, {-90.0f, 38.0f}, {-38.0f, 58.0f}, {-10.0f, 58.0f}};
    if (state == LogisticsState::SledgeLoading)
        return {{-108.95f, -10.0f}, {-108.95f, -7.0f}};
    if (state == LogisticsState::Hauling)
        return haul;
    if (state == LogisticsState::RampApproach)
        return {{-10.0f, 58.0f}, {-6.0f, 58.0f}, {-3.0f, 57.0f},
            {-1.0f, 55.0f}, {0.0f, 52.0f}};
    if (state == LogisticsState::UpperTransfer)
        return upperTransferWaypoints();
    const RampDescriptor& ramp = MonumentalSite::mainRamp();
    const UpperWorkDeckPanel& westDeck = MonumentalSite::upperWorkDeckPanels()[1];
    const glm::vec3 toe = SceneSupport::transportRampToe(ramp).base;
    const float turnZ = ramp.top.z - 0.30f;
    const float stagingX = westDeck.center.x + 1.25f;
    return {{toe.x, toe.z}, {ramp.base.x, ramp.base.z},
            {ramp.top.x, ramp.top.z}, {ramp.top.x, turnZ},
            {stagingX, turnZ}, {stagingX, 0.0f}};
}

LogisticsSnapshot ConstructionLogistics::samplePhysicalRoute(
    LogisticsState state, float progress, float constructionProgress)
{
    LogisticsSnapshot result;
    result.state = state;
    result.stateProgress = progress;
    result.supportProgress = constructionProgress;
    const auto points = physicalRouteWaypoints(state);
    const float length = horizontalRouteLength(points);
    const float routeDistance = state == LogisticsState::SledgeLoading ? 0.0f :
        std::clamp(progress, 0.0f, 1.0f) * length;
    float distance = routeDistance;
    glm::vec2 point = points.front();
    glm::vec2 direction{0.0f, -1.0f};
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const float segment = glm::distance(points[i - 1], points[i]);
        if (distance > segment && i + 1 < points.size())
        {
            distance -= segment;
            continue;
        }
        point = glm::mix(points[i - 1], points[i], std::clamp(distance / segment, 0.0f, 1.0f));
        direction = glm::normalize(points[i] - points[i - 1]);
        break;
    }
    result.sledgePosition = {point.x, 0.0f, point.y};
    if (state == LogisticsState::RampAscent ||
        state == LogisticsState::UpperTransfer)
    {
        const glm::vec2 before = horizontalRouteAt(points,
            std::max(0.0f, routeDistance - 0.8f)).point;
        const glm::vec2 after = horizontalRouteAt(points, routeDistance + 0.8f).point;
        direction = glm::normalize(after - before);
    }
    result.sledgeHeading = headingDegreesFor({direction.x, 0.0f, direction.y});
    const glm::vec2 side{-direction.y, direction.x};
    const auto endSupport = [&](float along)
    {
        float height = -std::numeric_limits<float>::infinity();
        for (float lateral : {-0.62f, 0.0f, 0.62f})
            height = std::max(height, SceneSupport::transportAt(
                point + direction * along + side * lateral, constructionProgress).height);
        return height;
    };
    const float forwardHeight = endSupport(1.375f);
    const float rearHeight = endSupport(-1.375f);
    // The rigid runners bridge narrow seams at deck/ramp junctions. A probe
    // that falls beside a bridge must not pitch the whole loaded sledge 65°.
    result.sledgePitch = std::clamp(
        glm::degrees(std::atan2(forwardHeight - rearHeight, 2.75f)), -25.0f, 25.0f);
    const SupportSurface forward = SceneSupport::transportAt(point + direction * 1.375f, constructionProgress);
    const SupportSurface rear = SceneSupport::transportAt(point - direction * 1.375f, constructionProgress);
    const int contactSamples = forward.kind == SupportSurfaceKind::RampSurface ||
        rear.kind == SupportSurfaceKind::RampSurface ? 32 : 8;
    const glm::mat4 rotation = physicalSledgeRoot(result);
    float rootY = -std::numeric_limits<float>::infinity();
    // Both runners share a rigid frame, measured against actual support faces.
    for (float x : {-0.74f, -0.50f, 0.50f, 0.74f})
    {
        const auto contactAt = [&](float t)
        {
            const float z = -1.375f + 2.75f * t;
            const glm::vec3 bottom{rotation * glm::vec4{x, Sledge::runnerBottomLocalY, z, 1.0f}};
            rootY = std::max(rootY, SceneSupport::transportAt({bottom.x, bottom.z}, constructionProgress).height - bottom.y);
        };
        for (int sample = 0; sample <= contactSamples; ++sample)
            contactAt(static_cast<float>(sample) / contactSamples);
        // The terrain is a triangle mesh. Along a straight runner edge, the
        // exact height extrema occur at grid or triangle-diagonal crossings.
        // Include those crossings rather than missing a narrow mesh crease.
        const glm::vec3 from{rotation * glm::vec4{x, Sledge::runnerBottomLocalY, -1.375f, 1.0f}};
        const glm::vec3 to{rotation * glm::vec4{x, Sledge::runnerBottomLocalY, 1.375f, 1.0f}};
        const glm::vec2 gridFrom{(from.x - SandSimulation::WorldMinX) / SandSimulation::CellSizeX,
            (from.z - SandSimulation::WorldMinZ) / SandSimulation::CellSizeZ};
        const glm::vec2 gridTo{(to.x - SandSimulation::WorldMinX) / SandSimulation::CellSizeX,
            (to.z - SandSimulation::WorldMinZ) / SandSimulation::CellSizeZ};
        const auto crossings = [&](float start, float end)
        {
            if (std::abs(end - start) < 1.0e-6f) return;
            for (int edge = static_cast<int>(std::ceil(std::min(start, end)));
                 edge <= static_cast<int>(std::floor(std::max(start, end))); ++edge)
            {
                const float t = (edge - start) / (end - start);
                if (t >= 0.0f && t <= 1.0f) contactAt(t);
            }
        };
        crossings(gridFrom.x, gridTo.x);
        crossings(gridFrom.y, gridTo.y);
        crossings(gridFrom.x + gridFrom.y, gridTo.x + gridTo.y);
    }
    result.sledgePosition.y = rootY + 0.015f;
    return result;
}

glm::mat4 ConstructionLogistics::physicalWorkerRoot(
    const LogisticsSnapshot& snapshot, std::size_t crewIndex,
    const WorkerJointAngles& angles)
{
    glm::vec3 anchor{physicalSledgeRoot(snapshot) *
                     glm::vec4{formationOffset(crewIndex), 1.0f}};
    float workerHeading = snapshot.sledgeHeading;
    if (snapshot.state == LogisticsState::RampAscent ||
        snapshot.state == LogisticsState::UpperTransfer)
    {
        const auto route = physicalRouteWaypoints(snapshot.state);
        const float distance = snapshot.stateProgress * horizontalRouteLength(route) + 4.70f;
        const HorizontalRouteSample ahead = horizontalRouteAt(route, distance);
        const glm::vec2 side{-ahead.direction.y, ahead.direction.x};
        const float lateral = formationOffset(crewIndex).x;
        anchor.x = ahead.point.x + lateral * side.x;
        anchor.z = ahead.point.y + lateral * side.y;
        workerHeading = headingDegreesFor({ahead.direction.x, 0.0f, ahead.direction.y});
    }
    // The transport frame supplies position and heading. Workers remain
    // upright while their feet are grounded to its inclined support plane.
    glm::mat4 root = makeTransform(anchor, {0.0f, workerHeading, 0.0f},
                                   {1.0f, 1.0f, 1.0f});
    const Worker::EvaluatedPose ungrounded = Worker::evaluate(root, angles);
    const float leftFootBottom = SceneSupport::transformedBottomY(
        ungrounded[static_cast<std::size_t>(BodyPart::LeftFoot)].model);
    const float rightFootBottom = SceneSupport::transformedBottomY(
        ungrounded[static_cast<std::size_t>(BodyPart::RightFoot)].model);
    const float footBottom = std::min(leftFootBottom, rightFootBottom);
    const glm::vec3 position{root[3]};
    const float support = haulingSupportHeight(
        snapshot, {position.x, position.z});
    root[3].y += support + workerGroundClearance - footBottom;
    return root;
}

glm::vec3 ConstructionLogistics::pullingHandPosition(
    const Worker::EvaluatedPose& pose, std::size_t crewIndex)
{
    if (crewIndex >= pullingCrewSize())
        throw std::out_of_range("Physical pulling-hand index is invalid");
    const BodyPart hand = crewIndex == 0 ? BodyPart::RightHand
                                         : BodyPart::LeftHand;
    return glm::vec3{pose[static_cast<std::size_t>(hand)].jointWorld[3]};
}

glm::vec3 ConstructionLogistics::physicalTowPoint(
    const LogisticsSnapshot& snapshot, std::size_t crewIndex)
{
    if (crewIndex >= sledgeTowPointOffsets.size())
        throw std::out_of_range("Physical tow-point index is invalid");
    return glm::vec3{physicalSledgeRoot(snapshot) *
                     glm::vec4{sledgeTowPointOffsets[crewIndex], 1.0f}};
}

glm::vec3 ConstructionLogistics::computeHaulPosition(float progress) const
{
    return samplePhysicalRoute(LogisticsState::Hauling, progress, supportProgress_).sledgePosition;
}

glm::vec3 ConstructionLogistics::computeRampPosition(float progress) const
{
    return samplePhysicalRoute(LogisticsState::RampAscent, progress, supportProgress_).sledgePosition;
}

glm::vec3 ConstructionLogistics::computePlacementPosition(float progress, ConstructionSimulation& simulation) const
{
    const ConstructionBlock* block = findActiveBlock(simulation, activeBlockId_);
    if (block == nullptr)
        return QuarryPulleyAnimationController::destinationLoadPosition();
    return samplePostLiftRoute(makePostLiftRoute(block->targetPlacement), progress)
        .position;
}

void ConstructionLogistics::settlePlacementIfSpatiallyReady(
    ConstructionSimulation& simulation,
    ConstructionTimelineController& timeline)
{
    if (state_ != LogisticsState::Placement || stateProgress_ < 1.0f)
        return;

    ConstructionBlock* block = simulation.getBlock(activeBlockId_);
    if (block == nullptr)
        throw std::runtime_error("Placement lost its authoritative block");

    const float positionError =
        glm::distance(block->position, block->targetPlacement.position);
    const float orientationError = glm::length(block->rotation);
    const float scaleError = glm::length(block->scale - block->targetPlacement.scale);
    if (positionError > placementPositionTolerance ||
        orientationError > placementOrientationToleranceDegrees ||
        scaleError > placementScaleTolerance)
        return;

    const std::size_t targetIndex = block->targetIndex;
    if (simulation.isTargetOccupied(targetIndex) ||
        !simulation.settleBlock(activeBlockId_))
        throw std::runtime_error(
            "Construction block spatial settlement rejected for invalid or occupied target");

    state_ = LogisticsState::Settled;
    stateTimer_ = 0.0f;
    stateProgress_ = 0.0f;
    activeWorkers_ = 4;
    ropeTaut_ = false;
    justSettled_ = true;
    settledFlashTimer_ = 1.5f;
    blockPosition_ = block->targetPlacement.position;
    blockRotation_ = glm::vec3{0.0f};
    blockScale_ = block->targetPlacement.scale;
    timeline.registerPhysicalBlockSettlement(simulation);

    if (activeBlockId_ == 1000)
    {
        std::cout << "\n[POST-LIFT TRACE]\n"
                  << "Block " << activeBlockId_ << "\n"
                  << "SETTLED: (" << blockPosition_.x << ", "
                  << blockPosition_.y << ", " << blockPosition_.z << ")\n"
                  << "largest observed frame-to-frame displacement: "
                  << largestPostLiftDisplacement_ << std::endl;
    }

    std::cout << "\n[RUNTIME CONSTRUCTION]\n"
              << "Block " << activeBlockNumber_ << "\n"
              << "State: " << static_cast<int>(LogisticsState::Placement)
              << " -> " << static_cast<int>(state_) << "\n"
              << "Block position: (" << blockPosition_.x << ", "
              << blockPosition_.y << ", " << blockPosition_.z << ")\n"
              << "Sledge position: (" << sledgePosition_.x << ", "
              << sledgePosition_.y << ", " << sledgePosition_.z << ")\n"
              << "Settled count: " << simulation.settledCount() << std::endl;
}

void ConstructionLogistics::advanceState(ConstructionSimulation& simulation, QuarrySystem& quarry,
                                        QuarryPulleyAnimationController& pulley,
                                        bool physicalLiftEnabled)
{
    const LogisticsState oldState = state_;
    stateTimer_ = 0.0f;
    stateProgress_ = 0.0f;

    switch (state_)
    {
    case LogisticsState::QuarryReady:
    {
        for (auto& block : simulation.activeBlocks) {
            if (block.state == BlockState::Staged) {
                activeBlockId_ = block.id;
                activeBlockNumber_ = static_cast<int>(block.id);
                blockScale_ = physicalBlockScale;
                state_ = LogisticsState::Staged;
                activeWorkers_ = 6;
                ropeTaut_ = false;
                quarry.markCurrentDepositTransported();
                break;
            }
        }
        break;
    }

    case LogisticsState::Extracting:
        break;

    case LogisticsState::Staged:
        state_ = LogisticsState::LiftPrep;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.setPhysicalPreparationProgress(activeBlockId_, 0.0f))
            throw std::runtime_error("Failed to attach authoritative block to quarry pulley");
        break;

    case LogisticsState::SledgeLoading:
        state_ = LogisticsState::Hauling;
        activeWorkers_ = 8;
        ropeTaut_ = true;
        break;

    case LogisticsState::Hauling:
        state_ = LogisticsState::RampApproach;
        activeWorkers_ = 8;
        ropeTaut_ = true;
        break;

    case LogisticsState::RampApproach:
        state_ = LogisticsState::RampAscent;
        activeWorkers_ = 8;
        ropeTaut_ = true;
        break;

    case LogisticsState::RampAscent:
        state_ = LogisticsState::UpperTransfer;
        activeWorkers_ = 8;
        ropeTaut_ = true;
        break;

    case LogisticsState::UpperTransfer:
        // Stop with the block aboard on the supported target-course landing.
        break;

    case LogisticsState::LiftPrep:
        state_ = LogisticsState::Lifting;
        activeWorkers_ = 6;
        ropeTaut_ = true;
        if (physicalLiftEnabled &&
            !pulley.setPhysicalLiftProgress(activeBlockId_, 0.0f))
            throw std::runtime_error(
                "Physical pulley lost the authoritative block before lifting");
        break;

    case LogisticsState::Lifting:
        state_ = LogisticsState::QuarryPlatformTransfer;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.holdPhysicalBlockAtLiftTop(activeBlockId_))
            throw std::runtime_error(
                "Physical pulley failed to retain the block at lift top");
        previousPostLiftPosition_ =
            QuarryPulleyAnimationController::raisedLoadPosition();
        largestPostLiftDisplacement_ = 0.0f;
        hasPreviousPostLiftPosition_ = true;
        break;

    case LogisticsState::QuarryPlatformTransfer:
        if (physicalLiftEnabled &&
            !pulley.setPhysicalTransferProgress(activeBlockId_, 1.0f))
            throw std::runtime_error(
                "Physical pulley failed to complete the platform transfer");
        state_ = LogisticsState::SledgeLoading;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.releasePhysicalBlock(activeBlockId_))
            throw std::runtime_error(
                "Physical pulley released an unexpected block identity");
        break;

    case LogisticsState::Placement:
        // Placement completion is spatially gated after the authoritative
        // block pose has been updated, not triggered by elapsed time here.
        break;

    case LogisticsState::Settled:
        state_ = LogisticsState::QuarryReady;
        activeBlockId_ = 0;
        blockScale_ = physicalBlockScale;
        hasPreviousPostLiftPosition_ = false;
        justSettled_ = false;
        break;
    }

    if (state_ != oldState)
    {
        std::cout << "\n[RUNTIME CONSTRUCTION]\n"
                  << "Block " << activeBlockNumber_ << "\n"
                  << "State: " << static_cast<int>(oldState) << " -> " << static_cast<int>(state_) << "\n"
                  << "Block position: (" << blockPosition_.x << ", " << blockPosition_.y << ", " << blockPosition_.z << ")\n"
                  << "Sledge position: (" << sledgePosition_.x << ", " << sledgePosition_.y << ", " << sledgePosition_.z << ")\n"
                  << "Settled count: " << simulation.settledCount() << std::endl;
    }
}

void ConstructionLogistics::update(float deltaTime, ConstructionSimulation& simulation,
                                   QuarrySystem& quarry,
                                   ConstructionTimelineController& timeline,
                                   QuarryPulleyAnimationController& pulley,
                                   bool physicalLiftEnabled)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    const float duration = stateBaseDuration(state_);
    // Consume transport-state boundaries exactly; retain all remaining time.
    // Lift and placement timing are outside this repair.
    if (state_ >= LogisticsState::Staged && state_ <= LogisticsState::UpperTransfer)
    {
        const float remaining = std::max(0.0f, duration - stateTimer_);
        if (remaining > 1.0e-5f && deltaTime > remaining + 1.0e-5f)
        {
            update(remaining, simulation, quarry, timeline, pulley, physicalLiftEnabled);
            update(deltaTime - remaining, simulation, quarry, timeline, pulley, physicalLiftEnabled);
            return;
        }
    }
    stateTimer_ += deltaTime;
    if (stateTimer_ >= duration - 1.0e-5f)
        stateTimer_ = std::max(stateTimer_, duration);
    stateProgress_ = std::min(1.0f, stateTimer_ / duration);

    bool shouldAdvance = false;
    switch (state_) {
        case LogisticsState::QuarryReady:
        case LogisticsState::Extracting:
        case LogisticsState::UpperTransfer:
        case LogisticsState::Placement:
            shouldAdvance = false;
            break;
        default:
            shouldAdvance = (stateProgress_ >= 1.0f);
            break;
    }

    if (shouldAdvance && state_ != LogisticsState::QuarryReady) {
        advanceState(simulation, quarry, pulley,
                     physicalLiftEnabled);
        if (state_ == LogisticsState::QuarryReady) {
            return;
        }
    }

    if (settledFlashTimer_ > 0.0f)
        settledFlashTimer_ = std::max(0.0f, settledFlashTimer_ - deltaTime);

    switch (state_)
    {
    case LogisticsState::QuarryReady:
    {
        advanceState(simulation, quarry, pulley,
                     physicalLiftEnabled);
        break;
    }
    
    case LogisticsState::Extracting:
    {
        if (const ConstructionBlock* active = findActiveBlock(simulation, activeBlockId_))
            blockPosition_ = active->position;
        const LogisticsSnapshot staged = samplePhysicalRoute(LogisticsState::SledgeLoading, 0.0f, supportProgress_);
        sledgePosition_ = staged.sledgePosition;
        sledgeHeading_ = staged.sledgeHeading;
        sledgePitch_ = staged.sledgePitch;
        break;
    }
    case LogisticsState::Staged:
    {
        const glm::vec3 pickup =
            QuarryPulleyAnimationController::startLoadPosition();
        const SupportSurface floor = SceneSupport::transportAt(
            {-121.0f, -3.0f}, supportProgress_);
        const glm::vec3 staging{-121.0f,
                                floor.height + 0.5f * physicalBlockScale.y,
                                -3.0f};
        blockPosition_ = glm::mix(staging, pickup, stateProgress_);
        const LogisticsSnapshot loading = samplePhysicalRoute(
            LogisticsState::SledgeLoading, 0.0f, supportProgress_);
        sledgePosition_ = loading.sledgePosition;
        sledgeHeading_ = loading.sledgeHeading;
        sledgePitch_ = loading.sledgePitch;
        break;
    }

    case LogisticsState::SledgeLoading:
    case LogisticsState::Hauling:
    case LogisticsState::RampApproach:
    case LogisticsState::RampAscent:
    case LogisticsState::UpperTransfer:
    {
        const LogisticsSnapshot supported = samplePhysicalRoute(state_, stateProgress_, supportProgress_);
        sledgePosition_ = supported.sledgePosition;
        sledgeHeading_ = supported.sledgeHeading;
        sledgePitch_ = supported.sledgePitch;
        const glm::vec3 mounted{physicalSledgeRoot(supported) * glm::vec4{cargoSocketOffset, 1.0f}};
        if (state_ == LogisticsState::SledgeLoading)
        {
            blockPosition_ = glm::mix(
                QuarryPulleyAnimationController::destinationLoadPosition(),
                mounted, stateProgress_);
            blockRotation_ = glm::vec3{0.0f};
        }
        else
        {
            blockPosition_ = mounted;
            blockRotation_ = {sledgePitch_, sledgeHeading_, 0.0f};
        }
        blockScale_ = physicalBlockScale;
        break;
    }
    case LogisticsState::LiftPrep:
    {
        sledgePosition_ = quarryLoadingSledgePosition();
        sledgeHeading_ = 180.0f;
        sledgePitch_ = 0.0f;
        if (physicalLiftEnabled)
        {
            if (!pulley.setPhysicalPreparationProgress(activeBlockId_,
                                                       stateProgress_))
                throw std::runtime_error(
                    "Physical pulley preparation rejected the active block");
            blockPosition_ = QuarryPulleyAnimationController::startLoadPosition();
        }
        else
            blockPosition_ = QuarryPulleyAnimationController::startLoadPosition();
        blockRotation_ = glm::vec3{0.0f};
        blockScale_ = physicalBlockScale;
        break;
    }
    case LogisticsState::Lifting:
    {
        sledgePosition_ = quarryLoadingSledgePosition();
        sledgeHeading_ = 180.0f;
        sledgePitch_ = 0.0f;
        if (physicalLiftEnabled)
        {
            if (!pulley.setPhysicalLiftProgress(activeBlockId_, stateProgress_))
                throw std::runtime_error(
                    "Physical pulley lift rejected the active block");
            const QuarryPulleySnapshot lift = pulley.snapshot();
            blockPosition_ = lift.loadPosition;
            liftHeight_ = blockPosition_.y -
                          QuarryPulleyAnimationController::startLoadPosition().y;
        }
        else
        {
            const float eased = glm::smoothstep(0.0f, 1.0f, stateProgress_);
            blockPosition_ = glm::mix(
                QuarryPulleyAnimationController::startLoadPosition(),
                QuarryPulleyAnimationController::raisedLoadPosition(), eased);
            liftHeight_ = blockPosition_.y -
                          QuarryPulleyAnimationController::startLoadPosition().y;
        }
        blockRotation_ = glm::vec3{0.0f};
        blockScale_ = physicalBlockScale;
        break;
    }

    case LogisticsState::QuarryPlatformTransfer:
        sledgePosition_ = quarryLoadingSledgePosition();
        sledgeHeading_ = 180.0f;
        if (physicalLiftEnabled)
        {
            if (!pulley.setPhysicalTransferProgress(activeBlockId_,
                                                    stateProgress_))
                throw std::runtime_error(
                    "Physical pulley transfer rejected the active block");
            blockPosition_ = pulley.snapshot().loadPosition;
        }
        else
            blockPosition_ = glm::mix(
                QuarryPulleyAnimationController::raisedLoadPosition(),
                QuarryPulleyAnimationController::destinationLoadPosition(),
                stateProgress_);
        blockRotation_ = glm::vec3{0.0f};
        blockScale_ = physicalBlockScale;
        break;

    case LogisticsState::Placement:
    {
        sledgePosition_ = quarryLoadingSledgePosition();
        const ConstructionBlock* active =
            findActiveBlock(simulation, activeBlockId_);
        if (active == nullptr)
            throw std::runtime_error("Placement lost its authoritative block");
        const PostLiftRoute route = makePostLiftRoute(active->targetPlacement);
        const PostLiftRouteSample sample =
            samplePostLiftRoute(route, stateProgress_);
        blockPosition_ = sample.position;
        blockRotation_ = glm::vec3{0.0f};
        blockScale_ = sample.segment + 2 == route.points.size()
                          ? glm::mix(physicalBlockScale,
                                     active->targetPlacement.scale,
                                     glm::smoothstep(0.0f, 1.0f,
                                                     sample.segmentProgress))
                          : physicalBlockScale;
        break;
    }

    case LogisticsState::Settled:
        sledgePosition_ = quarryLoadingSledgePosition();
        blockPosition_ = computePlacementPosition(1.0f, simulation);
        blockRotation_ = glm::vec3{0.0f};
        if (const ConstructionBlock* active =
                findActiveBlock(simulation, activeBlockId_))
            blockScale_ = active->targetPlacement.scale;
        break;
    }

    if ((state_ == LogisticsState::QuarryPlatformTransfer ||
         state_ == LogisticsState::Placement) &&
        hasPreviousPostLiftPosition_)
    {
        largestPostLiftDisplacement_ = std::max(
            largestPostLiftDisplacement_,
            glm::distance(previousPostLiftPosition_, blockPosition_));
        previousPostLiftPosition_ = blockPosition_;
    }

    if (activeBlockId_ != 0) {
        ConstructionBlock* block = simulation.getBlock(activeBlockId_);
        if (block) {
            block->previousPosition = block->position;
            block->position = blockPosition_;
            block->rotation = blockRotation_;
            block->scale = blockScale_;
            block->sledgeId = ((state_ == LogisticsState::SledgeLoading &&
                                stateProgress_ >= 1.0f) ||
                               state_ == LogisticsState::Hauling ||
                               state_ == LogisticsState::RampApproach ||
                               state_ == LogisticsState::RampAscent ||
                               state_ == LogisticsState::UpperTransfer) ? 1 : -1;
            block->taskProgress = stateProgress_;
            
            switch (state_) {
                case LogisticsState::SledgeLoading: block->state = BlockState::LoadedOnSledge; break;
                case LogisticsState::Hauling: block->state = BlockState::Hauling; break;
                case LogisticsState::RampApproach: block->state = BlockState::RampApproach; break;
                case LogisticsState::RampAscent: block->state = BlockState::RampAscent; break;
                case LogisticsState::UpperTransfer: block->state = BlockState::UpperTransfer; break;
                case LogisticsState::LiftPrep: block->state = BlockState::LiftPrep; break;
                case LogisticsState::Lifting: block->state = BlockState::Lifting; break;
                case LogisticsState::QuarryPlatformTransfer: block->state = BlockState::QuarryPlatformTransfer; break;
                case LogisticsState::Placement: block->state = BlockState::Placement; break;
                case LogisticsState::Settled: block->state = BlockState::Settled; break;
                default: break;
            }
        }
    }


    settlePlacementIfSpatiallyReady(simulation, timeline);
}

namespace
{
bool validatePhysicalPulleyIntegration(std::ostream& output)
{
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;

    simulation.initialize(
        PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    timeline.setProgress(0.0f);
    quarry.startExtraction(simulation, 0);
    if (simulation.activeBlocks.empty())
    {
        output << "Phase 13 physical pulley integration validation\n"
               << "  authoritative block creation: FAIL\n";
        return false;
    }

    const std::uint64_t blockId = simulation.activeBlocks.back().id;
    bool pickupReached = false;
    bool attachmentAligned = true;
    bool ropeAligned = true;
    bool sameIdentity = true;
    bool singlePayload = true;
    std::size_t maximumSimulationCubes = 0;
    std::size_t maximumMatchingPayloadCubes = 0;
    bool logisticsDummyDetected = false;
    bool liftSampled = false;
    bool topReached = false;
    float maximumAttachmentError = 0.0f;
    glm::vec3 pickupBlock{0.0f};
    glm::vec3 liftTopBlock{0.0f};

    constexpr float deltaTime = 0.05f;
    constexpr int maximumSteps = 10000;
    for (int step = 0; step < maximumSteps && !topReached; ++step)
    {
        quarry.update(deltaTime, simulation);
        logistics.update(deltaTime, simulation, quarry, timeline, pulley, true);

        ConstructionBlock* block = simulation.getBlock(blockId);
        if (block == nullptr)
        {
            sameIdentity = false;
            break;
        }

        const QuarryPulleySnapshot snapshot = pulley.snapshot();
        if (logistics.state() == LogisticsState::LiftPrep &&
            logistics.stateProgress() >= 0.90f)
        {
            pickupBlock = block->position;
            pickupReached =
                glm::distance(block->position,
                              QuarryPulleyAnimationController::startLoadPosition()) <=
                1.0e-3f;
            sameIdentity = sameIdentity &&
                           pulley.activePhysicalBlockId() == blockId;
        }
        else if (logistics.state() == LogisticsState::Lifting)
        {
            liftSampled = true;
            const glm::vec3 blockAttachment =
                block->position +
                QuarryPulleyAnimationController::loadAttachmentOffset();
            const float attachmentError =
                glm::distance(blockAttachment, snapshot.loadAttachmentPoint);
            maximumAttachmentError =
                std::max(maximumAttachmentError, attachmentError);
            attachmentAligned = attachmentAligned &&
                                glm::distance(block->position,
                                              snapshot.loadPosition) <= 1.0e-3f &&
                                attachmentError <= 1.0e-3f;
            ropeAligned = ropeAligned && snapshot.ropeAttached &&
                          glm::distance(snapshot.ropeEnd,
                                        blockAttachment) <= 1.0e-3f;
            sameIdentity = sameIdentity &&
                           pulley.activePhysicalBlockId() == blockId;

            std::vector<SceneObject> simulationObjects;
            std::vector<SceneObject> logisticsObjects;
            simulation.collectSceneObjects(simulationObjects);
            logistics.collectSceneObjects(logisticsObjects);
            const std::size_t simulationCubes =
                static_cast<std::size_t>(std::count_if(
                    simulationObjects.begin(), simulationObjects.end(),
                    [](const SceneObject& object) {
                        return object.primitive == ScenePrimitive::Cube;
                    }));
            const std::size_t matchingPayloadCubes =
                static_cast<std::size_t>(std::count_if(
                    simulationObjects.begin(), simulationObjects.end(),
                    [block](const SceneObject& object) {
                        return object.primitive == ScenePrimitive::Cube &&
                               glm::distance(glm::vec3{object.model[3]},
                                             block->position) <= 1.0e-3f;
                    }));
            const bool logisticsHasDummy = std::any_of(
                logisticsObjects.begin(), logisticsObjects.end(),
                [](const SceneObject& object) {
                    return object.primitive == ScenePrimitive::Cube;
                });
            maximumSimulationCubes =
                std::max(maximumSimulationCubes, simulationCubes);
            maximumMatchingPayloadCubes = std::max(
                maximumMatchingPayloadCubes, matchingPayloadCubes);
            logisticsDummyDetected =
                logisticsDummyDetected || logisticsHasDummy;
            singlePayload = singlePayload && matchingPayloadCubes == 1 &&
                            !logisticsHasDummy;
        }
        else if (logistics.state() == LogisticsState::QuarryPlatformTransfer)
        {
            liftTopBlock = block->position;
            topReached =
                glm::distance(block->position,
                              QuarryPulleyAnimationController::raisedLoadPosition()) <=
                    1.0e-3f &&
                glm::distance(block->position, snapshot.loadPosition) <=
                    1.0e-3f &&
                pulley.activePhysicalBlockId() == blockId;
        }
    }

    const glm::vec3 sledge = logistics.snapshot().sledgePosition;
    const glm::vec3 pickup =
        QuarryPulleyAnimationController::startLoadPosition();
    const bool sledgeAtDeck =
        SceneSupport::transportAt({sledge.x, sledge.z}).kind ==
            SupportSurfaceKind::WorkPlatform &&
        glm::distance(glm::vec2{sledge.x, sledge.z},
                      glm::vec2{-108.95f, -10.0f}) <= 0.01f;
    const bool valid = pickupReached && liftSampled && attachmentAligned &&
                       ropeAligned && sameIdentity && singlePayload &&
                       topReached && sledgeAtDeck;

    output << std::fixed << std::setprecision(3)
           << "Phase 13 physical pulley integration validation\n"
           << "  authoritative block ID: " << blockId << '\n'
           << "  pulley pickup: (" << pickup.x << ", " << pickup.y << ", "
           << pickup.z << ")\n"
           << "  block pickup: (" << pickupBlock.x << ", " << pickupBlock.y
           << ", " << pickupBlock.z << ") "
           << (pickupReached ? "PASS" : "FAIL") << '\n'
           << "  maximum attachment/rope error: "
           << maximumAttachmentError << ' '
           << (attachmentAligned && ropeAligned ? "PASS" : "FAIL") << '\n'
           << "  same block identity throughout lift: "
           << (sameIdentity ? "PASS" : "FAIL") << '\n'
           << "  one matching simulation payload and no logistics dummy: "
           << (singlePayload ? "PASS" : "FAIL")
           << " (matching " << maximumMatchingPayloadCubes
           << ", all active simulation cubes " << maximumSimulationCubes
           << ", logistics dummy "
           << (logisticsDummyDetected ? "YES" : "NO") << ")\n"
           << "  sledge waits on quarry receiving deck: "
           << (sledgeAtDeck ? "PASS" : "FAIL") << '\n'
           << "  lift top: (" << liftTopBlock.x << ", " << liftTopBlock.y
           << ", " << liftTopBlock.z << ") "
           << (topReached ? "PASS" : "FAIL") << '\n'
           << (valid ? "Physical pulley integration checks passed.\n"
                     : "Physical pulley integration checks failed.\n");
    return valid;
}

bool validatePhysicalHaulingCrewIntegration(std::ostream& output)
{
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;

    simulation.initialize(
        PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    timeline.setProgress(0.0f);
    quarry.startExtraction(simulation, 0);

    std::array<bool, 4> sampledStates{{false, false, false, false}};
    bool rootsFinite = true;
    bool formationSynchronized = true;
    bool spacingValid = true;
    bool workersInFront = true;
    bool workersFacing = true;
    bool handsFinite = true;
    bool towPointsFinite = true;
    bool ropeEndpointsOwned = true;
    bool ropesAboveSupport = true;
    bool flatGrounded = true;
    bool rampGrounded = true;
    float minimumSpacing = std::numeric_limits<float>::max();
    float minimumRampLead = std::numeric_limits<float>::max();
    float minimumFacingDot = 1.0f;
    float maximumGroundingError = 0.0f;

    constexpr float deltaTime = 0.05f;
    constexpr int maximumSteps = 200;
    for (int step = 0; step < maximumSteps; ++step)
    {
        const LogisticsState sampledState = static_cast<LogisticsState>(
            static_cast<int>(LogisticsState::SledgeLoading) + step % 4);
        LogisticsSnapshot snapshot = ConstructionLogistics::samplePhysicalRoute(
            sampledState, static_cast<float>(step / 4) / 49.0f,
            sampledState == LogisticsState::RampAscent ? 0.75f : 0.0f);
        snapshot.ropeTaut = sampledState != LogisticsState::SledgeLoading;

        const std::size_t stateIndex =
            static_cast<std::size_t>(snapshot.state) -
            static_cast<std::size_t>(LogisticsState::SledgeLoading);
        if (stateIndex < sampledStates.size())
            sampledStates[stateIndex] = true;

        const glm::mat4 sledgeRoot =
            ConstructionLogistics::physicalSledgeRoot(snapshot);
        glm::vec3 direction = ConstructionLogistics::haulingDirection(snapshot);
        direction.y = 0.0f;
        direction = glm::normalize(direction);
        std::array<glm::vec3, 2> workerPositions{};

        for (std::size_t crewIndex = 0;
             crewIndex < ConstructionLogistics::pullingCrewSize();
             ++crewIndex)
        {
            const bool moving = snapshot.state != LogisticsState::SledgeLoading;
            const WorkerJointAngles angles = moving
                ? ConstructionAnimationController::walkingPose(
                      Worker::poseAngles(WorkerPose::PullingReady),
                      static_cast<float>(step) * deltaTime +
                          (crewIndex == 0u ? 0.0f : 0.63f),
                      1.0f, true)
                : Worker::poseAngles(WorkerPose::PullingReady);
            const glm::mat4 root = ConstructionLogistics::physicalWorkerRoot(
                snapshot, crewIndex, angles);
            rootsFinite = rootsFinite && isFiniteNonSingularTransform(root);
            workerPositions[crewIndex] = glm::vec3{root[3]};

            glm::vec3 expectedWorld{
                sledgeRoot * glm::vec4{formationOffset(crewIndex), 1.0f}};
            glm::vec3 expectedForward = direction;
            if (snapshot.state == LogisticsState::RampAscent)
            {
                const auto route = ConstructionLogistics::physicalRouteWaypoints(snapshot.state);
                const float aheadDistance = snapshot.stateProgress *
                    horizontalRouteLength(route) + 4.70f;
                const HorizontalRouteSample ahead = horizontalRouteAt(route, aheadDistance);
                const glm::vec2 side{-ahead.direction.y, ahead.direction.x};
                expectedWorld.x = ahead.point.x + formationOffset(crewIndex).x * side.x;
                expectedWorld.z = ahead.point.y + formationOffset(crewIndex).x * side.y;
                expectedForward = {ahead.direction.x, 0.0f, ahead.direction.y};
            }
            formationSynchronized = formationSynchronized &&
                std::abs(workerPositions[crewIndex].x - expectedWorld.x) <=
                    1.0e-3f &&
                std::abs(workerPositions[crewIndex].z - expectedWorld.z) <=
                    1.0e-3f;

            glm::vec3 workerForward{
                root * glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}};
            workerForward.y = 0.0f;
            workerForward = glm::normalize(workerForward);
            minimumFacingDot = std::min(minimumFacingDot,
                                        glm::dot(workerForward, expectedForward));
            workersFacing = workersFacing &&
                            glm::dot(workerForward, expectedForward) >= 0.999f;
            glm::vec3 lead = workerPositions[crewIndex] - snapshot.sledgePosition;
            lead.y = 0.0f;
            if (snapshot.state == LogisticsState::RampAscent)
                minimumRampLead = std::min(minimumRampLead, glm::length(lead));
            workersInFront = workersInFront &&
                (snapshot.state == LogisticsState::RampAscent
                     ? glm::length(lead) > 2.75f
                     : glm::dot(lead, direction) > 3.5f);

            const Worker::EvaluatedPose pose = Worker::evaluate(root, angles);
            const float leftFootBottom = SceneSupport::transformedBottomY(
                pose[static_cast<std::size_t>(BodyPart::LeftFoot)].model);
            const float rightFootBottom = SceneSupport::transformedBottomY(
                pose[static_cast<std::size_t>(BodyPart::RightFoot)].model);
            const float footBottom = std::min(leftFootBottom, rightFootBottom);
            const float support = ConstructionLogistics::haulingSupportHeight(
                snapshot, {workerPositions[crewIndex].x,
                           workerPositions[crewIndex].z});
            const float groundingError =
                std::abs(footBottom - support - workerGroundClearance);
            maximumGroundingError =
                std::max(maximumGroundingError, groundingError);
            if (snapshot.state == LogisticsState::RampAscent)
                rampGrounded = rampGrounded && groundingError <= 1.0e-3f;
            else
                flatGrounded = flatGrounded && groundingError <= 1.0e-3f;

            const glm::vec3 hand =
                ConstructionLogistics::pullingHandPosition(pose, crewIndex);
            const glm::vec3 tow =
                ConstructionLogistics::physicalTowPoint(snapshot, crewIndex);
            handsFinite = handsFinite && finiteVector(hand);
            towPointsFinite = towPointsFinite && finiteVector(tow);
            ropeEndpointsOwned = ropeEndpointsOwned &&
                glm::distance(hand,
                              ConstructionLogistics::pullingHandPosition(
                                  pose, crewIndex)) <= 1.0e-6f &&
                glm::distance(tow,
                              ConstructionLogistics::physicalTowPoint(
                                  snapshot, crewIndex)) <= 1.0e-6f;
            if (snapshot.ropeTaut)
            {
                const glm::mat4 rope =
                    ConstructionAnimationController::cylinderBetween(
                        hand, tow, 0.065f);
                rootsFinite = rootsFinite && isFiniteNonSingularTransform(rope);
                for (int sample = 0; sample <= 8; ++sample)
                {
                    const glm::vec3 point = glm::mix(
                        hand, tow, static_cast<float>(sample) / 8.0f);
                    const float ropeSupport =
                        ConstructionLogistics::haulingSupportHeight(
                            snapshot, {point.x, point.z});
                    ropesAboveSupport = ropesAboveSupport &&
                                        point.y - ropeSupport >= 0.08f;
                }
            }
        }

        const float spacing =
            glm::distance(workerPositions[0], workerPositions[1]);
        minimumSpacing = std::min(minimumSpacing, spacing);
        spacingValid = spacingValid && spacing >= 1.25f;
    }

    const bool allStatesSampled =
        std::all_of(sampledStates.begin(), sampledStates.end(),
                    [](bool sampled) { return sampled; });
    const bool valid = allStatesSampled && rootsFinite &&
                       formationSynchronized && spacingValid &&
                       workersInFront && workersFacing && flatGrounded &&
                       rampGrounded && handsFinite && towPointsFinite &&
                       ropeEndpointsOwned && ropesAboveSupport;

    output << std::fixed << std::setprecision(3)
           << "Phase 13 physical hauling crew validation\n"
           << "  all physical hauling states sampled: "
           << (allStatesSampled ? "PASS" : "FAIL") << '\n'
           << "  roots derive from logistics sledge: "
           << (formationSynchronized ? "PASS" : "FAIL") << '\n'
           << "  minimum worker spacing: " << minimumSpacing << ' '
           << (spacingValid ? "PASS" : "FAIL") << '\n'
           << "  workers ahead and facing haul direction: "
           << (workersInFront && workersFacing ? "PASS" : "FAIL")
           << " (minimum ascent lead " << minimumRampLead
           << ", facing dot " << minimumFacingDot << ")\n"
           << "  maximum foot/support error: " << maximumGroundingError << ' '
           << (flatGrounded && rampGrounded ? "PASS" : "FAIL") << '\n'
           << "  hand and tow endpoints finite/authoritative: "
           << (handsFinite && towPointsFinite && ropeEndpointsOwned
                   ? "PASS" : "FAIL") << '\n'
           << "  taut ropes remain above transport support: "
           << (ropesAboveSupport ? "PASS" : "FAIL") << '\n'
           << (valid ? "Physical hauling crew checks passed.\n"
                     : "Physical hauling crew checks failed.\n");
    return valid;
}

[[maybe_unused]] bool validatePostLiftRouteIntegration(std::ostream& output)
{
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;

    simulation.initialize(
        PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    const std::size_t baselineOccupied = simulation.prebuiltCount();
    timeline.setProgress(0.0f);
    quarry.startExtraction(simulation, 0);
    if (simulation.activeBlocks.empty())
    {
        output << "Phase 13 post-lift route validation\n"
               << "  authoritative block creation: FAIL\n";
        return false;
    }

    const std::uint64_t blockId = simulation.activeBlocks.back().id;
    const std::size_t targetIndex =
        simulation.activeBlocks.back().targetIndex;
    const PyramidBlockPlacement target =
        simulation.activeBlocks.back().targetPlacement;
    const PostLiftRoute route = makePostLiftRoute(target);
    const glm::vec3 liftTop =
        QuarryPulleyAnimationController::raisedLoadPosition();
    const glm::vec3 stagingDestination =
        QuarryPulleyAnimationController::destinationLoadPosition();

    constexpr float deltaTime = 0.05f;
    constexpr int maximumSteps = 20000;
    const float upperMaximumSpeed =
        1.5f * glm::distance(liftTop,
                             glm::vec3{stagingDestination.x, liftTop.y,
                                       stagingDestination.z}) /
        QuarryPulleyAnimationController::stateDuration(
            QuarryPulleyState::GuideToPlatform);
    const float placementSpeed =
        route.length / stateBaseDuration(LogisticsState::Placement);
    const float displacementBound =
        std::max(upperMaximumSpeed, placementSpeed) * deltaTime + 0.03f;

    LogisticsState previousState = logistics.state();
    glm::vec3 previousPosition{0.0f};
    bool havePreviousPostLift = false;
    bool sameIdentity = true;
    bool finiteAndBounded = true;
    bool liftToStagingContinuous = false;
    bool stagingToPlacementContinuous = false;
    bool upperSampled = false;
    bool placementSampled = false;
    bool occupiedBefore = !simulation.isTargetOccupied(targetIndex);
    bool occupiedAfter = false;
    int settlementTransitions = 0;
    float maximumDisplacement = 0.0f;
    glm::vec3 upperStart{0.0f};
    glm::vec3 upperEnd{0.0f};
    glm::vec3 placementStart{0.0f};

    for (int step = 0; step < maximumSteps; ++step)
    {
        quarry.update(deltaTime, simulation);
        logistics.update(deltaTime, simulation, quarry, timeline, pulley, true);
        ConstructionBlock* block = simulation.getBlock(blockId);
        if (block == nullptr)
        {
            sameIdentity = false;
            break;
        }
        sameIdentity = sameIdentity && block->id == blockId &&
                       (logistics.activeBlockId() == blockId ||
                        logistics.state() == LogisticsState::QuarryReady);

        const LogisticsState state = logistics.state();
        if (state == LogisticsState::QuarryPlatformTransfer ||
            state == LogisticsState::Placement)
        {
            finiteAndBounded = finiteAndBounded && finiteVector(block->position) &&
                std::abs(block->position.x) <=
                    0.5f * MonumentalSite::scale().worldWidth + 10.0f &&
                std::abs(block->position.z) <=
                    0.5f * MonumentalSite::scale().worldDepth + 10.0f;
            if (havePreviousPostLift)
            {
                const float displacement =
                    glm::distance(previousPosition, block->position);
                maximumDisplacement =
                    std::max(maximumDisplacement, displacement);
                finiteAndBounded = finiteAndBounded &&
                                   displacement <= displacementBound;
            }
            previousPosition = block->position;
            havePreviousPostLift = true;
        }

        if (state == LogisticsState::QuarryPlatformTransfer)
        {
            if (!upperSampled)
            {
                upperStart = block->position;
                liftToStagingContinuous =
                    glm::distance(upperStart, liftTop) <= 1.0e-3f;
            }
            upperEnd = block->position;
            upperSampled = true;
        }
        else if (state == LogisticsState::Placement)
        {
            if (!placementSampled)
            {
                placementStart = block->position;
                stagingToPlacementContinuous =
                    glm::distance(placementStart, stagingDestination) <=
                    1.0e-3f &&
                    glm::distance(route.points.front(), stagingDestination) <=
                    1.0e-6f;
            }
            placementSampled = true;
        }

        if (state == LogisticsState::Settled &&
            previousState != LogisticsState::Settled)
        {
            ++settlementTransitions;
            occupiedAfter = simulation.isTargetOccupied(targetIndex);
            break;
        }
        previousState = state;
    }

    const ConstructionBlock* block = findActiveBlock(simulation, blockId);
    const float finalPositionError = block == nullptr
        ? std::numeric_limits<float>::infinity()
        : glm::distance(block->position, target.position);
    const float finalOrientationError = block == nullptr
        ? std::numeric_limits<float>::infinity()
        : glm::length(block->rotation);
    const float finalScaleError = block == nullptr
        ? std::numeric_limits<float>::infinity()
        : glm::length(block->scale - target.scale);
    bool waypointsFinite = route.length > 0.0f;
    for (const glm::vec3& point : route.points)
        waypointsFinite = waypointsFinite && finiteVector(point) &&
            std::abs(point.x) <= 0.5f * MonumentalSite::scale().worldWidth + 10.0f &&
            std::abs(point.z) <= 0.5f * MonumentalSite::scale().worldDepth + 10.0f;

    const bool spatiallySettled = block != nullptr &&
        block->state == BlockState::Settled &&
        finalPositionError <= placementPositionTolerance &&
        finalOrientationError <= placementOrientationToleranceDegrees &&
        finalScaleError <= placementScaleTolerance;
    const bool occupancyOnce = occupiedBefore && occupiedAfter &&
        simulation.settledCount() == 1 &&
        simulation.occupiedTargetCount() == baselineOccupied + 1 &&
        settlementTransitions == 1;
    const bool valid = upperSampled && placementSampled && sameIdentity &&
        liftToStagingContinuous && stagingToPlacementContinuous &&
        finiteAndBounded && waypointsFinite && spatiallySettled &&
        occupancyOnce && maximumDisplacement <= displacementBound;

    output << std::fixed << std::setprecision(4)
           << "Phase 13 post-lift route validation\n"
           << "  block/target: " << blockId << " / " << targetIndex << '\n'
           << "  lift top: (" << liftTop.x << ", " << liftTop.y << ", "
           << liftTop.z << ")\n"
           << "  upper staging start: (" << upperStart.x << ", "
           << upperStart.y << ", " << upperStart.z << ") "
           << (liftToStagingContinuous ? "PASS" : "FAIL") << '\n'
           << "  upper staging end sample: (" << upperEnd.x << ", "
           << upperEnd.y << ", " << upperEnd.z << ")\n"
           << "  placement start: (" << placementStart.x << ", "
           << placementStart.y << ", " << placementStart.z << ") "
           << (stagingToPlacementContinuous ? "PASS" : "FAIL") << '\n'
           << "  target: (" << target.position.x << ", " << target.position.y
           << ", " << target.position.z << ")\n"
           << "  route length / waypoints: " << route.length << " / "
           << route.points.size() << ' '
           << (waypointsFinite ? "PASS" : "FAIL") << '\n'
           << "  largest frame displacement / bound: "
           << maximumDisplacement << " / " << displacementBound << ' '
           << (maximumDisplacement <= displacementBound ? "PASS" : "FAIL")
           << '\n'
           << "  final position/orientation/scale error: "
           << finalPositionError << " / " << finalOrientationError << " / "
           << finalScaleError << ' '
           << (spatiallySettled ? "PASS" : "FAIL") << '\n'
           << "  same block identity and single settlement: "
           << (sameIdentity && occupancyOnce ? "PASS" : "FAIL") << '\n'
           << "  occupancy false -> true exactly once: "
           << (occupancyOnce ? "PASS" : "FAIL") << '\n'
           << (valid ? "Post-lift route checks passed.\n"
                     : "Post-lift route checks failed.\n");
    return valid;
}
bool validateQuarryHandoffIntegration(std::ostream& output)
{
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    simulation.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    timeline.setProgress(0.0f);
    quarry.startExtraction(simulation, 0);
    if (simulation.activeBlocks.empty())
        return false;
    const std::uint64_t blockId = simulation.activeBlocks.back().id;
    const std::size_t occupiedBefore = simulation.occupiedTargetCount();
    const std::array<LogisticsState, 6> expected{{
        LogisticsState::Staged, LogisticsState::LiftPrep,
        LogisticsState::Lifting, LogisticsState::QuarryPlatformTransfer,
        LogisticsState::SledgeLoading, LogisticsState::Hauling}};
    std::size_t nextState = 0;
    LogisticsState previousState = LogisticsState::QuarryReady;
    glm::vec3 previousPosition{0.0f};
    bool havePrevious = false;
    bool sameIdentity = true, continuous = true, noDuplicate = true;
    bool pickup = false, lift = false, platform = false;
    bool supportedTransfer = true, loaded = false, noPyramidTravel = true;
    float maximumDisplacement = 0.0f;
    constexpr float dt = 0.02f;
    constexpr float maximumSpeed = 7.125f; // Pulley guide: 1.5 * 9.5 m / 2 s.
    constexpr float displacementBound = maximumSpeed * dt + 0.03f;
    for (int step = 0; step < 20000 && !loaded; ++step)
    {
        quarry.update(dt, simulation);
        logistics.update(dt, simulation, quarry, timeline, pulley, true);
        const LogisticsSnapshot pose = logistics.snapshot();
        const ConstructionBlock* block = simulation.getBlock(blockId);
        if (block == nullptr ||
            (logistics.activeBlockId() != blockId &&
             pose.state != LogisticsState::QuarryReady))
        {
            sameIdentity = false;
            break;
        }
        if (pose.state == LogisticsState::QuarryReady)
            continue;
        if (pose.state != previousState)
        {
            if (nextState >= expected.size() || pose.state != expected[nextState])
                sameIdentity = false;
            else
                ++nextState;
            previousState = pose.state;
        }
        noPyramidTravel = noPyramidTravel && block->position.x < -105.0f;
        if (havePrevious)
        {
            const float displacement =
                glm::distance(previousPosition, block->position);
            maximumDisplacement = std::max(maximumDisplacement, displacement);
            continuous = continuous && displacement <= displacementBound;
        }
        previousPosition = block->position;
        havePrevious = true;

        std::vector<SceneObject> payloadObjects, logisticsObjects;
        simulation.collectSceneObjects(payloadObjects);
        logistics.collectSceneObjects(logisticsObjects);
        const std::size_t matching = static_cast<std::size_t>(std::count_if(
            payloadObjects.begin(), payloadObjects.end(), [block](const SceneObject& object) {
                return object.primitive == ScenePrimitive::Cube &&
                    glm::distance(glm::vec3{object.model[3]}, block->position) <= 1.0e-3f;
            }));
        noDuplicate = noDuplicate && matching == 1 &&
            std::none_of(logisticsObjects.begin(), logisticsObjects.end(),
                         [](const SceneObject& object) {
                             return object.primitive == ScenePrimitive::Cube;
                         });

        if (pose.state == LogisticsState::LiftPrep &&
            pose.stateProgress >= 0.90f)
            pickup = pickup || (glm::distance(block->position,
                QuarryPulleyAnimationController::startLoadPosition()) < 0.001f &&
                pulley.activePhysicalBlockId() == blockId);
        if (pose.state == LogisticsState::Lifting && pose.stateProgress > 0.05f)
            lift = lift || (pulley.activePhysicalBlockId() == blockId &&
                glm::distance(block->position, pulley.snapshot().loadPosition) < 0.001f);
        if (pose.state == LogisticsState::QuarryPlatformTransfer &&
            pose.stateProgress >= 0.99f)
        {
            const SupportSurface deck = SceneSupport::transportAt(
                {block->position.x, block->position.z});
            platform = platform ||
                (glm::distance(block->position,
                    QuarryPulleyAnimationController::destinationLoadPosition()) < 0.01f &&
                 deck.kind == SupportSurfaceKind::WorkPlatform &&
                 std::abs(block->position.y - 0.5f * block->scale.y - deck.height) < 0.01f);
        }
        if (pose.state == LogisticsState::SledgeLoading)
        {
            const float blockBottom = block->position.y - 0.5f * block->scale.y;
            const float skidTop = -2.60f + 0.525f * pose.stateProgress;
            supportedTransfer = supportedTransfer &&
                std::abs(blockBottom - skidTop) < 0.02f &&
                block->position.x >= -111.01f && block->position.x <= -108.94f &&
                std::abs(block->position.z + 10.0f) < 0.03f;
        }
        if (pose.state == LogisticsState::Hauling)
        {
            const glm::vec3 mounted{ConstructionLogistics::physicalSledgeRoot(pose) *
                glm::vec4{cargoSocketOffset, 1.0f}};
            loaded = glm::distance(block->position, mounted) < 0.001f &&
                block->sledgeId == 1 && pose.ropeTaut;
        }
    }
    const bool occupancyUnchanged = simulation.occupiedTargetCount() == occupiedBefore &&
        simulation.settledCount() == 0;
    const bool valid = nextState == expected.size() && sameIdentity &&
        pickup && lift && platform && supportedTransfer && loaded &&
        continuous && noDuplicate && noPyramidTravel && occupancyUnchanged;
    output << std::fixed << std::setprecision(4)
           << "Phase 13 quarry pulley-to-sledge lifecycle validation\n"
           << "  authoritative block ID: " << blockId << '\n'
           << "  staged -> lift prep -> lift -> deck -> load: "
           << (nextState == expected.size() ? "PASS" : "FAIL") << '\n'
           << "  pickup / lift / supported deck: "
           << (pickup && lift && platform ? "PASS" : "FAIL") << '\n'
           << "  supported skid transfer / rigid cargo socket: "
           << (supportedTransfer && loaded ? "PASS" : "FAIL") << '\n'
           << "  maximum frame displacement / speed bound: "
           << maximumDisplacement << " / " << displacementBound << ' '
           << (continuous ? "PASS" : "FAIL") << '\n'
           << "  same identity / one payload / no pyramid travel / occupancy unchanged: "
           << (sameIdentity && noDuplicate && noPyramidTravel &&
               occupancyUnchanged ? "PASS" : "FAIL") << '\n'
           << (valid ? "Quarry handoff checks passed.\n" :
                       "Quarry handoff checks failed.\n");
    return valid;
}
} // namespace


bool ConstructionLogistics::validatePhysicalRoute(std::ostream& output)
{
    struct Obstacle { const char* name; HorizontalFootprint footprint; float topY; };
    std::vector<Obstacle> obstacles;
    const auto add = [&](const char* name, glm::vec2 center, glm::vec2 size,
                         float yaw = 0.0f, float topY = 100.0f)
    { obstacles.push_back({name, {center, 0.5f * size, yaw}, topY}); };
    for (float sign : {-1.0f, 1.0f})
    {
        add("QuarryWall", {-128.0f + sign * 29.0f, -15.0f}, {6.0f, 72.0f}, 0.0f, 0.0f);
        add("QuarryWall", {-128.0f + sign * 24.0f, -15.0f}, {4.0f, 58.0f}, 0.0f, -2.50f);
        add("QuarryWall", {-128.0f + sign * 20.0f, -15.0f}, {4.0f, 44.0f}, 0.0f, -4.60f);
    }
    add("QuarryTerrace", {-128.0f, -48.0f}, {64.0f, 6.0f});
    add("QuarryTerrace", {-128.0f, -41.0f}, {54.0f, 8.0f});
    add("QuarryTerrace", {-128.0f, -33.0f}, {44.0f, 8.0f});
    add("QuarryWall", {-146.0f, 17.0f}, {24.0f, 5.0f}, 0.0f, 0.0f);
    add("QuarryWall", {-116.0f, 17.0f}, {16.0f, 5.0f}, 0.0f, 0.0f);
    for (int stone = 0; stone < 12; ++stone)
        add("QuarryStones", {-116.0f + (stone % 4) * 3.2f, -4.0f + (stone / 4) * 3.1f},
            {2.45f, 2.35f}, static_cast<float>((stone % 5) * 6), -5.925f);
    for (int group = 0; group < 3; ++group)
        for (int rock = 0; rock < 10; ++rock)
        {
            const float size = 0.65f + 0.12f * ((rock + group) % 4);
            add("QuarrySpoil", {-151.0f + group * 13.0f + (rock % 5) * 1.65f,
                2.0f + (rock / 5) * 1.8f}, {size * 1.3f, size}, rock * 17.0f, -6.95f + size);
        }
    for (const RepositoryDescriptor& depot : IndustrialLandscape::repositories())
        for (unsigned row = 0; row < depot.rows; ++row)
            for (unsigned column = 0; column < depot.columns; ++column)
            {
                const glm::vec2 center{
                    depot.center.x + (column - 0.5f * (depot.columns - 1)) * (depot.blockScale.x + depot.spacing),
                    depot.center.z + (row - 0.5f * (depot.rows - 1)) * (depot.blockScale.z + depot.spacing)};
                const float yaw = std::string(depot.id) == "RoughDepot" ?
                    static_cast<float>((row * 13 + column * 7) % 17) - 8.0f : 0.0f;
                add("Repository", center, {depot.blockScale.x, depot.blockScale.z}, yaw,
                    depot.center.y + depot.levels * depot.blockScale.y);
            }
    for (int bed = 0; bed < 4; ++bed)
        add("CuttingBed", {-65.0f + bed * 4.7f, 16.0f}, {3.6f, 3.1f});
    for (float x : {-15.5f, -4.5f})
        for (float z : {38.7f, 45.3f})
            add("LoadingPost", {x, z}, {0.2f, 0.2f});
    for (int waiting = 0; waiting < 4; ++waiting)
        add("WaitingStone", {-18.0f + waiting * 3.0f, 48.5f}, {2.55f, 2.35f});
    add("InspectionShelter", {-30.0f, 51.0f}, {8.0f, 5.8f});
    add("QuarryStagingStructure", {-109.0f, 8.5f}, {10.0f, 5.0f}, 0.0f, -6.80f);
    for (float x : {-123.5f, -108.0f})
        for (float z : {-13.2f, -6.8f})
            add("PulleyPost", {x, z}, {0.48f, 0.48f}, 0.0f, 2.0f);
    for (float z : {-14.3f, -5.7f})
        add("PulleyAnchor", {-124.7f, z}, {0.58f, 0.58f});
    const HeavyLiftingRigDescriptor& rig = IndustrialLandscape::liftingRig();
    for (float x : {rig.center.x - 0.5f * rig.width,
                    rig.center.x + 0.5f * rig.width})
        for (float z : {rig.center.z - 0.5f * rig.depth,
                        rig.center.z + 0.5f * rig.depth})
            add("HeavyRigPost", {x, z}, {0.28f, 0.28f}, 0.0f, rig.height);
    add("HeavyRigLoad", {rig.center.x, rig.center.z}, {3.2f, 2.8f}, 0.0f, 2.2f);
    // The receiving deck is the load-bearing starting support, not an obstacle.

    const auto axes = [](const HorizontalFootprint& box)
    {
        const float a = glm::radians(-box.yawDegrees);
        return std::array<glm::vec2, 2>{{{std::cos(a), std::sin(a)}, {-std::sin(a), std::cos(a)}}};
    };
    const auto overlaps = [&](const HorizontalFootprint& a, const HorizontalFootprint& b)
    {
        const auto aa = axes(a), bb = axes(b);
        for (glm::vec2 axis : {aa[0], aa[1], bb[0], bb[1]})
        {
            const float ra = a.halfExtents.x * std::abs(glm::dot(axis, aa[0])) + a.halfExtents.y * std::abs(glm::dot(axis, aa[1]));
            const float rb = b.halfExtents.x * std::abs(glm::dot(axis, bb[0])) + b.halfExtents.y * std::abs(glm::dot(axis, bb[1]));
            if (std::abs(glm::dot(b.center - a.center, axis)) > ra + rb + 0.20f)
                return false;
        }
        return true;
    };
    SandSimulation sand;
    SceneSupport::setTerrainSource(&sand);
    bool valid = true;
    int intersections = 0;
    std::map<std::string, std::pair<int, glm::vec2>> collisionGroups;
    float maximumSupportError = 0.0f, maximumSandError = 0.0f, maximumStep = 0.0f;
    float total = 0.0f, exposed = 0.0f, road = 0.0f, ramp = 0.0f, structural = 0.0f;
    float longest = 0.0f, run = 0.0f;
    bool sawRamp = false, sawTerrain = false;
    for (LogisticsState state : {LogisticsState::Hauling, LogisticsState::RampApproach})
    {
        const auto points = physicalRouteWaypoints(state);
        float length = 0.0f;
        for (std::size_t i = 1; i < points.size(); ++i)
            length += glm::distance(points[i - 1], points[i]);
        total += length;
        const int samples = static_cast<int>(std::ceil(length / 0.20f));
        LogisticsSnapshot previous = samplePhysicalRoute(state, 0.0f);
        for (int i = 0; i <= samples; ++i)
        {
            const float p = static_cast<float>(i) / samples;
            const LogisticsSnapshot pose = samplePhysicalRoute(state, p);
            const glm::mat4 root = physicalSledgeRoot(pose);
            valid = valid && finiteVector(pose.sledgePosition) &&
                glm::distance(pose.sledgePosition, samplePhysicalRoute(state, p).sledgePosition) < 1.0e-6f;
            if (i > 0)
                maximumStep = std::max(maximumStep, std::abs(pose.sledgePosition.y - previous.sledgePosition.y));
            previous = pose;
            const glm::vec3 center{root * glm::vec4{0.0f, 0.05f, -0.53f, 1.0f}};
            const HorizontalFootprint footprint{{center.x, center.z}, {1.30f, 1.91f}, pose.sledgeHeading};
            for (const Obstacle& obstacle : obstacles)
                if (overlaps(footprint, obstacle.footprint))
                {
                    bool intersects = obstacle.topY > 50.0f;
                    if (!intersects)
                    {
                        HorizontalFootprint clearance = obstacle.footprint;
                        clearance.halfExtents += glm::vec2{0.20f};
                        const auto checkPart = [&](float halfWidth, float bottomY,
                                                   float back, float front)
                        {
                            for (float x : {-halfWidth, 0.0f, halfWidth})
                                for (int sample = 0; sample <= 16; ++sample)
                                {
                                    const float z = back + (front - back) * sample / 16.0f;
                                    const glm::vec3 bottom{root * glm::vec4{x, bottomY, z, 1.0f}};
                                    if (SceneSupport::footprintContains(clearance, {bottom.x, bottom.z}) &&
                                        bottom.y < obstacle.topY - 0.05f)
                                        intersects = true;
                                }
                        };
                        checkPart(0.74f, Sledge::runnerBottomLocalY, -1.375f, 1.375f);
                        checkPart(0.75f, 0.37f, -1.105f, 0.945f);
                        checkPart(0.51f, 0.24f, -2.435f, -1.085f);
                    }
                    if (intersects)
                    {
                        ++intersections;
                        auto& group = collisionGroups[obstacle.name];
                        ++group.first;
                        group.second = {pose.sledgePosition.x, pose.sledgePosition.z};
                        if (intersections <= 8)
                            output << "  collision " << obstacle.name << " at " << pose.sledgePosition.x << ',' << pose.sledgePosition.z << '\n';
                    }
                }
            float minimumGap = std::numeric_limits<float>::infinity();
            for (float x : {-0.74f, -0.50f, 0.50f, 0.74f})
                for (int j = 0; j <= 32; ++j)
                {
                    const glm::vec3 bottom{root * glm::vec4{x, Sledge::runnerBottomLocalY, -1.375f + 2.75f * j / 32.0f, 1.0f}};
                    const float gap = bottom.y - SceneSupport::transportAt({bottom.x, bottom.z}).height;
                    valid = valid && gap >= -1.0e-4f;
                    minimumGap = std::min(minimumGap, gap);
                }
            maximumSupportError = std::max(maximumSupportError, std::abs(minimumGap));
            const glm::vec3 runner{root * glm::vec4{0.0f, Sledge::runnerBottomLocalY, 0.0f, 1.0f}};
            const SupportSurface support = SceneSupport::transportAt({runner.x, runner.z});
            const int gx = std::clamp(static_cast<int>(std::round((runner.x - SandSimulation::WorldMinX) / SandSimulation::CellSizeX)), 0, SandSimulation::GridColumns - 1);
            const int gz = std::clamp(static_cast<int>(std::round((runner.z - SandSimulation::WorldMinZ) / SandSimulation::CellSizeZ)), 0, SandSimulation::GridRows - 1);
            const float ds = i == 0 ? 0.0f : length / samples;
            const bool mobile = sand.cellAt(gx, gz).mobility > 0.01f;
            const bool isSand = support.kind == SupportSurfaceKind::DesertGround && mobile;
            if (isSand)
            {
                exposed += ds;
                run += ds;
                longest = std::max(longest, run);
                sawTerrain = true;
                maximumSandError = std::max(maximumSandError, std::abs(runner.y - sand.terrainHeightAt(runner.x, runner.z)));
            }
            else
            {
                run = 0.0f;
                if (support.kind == SupportSurfaceKind::Road) road += ds;
                else if (support.kind == SupportSurfaceKind::RampSurface) { ramp += ds; sawRamp = true; }
                else structural += ds;
            }
        }
    }
    bool cargoConsistent = true, traversalConsistent = true;
    float maximumFrameRateError = 0.0f, maximumCargoError = 0.0f;
    const auto atTime = [&](int fps, float time)
    {
        ConstructionSimulation simulation;
        simulation.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));
        simulation.spawnNewBlock({-121.0f, -6.65f, -3.0f});
        simulation.activeBlocks.back().state = BlockState::Staged;
        QuarrySystem quarry;
        ConstructionTimelineController timeline;
        QuarryPulleyAnimationController pulley;
        ConstructionLogistics logistics;
        logistics.update(0.001f, simulation, quarry, timeline, pulley, true);
        const float fineWindow = std::min(0.1f, time);
        logistics.update(time - fineWindow, simulation, quarry, timeline, pulley, true);
        double elapsed = time - fineWindow;
        while (elapsed < time - 1.0e-8)
        {
            const float dt = static_cast<float>(std::min(1.0 / fps, time - elapsed));
            logistics.update(dt, simulation, quarry, timeline, pulley, true);
            elapsed += dt;
            const LogisticsSnapshot pose = logistics.snapshot();
            if (isPhysicalHaulingState(pose.state) && pose.state != LogisticsState::SledgeLoading)
            {
                const glm::vec3 mounted{physicalSledgeRoot(pose) * glm::vec4{cargoSocketOffset, 1.0f}};
                const ConstructionBlock* block = simulation.getBlock(logistics.activeBlockId());
                if (block)
                    maximumCargoError = std::max(maximumCargoError,
                        glm::distance(block->position, mounted));
                cargoConsistent = cargoConsistent && block && maximumCargoError <= 1.0e-4f;
            }
        }
        return logistics.snapshot();
    };
    for (float time : {19.5f, 42.0f, 70.5f, 74.5f, 77.0f})
    {
        const LogisticsSnapshot reference = atTime(60, time);
        for (int fps : {30, 144})
        {
            const LogisticsSnapshot compared = atTime(fps, time);
            const float error = glm::distance(reference.sledgePosition, compared.sledgePosition);
            maximumFrameRateError = std::max(maximumFrameRateError, error);
            traversalConsistent = traversalConsistent && reference.state == compared.state && error <= 0.003f;
        }
    }
    output << "  waypoints at visible-scene progress 0.75 (X, root Y, Z, actual surface):\n";
    for (LogisticsState state : {LogisticsState::Hauling, LogisticsState::RampApproach})
    {
        const auto points = physicalRouteWaypoints(state);
        float length = 0.0f, distance = 0.0f;
        for (std::size_t i = 1; i < points.size(); ++i) length += glm::distance(points[i - 1], points[i]);
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            if (i > 0) distance += glm::distance(points[i - 1], points[i]);
            const LogisticsSnapshot pose = samplePhysicalRoute(state, distance / length, 0.75f);
            output << "    " << stateName(state) << ": " << points[i].x << ", " << pose.sledgePosition.y << ", "
                   << points[i].y << ", " << SceneSupport::surfaceKindName(SceneSupport::transportAt(points[i], 0.75f).kind) << '\n';
        }
    }
    SceneSupport::setTerrainSource(nullptr);
    for (const auto& [name, group] : collisionGroups)
        output << "  obstacle " << name << ": " << group.first << " hits, last at "
               << group.second.x << ',' << group.second.y << '\n';
    valid = valid && intersections == 0 && exposed > 1.0f && sawRamp && sawTerrain &&
        maximumSupportError <= 0.08f && maximumSandError <= 0.08f && maximumStep <= 0.20f &&
        cargoConsistent && traversalConsistent;
    output << std::fixed << std::setprecision(4)
           << "Physical route/support validation\n"
           << "  total / sand / road / ramp / protected distance: " << total << " / " << exposed << " / " << road << " / " << ramp << " / " << structural << '\n'
           << "  longest sand segment / eligible percent: " << longest << " / " << 100.0f * exposed / total << '\n'
           << "  support / sand error / maximum 0.2m sample step: " << maximumSupportError << " / " << maximumSandError << " / " << maximumStep << '\n'
           << "  footprint intersections (0.2m clearance): " << intersections << '\n'
           << "  rigid cargo socket / 30-60-144 FPS traversal: " << (cargoConsistent ? "PASS" : "FAIL")
           << " (max socket error " << maximumCargoError << ")"
           << " / " << (traversalConsistent ? "PASS" : "FAIL") << " (max error " << maximumFrameRateError << ")\n"
           << (valid ? "Physical route/support checks passed.\n" : "Physical route/support checks failed.\n");
    return valid;
}

namespace
{
bool validateMainRampAscent(std::ostream& output)
{
    const auto route = ConstructionLogistics::physicalRouteWaypoints(LogisticsState::RampAscent);
    const float length = horizontalRouteLength(route);
    const LogisticsSnapshot approach = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::RampApproach, 1.0f, 0.75f);
    const LogisticsSnapshot start = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::RampAscent, 0.0f, 0.75f);
    const LogisticsSnapshot end = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::RampAscent, 1.0f, 0.75f);
    const RampDescriptor& ramp = MonumentalSite::mainRamp();
    const SupportSurface rampMid = SceneSupport::transportAt(
        {0.0f, 0.5f * (ramp.base.z + ramp.top.z)}, 0.75f);
    const SupportSurface landing = SceneSupport::transportAt(
        {0.0f, -4.60f}, 0.75f);
    const SupportSurface deck = SceneSupport::transportAt(
        {end.sledgePosition.x, end.sledgePosition.z}, 0.75f);
    float maximumStep = 0.0f, maximumPitchStep = 0.0f;
    float maximumContactError = 0.0f, maximumFloatingGap = 0.0f;
    float maximumCargoError = 0.0f, maximumGroundingError = 0.0f;
    float minimumRopeClearance = std::numeric_limits<float>::infinity();
    float maximumPitch = 0.0f;
    bool finite = true, noSandOnStructure = true, withinRampWidth = true;
    struct CollisionBox { const char* name; glm::vec3 center; glm::vec3 half; };
    std::vector<CollisionBox> obstacles;
    const auto obstacle = [&](const char* name, glm::vec3 center, glm::vec3 size)
    { obstacles.push_back({name, center, 0.5f * size}); };
    for (float x : {-14.0f, -6.0f, 4.0f, 10.0f})
        for (float z : {0.0f, 5.0f})
            obstacle("DeckPost", {x, 4.10f, z}, {0.42f, 8.20f, 0.42f});
    for (float x : {5.20f, 9.80f, 14.40f, 19.00f})
        for (float z : {-6.0f, -3.6f})
            obstacle("RampTopScaffold", {x, 4.05f, z}, {0.18f, 8.10f, 0.18f});
    for (int block = 0; block < 4; ++block)
        obstacle("UpperPreparedStone", {6.0f + (block % 2) * 3.0f,
            9.25f, 1.0f + (block / 2) * 3.5f}, {2.45f, 1.35f, 2.30f});
    for (float x : {6.0f, 12.0f})
        for (float z : {-3.8f, -1.0f})
            obstacle("UpperSkid", {x, 8.725f, z}, {0.30f, 0.35f, 3.20f});
    for (float x : {-15.0f, -12.0f})
        obstacle("UpperLeverRack", {x, 9.45f, 3.8f}, {0.28f, 1.8f, 2.0f});
    for (float x : {-16.0f, -14.0f, 8.5f, 10.5f})
        obstacle("UpperRopePost", {x, 9.95f, -2.0f}, {0.12f, 2.8f, 0.12f});
    const auto bounds = [](const glm::mat4& model)
    {
        return CollisionBox{"", glm::vec3{model[3]},
            0.5f * (glm::abs(glm::vec3{model[0]}) +
                    glm::abs(glm::vec3{model[1]}) +
                    glm::abs(glm::vec3{model[2]}))};
    };
    const auto intersects = [](const CollisionBox& a, const CollisionBox& b)
    {
        const glm::vec3 distance = glm::abs(a.center - b.center);
        const glm::vec3 reach = a.half + b.half;
        return distance.x < reach.x - 0.01f &&
               distance.y < reach.y - 0.01f &&
               distance.z < reach.z - 0.01f;
    };
    const auto structuralBlocks = PyramidLayout::generateComplete(PyramidLayoutConfig{});
    const std::vector<ObjectPart> sledgeParts = Sledge::create(false);
    int propCollisions = 0, pyramidCollisions = 0;
    const auto checkCollision = [&](const CollisionBox& part)
    {
        for (const CollisionBox& item : obstacles)
            if (intersects(part, item))
            {
                ++propCollisions;
            }
        if (part.center.z > -3.0f || part.center.y > 24.0f)
            return;
        for (const PyramidBlockPlacement& stone : structuralBlocks)
        {
            if (stone.level >= ConstructionSimulation::prebuiltLevelCount)
                break;
            if (intersects(part, {"Pyramid", stone.position, 0.5f * stone.scale}))
                ++pyramidCollisions;
        }
    };
    const int samples = static_cast<int>(std::ceil(length / 0.10f));
    LogisticsSnapshot previous = start;
    for (int i = 0; i <= samples; ++i)
    {
        const LogisticsSnapshot pose = ConstructionLogistics::samplePhysicalRoute(
            LogisticsState::RampAscent, static_cast<float>(i) / samples, 0.75f);
        const glm::mat4 root = ConstructionLogistics::physicalSledgeRoot(pose);
        for (const ObjectPart& part : sledgeParts)
            if (std::string(part.name) != "PullingRope")
                checkCollision(bounds(root * part.localTransform));
        checkCollision(bounds(root * makeTransform(cargoSocketOffset, {},
            physicalBlockScale)));
        finite = finite && finiteVector(pose.sledgePosition) &&
            std::isfinite(pose.sledgeHeading) && std::isfinite(pose.sledgePitch);
        maximumPitch = std::max(maximumPitch, std::abs(pose.sledgePitch));
        if (i > 0)
        {
            const float step = std::abs(pose.sledgePosition.y - previous.sledgePosition.y);
            maximumStep = std::max(maximumStep, step);
            const float pitchStep = std::abs(pose.sledgePitch - previous.sledgePitch);
            maximumPitchStep = std::max(maximumPitchStep, pitchStep);
        }
        previous = pose;
        float minimumGap = std::numeric_limits<float>::infinity();
        for (float x : {-0.74f, -0.50f, 0.50f, 0.74f})
            for (int j = 0; j <= 32; ++j)
            {
                const glm::vec3 bottom{root * glm::vec4{x, Sledge::runnerBottomLocalY,
                    -1.375f + 2.75f * j / 32.0f, 1.0f}};
                const float gap = bottom.y - SceneSupport::transportAt(
                    {bottom.x, bottom.z}, 0.75f).height;
                minimumGap = std::min(minimumGap, gap);
                maximumFloatingGap = std::max(maximumFloatingGap, gap);
            }
        maximumContactError = std::max(maximumContactError, std::abs(minimumGap));
        for (std::size_t crew = 0; crew < ConstructionLogistics::pullingCrewSize(); ++crew)
        {
            const WorkerJointAngles angles = Worker::poseAngles(WorkerPose::PullingReady);
            const glm::mat4 worker = ConstructionLogistics::physicalWorkerRoot(pose, crew, angles);
            const Worker::EvaluatedPose evaluated = Worker::evaluate(worker, angles);
            for (const WorkerPartTransform& part : evaluated)
                checkCollision(bounds(part.model));
            const float foot = std::min(
                SceneSupport::transformedBottomY(evaluated[static_cast<std::size_t>(BodyPart::LeftFoot)].model),
                SceneSupport::transformedBottomY(evaluated[static_cast<std::size_t>(BodyPart::RightFoot)].model));
            const glm::vec3 workerPosition{worker[3]};
            maximumGroundingError = std::max(maximumGroundingError, std::abs(
                foot - SceneSupport::transportAt({workerPosition.x, workerPosition.z}, 0.75f).height -
                workerGroundClearance));
            const glm::vec3 hand = ConstructionLogistics::pullingHandPosition(evaluated, crew);
            const glm::vec3 tow = ConstructionLogistics::physicalTowPoint(pose, crew);
            for (int j = 0; j <= 8; ++j)
            {
                const glm::vec3 point = glm::mix(hand, tow, j / 8.0f);
                const float clearance = point.y - SceneSupport::transportAt({point.x, point.z}, 0.75f).height;
                minimumRopeClearance = std::min(minimumRopeClearance, clearance);
            }
        }
        const glm::vec3 runner{root * glm::vec4{0.0f, Sledge::runnerBottomLocalY, 0.0f, 1.0f}};
        if (std::abs(pose.sledgePosition.x - ramp.base.x) < 0.25f &&
            pose.sledgePosition.z < ramp.base.z - 2.0f &&
            pose.sledgePosition.z > ramp.top.z + 2.0f)
        {
            for (float x : {-1.2f, 1.2f})
                for (float z : {-1.5f, 1.5f})
                {
                    const glm::vec3 corner{root * glm::vec4{x, 0.0f, z, 1.0f}};
                    withinRampWidth = withinRampWidth &&
                        std::abs(corner.x - ramp.base.x) <= ramp.width * 0.5f - 0.1f;
                }
        }
        if (runner.z < 45.0f && runner.z > -5.0f)
            noSandOnStructure = noSandOnStructure &&
                SceneSupport::transportAt({runner.x, runner.z}, 0.75f).kind !=
                    SupportSurfaceKind::DesertGround;
    }
    const bool seamless = glm::distance(start.sledgePosition, approach.sledgePosition) < 0.01f;
    const bool supportKinds = rampMid.kind == SupportSurfaceKind::RampSurface &&
        landing.kind == SupportSurfaceKind::RampSurface &&
        deck.kind == SupportSurfaceKind::WorkPlatform;
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    simulation.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    const std::size_t baselineOccupied = simulation.occupiedTargetCount();
    timeline.setProgress(0.75f);
    quarry.startExtraction(simulation, 0);
    const std::uint64_t blockId = simulation.activeBlocks.empty()
        ? 0 : simulation.activeBlocks.front().id;
    bool approachReached = false, ascentReached = false, ascentCompleted = false;
    bool sameBlock = blockId == 1000, noPlacement = true;
    float maximumFrameDisplacement = 0.0f;
    glm::vec3 previousAscent{0.0f};
    bool havePreviousAscent = false;
    for (int step = 0; step < 3000 && !ascentCompleted; ++step)
    {
        quarry.update(0.05f, simulation);
        logistics.setSupportProgress(0.75f);
        logistics.update(0.05f, simulation, quarry, timeline, pulley, true);
        const LogisticsSnapshot pose = logistics.snapshot();
        const ConstructionBlock* block = findActiveBlock(simulation, blockId);
        sameBlock = sameBlock && block != nullptr &&
            (pose.state == LogisticsState::QuarryReady ||
             pose.state == LogisticsState::Extracting ||
             logistics.activeBlockId() == blockId);
        noPlacement = noPlacement && pose.state != LogisticsState::Placement &&
            pose.state != LogisticsState::Settled;
        if (pose.state == LogisticsState::RampApproach)
            approachReached = true;
        if (pose.state == LogisticsState::UpperTransfer && block != nullptr)
        {
            ascentCompleted = true;
            sameBlock = sameBlock && block->sledgeId == 1 &&
                block->state == BlockState::UpperTransfer &&
                glm::distance(pose.sledgePosition, end.sledgePosition) <
                    horizontalRouteLength(ConstructionLogistics::physicalRouteWaypoints(
                        LogisticsState::UpperTransfer)) / 7.5f * 0.05f + 0.06f;
            break;
        }
        if (pose.state == LogisticsState::RampAscent && block != nullptr)
        {
            ascentReached = true;
            const glm::vec3 mounted{ConstructionLogistics::physicalSledgeRoot(pose) *
                glm::vec4{cargoSocketOffset, 1.0f}};
            maximumCargoError = std::max(maximumCargoError,
                glm::distance(block->position, mounted));
            sameBlock = sameBlock && block->sledgeId == 1 &&
                block->state == BlockState::RampAscent;
            if (havePreviousAscent)
                maximumFrameDisplacement = std::max(maximumFrameDisplacement,
                    glm::distance(pose.sledgePosition, previousAscent));
            previousAscent = pose.sledgePosition;
            havePreviousAscent = true;
        }
    }
    const bool occupancyUnchanged = simulation.occupiedTargetCount() == baselineOccupied &&
        simulation.settledCount() == 0;
    const bool valid = finite && seamless && supportKinds && noSandOnStructure &&
        withinRampWidth &&
        maximumContactError <= 0.08f && maximumFloatingGap <= 0.25f &&
        maximumStep <= 0.15f && maximumPitchStep <= 5.0f &&
        maximumGroundingError <= 0.001f && minimumRopeClearance >= 0.08f &&
        maximumPitch >= 7.0f && std::abs(end.sledgePosition.y - 8.50f) < 0.20f &&
        propCollisions == 0 && pyramidCollisions == 0 &&
        approachReached && ascentReached && ascentCompleted && sameBlock &&
        noPlacement && occupancyUnchanged && maximumCargoError <= 0.001f &&
        maximumFrameDisplacement <= length / 8.0f * 0.05f + 0.05f;
    output << std::fixed << std::setprecision(4)
           << "Main pyramid ramp ascent validation\n"
           << "  start / end: (" << start.sledgePosition.x << ", "
           << start.sledgePosition.y << ", " << start.sledgePosition.z << ") / ("
           << end.sledgePosition.x << ", " << end.sledgePosition.y << ", "
           << end.sledgePosition.z << ")\n"
           << "  length / maximum pitch: " << length << " / " << maximumPitch << '\n'
           << "  main ramp / landing / deck support: " << (supportKinds ? "PASS" : "FAIL") << '\n'
           << "  contact / maximum local gap / 0.1m vertical step / pitch step: "
           << maximumContactError << " / " << maximumFloatingGap << " / "
           << maximumStep << " / " << maximumPitchStep << '\n'
           << "  cargo socket / worker grounding / minimum rope clearance: "
           << maximumCargoError << " / " << maximumGroundingError << " / "
           << minimumRopeClearance << '\n'
           << "  lifecycle approach / ascent / deck stop / same block / occupancy: "
           << (approachReached && ascentReached && ascentCompleted &&
               sameBlock && noPlacement && occupancyUnchanged ? "PASS" : "FAIL")
           << " (block " << blockId << ", frame displacement "
           << maximumFrameDisplacement << ")\n"
           << "  prop / pyramid part collisions: " << propCollisions << " / "
           << pyramidCollisions << '\n'
           << "  entry / ramp width / structural sand exclusion: "
           << (seamless && withinRampWidth && noSandOnStructure ? "PASS" : "FAIL")
           << " (" << seamless << ", " << withinRampWidth << ", "
           << noSandOnStructure << ")\n"
           << (valid ? "Main ramp ascent checks passed.\n" :
                       "Main ramp ascent checks failed.\n");
    return valid;
}

bool validateUpperAccessStageOneImpl(std::ostream& output)
{
    const UpperAccessLayout& access = MonumentalSite::upperAccessLayout();
    const RampDescriptor& ramp = MonumentalSite::upperRampA();
    const auto points = upperTransferStageOneWaypoints();
    const float length = horizontalRouteLength(points);
    const float wholeLength = horizontalRouteLength(
        ConstructionLogistics::physicalRouteWaypoints(LogisticsState::UpperTransfer));
    const float stageOneFraction = length / wholeLength;
    const LogisticsSnapshot preceding = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::RampAscent, 1.0f, 0.75f);
    const LogisticsSnapshot first = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::UpperTransfer, 0.0f, 0.75f);
    const LogisticsSnapshot last = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::UpperTransfer, stageOneFraction, 0.75f);
    const float topA = MonumentalSite::rampSurfacePoint(ramp, 0.0f).y;
    const float topB = MonumentalSite::rampSurfacePoint(ramp, 1.0f).y;
    const auto surface = [](glm::vec2 point)
    { return SceneSupport::transportAt(point, 0.75f); };
    const float apronTop = access.apron.center.y + 0.5f * access.apron.size.y;
    const float landingTop = access.turningLanding.center.y +
        0.5f * access.turningLanding.size.y;
    const bool geometry = std::abs(topA - apronTop) < 0.002f &&
        std::abs(topB - landingTop) < 0.002f &&
        std::abs(topB - 12.98476f) < 0.002f &&
        std::abs(MonumentalSite::rampFrame(ramp).slopeDegrees - 9.0f) < 0.10f;
    const bool handoff = glm::distance(preceding.sledgePosition,
        first.sledgePosition) < 0.01f &&
        std::abs(std::remainder(preceding.sledgeHeading -
            first.sledgeHeading, 360.0f)) < 0.01f;
    const bool supportKinds = surface({-9.0f, 7.0f}).kind ==
            SupportSurfaceKind::WorkPlatform &&
        surface({-31.0f, 11.0f}).kind == SupportSurfaceKind::RampSurface &&
        surface({-50.0f, 5.5f}).kind == SupportSurfaceKind::WorkPlatform;
    struct Box { glm::vec3 center; glm::vec3 half; };
    const auto bounds = [](const glm::mat4& model)
    { return Box{glm::vec3{model[3]}, 0.5f *
        (glm::abs(glm::vec3{model[0]}) + glm::abs(glm::vec3{model[1]}) +
         glm::abs(glm::vec3{model[2]}))}; };
    const auto intersects = [](const Box& a, const Box& b)
    {
        const glm::vec3 delta = glm::abs(a.center - b.center);
        const glm::vec3 reach = a.half + b.half;
        return delta.x < reach.x - 0.01f && delta.y < reach.y - 0.01f &&
               delta.z < reach.z - 0.01f;
    };
    std::vector<Box> obstacles;
    const auto addBox = [&](glm::vec3 center, glm::vec3 size)
    { obstacles.push_back({center, 0.5f * size}); };
    for (float x : {-14.0f, -6.0f})
        for (float z : {0.0f, 5.0f})
            addBox({x, 4.1f, z}, {0.42f, 8.2f, 0.42f});
    addBox({-11.5f, 9.8f, 2.0f}, {0.38f, 2.5f, 0.38f});
    for (float x : {-15.0f, -12.0f})
        addBox({x, 9.45f, 3.8f}, {0.28f, 1.8f, 2.0f});
    for (float x : {-16.5f, -8.0f})
        addBox({x, 4.1f, 12.5f}, {0.42f, 8.2f, 0.42f});
    for (float x : {-53.0f, -46.0f})
        for (float z : {-1.0f, 13.0f})
            addBox({x, 6.3f, z}, {0.42f, 12.6f, 0.42f});
    for (int bent = 1; bent <= 6; ++bent)
    {
        const glm::vec3 center = MonumentalSite::rampSurfacePoint(
            ramp, bent / 7.0f);
        for (float z : {7.95f, 14.05f})
            addBox({center.x, 0.5f * center.y, z},
                   {0.38f, center.y, 0.38f});
    }
    const auto parts = Sledge::create(false);
    float maximumContactError = 0.0f, maximumFloatingGap = 0.0f;
    float maximumYStep = 0.0f, maximumPitchStep = 0.0f;
    float maximumHeadingStep = 0.0f, maximumPitch = 0.0f;
    float maximumWorkerError = 0.0f, minimumRopeClearance = 1000.0f;
    int collisions = 0;
    bool finite = true, structural = true, workersAhead = true;
    const int count = static_cast<int>(std::ceil(length / 0.25f));
    LogisticsSnapshot previous = first;
    for (int i = 0; i <= count; ++i)
    {
        const LogisticsSnapshot pose = ConstructionLogistics::samplePhysicalRoute(
            LogisticsState::UpperTransfer,
            stageOneFraction * static_cast<float>(i) / count, 0.75f);
        const glm::mat4 root = ConstructionLogistics::physicalSledgeRoot(pose);
        const auto check = [&](const Box& body)
        {
            for (const Box& obstacle : obstacles)
                if (intersects(body, obstacle)) ++collisions;
            if (body.center.z < -1.18f && body.center.x > -40.82f &&
                body.center.x < 40.82f && body.center.y < 24.0f)
                ++collisions;
        };
        for (const ObjectPart& part : parts)
            if (std::string(part.name) != "PullingRope")
                check(bounds(root * part.localTransform));
        check(bounds(root * makeTransform(cargoSocketOffset, {}, physicalBlockScale)));
        finite = finite && finiteVector(pose.sledgePosition) &&
            std::isfinite(pose.sledgeHeading) && std::isfinite(pose.sledgePitch);
        maximumPitch = std::max(maximumPitch, std::abs(pose.sledgePitch));
        if (i > 0)
        {
            maximumYStep = std::max(maximumYStep,
                std::abs(pose.sledgePosition.y - previous.sledgePosition.y));
            maximumPitchStep = std::max(maximumPitchStep,
                std::abs(pose.sledgePitch - previous.sledgePitch));
            const float headingStep = std::abs(std::remainder(
                pose.sledgeHeading - previous.sledgeHeading, 360.0f));
            maximumHeadingStep = std::max(maximumHeadingStep, headingStep);
        }
        previous = pose;
        float leastGap = 1000.0f;
        for (float x : {-0.74f, 0.74f})
            for (int j = 0; j <= 24; ++j)
            {
                const glm::vec3 bottom{root * glm::vec4{x,
                    Sledge::runnerBottomLocalY,
                    -1.375f + 2.75f * j / 24.0f, 1.0f}};
                const float gap = bottom.y - surface({bottom.x, bottom.z}).height;
                leastGap = std::min(leastGap, gap);
                maximumFloatingGap = std::max(maximumFloatingGap, gap);
            }
        maximumContactError = std::max(maximumContactError, std::abs(leastGap));
        if (surface({pose.sledgePosition.x, pose.sledgePosition.z}).kind ==
            SupportSurfaceKind::DesertGround)
            structural = false;
        for (std::size_t crew = 0; crew < ConstructionLogistics::pullingCrewSize(); ++crew)
        {
            const WorkerJointAngles angles = Worker::poseAngles(WorkerPose::PullingReady);
            const glm::mat4 worker = ConstructionLogistics::physicalWorkerRoot(
                pose, crew, angles);
            const Worker::EvaluatedPose evaluated = Worker::evaluate(worker, angles);
            for (const WorkerPartTransform& part : evaluated)
                check(bounds(part.model));
            const glm::vec3 workerAt{worker[3]};
            const float foot = std::min(
                SceneSupport::transformedBottomY(evaluated[static_cast<std::size_t>(BodyPart::LeftFoot)].model),
                SceneSupport::transformedBottomY(evaluated[static_cast<std::size_t>(BodyPart::RightFoot)].model));
            maximumWorkerError = std::max(maximumWorkerError, std::abs(
                foot - surface({workerAt.x, workerAt.z}).height - workerGroundClearance));
            if (surface({workerAt.x, workerAt.z}).kind ==
                SupportSurfaceKind::DesertGround)
                structural = false;
            const glm::vec3 lead = workerAt - pose.sledgePosition;
            workersAhead = workersAhead && glm::length(glm::vec2{lead.x, lead.z}) > 2.75f;
            const glm::vec3 hand = ConstructionLogistics::pullingHandPosition(evaluated, crew);
            const glm::vec3 tow = ConstructionLogistics::physicalTowPoint(pose, crew);
            for (int j = 0; j <= 8; ++j)
            {
                const glm::vec3 p = glm::mix(hand, tow, j / 8.0f);
                const float clearance = p.y - surface({p.x, p.z}).height;
                minimumRopeClearance = std::min(minimumRopeClearance, clearance);
            }
        }
    }
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    simulation.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    const std::size_t baselineOccupied = simulation.occupiedTargetCount();
    timeline.setProgress(0.75f);
    quarry.startExtraction(simulation, 0);
    const std::uint64_t blockId = simulation.activeBlocks.front().id;
    bool entered = false, parked = false, sameBlock = blockId == 1000;
    float maximumCargoError = 0.0f, maximumFrameStep = 0.0f;
    glm::vec3 precedingPosition{0.0f};
    bool havePrevious = false;
    for (int step = 0; step < 3000 && !parked; ++step)
    {
        quarry.update(0.05f, simulation);
        logistics.setSupportProgress(0.75f);
        logistics.update(0.05f, simulation, quarry, timeline, pulley, true);
        const LogisticsSnapshot pose = logistics.snapshot();
        if (pose.state != LogisticsState::UpperTransfer) continue;
        entered = true;
        const ConstructionBlock* block = findActiveBlock(simulation, blockId);
        sameBlock = sameBlock && block != nullptr && block->sledgeId == 1 &&
            block->state == BlockState::UpperTransfer;
        if (block != nullptr)
        {
            const glm::vec3 socket{ConstructionLogistics::physicalSledgeRoot(pose) *
                glm::vec4{cargoSocketOffset, 1.0f}};
            maximumCargoError = std::max(maximumCargoError,
                glm::distance(block->position, socket));
        }
        if (havePrevious)
            maximumFrameStep = std::max(maximumFrameStep,
                glm::distance(precedingPosition, pose.sledgePosition));
        precedingPosition = pose.sledgePosition;
        havePrevious = true;
        parked = pose.stateProgress >= stageOneFraction;
    }
    const bool occupancy = simulation.occupiedTargetCount() == baselineOccupied &&
        simulation.settledCount() == 0;
    const bool valid = geometry && handoff && supportKinds && finite && structural &&
        workersAhead && collisions == 0 && maximumContactError <= 0.08f &&
        maximumFloatingGap <= 0.25f && maximumYStep <= 0.15f &&
        maximumPitchStep <= 5.0f && maximumHeadingStep <= 5.0f &&
        maximumWorkerError <= 0.001f && minimumRopeClearance >= 0.08f &&
        maximumPitch >= 7.0f && entered && parked && sameBlock && occupancy &&
        maximumCargoError <= 0.001f &&
        maximumFrameStep <= wholeLength / stateBaseDuration(
            LogisticsState::UpperTransfer) * 0.05f + 0.06f &&
        glm::distance(last.sledgePosition, logistics.snapshot().sledgePosition) <
            wholeLength / stateBaseDuration(LogisticsState::UpperTransfer) * 0.05f + 0.06f;
    output << std::fixed << std::setprecision(4)
           << "Upper access stage-one validation\n"
           << "  apron / Ramp A / landing top: " << apronTop << " / " << topB
           << " / " << landingTop << " (geometry " << geometry << ")\n"
           << "  route / Ramp A / turning arc: " << length << " / 28.0000 / "
           << access.turnRadius * access.turnRadians << "\n"
           << "  handoff / structural support: " << handoff << " / "
           << (supportKinds && structural) << '\n'
           << "  runner contact / local gap / vertical step: " << maximumContactError
           << " / " << maximumFloatingGap << " / " << maximumYStep << '\n'
           << "  pitch / pitch step / heading step: " << maximumPitch << " / "
           << maximumPitchStep << " / " << maximumHeadingStep << '\n'
           << "  worker foot error / minimum rope clearance: "
           << maximumWorkerError << " / " << minimumRopeClearance << '\n'
           << "  collisions / cargo socket / frame step: " << collisions << " / "
           << maximumCargoError << " / " << maximumFrameStep << '\n'
           << "  entered / reached turn / same block / occupancy unchanged: "
           << entered << " / " << parked << " / " << sameBlock << " / " << occupancy << '\n'
           << (valid ? "Upper access stage-one checks passed.\n" :
                       "Upper access stage-one checks failed.\n");
    return valid;
}

bool validateUpperAccessStageTwoImpl(std::ostream& output)
{
    const RampDescriptor& ramp = MonumentalSite::upperRampB();
    const RampDescriptor& landing = MonumentalSite::targetLevelLanding();
    const RampFrame rampFrame = MonumentalSite::rampFrame(ramp);
    const auto wholeRoute = upperTransferWaypoints();
    const float stageOneLength = horizontalRouteLength(upperTransferStageOneWaypoints());
    const float wholeLength = horizontalRouteLength(wholeRoute);
    const float stageTwoLength = wholeLength - stageOneLength;
    const float entryProgress = stageOneLength / wholeLength;
    const LogisticsSnapshot entry = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::UpperTransfer, entryProgress, 0.75f);
    const LogisticsSnapshot parked = ConstructionLogistics::samplePhysicalRoute(
        LogisticsState::UpperTransfer, 1.0f, 0.75f);
    const glm::vec3 rampToe = MonumentalSite::rampSurfacePoint(ramp, 0.0f);
    const glm::vec3 rampTop = MonumentalSite::rampSurfacePoint(ramp, 1.0f);
    const glm::vec3 landingToe = MonumentalSite::rampSurfacePoint(landing, 0.0f);
    const auto surface = [](glm::vec2 xz)
    { return SceneSupport::transportAt(xz, 0.75f); };
    const bool connection = glm::distance(glm::vec2{entry.sledgePosition.x,
        entry.sledgePosition.z}, glm::vec2{ramp.base.x, ramp.base.z}) < 0.005f &&
        glm::distance(rampTop, landingToe) < 0.45f &&
        std::abs(rampToe.y - 12.98476f) < 0.005f &&
        std::abs(rampTop.y - 24.0f) < 0.005f;
    const bool supportKinds = surface({-40.0f, -27.0f}).kind ==
            SupportSurfaceKind::RampSurface &&
        surface({parked.sledgePosition.x, parked.sledgePosition.z}).kind ==
            SupportSurfaceKind::WorkPlatform;

    ConstructionSimulation geometrySimulation;
    geometrySimulation.initialize(PyramidLayout::generateComplete(
        PyramidLayoutConfig{}));
    struct Box { glm::vec3 center; glm::vec3 half; };
    std::vector<Box> occupied;
    for (std::size_t i = 0; i < geometrySimulation.layout.size(); ++i)
    {
        if (!geometrySimulation.isTargetPrebuilt(i)) continue;
        const PyramidBlockPlacement& block = geometrySimulation.layout[i];
        if (block.position.x > -17.0f || block.position.x < -58.0f ||
            block.position.z < -76.0f || block.position.z > 8.0f)
            continue;
        occupied.push_back({block.position, 0.5f * block.scale});
    }
    const auto bounds = [](const glm::mat4& model)
    { return Box{glm::vec3{model[3]}, 0.5f *
        (glm::abs(glm::vec3{model[0]}) + glm::abs(glm::vec3{model[1]}) +
         glm::abs(glm::vec3{model[2]}))}; };
    const auto clearance = [&](const Box& body)
    {
        float best = std::numeric_limits<float>::infinity();
        for (const Box& block : occupied)
        {
            if (body.center.y - body.half.y >= block.center.y + block.half.y - 0.001f ||
                body.center.y + body.half.y <= block.center.y - block.half.y + 0.001f)
                continue;
            const glm::vec2 delta = glm::abs(glm::vec2{body.center.x - block.center.x,
                body.center.z - block.center.z}) - glm::vec2{body.half.x + block.half.x,
                body.half.z + block.half.z};
            best = std::min(best, glm::length(glm::max(delta, glm::vec2{0.0f})));
        }
        return best;
    };
    float rampClearance = std::numeric_limits<float>::infinity();
    float rampSupportError = 0.0f;
    for (int i = 0; i <= 340; ++i)
    {
        const glm::vec3 top = MonumentalSite::rampSurfacePoint(ramp, i / 340.0f);
        for (int j = -14; j <= 14; ++j)
        {
            const glm::vec3 edge = top + rampFrame.right * (j * 0.25f);
            rampClearance = std::min(rampClearance, clearance(
                {edge - rampFrame.up * (0.5f * ramp.thickness),
                 {0.005f, 0.35f, 0.005f}}));
            if (j % 7 == 0)
                rampSupportError = std::max(rampSupportError,
                    std::abs(surface({edge.x, edge.z}).height - edge.y));
        }
    }
    float landingSupportError = 0.0f;
    float landingClearance = std::numeric_limits<float>::infinity();
    const RampFrame landingFrame = MonumentalSite::rampFrame(landing);
    for (int i = 0; i <= 38; ++i)
        for (int j = -22; j <= 22; ++j)
        {
            const glm::vec3 point = MonumentalSite::rampSurfacePoint(
                landing, i / 38.0f) + landingFrame.right * (j * 0.10f);
            landingClearance = std::min(landingClearance, clearance(
                {point - landingFrame.up * (0.5f * landing.thickness),
                 {0.005f, 0.175f, 0.005f}}));
            if (j % 11 == 0)
                landingSupportError = std::max(landingSupportError,
                    std::abs(surface({point.x, point.z}).height - point.y));
        }
    int groundLegs = 0, courseLegs = 0, supportPenetrations = 0;
    int bracePenetrations = 0;
    std::vector<Box> supportLegs;
    const auto checkLegs = [&](const RampDescriptor& deck, int bays, float lateral)
    {
        const RampFrame frame = MonumentalSite::rampFrame(deck);
        for (int bay = 1; bay <= bays; ++bay)
        {
            const glm::vec3 top = MonumentalSite::rampSurfacePoint(
                deck, static_cast<float>(bay) / (bays + 1));
            for (float side : {-1.0f, 1.0f})
            {
                const glm::vec3 end = top - frame.up * deck.thickness +
                    frame.right * side * lateral;
                const UpperSupportFooting footing = MonumentalSite::upperSupportFooting(
                    {end.x, end.z}, end.y);
                if (!footing.valid)
                { ++supportPenetrations; continue; }
                const float foundation = footing.onCourse ? footing.courseTop :
                    SceneSupport::terrainAt(footing.position).height;
                if (footing.onCourse) ++courseLegs; else ++groundLegs;
                if (foundation >= end.y - 0.15f) ++supportPenetrations;
                const Box leg{{footing.position.x, 0.5f * (end.y + foundation),
                    footing.position.y},
                    {0.19f, 0.5f * (end.y - foundation), 0.19f}};
                supportLegs.push_back(leg);
                if (clearance(leg) < 0.001f)
                    ++supportPenetrations;
                if (bays == 9)
                    for (int j = 0; j <= 24; ++j)
                    {
                        const glm::vec3 brace = glm::mix(
                            glm::vec3{footing.position.x,
                                      foundation + 0.55f * (end.y - foundation),
                                      footing.position.y},
                            top - frame.up * deck.thickness, j / 24.0f);
                        if (clearance({brace, {0.13f, 0.13f, 0.13f}}) < 0.001f)
                            ++bracePenetrations;
                    }
            }
        }
    };
    checkLegs(ramp, 9, 3.05f);
    checkLegs(landing, 2, 1.85f);
    for (float z : {-65.36f, -62.44f})
        for (int j = 0; j <= 16; ++j)
        {
            const float x = -26.2f + 1.30f * j / 16.0f;
            if (clearance({{x, 23.43f, z}, {0.10f, 0.10f, 0.10f}}) < 0.001f)
                ++bracePenetrations;
        }

    const auto parts = Sledge::create(false);
    float sledgeClearance = std::numeric_limits<float>::infinity();
    float cargoClearance = std::numeric_limits<float>::infinity();
    float workerClearance = std::numeric_limits<float>::infinity();
    float maximumContactError = 0.0f, maximumLocalGap = 0.0f;
    float maximumWorkerError = 0.0f, minimumRopeHeight = 1000.0f;
    float minimumRopeClearance = std::numeric_limits<float>::infinity();
    float maximumYStep = 0.0f, maximumPitchStep = 0.0f;
    float maximumHeadingStep = 0.0f, maximumPitch = 0.0f;
    int formationSupportCollisions = 0;
    bool finite = true, structural = true, workersAhead = true;
    glm::vec3 largestGapAt{0.0f}, largestFrameAt{0.0f};
    const int samples = static_cast<int>(std::ceil(stageTwoLength / 0.25f));
    LogisticsSnapshot previous = entry;
    for (int i = 0; i <= samples; ++i)
    {
        const float progress = entryProgress + (1.0f - entryProgress) * i / samples;
        const LogisticsSnapshot pose = ConstructionLogistics::samplePhysicalRoute(
            LogisticsState::UpperTransfer, progress, 0.75f);
        const glm::mat4 root = ConstructionLogistics::physicalSledgeRoot(pose);
        const auto checkSupportCollision = [&](const Box& body)
        {
            for (const Box& leg : supportLegs)
            {
                const glm::vec3 reach = body.half + leg.half;
                const glm::vec3 delta = glm::abs(body.center - leg.center);
                if (delta.x < reach.x - 0.01f &&
                    delta.y < reach.y - 0.01f &&
                    delta.z < reach.z - 0.01f)
                    ++formationSupportCollisions;
            }
        };
        for (const ObjectPart& part : parts)
            if (std::string(part.name) != "PullingRope")
            {
                const Box body = bounds(root * part.localTransform);
                sledgeClearance = std::min(sledgeClearance, clearance(body));
                checkSupportCollision(body);
            }
        const Box cargo = bounds(root * makeTransform(cargoSocketOffset, {},
            physicalBlockScale));
        cargoClearance = std::min(cargoClearance, clearance(cargo));
        checkSupportCollision(cargo);
        finite = finite && finiteVector(pose.sledgePosition) &&
            std::isfinite(pose.sledgeHeading) && std::isfinite(pose.sledgePitch);
        maximumPitch = std::max(maximumPitch, std::abs(pose.sledgePitch));
        if (i > 0)
        {
            maximumYStep = std::max(maximumYStep,
                std::abs(pose.sledgePosition.y - previous.sledgePosition.y));
            maximumPitchStep = std::max(maximumPitchStep,
                std::abs(pose.sledgePitch - previous.sledgePitch));
            maximumHeadingStep = std::max(maximumHeadingStep,
                std::abs(std::remainder(pose.sledgeHeading - previous.sledgeHeading,
                                        360.0f)));
        }
        previous = pose;
        float leastGap = 1000.0f;
        for (float x : {-0.74f, 0.74f})
            for (int j = 0; j <= 24; ++j)
            {
                const glm::vec3 bottom{root * glm::vec4{x,
                    Sledge::runnerBottomLocalY, -1.375f + 2.75f * j / 24.0f, 1.0f}};
                const float gap = bottom.y - surface({bottom.x, bottom.z}).height;
                leastGap = std::min(leastGap, gap);
                if (gap > maximumLocalGap)
                { maximumLocalGap = gap; largestGapAt = bottom; }
            }
        maximumContactError = std::max(maximumContactError, std::abs(leastGap));
        structural = structural && surface({pose.sledgePosition.x,
            pose.sledgePosition.z}).kind != SupportSurfaceKind::DesertGround;
        for (std::size_t crew = 0; crew < ConstructionLogistics::pullingCrewSize(); ++crew)
        {
            const WorkerJointAngles angles = Worker::poseAngles(WorkerPose::PullingReady);
            const glm::mat4 worker = ConstructionLogistics::physicalWorkerRoot(
                pose, crew, angles);
            const Worker::EvaluatedPose evaluated = Worker::evaluate(worker, angles);
            for (const WorkerPartTransform& part : evaluated)
            {
                const Box body = bounds(part.model);
                workerClearance = std::min(workerClearance, clearance(body));
                checkSupportCollision(body);
            }
            const glm::vec3 at{worker[3]};
            const float foot = std::min(
                SceneSupport::transformedBottomY(evaluated[static_cast<std::size_t>(BodyPart::LeftFoot)].model),
                SceneSupport::transformedBottomY(evaluated[static_cast<std::size_t>(BodyPart::RightFoot)].model));
            maximumWorkerError = std::max(maximumWorkerError,
                std::abs(foot - surface({at.x, at.z}).height - workerGroundClearance));
            structural = structural && surface({at.x, at.z}).kind !=
                SupportSurfaceKind::DesertGround;
            workersAhead = workersAhead &&
                glm::length(glm::vec2{at.x - pose.sledgePosition.x,
                                      at.z - pose.sledgePosition.z}) > 2.75f;
            const glm::vec3 hand = ConstructionLogistics::pullingHandPosition(evaluated, crew);
            const glm::vec3 tow = ConstructionLogistics::physicalTowPoint(pose, crew);
            for (int j = 0; j <= 8; ++j)
            {
                const glm::vec3 rope = glm::mix(hand, tow, j / 8.0f);
                minimumRopeHeight = std::min(minimumRopeHeight,
                    rope.y - surface({rope.x, rope.z}).height);
                minimumRopeClearance = std::min(minimumRopeClearance,
                    clearance({rope, {0.05f, 0.05f, 0.05f}}));
            }
        }
    }

    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    simulation.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    const std::size_t baselineOccupied = simulation.occupiedTargetCount();
    timeline.setProgress(0.75f);
    quarry.startExtraction(simulation, 0);
    const std::uint64_t blockId = simulation.activeBlocks.front().id;
    const std::size_t targetIndex = simulation.activeBlocks.front().targetIndex;
    bool reached = false, sameBlock = blockId == 1000;
    float maximumCargoError = 0.0f, maximumFrameStep = 0.0f;
    glm::vec3 lastPosition{0.0f};
    bool hadPrevious = false;
    for (int step = 0; step < 3600 && !reached; ++step)
    {
        quarry.update(0.05f, simulation);
        logistics.setSupportProgress(0.75f);
        logistics.update(0.05f, simulation, quarry, timeline, pulley, true);
        const LogisticsSnapshot pose = logistics.snapshot();
        if (pose.state != LogisticsState::UpperTransfer) continue;
        const ConstructionBlock* block = findActiveBlock(simulation, blockId);
        sameBlock = sameBlock && block != nullptr && block->sledgeId == 1 &&
            block->state == BlockState::UpperTransfer;
        if (block != nullptr)
        {
            const glm::vec3 socket{ConstructionLogistics::physicalSledgeRoot(pose) *
                glm::vec4{cargoSocketOffset, 1.0f}};
            maximumCargoError = std::max(maximumCargoError,
                glm::distance(block->position, socket));
        }
        if (hadPrevious)
        {
            const float stepDistance = glm::distance(lastPosition, pose.sledgePosition);
            if (stepDistance > maximumFrameStep)
            { maximumFrameStep = stepDistance; largestFrameAt = pose.sledgePosition; }
        }
        lastPosition = pose.sledgePosition;
        hadPrevious = true;
        reached = pose.stateProgress >= 1.0f - 1.0e-5f;
    }
    const bool occupancy = simulation.occupiedTargetCount() == baselineOccupied &&
        simulation.settledCount() == 0 &&
        !simulation.isTargetOccupied(targetIndex) && targetIndex == 6218;
    const glm::vec3 cargoCenter{ConstructionLogistics::physicalSledgeRoot(parked) *
        glm::vec4{cargoSocketOffset, 1.0f}};
    const glm::vec3 target = simulation.layout[targetIndex].position;
    const float targetHorizontal = glm::distance(
        glm::vec2{cargoCenter.x, cargoCenter.z}, glm::vec2{target.x, target.z});
    const glm::vec2 towardTarget = glm::normalize(glm::vec2{
        target.x - cargoCenter.x, target.z - cargoCenter.z});
    const glm::mat4 parkedRoot = ConstructionLogistics::physicalSledgeRoot(parked);
    const float cargoReach = 0.5f * physicalBlockScale.x *
        std::abs(glm::dot(towardTarget,
            glm::vec2{parkedRoot[0].x, parkedRoot[0].z})) +
        0.5f * physicalBlockScale.z *
        std::abs(glm::dot(towardTarget,
            glm::vec2{parkedRoot[2].x, parkedRoot[2].z}));
    const float targetReach = 0.5f * simulation.layout[targetIndex].scale.x *
        std::abs(towardTarget.x) +
        0.5f * simulation.layout[targetIndex].scale.z *
        std::abs(towardTarget.y);
    const float targetEdgeGap = std::max(0.0f,
        targetHorizontal - cargoReach - targetReach);
    const bool valid = connection && supportKinds && rampClearance > 0.25f &&
        landingClearance > 0.20f &&
        sledgeClearance > 0.05f && cargoClearance > 0.05f &&
        workerClearance > 0.05f && supportPenetrations == 0 &&
        bracePenetrations == 0 && formationSupportCollisions == 0 &&
        minimumRopeClearance > 0.05f &&
        groundLegs > 0 && courseLegs > 0 && rampSupportError < 0.03f &&
        landingSupportError < 0.03f && finite && structural && workersAhead &&
        maximumContactError < 0.08f && maximumLocalGap < 0.25f &&
        maximumWorkerError < 0.001f && minimumRopeHeight > 0.08f &&
        maximumYStep < 0.15f && maximumPitchStep < 5.0f &&
        maximumHeadingStep < 5.0f && maximumPitch > 8.0f && reached &&
        sameBlock && occupancy && maximumCargoError < 0.001f &&
        maximumFrameStep < wholeLength / stateBaseDuration(
            LogisticsState::UpperTransfer) * 0.05f + 0.06f &&
        glm::distance(parked.sledgePosition, logistics.snapshot().sledgePosition) < 0.01f;
    output << std::fixed << std::setprecision(4)
           << "Upper access stage-two validation\n"
           << "  Ramp B run / rise / slope: "
           << glm::length(glm::vec2{ramp.top.x-ramp.base.x,ramp.top.z-ramp.base.z})
           << " / " << rampTop.y-rampToe.y << " / " << rampFrame.slopeDegrees << '\n'
           << "  stage-two / whole route distance: " << stageTwoLength << " / "
           << wholeLength << '\n'
           << "  ramp / landing / sledge / cargo / worker clearance: "
           << rampClearance << " / " << landingClearance << " / "
           << sledgeClearance << " / " << cargoClearance << " / "
           << workerClearance << '\n'
           << "  ground / course legs / leg / brace penetrations: "
           << groundLegs << " / " << courseLegs << " / "
           << supportPenetrations << " / " << bracePenetrations << '\n'
           << "  formation/support collisions: " << formationSupportCollisions << '\n'
           << "  ramp / landing query error: " << rampSupportError << " / "
           << landingSupportError << '\n'
           << "  runner error / local gap / vertical step: " << maximumContactError
           << " / " << maximumLocalGap << " / " << maximumYStep
           << " (gap at " << largestGapAt.x << ',' << largestGapAt.z << ")\n"
           << "  pitch / pitch step / heading step: " << maximumPitch << " / "
           << maximumPitchStep << " / " << maximumHeadingStep << '\n'
           << "  worker foot / rope support / rope pyramid clearance: "
           << maximumWorkerError << " / " << minimumRopeHeight << " / "
           << minimumRopeClearance << '\n'
           << "  cargo socket / frame step: " << maximumCargoError << " / "
           << maximumFrameStep << " (frame at " << largestFrameAt.x << ','
           << largestFrameAt.z << ")\n"
           << "  parked sledge: " << parked.sledgePosition.x << ','
           << parked.sledgePosition.y << ',' << parked.sledgePosition.z << '\n'
           << "  cargo / target: " << cargoCenter.x << ',' << cargoCenter.y << ','
           << cargoCenter.z << " / " << target.x << ',' << target.y << ','
           << target.z << " (horizontal " << targetHorizontal
           << ", edge gap " << targetEdgeGap
           << ", center elevation " << cargoCenter.y - target.y << ")\n"
           << "  connected / supported / reached / same block / occupancy: "
           << connection << " / " << supportKinds << " / " << reached << " / "
           << sameBlock << " / " << occupancy << '\n'
           << (valid ? "Upper access stage-two checks passed.\n" :
                       "Upper access stage-two checks failed.\n");
    return valid;
}
}

bool ConstructionLogistics::validateConstructionLogistics(std::ostream& output)
{
    const bool occupancyValid = validateConstructionOccupancy(output);
    const bool pulleyValid = validatePhysicalPulleyIntegration(output);
    const bool handoffValid = validateQuarryHandoffIntegration(output);
    const bool haulingCrewValid = validatePhysicalHaulingCrewIntegration(output);
    const bool routeValid = validatePhysicalRoute(output);
    const bool ascentValid = validateMainRampAscent(output);
    const bool upperValid = validateUpperAccessStageOneImpl(output);
    return occupancyValid && pulleyValid && handoffValid && haulingCrewValid &&
           routeValid && ascentValid && upperValid;
}

bool ConstructionLogistics::validateUpperAccessStageOne(std::ostream& output)
{
    return validateUpperAccessStageOneImpl(output);
}

bool ConstructionLogistics::validateUpperAccessStageTwo(std::ostream& output)
{
    return validateUpperAccessStageTwoImpl(output);
}

bool ConstructionLogistics::validateConstructionTrace(std::ostream& output)
{
    ConstructionSimulation sim;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    
    sim.initialize(PyramidLayout::generate(PyramidLayoutConfig()));
    const std::size_t baselineOccupied = sim.prebuiltCount();
    
    // Physical construction stops at the quarry sledge handoff in this phase.
    timeline.setProgress(0.0f);
    
    quarry.startExtraction(sim, 0);
    uint64_t targetBlockId = sim.activeBlocks.back().id;
    const std::size_t targetIndex = sim.activeBlocks.back().targetIndex;
    const std::size_t unrelatedTarget = targetIndex + 1;
    const bool occupiedBefore = sim.isTargetOccupied(targetIndex);
    
    output << "=== QUARRY HANDOFF TRACE ===\n\n";
    output << "Block " << targetBlockId << "\n";
    output << "Target Cell Index: " << targetIndex << "\n";
    output << "Target Occupied Before Handoff: "
           << (occupiedBefore ? "YES" : "NO") << "\n\n";
           
    const int maxSteps = 10000;
    const float dt = 0.1f;
    BlockState currentState = BlockState::QuarryBedrock;
    
    for (int step = 0; step < maxSteps; ++step) {
        quarry.update(dt, sim);
        
        ConstructionBlock* block = sim.getBlock(targetBlockId);
        if (block) {
            BlockState newState = block->state;
            if (newState != currentState) {
                output << (int)currentState << " -> " 
                       << (int)newState << "\n";
                output << "position = (" << block->position.x << ", " << block->position.y << ", " << block->position.z << ")\n";
                output << "target = (" << block->targetPlacement.position.x << ", " << block->targetPlacement.position.y << ", " << block->targetPlacement.position.z << ")\n\n";
                currentState = newState;
            }
        }
        
        logistics.update(dt, sim, quarry, timeline, pulley);
        
        block = sim.getBlock(targetBlockId);
        if (block) {
            BlockState newState = block->state;
            if (newState != currentState) {
                output << (int)currentState << " -> " 
                       << (int)newState << "\n";
                output << "position = (" << block->position.x << ", " << block->position.y << ", " << block->position.z << ")\n";
                output << "target = (" << block->targetPlacement.position.x << ", " << block->targetPlacement.position.y << ", " << block->targetPlacement.position.z << ")\n\n";
                currentState = newState;
            }
            if (newState == BlockState::LoadedOnSledge &&
                block->sledgeId == 1) {
                break;
            }
        }
    }
    
    ConstructionBlock* block = sim.getBlock(targetBlockId);
    if (!block) {
        output << "Block lost in simulation.\nFAIL\n";
        return false;
    }
    
    output << "Final Position: (" << block->position.x << ", " << block->position.y << ", " << block->position.z << ")\n";
    const LogisticsSnapshot loaded = logistics.snapshot();
    const glm::vec3 socket{physicalSledgeRoot(loaded) *
        glm::vec4{cargoSocketOffset, 1.0f}};
    output << "Sledge Socket: (" << socket.x << ", " << socket.y << ", " << socket.z << ")\n";
    
    float posError = glm::distance(block->position, socket);
    output << "Cargo Socket Error: " << posError << "\n";
    output << "Loaded: " << (block->sledgeId == 1 ? "YES" : "NO") << "\n\n";
    
    output << "Pyramid Runtime Settled Count Before: 0\n";
    output << "Pyramid Prebuilt Count: " << baselineOccupied << "\n";
    output << "Pyramid Settled Count At Handoff: " << sim.settledCount() << "\n\n";

    const bool occupiedAfter = sim.isTargetOccupied(targetIndex);
    const bool unrelatedOccupied = unrelatedTarget < sim.totalCount() &&
                                   sim.isTargetOccupied(unrelatedTarget);
    output << "Target Occupied At Handoff: "
           << (occupiedAfter ? "YES" : "NO") << "\n";
    output << "Unrelated Target Occupied: "
           << (unrelatedOccupied ? "YES" : "NO") << "\n\n";

    bool valid = !occupiedBefore && !occupiedAfter && !unrelatedOccupied &&
                 logistics.state() == LogisticsState::SledgeLoading &&
                 logistics.stateProgress() >= 1.0f &&
                 block->state == BlockState::LoadedOnSledge &&
                 block->id == targetBlockId && block->sledgeId == 1 &&
                 posError < 0.001f && sim.settledCount() == 0 &&
                 sim.occupiedTargetCount() == baselineOccupied;
    output << "Conservation Accounting Valid: " << (valid ? "YES" : "NO") << "\n\n";
    output << (valid ? "PASS\n" : "FAIL\n");
    
    return valid;
}

void ConstructionLogistics::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    // The authoritative block body is rendered by ConstructionSimulation.
    if (state_ == LogisticsState::Staged || state_ == LogisticsState::SledgeLoading || state_ == LogisticsState::Hauling ||
        state_ == LogisticsState::RampApproach || state_ == LogisticsState::RampAscent ||
        state_ == LogisticsState::UpperTransfer ||
        state_ == LogisticsState::LiftPrep || state_ == LogisticsState::Lifting ||
        state_ == LogisticsState::QuarryPlatformTransfer || state_ == LogisticsState::Placement)
    {
        // The main block body is now rendered by ConstructionSimulation::collectSceneObjects
        // Sledge ropes / lashings securing the block to sledge
        if (state_ == LogisticsState::Hauling || state_ == LogisticsState::RampAscent ||
            state_ == LogisticsState::UpperTransfer)
        {
            for (float zOffset : {-0.7f, 0.7f})
            {
                objects.push_back({ScenePrimitive::Cylinder,
                                   makeTransform(blockPosition_ + glm::vec3{0.0f, 0.68f, zOffset},
                                                 {0.0f, 0.0f, 90.0f}, {0.06f, 2.7f, 0.06f}),
                                   MaterialId::Rope});
            }
        }

        // The quarry pulley renderer owns the only hoisting rope and sling.
    }
}
