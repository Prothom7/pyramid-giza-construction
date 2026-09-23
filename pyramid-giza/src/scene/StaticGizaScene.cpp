#include "scene/StaticGizaScene.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics/PrimitiveGenerator.h"
#include "objects/ConstructionProps.h"
#include "objects/Scaffold.h"
#include "objects/Sledge.h"
#include "scene/MonumentalSite.h"

namespace
{
constexpr std::size_t heroWorkerCount = static_cast<std::size_t>(WorkerRole::Count);

glm::vec3 rampSide(const RampDescriptor& ramp)
{
    const glm::vec3 direction = glm::normalize(ramp.top - ramp.base);
    return glm::normalize(glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, direction));
}
} // namespace

StaticGizaScene::StaticGizaScene()
    : shader_("shaders/basic.vert", "shaders/basic.frag"),
      plane_(PrimitiveGenerator::createPlane()),
      cube_(PrimitiveGenerator::createCube()),
      cylinder_(PrimitiveGenerator::createCylinder()),
      sphere_(PrimitiveGenerator::createSphere())
{
    objects_.reserve(8500);
    workers_.reserve(25);
    buildGround();
    buildPyramid();
    buildTransportLanes();
    buildRampNetwork();
    buildScaffolding();
    buildQuarryAndCutting();
    buildStockpiles();
    buildTimberAndCamp();
    buildCompositeObjects();

    // Maximum draw count includes the dynamic loaded sledge, two ropes,
    // the hand-attached mallet, and the three-part animated lever.
    stats_.totalDrawCalls = objects_.size() + stats_.workerParts +
                            (loadedSledgeParts_.size() - 1) + 2 + 2 + 3;

    const PyramidLayoutStats pyramidStats = PyramidLayout::statistics(
        pyramidConfig_, PyramidLayout::generate(pyramidConfig_));
    std::cout << "Monumental Giza site: " << stats_.pyramidBlocks << " pyramid blocks, "
              << stats_.totalDrawCalls << " maximum draw calls\n"
              << "Pyramid footprint: " << pyramidStats.baseWidth << " x "
              << pyramidStats.baseDepth << ", completed height: "
              << pyramidStats.completedHeight << " ("
              << pyramidStats.completedHeight / Worker::approximateHeight()
              << " worker-heights)\n"
              << "Site activity: " << stats_.workerInstances << " workers ("
              << stats_.heroWorkers << " hero, " << stats_.secondaryWorkers
              << " secondary, " << stats_.backgroundWorkers << " background), "
              << stats_.sledgeInstances << " sledges, " << stats_.scaffoldModules
              << " scaffold modules, " << MonumentalSite::ramps().size() << " ramps\n";
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

void StaticGizaScene::addWorker(const glm::vec3& position, float rotationY,
                                WorkerPose pose, MaterialId clothing, bool hero,
                                bool secondary, bool demoWorker, float phase)
{
    const WorkerStyle style{clothing, MaterialId::Headwear};
    workers_.push_back({
        makeTransform(position, {0.0f, rotationY, 0.0f}, {1.0f, 1.0f, 1.0f}),
        pose, Worker::poseAngles(pose), style, hero, secondary, demoWorker, phase});
    ++stats_.workerInstances;
    stats_.workerParts += Worker::partCount();
    if (hero)
        ++stats_.heroWorkers;
    else if (secondary)
        ++stats_.secondaryWorkers;
    else
        ++stats_.backgroundWorkers;
}

void StaticGizaScene::addStaticSledge(const glm::vec3& position, float rotationY, float scale)
{
    addComposite(makeTransform(position, {0.0f, rotationY, 0.0f}, {scale, scale, scale}),
                 Sledge::create(false));
    ++stats_.sledgeInstances;
}

void StaticGizaScene::buildGround()
{
    const WorldScale& world = MonumentalSite::scale();
    addObject(ScenePrimitive::Plane,
              makeTransform({-4.0f, -0.02f, -25.0f}, {},
                            {world.worldWidth, 1.0f, world.worldDepth}),
              MaterialId::Sand);
}

void StaticGizaScene::buildPyramid()
{
    const std::vector<PyramidBlockPlacement> blocks = PyramidLayout::generate(pyramidConfig_);
    for (const PyramidBlockPlacement& block : blocks)
    {
        const MaterialId material = (block.level % 4 == 1 || block.level % 4 == 2)
                                        ? MaterialId::LimestoneVariation
                                        : MaterialId::Limestone;
        addObject(ScenePrimitive::Cube, makeTransform(block.position, {}, block.scale), material);
    }
    stats_.pyramidBlocks = blocks.size();
}

void StaticGizaScene::buildTransportLanes()
{
    // Thin raised strips imply compacted hauling corridors without textures.
    addObject(ScenePrimitive::Cube,
              makeTransform({-27.0f, 0.025f, 36.0f}, {}, {47.0f, 0.05f, 5.0f}),
              MaterialId::RampEarth);
    addObject(ScenePrimitive::Cube,
              makeTransform({-57.0f, 0.028f, 10.0f}, {0.0f, -28.0f, 0.0f},
                            {5.0f, 0.055f, 34.0f}),
              MaterialId::RampEarth);
    addObject(ScenePrimitive::Cube,
              makeTransform({26.0f, 0.026f, 28.0f}, {0.0f, 18.0f, 0.0f},
                            {5.0f, 0.052f, 35.0f}),
              MaterialId::RampEarth);
}

void StaticGizaScene::buildRampNetwork()
{
    const std::size_t start = objects_.size();
    for (const RampDescriptor& ramp : MonumentalSite::ramps())
    {
        addObject(ScenePrimitive::Cube, MonumentalSite::rampModel(ramp), ramp.material);

        const glm::vec3 side = rampSide(ramp);
        for (float sign : {-1.0f, 1.0f})
        {
            RampDescriptor rail = ramp;
            const glm::vec3 offset = side * sign * (0.5f * ramp.width + 0.12f);
            rail.base += offset;
            rail.top += offset;
            rail.width = 0.20f;
            rail.thickness = 0.24f;
            addObject(ScenePrimitive::Cube, MonumentalSite::rampModel(rail),
                      MaterialId::DarkWood);
        }
    }

    const RampDescriptor& main = MonumentalSite::mainRamp();
    for (int sample = 1; sample < 14; ++sample)
    {
        const float t = static_cast<float>(sample) / 14.0f;
        const glm::vec3 point = MonumentalSite::rampSurfacePoint(main, t);
        addObject(ScenePrimitive::Cylinder,
                  makeTransform(point + glm::vec3{0.0f, 0.18f, 0.0f},
                                {0.0f, 0.0f, 90.0f},
                                {0.24f, main.width - 0.45f, 0.24f}),
                  MaterialId::Wood);
    }

    for (float t : {0.25f, 0.50f, 0.75f})
    {
        const glm::vec3 surface = MonumentalSite::rampSurfacePoint(main, t);
        const float supportHeight = surface.y - 0.10f;
        for (float x : {-2.75f, 2.75f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({x, supportHeight * 0.5f, surface.z}, {},
                                    {0.28f, supportHeight, 0.28f}),
                      MaterialId::Wood);
        addObject(ScenePrimitive::Cube,
                  makeTransform({0.0f, supportHeight, surface.z}, {},
                                {6.4f, 0.22f, 0.32f}),
                  MaterialId::DarkWood);
    }

    // Work platforms bridge the main ramp, active face, and upper connector.
    addObject(ScenePrimitive::Cube,
              makeTransform({0.0f, 8.55f, -0.8f}, {}, {10.0f, 0.34f, 3.0f}),
              MaterialId::Wood);
    addObject(ScenePrimitive::Cube,
              makeTransform({16.0f, 13.35f, -9.0f}, {0.0f, -35.0f, 0.0f},
                            {7.5f, 0.32f, 4.2f}),
              MaterialId::Wood);
    stats_.rampComponents = objects_.size() - start;
}

void StaticGizaScene::buildScaffolding()
{
    const std::vector<ObjectPart> module = Scaffold::createModule();
    for (const ScaffoldPlacement& group : MonumentalSite::scaffolds())
    {
        const glm::mat4 groupRoot = makeTransform(
            group.origin, {0.0f, group.rotationY, 0.0f}, {1.0f, 1.0f, 1.0f});
        for (unsigned int level = 0; level < group.levels; ++level)
            for (unsigned int bay = 0; bay < group.bays; ++bay)
            {
                const glm::mat4 local = glm::translate(
                    glm::mat4{1.0f},
                    {static_cast<float>(bay) * Scaffold::width(),
                     static_cast<float>(level) * Scaffold::levelHeight(), 0.0f});
                addComposite(groupRoot * local, module);
                ++stats_.scaffoldModules;
            }
    }
}

void StaticGizaScene::buildQuarryAndCutting()
{
    const std::size_t start = objects_.size();

    // Rough extraction field: deterministic size/rotation patterns read as quarry stone.
    for (int row = 0; row < 6; ++row)
        for (int column = 0; column < 7; ++column)
        {
            const int pattern = (row * 5 + column * 3) % 5;
            const glm::vec3 scale{2.1f + 0.24f * pattern,
                                  1.05f + 0.16f * ((row + column) % 4),
                                  1.8f + 0.20f * ((row * 2 + column) % 4)};
            const glm::vec3 position{-84.0f + column * 4.0f, scale.y * 0.5f,
                                     -33.0f + row * 4.4f};
            addObject(ScenePrimitive::Cube,
                      makeTransform(position,
                                    {static_cast<float>((row + column) % 2) * 3.0f,
                                     static_cast<float>((row - column) * 8), 0.0f},
                                    scale),
                      (pattern % 2 == 0) ? MaterialId::QuarryStone
                                         : MaterialId::LimestoneVariation);
        }

    // A stepped cut face and low earth banks make the extraction zone legible.
    for (int level = 0; level < 4; ++level)
        for (int column = 0; column < 6; ++column)
        {
            const glm::vec3 scale{3.1f, 1.25f, 1.8f};
            addObject(ScenePrimitive::Cube,
                      makeTransform({-86.0f + column * 3.15f,
                                     0.625f + level * 1.25f,
                                     -39.0f - level * 1.05f}, {}, scale),
                      MaterialId::QuarryStone);
        }
    addObject(ScenePrimitive::Cube,
              makeTransform({-72.0f, 0.35f, -45.0f}, {}, {35.0f, 0.70f, 3.0f}),
              MaterialId::RampEarth);

    // Cutting progression: rough stones, stones on cutting beds, then prepared blocks.
    for (int station = 0; station < 5; ++station)
    {
        addObject(ScenePrimitive::Cube,
                  makeTransform({-57.0f + station * 3.6f, 0.42f, 14.0f}, {},
                                {2.6f, 0.32f, 2.5f}),
                  MaterialId::DarkWood);
        addObject(ScenePrimitive::Cube,
                  makeTransform({-57.0f + station * 3.6f, 1.05f, 14.0f},
                                {0.0f, static_cast<float>((station - 2) * 4), 0.0f},
                                {1.9f, 1.15f, 1.8f}),
                  MaterialId::QuarryStone);
    }
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 5; ++column)
            addObject(ScenePrimitive::Cube,
                      makeTransform({-56.0f + column * 3.1f, 0.70f,
                                     20.0f + row * 3.2f}, {},
                                    {2.65f, 1.40f, 2.55f}),
                      MaterialId::PreparedStone);

    stats_.quarryBlocks = objects_.size() - start;
}

void StaticGizaScene::buildStockpiles()
{
    const std::size_t start = objects_.size();
    constexpr glm::vec3 preparedScale{2.60f, 1.35f, 2.45f};

    // Orderly prepared-stone stack beside the transport corridor.
    for (int level = 0; level < 3; ++level)
        for (int row = 0; row < 3 - level; ++row)
            for (int column = 0; column < 5 - level; ++column)
                addObject(ScenePrimitive::Cube,
                          makeTransform({10.0f + level * 1.45f + column * 2.85f,
                                         preparedScale.y * (0.5f + level),
                                         31.0f + level * 1.35f + row * 2.70f},
                                        {}, preparedScale),
                          MaterialId::PreparedStone);

    // Transport-ready line near the hero sledge and main-ramp base.
    for (int column = 0; column < 6; ++column)
        addObject(ScenePrimitive::Cube,
                  makeTransform({-12.0f + column * 3.0f, 0.68f, 40.0f}, {},
                                {2.65f, 1.36f, 2.45f}),
                  MaterialId::PreparedStone);

    // Smaller upper staging group sits beside, not inside, the animated lane.
    for (int column = 0; column < 4; ++column)
        addObject(ScenePrimitive::Cube,
                  makeTransform({-10.0f - column * 2.8f, 0.68f, 6.0f}, {},
                                {2.50f, 1.36f, 2.35f}),
                  MaterialId::Limestone);
    stats_.stockpileBlocks = objects_.size() - start;
}

void StaticGizaScene::buildTimberAndCamp()
{
    const std::size_t start = objects_.size();

    // Timber yard: stacked logs, squared beams, and spare frames.
    for (int layer = 0; layer < 3; ++layer)
        for (int log = 0; log < 6 - layer; ++log)
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({42.0f + log * 1.05f + layer * 0.50f,
                                     0.28f + layer * 0.48f, 5.0f},
                                    {0.0f, 0.0f, 90.0f},
                                    {0.42f, 4.5f, 0.42f}),
                      MaterialId::Wood);
    for (int stack = 0; stack < 4; ++stack)
        for (int layer = 0; layer < 3; ++layer)
            addObject(ScenePrimitive::Cube,
                      makeTransform({43.0f + stack * 2.2f, 0.18f + layer * 0.34f, 11.0f},
                                    {}, {5.0f, 0.28f, 0.30f}),
                      layer % 2 == 0 ? MaterialId::DarkWood : MaterialId::Wood);

    const std::vector<ObjectPart> frame = ConstructionProps::createWoodenFrame();
    for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
        addComposite(makeTransform({42.0f + frameIndex * 4.0f, 0.0f, 15.0f},
                                   {0.0f, 8.0f * frameIndex, 0.0f},
                                   {1.2f, 1.2f, 1.2f}), frame);

    // Two simple shade shelters establish a work-camp edge.
    for (int shelter = 0; shelter < 2; ++shelter)
    {
        const glm::vec3 origin{45.0f + shelter * 12.0f, 0.0f, -26.0f};
        for (float x : {-3.0f, 3.0f})
            for (float z : {-2.2f, 2.2f})
                addObject(ScenePrimitive::Cylinder,
                          makeTransform(origin + glm::vec3{x, 2.1f, z}, {},
                                        {0.24f, 4.2f, 0.24f}),
                          MaterialId::Wood);
        addObject(ScenePrimitive::Cube,
                  makeTransform(origin + glm::vec3{0.0f, 4.15f, 0.0f}, {},
                                {7.0f, 0.25f, 5.4f}),
                  MaterialId::DarkWood);
    }

    // Tool clusters are grouped rather than scattered through the whole site.
    const std::vector<ObjectPart> mallet = ConstructionProps::createMallet();
    const std::vector<ObjectPart> roller = ConstructionProps::createRoller();
    for (int cluster = 0; cluster < 3; ++cluster)
    {
        addComposite(makeTransform({39.0f + cluster * 3.0f, 0.0f, -17.0f},
                                   {0.0f, 20.0f * cluster, 12.0f}, {1.0f, 1.0f, 1.0f}),
                     mallet);
        addComposite(makeTransform({40.0f + cluster * 3.4f, 0.0f, -13.5f},
                                   {0.0f, 25.0f * cluster, 0.0f}, {1.0f, 1.0f, 1.0f}),
                     roller);
    }
    stats_.constructionProps = objects_.size() - start;
}

