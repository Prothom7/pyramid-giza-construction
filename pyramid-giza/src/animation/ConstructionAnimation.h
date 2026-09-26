#pragma once

#include <array>

#include <glm/glm.hpp>

#include "objects/Worker.h"

enum class ConstructionState
{
    Idle,
    WorkersApproach,
    PullReady,
    PullGround,
    ApproachRamp,
    RampPull,
    Arrival,
    LeverPreparation,
    PlacementReady,
    Complete
};

enum class WorkerRole
{
    PullerLeft,
    PullerRight,
    QuarryMallet,
    QuarryCarrier,
    RampGuide,
    LeverOperator,
    StockpileWorker,
    Count
};

struct AnimatedWorkerState
{
    glm::mat4 root{1.0f};
    WorkerJointAngles joints;
};

struct TransportFrame
{
    glm::vec3 sledgePosition{0.0f};
    glm::vec3 leftWorkerPosition{0.0f};
    glm::vec3 rightWorkerPosition{0.0f};
    float headingDegrees = 0.0f;
    float rampPitchDegrees = 0.0f;
};

struct ConstructionAnimationSnapshot
{
    ConstructionState state = ConstructionState::Idle;
    float stateProgress = 0.0f;
    float transportProgress = 0.0f;
    std::array<AnimatedWorkerState, static_cast<std::size_t>(WorkerRole::Count)> workers{};
    glm::mat4 loadedSledgeRoot{1.0f};
    float leverAngleDegrees = 0.0f;
    float liftedStoneOffset = 0.0f;
    bool ropeVisible = false;
};

class ConstructionAnimationController
{
public:
    void update(float deltaTime);
    void reset();
    void togglePaused() { paused_ = !paused_; }
    void toggleLooping() { looping_ = !looping_; }
    void advanceState();
    void setSpeed(float speed);
    void setLooping(bool looping) { looping_ = looping; }
    void seek(float elapsedTime, bool playing);

    ConstructionAnimationSnapshot snapshot() const;

    ConstructionState state() const { return state_; }
    float stateTime() const { return stateTime_; }
    float elapsedTime() const { return elapsedTime_; }
    float speed() const { return speed_; }
    bool paused() const { return paused_; }
    bool looping() const { return looping_; }

    static const char* stateName(ConstructionState state);
    static float stateDuration(ConstructionState state);
    static float sequenceDuration();
    static float rampSurfaceHeight(float z);
    static TransportFrame transportAt(float progress);
    static WorkerJointAngles blendPoses(const WorkerJointAngles& from,
                                        const WorkerJointAngles& to, float amount);
    static WorkerJointAngles walkingPose(const WorkerJointAngles& base,
                                         float time, float strength, bool pulling);
    static glm::mat4 cylinderBetween(const glm::vec3& start, const glm::vec3& end,
                                     float diameter);
    static glm::mat4 toolAttachmentRoot(const glm::mat4& handJointWorld);
    static glm::mat4 leverRoot();
    static glm::mat4 leverPivotFrame(float angleDegrees);
    static glm::mat4 leverBeamModel(float angleDegrees);
    static glm::mat4 leverStoneModel(float liftOffset);

private:
    static ConstructionState nextState(ConstructionState state);
    void finishState();

    ConstructionState state_ = ConstructionState::Idle;
    float stateTime_ = 0.0f;
    float elapsedTime_ = 0.0f;
    float speed_ = 1.0f;
    bool paused_ = false;
    bool looping_ = true;
};
