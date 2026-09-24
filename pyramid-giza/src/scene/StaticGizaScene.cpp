#include "scene/StaticGizaScene.h"

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics/PrimitiveGenerator.h"
#include "objects/ConstructionProps.h"
#include "objects/Scaffold.h"
#include "objects/Sledge.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"
#include "scene/ObjectEnrichment.h"

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
    objects_.reserve(9800);
    workers_.reserve(44);
    buildGround();
    buildPyramid();
    buildTransportLanes();
    buildRampNetwork();
    buildScaffolding();
    buildQuarryAndCutting();
    buildStockpiles();
    buildTimberAndCamp();
    buildNileAndContext();
    buildHeavyLiftingRig();
    buildObjectEnrichment();
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
              << " scaffold modules, " << MonumentalSite::ramps().size() << " ramps\n"
              << "Industrial context: " << stats_.quarryObjects << " quarry objects, "
              << stats_.repositoryBlocks << " repository stones, "
              << stats_.treeInstances << " trees, " << stats_.liftingRigs
              << " rope/lifting rigs, " << stats_.sphinxParts << " Sphinx-context parts\n"
              << "Object enrichment: " << stats_.enrichmentObjects << " primitive instances, "
              << stats_.anchorPosts << " anchors, " << stats_.ladders << " ladders, "
              << stats_.boats << " boats, 1 workshop and 1 sledge-repair station\n";
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
    // Four plateau slabs leave an actual opening for the recessed open-cut quarry.
    addObject(ScenePrimitive::Plane,
              makeTransform({-179.0f, -0.02f, -30.0f}, {}, {42.0f, 1.0f, 300.0f}),
              MaterialId::Sand);
    addObject(ScenePrimitive::Plane,
              makeTransform({31.0f, -0.02f, -30.0f}, {}, {258.0f, 1.0f, 300.0f}),
              MaterialId::Sand);
    addObject(ScenePrimitive::Plane,
              makeTransform({-128.0f, -0.02f, 57.5f}, {}, {60.0f, 1.0f, 85.0f}),
              MaterialId::Sand);
    addObject(ScenePrimitive::Plane,
              makeTransform({-128.0f, -0.02f, -117.5f}, {}, {60.0f, 1.0f, 125.0f}),
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
    // A broad deterministic road follows the full quarry-to-loading material flow.
    const RampDescriptor quarryRoad{
        "QuarryHaulRoad", {-91.0f, 0.04f, 9.0f}, {-10.0f, 0.04f, 42.0f},
        8.0f, 0.08f, MaterialId::RampEarth, false};
    const RampDescriptor loadingRoad{
        "LoadingToRamp", {-10.0f, 0.045f, 42.0f}, {0.0f, 0.045f, 45.0f},
        8.0f, 0.09f, MaterialId::RampEarth, false};
    addObject(ScenePrimitive::Cube, MonumentalSite::rampModel(quarryRoad),
              MaterialId::RampEarth);
    addObject(ScenePrimitive::Cube, MonumentalSite::rampModel(loadingRoad),
              MaterialId::RampEarth);

    const glm::vec3 side = rampSide(quarryRoad);
    for (int marker = 0; marker <= 9; ++marker)
    {
        const float t = static_cast<float>(marker) / 9.0f;
        const glm::vec3 center = glm::mix(quarryRoad.base, quarryRoad.top, t);
        for (float sign : {-1.0f, 1.0f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform(center + side * sign * 4.6f +
                                        glm::vec3{0.0f, 1.25f, 0.0f},
                                    {}, {0.18f, 2.5f, 0.18f}),
                      MaterialId::Wood);
    }
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

    // Side stairs and rope-guide posts give workers a route separate from the load.
    for (int step = 0; step < 20; ++step)
    {
        const float t = (static_cast<float>(step) + 0.5f) / 20.0f;
        const glm::vec3 surface = MonumentalSite::rampSurfacePoint(main, t);
        addObject(ScenePrimitive::Cube,
                  makeTransform({5.0f, surface.y - 0.10f, surface.z}, {},
                                {1.45f, 0.20f, 2.20f}),
                  MaterialId::DarkWood);
        if (step % 3 == 0)
            for (float x : {-4.25f, 4.25f})
                addObject(ScenePrimitive::Cylinder,
                          makeTransform({x, surface.y + 1.35f, surface.z}, {},
                                        {0.18f, 2.7f, 0.18f}),
                          MaterialId::Wood);
    }

    const glm::vec3 restPoint = MonumentalSite::rampSurfacePoint(main, 0.52f);
    addObject(ScenePrimitive::Cube,
              makeTransform({0.0f, restPoint.y - 0.28f, restPoint.z}, {},
                            {14.0f, 0.34f, 5.2f}),
              MaterialId::Wood);
    for (float x : {-6.4f, 6.4f})
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({x, restPoint.y + 1.2f, restPoint.z}, {},
                                {0.20f, 2.8f, 0.20f}),
                  MaterialId::Wood);

    const RampDescriptor* quarryRamp = nullptr;
    for (const RampDescriptor& ramp : MonumentalSite::ramps())
        if (std::string(ramp.id) == "QuarryExitRamp")
            quarryRamp = &ramp;
    if (quarryRamp != nullptr)
    {
        const glm::vec3 stairSide = rampSide(*quarryRamp);
        for (int step = 0; step < 15; ++step)
        {
            const float t = (static_cast<float>(step) + 0.5f) / 15.0f;
            const glm::vec3 surface = MonumentalSite::rampSurfacePoint(*quarryRamp, t);
            for (float sign : {-1.0f, 1.0f})
                addObject(ScenePrimitive::Cube,
                          makeTransform(surface + stairSide * sign * 5.0f +
                                            glm::vec3{0.0f, -0.12f, 0.0f},
                                        {}, {1.25f, 0.22f, 1.40f}),
                          MaterialId::DarkWood);
            if (step % 3 == 0)
                for (float sign : {-1.0f, 1.0f})
                    addObject(ScenePrimitive::Cylinder,
                              makeTransform(surface + stairSide * sign * 3.9f +
                                                glm::vec3{0.0f, 1.15f, 0.0f},
                                            {}, {0.18f, 2.3f, 0.18f}),
                              MaterialId::Wood);
        }
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

    // A full pit base prevents an open void below the narrower active floor.
    addObject(ScenePrimitive::Plane,
              makeTransform({-128.0f, -7.48f, -15.0f}, {}, {60.0f, 1.0f, 64.0f}),
              MaterialId::QuarryStone);
    // Recessed active floor and three stepped working elevations.
    addObject(ScenePrimitive::Plane,
              makeTransform({-128.0f, -7.45f, -15.0f}, {}, {42.0f, 1.0f, 34.0f}),
              MaterialId::QuarryStone);
    addObject(ScenePrimitive::Cube,
              makeTransform({-128.0f, -1.25f, -48.0f}, {}, {64.0f, 2.5f, 6.0f}),
              MaterialId::QuarryStone);
    addObject(ScenePrimitive::Cube,
              makeTransform({-128.0f, -3.55f, -41.0f}, {}, {54.0f, 2.1f, 8.0f}),
              MaterialId::LimestoneVariation);
    addObject(ScenePrimitive::Cube,
              makeTransform({-128.0f, -5.65f, -33.0f}, {}, {44.0f, 2.1f, 8.0f}),
              MaterialId::QuarryStone);

    for (float sign : {-1.0f, 1.0f})
    {
        addObject(ScenePrimitive::Cube,
                  makeTransform({-128.0f + sign * 29.0f, -1.25f, -15.0f}, {},
                                {6.0f, 2.5f, 72.0f}),
                  MaterialId::QuarryStone);
        addObject(ScenePrimitive::Cube,
                  makeTransform({-128.0f + sign * 24.0f, -3.55f, -15.0f}, {},
                                {4.0f, 2.1f, 58.0f}),
                  MaterialId::LimestoneVariation);
        addObject(ScenePrimitive::Cube,
                  makeTransform({-128.0f + sign * 20.0f, -5.65f, -15.0f}, {},
                                {4.0f, 2.1f, 44.0f}),
                  MaterialId::QuarryStone);
    }
    addObject(ScenePrimitive::Cube,
              makeTransform({-146.0f, -2.0f, 17.0f}, {}, {24.0f, 4.0f, 5.0f}),
              MaterialId::QuarryStone);
    addObject(ScenePrimitive::Cube,
              makeTransform({-116.0f, -2.0f, 17.0f}, {}, {16.0f, 4.0f, 5.0f}),
              MaterialId::LimestoneVariation);

    // Four extraction bays explain the bedrock-to-separated-block progression.
    for (const ExtractionBayDescriptor& bay : IndustrialLandscape::extractionBays())
        addObject(ScenePrimitive::Cube,
                  makeTransform({bay.center.x, -7.22f, bay.center.z}, {},
                                {8.2f, 0.42f, 8.0f}),
                  MaterialId::LimestoneVariation);

    const auto& bays = IndustrialLandscape::extractionBays();
    addObject(ScenePrimitive::Cube,
              makeTransform({bays[0].center.x, -6.25f, bays[0].center.z}, {},
                            {5.2f, 1.55f, 5.0f}),
              MaterialId::QuarryStone);
    for (float offset : {-3.05f, 3.05f})
    {
        addObject(ScenePrimitive::Cube,
                  makeTransform({bays[1].center.x + offset, -6.50f, bays[1].center.z}, {},
                                {1.25f, 1.25f, 7.0f}),
                  MaterialId::QuarryStone);
        addObject(ScenePrimitive::Cube,
                  makeTransform({bays[1].center.x, -6.50f, bays[1].center.z + offset}, {},
                                {5.0f, 1.25f, 1.20f}),
                  MaterialId::QuarryStone);
    }
    addObject(ScenePrimitive::Cube,
              makeTransform({bays[1].center.x, -6.35f, bays[1].center.z}, {},
                            {4.4f, 1.50f, 4.4f}),
              MaterialId::Limestone);

    addObject(ScenePrimitive::Cube,
              makeTransform({bays[2].center.x, -6.20f, bays[2].center.z},
                            {0.0f, 6.0f, 3.0f}, {4.7f, 1.65f, 4.3f}),
              MaterialId::Limestone);
    addObject(ScenePrimitive::Cube,
              makeTransform({bays[2].center.x + 3.0f, -6.55f, bays[2].center.z},
                            {0.0f, 0.0f, 45.0f}, {0.65f, 0.65f, 0.85f}),
              MaterialId::QuarryStone);
    addObject(ScenePrimitive::Cylinder,
              makeTransform({bays[2].center.x + 2.1f, -5.45f, bays[2].center.z},
                            {0.0f, 0.0f, 68.0f}, {0.18f, 4.8f, 0.18f}),
              MaterialId::Wood);

    addObject(ScenePrimitive::Cube,
              makeTransform({bays[3].center.x, -7.34f, bays[3].center.z}, {},
                            {5.1f, 0.18f, 5.0f}),
              MaterialId::RampEarth);
    for (float offset : {-3.0f, 3.0f})
    {
        addObject(ScenePrimitive::Cube,
                  makeTransform({bays[3].center.x + offset, -6.75f, bays[3].center.z}, {},
                                {1.0f, 1.0f, 7.0f}),
                  MaterialId::QuarryStone);
        addObject(ScenePrimitive::Cube,
                  makeTransform({bays[3].center.x, -6.75f, bays[3].center.z + offset}, {},
                                {5.0f, 1.0f, 1.0f}),
                  MaterialId::QuarryStone);
    }

    // Quarry-floor staging and deterministic spoil groups.
    for (int stone = 0; stone < 12; ++stone)
    {
        const int row = stone / 4;
        const int column = stone % 4;
        addObject(ScenePrimitive::Cube,
                  makeTransform({-116.0f + column * 3.2f, -6.65f,
                                 -4.0f + row * 3.1f},
                                {0.0f, static_cast<float>((stone % 5) * 6), 0.0f},
                                {2.45f, 1.45f, 2.35f}),
                  MaterialId::QuarryStone);
    }
    for (int group = 0; group < 3; ++group)
        for (int rock = 0; rock < 10; ++rock)
        {
            const float x = -151.0f + group * 13.0f + (rock % 5) * 1.65f;
            const float z = 2.0f + (rock / 5) * 1.8f;
            const float size = 0.65f + 0.12f * static_cast<float>((rock + group) % 4);
            addObject(ScenePrimitive::Cube,
                      makeTransform({x, -6.95f + size * 0.5f, z},
                                    {0.0f, static_cast<float>(rock * 17), 0.0f},
                                    {size * 1.3f, size, size}),
                      MaterialId::QuarryStone);
        }

    addObject(ScenePrimitive::Cube,
              makeTransform({-109.0f, -6.95f, 8.5f}, {}, {10.0f, 0.30f, 5.0f}),
              MaterialId::DarkWood);
    stats_.quarryObjects = objects_.size() - start;
    stats_.quarryBlocks = stats_.quarryObjects;
}

void StaticGizaScene::buildStockpiles()
{
    const std::size_t start = objects_.size();
    std::size_t repositoryBlocks = 0;
    for (const RepositoryDescriptor& repository : IndustrialLandscape::repositories())
    {
        const float stepX = repository.blockScale.x + repository.spacing;
        const float stepZ = repository.blockScale.z + repository.spacing;
        for (unsigned int level = 0; level < repository.levels; ++level)
            for (unsigned int row = 0; row < repository.rows; ++row)
                for (unsigned int column = 0; column < repository.columns; ++column)
                {
                    const float x = repository.center.x +
                        (static_cast<float>(column) -
                         0.5f * static_cast<float>(repository.columns - 1)) * stepX;
                    const float z = repository.center.z +
                        (static_cast<float>(row) -
                         0.5f * static_cast<float>(repository.rows - 1)) * stepZ;
                    const float yaw = std::string(repository.id) == "RoughDepot"
                                          ? static_cast<float>((row * 13 + column * 7) % 17) - 8.0f
                                          : 0.0f;
                    addObject(ScenePrimitive::Cube,
                              makeTransform({x, repository.center.y +
                                                   repository.blockScale.y *
                                                       (0.5f + static_cast<float>(level)), z},
                                            {0.0f, yaw, 0.0f}, repository.blockScale),
                              repository.material);
                    ++repositoryBlocks;
                }
    }
    stats_.repositoryBlocks = repositoryBlocks;

    // The dressing yard uses low cutting beds and paired timber supports.
    for (int bed = 0; bed < 4; ++bed)
    {
        const glm::vec3 center{-65.0f + bed * 4.7f, 0.28f, 16.0f};
        addObject(ScenePrimitive::Cube,
                  makeTransform(center, {}, {3.6f, 0.42f, 2.7f}),
                  MaterialId::QuarryStone);
        for (float x : {-1.45f, 1.45f})
            addObject(ScenePrimitive::Cube,
                      makeTransform(center + glm::vec3{x, -0.12f, 0.0f}, {},
                                    {0.28f, 0.48f, 3.1f}),
                      MaterialId::Wood);
    }

    // A defined loading station links the finished depot to the hero haul route.
    addObject(ScenePrimitive::Cube,
              makeTransform({-10.0f, 0.24f, 42.0f}, {}, {14.0f, 0.40f, 9.0f}),
              MaterialId::DarkWood);
    for (float x : {-5.5f, 5.5f})
        for (float z : {-3.3f, 3.3f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({-10.0f + x, 1.65f, 42.0f + z}, {},
                                    {0.20f, 2.9f, 0.20f}),
                      MaterialId::Wood);
    for (int waiting = 0; waiting < 4; ++waiting)
        addObject(ScenePrimitive::Cube,
                  makeTransform({-18.0f + waiting * 3.0f, 0.92f, 48.5f}, {},
                                {2.55f, 1.35f, 2.35f}),
                  MaterialId::PreparedStone);
    addObject(ScenePrimitive::Cylinder,
              makeTransform({-15.2f, 1.05f, 39.2f}, {0.0f, 0.0f, 68.0f},
                            {0.16f, 4.8f, 0.16f}),
              MaterialId::Wood);

    // Upper placement material sits beside the route, leaving animation clearance.
    for (int column = 0; column < 4; ++column)
        addObject(ScenePrimitive::Cube,
                  makeTransform({-10.0f - column * 2.8f, 0.68f, 6.0f}, {},
                                {2.50f, 1.36f, 2.35f}),
                  MaterialId::Limestone);
    addObject(ScenePrimitive::Cylinder,
              makeTransform({-8.2f, 1.0f, 1.5f}, {0.0f, 0.0f, 66.0f},
                            {0.17f, 4.5f, 0.17f}),
              MaterialId::Wood);
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

    // Water jars, supply crates, and an inspection shelter support the workforce.
    for (int jar = 0; jar < 6; ++jar)
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({51.0f + (jar % 3) * 1.0f, 0.52f,
                                 -18.5f + (jar / 3) * 1.1f}, {},
                                {0.62f, 1.05f, 0.62f}),
                  MaterialId::RampEarth);
    for (int crate = 0; crate < 5; ++crate)
        addObject(ScenePrimitive::Cube,
                  makeTransform({58.0f + (crate % 3) * 1.6f, 0.48f,
                                 -17.0f + (crate / 3) * 1.6f}, {},
                                {1.35f, 0.95f, 1.25f}),
                  MaterialId::Wood);
    for (float x : {-3.5f, 3.5f})
        for (float z : {-2.4f, 2.4f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({-30.0f + x, 2.3f, 51.0f + z}, {},
                                    {0.22f, 4.6f, 0.22f}),
                      MaterialId::Wood);
    addObject(ScenePrimitive::Cube,
              makeTransform({-30.0f, 4.5f, 51.0f}, {}, {8.0f, 0.28f, 5.8f}),
              MaterialId::DarkWood);
    stats_.constructionProps = objects_.size() - start;
}

void StaticGizaScene::buildNileAndContext()
{
    const std::size_t start = objects_.size();
    const EnvironmentalContext& context = IndustrialLandscape::environment();
    addObject(ScenePrimitive::Plane,
              makeTransform(context.nileCenter, {},
                            {context.nileSize.x, 1.0f, context.nileSize.y}),
              MaterialId::Water);
    addObject(ScenePrimitive::Plane,
              makeTransform(context.floodplainCenter, {},
                            {context.floodplainSize.x, 1.0f, context.floodplainSize.y}),
              MaterialId::Floodplain);

    const std::vector<glm::vec3>& trees = IndustrialLandscape::treePositions();
    for (std::size_t index = 0; index < trees.size(); ++index)
    {
        const float height = 4.6f + 0.35f * static_cast<float>(index % 4);
        addObject(ScenePrimitive::Cylinder,
                  makeTransform(trees[index] + glm::vec3{0.0f, height * 0.5f, 0.0f}, {},
                                {0.34f, height, 0.34f}),
                  MaterialId::Wood);
        for (int leaf = 0; leaf < 5; ++leaf)
            addObject(ScenePrimitive::Cube,
                      makeTransform(trees[index] + glm::vec3{0.0f, height + 0.15f, 0.0f},
                                    {-12.0f, static_cast<float>(leaf) * 72.0f, 16.0f},
                                    {0.48f, 0.16f, 4.0f}),
                      MaterialId::Foliage);
    }
    stats_.treeInstances = trees.size();

    // A deliberately stylized, distant Giza-context landmark made only of primitives.
    const glm::vec3 sphinx = context.sphinxCenter;
    const std::size_t sphinxStart = objects_.size();
    addObject(ScenePrimitive::Cube,
              makeTransform(sphinx + glm::vec3{0.0f, 2.0f, 0.0f}, {}, {13.0f, 4.0f, 5.8f}),
              MaterialId::LimestoneVariation);
    addObject(ScenePrimitive::Cube,
              makeTransform(sphinx + glm::vec3{0.0f, 3.4f, -3.7f}, {}, {5.8f, 5.0f, 4.0f}),
              MaterialId::LimestoneVariation);
    addObject(ScenePrimitive::Sphere,
              makeTransform(sphinx + glm::vec3{0.0f, 6.7f, -4.1f}, {}, {3.1f, 3.5f, 2.7f}),
              MaterialId::Limestone);
    for (float x : {-3.5f, 3.5f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(sphinx + glm::vec3{x, 0.75f, -5.1f}, {}, {2.8f, 1.5f, 8.5f}),
                  MaterialId::Limestone);
    for (float x : {-2.35f, 2.35f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(sphinx + glm::vec3{x, 7.1f, -4.0f}, {0.0f, 0.0f, x * 3.0f},
                                {1.5f, 4.1f, 3.2f}),
                  MaterialId::PreparedStone);
    stats_.sphinxParts = objects_.size() - sphinxStart;
    stats_.environmentalObjects = objects_.size() - start;
}

void StaticGizaScene::buildHeavyLiftingRig()
{
    const HeavyLiftingRigDescriptor& rig = IndustrialLandscape::liftingRig();
    const glm::vec3 center = rig.center;
    for (float x : {-rig.width * 0.5f, rig.width * 0.5f})
        for (float z : {-rig.depth * 0.5f, rig.depth * 0.5f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform(center + glm::vec3{x, rig.height * 0.5f, z}, {},
                                    {0.28f, rig.height, 0.28f}),
                      MaterialId::Wood);
    addObject(ScenePrimitive::Cube,
              makeTransform(center + glm::vec3{0.0f, rig.height, 0.0f}, {},
                            {rig.width + 1.0f, 0.34f, 0.42f}),
              MaterialId::DarkWood);
    for (float x : {-1.7f, 1.7f})
        addObject(ScenePrimitive::Cylinder,
                  makeTransform(center + glm::vec3{x, rig.height - 0.45f, 0.0f},
                                {0.0f, 0.0f, 90.0f}, {0.82f, 0.34f, 0.82f}),
                  MaterialId::Wood);

    const glm::vec3 loadCenter = center + glm::vec3{0.0f, 1.35f, 0.0f};
    addObject(ScenePrimitive::Cube,
              makeTransform(loadCenter, {}, {3.2f, 1.7f, 2.8f}),
              MaterialId::QuarryStone);
    addObject(ScenePrimitive::Cylinder,
              ConstructionAnimationController::cylinderBetween(
                  center + glm::vec3{-1.7f, rig.height - 0.45f, 0.0f},
                  loadCenter + glm::vec3{-1.1f, 1.0f, 0.0f}, 0.075f),
              MaterialId::Rope);
    addObject(ScenePrimitive::Cylinder,
              ConstructionAnimationController::cylinderBetween(
                  center + glm::vec3{1.7f, rig.height - 0.45f, 0.0f},
                  loadCenter + glm::vec3{1.1f, 1.0f, 0.0f}, 0.075f),
              MaterialId::Rope);
    stats_.liftingRigs = 1;
}

void StaticGizaScene::buildObjectEnrichment()
{
    const std::size_t start = objects_.size();
    buildRopeInfrastructure();
    buildScaffoldAccess();
    buildWorkshopRepairAndInspection();
    buildRiverLanding();
    buildUpperPlatformDetails();
    stats_.enrichmentObjects = objects_.size() - start;
    if (stats_.enrichmentObjects != ObjectEnrichment::expectedStaticInstances)
        throw std::runtime_error("Object-enrichment instance count changed unexpectedly");
}

void StaticGizaScene::buildRopeInfrastructure()
{
    // These rigs are deliberately labelled speculative: they visualize useful
    // graphics/composition ideas without asserting known Khufu-era pulley machinery.
    for (const RopeRigDescriptor& rig : ObjectEnrichment::ropeRigs())
    {
        const glm::mat4 root = makeTransform(
            rig.center, {0.0f, rig.yawDegrees, 0.0f}, {1.0f, 1.0f, 1.0f});
        const auto worldPoint = [&root](const glm::vec3& local) {
            return glm::vec3{root * glm::vec4{local, 1.0f}};
        };
        const auto addSegment = [this](const glm::vec3& from, const glm::vec3& to,
                                       float diameter, MaterialId material) {
            addObject(ScenePrimitive::Cylinder,
                      ConstructionAnimationController::cylinderBetween(
                          from, to, diameter), material);
        };

        glm::vec3 wheelCenter{0.0f, rig.height - 0.48f, 0.0f};
        if (rig.type == RopeRigType::AFrameLift)
        {
            for (float x : {-0.5f * rig.width, 0.5f * rig.width})
                for (float z : {-0.5f * rig.depth, 0.5f * rig.depth})
                    addSegment(worldPoint({x, 0.0f, z}),
                               worldPoint({x * 0.28f, rig.height, 0.0f}),
                               0.30f, MaterialId::Wood);
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, rig.height, 0.0f}, {},
                                           {rig.width * 0.48f, 0.34f, 0.42f}),
                      MaterialId::DarkWood);
        }
        else
        {
            const float postDepth = rig.type == RopeRigType::HorizontalRedirection
                                        ? 0.5f * rig.depth : 0.0f;
            for (float x : {-0.5f * rig.width, 0.5f * rig.width})
                for (float z : (postDepth > 0.0f
                                    ? std::initializer_list<float>{-postDepth, postDepth}
                                    : std::initializer_list<float>{0.0f}))
                    addObject(ScenePrimitive::Cylinder,
                              root * makeTransform({x, 0.5f * rig.height, z}, {},
                                                   {0.30f, rig.height, 0.30f}),
                              MaterialId::Wood);
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, rig.height, 0.0f}, {},
                                           {rig.width + 0.6f, 0.34f, 0.42f}),
                      MaterialId::DarkWood);
            for (float x : {-0.5f * rig.width, 0.5f * rig.width})
                addSegment(worldPoint({x, 0.3f, 0.0f}),
                           worldPoint({x * 0.62f, rig.height - 0.15f, 0.0f}),
                           0.18f, MaterialId::DarkWood);
        }

        const float wheelSpacing = rig.wheelCount == 2 ? 1.35f : 0.0f;
        for (unsigned int wheel = 0; wheel < rig.wheelCount; ++wheel)
        {
            const float x = (static_cast<float>(wheel) -
                             0.5f * static_cast<float>(rig.wheelCount - 1)) * wheelSpacing;
            wheelCenter.x = x;
            addObject(ScenePrimitive::Cylinder,
                      root * makeTransform(wheelCenter, {90.0f, 0.0f, 0.0f},
                                           {0.92f, 0.32f, 0.92f}),
                      MaterialId::Wood);
            addObject(ScenePrimitive::Cylinder,
                      root * makeTransform(wheelCenter, {90.0f, 0.0f, 0.0f},
                                           {0.18f, rig.depth + 0.50f, 0.18f}),
                      MaterialId::DarkWood);
        }

        const glm::vec3 guide = worldPoint(wheelCenter);
        if (rig.type == RopeRigType::AFrameLift)
        {
            const glm::vec3 load = worldPoint({0.0f, 0.75f, 0.0f});
            addSegment(guide, load + glm::vec3{0.0f, 0.75f, 0.0f},
                       0.075f, MaterialId::Rope);
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, 0.75f, 0.0f}, {},
                                           {2.5f, 1.5f, 2.3f}),
                      MaterialId::PreparedStone);
        }
        else if (rig.type == RopeRigType::RampAssist)
        {
            addSegment(guide, worldPoint({-7.0f, 0.35f, -5.5f}),
                       0.075f, MaterialId::Rope);
        }
        else
        {
            addSegment(guide, worldPoint({6.2f, 0.45f, -3.0f}),
                       0.075f, MaterialId::Rope);
        }
    }

    for (const AnchorPostDescriptor& post : ObjectEnrichment::anchorPosts())
    {
        addObject(ScenePrimitive::Cylinder,
                  makeTransform(post.base + glm::vec3{0.0f, 0.5f * post.height, 0.0f},
                                {}, {post.diameter, post.height, post.diameter}),
                  MaterialId::DarkWood);
        addObject(ScenePrimitive::Cylinder,
                  makeTransform(post.base + glm::vec3{0.0f, post.height - 0.35f, 0.0f},
                                {0.0f, 0.0f, 90.0f},
                                {0.18f, 1.15f, 0.18f}),
                  MaterialId::Wood);
    }
    stats_.enrichmentRopeRigs = ObjectEnrichment::ropeRigs().size();
    stats_.liftingRigs += stats_.enrichmentRopeRigs;
    stats_.anchorPosts = ObjectEnrichment::anchorPosts().size();
}

