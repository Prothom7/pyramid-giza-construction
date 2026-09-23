#pragma once

#include <vector>

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
    LeverReady
};

struct WorkerStyle
{
    MaterialId clothing = MaterialId::ClothingLinen;
    MaterialId headwear = MaterialId::Headwear;
};

class Worker
{
public:
    static std::vector<ObjectPart> create(WorkerPose pose,
                                          const WorkerStyle& style = {});
    static constexpr std::size_t partCount() { return 17; }
    static constexpr float approximateHeight() { return 2.14f; }
    static const char* poseName(WorkerPose pose);
    static const char* bodyPartName(BodyPart part);
};
