#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "animation/ConstructionTimeline.h"
#include "animation/QuarryPulleyAnimation.h"
#include "objects/Worker.h"
#include "scene/QuarrySystem.h"
#include "scene/SceneTypes.h"
#include "scene/ConstructionSimulation.h"

enum class LogisticsState
{
    QuarryReady,
    Extracting,
    Staged,
    SledgeLoading,
    Hauling,
    RampApproach,
    RampAscent,
    LiftPrep,
    Lifting,
    UpperStaging,
    Placement,
    Settled
};

struct LogisticsSnapshot
{
    LogisticsState state = LogisticsState::QuarryReady;
    int blockIndex = 1;
    float stateProgress = 0.0f;
    glm::vec3 blockPosition{0.0f};
    glm::vec3 blockRotation{0.0f};
    glm::vec3 sledgePosition{0.0f};
    float sledgeHeading = 0.0f; // Degrees; local -Z is the hauling direction.
    float sledgePitch = 0.0f;   // Degrees; follows the current support grade.
    bool ropeTaut = false;
    int activeWorkers = 0;
    float liftHeight = 0.0f;
    bool blockSettled = false;
    const char* routeDescription = "";
};

class ConstructionLogistics
{
public:
    static constexpr std::size_t pullingCrewSize() { return 2; }

    ConstructionLogistics();

    void update(float deltaTime, ConstructionSimulation& simulation,
                QuarrySystem& quarry,
                ConstructionTimelineController& timeline,
                QuarryPulleyAnimationController& pulley,
                bool physicalLiftEnabled = true);
    void reset();

    LogisticsSnapshot snapshot() const;
    uint64_t activeBlockId() const { return activeBlockId_; }
    LogisticsState state() const { return state_; }
    float stateProgress() const { return stateProgress_; }
    int activeBlockNumber() const { return activeBlockNumber_; }
    const char* routeDescription() const;

    // Physical-mode hauling geometry. These helpers keep the visible sledge,
    // pulling crew, evaluated hands, and tow ropes in one transport frame.
    static bool isPhysicalHaulingState(LogisticsState state);
    static glm::mat4 physicalSledgeRoot(const LogisticsSnapshot& snapshot);
    static glm::vec3 haulingDirection(const LogisticsSnapshot& snapshot);
    static float haulingSupportHeight(const LogisticsSnapshot& snapshot,
                                      const glm::vec2& worldPoint);
    static glm::mat4 physicalWorkerRoot(const LogisticsSnapshot& snapshot,
                                        std::size_t crewIndex,
                                        const WorkerJointAngles& angles);
    static glm::vec3 pullingHandPosition(const Worker::EvaluatedPose& pose,
                                         std::size_t crewIndex);
    static glm::vec3 physicalTowPoint(const LogisticsSnapshot& snapshot,
                                      std::size_t crewIndex);

    // Visual rendering of the active in-transit block, sledge lashings, and props
    void collectSceneObjects(std::vector<SceneObject>& objects) const;

    static const char* stateName(LogisticsState state);
    static bool validateConstructionLogistics(std::ostream& output);
    static bool validateConstructionTrace(std::ostream& output);

private:
    void advanceState(ConstructionSimulation& simulation, QuarrySystem& quarry, ConstructionTimelineController& timeline,
                      QuarryPulleyAnimationController& pulley,
                      bool physicalLiftEnabled);
    glm::vec3 computeHaulPosition(float progress) const;
    glm::vec3 computeRampPosition(float progress) const;
    glm::vec3 computePlacementPosition(float progress, ConstructionSimulation& simulation) const;

    LogisticsState state_ = LogisticsState::QuarryReady;
    uint64_t activeBlockId_ = 0;
    int activeBlockNumber_ = 101;
    float stateProgress_ = 0.0f;
    float stateTimer_ = 0.0f;
    glm::vec3 blockPosition_{-108.0f, -6.2f, -5.0f};
    glm::vec3 blockRotation_{0.0f};
    glm::vec3 sledgePosition_{-108.0f, -6.2f, -5.0f};
    float sledgeHeading_ = 0.0f;
    float sledgePitch_ = 0.0f;
    bool ropeTaut_ = false;
    int activeWorkers_ = 6;
    float liftHeight_ = 0.0f;
    bool justSettled_ = false;
    float settledFlashTimer_ = 0.0f;
};
