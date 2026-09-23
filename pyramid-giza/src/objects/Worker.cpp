#include "objects/Worker.h"

#include <stdexcept>

namespace
{
namespace dimensions
{
constexpr glm::vec3 pelvis{0.48f, 0.24f, 0.28f};
constexpr glm::vec3 torso{0.65f, 0.62f, 0.34f};
constexpr glm::vec3 neck{0.15f, 0.16f, 0.15f};
constexpr glm::vec3 head{0.36f, 0.40f, 0.36f};
constexpr glm::vec3 headwear{0.42f, 0.11f, 0.42f};
constexpr glm::vec3 upperArm{0.17f, 0.47f, 0.17f};
constexpr glm::vec3 forearm{0.15f, 0.38f, 0.15f};
constexpr glm::vec3 hand{0.17f, 0.17f, 0.17f};
constexpr glm::vec3 thigh{0.20f, 0.52f, 0.20f};
constexpr glm::vec3 lowerLeg{0.17f, 0.45f, 0.17f};
constexpr glm::vec3 foot{0.28f, 0.20f, 0.46f};
} // namespace dimensions

void add(std::vector<ObjectPart>& parts, BodyPart part, ScenePrimitive primitive,
         const glm::vec3& position, const glm::vec3& rotation,
         const glm::vec3& scale, MaterialId material, const char* parent)
{
    parts.push_back({Worker::bodyPartName(part), primitive,
                     makeTransform(position, rotation, scale), material, parent});
}

struct ArmPose
{
    glm::vec3 leftUpperPosition;
    glm::vec3 leftUpperRotation;
    glm::vec3 leftForePosition;
    glm::vec3 leftForeRotation;
    glm::vec3 leftHandPosition;
    glm::vec3 rightUpperPosition;
    glm::vec3 rightUpperRotation;
    glm::vec3 rightForePosition;
    glm::vec3 rightForeRotation;
    glm::vec3 rightHandPosition;
};

ArmPose armPose(WorkerPose pose)
{
    switch (pose)
    {
    case WorkerPose::PullingReady:
        return {{-0.34f, 1.48f, -0.19f}, {-52.0f, 0.0f, -7.0f},
                {-0.31f, 1.18f, -0.56f}, {-67.0f, 0.0f, 0.0f},
                {-0.31f, 1.03f, -0.80f},
                {0.34f, 1.48f, -0.19f}, {-52.0f, 0.0f, 7.0f},
                {0.31f, 1.18f, -0.56f}, {-67.0f, 0.0f, 0.0f},
                {0.31f, 1.03f, -0.80f}};
    case WorkerPose::CarryingReady:
        return {{-0.43f, 1.48f, 0.0f}, {0.0f, 0.0f, -28.0f},
                {-0.59f, 1.27f, -0.18f}, {-48.0f, 0.0f, -36.0f},
                {-0.71f, 1.12f, -0.36f},
                {0.43f, 1.48f, 0.0f}, {0.0f, 0.0f, 28.0f},
                {0.59f, 1.27f, -0.18f}, {-48.0f, 0.0f, 36.0f},
                {0.71f, 1.12f, -0.36f}};
    case WorkerPose::LeverReady:
        return {{-0.38f, 1.47f, -0.12f}, {-38.0f, 0.0f, -12.0f},
                {-0.31f, 1.20f, -0.42f}, {-62.0f, 0.0f, 8.0f},
                {-0.27f, 1.05f, -0.66f},
                {0.39f, 1.45f, 0.02f}, {18.0f, 0.0f, 12.0f},
                {0.48f, 1.13f, -0.16f}, {-42.0f, 0.0f, 18.0f},
                {0.56f, 0.96f, -0.35f}};
    case WorkerPose::Standing:
    default:
        return {{-0.43f, 1.43f, 0.0f}, {0.0f, 0.0f, -5.0f},
                {-0.45f, 1.04f, 0.0f}, {0.0f, 0.0f, -2.0f},
                {-0.46f, 0.80f, 0.0f},
                {0.43f, 1.43f, 0.0f}, {0.0f, 0.0f, 5.0f},
                {0.45f, 1.04f, 0.0f}, {0.0f, 0.0f, 2.0f},
                {0.46f, 0.80f, 0.0f}};
    }
}
} // namespace

