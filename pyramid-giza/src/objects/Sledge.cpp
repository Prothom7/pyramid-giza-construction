#include "objects/Sledge.h"

namespace
{
void add(std::vector<ObjectPart>& parts, const char* name, ScenePrimitive primitive,
         const glm::vec3& position, const glm::vec3& rotation,
         const glm::vec3& scale, MaterialId material, const char* parent)
{
    parts.push_back({name, primitive, makeTransform(position, rotation, scale), material, parent});
}
} // namespace

std::vector<ObjectPart> Sledge::create(bool loadedStone)
{
    std::vector<ObjectPart> parts;
    parts.reserve(loadedStone ? loadedPartCount() : unloadedPartCount());
    add(parts, "LeftRunner", ScenePrimitive::Cube, {-0.62f, 0.16f, 0.0f}, {},
        {0.24f, 0.22f, 2.75f}, MaterialId::DarkWood, "SledgeRoot");
    add(parts, "RightRunner", ScenePrimitive::Cube, {0.62f, 0.16f, 0.0f}, {},
        {0.24f, 0.22f, 2.75f}, MaterialId::DarkWood, "SledgeRoot");
    for (int brace = 0; brace < 3; ++brace)
        add(parts, brace == 0 ? "RearBrace" : (brace == 1 ? "MiddleBrace" : "FrontBrace"),
            ScenePrimitive::Cube, {0.0f, 0.32f, -0.86f + brace * 0.86f}, {},
            {1.55f, 0.18f, 0.20f}, MaterialId::Wood, "Runners");
    add(parts, "Platform", ScenePrimitive::Cube, {0.0f, 0.46f, -0.08f}, {},
        {1.50f, 0.18f, 2.05f}, MaterialId::Wood, "CrossBraces");
    add(parts, "TowBar", ScenePrimitive::Cube, {0.0f, 0.32f, -1.76f}, {},
        {1.02f, 0.16f, 1.35f}, MaterialId::DarkWood, "Runners");
    add(parts, "PullingRope", ScenePrimitive::Cylinder, {0.0f, 0.36f, -3.25f},
        {90.0f, 0.0f, 0.0f}, {0.08f, 2.05f, 0.08f}, MaterialId::Rope, "TowBar");
    if (loadedStone)
        add(parts, "TransportStone", ScenePrimitive::Cube, {0.0f, 1.04f, -0.02f}, {},
            {1.20f, 0.98f, 1.38f}, MaterialId::PreparedStone, "Platform");
    return parts;
}
