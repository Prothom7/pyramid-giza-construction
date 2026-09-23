#include "objects/Scaffold.h"

#include <cmath>

namespace
{
void add(std::vector<ObjectPart>& parts, const char* name, ScenePrimitive primitive,
         const glm::vec3& position, const glm::vec3& rotation,
         const glm::vec3& scale, MaterialId material)
{
    parts.push_back({name, primitive, makeTransform(position, rotation, scale),
                     material, "ScaffoldModule"});
}
} // namespace

std::vector<ObjectPart> Scaffold::createModule()
{
    std::vector<ObjectPart> parts;
    parts.reserve(partCount());
    constexpr float poleDiameter = 0.18f;
    constexpr float beamThickness = 0.16f;
    const float halfWidth = width() * 0.5f;
    const float halfDepth = depth() * 0.5f;

    for (float x : {-halfWidth, halfWidth})
        for (float z : {-halfDepth, halfDepth})
            add(parts, "VerticalPole", ScenePrimitive::Cylinder,
                {x, levelHeight() * 0.5f, z}, {},
                {poleDiameter, levelHeight(), poleDiameter}, MaterialId::Wood);

    for (float z : {-halfDepth, halfDepth})
        add(parts, "HorizontalBeamX", ScenePrimitive::Cube,
            {0.0f, levelHeight() - 0.10f, z}, {},
            {width() + 0.25f, beamThickness, beamThickness}, MaterialId::DarkWood);
    for (float x : {-halfWidth, halfWidth})
        add(parts, "HorizontalBeamZ", ScenePrimitive::Cube,
            {x, levelHeight() - 0.10f, 0.0f}, {},
            {beamThickness, beamThickness, depth() + 0.25f}, MaterialId::DarkWood);

    const float braceLength = std::sqrt(width() * width() + levelHeight() * levelHeight());
    const float braceAngle = glm::degrees(std::atan2(levelHeight(), width()));
    for (float z : {-halfDepth, halfDepth})
    {
        add(parts, "CrossBrace", ScenePrimitive::Cube,
            {0.0f, levelHeight() * 0.5f, z}, {0.0f, 0.0f, braceAngle},
            {braceLength, 0.10f, 0.10f}, MaterialId::Wood);
        add(parts, "CrossBrace", ScenePrimitive::Cube,
            {0.0f, levelHeight() * 0.5f, z}, {0.0f, 0.0f, -braceAngle},
            {braceLength, 0.10f, 0.10f}, MaterialId::Wood);
    }

    add(parts, "Platform", ScenePrimitive::Cube,
        {0.0f, levelHeight(), 0.0f}, {},
        {width() + 0.35f, 0.16f, depth() + 0.35f}, MaterialId::Wood);
    return parts;
}
