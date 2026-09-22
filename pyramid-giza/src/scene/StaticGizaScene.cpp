#include "scene/StaticGizaScene.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics/PrimitiveGenerator.h"

namespace
{
struct Palette
{
    const glm::vec3 sand{0.76f, 0.60f, 0.35f};
    const glm::vec3 limestone{0.82f, 0.72f, 0.50f};
    const glm::vec3 limestoneVariation{0.72f, 0.61f, 0.40f};
    const glm::vec3 quarryStone{0.50f, 0.43f, 0.34f};
    const glm::vec3 preparedStone{0.88f, 0.78f, 0.57f};
    const glm::vec3 rampEarth{0.48f, 0.31f, 0.17f};
    const glm::vec3 wood{0.34f, 0.18f, 0.08f};
    const glm::vec3 darkWood{0.23f, 0.11f, 0.045f};
};

const Palette colors;

glm::mat4 makeModel(const glm::vec3& translation, const glm::vec3& rotationDegrees,
                    const glm::vec3& scale)
{
    glm::mat4 model{1.0f};
    model = glm::translate(model, translation);
    model = glm::rotate(model, glm::radians(rotationDegrees.y), {0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, glm::radians(rotationDegrees.x), {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(rotationDegrees.z), {0.0f, 0.0f, 1.0f});
    return glm::scale(model, scale);
}

float rampHeightAt(float z)
{
    constexpr float centerY = 2.0f;
    constexpr float centerZ = 5.35f;
    constexpr float slopeDegrees = 10.0f;
    return centerY - (z - centerZ) * std::tan(glm::radians(slopeDegrees));
}

bool finiteMatrix(const glm::mat4& matrix)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(matrix[column][row]))
                return false;
    return true;
}
} // namespace

StaticGizaScene::StaticGizaScene()
    : shader_("shaders/basic.vert", "shaders/basic.frag"),
      plane_(PrimitiveGenerator::createPlane()),
      cube_(PrimitiveGenerator::createCube()),
      cylinder_(PrimitiveGenerator::createCylinder())
{
    objects_.reserve(1280);
    buildGround();
    buildPyramid();
    buildRamp();
    buildQuarry();
    buildStockpile();
    buildConstructionProps();
    stats_.totalDrawCalls = objects_.size();

    const PyramidLayoutStats pyramidStats = PyramidLayout::statistics(
        pyramidConfig_, PyramidLayout::generate(pyramidConfig_));
    std::cout << "Static Giza scene: " << stats_.pyramidBlocks << " pyramid blocks, "
              << stats_.totalDrawCalls << " total draw calls\n"
              << "Pyramid footprint: " << pyramidStats.baseWidth << " x "
              << pyramidStats.baseDepth << ", completed height: "
              << pyramidStats.completedHeight << '\n';
}

void StaticGizaScene::addObject(ScenePrimitive primitive, const glm::mat4& model,
                                const glm::vec3& color)
{
    if (!finiteMatrix(model) || std::abs(glm::determinant(glm::mat3(model))) < 1.0e-8f)
        throw std::runtime_error("Static scene contains an invalid model transform");
    objects_.push_back({primitive, model, color});
}

void StaticGizaScene::buildGround()
{
    addObject(ScenePrimitive::Plane,
              makeModel({0.0f, 0.0f, -4.0f}, {0.0f, 0.0f, 0.0f}, {90.0f, 1.0f, 72.0f}),
              colors.sand);
}

void StaticGizaScene::buildPyramid()
{
    const std::vector<PyramidBlockPlacement> blocks = PyramidLayout::generate(pyramidConfig_);
    for (const PyramidBlockPlacement& block : blocks)
    {
        const glm::vec3 color = (block.level % 3 == 1) ? colors.limestoneVariation
                                                       : colors.limestone;
        addObject(ScenePrimitive::Cube,
                  makeModel(block.position, {0.0f, 0.0f, 0.0f}, block.scale), color);
    }
    stats_.pyramidBlocks = blocks.size();
}

void StaticGizaScene::buildRamp()
{
    const std::size_t start = objects_.size();
    constexpr float slope = 10.0f;
    addObject(ScenePrimitive::Cube,
              makeModel({0.0f, 2.0f, 5.35f}, {slope, 0.0f, 0.0f}, {4.6f, 0.42f, 19.6f}),
              colors.rampEarth);
    for (float x : {-2.35f, 2.35f})
        addObject(ScenePrimitive::Cube,
                  makeModel({x, 2.23f, 5.35f}, {slope, 0.0f, 0.0f}, {0.20f, 0.30f, 19.8f}),
                  colors.darkWood);

    for (int sample = 0; sample < 10; ++sample)
    {
        const float z = -3.0f + sample * 1.85f;
        addObject(ScenePrimitive::Cylinder,
                  makeModel({0.0f, rampHeightAt(z) + 0.28f, z}, {0.0f, 0.0f, 90.0f},
                            {0.24f, 4.9f, 0.24f}),
                  colors.wood);
    }

    for (float z : {1.0f, 6.0f, 11.0f})
    {
        const float height = rampHeightAt(z) - 0.10f;
        for (float x : {-2.0f, 2.0f})
            addObject(ScenePrimitive::Cylinder,
                      makeModel({x, height * 0.5f, z}, {0.0f, 0.0f, 0.0f},
                                {0.28f, height, 0.28f}),
                      colors.wood);
        addObject(ScenePrimitive::Cube,
                  makeModel({0.0f, height, z}, {0.0f, 0.0f, 0.0f}, {4.5f, 0.20f, 0.30f}),
                  colors.darkWood);
    }
    stats_.rampComponents = objects_.size() - start;
}

