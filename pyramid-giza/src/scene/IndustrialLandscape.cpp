#include "scene/IndustrialLandscape.h"

#include <cmath>
#include <cstring>
#include <ostream>

#include "objects/Scaffold.h"
#include "objects/Sledge.h"
#include "objects/Worker.h"
#include "scene/MonumentalSite.h"
#include "scene/PyramidLayout.h"

namespace
{
bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

const RampDescriptor* findRamp(const char* id)
{
    for (const RampDescriptor& ramp : MonumentalSite::ramps())
        if (std::strcmp(ramp.id, id) == 0)
            return &ramp;
    return nullptr;
}
} // namespace

const QuarryConfig& IndustrialLandscape::quarry()
{
    static const QuarryConfig value;
    return value;
}

const std::vector<QuarryTerraceDescriptor>& IndustrialLandscape::terraces()
{
    static const std::vector<QuarryTerraceDescriptor> values{
        {"UpperBench", -1.5f, 0.0f, "Quarry rim and initial cutting"},
        {"MiddleBench", -3.7f, 6.0f, "Intermediate extraction level"},
        {"LowerBench", -5.7f, 12.0f, "Active lower working level"}
    };
    return values;
}

const std::vector<ExtractionBayDescriptor>& IndustrialLandscape::extractionBays()
{
    static const std::vector<ExtractionBayDescriptor> values{
        {"BayA", {-143.0f, -6.75f, -22.0f}, "Marked bedrock"},
        {"BayB", {-133.0f, -6.75f, -22.0f}, "Channel-cut block"},
        {"BayC", {-123.0f, -6.55f, -22.0f}, "Lever-ready block"},
        {"BayD", {-113.0f, -7.20f, -22.0f}, "Removed-block cavity"}
    };
    return values;
}

const std::vector<RepositoryDescriptor>& IndustrialLandscape::repositories()
{
    static const std::vector<RepositoryDescriptor> values{
        {"RoughDepot", "Rough stone", {-82.0f, 0.0f, 12.0f},
         6, 8, 1, {2.8f, 1.65f, 2.7f}, 0.55f, MaterialId::QuarryStone},
        {"PartialDepot", "Partially dressed", {-58.0f, 0.0f, 26.0f},
         3, 6, 1, {2.7f, 1.55f, 2.6f}, 0.50f, MaterialId::LimestoneVariation},
        {"FinishedDepot", "Finished stone", {-34.0f, 0.0f, 35.0f},
         5, 6, 2, {2.6f, 1.35f, 2.45f}, 0.42f, MaterialId::PreparedStone}
    };
    return values;
}

const std::vector<LogisticsStageDescriptor>& IndustrialLandscape::logisticsStages()
{
    static const std::vector<LogisticsStageDescriptor> values{
        {"Bedrock", 0, {-128.0f, -6.8f, -22.0f}},
        {"QuarryFloorStaging", 1, {-108.0f, -6.2f, -5.0f}},
        {"QuarryExit", 2, {-91.0f, 0.4f, 9.0f}},
        {"RoughRepository", 3, {-82.0f, 0.0f, 12.0f}},
        {"DressingYard", 4, {-58.0f, 0.0f, 26.0f}},
        {"FinishedRepository", 5, {-34.0f, 0.0f, 35.0f}},
        {"LoadingStation", 6, {-10.0f, 0.0f, 42.0f}},
        {"PyramidStaging", 7, {0.0f, 0.0f, 45.0f}},
        {"UpperPlacement", 8, {0.0f, 8.55f, -0.8f}}
    };
    return values;
}

const HeavyLiftingRigDescriptor& IndustrialLandscape::liftingRig()
{
    static const HeavyLiftingRigDescriptor value{{-91.0f, 0.0f, 22.0f}, 6.5f, 6.2f, 3.4f};
    return value;
}

const EnvironmentalContext& IndustrialLandscape::environment()
{
    static const EnvironmentalContext value;
    return value;
}

const std::vector<glm::vec3>& IndustrialLandscape::treePositions()
{
    static const std::vector<glm::vec3> values{
        {-145.0f, 0.0f, -143.0f}, {-126.0f, 0.0f, -149.0f},
        {-104.0f, 0.0f, -139.0f}, {-82.0f, 0.0f, -151.0f},
        {-58.0f, 0.0f, -142.0f}, {-34.0f, 0.0f, -148.0f},
        {-8.0f, 0.0f, -140.0f}, {18.0f, 0.0f, -150.0f},
        {43.0f, 0.0f, -141.0f}, {68.0f, 0.0f, -149.0f},
        {94.0f, 0.0f, -140.0f}, {121.0f, 0.0f, -147.0f},
        {142.0f, 0.0f, -138.0f}, {55.0f, 0.0f, -31.0f},
        {61.0f, 0.0f, -22.0f}, {47.0f, 0.0f, -18.0f},
        {40.0f, 0.0f, -30.0f}, {66.0f, 0.0f, -35.0f}
    };
    return values;
}

std::size_t IndustrialLandscape::repositoryBlockCount(
    const RepositoryDescriptor& repository)
{
    return static_cast<std::size_t>(repository.rows) * repository.columns * repository.levels;
}

