#include "scene/ConstructionLogistics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionAnimation.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"
#include <iostream>

namespace
{
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
    case LogisticsState::UpperStaging: return 3.0f;
    case LogisticsState::Placement: return 5.0f;
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
    sledgePosition_ = glm::vec3{-108.0f, -6.2f, -5.0f};
    sledgeHeading_ = 0.0f;
    ropeTaut_ = false;
    activeWorkers_ = 4;
    liftHeight_ = 0.0f;
    justSettled_ = false;
    settledFlashTimer_ = 0.0f;
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
    s.ropeTaut = ropeTaut_;
    s.activeWorkers = activeWorkers_;
    s.liftHeight = liftHeight_;
    s.blockSettled = justSettled_;
    s.routeDescription = routeDescription();
    return s;
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
    // Ramp base up to lift level
    const glm::vec3 rampStart{0.0f, 1.0f, 45.0f};
    const glm::vec3 rampEnd{0.0f, 8.55f, -0.8f};
    return glm::mix(rampStart, rampEnd, progress);
}

glm::vec3 ConstructionLogistics::computeLiftPosition(float progress) const
{
    const glm::vec3 liftBottom{0.0f, 8.55f, -0.8f};
    const glm::vec3 liftTop{0.0f, 15.35f, -0.8f};
    return glm::mix(liftBottom, liftTop, progress);
}

glm::vec3 ConstructionLogistics::computePlacementPosition(float progress, ConstructionSimulation& simulation) const
{
    // Upper platform staging to actual target position
    const glm::vec3 stagePos{0.0f, 8.55f, -0.8f};
    glm::vec3 frontierPos = stagePos;
    
    if (activeBlockId_ != 0) {
        for (const auto& block : simulation.activeBlocks) {
            if (block.id == activeBlockId_) {
                frontierPos = block.targetPlacement.position;
                break;
            }
        }
    }
    
    return glm::mix(stagePos, frontierPos, progress);
}