void StaticGizaScene::buildScaffoldAccess()
{
    for (const LadderDescriptor& ladder : ObjectEnrichment::ladders())
    {
        const glm::mat4 root = makeTransform(
            ladder.base, {0.0f, ladder.yawDegrees, ladder.leanDegrees},
            {1.0f, 1.0f, 1.0f});
        for (float sign : {-1.0f, 1.0f})
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({sign * 0.5f * ladder.width,
                                            0.5f * ladder.height, 0.0f}, {},
                                           {0.16f, ladder.height, 0.18f}),
                      MaterialId::DarkWood);
        const float spacing = ladder.height / static_cast<float>(ladder.rungCount + 1);
        for (unsigned int rung = 1; rung <= ladder.rungCount; ++rung)
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, spacing * static_cast<float>(rung), 0.0f}, {},
                                           {ladder.width + 0.18f, 0.12f, 0.16f}),
                      MaterialId::Wood);
    }

    struct Walkway
    {
        glm::vec3 center;
        glm::vec3 scale;
        float yaw;
    };
    const Walkway walkways[]{
        {{-17.0f, 5.55f, 2.1f}, {8.0f, 0.22f, 1.35f}, 0.0f},
        {{18.0f, 5.55f, 2.1f}, {8.0f, 0.22f, 1.35f}, 0.0f},
        {{-1.0f, 11.35f, -1.0f}, {7.0f, 0.22f, 1.30f}, 0.0f},
        {{-40.0f, 5.35f, -25.0f}, {6.0f, 0.22f, 1.25f}, 0.0f}
    };
    for (const Walkway& walkway : walkways)
    {
        const glm::mat4 root = makeTransform(
            walkway.center, {0.0f, walkway.yaw, 0.0f}, {1.0f, 1.0f, 1.0f});
        addObject(ScenePrimitive::Cube,
                  root * makeTransform({}, {}, walkway.scale), MaterialId::Wood);
        for (float z : {-0.72f, 0.72f})
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, 0.80f, z}, {},
                                           {walkway.scale.x, 0.14f, 0.14f}),
                      MaterialId::DarkWood);
    }
    stats_.ladders = ObjectEnrichment::ladders().size();
    stats_.scaffoldAccessObjects = stats_.ladders + ObjectEnrichment::accessWalkways;
}