bool validateIndustrialLandscape(std::ostream& output)
{
    const QuarryConfig& quarryConfig = IndustrialLandscape::quarry();
    const WorldScale& world = MonumentalSite::scale();
    const glm::vec2 pyramidCenter{world.pyramidOrigin.x, world.pyramidOrigin.z};
    const glm::vec2 quarryCenter{quarryConfig.center.x, quarryConfig.center.z};
    const float quarryDistance = glm::distance(pyramidCenter, quarryCenter);

    bool quarryValid = quarryConfig.excavationDepth > 0.0f &&
                       quarryConfig.width > quarryConfig.floorWidth &&
                       quarryConfig.depth > quarryConfig.floorDepth &&
                       quarryDistance >= 100.0f && quarryDistance > 72.0f;
    float previousElevation = 1.0f;
    for (const QuarryTerraceDescriptor& terrace : IndustrialLandscape::terraces())
    {
        quarryValid = quarryValid && std::isfinite(terrace.elevation) &&
                       terrace.elevation < previousElevation && terrace.inset >= 0.0f;
        previousElevation = terrace.elevation;
    }
    for (const ExtractionBayDescriptor& bay : IndustrialLandscape::extractionBays())
        quarryValid = quarryValid && finiteVector(bay.center) &&
                      bay.center.y < 0.0f;

    const RampDescriptor* quarryRamp = findRamp("QuarryExitRamp");
    quarryValid = quarryValid && quarryRamp != nullptr;
    if (quarryRamp != nullptr)
        quarryValid = quarryValid && glm::distance(quarryRamp->base, quarryRamp->top) > 1.0f &&
                      quarryRamp->width > 2.0f * Worker::approximateHeight() &&
                      isFiniteNonSingularTransform(MonumentalSite::rampModel(*quarryRamp));

    bool repositoriesValid = IndustrialLandscape::repositories().size() == 3;
    std::size_t repositoryBlocks = 0;
    for (const RepositoryDescriptor& repository : IndustrialLandscape::repositories())
    {
        const std::size_t count = IndustrialLandscape::repositoryBlockCount(repository);
        repositoriesValid = repositoriesValid && finiteVector(repository.center) &&
                            count > 0 && glm::all(glm::greaterThan(
                                repository.blockScale, glm::vec3{0.0f}));
        repositoryBlocks += count;
    }
    repositoriesValid = repositoriesValid && repositoryBlocks == 126;

    bool logisticsValid = IndustrialLandscape::logisticsStages().size() == 9;
    for (std::size_t index = 0; index < IndustrialLandscape::logisticsStages().size(); ++index)
    {
        const LogisticsStageDescriptor& stage = IndustrialLandscape::logisticsStages()[index];
        logisticsValid = logisticsValid && stage.order == index && finiteVector(stage.center);
        if (index > 0)
            logisticsValid = logisticsValid &&
                             glm::distance(stage.center,
                                           IndustrialLandscape::logisticsStages()[index - 1].center) > 1.0f;
    }

    const EnvironmentalContext& context = IndustrialLandscape::environment();
    const float nileDistance = glm::distance(
        pyramidCenter, glm::vec2{context.nileCenter.x, context.nileCenter.z});
    bool environmentValid = nileDistance > 100.0f &&
                            context.nileSize.x > 100.0f && context.nileSize.y > 10.0f &&
                            context.floodplainSize.x > context.nileSize.x &&
                            glm::distance(glm::vec2{context.sphinxCenter.x, context.sphinxCenter.z},
                                          pyramidCenter) > 50.0f;
    for (const glm::vec3& tree : IndustrialLandscape::treePositions())
    {
        const bool insideQuarry =
            std::abs(tree.x - quarryConfig.center.x) < quarryConfig.width * 0.5f &&
            std::abs(tree.z - quarryConfig.center.z) < quarryConfig.depth * 0.5f;
        environmentValid = environmentValid && finiteVector(tree) && !insideQuarry;
    }
    environmentValid = environmentValid && IndustrialLandscape::treePositions().size() >= 8 &&
                       IndustrialLandscape::treePositions().size() <= 25;

    const HeavyLiftingRigDescriptor& rig = IndustrialLandscape::liftingRig();
    const bool rigValid = finiteVector(rig.center) && rig.width > 0.0f &&
                          rig.height > 0.0f && rig.depth > 0.0f;
    const bool valid = quarryValid && repositoriesValid && logisticsValid &&
                       environmentValid && rigValid;

    output << "Phase 5.6 industrial-landscape validation\n"
           << "  quarry distance: " << quarryDistance << "\n"
           << "  quarry depth: " << quarryConfig.excavationDepth << "\n"
           << "  terraces: " << IndustrialLandscape::terraces().size() << "\n"
           << "  extraction bays: " << IndustrialLandscape::extractionBays().size() << "\n"
           << "  repository blocks: " << repositoryBlocks << "\n"
           << "  logistics stages: " << IndustrialLandscape::logisticsStages().size() << "\n"
           << "  trees: " << IndustrialLandscape::treePositions().size() << "\n"
           << "  Nile distance: " << nileDistance << "\n"
           << (valid ? "Industrial-landscape checks passed.\n"
                     : "Industrial-landscape checks failed.\n");
    return valid;
}
