#include "scene/StaticGizaScene.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>

#include "graphics/PrimitiveGenerator.h"
#include "objects/ConstructionProps.h"
#include "objects/Sledge.h"
#include "objects/Worker.h"

namespace
{
float rampHeightAt(float z)
{
    constexpr float centerY = 2.0f;
    constexpr float centerZ = 5.35f;
    constexpr float slopeDegrees = 10.0f;
    return centerY - (z - centerZ) * std::tan(glm::radians(slopeDegrees));
}

} // namespace

StaticGizaScene::StaticGizaScene()
    : shader_("shaders/basic.vert", "shaders/basic.frag"),
      plane_(PrimitiveGenerator::createPlane()),
      cube_(PrimitiveGenerator::createCube()),
      cylinder_(PrimitiveGenerator::createCylinder()),
      sphere_(PrimitiveGenerator::createSphere())
{
    objects_.reserve(1280);
    workers_.reserve(7);
    buildGround();
    buildPyramid();
    buildRamp();
    buildQuarry();
    buildStockpile();
    buildConstructionProps();
    buildCompositeObjects();
    stats_.totalDrawCalls = objects_.size() + stats_.workerParts;

    const PyramidLayoutStats pyramidStats = PyramidLayout::statistics(
        pyramidConfig_, PyramidLayout::generate(pyramidConfig_));
    std::cout << "Static Giza scene: " << stats_.pyramidBlocks << " pyramid blocks, "
              << stats_.totalDrawCalls << " total draw calls\n"
              << "Pyramid footprint: " << pyramidStats.baseWidth << " x "
              << pyramidStats.baseDepth << ", completed height: "
              << pyramidStats.completedHeight << '\n'
              << "Composite objects: " << stats_.workerInstances << " workers ("
              << stats_.workerParts << " body-part draws), " << stats_.sledgeInstances
              << " sledges\n";
}

void StaticGizaScene::addObject(ScenePrimitive primitive, const glm::mat4& model,
                                MaterialId material)
{
    if (!isFiniteNonSingularTransform(model))
        throw std::runtime_error("Static scene contains an invalid model transform");
    static_cast<void>(materialDefinition(material));
    objects_.push_back({primitive, model, material});
}

void StaticGizaScene::addComposite(const glm::mat4& root,
                                   const std::vector<ObjectPart>& parts)
{
    if (!isFiniteNonSingularTransform(root))
        throw std::runtime_error("Composite object contains an invalid root transform");
    for (const ObjectPart& part : parts)
        addObject(part.primitive, root * part.localTransform, part.material);
}

void StaticGizaScene::buildGround()
{
    addObject(ScenePrimitive::Plane,
              makeTransform({0.0f, 0.0f, -4.0f}, {}, {90.0f, 1.0f, 72.0f}),
              MaterialId::Sand);
}

void StaticGizaScene::buildPyramid()
{
    const std::vector<PyramidBlockPlacement> blocks = PyramidLayout::generate(pyramidConfig_);
    for (const PyramidBlockPlacement& block : blocks)
    {
        const MaterialId material = (block.level % 3 == 1) ? MaterialId::LimestoneVariation
                                                            : MaterialId::Limestone;
        addObject(ScenePrimitive::Cube,
                  makeTransform(block.position, {}, block.scale), material);
    }
    stats_.pyramidBlocks = blocks.size();
}

