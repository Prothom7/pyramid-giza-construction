#include "scene/ConstructionSimulation.h"
#include <algorithm>
#include <ostream>
#include <glm/gtc/matrix_transform.hpp>

ConstructionSimulation::ConstructionSimulation() = default;

void ConstructionSimulation::initialize(const std::vector<PyramidBlockPlacement>& fullLayout)
{
    layout = fullLayout;
    targetCellsOccupied_.assign(layout.size(), false);
    activeBlocks.clear();
    settledBlocks_ = 0;
    nextTargetIndex_ = 0;
    nextBlockId_ = 1000;
    ++occupancyRevision_;
}

void ConstructionSimulation::reset()
{
    targetCellsOccupied_.assign(layout.size(), false);
    activeBlocks.clear();
    settledBlocks_ = 0;
    nextTargetIndex_ = 0;
    nextBlockId_ = 1000;
    ++occupancyRevision_;
}

void ConstructionSimulation::update(float deltaTime)
{
    // Update individual block generic physics / simulation here if needed
}

ConstructionBlock* ConstructionSimulation::getBlock(uint64_t id)
{
    for (auto& block : activeBlocks)
    {
        if (block.id == id) return &block;
    }
    return nullptr;
}

void ConstructionSimulation::spawnNewBlock(const glm::vec3& quarryPos)
{
    ConstructionBlock block;
    block.id = nextBlockId_++;
    block.state = BlockState::QuarryBedrock;
    block.position = quarryPos;
    block.previousPosition = quarryPos;
    
    // Assign a target immediately upon creation so we know where it's going
    if (!assignTarget(block))
        return;

    activeBlocks.push_back(block);
}

bool ConstructionSimulation::assignTarget(ConstructionBlock& block)
{
    while (nextTargetIndex_ < layout.size() &&
           targetCellsOccupied_[nextTargetIndex_])
        ++nextTargetIndex_;
    if (nextTargetIndex_ >= layout.size())
        return false;

    block.targetIndex = nextTargetIndex_;
    block.targetPlacement = layout[nextTargetIndex_];
    ++nextTargetIndex_;
    return true;
}

bool ConstructionSimulation::isTargetOccupied(std::size_t index) const
{
    return index < targetCellsOccupied_.size() && targetCellsOccupied_[index];
}

bool ConstructionSimulation::settleBlock(uint64_t blockId)
{
    ConstructionBlock* block = getBlock(blockId);
    if (block == nullptr || block->targetIndex >= targetCellsOccupied_.size() ||
        targetCellsOccupied_[block->targetIndex])
        return false;

    targetCellsOccupied_[block->targetIndex] = true;
    ++settledBlocks_;
    ++occupancyRevision_;
    block->state = BlockState::Settled;
    block->position = block->targetPlacement.position;
    block->previousPosition = block->position;
    block->taskProgress = 1.0f;
    block->isSettled = true;
    return true;
}

void ConstructionSimulation::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    for (const auto& block : activeBlocks)
    {
        if (block.state == BlockState::QuarryBedrock || block.state == BlockState::Settled)
            continue; // Bedrock is a quarry deposit; settled blocks use occupied target batches.

        MaterialId material = MaterialId::PreparedStone;
        if (block.state == BlockState::Extracting || block.state == BlockState::Separating) {
            material = MaterialId::QuarryStone;
        } else if (block.state == BlockState::Shaping) {
            material = (block.taskProgress > 0.5f) ? MaterialId::LimestoneVariation : MaterialId::QuarryStone;
        } else {
            material = MaterialId::PreparedStone;
        }

        // Apply scale/transform
        glm::mat4 model = glm::translate(glm::mat4(1.0f), block.position);
        
        // Handle rotation from Euler angles (if any)
        if (glm::length(block.rotation) > 0.001f) {
            model = glm::rotate(model, block.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, block.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, block.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // Apply scale (approximate standard block size: length 2.6, height 1.6, width 2.4)
        glm::mat4 scaleOnly = glm::scale(glm::mat4(1.0f), glm::vec3(2.6f, 1.6f, 2.4f));

        objects.push_back({ScenePrimitive::Cube, model * scaleOnly, material});
    }
}

bool validateConstructionOccupancy(std::ostream& output)
{
    ConstructionSimulation simulation;
    simulation.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));

    const bool initiallyEmpty = simulation.occupiedTargetCount() == 0;
    simulation.spawnNewBlock({-143.0f, -6.75f, -22.0f});
    ConstructionBlock* first = simulation.activeBlocks.empty()
                                   ? nullptr
                                   : &simulation.activeBlocks.back();
    const std::size_t firstTarget = first == nullptr
                                        ? simulation.totalCount()
                                        : first->targetIndex;
    const std::size_t unrelatedTarget = firstTarget + 1;
    const bool firstWasEmpty = !simulation.isTargetOccupied(firstTarget);
    const bool firstSettled = first != nullptr && simulation.settleBlock(first->id);
    const bool exactFirstOccupied = simulation.isTargetOccupied(firstTarget);
    const bool unrelatedStillEmpty =
        unrelatedTarget < simulation.totalCount() &&
        !simulation.isTargetOccupied(unrelatedTarget);
    const bool duplicateRejected =
        first != nullptr && !simulation.settleBlock(first->id);

    simulation.spawnNewBlock({-143.0f, -6.75f, -22.0f});
    ConstructionBlock* second = simulation.activeBlocks.empty()
                                    ? nullptr
                                    : &simulation.activeBlocks.back();
    const bool distinctTarget = second != nullptr &&
                                second->targetIndex != firstTarget;
    const bool secondSettled = second != nullptr &&
                               simulation.settleBlock(second->id);
    const bool twoOccupied = simulation.occupiedTargetCount() == 2 &&
                             simulation.settledCount() == 2;

    simulation.spawnNewBlock({-143.0f, -6.75f, -22.0f});
    ConstructionBlock* invalid = simulation.activeBlocks.empty()
                                     ? nullptr
                                     : &simulation.activeBlocks.back();
    if (invalid != nullptr)
        invalid->targetIndex = simulation.totalCount();
    const bool invalidRejected = invalid != nullptr &&
                                 !simulation.settleBlock(invalid->id);

    simulation.reset();
    const bool resetValid = simulation.occupiedTargetCount() == 0 &&
                            simulation.settledCount() == 0 &&
                            simulation.activeBlocks.empty() &&
                            !simulation.isTargetOccupied(firstTarget);

    const bool valid = initiallyEmpty && firstWasEmpty && firstSettled &&
                       exactFirstOccupied && unrelatedStillEmpty &&
                       duplicateRejected && distinctTarget && secondSettled &&
                       twoOccupied && invalidRejected && resetValid;
    output << "Phase 13 physical target occupancy validation\n"
           << "  initial occupied count is zero: "
           << (initiallyEmpty ? "PASS" : "FAIL") << '\n'
           << "  exact first target occupied only: "
           << (firstSettled && exactFirstOccupied && unrelatedStillEmpty
                   ? "PASS" : "FAIL") << '\n'
           << "  duplicate and invalid targets rejected: "
           << (duplicateRejected && invalidRejected ? "PASS" : "FAIL") << '\n'
           << "  second target raises occupied count to two: "
           << (distinctTarget && secondSettled && twoOccupied ? "PASS" : "FAIL")
           << '\n'
           << "  reset clears occupancy and active blocks: "
           << (resetValid ? "PASS" : "FAIL") << '\n';
    return valid;
}
