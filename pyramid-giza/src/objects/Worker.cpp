#include "objects/Worker.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr int noParent = -1;

namespace dimensions
{
constexpr float pelvisHeight = 1.17f;
constexpr glm::vec3 pelvis{0.48f, 0.24f, 0.28f};
constexpr glm::vec3 torso{0.65f, 0.48f, 0.34f};
constexpr glm::vec3 neck{0.15f, 0.12f, 0.15f};
constexpr glm::vec3 head{0.32f, 0.30f, 0.32f};
constexpr glm::vec3 headwear{0.38f, 0.08f, 0.38f};
constexpr glm::vec3 upperArm{0.17f, 0.47f, 0.17f};
constexpr glm::vec3 forearm{0.15f, 0.38f, 0.15f};
constexpr glm::vec3 hand{0.17f, 0.17f, 0.17f};
constexpr glm::vec3 thigh{0.20f, 0.52f, 0.20f};
constexpr glm::vec3 lowerLeg{0.17f, 0.45f, 0.17f};
constexpr glm::vec3 foot{0.28f, 0.20f, 0.46f};
} // namespace dimensions

glm::mat4 rotationDegrees(const glm::vec3& degrees)
{
    glm::mat4 rotation{1.0f};
    rotation = glm::rotate(rotation, glm::radians(degrees.y), {0.0f, 1.0f, 0.0f});
    rotation = glm::rotate(rotation, glm::radians(degrees.x), {1.0f, 0.0f, 0.0f});
    return glm::rotate(rotation, glm::radians(degrees.z), {0.0f, 0.0f, 1.0f});
}

glm::vec3& rotationFor(WorkerJointAngles& angles, BodyPart part)
{
    switch (part)
    {
    case BodyPart::Pelvis: return angles.pelvis;
    case BodyPart::Torso: return angles.torso;
    case BodyPart::Neck: return angles.neck;
    case BodyPart::Head: return angles.head;
    case BodyPart::LeftUpperArm: return angles.leftShoulder;
    case BodyPart::LeftForearm: return angles.leftElbow;
    case BodyPart::LeftHand: return angles.leftWrist;
    case BodyPart::RightUpperArm: return angles.rightShoulder;
    case BodyPart::RightForearm: return angles.rightElbow;
    case BodyPart::RightHand: return angles.rightWrist;
    case BodyPart::LeftThigh: return angles.leftHip;
    case BodyPart::LeftLowerLeg: return angles.leftKnee;
    case BodyPart::LeftFoot: return angles.leftAnkle;
    case BodyPart::RightThigh: return angles.rightHip;
    case BodyPart::RightLowerLeg: return angles.rightKnee;
    case BodyPart::RightFoot: return angles.rightAnkle;
    case BodyPart::Headwear:
    default: throw std::out_of_range("Body part does not own a joint rotation");
    }
}

const glm::vec3& rotationFor(const WorkerJointAngles& angles, BodyPart part)
{
    switch (part)
    {
    case BodyPart::Pelvis: return angles.pelvis;
    case BodyPart::Torso: return angles.torso;
    case BodyPart::Neck: return angles.neck;
    case BodyPart::Head: return angles.head;
    case BodyPart::LeftUpperArm: return angles.leftShoulder;
    case BodyPart::LeftForearm: return angles.leftElbow;
    case BodyPart::LeftHand: return angles.leftWrist;
    case BodyPart::RightUpperArm: return angles.rightShoulder;
    case BodyPart::RightForearm: return angles.rightElbow;
    case BodyPart::RightHand: return angles.rightWrist;
    case BodyPart::LeftThigh: return angles.leftHip;
    case BodyPart::LeftLowerLeg: return angles.leftKnee;
    case BodyPart::LeftFoot: return angles.leftAnkle;
    case BodyPart::RightThigh: return angles.rightHip;
    case BodyPart::RightLowerLeg: return angles.rightKnee;
    case BodyPart::RightFoot: return angles.rightAnkle;
    case BodyPart::Headwear:
    default: throw std::out_of_range("Body part does not own a joint rotation");
    }
}

MaterialId materialFor(const WorkerNode& node, const WorkerStyle& style)
{
    if (node.id == BodyPart::Pelvis || node.id == BodyPart::Torso)
        return style.clothing;
    if (node.id == BodyPart::Headwear)
        return style.headwear;
    return node.restMaterial;
}
} // namespace