void StaticGizaScene::buildCompositeObjects()
{
    // The first seven entries retain the exact Phase 5 role order.
    addWorker({7.5f, 0.0f, 35.0f}, 0.0f, WorkerPose::PullingReady,
              MaterialId::ClothingLinen, true, false);
    addWorker({12.0f, 0.0f, 35.0f}, 0.0f, WorkerPose::PullingReady,
              MaterialId::ClothingBlue, true, false);
    addWorker({-73.0f, 0.0f, -18.0f}, -80.0f, WorkerPose::Standing,
              MaterialId::ClothingBlue, true, false);
    addWorker({-66.0f, 0.0f, -10.0f}, 155.0f, WorkerPose::CarryingReady,
              MaterialId::ClothingLinen, true, false);
    addWorker({5.0f, 0.0f, 2.0f}, -35.0f, WorkerPose::CarryingReady,
              MaterialId::ClothingBlue, true, false);
    addWorker({23.0f, 0.0f, 12.0f}, 90.0f, WorkerPose::LeverReady,
              MaterialId::ClothingLinen, true, false);
    addWorker({14.0f, 0.0f, 32.0f}, 170.0f, WorkerPose::Standing,
              MaterialId::ClothingBlue, true, false, true);

    // Secondary workers use inexpensive local pose previews.
    const struct WorkerSeed
    {
        glm::vec3 position;
        float heading;
        WorkerPose pose;
    } secondarySeeds[] = {
        {{-81.0f, 0.0f, -25.0f}, 45.0f, WorkerPose::BentKnees},
        {{-75.0f, 0.0f, -28.0f}, -70.0f, WorkerPose::Standing},
        {{-54.0f, 0.0f, 16.0f}, 90.0f, WorkerPose::BentKnees},
        {{-48.0f, 0.0f, 21.0f}, -90.0f, WorkerPose::CarryingReady},
        {{-5.0f, 0.0f, 39.0f}, 0.0f, WorkerPose::PullingReady},
        {{4.0f, 0.0f, 27.0f}, 180.0f, WorkerPose::CarryingReady},
        {{-19.0f, 2.78f, 3.0f}, -90.0f, WorkerPose::CarryingReady},
        {{17.0f, 5.48f, 3.0f}, 90.0f, WorkerPose::Standing},
        {{45.0f, 0.0f, 7.0f}, 135.0f, WorkerPose::CarryingReady}
    };
    for (std::size_t index = 0; index < std::size(secondarySeeds); ++index)
    {
        const WorkerSeed& seed = secondarySeeds[index];
        addWorker(seed.position, seed.heading, seed.pose,
                  index % 2 == 0 ? MaterialId::ClothingLinen : MaterialId::ClothingBlue,
                  false, true, false, static_cast<float>(index) * 0.47f);
    }

    const struct WorkerSeed backgroundSeeds[] = {
        {{-68.0f, 0.0f, -20.0f}, 120.0f, WorkerPose::Standing},
        {{-78.0f, 0.0f, -12.0f}, -20.0f, WorkerPose::CarryingReady},
        {{-45.0f, 0.0f, 14.0f}, 70.0f, WorkerPose::Standing},
        {{-52.0f, 0.0f, 25.0f}, 160.0f, WorkerPose::CarryingReady},
        {{-25.0f, 0.0f, 3.2f}, 0.0f, WorkerPose::Standing},
        {{13.0f, 0.0f, 3.2f}, 180.0f, WorkerPose::ArmsOut},
        {{6.0f, 8.70f, 0.2f}, 180.0f, WorkerPose::Standing},
        {{50.0f, 0.0f, 11.0f}, -120.0f, WorkerPose::CarryingReady},
        {{49.0f, 0.0f, -22.0f}, 45.0f, WorkerPose::Standing}
    };
    for (std::size_t index = 0; index < std::size(backgroundSeeds); ++index)
    {
        const WorkerSeed& seed = backgroundSeeds[index];
        addWorker(seed.position, seed.heading, seed.pose,
                  index % 2 == 0 ? MaterialId::ClothingBlue : MaterialId::ClothingLinen,
                  false, false);
    }

    loadedSledgeParts_ = Sledge::create(true);
    stats_.sledgeInstances = 1;
    addStaticSledge({-72.0f, 0.0f, -7.0f}, -40.0f, 1.05f);
    addStaticSledge({-50.0f, 0.0f, 10.0f}, 15.0f, 1.0f);
    addStaticSledge({-19.0f, 0.0f, 34.0f}, -15.0f, 1.0f);
    addStaticSledge({-8.0f, 0.0f, 44.0f}, 0.0f, 1.0f);
    addStaticSledge({51.0f, 0.0f, 2.0f}, 75.0f, 0.95f);

    stats_.compositeEquipmentParts = 3 + ConstructionProps::createMallet().size() +
                                     ConstructionProps::createWoodenFrame().size() +
                                     ConstructionProps::createRoller().size();
}

