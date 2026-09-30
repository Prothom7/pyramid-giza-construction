#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/SceneTypes.h"

enum class RockProfileType
{
    SmallAngularLimestone,
    MediumRoughLimestone,
    LargeQuarryStone,
    PartiallyShapedBlock,
    FinishedConstructionBlock
};

enum class QuarryDepositState
{
    Natural,
    Marked,
    Cutting,
    Detached,
    Shaped,
    Staged,
    Transported,
    Depleted
};

struct QuarryDeposit
{
    int id = 0;
    glm::vec3 position{0.0f};
    glm::vec3 size{2.6f, 1.4f, 2.5f};
    RockProfileType profile = RockProfileType::LargeQuarryStone;
    MaterialId material = MaterialId::QuarryStone;
    QuarryDepositState state = QuarryDepositState::Natural;
    float initialVolume = 9.1f;
    float remainingVolume = 9.1f;
    float extractionProgress = 0.0f; // 0.0 to 1.0 during cutting
    float shapingProgress = 0.0f;    // 0.0 to 1.0 during dressing
    float separationOffset = 0.0f;   // Visual shift upon detachment
    float seamWidth = 0.0f;          // Channel cut trench width (meters)
    float vibration = 0.0f;          // Visual mallet oscillation
    glm::vec3 stagedPosition{0.0f};  // Location at quarry staging floor
};

struct QuarryToolActivity
{
    glm::vec3 chiselPoint{0.0f};
    glm::vec3 malletWorkerPos{0.0f};
    glm::vec3 masonWorkerPos{0.0f};
    glm::vec3 inspectorPos{0.0f};
    bool isHammering = false;
    bool emitDust = false;
};

class QuarrySystem
{
public:
    QuarrySystem();

    void update(float deltaTime);
    void reset();

    // Query active state
    int activeDepositId() const { return activeDepositIndex_ >= 0 ? deposits_[activeDepositIndex_].id : -1; }
    const QuarryDeposit* activeDeposit() const;
    QuarryDeposit* activeDeposit();
    const std::vector<QuarryDeposit>& deposits() const { return deposits_; }
    QuarryToolActivity toolActivity() const;

    // Simulation control
    void startExtraction(int depositIndex = -1);
    bool isCurrentDepositStaged() const;
    void markCurrentDepositTransported();

    // Visual geometry generation
    void collectSceneObjects(std::vector<SceneObject>& objects) const;

    // Validation
    static bool validateQuarry(std::ostream& output);

    static const char* stateName(QuarryDepositState state);
    static const char* profileName(RockProfileType profile);

private:
    void initDeposits();
    void updateExtraction(float deltaTime);

    std::vector<QuarryDeposit> deposits_;
    int activeDepositIndex_ = 0;
    float cycleTimer_ = 0.0f;
    float dustTimer_ = 0.0f;
    bool emitDustNow_ = false;
};