const Worker::Hierarchy& Worker::hierarchy()
{
    // Parent indices always precede children, enabling deterministic iterative traversal.
    static const Hierarchy nodes{{
        {BodyPart::Pelvis, noParent, ScenePrimitive::Cube,
         {0.0f, dimensions::pelvisHeight, 0.0f}, {}, {}, dimensions::pelvis, MaterialId::ClothingLinen},
        {BodyPart::Torso, 0, ScenePrimitive::Cube,
         {0.0f, 0.12f, 0.0f}, {}, {0.0f, 0.24f, 0.0f}, dimensions::torso, MaterialId::ClothingLinen},
        {BodyPart::Neck, 1, ScenePrimitive::Cylinder,
         {0.0f, 0.48f, 0.0f}, {}, {0.0f, 0.06f, 0.0f}, dimensions::neck, MaterialId::Skin},
        {BodyPart::Head, 2, ScenePrimitive::Sphere,
         {0.0f, 0.12f, 0.0f}, {}, {0.0f, 0.15f, 0.0f}, dimensions::head, MaterialId::Skin},
        {BodyPart::Headwear, 3, ScenePrimitive::Cylinder,
         {0.0f, 0.29f, 0.0f}, {}, {0.0f, 0.04f, 0.0f}, dimensions::headwear, MaterialId::Headwear},
        {BodyPart::LeftUpperArm, 1, ScenePrimitive::Cylinder,
         {-0.36f, 0.40f, 0.0f}, {}, {0.0f, -0.235f, 0.0f}, dimensions::upperArm, MaterialId::Skin},
        {BodyPart::LeftForearm, 5, ScenePrimitive::Cylinder,
         {0.0f, -0.47f, 0.0f}, {}, {0.0f, -0.19f, 0.0f}, dimensions::forearm, MaterialId::Skin},
        {BodyPart::LeftHand, 6, ScenePrimitive::Sphere,
         {0.0f, -0.38f, 0.0f}, {}, {0.0f, -0.055f, 0.0f}, dimensions::hand, MaterialId::Skin},
        {BodyPart::RightUpperArm, 1, ScenePrimitive::Cylinder,
         {0.36f, 0.40f, 0.0f}, {}, {0.0f, -0.235f, 0.0f}, dimensions::upperArm, MaterialId::Skin},
        {BodyPart::RightForearm, 8, ScenePrimitive::Cylinder,
         {0.0f, -0.47f, 0.0f}, {}, {0.0f, -0.19f, 0.0f}, dimensions::forearm, MaterialId::Skin},
        {BodyPart::RightHand, 9, ScenePrimitive::Sphere,
         {0.0f, -0.38f, 0.0f}, {}, {0.0f, -0.055f, 0.0f}, dimensions::hand, MaterialId::Skin},
        {BodyPart::LeftThigh, 0, ScenePrimitive::Cylinder,
         {-0.16f, 0.0f, 0.0f}, {}, {0.0f, -0.26f, 0.0f}, dimensions::thigh, MaterialId::Skin},
        {BodyPart::LeftLowerLeg, 11, ScenePrimitive::Cylinder,
         {0.0f, -0.52f, 0.0f}, {}, {0.0f, -0.225f, 0.0f}, dimensions::lowerLeg, MaterialId::Skin},
        {BodyPart::LeftFoot, 12, ScenePrimitive::Cube,
         {0.0f, -0.45f, 0.0f}, {}, {0.0f, -0.10f, -0.11f}, dimensions::foot, MaterialId::Skin},
        {BodyPart::RightThigh, 0, ScenePrimitive::Cylinder,
         {0.16f, 0.0f, 0.0f}, {}, {0.0f, -0.26f, 0.0f}, dimensions::thigh, MaterialId::Skin},
        {BodyPart::RightLowerLeg, 14, ScenePrimitive::Cylinder,
         {0.0f, -0.52f, 0.0f}, {}, {0.0f, -0.225f, 0.0f}, dimensions::lowerLeg, MaterialId::Skin},
        {BodyPart::RightFoot, 15, ScenePrimitive::Cube,
         {0.0f, -0.45f, 0.0f}, {}, {0.0f, -0.10f, -0.11f}, dimensions::foot, MaterialId::Skin}
    }};
    return nodes;
}