void StaticGizaScene::buildWorkshopRepairAndInspection()
{
    const glm::vec3 workshop{32.0f, 0.0f, -15.0f};
    addObject(ScenePrimitive::Cube,
              makeTransform(workshop + glm::vec3{0.0f, 0.12f, 0.0f}, {},
                            {12.0f, 0.20f, 8.0f}),
              MaterialId::RampEarth);
    addObject(ScenePrimitive::Cube,
              makeTransform(workshop + glm::vec3{-2.2f, 1.15f, 0.0f}, {},
                            {5.0f, 0.30f, 2.0f}),
              MaterialId::DarkWood);
    for (float x : {-4.2f, -0.2f})
        for (float z : {-0.75f, 0.75f})
            addObject(ScenePrimitive::Cube,
                      makeTransform(workshop + glm::vec3{x, 0.58f, z}, {},
                                    {0.24f, 1.15f, 0.24f}),
                      MaterialId::Wood);

    // A readable tool rack: frame plus six coarse mallet/chisel silhouettes.
    for (float x : {1.2f, 5.2f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(workshop + glm::vec3{x, 1.7f, 2.2f}, {},
                                {0.25f, 3.4f, 0.25f}),
                  MaterialId::DarkWood);
    for (float y : {0.55f, 2.85f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(workshop + glm::vec3{3.2f, y, 2.2f}, {},
                                {4.3f, 0.20f, 0.25f}),
                  MaterialId::Wood);
    for (int tool = 0; tool < 6; ++tool)
    {
        const float x = 1.55f + static_cast<float>(tool) * 0.66f;
        addObject(ScenePrimitive::Cylinder,
                  makeTransform(workshop + glm::vec3{x, 1.65f, 2.05f},
                                {0.0f, 0.0f, tool % 2 == 0 ? 8.0f : -8.0f},
                                {0.09f, 1.85f, 0.09f}),
                  MaterialId::Wood);
        addObject(ScenePrimitive::Cube,
                  makeTransform(workshop + glm::vec3{x, 0.76f, 2.05f}, {},
                                {0.40f, 0.20f, 0.28f}),
                  MaterialId::ToolMetal);
    }
    addObject(ScenePrimitive::Cylinder,
              makeTransform(workshop + glm::vec3{4.2f, 1.25f, -1.4f},
                            {90.0f, 0.0f, 0.0f}, {1.05f, 0.42f, 1.05f}),
              MaterialId::QuarryStone);
    addObject(ScenePrimitive::Cylinder,
              makeTransform(workshop + glm::vec3{4.2f, 1.25f, -1.4f},
                            {90.0f, 0.0f, 0.0f}, {0.16f, 2.0f, 0.16f}),
              MaterialId::DarkWood);
    for (float x : {3.2f, 5.2f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(workshop + glm::vec3{x, 0.68f, -1.4f}, {},
                                {0.28f, 1.36f, 1.20f}),
                  MaterialId::Wood);
    for (int crate = 0; crate < 4; ++crate)
        addObject(ScenePrimitive::Cube,
                  makeTransform(workshop + glm::vec3{-4.5f + (crate % 2) * 1.5f,
                                                     0.45f, -2.5f + (crate / 2) * 1.4f}, {},
                                {1.25f, 0.90f, 1.10f}),
                  MaterialId::Wood);

    // A partial sledge and spare components make the repair purpose visible.
    const glm::vec3 repair{64.0f, 0.0f, 8.0f};
    addObject(ScenePrimitive::Cube,
              makeTransform(repair + glm::vec3{0.0f, 0.08f, 0.0f}, {},
                            {13.0f, 0.14f, 10.0f}),
              MaterialId::RampEarth);
    for (float x : {-1.25f, 1.25f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(repair + glm::vec3{x, 0.38f, 0.0f}, {},
                                {0.42f, 0.45f, 5.8f}),
                  MaterialId::DarkWood);
    for (float z : {-2.1f, 0.0f, 2.1f})
        addObject(ScenePrimitive::Cube,
                  makeTransform(repair + glm::vec3{0.0f, 0.70f, z}, {},
                                {3.4f, 0.24f, 0.34f}),
                  MaterialId::Wood);
    for (int runner = 0; runner < 3; ++runner)
        addObject(ScenePrimitive::Cube,
                  makeTransform(repair + glm::vec3{4.4f, 0.32f + runner * 0.34f,
                                                   -1.5f + runner * 0.35f},
                                {0.0f, -8.0f, 0.0f}, {0.38f, 0.28f, 5.5f}),
                  MaterialId::Wood);
    addObject(ScenePrimitive::Cube,
              makeTransform(repair + glm::vec3{-4.1f, 1.05f, -1.4f}, {},
                            {3.6f, 0.28f, 1.7f}), MaterialId::DarkWood);
    for (float x : {-5.4f, -2.8f})
        for (float z : {-2.0f, -0.8f})
            addObject(ScenePrimitive::Cube,
                      makeTransform(repair + glm::vec3{x, 0.52f, z}, {},
                                    {0.22f, 1.05f, 0.22f}), MaterialId::Wood);
    const std::vector<ObjectPart> mallet = ConstructionProps::createMallet();
    for (int tool = 0; tool < 3; ++tool)
        addComposite(makeTransform(repair + glm::vec3{-4.8f + tool * 0.8f, 1.25f, -1.1f},
                                   {0.0f, 20.0f * tool, 72.0f}, {0.85f, 0.85f, 0.85f}),
                     mallet);

    // Inspection bed, reference rods and a plumb marker link dressing to loading.
    const glm::vec3 inspection{-50.0f, 0.0f, 8.0f};
    addObject(ScenePrimitive::Cube,
              makeTransform(inspection + glm::vec3{0.0f, 0.42f, 0.0f}, {},
                            {7.0f, 0.50f, 5.0f}), MaterialId::DarkWood);
    addObject(ScenePrimitive::Cube,
              makeTransform(inspection + glm::vec3{0.0f, 1.32f, 0.0f}, {},
                            {3.4f, 1.35f, 3.0f}), MaterialId::PreparedStone);
    for (float x : {-2.7f, 2.7f})
        for (float z : {-1.8f, 1.8f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform(inspection + glm::vec3{x, 1.75f, z}, {},
                                    {0.12f, 3.5f, 0.12f}), MaterialId::Wood);
    addObject(ScenePrimitive::Cylinder,
              ConstructionAnimationController::cylinderBetween(
                  inspection + glm::vec3{-2.7f, 2.8f, -1.8f},
                  inspection + glm::vec3{2.7f, 2.8f, -1.8f}, 0.055f),
              MaterialId::Rope);
    addObject(ScenePrimitive::Cylinder,
              ConstructionAnimationController::cylinderBetween(
                  inspection + glm::vec3{0.0f, 2.8f, -1.8f},
                  inspection + glm::vec3{0.0f, 0.65f, -1.8f}, 0.045f),
              MaterialId::Rope);
    addObject(ScenePrimitive::Sphere,
              makeTransform(inspection + glm::vec3{0.0f, 0.55f, -1.8f}, {},
                            {0.18f, 0.26f, 0.18f}), MaterialId::ToolMetal);
    for (float x : {-2.35f, 2.35f})
        addObject(ScenePrimitive::Cube,
                  makeTransform({-21.0f + x, 0.25f, 45.5f}, {-8.0f, 0.0f, 0.0f},
                                {1.2f, 0.24f, 6.5f}), MaterialId::DarkWood);

    // Organized lever/timber racks at extraction, loading, and repair zones.
    const glm::vec3 rackCenters[]{
        {-108.0f, 0.0f, 13.0f}, {-23.0f, 0.0f, 51.0f}, {55.0f, 0.0f, 16.0f}
    };
    for (const glm::vec3& center : rackCenters)
    {
        for (float x : {-2.6f, 2.6f})
            addObject(ScenePrimitive::Cube,
                      makeTransform(center + glm::vec3{x, 1.0f, 0.0f}, {},
                                    {0.28f, 2.0f, 2.4f}), MaterialId::DarkWood);
        for (int beam = 0; beam < 5; ++beam)
            addObject(ScenePrimitive::Cylinder,
                      makeTransform(center + glm::vec3{0.0f, 0.55f + beam * 0.28f,
                                                       -0.65f + (beam % 2) * 1.30f},
                                    {0.0f, 0.0f, 90.0f}, {0.18f, 5.6f, 0.18f}),
                      MaterialId::Wood);
    }

    // Small camp/admin arrangement remains secondary to the construction zones.
    addObject(ScenePrimitive::Cube,
              makeTransform({65.0f, 0.95f, -26.0f}, {}, {4.0f, 0.25f, 2.2f}),
              MaterialId::DarkWood);
    for (float x : {63.4f, 66.6f})
        for (float z : {-26.8f, -25.2f})
            addObject(ScenePrimitive::Cube,
                      makeTransform({x, 0.46f, z}, {}, {0.22f, 0.92f, 0.22f}),
                      MaterialId::Wood);
    for (float z : {-29.0f, -23.0f})
        addObject(ScenePrimitive::Cube,
                  makeTransform({65.0f, 0.35f, z}, {}, {5.0f, 0.38f, 0.85f}),
                  MaterialId::Wood);

    stats_.workshopClusters = ObjectEnrichment::workshopClusters;
    stats_.repairStations = ObjectEnrichment::repairStations;
}

void StaticGizaScene::buildRiverLanding()
{
    const std::size_t start = objects_.size();

    // Segmented primitive quay sits at the floodplain/water boundary.
    for (int section = 0; section < 8; ++section)
    {
        const float x = -42.0f + static_cast<float>(section) * 5.0f;
        addObject(ScenePrimitive::Cube,
                  makeTransform({x, 0.24f, -153.2f}, {}, {4.7f, 0.36f, 5.5f}),
                  MaterialId::QuarryStone);
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({x - 1.8f, 0.80f, -155.4f}, {},
                                {0.24f, 2.0f, 0.24f}), MaterialId::DarkWood);
    }
    addObject(ScenePrimitive::Cube,
              makeTransform({-24.0f, 0.10f, -143.0f}, {}, {8.0f, 0.14f, 17.0f}),
              MaterialId::RampEarth);

    for (const BoatDescriptor& boat : ObjectEnrichment::boats())
    {
        const glm::mat4 root = makeTransform(
            boat.center, {0.0f, boat.yawDegrees, 0.0f}, {1.0f, 1.0f, 1.0f});
        addObject(ScenePrimitive::Cube,
                  root * makeTransform({0.0f, 0.26f, 0.0f}, {},
                                       {boat.width * 0.68f, 0.42f, boat.length * 0.82f}),
                  MaterialId::DarkWood);
        for (float sign : {-1.0f, 1.0f})
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({sign * boat.width * 0.43f, 0.72f, 0.0f},
                                           {0.0f, 0.0f, -sign * 12.0f},
                                           {0.34f, 0.80f, boat.length}),
                      MaterialId::Wood);
        for (float sign : {-1.0f, 1.0f})
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, 0.82f, sign * boat.length * 0.45f},
                                           {sign * 22.0f, 0.0f, 0.0f},
                                           {boat.width * 0.82f, 0.62f, 1.45f}),
                      MaterialId::Wood);
        for (float z : {-0.25f * boat.length, 0.0f, 0.25f * boat.length})
            addObject(ScenePrimitive::Cube,
                      root * makeTransform({0.0f, 0.88f, z}, {},
                                           {boat.width * 0.72f, 0.18f, 0.34f}),
                      MaterialId::DarkWood);
        addObject(ScenePrimitive::Cylinder,
                  root * makeTransform({0.0f, 2.1f, 0.8f}, {},
                                       {0.16f, 3.5f, 0.16f}), MaterialId::Wood);
        addObject(ScenePrimitive::Cube,
                  root * makeTransform({0.0f, 3.0f, 0.8f}, {0.0f, 0.0f, 8.0f},
                                       {2.4f, 0.10f, 1.6f}), MaterialId::ClothingLinen);

        if (boat.mooredAtLanding)
        {
            const glm::vec3 bow{root * glm::vec4{0.0f, 0.8f, 0.45f * boat.length, 1.0f}};
            const glm::vec3 stern{root * glm::vec4{0.0f, 0.8f, -0.45f * boat.length, 1.0f}};
            addObject(ScenePrimitive::Cylinder,
                      ConstructionAnimationController::cylinderBetween(
                          bow, {-30.0f, 1.9f, -153.0f}, 0.055f), MaterialId::Rope);
            addObject(ScenePrimitive::Cylinder,
                      ConstructionAnimationController::cylinderBetween(
                          stern, {-42.0f, 1.9f, -153.0f}, 0.055f), MaterialId::Rope);
        }
    }

    // Cargo, timber and a small landing shelter establish river logistics.
    for (int crate = 0; crate < 10; ++crate)
        addObject(ScenePrimitive::Cube,
                  makeTransform({-40.0f + (crate % 5) * 1.55f,
                                 0.72f + (crate / 5) * 0.82f,
                                 -149.0f}, {}, {1.25f, 0.78f, 1.15f}), MaterialId::Wood);
    for (int timber = 0; timber < 7; ++timber)
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({-17.0f, 0.30f + timber * 0.24f,
                                 -149.0f + (timber % 2) * 0.60f},
                                {0.0f, 0.0f, 90.0f}, {0.22f, 5.6f, 0.22f}),
                  MaterialId::Wood);
    for (float x : {-3.0f, 3.0f})
        for (float z : {-2.0f, 2.0f})
            addObject(ScenePrimitive::Cylinder,
                      makeTransform({-7.0f + x, 1.8f, -145.0f + z}, {},
                                    {0.20f, 3.6f, 0.20f}), MaterialId::Wood);
    addObject(ScenePrimitive::Cube,
              makeTransform({-7.0f, 3.55f, -145.0f}, {}, {7.0f, 0.22f, 5.0f}),
              MaterialId::DarkWood);

    stats_.boats = ObjectEnrichment::boats().size();
    stats_.riverLandingObjects = objects_.size() - start;
}