void StaticGizaScene::buildRamp()
{
    const std::size_t start = objects_.size();
    constexpr float slope = 10.0f;
    addObject(ScenePrimitive::Cube,
              makeTransform({0.0f, 2.0f, 5.35f}, {slope, 0.0f, 0.0f}, {4.6f, 0.42f, 19.6f}),
              MaterialId::RampEarth);
    for (float x : {-2.35f, 2.35f})
        addObject(ScenePrimitive::Cube,
                  makeTransform({x, 2.23f, 5.35f}, {slope, 0.0f, 0.0f}, {0.20f, 0.30f, 19.8f}),
                  MaterialId::DarkWood);

    for (int sample = 0; sample < 10; ++sample)
    {
        const float z = -3.0f + sample * 1.85f;
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({0.0f, rampHeightAt(z) + 0.28f, z}, {0.0f, 0.0f, 90.0f},
                            {0.24f, 4.9f, 0.24f}),
                  MaterialId::Wood);
    }

    for (float z : {1.0f, 6.0f, 11.0f})
    {
        const float height = rampHeightAt(z) - 0.10f;
        for (float x : {-2.0f, 2.0f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({x, height * 0.5f, z}, {},
                                {0.28f, height, 0.28f}),
                      MaterialId::Wood);
        addObject(ScenePrimitive::Cube,
                  makeTransform({0.0f, height, z}, {}, {4.5f, 0.20f, 0.30f}),
                  MaterialId::DarkWood);
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
                      makeTransform(position, {0.0f, static_cast<float>((row - column) * 7), 0.0f}, scale),
                      (pattern % 2 == 0) ? MaterialId::QuarryStone : MaterialId::LimestoneVariation);
        }
    }

    // Stepped cut face makes the area read as a quarry rather than a loose pile.
    for (int level = 0; level < 3; ++level)
        for (int column = 0; column < 3; ++column)
        {
            const glm::vec3 scale{2.0f, 0.85f, 1.25f};
            addObject(ScenePrimitive::Cube,
                      makeTransform({-23.0f + column * 2.05f, 0.425f + level * 0.85f,
                                 -5.0f - level * 0.72f},
                                {}, scale),
                      MaterialId::QuarryStone);
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
                      makeTransform({9.5f + column * 1.48f, blockScale.y * 0.5f,
                                 3.0f + row * 1.28f},
                                {}, blockScale),
                      MaterialId::PreparedStone);
    for (int column = 0; column < 3; ++column)
        addObject(ScenePrimitive::Cube,
                  makeTransform({10.25f + column * 1.48f, blockScale.y * 1.5f, 4.28f},
                            {}, blockScale),
                  MaterialId::PreparedStone);
    stats_.stockpileBlocks = objects_.size() - start;
}

void StaticGizaScene::buildConstructionProps()
{
    const std::size_t start = objects_.size();
    for (float x : {15.0f, 19.0f})
        for (float z : {-8.0f, -3.5f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({x, 1.8f, z}, {}, {0.34f, 3.6f, 0.34f}),
                      MaterialId::Wood);
    for (float z : {-8.0f, -3.5f})
        addObject(ScenePrimitive::Cube,
                  makeTransform({17.0f, 3.45f, z}, {}, {4.5f, 0.28f, 0.32f}),
                  MaterialId::DarkWood);
    for (float x : {15.0f, 19.0f})
        addObject(ScenePrimitive::Cube,
                  makeTransform({x, 3.45f, -5.75f}, {}, {0.32f, 0.28f, 4.8f}),
                  MaterialId::DarkWood);

    for (int log = 0; log < 5; ++log)
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({15.5f + log * 0.7f, 0.24f, 0.0f}, {0.0f, 0.0f, 90.0f},
                            {0.38f, 2.4f, 0.38f}),
                  MaterialId::Wood);
    stats_.constructionProps = objects_.size() - start;
}