void ConstructionLogistics::advanceState(ConstructionSimulation& simulation, QuarrySystem& quarry,
                                        ConstructionTimelineController& timeline,
                                        QuarryPulleyAnimationController& pulley)
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
        pulley.seek(0.0f, true);
        break;

    case LogisticsState::LiftPrep:
        state_ = LogisticsState::Lifting;
        activeWorkers_ = 6;
        ropeTaut_ = true;
        break;

    case LogisticsState::Lifting:
        state_ = LogisticsState::UpperStaging;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        break;

    case LogisticsState::UpperStaging:
        state_ = LogisticsState::Placement;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        break;

    case LogisticsState::Placement:
        state_ = LogisticsState::Settled;
        activeWorkers_ = 4;
        ropeTaut_ = false;
        justSettled_ = true;
        settledFlashTimer_ = 1.5f;
        simulation.incrementSettledCount();
        timeline.registerPhysicalBlockSettlement(simulation);
        break;

    case LogisticsState::Settled:
        state_ = LogisticsState::QuarryReady;
        activeBlockId_ = 0;
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
                                   QuarryPulleyAnimationController& pulley)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    const float duration = stateBaseDuration(state_);
    stateTimer_ += deltaTime;
    stateProgress_ = std::min(1.0f, stateTimer_ / duration);

    bool shouldAdvance = false;
    glm::vec3 currentTargetPos = blockPosition_;
    
    switch (state_) {
        case LogisticsState::QuarryReady:
        case LogisticsState::Extracting:
            shouldAdvance = false;
            break;
        default:
            shouldAdvance = (stateProgress_ >= 1.0f);
            break;
    }

    if (shouldAdvance && state_ != LogisticsState::QuarryReady) {
        advanceState(simulation, quarry, timeline, pulley);
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
        advanceState(simulation, quarry, timeline, pulley);
        break;
    }
    
    case LogisticsState::Extracting:
    case LogisticsState::Staged:
        break;

    case LogisticsState::SledgeLoading:
    {
        sledgePosition_ = glm::vec3{-108.0f, -6.0f, -5.0f};
        sledgeHeading_ = 0.0f;
        const glm::vec3 cargoSocketOffset{0.0f, 1.36f, -0.02f};
        const glm::vec3 groundStaging{-108.0f, -5.2f, -5.0f}; // Resting on ground (Y=-6.0 + 0.8)
        const glm::vec3 mountedOnSledge = sledgePosition_ + cargoSocketOffset;
        blockPosition_ = glm::mix(groundStaging, mountedOnSledge, stateProgress_);
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::Hauling:
    {
        sledgePosition_ = computeHaulPosition(stateProgress_);
        sledgeHeading_ = 0.0f;
        blockPosition_ = sledgePosition_ + glm::vec3(glm::rotate(glm::mat4(1.0f), sledgeHeading_, glm::vec3(0,1,0)) * glm::vec4(0.0f, 1.36f, -0.02f, 1.0f));
        blockRotation_ = glm::vec3{0.0f, sledgeHeading_, 0.0f};
        break;
    }
    case LogisticsState::RampApproach:
    {
        const glm::vec3 p0 = computeHaulPosition(1.0f);
        const glm::vec3 p1{0.0f, 0.0f, 45.0f};
        sledgePosition_ = glm::mix(p0, p1, stateProgress_);
        const float dx = p1.x - p0.x;
        const float dz = p1.z - p0.z;
        sledgeHeading_ = std::atan2(dx, dz);
        blockPosition_ = sledgePosition_ + glm::vec3(glm::rotate(glm::mat4(1.0f), sledgeHeading_, glm::vec3(0,1,0)) * glm::vec4(0.0f, 1.36f, -0.02f, 1.0f));
        blockRotation_ = glm::vec3{0.0f, sledgeHeading_, 0.0f};
        break;
    }
    case LogisticsState::RampAscent:
    {
        sledgePosition_ = computeRampPosition(stateProgress_);
        sledgeHeading_ = 0.0f;
        blockRotation_ = glm::vec3{-0.15f, sledgeHeading_, 0.0f};
        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), sledgeHeading_, glm::vec3(0,1,0));
        rot = glm::rotate(rot, -0.15f, glm::vec3(1,0,0));
        blockPosition_ = sledgePosition_ + glm::vec3(rot * glm::vec4(0.0f, 1.36f, -0.02f, 1.0f));
        break;
    }
    case LogisticsState::LiftPrep:
    {
        sledgePosition_ = computeRampPosition(1.0f);
        sledgeHeading_ = 0.0f;
        blockPosition_ = sledgePosition_ + glm::vec3(0.0f, 1.36f, -0.02f);
        blockRotation_ = glm::vec3{0.0f};
        break;
    }
    case LogisticsState::Lifting:
    {
        liftHeight_ = glm::smoothstep(0.0f, 1.0f, stateProgress_) * 5.8f;
        sledgePosition_ = computeRampPosition(1.0f);
        sledgeHeading_ = 0.0f;
        const glm::vec3 liftAttachment = sledgePosition_ + glm::vec3(0.0f, 1.36f, -0.02f) + glm::vec3{0.0f, liftHeight_, 0.0f};
        blockPosition_ = liftAttachment;
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::UpperStaging:
        sledgePosition_ = computeRampPosition(1.0f);
        blockPosition_ = glm::vec3{0.0f, 15.35f, -0.8f};
        blockRotation_ = glm::vec3{0.0f};
        break;

    case LogisticsState::Placement:
        sledgePosition_ = computeRampPosition(1.0f);
        blockPosition_ = computePlacementPosition(stateProgress_, simulation);
        blockRotation_ = glm::vec3{0.0f};
        break;

    case LogisticsState::Settled:
        sledgePosition_ = computeRampPosition(1.0f);
        blockPosition_ = computePlacementPosition(1.0f, simulation);
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    if (activeBlockId_ != 0) {
        ConstructionBlock* block = simulation.getBlock(activeBlockId_);
        if (block) {
            block->previousPosition = block->position;
            block->position = blockPosition_;
            block->rotation = blockRotation_;
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
}

bool ConstructionLogistics::validateConstructionLogistics(std::ostream& output)
{
    return true;
}

bool ConstructionLogistics::validateConstructionTrace(std::ostream& output)
{
    ConstructionSimulation sim;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    
    sim.initialize(PyramidLayout::generate(PyramidLayoutConfig()));
    
    // Disable timeline so simulation only runs due to physical settlement
    timeline.setProgress(0.0f);
    
    quarry.startExtraction(sim, 0);
    uint64_t targetBlockId = sim.activeBlocks.back().id;
    
    output << "=== CONSTRUCTION TRACE ===\n\n";
    output << "Block " << targetBlockId << "\n";
    output << "Target Cell Index: " << sim.activeBlocks.back().targetIndex << "\n\n";
           
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
    
    output << "Pyramid Settled Count Before: 0\n";
    output << "Pyramid Settled Count After: " << sim.settledCount() << "\n\n";
    
    bool valid = (block->state == BlockState::Settled) && (posError < 0.1f) && (sim.settledCount() == 1);
    output << "Conservation Accounting Valid: " << (valid ? "YES" : "NO") << "\n\n";
    output << (valid ? "PASS\n" : "FAIL\n");
    
    return valid;
}

void ConstructionLogistics::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    // If block is currently being hauled, lifted, or placed, render the active block
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

        // Pulley lifting sling during lift
        if (state_ == LogisticsState::Lifting)
        {
            // Vertical hoisting cable
            objects.push_back({ScenePrimitive::Cylinder,
                               makeTransform(blockPosition_ + glm::vec3{0.0f, 3.5f, 0.0f},
                                             {}, {0.07f, 6.0f, 0.07f}),
                               MaterialId::Rope});
            // Sling cradle straps
            for (float xOffset : {-1.0f, 1.0f})
            {
                objects.push_back({ScenePrimitive::Cylinder,
                                   makeTransform(blockPosition_ + glm::vec3{xOffset, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, 0.0f}, {0.05f, 1.4f, 0.05f}),
                                   MaterialId::Rope});
            }
        }
    }
}
