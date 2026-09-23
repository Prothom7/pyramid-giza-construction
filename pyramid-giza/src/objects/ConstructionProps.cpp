#include "objects/ConstructionProps.h"

namespace
{
ObjectPart part(const char* name, ScenePrimitive primitive, const glm::vec3& position,
                const glm::vec3& rotation, const glm::vec3& scale,
                MaterialId material, const char* parent)
{
    return {name, primitive, makeTransform(position, rotation, scale), material, parent};
}
} // namespace

std::vector<ObjectPart> ConstructionProps::createLever()
{
    return {
        part("Fulcrum", ScenePrimitive::Cube, {0.0f, 0.28f, 0.0f}, {0.0f, 0.0f, 45.0f},
             {0.55f, 0.55f, 0.70f}, MaterialId::QuarryStone, "LeverRoot"),
        part("LeverBeam", ScenePrimitive::Cylinder, {0.0f, 0.70f, 0.0f},
             {0.0f, 0.0f, 68.0f}, {0.16f, 3.10f, 0.16f}, MaterialId::Wood, "Fulcrum"),
        part("LiftStone", ScenePrimitive::Cube, {-1.25f, 0.35f, 0.0f}, {},
             {0.90f, 0.70f, 0.90f}, MaterialId::PreparedStone, "LeverRoot")
    };
}

std::vector<ObjectPart> ConstructionProps::createMallet()
{
    return {
        part("MalletHandle", ScenePrimitive::Cylinder, {0.0f, 0.48f, 0.0f}, {},
             {0.10f, 0.95f, 0.10f}, MaterialId::Wood, "MalletRoot"),
        part("MalletHead", ScenePrimitive::Cube, {0.0f, 0.96f, 0.0f}, {},
             {0.58f, 0.28f, 0.30f}, MaterialId::DarkWood, "MalletHandle")
    };
}

std::vector<ObjectPart> ConstructionProps::createWoodenFrame()
{
    return {
        part("LeftPost", ScenePrimitive::Cylinder, {-0.90f, 1.15f, 0.0f}, {},
             {0.20f, 2.30f, 0.20f}, MaterialId::Wood, "FrameRoot"),
        part("RightPost", ScenePrimitive::Cylinder, {0.90f, 1.15f, 0.0f}, {},
             {0.20f, 2.30f, 0.20f}, MaterialId::Wood, "FrameRoot"),
        part("TopBeam", ScenePrimitive::Cube, {0.0f, 2.25f, 0.0f}, {},
             {2.15f, 0.22f, 0.24f}, MaterialId::DarkWood, "Posts"),
        part("CrossBeam", ScenePrimitive::Cube, {0.0f, 1.24f, 0.0f}, {},
             {1.80f, 0.18f, 0.20f}, MaterialId::Wood, "Posts"),
        part("HangingRope", ScenePrimitive::Cylinder, {0.0f, 1.73f, 0.0f}, {},
             {0.07f, 0.82f, 0.07f}, MaterialId::Rope, "TopBeam")
    };
}

std::vector<ObjectPart> ConstructionProps::createRoller()
{
    return {part("TimberRoller", ScenePrimitive::Cylinder, {0.0f, 0.22f, 0.0f},
                 {0.0f, 0.0f, 90.0f}, {0.36f, 2.20f, 0.36f}, MaterialId::Wood, "RollerRoot")};
}