void StaticGizaScene::buildUpperPlatformDetails()
{
    const std::size_t start = objects_.size();

    for (float x : {-10.5f, -8.5f, 8.5f, 10.5f})
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({x, 9.95f, -2.0f}, {}, {0.12f, 2.8f, 0.12f}),
                  MaterialId::Wood);
    addObject(ScenePrimitive::Cylinder,
              ConstructionAnimationController::cylinderBetween(
                  {-10.5f, 10.8f, -2.0f}, {-8.5f, 10.8f, -2.0f}, 0.045f),
              MaterialId::Rope);
    addObject(ScenePrimitive::Cylinder,
              ConstructionAnimationController::cylinderBetween(
                  {8.5f, 10.8f, -2.0f}, {10.5f, 10.8f, -2.0f}, 0.045f),
              MaterialId::Rope);

    for (int block = 0; block < 4; ++block)
        addObject(ScenePrimitive::Cube,
                  makeTransform({-14.0f + block * 2.75f, 9.25f, -4.5f}, {},
                                {2.45f, 1.35f, 2.30f}), MaterialId::PreparedStone);
    for (float x : {-6.0f, 6.0f})
        for (float z : {-3.8f, -1.0f})
            addObject(ScenePrimitive::Cube,
                      makeTransform({x, 8.82f, z}, {}, {0.30f, 0.35f, 3.2f}),
                      MaterialId::DarkWood);

    // Upper lever rack remains outside the x=0 animated arrival lane.
    for (float x : {-13.0f, -8.0f})
        addObject(ScenePrimitive::Cube,
                  makeTransform({x, 9.45f, 3.8f}, {}, {0.28f, 1.8f, 2.0f}),
                  MaterialId::DarkWood);
    for (int lever = 0; lever < 5; ++lever)
        addObject(ScenePrimitive::Cylinder,
                  makeTransform({-10.5f, 9.15f + lever * 0.25f,
                                 3.25f + (lever % 2) * 1.1f},
                                {0.0f, 0.0f, 90.0f}, {0.16f, 5.4f, 0.16f}),
                  MaterialId::Wood);

    stats_.upperPlatformObjects = objects_.size() - start;
}

