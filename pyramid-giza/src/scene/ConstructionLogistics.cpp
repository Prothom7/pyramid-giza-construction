#include "scene/ConstructionLogistics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionAnimation.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"
#include "scene/SupportSystem.h"
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

float pitchDegreesFor(const glm::vec3& direction)
{
    const float horizontal = glm::length(glm::vec2{direction.x, direction.z});
    return glm::degrees(std::atan2(direction.y, horizontal));
}

glm::vec3 normalizedRouteDirection(const glm::vec3& from,
                                   const glm::vec3& to)
{
    const glm::vec3 delta = to - from;
    const float length = glm::length(delta);
    return length > 1.0e-5f ? delta / length : glm::vec3{0.0f, 0.0f, -1.0f};
}

glm::vec3 formationOffset(std::size_t crewIndex)
{
    if (crewIndex >= pullingWorkerOffsets.size())
        throw std::out_of_range("Physical pulling-crew index is invalid");
    return pullingWorkerOffsets[crewIndex];
}

glm::vec3 pulleySledgeParkPosition()
{
    const glm::vec3 pickup =
        QuarryPulleyAnimationController::startLoadPosition();
    return {pickup.x - 3.6f,
            QuarryPulleyAnimationController::quarryFloorY(), pickup.z};
}

glm::vec3 pulleyMountedCargoPosition()
{
    return pulleySledgeParkPosition() + cargoSocketOffset;
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
    case LogisticsState::Hauling: return 7.0f;
    case LogisticsState::RampApproach: return 4.0f;
    case LogisticsState::RampAscent: return 8.0f;
    case LogisticsState::LiftPrep: return 3.0f;
    case LogisticsState::Lifting: return 6.0f;
    case LogisticsState::UpperStaging: return 4.0f;
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
    blockPosition_ = glm::vec3{-108.0f, -6.2f, -5.0f};
    blockRotation_ = glm::vec3{0.0f};
    blockScale_ = physicalBlockScale;
    sledgePosition_ = glm::vec3{-108.0f, -6.2f, -5.0f};
    sledgeHeading_ = 0.0f;
    sledgePitch_ = 0.0f;
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
    case LogisticsState::LiftPrep: return "Lift Preparation";
    case LogisticsState::Lifting: return "Pulley Lift -> Working Tier Elevation";
    case LogisticsState::UpperStaging: return "Upper Tier Staging";
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
    case LogisticsState::Staged:
    case LogisticsState::SledgeLoading: return "Quarry Extraction Bay C -> Staging Ground";
    case LogisticsState::Hauling: return "Transport Corridor -> Ramp Base";
    case LogisticsState::RampApproach:
    case LogisticsState::RampAscent:
    case LogisticsState::LiftPrep: return "Main Construction Ramp -> Lift Station";
    case LogisticsState::Lifting:
    case LogisticsState::UpperStaging:
    case LogisticsState::Placement: return "Lift Station -> Upper Construction Tier -> Target Cell";
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
           state == LogisticsState::RampAscent;
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
    const glm::mat4 root = physicalSledgeRoot(snapshot);
    const glm::vec3 normal =
        glm::normalize(glm::vec3{root * glm::vec4{0.0f, 1.0f, 0.0f, 0.0f}});
    if (std::abs(normal.y) <= 1.0e-5f)
        throw std::runtime_error("Physical hauling support plane is vertical");
    const glm::vec3 delta{worldPoint.x - snapshot.sledgePosition.x, 0.0f,
                          worldPoint.y - snapshot.sledgePosition.z};
    return snapshot.sledgePosition.y -
           (normal.x * delta.x + normal.z * delta.z) / normal.y;
}

