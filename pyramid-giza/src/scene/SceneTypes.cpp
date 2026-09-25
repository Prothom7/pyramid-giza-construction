#include "scene/SceneTypes.h"

#include <array>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr std::array<Material, static_cast<std::size_t>(MaterialId::Count)> materials{{
    {{0.76f, 0.60f, 0.35f}, 0.22f, 0.76f, 0.025f, 8.0f, TextureId::Sand, {28.0f, 24.0f}, {}, 0.72f},
    {{0.86f, 0.75f, 0.54f}, 0.20f, 0.84f, 0.10f, 28.0f, TextureId::Limestone, {1.1f, 0.9f}, {}, 0.58f},
    {{0.75f, 0.63f, 0.42f}, 0.20f, 0.80f, 0.08f, 22.0f, TextureId::Limestone, {1.3f, 1.0f}, {0.31f, 0.17f}, 0.68f},
    {{0.50f, 0.43f, 0.34f}, 0.22f, 0.70f, 0.025f, 8.0f, TextureId::QuarryStone, {1.6f, 1.4f}, {}, 0.72f},
    {{0.90f, 0.80f, 0.59f}, 0.20f, 0.86f, 0.14f, 34.0f, TextureId::Limestone, {0.9f, 0.8f}, {0.13f, 0.29f}, 0.42f},
    {{0.48f, 0.31f, 0.17f}, 0.22f, 0.70f, 0.02f, 8.0f, TextureId::Sand, {8.0f, 16.0f}, {0.19f, 0.07f}, 0.64f},
    {{0.34f, 0.18f, 0.08f}, 0.19f, 0.76f, 0.12f, 22.0f, TextureId::Wood, {1.0f, 3.0f}, {}, 0.72f},
    {{0.23f, 0.11f, 0.045f}, 0.19f, 0.72f, 0.10f, 18.0f, TextureId::Wood, {1.1f, 3.5f}, {0.37f, 0.0f}, 0.74f},
    {{0.58f, 0.32f, 0.18f}, 0.24f, 0.78f, 0.08f, 24.0f, TextureId::None, {1.0f, 1.0f}, {}, 0.0f},
    {{0.78f, 0.70f, 0.50f}, 0.22f, 0.80f, 0.05f, 16.0f, TextureId::Cloth, {4.0f, 4.0f}, {}, 0.42f},
    {{0.18f, 0.32f, 0.44f}, 0.20f, 0.82f, 0.08f, 20.0f, TextureId::Cloth, {5.0f, 5.0f}, {0.25f, 0.25f}, 0.48f},
    {{0.72f, 0.59f, 0.34f}, 0.22f, 0.78f, 0.05f, 14.0f, TextureId::Cloth, {3.0f, 3.0f}, {}, 0.38f},
    {{0.48f, 0.34f, 0.18f}, 0.20f, 0.72f, 0.025f, 8.0f, TextureId::Wood, {1.0f, 6.0f}, {}, 0.40f},
    {{0.55f, 0.27f, 0.09f}, 0.16f, 0.68f, 0.48f, 52.0f, TextureId::None, {1.0f, 1.0f}, {}, 0.0f},
    {{0.10f, 0.36f, 0.50f}, 0.25f, 0.72f, 0.55f, 64.0f, TextureId::Water, {10.0f, 4.0f}, {}, 0.72f},
    {{0.36f, 0.43f, 0.22f}, 0.23f, 0.74f, 0.025f, 8.0f, TextureId::Foliage, {16.0f, 12.0f}, {}, 0.56f},
    {{0.18f, 0.34f, 0.10f}, 0.20f, 0.76f, 0.035f, 10.0f, TextureId::Foliage, {3.0f, 4.0f}, {0.21f, 0.34f}, 0.68f}
}};

constexpr std::array<const char*, static_cast<std::size_t>(MaterialId::Count)> materialNames{{
    "Sand", "Limestone", "LimestoneVariation", "QuarryStone", "PreparedStone",
    "RampEarth", "Wood", "DarkWood", "Skin", "ClothingLinen", "ClothingBlue",
    "Headwear", "Rope", "ToolMetal", "Water", "Floodplain", "Foliage"
}};

constexpr std::array<const char*, static_cast<std::size_t>(TextureId::Count)> textureNames{{
    "None", "Sand", "Limestone", "QuarryStone", "Wood", "Cloth", "Water", "Foliage"
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

const char* textureName(TextureId id)
{
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= textureNames.size())
        throw std::out_of_range("Unknown texture identifier");
    return textureNames[index];
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