void StaticGizaScene::update(float deltaTime)
{
    if (coordinatedAnimationEnabled_)
        animationController_.update(deltaTime);
    else if (articulationPreviewEnabled_ && std::isfinite(deltaTime) && deltaTime > 0.0f)
        articulationTime_ += deltaTime * articulationSpeed_;
}

void StaticGizaScene::togglePlayback()
{
    if (coordinatedAnimationEnabled_)
        animationController_.togglePaused();
    else
        articulationPreviewEnabled_ = !articulationPreviewEnabled_;
}

void StaticGizaScene::toggleCoordinatedAnimation()
{
    coordinatedAnimationEnabled_ = !coordinatedAnimationEnabled_;
}

void StaticGizaScene::advanceAnimationState()
{
    animationController_.advanceState();
}

void StaticGizaScene::toggleAnimationLoop()
{
    animationController_.toggleLooping();
}

void StaticGizaScene::adjustAnimationSpeed(float amount)
{
    animationController_.setSpeed(animationController_.speed() + amount);
}

void StaticGizaScene::cycleDemoPose()
{
    demoPose_ = Worker::nextPose(demoPose_);
    articulationTime_ = 0.0f;
}

void StaticGizaScene::resetAnimation()
{
    animationController_.reset();
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

    const ConstructionAnimationSnapshot animation = animationController_.snapshot();
    std::vector<Worker::EvaluatedPose> evaluatedWorkers(workers_.size());
    for (std::size_t index = 0; index < workers_.size(); ++index)
    {
        const WorkerInstance& worker = workers_[index];
        glm::mat4 root = worker.root;
        WorkerJointAngles angles = worker.jointAngles;
        if (coordinatedAnimationEnabled_ && worker.isHero && index < heroWorkerCount)
        {
            root = animation.workers[index].root;
            angles = animation.workers[index].joints;
        }
        else if (worker.isSecondary)
        {
            const float time = coordinatedAnimationEnabled_
                                   ? animationController_.elapsedTime() + worker.animationPhase
                                   : articulationTime_ + worker.animationPhase;
            angles = Worker::animatedPreview(worker.pose, time);
        }
        else if (!coordinatedAnimationEnabled_ && worker.isDemoWorker)
            angles = Worker::animatedPreview(demoPose_, articulationTime_);

        evaluatedWorkers[index] = Worker::evaluate(root, angles, worker.style);
        for (const WorkerPartTransform& part : evaluatedWorkers[index])
            drawPart(part.primitive, part.model, part.material);
    }

    const glm::mat4 loadedRoot = coordinatedAnimationEnabled_
                                     ? animation.loadedSledgeRoot
                                     : makeTransform({10.0f, 0.0f, 40.0f}, {},
                                                     {1.0f, 1.0f, 1.0f});
    for (const ObjectPart& part : loadedSledgeParts_)
    {
        if (part.name == "PullingRope")
            continue;
        drawPart(part.primitive, loadedRoot * part.localTransform, part.material);
    }

    if (coordinatedAnimationEnabled_ && animation.ropeVisible)
    {
        const auto handPoint = [](const Worker::EvaluatedPose& pose) {
            const glm::vec3 left{
                pose[static_cast<std::size_t>(BodyPart::LeftHand)].jointWorld[3]};
            const glm::vec3 right{
                pose[static_cast<std::size_t>(BodyPart::RightHand)].jointWorld[3]};
            return (left + right) * 0.5f;
        };
        const glm::vec3 leftStart = handPoint(evaluatedWorkers[0]);
        const glm::vec3 rightStart = handPoint(evaluatedWorkers[1]);
        const glm::vec3 leftEnd{loadedRoot * glm::vec4{-0.30f, 0.36f, -2.42f, 1.0f}};
        const glm::vec3 rightEnd{loadedRoot * glm::vec4{0.30f, 0.36f, -2.42f, 1.0f}};
        drawPart(ScenePrimitive::Cylinder,
                 ConstructionAnimationController::cylinderBetween(
                     leftStart, leftEnd, 0.065f),
                 MaterialId::Rope);
        drawPart(ScenePrimitive::Cylinder,
                 ConstructionAnimationController::cylinderBetween(
                     rightStart, rightEnd, 0.065f),
                 MaterialId::Rope);
    }

    // The quarry mallet follows the right-hand joint frame of hero worker 2.
    const glm::mat4 toolRoot = ConstructionAnimationController::toolAttachmentRoot(
        evaluatedWorkers[static_cast<std::size_t>(WorkerRole::QuarryMallet)]
            [static_cast<std::size_t>(BodyPart::RightHand)].jointWorld);
    drawPart(ScenePrimitive::Cylinder,
             glm::scale(glm::translate(toolRoot, {0.0f, -0.32f, 0.0f}),
                        {0.09f, 0.64f, 0.09f}),
             MaterialId::Wood);
    drawPart(ScenePrimitive::Cube,
             glm::scale(glm::translate(toolRoot, {0.0f, -0.68f, 0.0f}),
                        {0.42f, 0.20f, 0.26f}),
             MaterialId::DarkWood);

    const float leverAngle = coordinatedAnimationEnabled_
                                 ? animation.leverAngleDegrees
                                 : 0.0f;
    const float liftOffset = coordinatedAnimationEnabled_
                                 ? animation.liftedStoneOffset
                                 : 0.0f;
    const glm::mat4 leverRoot = ConstructionAnimationController::leverRoot();
    drawPart(ScenePrimitive::Cube,
             leverRoot * makeTransform({0.0f, 0.28f, 0.0f},
                                       {0.0f, 0.0f, 45.0f},
                                       {0.55f, 0.55f, 0.70f}),
             MaterialId::QuarryStone);
    drawPart(ScenePrimitive::Cylinder,
             ConstructionAnimationController::leverBeamModel(leverAngle),
             MaterialId::Wood);
    drawPart(ScenePrimitive::Cube,
             ConstructionAnimationController::leverStoneModel(liftOffset),
             MaterialId::PreparedStone);
}