const std::array<WorkerJointLimit, 16>& Worker::jointLimits()
{
    static const std::array<WorkerJointLimit, 16> limits{{
        {BodyPart::Pelvis, {-20.0f, -45.0f, -20.0f}, {20.0f, 45.0f, 20.0f}},
        {BodyPart::Torso, {-35.0f, -45.0f, -25.0f}, {35.0f, 45.0f, 25.0f}},
        {BodyPart::Neck, {-25.0f, -45.0f, -20.0f}, {25.0f, 45.0f, 20.0f}},
        {BodyPart::Head, {-35.0f, -70.0f, -30.0f}, {35.0f, 70.0f, 30.0f}},
        {BodyPart::LeftUpperArm, {-120.0f, -90.0f, -170.0f}, {150.0f, 90.0f, 90.0f}},
        {BodyPart::LeftForearm, {0.0f, -10.0f, -10.0f}, {145.0f, 10.0f, 10.0f}},
        {BodyPart::LeftHand, {-45.0f, -45.0f, -35.0f}, {45.0f, 45.0f, 35.0f}},
        {BodyPart::RightUpperArm, {-120.0f, -90.0f, -90.0f}, {150.0f, 90.0f, 170.0f}},
        {BodyPart::RightForearm, {0.0f, -10.0f, -10.0f}, {145.0f, 10.0f, 10.0f}},
        {BodyPart::RightHand, {-45.0f, -45.0f, -35.0f}, {45.0f, 45.0f, 35.0f}},
        {BodyPart::LeftThigh, {-90.0f, -45.0f, -35.0f}, {60.0f, 45.0f, 35.0f}},
        {BodyPart::LeftLowerLeg, {0.0f, -5.0f, -5.0f}, {130.0f, 5.0f, 5.0f}},
        {BodyPart::LeftFoot, {-45.0f, -25.0f, -20.0f}, {45.0f, 25.0f, 20.0f}},
        {BodyPart::RightThigh, {-90.0f, -45.0f, -35.0f}, {60.0f, 45.0f, 35.0f}},
        {BodyPart::RightLowerLeg, {0.0f, -5.0f, -5.0f}, {130.0f, 5.0f, 5.0f}},
        {BodyPart::RightFoot, {-45.0f, -25.0f, -20.0f}, {45.0f, 25.0f, 20.0f}}
    }};
    return limits;
}

WorkerJointAngles Worker::poseAngles(WorkerPose pose)
{
    WorkerJointAngles angles;
    switch (pose)
    {
    case WorkerPose::Standing:
        break;
    case WorkerPose::PullingReady:
        angles.torso.x = 10.0f;
        angles.leftShoulder = {62.0f, 0.0f, -8.0f};
        angles.rightShoulder = {62.0f, 0.0f, 8.0f};
        angles.leftElbow.x = 32.0f;
        angles.rightElbow.x = 32.0f;
        angles.leftHip.x = -8.0f;
        angles.rightHip.x = 8.0f;
        angles.leftKnee.x = 16.0f;
        break;
    case WorkerPose::CarryingReady:
        angles.leftShoulder = {48.0f, 0.0f, -42.0f};
        angles.rightShoulder = {48.0f, 0.0f, 42.0f};
        angles.leftElbow.x = 62.0f;
        angles.rightElbow.x = 62.0f;
        break;
    case WorkerPose::LeverReady:
        angles.torso = {12.0f, 8.0f, 0.0f};
        angles.leftShoulder = {68.0f, 0.0f, -12.0f};
        angles.leftElbow.x = 34.0f;
        angles.rightShoulder = {34.0f, 0.0f, 20.0f};
        angles.rightElbow.x = 58.0f;
        angles.leftKnee.x = 18.0f;
        angles.rightKnee.x = 12.0f;
        break;
    case WorkerPose::ArmsOut:
        angles.leftShoulder.z = -90.0f;
        angles.rightShoulder.z = 90.0f;
        break;
    case WorkerPose::BentKnees:
        angles.torso.x = -8.0f;
        angles.leftHip.x = -24.0f;
        angles.rightHip.x = -24.0f;
        angles.leftKnee.x = 52.0f;
        angles.rightKnee.x = 52.0f;
        angles.leftAnkle.x = -24.0f;
        angles.rightAnkle.x = -24.0f;
        break;
    default:
        throw std::out_of_range("Unknown worker pose");
    }
    return clampJointAngles(angles);
}

WorkerJointAngles Worker::animatedPreview(WorkerPose basePose, float elapsedTime)
{
    WorkerJointAngles angles = poseAngles(basePose);
    const float wave = std::sin(elapsedTime * 1.35f);
    const float counterWave = std::sin(elapsedTime * 1.35f + 1.5707963f);
    angles.head.y += 18.0f * wave;
    angles.leftShoulder.z -= 28.0f + 24.0f * wave;
    angles.leftElbow.x += 38.0f + 22.0f * counterWave;
    angles.leftKnee.x += 10.0f + 8.0f * counterWave;
    return clampJointAngles(angles);
}