glm::mat4 ConstructionLogistics::physicalWorkerRoot(
    const LogisticsSnapshot& snapshot, std::size_t crewIndex,
    const WorkerJointAngles& angles)
{
    const glm::vec3 anchor{physicalSledgeRoot(snapshot) *
                           glm::vec4{formationOffset(crewIndex), 1.0f}};
    // The transport frame supplies position and heading. Workers remain
    // upright while their feet are grounded to its inclined support plane.
    glm::mat4 root = makeTransform(anchor, {0.0f, snapshot.sledgeHeading, 0.0f},
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
    // Staging ground to base of ramp
    const glm::vec3 start{-108.0f, -6.0f, -5.0f};
    const glm::vec3 end{-10.0f, 1.0f, 42.0f};
    return glm::mix(start, end, progress);
}

glm::vec3 ConstructionLogistics::computeRampPosition(float progress) const
{
    // The legacy state name is retained, but the physical route now finishes
    // beside the actual supported quarry pulley rather than an imaginary lift.
    const glm::vec3 rampStart{0.0f, 1.0f, 45.0f};
    const glm::vec3 rampEnd = pulleySledgeParkPosition();
    return glm::mix(rampStart, rampEnd, progress);
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
        state_ = LogisticsState::SledgeLoading;
        activeWorkers_ = 6;
        ropeTaut_ = false;
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
        activeWorkers_ = 10;
        ropeTaut_ = true;
        break;

    case LogisticsState::RampAscent:
        state_ = LogisticsState::LiftPrep;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.setPhysicalPreparationProgress(activeBlockId_, 0.0f))
            throw std::runtime_error(
                "Failed to attach authoritative block to physical pulley");
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
        state_ = LogisticsState::UpperStaging;
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
        if (activeBlockId_ == 1000)
        {
            const glm::vec3 top =
                QuarryPulleyAnimationController::raisedLoadPosition();
            std::cout << "\n[POST-LIFT TRACE]\n"
                      << "Block " << activeBlockId_ << "\n"
                      << "LIFTING end: (" << top.x << ", " << top.y << ", "
                      << top.z << ")\n"
                      << "UPPER_STAGING start: (" << top.x << ", " << top.y
                      << ", " << top.z << ")" << std::endl;
        }
        break;

    case LogisticsState::UpperStaging:
        if (physicalLiftEnabled &&
            !pulley.setPhysicalTransferProgress(activeBlockId_, 1.0f))
            throw std::runtime_error(
                "Physical pulley failed to complete the platform transfer");
        state_ = LogisticsState::Placement;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.releasePhysicalBlock(activeBlockId_))
            throw std::runtime_error(
                "Physical pulley released an unexpected block identity");
        if (activeBlockId_ == 1000)
        {
            const glm::vec3 staging =
                QuarryPulleyAnimationController::destinationLoadPosition();
            const ConstructionBlock* block =
                findActiveBlock(simulation, activeBlockId_);
            std::cout << "\n[POST-LIFT TRACE]\n"
                      << "Block " << activeBlockId_ << "\n"
                      << "UPPER_STAGING end: (" << staging.x << ", "
                      << staging.y << ", " << staging.z << ")\n"
                      << "PLACEMENT start: (" << staging.x << ", "
                      << staging.y << ", " << staging.z << ")";
            if (block != nullptr)
            {
                const PostLiftRoute route =
                    makePostLiftRoute(block->targetPlacement);
                const glm::vec3 approach =
                    route.points[route.points.size() - 2];
                const glm::vec3 target = route.points.back();
                std::cout << "\nTarget approach: (" << approach.x << ", "
                          << approach.y << ", " << approach.z << ")\n"
                          << "Target: (" << target.x << ", " << target.y
                          << ", " << target.z << ")";
            }
            std::cout << std::endl;
        }
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
    stateTimer_ += deltaTime;
    stateProgress_ = std::min(1.0f, stateTimer_ / duration);

    bool shouldAdvance = false;
    switch (state_) {
        case LogisticsState::QuarryReady:
        case LogisticsState::Extracting:
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
    case LogisticsState::Staged:
        break;

    case LogisticsState::SledgeLoading:
    {
        sledgePosition_ = glm::vec3{-108.0f, -6.0f, -5.0f};
        const glm::vec3 direction = normalizedRouteDirection(
            computeHaulPosition(0.0f), computeHaulPosition(1.0f));
        sledgeHeading_ = headingDegreesFor(direction);
        sledgePitch_ = pitchDegreesFor(direction);
        const glm::vec3 groundStaging{-108.0f, -5.2f, -5.0f}; // Resting on ground (Y=-6.0 + 0.8)
        const LogisticsSnapshot loading = snapshot();
        const glm::vec3 mountedOnSledge{
            physicalSledgeRoot(loading) * glm::vec4{cargoSocketOffset, 1.0f}};
        blockPosition_ = glm::mix(groundStaging, mountedOnSledge, stateProgress_);
        blockRotation_ = glm::vec3{0.0f};
        blockScale_ = physicalBlockScale;
        break;
    }

    case LogisticsState::Hauling:
    {
        sledgePosition_ = computeHaulPosition(stateProgress_);
        const glm::vec3 direction = normalizedRouteDirection(
            computeHaulPosition(0.0f), computeHaulPosition(1.0f));
        sledgeHeading_ = headingDegreesFor(direction);
        sledgePitch_ = pitchDegreesFor(direction);
        const LogisticsSnapshot hauling = snapshot();
        blockPosition_ = glm::vec3{physicalSledgeRoot(hauling) *
                                   glm::vec4{cargoSocketOffset, 1.0f}};
        blockRotation_ = {sledgePitch_, sledgeHeading_, 0.0f};
        blockScale_ = physicalBlockScale;
        break;
    }
    case LogisticsState::RampApproach:
    {
        const glm::vec3 p0 = computeHaulPosition(1.0f);
        const glm::vec3 p1{0.0f, 0.0f, 45.0f};
        sledgePosition_ = glm::mix(p0, p1, stateProgress_);
        const glm::vec3 direction = normalizedRouteDirection(p0, p1);
        sledgeHeading_ = headingDegreesFor(direction);
        sledgePitch_ = pitchDegreesFor(direction);
        const LogisticsSnapshot approach = snapshot();
        blockPosition_ = glm::vec3{physicalSledgeRoot(approach) *
                                   glm::vec4{cargoSocketOffset, 1.0f}};
        blockRotation_ = {sledgePitch_, sledgeHeading_, 0.0f};
        blockScale_ = physicalBlockScale;
        break;
    }
    case LogisticsState::RampAscent:
    {
        sledgePosition_ = computeRampPosition(stateProgress_);
        const glm::vec3 direction = normalizedRouteDirection(
            computeRampPosition(0.0f), computeRampPosition(1.0f));
        sledgeHeading_ = headingDegreesFor(direction);
        sledgePitch_ = pitchDegreesFor(direction);
        blockRotation_ = {sledgePitch_, sledgeHeading_, 0.0f};
        const LogisticsSnapshot ramp = snapshot();
        blockPosition_ = glm::vec3{physicalSledgeRoot(ramp) *
                                   glm::vec4{cargoSocketOffset, 1.0f}};
        blockScale_ = physicalBlockScale;
        break;
    }
    case LogisticsState::LiftPrep:
    {
        sledgePosition_ = pulleySledgeParkPosition();
        sledgeHeading_ = 0.0f;
        sledgePitch_ = 0.0f;
        if (physicalLiftEnabled)
        {
            if (!pulley.setPhysicalPreparationProgress(activeBlockId_,
                                                       stateProgress_))
                throw std::runtime_error(
                    "Physical pulley preparation rejected the active block");
            // Finish positioning the stone before the controller reaches its
            // tension phase, so the sling always closes on the real payload.
            const float positioning = glm::smoothstep(
                0.0f, 0.55f, stateProgress_);
            blockPosition_ = glm::mix(
                pulleyMountedCargoPosition(),
                QuarryPulleyAnimationController::startLoadPosition(),
                positioning);
        }
        else
            blockPosition_ = pulleyMountedCargoPosition();
        blockRotation_ = glm::vec3{0.0f};
        blockScale_ = physicalBlockScale;
        break;
    }
    case LogisticsState::Lifting:
    {
        sledgePosition_ = pulleySledgeParkPosition();
        sledgeHeading_ = 0.0f;
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

    case LogisticsState::UpperStaging:
        sledgePosition_ = pulleySledgeParkPosition();
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
        sledgePosition_ = pulleySledgeParkPosition();
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
        sledgePosition_ = pulleySledgeParkPosition();
        blockPosition_ = computePlacementPosition(1.0f, simulation);
        blockRotation_ = glm::vec3{0.0f};
        if (const ConstructionBlock* active =
                findActiveBlock(simulation, activeBlockId_))
            blockScale_ = active->targetPlacement.scale;
        break;
    }

    if ((state_ == LogisticsState::UpperStaging ||
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
            block->sledgeId = (state_ >= LogisticsState::SledgeLoading && state_ <= LogisticsState::RampAscent) ? 1 : -1;
            block->taskProgress = stateProgress_;
            
            switch (state_) {
                case LogisticsState::SledgeLoading: block->state = BlockState::LoadedOnSledge; break;
                case LogisticsState::Hauling: block->state = BlockState::Hauling; break;
                case LogisticsState::RampApproach: block->state = BlockState::RampApproach; break;
                case LogisticsState::RampAscent: block->state = BlockState::RampAscent; break;
                case LogisticsState::LiftPrep: block->state = BlockState::LiftPrep; break;
                case LogisticsState::Lifting: block->state = BlockState::Lifting; break;
                case LogisticsState::UpperStaging: block->state = BlockState::UpperStaging; break;
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
        else if (logistics.state() == LogisticsState::UpperStaging)
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
    const float horizontalSledgeDistance = glm::length(glm::vec2{
        sledge.x - pickup.x, sledge.z - pickup.z});
    const bool sledgeAtRig = horizontalSledgeDistance <= 5.0f;
    const bool valid = pickupReached && liftSampled && attachmentAligned &&
                       ropeAligned && sameIdentity && singlePayload &&
                       topReached && sledgeAtRig;

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
           << "  sledge reaches visible pulley area: "
           << (sledgeAtRig ? "PASS" : "FAIL") << '\n'
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
    float maximumGroundingError = 0.0f;

    constexpr float deltaTime = 0.05f;
    constexpr int maximumSteps = 10000;
    for (int step = 0; step < maximumSteps; ++step)
    {
        quarry.update(deltaTime, simulation);
        logistics.update(deltaTime, simulation, quarry, timeline, pulley, true);
        const LogisticsSnapshot snapshot = logistics.snapshot();
        if (!ConstructionLogistics::isPhysicalHaulingState(snapshot.state))
        {
            if (snapshot.state == LogisticsState::LiftPrep)
                break;
            continue;
        }

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

            const glm::vec3 expectedWorld{
                sledgeRoot * glm::vec4{formationOffset(crewIndex), 1.0f}};
            formationSynchronized = formationSynchronized &&
                std::abs(workerPositions[crewIndex].x - expectedWorld.x) <=
                    1.0e-3f &&
                std::abs(workerPositions[crewIndex].z - expectedWorld.z) <=
                    1.0e-3f;

            glm::vec3 workerForward{
                root * glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}};
            workerForward.y = 0.0f;
            workerForward = glm::normalize(workerForward);
            workersFacing = workersFacing &&
                            glm::dot(workerForward, direction) >= 0.999f;
            glm::vec3 lead = workerPositions[crewIndex] - snapshot.sledgePosition;
            lead.y = 0.0f;
            workersInFront = workersInFront && glm::dot(lead, direction) > 3.5f;

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
           << (workersInFront && workersFacing ? "PASS" : "FAIL") << '\n'
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

bool validatePostLiftRouteIntegration(std::ostream& output)
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
        if (state == LogisticsState::UpperStaging ||
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

        if (state == LogisticsState::UpperStaging)
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
} // namespace

bool ConstructionLogistics::validateConstructionLogistics(std::ostream& output)
{
    const bool occupancyValid = validateConstructionOccupancy(output);
    const bool pulleyValid = validatePhysicalPulleyIntegration(output);
    const bool haulingCrewValid = validatePhysicalHaulingCrewIntegration(output);
    const bool postLiftValid = validatePostLiftRouteIntegration(output);
    return occupancyValid && pulleyValid && haulingCrewValid && postLiftValid;
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
    
    // Disable timeline so simulation only runs due to physical settlement
    timeline.setProgress(0.0f);
    
    quarry.startExtraction(sim, 0);
    uint64_t targetBlockId = sim.activeBlocks.back().id;
    const std::size_t targetIndex = sim.activeBlocks.back().targetIndex;
    const std::size_t unrelatedTarget = targetIndex + 1;
    const bool occupiedBefore = sim.isTargetOccupied(targetIndex);
    
    output << "=== CONSTRUCTION TRACE ===\n\n";
    output << "Block " << targetBlockId << "\n";
    output << "Target Cell Index: " << targetIndex << "\n";
    output << "Occupied Before Settlement: "
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
            if (newState == BlockState::Settled) {
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
    output << "Target Position: (" << block->targetPlacement.position.x << ", " << block->targetPlacement.position.y << ", " << block->targetPlacement.position.z << ")\n";
    
    float posError = glm::distance(block->position, block->targetPlacement.position);
    output << "Position Error: " << posError << "\n";
    output << "Settled: " << (block->state == BlockState::Settled ? "YES" : "NO") << "\n\n";
    
    output << "Pyramid Runtime Settled Count Before: 0\n";
    output << "Pyramid Prebuilt Count: " << baselineOccupied << "\n";
    output << "Pyramid Settled Count After: " << sim.settledCount() << "\n\n";

    const bool occupiedAfter = sim.isTargetOccupied(targetIndex);
    const bool unrelatedOccupied = unrelatedTarget < sim.totalCount() &&
                                   sim.isTargetOccupied(unrelatedTarget);
    output << "Occupied After Settlement: "
           << (occupiedAfter ? "YES" : "NO") << "\n";
    output << "Unrelated Target Occupied: "
           << (unrelatedOccupied ? "YES" : "NO") << "\n\n";

    bool valid = !occupiedBefore && occupiedAfter && !unrelatedOccupied &&
                 (block->state == BlockState::Settled) &&
                 (posError < 0.1f) && (sim.settledCount() == 1) &&
                 (sim.occupiedTargetCount() == baselineOccupied +
                                                   sim.settledCount());
    output << "Conservation Accounting Valid: " << (valid ? "YES" : "NO") << "\n\n";
    output << (valid ? "PASS\n" : "FAIL\n");
    
    return valid;
}

void ConstructionLogistics::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    // The authoritative block body is rendered by ConstructionSimulation.
    if (state_ == LogisticsState::Staged || state_ == LogisticsState::SledgeLoading || state_ == LogisticsState::Hauling ||
        state_ == LogisticsState::RampApproach || state_ == LogisticsState::RampAscent ||
        state_ == LogisticsState::LiftPrep || state_ == LogisticsState::Lifting ||
        state_ == LogisticsState::UpperStaging || state_ == LogisticsState::Placement)
    {
        // The main block body is now rendered by ConstructionSimulation::collectSceneObjects
        // Sledge ropes / lashings securing the block to sledge
        if (state_ == LogisticsState::Hauling || state_ == LogisticsState::RampAscent)
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
