#include "scene/ConstructionSimulation.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

ConstructionSimulation::ConstructionSimulation() = default;

void ConstructionSimulation::initialize(const std::vector<PyramidBlockPlacement>& fullLayout)
{
    layout = fullLayout;
    targetCellsOccupied.assign(layout.size(), false);
    activeBlocks.clear();
    settledBlocks_ = 0;
    nextTargetIndex_ = 0;
    nextBlockId_ = 1000;
}

void ConstructionSimulation::reset()
{
    targetCellsOccupied.assign(layout.size(), false);
    activeBlocks.clear();
    settledBlocks_ = 0;
    nextTargetIndex_ = 0;
    nextBlockId_ = 1000;
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
    assignTarget(block);

    activeBlocks.push_back(block);
}

void ConstructionSimulation::assignTarget(ConstructionBlock& block)
{
    if (nextTargetIndex_ < layout.size())
    {
        block.targetIndex = nextTargetIndex_;
        block.targetPlacement = layout[nextTargetIndex_];
        nextTargetIndex_++;
    }
}

void ConstructionSimulation::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    for (const auto& block : activeBlocks)
    {
        if (block.state == BlockState::QuarryBedrock || block.state == BlockState::Settled)
            continue; // Quarry bedrock handled by deposit (unshaped), settled handled by frontier batches

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