void StaticGizaScene::buildCompositeObjects()
{
    // The first seven entries retain the exact Phase 5 role order.
    addWorker({7.5f, 0.0f, 35.0f}, 0.0f, WorkerPose::PullingReady,
              MaterialId::ClothingLinen, true, false);
    addWorker({12.0f, 0.0f, 35.0f}, 0.0f, WorkerPose::PullingReady,
              MaterialId::ClothingBlue, true, false);
    addWorker({-123.0f, -7.42f, -16.0f}, -80.0f, WorkerPose::Standing,
              MaterialId::ClothingBlue, true, false);
    addWorker({-108.0f, -7.42f, -3.0f}, 155.0f, WorkerPose::CarryingReady,
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
        {{-143.0f, -7.42f, -17.0f}, 45.0f, WorkerPose::BentKnees},
        {{-133.0f, -7.42f, -17.0f}, -70.0f, WorkerPose::Standing},
        {{-118.0f, -7.42f, -5.0f}, 90.0f, WorkerPose::BentKnees},
        {{-105.0f, -7.42f, 5.0f}, -90.0f, WorkerPose::CarryingReady},
        {{-82.0f, 0.0f, 20.0f}, 120.0f, WorkerPose::CarryingReady},
        {{-61.0f, 0.0f, 16.0f}, 20.0f, WorkerPose::Standing},
        {{-55.0f, 0.0f, 18.0f}, -60.0f, WorkerPose::BentKnees},
        {{-34.0f, 0.0f, 45.0f}, 170.0f, WorkerPose::CarryingReady},
        {{-13.0f, 0.5f, 43.0f}, 90.0f, WorkerPose::LeverReady},
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
        {{-150.0f, -5.55f, -10.0f}, 120.0f, WorkerPose::Standing},
        {{-142.0f, -3.45f, -35.0f}, -20.0f, WorkerPose::CarryingReady},
        {{-112.0f, -5.55f, -28.0f}, 70.0f, WorkerPose::Standing},
        {{-91.0f, 0.0f, 18.0f}, 160.0f, WorkerPose::CarryingReady},
        {{-76.0f, 0.0f, 6.0f}, -30.0f, WorkerPose::Standing},
        {{-58.0f, 0.0f, 34.0f}, 120.0f, WorkerPose::CarryingReady},
        {{-30.0f, 0.0f, 52.0f}, 170.0f, WorkerPose::Standing},
        {{-8.0f, 0.5f, 48.0f}, -90.0f, WorkerPose::CarryingReady},
        {{0.0f, 4.35f, 15.0f}, 180.0f, WorkerPose::PullingReady},
        {{-25.0f, 0.0f, 3.2f}, 0.0f, WorkerPose::Standing},
        {{13.0f, 0.0f, 3.2f}, 180.0f, WorkerPose::ArmsOut},
        {{6.0f, 8.70f, 0.2f}, 180.0f, WorkerPose::Standing},
        {{50.0f, 0.0f, 11.0f}, -120.0f, WorkerPose::CarryingReady},
        {{49.0f, 0.0f, -22.0f}, 45.0f, WorkerPose::Standing},
        {{34.0f, 0.0f, -13.0f}, -110.0f, WorkerPose::Standing},
        {{62.0f, 0.0f, 7.0f}, 80.0f, WorkerPose::BentKnees},
        {{-33.0f, 0.20f, -149.0f}, 160.0f, WorkerPose::CarryingReady},
        {{-14.0f, 0.20f, -148.0f}, -30.0f, WorkerPose::Standing}
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
    addStaticSledge({-108.0f, -7.40f, -3.5f}, -35.0f, 1.05f);
    addStaticSledge({-82.0f, 0.0f, 21.0f}, 15.0f, 1.0f);
    addComposite(makeTransform({-42.0f, 0.0f, 33.0f}, {0.0f, -25.0f, 0.0f},
                               {1.0f, 1.0f, 1.0f}), Sledge::create(true));
    ++stats_.sledgeInstances;
    addStaticSledge({-10.0f, 0.48f, 42.0f}, 0.0f, 1.0f);
    addStaticSledge({-55.0f, 0.0f, 42.0f}, 70.0f, 0.95f);

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

glm::vec3 StaticGizaScene::transportTarget() const
{
    if (!coordinatedAnimationEnabled_)
        return {10.0f, 0.0f, 40.0f};
    const glm::mat4 root = animationController_.snapshot().loadedSledgeRoot;
    return glm::vec3{root[3]};
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
