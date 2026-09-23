#include "scene/SceneTypes.h"

#include <array>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr std::array<Material, 14> materials{{
    {{0.76f, 0.60f, 0.35f}, 0.24f, 0.72f, 0.05f}, // Sand
    {{0.82f, 0.72f, 0.50f}, 0.20f, 0.76f, 0.12f}, // Limestone
    {{0.72f, 0.61f, 0.40f}, 0.20f, 0.76f, 0.10f}, // Limestone variation
    {{0.50f, 0.43f, 0.34f}, 0.20f, 0.72f, 0.08f}, // Quarry stone
    {{0.88f, 0.78f, 0.57f}, 0.20f, 0.78f, 0.14f}, // Prepared stone
    {{0.48f, 0.31f, 0.17f}, 0.22f, 0.70f, 0.04f}, // Ramp earth
    {{0.34f, 0.18f, 0.08f}, 0.18f, 0.72f, 0.10f}, // Wood
    {{0.23f, 0.11f, 0.045f}, 0.18f, 0.68f, 0.08f}, // Dark wood
    {{0.58f, 0.32f, 0.18f}, 0.25f, 0.72f, 0.10f}, // Skin
    {{0.78f, 0.70f, 0.50f}, 0.22f, 0.74f, 0.08f}, // Linen
    {{0.18f, 0.32f, 0.44f}, 0.18f, 0.75f, 0.12f}, // Blue clothing
    {{0.72f, 0.59f, 0.34f}, 0.21f, 0.73f, 0.08f}, // Headwear
    {{0.48f, 0.34f, 0.18f}, 0.20f, 0.70f, 0.05f}, // Rope
    {{0.22f, 0.24f, 0.25f}, 0.16f, 0.55f, 0.42f}  // Tool metal
}};

constexpr std::array<const char*, 14> materialNames{{
    "Sand", "Limestone", "LimestoneVariation", "QuarryStone", "PreparedStone",
    "RampEarth", "Wood", "DarkWood", "Skin", "ClothingLinen", "ClothingBlue",
    "Headwear", "Rope", "ToolMetal"
}};

std::size_t checkedMaterialIndex(MaterialId id)
{
    const auto index = static_cast<std::size_t>(id);
    if (index >= materials.size())
        throw std::out_of_range("Unknown material identifier");
    return index;
}
} // namespace

const Material& materialDefinition(MaterialId id)
{
    return materials[checkedMaterialIndex(id)];
}

const char* materialName(MaterialId id)
{
    return materialNames[checkedMaterialIndex(id)];
}

const char* primitiveName(ScenePrimitive primitive)
{
    switch (primitive)
    {
    case ScenePrimitive::Plane: return "Plane";
    case ScenePrimitive::Cube: return "Cube";
    case ScenePrimitive::Cylinder: return "Cylinder";
    case ScenePrimitive::Sphere: return "Sphere";
    default: throw std::out_of_range("Unknown scene primitive");
    }
}

glm::mat4 makeTransform(const glm::vec3& translation,
                        const glm::vec3& rotationDegrees,
                        const glm::vec3& scale)
{
    glm::mat4 transform{1.0f};
    transform = glm::translate(transform, translation);
    transform = glm::rotate(transform, glm::radians(rotationDegrees.y), {0.0f, 1.0f, 0.0f});
    transform = glm::rotate(transform, glm::radians(rotationDegrees.x), {1.0f, 0.0f, 0.0f});
    transform = glm::rotate(transform, glm::radians(rotationDegrees.z), {0.0f, 0.0f, 1.0f});
    return glm::scale(transform, scale);
}

bool isFiniteNonSingularTransform(const glm::mat4& transform)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(transform[column][row]))
                return false;
    return std::abs(glm::determinant(glm::mat3(transform))) >= 1.0e-8f;
}