void StaticGizaScene::buildQuarry()
{
    const std::size_t start = objects_.size();
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            const int pattern = (row * 3 + column * 5) % 4;
            const glm::vec3 scale{1.35f + 0.18f * pattern,
                                  0.62f + 0.12f * ((row + column) % 3),
                                  1.10f + 0.16f * ((row * 2 + column) % 3)};
            const glm::vec3 position{-22.0f + column * 2.35f,
                                     scale.y * 0.5f,
                                     -1.5f + row * 2.25f};
            addObject(ScenePrimitive::Cube,
                      makeModel(position, {0.0f, static_cast<float>((row - column) * 7), 0.0f}, scale),
                      (pattern % 2 == 0) ? colors.quarryStone : colors.limestoneVariation);
        }
    }

    // Stepped cut face makes the area read as a quarry rather than a loose pile.
    for (int level = 0; level < 3; ++level)
        for (int column = 0; column < 3; ++column)
        {
            const glm::vec3 scale{2.0f, 0.85f, 1.25f};
            addObject(ScenePrimitive::Cube,
                      makeModel({-23.0f + column * 2.05f, 0.425f + level * 0.85f,
                                 -5.0f - level * 0.72f},
                                {0.0f, 0.0f, 0.0f}, scale),
                      colors.quarryStone);
        }
    stats_.quarryBlocks = objects_.size() - start;
}

void StaticGizaScene::buildStockpile()
{
    const std::size_t start = objects_.size();
    constexpr glm::vec3 blockScale{1.30f, 0.68f, 1.05f};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            addObject(ScenePrimitive::Cube,
                      makeModel({9.5f + column * 1.48f, blockScale.y * 0.5f,
                                 3.0f + row * 1.28f},
                                {0.0f, 0.0f, 0.0f}, blockScale),
                      colors.preparedStone);
    for (int column = 0; column < 3; ++column)
        addObject(ScenePrimitive::Cube,
                  makeModel({10.25f + column * 1.48f, blockScale.y * 1.5f, 4.28f},
                            {0.0f, 0.0f, 0.0f}, blockScale),
                  colors.preparedStone);
    stats_.stockpileBlocks = objects_.size() - start;
}

void StaticGizaScene::buildConstructionProps()
{
    const std::size_t start = objects_.size();
    for (float x : {15.0f, 19.0f})
        for (float z : {-8.0f, -3.5f})
            addObject(ScenePrimitive::Cylinder,
                      makeModel({x, 1.8f, z}, {0.0f, 0.0f, 0.0f}, {0.34f, 3.6f, 0.34f}),
                      colors.wood);
    for (float z : {-8.0f, -3.5f})
        addObject(ScenePrimitive::Cube,
                  makeModel({17.0f, 3.45f, z}, {0.0f, 0.0f, 0.0f}, {4.5f, 0.28f, 0.32f}),
                  colors.darkWood);
    for (float x : {15.0f, 19.0f})
        addObject(ScenePrimitive::Cube,
                  makeModel({x, 3.45f, -5.75f}, {0.0f, 0.0f, 0.0f}, {0.32f, 0.28f, 4.8f}),
                  colors.darkWood);

    for (int log = 0; log < 5; ++log)
        addObject(ScenePrimitive::Cylinder,
                  makeModel({15.5f + log * 0.7f, 0.24f, 0.0f}, {0.0f, 0.0f, 90.0f},
                            {0.38f, 2.4f, 0.38f}),
                  colors.wood);
    stats_.constructionProps = objects_.size() - start;
}

const Mesh& StaticGizaScene::meshFor(ScenePrimitive primitive) const
{
    switch (primitive)
    {
    case ScenePrimitive::Plane:
        return plane_;
    case ScenePrimitive::Cylinder:
        return cylinder_;
    case ScenePrimitive::Cube:
    default:
        return cube_;
    }
}

void StaticGizaScene::render(const glm::mat4& view, const glm::mat4& projection,
                             const glm::vec3& cameraPosition)
{
    shader_.use();
    shader_.setMat4("view", view);
    shader_.setMat4("projection", projection);
    shader_.setVec3("viewPosition", cameraPosition);
    shader_.setVec3("lightDirection", {-0.55f, -1.0f, -0.30f});
    shader_.setVec3("lightColor", {1.0f, 0.94f, 0.82f});

    for (const SceneObject& object : objects_)
    {
        shader_.setMat4("model", object.model);
        shader_.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(object.model))));
        shader_.setVec3("objectColor", object.color);
        meshFor(object.primitive).draw();
    }
}
