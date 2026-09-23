#pragma once

#include <array>
#include <cstddef>

#include "scene/SceneTypes.h"

enum class BodyPart
{
    Pelvis,
    Torso,
    Neck,
    Head,
    Headwear,
    LeftUpperArm,
    LeftForearm,
    LeftHand,
    RightUpperArm,
    RightForearm,
    RightHand,
    LeftThigh,
    LeftLowerLeg,
    LeftFoot,
    RightThigh,
    RightLowerLeg,
    RightFoot
};

enum class WorkerPose
{
    Standing,
    PullingReady,
    CarryingReady,
    LeverReady,
    ArmsOut,
    BentKnees
};

struct WorkerStyle
{
    MaterialId clothing = MaterialId::ClothingLinen;
    MaterialId headwear = MaterialId::Headwear;
};

struct WorkerJointAngles
{
    glm::vec3 pelvis{0.0f};
    glm::vec3 torso{0.0f};
    glm::vec3 neck{0.0f};
    glm::vec3 head{0.0f};
    glm::vec3 leftShoulder{0.0f};
    glm::vec3 leftElbow{0.0f};
    glm::vec3 leftWrist{0.0f};
    glm::vec3 rightShoulder{0.0f};
    glm::vec3 rightElbow{0.0f};
    glm::vec3 rightWrist{0.0f};
    glm::vec3 leftHip{0.0f};
    glm::vec3 leftKnee{0.0f};
    glm::vec3 leftAnkle{0.0f};
    glm::vec3 rightHip{0.0f};
    glm::vec3 rightKnee{0.0f};
    glm::vec3 rightAnkle{0.0f};
};

struct WorkerNode
{
    BodyPart id;
    int parentIndex;
    ScenePrimitive primitive;
    glm::vec3 jointOffset;
    glm::vec3 bindRotationDegrees;
    glm::vec3 shapeOffset;
    glm::vec3 shapeScale;
    MaterialId restMaterial;
};

struct WorkerPartTransform
{
    BodyPart id;
    ScenePrimitive primitive;
    glm::mat4 jointWorld{1.0f};
    glm::mat4 model{1.0f};
    MaterialId material = MaterialId::Skin;
};

struct WorkerJointLimit
{
    BodyPart joint;
    glm::vec3 minimumDegrees;
    glm::vec3 maximumDegrees;
};

class Worker
{
public:
    static constexpr std::size_t PartCount = 17;
    static constexpr std::size_t partCount() { return PartCount; }
    using Hierarchy = std::array<WorkerNode, PartCount>;
    using EvaluatedPose = std::array<WorkerPartTransform, PartCount>;

    static const Hierarchy& hierarchy();
    static const std::array<WorkerJointLimit, 16>& jointLimits();
    static WorkerJointAngles poseAngles(WorkerPose pose);
    static WorkerJointAngles animatedPreview(WorkerPose basePose, float elapsedTime);
    static WorkerJointAngles clampJointAngles(const WorkerJointAngles& angles);
    static bool jointAnglesWithinLimits(const WorkerJointAngles& angles);
    static EvaluatedPose evaluate(const glm::mat4& rootTransform,
                                  const WorkerJointAngles& angles,
                                  const WorkerStyle& style = {});

    static WorkerPose nextPose(WorkerPose pose);
    static constexpr float approximateHeight() { return 2.26f; }
    static const char* poseName(WorkerPose pose);
    static const char* bodyPartName(BodyPart part);
};