WorkerJointAngles Worker::clampJointAngles(const WorkerJointAngles& input)
{
    WorkerJointAngles result = input;
    for (const WorkerJointLimit& limit : jointLimits())
    {
        glm::vec3& rotation = rotationFor(result, limit.joint);
        rotation = glm::clamp(rotation, limit.minimumDegrees, limit.maximumDegrees);
    }
    return result;
}

bool Worker::jointAnglesWithinLimits(const WorkerJointAngles& angles)
{
    for (const WorkerJointLimit& limit : jointLimits())
    {
        const glm::vec3& rotation = rotationFor(angles, limit.joint);
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(rotation[axis]) || rotation[axis] < limit.minimumDegrees[axis] ||
                rotation[axis] > limit.maximumDegrees[axis])
                return false;
    }
    return true;
}

Worker::EvaluatedPose Worker::evaluate(const glm::mat4& rootTransform,
                                       const WorkerJointAngles& requestedAngles,
                                       const WorkerStyle& style)
{
    if (!isFiniteNonSingularTransform(rootTransform))
        throw std::invalid_argument("Worker root transform must be finite and non-singular");

    const WorkerJointAngles angles = clampJointAngles(requestedAngles);
    const Hierarchy& nodes = hierarchy();
    EvaluatedPose evaluated{};
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        const WorkerNode& node = nodes[index];
        if (node.parentIndex >= static_cast<int>(index))
            throw std::logic_error("Worker hierarchy must be stored in parent-first order");

        const glm::mat4 parentJoint = node.parentIndex == noParent
                                          ? rootTransform
                                          : evaluated[static_cast<std::size_t>(node.parentIndex)].jointWorld;
        glm::mat4 jointWorld = glm::translate(parentJoint, node.jointOffset);
        jointWorld *= rotationDegrees(node.bindRotationDegrees);
        if (node.id != BodyPart::Headwear)
            jointWorld *= rotationDegrees(rotationFor(angles, node.id));

        glm::mat4 model = glm::translate(jointWorld, node.shapeOffset);
        model = glm::scale(model, node.shapeScale);
        if (!isFiniteNonSingularTransform(jointWorld) || !isFiniteNonSingularTransform(model))
            throw std::runtime_error("Worker hierarchy produced an invalid transform");
        evaluated[index] = {node.id, node.primitive, jointWorld, model, materialFor(node, style)};
    }
    return evaluated;
}

WorkerPose Worker::nextPose(WorkerPose pose)
{
    switch (pose)
    {
    case WorkerPose::Standing: return WorkerPose::PullingReady;
    case WorkerPose::PullingReady: return WorkerPose::CarryingReady;
    case WorkerPose::CarryingReady: return WorkerPose::LeverReady;
    case WorkerPose::LeverReady: return WorkerPose::ArmsOut;
    case WorkerPose::ArmsOut: return WorkerPose::BentKnees;
    case WorkerPose::BentKnees: return WorkerPose::Standing;
    default: throw std::out_of_range("Unknown worker pose");
    }
}

const char* Worker::poseName(WorkerPose pose)
{
    switch (pose)
    {
    case WorkerPose::Standing: return "Standing";
    case WorkerPose::PullingReady: return "PullingReady";
    case WorkerPose::CarryingReady: return "CarryingReady";
    case WorkerPose::LeverReady: return "LeverReady";
    case WorkerPose::ArmsOut: return "ArmsOut";
    case WorkerPose::BentKnees: return "BentKnees";
    default: throw std::out_of_range("Unknown worker pose");
    }
}

const char* Worker::bodyPartName(BodyPart part)
{
    switch (part)
    {
    case BodyPart::Pelvis: return "Pelvis";
    case BodyPart::Torso: return "Torso";
    case BodyPart::Neck: return "Neck";
    case BodyPart::Head: return "Head";
    case BodyPart::Headwear: return "Headwear";
    case BodyPart::LeftUpperArm: return "LeftUpperArm";
    case BodyPart::LeftForearm: return "LeftForearm";
    case BodyPart::LeftHand: return "LeftHand";
    case BodyPart::RightUpperArm: return "RightUpperArm";
    case BodyPart::RightForearm: return "RightForearm";
    case BodyPart::RightHand: return "RightHand";
    case BodyPart::LeftThigh: return "LeftThigh";
    case BodyPart::LeftLowerLeg: return "LeftLowerLeg";
    case BodyPart::LeftFoot: return "LeftFoot";
    case BodyPart::RightThigh: return "RightThigh";
    case BodyPart::RightLowerLeg: return "RightLowerLeg";
    case BodyPart::RightFoot: return "RightFoot";
    default: throw std::out_of_range("Unknown worker body part");
    }
}
