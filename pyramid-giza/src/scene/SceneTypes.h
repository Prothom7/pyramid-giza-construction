#pragma once

#include <string>

#include <glm/glm.hpp>

enum class ScenePrimitive
{
    Plane,
    Cube,
    Cylinder,
    Sphere
};

enum class MaterialId
{
    Sand,
    Limestone,
    LimestoneVariation,
    QuarryStone,
    PreparedStone,
    RampEarth,
    Wood,
    DarkWood,
    Skin,
    ClothingLinen,
    ClothingBlue,
    Headwear,
    Rope,
    ToolMetal,
    Water,
    Floodplain,
    Foliage
};

struct Material
{
    glm::vec3 color;
    float ambient;
    float diffuse;
    float specular;
};

struct ObjectPart
{
    std::string name;
    ScenePrimitive primitive;
    glm::mat4 localTransform{1.0f};
    MaterialId material = MaterialId::Wood;
    std::string parentHint;
};

struct SceneObject
{
    ScenePrimitive primitive;
    glm::mat4 model{1.0f};
    MaterialId material = MaterialId::Wood;
};

const Material& materialDefinition(MaterialId id);
const char* materialName(MaterialId id);
const char* primitiveName(ScenePrimitive primitive);

glm::mat4 makeTransform(const glm::vec3& translation,
                        const glm::vec3& rotationDegrees,
                        const glm::vec3& scale);
bool isFiniteNonSingularTransform(const glm::mat4& transform);
