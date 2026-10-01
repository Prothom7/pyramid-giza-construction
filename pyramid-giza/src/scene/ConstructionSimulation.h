#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include "scene/PyramidLayout.h"
#include "scene/SceneTypes.h"
#include <vector>

enum class BlockState
{
    QuarryBedrock,
    Extracting,
    Separating,
    Shaping,
    Staged,
    LoadedOnSledge,
    Hauling,
    RampApproach,
    RampAscent,
    LiftPrep,
    Lifting,
    UpperStaging,
    Placement,
    Settling,
    Settled
};

struct ConstructionBlock
{
    uint64_t id = 0;
    BlockState state = BlockState::QuarryBedrock;

    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 rotation{0.0f};

    int sledgeId = -1;
    PyramidBlockPlacement targetPlacement;
    std::size_t targetIndex = 0;

    float taskProgress = 0.0f; // Generic progress for current state

    bool isExtracted = false;
    bool isShaped = false;
    bool isLoaded = false;
    bool isSettled = false;
};

class ConstructionSimulation
{
public:
    ConstructionSimulation();

    void initialize(const std::vector<PyramidBlockPlacement>& fullLayout);
    void update(float deltaTime);
    void reset();

    // Central authoritative data
    std::vector<ConstructionBlock> activeBlocks;
    
    void collectSceneObjects(std::vector<SceneObject>& objects) const;
    std::vector<bool> targetCellsOccupied; // Indexed by block generation index
    std::vector<PyramidBlockPlacement> layout;

    std::size_t settledCount() const { return settledBlocks_; }
    std::size_t totalCount() const { return targetCellsOccupied.size(); }
    void incrementSettledCount() { settledBlocks_++; }

    // Helpers
    ConstructionBlock* getBlock(uint64_t id);
    void spawnNewBlock(const glm::vec3& quarryPos);
    
    // Assigns an unassigned target to a block
    void assignTarget(ConstructionBlock& block);

private:
    std::size_t settledBlocks_ = 0;
    std::size_t nextTargetIndex_ = 0;
    uint64_t nextBlockId_ = 1000;
};