void StaticGizaScene::buildCompositeObjects()
{
    const auto addWorker = [&](const glm::vec3& position, float rotationY, WorkerPose pose,
                               MaterialId clothing, bool isDemoWorker = false) {
        const WorkerStyle style{clothing, MaterialId::Headwear};
        workers_.push_back({
            makeTransform(position, {0.0f, rotationY, 0.0f}, {1.0f, 1.0f, 1.0f}),
            pose, Worker::poseAngles(pose), style, isDemoWorker});
        ++stats_.workerInstances;
        stats_.workerParts += Worker::partCount();
    };

    // Two pullers face the loaded sledge's rope and the pyramid approach.
    addWorker({5.55f, 0.0f, 5.25f}, 0.0f, WorkerPose::PullingReady, MaterialId::ClothingLinen);
    addWorker({6.45f, 0.0f, 5.25f}, 0.0f, WorkerPose::PullingReady, MaterialId::ClothingBlue);
    addWorker({-17.0f, 0.0f, 1.2f}, -80.0f, WorkerPose::Standing, MaterialId::ClothingBlue);
    addWorker({-19.0f, 0.0f, 6.5f}, 155.0f, WorkerPose::CarryingReady, MaterialId::ClothingLinen);
    addWorker({3.55f, 0.0f, -1.8f}, -35.0f, WorkerPose::CarryingReady, MaterialId::ClothingBlue);
    addWorker({14.8f, 0.0f, -0.8f}, 90.0f, WorkerPose::LeverReady, MaterialId::ClothingLinen);
    addWorker({11.2f, 0.0f, 7.1f}, 170.0f, WorkerPose::Standing,
              MaterialId::ClothingBlue, true);

    addComposite(makeTransform({6.0f, 0.0f, 9.15f}, {}, {1.0f, 1.0f, 1.0f}),
                 Sledge::create(true));
    addComposite(makeTransform({-12.5f, 0.0f, 6.8f}, {0.0f, -55.0f, 0.0f}, {0.9f, 0.9f, 0.9f}),
                 Sledge::create(false));
    stats_.sledgeInstances = 2;

    const std::vector<ObjectPart> lever = ConstructionProps::createLever();
    const std::vector<ObjectPart> mallet = ConstructionProps::createMallet();
    const std::vector<ObjectPart> frame = ConstructionProps::createWoodenFrame();
    const std::vector<ObjectPart> roller = ConstructionProps::createRoller();
    addComposite(makeTransform({13.3f, 0.0f, -1.6f}, {0.0f, 90.0f, 0.0f}, {1.0f, 1.0f, 1.0f}), lever);
    addComposite(makeTransform({17.0f, 0.0f, -1.1f}, {0.0f, 18.0f, 18.0f}, {1.0f, 1.0f, 1.0f}), mallet);
    addComposite(makeTransform({-10.5f, 0.0f, 8.5f}, {0.0f, 20.0f, 0.0f}, {1.0f, 1.0f, 1.0f}), frame);
    addComposite(makeTransform({-13.2f, 0.0f, 3.7f}, {0.0f, 35.0f, 0.0f}, {1.0f, 1.0f, 1.0f}), roller);
    stats_.compositeEquipmentParts = lever.size() + mallet.size() + frame.size() + roller.size();
}

void StaticGizaScene::update(float deltaTime)
{
    if (articulationPreviewEnabled_ && std::isfinite(deltaTime) && deltaTime > 0.0f)
        articulationTime_ += deltaTime * articulationSpeed_;
}

void StaticGizaScene::toggleArticulationPreview()
{
    articulationPreviewEnabled_ = !articulationPreviewEnabled_;
}

void StaticGizaScene::cycleDemoPose()
{
    demoPose_ = Worker::nextPose(demoPose_);
    articulationTime_ = 0.0f;
}

void StaticGizaScene::resetArticulationPreview()
{
    demoPose_ = WorkerPose::Standing;
    articulationTime_ = 0.0f;
}

const Mesh& StaticGizaScene::meshFor(ScenePrimitive primitive) const
{
    switch (primitive)
    {
    case ScenePrimitive::Plane:
        return plane_;
    case ScenePrimitive::Cylinder:
        return cylinder_;
    case ScenePrimitive::Sphere:
        return sphere_;
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

    const auto drawPart = [&](ScenePrimitive primitive, const glm::mat4& model,
                              MaterialId materialId) {
        shader_.setMat4("model", model);
        shader_.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
        const Material& material = materialDefinition(materialId);
        shader_.setVec3("objectColor", material.color);
        shader_.setFloat("materialAmbient", material.ambient);
        shader_.setFloat("materialDiffuse", material.diffuse);
        shader_.setFloat("materialSpecular", material.specular);
        meshFor(primitive).draw();
    };

    for (const SceneObject& object : objects_)
        drawPart(object.primitive, object.model, object.material);

    for (const WorkerInstance& worker : workers_)
    {
        const WorkerJointAngles angles = worker.isDemoWorker
                                             ? Worker::animatedPreview(demoPose_, articulationTime_)
                                             : worker.jointAngles;
        const Worker::EvaluatedPose pose = Worker::evaluate(worker.root, angles, worker.style);
        for (const WorkerPartTransform& part : pose)
            drawPart(part.primitive, part.model, part.material);
    }
}
