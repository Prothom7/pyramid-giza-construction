#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"
#include "scene/PyramidLayout.h"

enum class ScenePrimitive
{
    Plane,
    Cube,
    Cylinder
};

struct SceneObject
{
    ScenePrimitive primitive;
    glm::mat4 model;
    glm::vec3 color;
};

struct StaticGizaSceneStats
{
    std::size_t pyramidBlocks = 0;
    std::size_t rampComponents = 0;
    std::size_t quarryBlocks = 0;
    std::size_t stockpileBlocks = 0;
    std::size_t constructionProps = 0;
    std::size_t totalDrawCalls = 0;
};

class StaticGizaScene
{
public:
    StaticGizaScene();

    void render(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPosition);

    const StaticGizaSceneStats& stats() const { return stats_; }
    const PyramidLayoutConfig& pyramidConfig() const { return pyramidConfig_; }

private:
    void addObject(ScenePrimitive primitive, const glm::mat4& model, const glm::vec3& color);
    void buildGround();
    void buildPyramid();
    void buildRamp();
    void buildQuarry();
    void buildStockpile();
    void buildConstructionProps();
    const Mesh& meshFor(ScenePrimitive primitive) const;

    Shader shader_;
    Mesh plane_;
    Mesh cube_;
    Mesh cylinder_;
    PyramidLayoutConfig pyramidConfig_;
    std::vector<SceneObject> objects_;
    StaticGizaSceneStats stats_;
};