std::vector<ObjectPart> Worker::create(WorkerPose pose, const WorkerStyle& style)
{
    std::vector<ObjectPart> parts;
    parts.reserve(partCount());

    add(parts, BodyPart::Pelvis, ScenePrimitive::Cube,
        {0.0f, 1.14f, 0.0f}, {}, dimensions::pelvis, style.clothing, "WorkerRoot");
    add(parts, BodyPart::Torso, ScenePrimitive::Cube,
        {0.0f, 1.52f, 0.0f}, {}, dimensions::torso, style.clothing, "Pelvis");
    add(parts, BodyPart::Neck, ScenePrimitive::Cylinder,
        {0.0f, 1.78f, 0.0f}, {}, dimensions::neck, MaterialId::Skin, "Torso");
    add(parts, BodyPart::Head, ScenePrimitive::Sphere,
        {0.0f, 1.94f, 0.0f}, {}, dimensions::head, MaterialId::Skin, "Neck");
    add(parts, BodyPart::Headwear, ScenePrimitive::Cylinder,
        {0.0f, 2.075f, 0.0f}, {}, dimensions::headwear, style.headwear, "Head");

    const ArmPose arms = armPose(pose);
    add(parts, BodyPart::LeftUpperArm, ScenePrimitive::Cylinder,
        arms.leftUpperPosition, arms.leftUpperRotation, dimensions::upperArm,
        MaterialId::Skin, "Torso");
    add(parts, BodyPart::LeftForearm, ScenePrimitive::Cylinder,
        arms.leftForePosition, arms.leftForeRotation, dimensions::forearm,
        MaterialId::Skin, "LeftUpperArm");
    add(parts, BodyPart::LeftHand, ScenePrimitive::Sphere,
        arms.leftHandPosition, {}, dimensions::hand, MaterialId::Skin, "LeftForearm");
    add(parts, BodyPart::RightUpperArm, ScenePrimitive::Cylinder,
        arms.rightUpperPosition, arms.rightUpperRotation, dimensions::upperArm,
        MaterialId::Skin, "Torso");
    add(parts, BodyPart::RightForearm, ScenePrimitive::Cylinder,
        arms.rightForePosition, arms.rightForeRotation, dimensions::forearm,
        MaterialId::Skin, "RightUpperArm");
    add(parts, BodyPart::RightHand, ScenePrimitive::Sphere,
        arms.rightHandPosition, {}, dimensions::hand, MaterialId::Skin, "RightForearm");

    add(parts, BodyPart::LeftThigh, ScenePrimitive::Cylinder,
        {-0.16f, 0.88f, 0.0f}, {0.0f, 0.0f, -3.0f}, dimensions::thigh,
        MaterialId::Skin, "Pelvis");
    add(parts, BodyPart::LeftLowerLeg, ScenePrimitive::Cylinder,
        {-0.18f, 0.43f, 0.0f}, {}, dimensions::lowerLeg, MaterialId::Skin, "LeftThigh");
    add(parts, BodyPart::LeftFoot, ScenePrimitive::Cube,
        {-0.18f, 0.10f, -0.07f}, {}, dimensions::foot, MaterialId::Skin, "LeftLowerLeg");
    add(parts, BodyPart::RightThigh, ScenePrimitive::Cylinder,
        {0.16f, 0.88f, 0.0f}, {0.0f, 0.0f, 3.0f}, dimensions::thigh,
        MaterialId::Skin, "Pelvis");
    add(parts, BodyPart::RightLowerLeg, ScenePrimitive::Cylinder,
        {0.18f, 0.43f, 0.0f}, {}, dimensions::lowerLeg, MaterialId::Skin, "RightThigh");
    add(parts, BodyPart::RightFoot, ScenePrimitive::Cube,
        {0.18f, 0.10f, -0.07f}, {}, dimensions::foot, MaterialId::Skin, "RightLowerLeg");
    return parts;
}

const char* Worker::poseName(WorkerPose pose)
{
    switch (pose)
    {
    case WorkerPose::Standing: return "Standing";
    case WorkerPose::PullingReady: return "PullingReady";
    case WorkerPose::CarryingReady: return "CarryingReady";
    case WorkerPose::LeverReady: return "LeverReady";
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
