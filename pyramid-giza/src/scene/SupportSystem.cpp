#include "scene/SupportSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionTimeline.h"
#include "objects/Scaffold.h"
#include "scene/ObjectEnrichment.h"
#include "scene/PyramidLayout.h"
#include "scene/SandSimulation.h"
#include "scene/SceneTypes.h"

namespace
{
constexpr int rampFillSegments = 64;
const SandSimulation* liveTerrain = nullptr;

bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

float pyramidSupportHeight(
    const PyramidBlockPlacement& block,
    const FrontierPlacementTransform& placement,
    const PyramidLayoutConfig& config,
    const std::vector<PyramidBlockPlacement>& blocks,
    const ConstructionTimelineController& timeline)
{
    if (block.level == 0u)
        return config.origin.y;

    const glm::vec3 center = block.position + placement.offset;
    const float bottom = center.y - 0.5f * block.scale.y;
    float height = -std::numeric_limits<float>::infinity();
    for (const PyramidBlockPlacement& support : blocks)
    {
        if (support.level >= block.level)
            continue;
        const ConstructionBlockState state = timeline.blockState(support, config);
        if (!state.visible || state.frontier)
            continue;
        const float top = support.position.y + 0.5f * support.scale.y;
        if (top > bottom + SceneSupport::maximumContactGap)
            continue;
        const float overlapX =
            0.5f * (block.scale.x + support.scale.x) -
            std::abs(center.x - support.position.x);
        const float overlapZ =
            0.5f * (block.scale.z + support.scale.z) -
            std::abs(center.z - support.position.z);
        if (overlapX >= 0.35f && overlapZ >= 0.35f)
            height = std::max(height, top);
    }
    return height;
}

float ladderSupportHeight(const LadderDescriptor& ladder)
{
    const std::string id = ladder.id;
    if (id == "QuarryLower") return -7.45f;
    if (id == "QuarryUpper") return -5.55f;
    if (id == "LandingAccess") return 0.08f;
    return 0.0f;
}

bool gapValid(float gap, float minimum = SceneSupport::minimumContactGap,
              float maximum = SceneSupport::maximumContactGap)
{
    return std::isfinite(gap) && gap >= minimum && gap <= maximum;
}
} // namespace

namespace SceneSupport
{
bool stageActive(float progress, float minimum, float maximum)
{
    const float value = std::clamp(progress, 0.0f, 1.0f);
    return value >= minimum &&
           (value < maximum || (value >= 1.0f && maximum > 1.0f));
}

float transformedBottomY(const glm::mat4& model,
                         const glm::vec3& canonicalHalfExtents)
{
    float bottom = std::numeric_limits<float>::infinity();
    for (float x : {-canonicalHalfExtents.x, canonicalHalfExtents.x})
        for (float y : {-canonicalHalfExtents.y, canonicalHalfExtents.y})
            for (float z : {-canonicalHalfExtents.z, canonicalHalfExtents.z})
            {
                const glm::vec4 point = model * glm::vec4{x, y, z, 1.0f};
                bottom = std::min(bottom, point.y);
            }
    return bottom;
}

bool footprintContains(const HorizontalFootprint& support,
                       const glm::vec2& point, float margin)
{
    const float yaw = glm::radians(-support.yawDegrees);
    const glm::vec2 delta = point - support.center;
    const glm::vec2 local{
        std::cos(yaw) * delta.x - std::sin(yaw) * delta.y,
        std::sin(yaw) * delta.x + std::cos(yaw) * delta.y};
    return std::abs(local.x) <= support.halfExtents.x + margin &&
           std::abs(local.y) <= support.halfExtents.y + margin;
}

SupportSurface terrainAt(const glm::vec2& point)
{
    const float height = liveTerrain
        ? liveTerrain->terrainHeightAt(point.x, point.y)
        : SandSimulation::staticTerrainHeightAt(point.x, point.y);
    const glm::vec3 normal = liveTerrain
        ? liveTerrain->terrainNormalAt(point.x, point.y)
        : SandSimulation::staticTerrainNormalAt(point.x, point.y);
    if (point.x < -112.0f && point.y > -30.0f && point.y < 3.0f)
        return {"QuarryFloor", SupportSurfaceKind::QuarryFloor,
                height, normal, true};
    return {"DesertGround", SupportSurfaceKind::DesertGround,
            height, normal, true};
}

RampDescriptor transportRampToe(const RampDescriptor& ramp)
{
    const glm::vec3 surface = MonumentalSite::rampSurfacePoint(ramp, 0.0f);
    RampDescriptor toe{"TransportRampToe", {0.0f, 0.04f, 52.0f},
        {surface.x, surface.y - 0.04f, surface.z - 0.10f}, ramp.width, 0.08f,
        MaterialId::RampEarth, false};
    toe.minimumProgress = ramp.minimumProgress;
    toe.maximumProgress = ramp.maximumProgress;
    return toe;
}

SupportSurface transportAt(const glm::vec2& point, float constructionProgress)
{
    const SupportSurface terrain = terrainAt(point);
    SupportSurface explicitSurface;
    const auto topFace = [&](const RampDescriptor& descriptor, SupportSurfaceKind kind)
    {
        const RampFrame frame = MonumentalSite::rampFrame(descriptor);
        const glm::vec3 center = MonumentalSite::rampSurfacePoint(descriptor, 0.5f);
        const float height = center.y -
            (frame.up.x * (point.x - center.x) +
             frame.up.z * (point.y - center.z)) / frame.up.y;
        const glm::vec3 offset{point.x - center.x, height - center.y, point.y - center.z};
        if (std::abs(glm::dot(offset, frame.right)) > 0.5f * descriptor.width ||
            std::abs(glm::dot(offset, frame.forward)) > 0.5f * frame.length ||
            height < terrain.height - 0.001f)
            return;
        if (!explicitSurface.valid || height > explicitSurface.height)
            explicitSurface = {descriptor.id, kind, height, frame.up, true};
    };
    for (const RampDescriptor& ramp : MonumentalSite::ramps())
        if (MonumentalSite::rampActive(ramp, constructionProgress))
        {
            topFace(ramp, SupportSurfaceKind::RampSurface);
            if (std::string(ramp.id).find("MainHauling") == 0)
            {
                topFace(transportRampToe(ramp), SupportSurfaceKind::RampSurface);
                // The visible transverse timbers also carry the runners.
                const RampFrame frame = MonumentalSite::rampFrame(ramp);
                const glm::vec2 along = glm::normalize(glm::vec2{frame.forward.x, frame.forward.z});
                for (int sleeper = 1; sleeper < 11; ++sleeper)
                {
                    const glm::vec3 center = MonumentalSite::rampSurfacePoint(ramp, sleeper / 11.0f);
                    const glm::vec2 delta = point - glm::vec2{center.x, center.z};
                    const float radial = glm::dot(delta, along);
                    const float side = glm::dot(delta, glm::vec2{frame.right.x, frame.right.z});
                    if (std::abs(radial) <= 0.11f && std::abs(side) <= 0.5f * ramp.width - 0.35f)
                    {
                        const float height = center.y + std::sqrt(std::max(0.0f, 0.0121f - radial * radial));
                        if (height >= terrain.height && (!explicitSurface.valid || height > explicitSurface.height))
                            explicitSurface = {ramp.id, SupportSurfaceKind::RampSurface, height,
                                {0.0f, 1.0f, 0.0f}, true};
                    }
                }
            }
        }
    const auto platform = [&](const char* id, glm::vec2 center, glm::vec2 half, float height,
                              SupportSurfaceKind kind)
    {
        if (footprintContains({center, half, 0.0f}, point) &&
            height >= terrain.height - 0.001f &&
            (!explicitSurface.valid || height > explicitSurface.height))
            explicitSurface = {id, kind, height, {0.0f, 1.0f, 0.0f}, true};
    };
    platform("QuarryWorkFloor", {-128.0f, -15.0f}, {21.0f, 17.0f}, -7.45f,
             SupportSurfaceKind::QuarryFloor);
    platform("QuarryPitBase", {-128.0f, -15.0f}, {30.0f, 32.0f}, -7.48f,
             SupportSurfaceKind::QuarryFloor);
    platform("QuarryStagingDeck", {-109.0f, 8.5f}, {5.0f, 2.5f}, -6.80f,
             SupportSurfaceKind::WorkPlatform);
    platform("LoadingDeck", {-10.0f, 42.0f}, {7.0f, 4.5f}, 0.44f,
             SupportSurfaceKind::WorkPlatform);
    platform("PulleyReceivingDeck", {-111.0f, -10.0f}, {2.8f, 2.6f}, -2.60f,
             SupportSurfaceKind::WorkPlatform);
    platform("QuarryExitDeckLip", {-107.9f, -9.8f}, {0.3f, 1.8f}, -2.60f,
             SupportSurfaceKind::WorkPlatform);
    if (SceneSupport::stageActive(constructionProgress, 0.62f, 0.90f))
        for (const UpperWorkDeckPanel& panel : MonumentalSite::upperWorkDeckPanels())
            platform("UpperWorkDeck", {panel.center.x, panel.center.z},
                     {0.5f * panel.size.x, 0.5f * panel.size.z},
                     panel.center.y + 0.5f * panel.size.y,
                     SupportSurfaceKind::WorkPlatform);
    if (explicitSurface.valid)
        return explicitSurface;
    const RampDescriptor quarryRoad{
        "QuarryHaulRoad", {-91.0f, 0.04f, 9.0f}, {-10.0f, 0.04f, 42.0f},
        8.0f, 0.08f, MaterialId::RampEarth, false};
    const RampDescriptor loadingRoad{
        "LoadingToRamp", {-10.0f, 0.045f, 42.0f}, {0.0f, 0.045f, 45.0f},
        8.0f, 0.09f, MaterialId::RampEarth, false};
    topFace(quarryRoad, SupportSurfaceKind::Road);
    topFace(loadingRoad, SupportSurfaceKind::Road);
    return explicitSurface.valid ? explicitSurface : terrain;
}

void setTerrainSource(const SandSimulation* terrain)
{
    liveTerrain = terrain;
}

float rampUndersideY(const RampDescriptor& ramp, float rampProgress)
{
    const float t = std::clamp(rampProgress, 0.0f, 1.0f);
    return (glm::mix(ramp.base, ramp.top, t) -
            MonumentalSite::rampFrame(ramp).up *
                (0.5f * ramp.thickness)).y;
}

SupportSurface rampFillAt(const RampDescriptor& ramp, float rampProgress)
{
    const int segments = std::string(ramp.id) == "MainLanding"
                             ? 1
                             : rampFillSegments;
    const float t = std::clamp(rampProgress, 0.0f, 1.0f);
    const int index = std::min(
        segments - 1, static_cast<int>(std::floor(t * segments)));
    const float center =
        (static_cast<float>(index) + 0.5f) / static_cast<float>(segments);
    return {ramp.id, SupportSurfaceKind::RampFill,
            rampUndersideY(ramp, center), {0.0f, 1.0f, 0.0f}, true};
}

float verticalGap(float objectBottom, const SupportSurface& support)
{
    return support.valid ? objectBottom - support.height
                         : std::numeric_limits<float>::infinity();
}

const std::vector<StageSupportDependency>& dependencies()
{
    static const std::vector<StageSupportDependency> values{
        {"MainHaulingLow", "MainHaulingLowFill", SupportCategory::StructureSupported,
         0.00f, 0.30f, 0.00f, 0.30f, "Stepped earth fill"},
        {"MainHaulingMiddle", "MainHaulingMiddleFill", SupportCategory::StructureSupported,
         0.30f, 0.62f, 0.30f, 0.62f, "Stepped earth fill"},
        {"MainHaulingRamp", "MainHaulingRampFill", SupportCategory::StructureSupported,
         0.62f, 0.90f, 0.62f, 0.90f, "Stepped earth fill"},
        {"MainLanding", "MainLandingFill", SupportCategory::StructureSupported,
         0.62f, 0.90f, 0.62f, 0.90f, "Earth-filled landing"},
        {"WestAccessRamp", "WestAccessRampFill", SupportCategory::StructureSupported,
         0.05f, 0.62f, 0.05f, 0.62f, "Stepped earth fill"},
        {"QuarryExitRamp", "QuarryExitRampFill", SupportCategory::StructureSupported,
         0.00f, 1.01f, 0.00f, 1.01f, "Quarry-floor earth fill"},
        {"FrontWestScaffold", "DesertGround", SupportCategory::GroundSupported,
         0.05f, 0.48f, 0.00f, 1.01f, "Ground-founded poles"},
        {"FrontEastScaffold", "DesertGround", SupportCategory::GroundSupported,
         0.20f, 0.62f, 0.00f, 1.01f, "Ground-founded poles"},
        {"RampTopScaffold", "DesertGround", SupportCategory::GroundSupported,
         0.62f, 0.90f, 0.00f, 1.01f, "Ground-founded poles"},
        {"FrontWestLadder", "FrontWestScaffold", SupportCategory::StructureSupported,
         0.05f, 0.48f, 0.05f, 0.48f, "Matches scaffold lifecycle"},
        {"FrontEastLadder", "FrontEastScaffold", SupportCategory::StructureSupported,
         0.20f, 0.62f, 0.20f, 0.62f, "Matches scaffold lifecycle"},
        {"RampTopLadder", "RampTopScaffold", SupportCategory::StructureSupported,
         0.62f, 0.90f, 0.62f, 0.90f, "Matches scaffold lifecycle"},
        {"WestAccessLadder", "WestAccessRamp", SupportCategory::StructureSupported,
         0.05f, 0.62f, 0.05f, 0.62f, "Ground foot and ramp-top access"},
        {"UpperWorkDeck", "UpperDeckPosts", SupportCategory::StructureSupported,
         0.62f, 0.90f, 0.62f, 0.90f, "Eight ground-founded posts"},
        {"UpperWorkProps", "UpperWorkDeck", SupportCategory::StructureSupported,
         0.62f, 0.90f, 0.62f, 0.90f, "Props appear only with deck"},
        {"FrontierBlocks", "PreviousPyramidCourse", SupportCategory::StructureSupported,
         0.00f, 1.01f, 0.00f, 1.01f, "Center overlap with stable course"},
        {"HeroRopes", "HeroSledgeAndWorkers", SupportCategory::IntentionallySuspended,
         0.00f, 1.01f, 0.00f, 1.01f, "Endpoint-driven rope visualization"},
        {"PulleyRopes", "PulleyFrames", SupportCategory::IntentionallySuspended,
         0.00f, 1.01f, 0.00f, 1.01f, "Speculative static rope redirection"},
        {"QuarryPulleyFrame", "QuarryFloor", SupportCategory::GroundSupported,
         0.00f, 1.01f, 0.00f, 1.01f, "Four floor-founded gantry posts"},
        {"QuarryPulleyPlatform", "QuarryPulleyPlatformPosts",
         SupportCategory::StructureSupported,
         0.00f, 1.01f, 0.00f, 1.01f, "Four floor-founded receiving posts"},
        {"QuarryPulleyTrolley", "QuarryPulleyFrame",
         SupportCategory::StructureSupported,
         0.00f, 1.01f, 0.00f, 1.01f, "Carriage rides on twin top rails"},
        {"QuarryPulleyLoad", "ConditionalFloorRopePlatform",
         SupportCategory::IntentionallySuspended,
         0.00f, 0.90f, 0.00f, 1.01f,
         "Floor -> rope suspension -> destination platform"},
        {"LeverStone", "LeverRig", SupportCategory::IntentionallySuspended,
         0.00f, 1.01f, 0.00f, 1.01f, "Deliberate animated lifting interval"}
    };
    return values;
}

const char* categoryName(SupportCategory category)
{
    switch (category)
    {
    case SupportCategory::GroundSupported: return "ground-supported";
    case SupportCategory::StructureSupported: return "structure-supported";
    case SupportCategory::IntentionallySuspended: return "intentionally suspended";
    default: return "unknown";
    }
}

const char* surfaceKindName(SupportSurfaceKind kind)
{
    switch (kind)
    {
    case SupportSurfaceKind::DesertGround: return "desert ground";
    case SupportSurfaceKind::QuarryFloor: return "quarry floor";
    case SupportSurfaceKind::PyramidCourse: return "pyramid course";
    case SupportSurfaceKind::RampFill: return "ramp fill";
    case SupportSurfaceKind::ScaffoldPlatform: return "scaffold platform";
    case SupportSurfaceKind::WorkPlatform: return "work platform";
    case SupportSurfaceKind::Sledge: return "sledge";
    case SupportSurfaceKind::LiftingRig: return "lifting rig";
    case SupportSurfaceKind::Road: return "road";
    case SupportSurfaceKind::RampSurface: return "ramp surface";
    default: return "none";
    }
}
} // namespace SceneSupport

bool validatePhase12_6Supports(std::ostream& output)
{
    const std::array<float, 5> checkpoints{{0.0f, 0.25f, 0.50f, 0.75f, 1.0f}};
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> blocks =
        PyramidLayout::generateComplete(config);
    bool valid = true;

    output << "Phase 12.6 support-surface validation\n";
    for (float progress : checkpoints)
    {
        std::size_t checked = 0;
        std::size_t unsupported = 0;
        float worstGap = 0.0f;

        for (const RampDescriptor& ramp : MonumentalSite::ramps())
        {
            if (!SceneSupport::stageActive(progress, ramp.minimumProgress,
                                           ramp.maximumProgress))
                continue;
            for (float sample : {0.0f, 0.25f, 0.50f, 0.75f, 1.0f})
            {
                const SupportSurface support =
                    SceneSupport::rampFillAt(ramp, sample);
                const float gap = SceneSupport::rampUndersideY(ramp, sample) -
                                  support.height;
                worstGap = std::max(worstGap, std::abs(gap));
                ++checked;
                if (!gapValid(gap))
                    ++unsupported;
            }
        }

        ConstructionTimelineController timeline;
        timeline.setProgress(progress);
        for (const PyramidBlockPlacement& block : blocks)
        {
            const ConstructionBlockState state =
                timeline.blockState(block, config);
            if (!state.frontier)
                continue;
            const FrontierPlacementTransform placement =
                ConstructionTimelineController::frontierTransform(
                    block, config, state.placementAmount);
            const float bottom =
                block.position.y + placement.offset.y - 0.5f * block.scale.y;
            const float support = pyramidSupportHeight(
                block, placement, config, blocks, timeline);
            const float gap = bottom - support;
            worstGap = std::max(worstGap, std::abs(gap));
            ++checked;
            if (!std::isfinite(support) || !gapValid(gap))
            {
                ++unsupported;
                output << "    frontier L" << block.level << " ["
                       << block.gridX << ',' << block.gridZ << "] amount="
                       << state.placementAmount << " phase="
                       << ConstructionTimelineController::frontierPhaseName(
                              placement.phase)
                       << " gap=" << gap << '\n';
            }
        }

        for (const ScaffoldPlacement& scaffold : MonumentalSite::scaffolds())
            if (SceneSupport::stageActive(progress, scaffold.minimumProgress,
                                          scaffold.maximumProgress))
            {
                ++checked;
                if (!gapValid(scaffold.origin.y))
                    ++unsupported;
            }

        valid = valid && unsupported == 0u;
        output << std::fixed << std::setprecision(3)
               << "  progress " << progress << ": checked=" << checked
               << ", unsupported=" << unsupported
               << ", worstAbsGap=" << worstGap
               << (unsupported == 0u ? " PASS\n" : " FAIL\n");
    }

    const glm::mat4 transformed = makeTransform(
        {2.0f, 3.0f, -1.0f}, {17.0f, 32.0f, -11.0f}, {2.0f, 4.0f, 3.0f});
    valid = valid && std::isfinite(SceneSupport::transformedBottomY(transformed));
    const HorizontalFootprint footprint{{3.0f, -2.0f}, {4.0f, 2.0f}, 25.0f};
    valid = valid && SceneSupport::footprintContains(footprint, {3.0f, -2.0f}) &&
            !SceneSupport::footprintContains(footprint, {20.0f, 20.0f});

    const SupportSurface road = SceneSupport::transportAt({-50.5f, 25.5f});
    const SupportSurface openSand = SceneSupport::transportAt({-70.0f, 50.0f});
    const SupportSurface loading = SceneSupport::transportAt({-10.0f, 42.0f});
    const RampDescriptor* lowRamp = MonumentalSite::findRamp("MainHaulingLow");
    const glm::vec3 rampPoint = MonumentalSite::rampSurfacePoint(*lowRamp, 0.4f);
    const SupportSurface rampTop = SceneSupport::transportAt({rampPoint.x, rampPoint.z});
    const bool actualTransportSurfaces = road.kind == SupportSurfaceKind::Road &&
        std::abs(road.height - 0.08f) < 1.0e-5f &&
        openSand.kind == SupportSurfaceKind::DesertGround &&
        loading.kind == SupportSurfaceKind::WorkPlatform &&
        std::abs(loading.height - 0.44f) < 1.0e-5f &&
        rampTop.kind == SupportSurfaceKind::RampSurface &&
        std::abs(rampTop.height - rampPoint.y) < 1.0e-5f;
    valid = valid && actualTransportSurfaces;
    output << "  transport support follows visible top faces: "
           << (actualTransportSurfaces ? "PASS\n" : "FAIL\n");

    output << (valid ? "Support-surface checks passed.\n"
                     : "Support-surface checks failed.\n");
    return valid;
}

bool validatePhase12_6Grounding(std::ostream& output)
{
    std::size_t checked = 0;
    std::size_t unsupported = 0;
    std::size_t excessiveGap = 0;
    std::size_t excessivePenetration = 0;

    for (const LadderDescriptor& ladder : ObjectEnrichment::ladders())
    {
        const float gap = ladder.base.y - ladderSupportHeight(ladder);
        ++checked;
        if (!finiteVector(ladder.base) || !gapValid(gap, -0.08f, 0.11f))
        {
            ++unsupported;
            if (gap > 0.11f) ++excessiveGap;
            if (gap < -0.08f) ++excessivePenetration;
        }
    }

    for (const RampDescriptor& ramp : MonumentalSite::ramps())
        for (float sample : {0.0f, 0.25f, 0.50f, 0.75f, 1.0f})
        {
            const float gap = SceneSupport::rampUndersideY(ramp, sample) -
                              SceneSupport::rampFillAt(ramp, sample).height;
            ++checked;
            if (!gapValid(gap))
            {
                ++unsupported;
                if (gap > SceneSupport::maximumContactGap) ++excessiveGap;
                if (gap < SceneSupport::minimumContactGap) ++excessivePenetration;
            }
        }

    // Ground staging blocks are 1.35 units high with centers at y=0.675.
    for (int queue = 0; queue < 3; ++queue)
    {
        const glm::mat4 model = makeTransform(
            {-8.0f - 3.2f * queue, 0.675f, 40.0f + 2.7f * queue}, {},
            {2.45f, 1.35f, 2.30f});
        const float gap = SceneSupport::transformedBottomY(model);
        ++checked;
        if (!gapValid(gap))
            ++unsupported;
    }

    // Upper deck bottom/post top, stone bottom/deck top and rail bottom/deck top.
    const std::array<float, 3> upperGaps{{8.20f - 8.20f,
                                          8.575f - 8.55f,
                                          8.55f - 8.55f}};
    for (float gap : upperGaps)
    {
        ++checked;
        if (!gapValid(gap))
            ++unsupported;
    }

    const bool valid = unsupported == 0u;
    output << "Phase 12.6 grounding validation\n"
           << "  checked objects/samples: " << checked << '\n'
           << "  unsupported: " << unsupported << '\n'
           << "  excessive gap: " << excessiveGap << '\n'
           << "  excessive penetration: " << excessivePenetration << '\n'
           << (valid ? "Grounding checks passed.\n"
                     : "Grounding checks failed.\n");
    return valid;
}

bool validatePhase12_6StageDependencies(std::ostream& output)
{
    std::size_t implications = 0;
    std::size_t violations = 0;
    for (const StageSupportDependency& dependency :
         SceneSupport::dependencies())
    {
        for (int sample = 0; sample <= 100; ++sample)
        {
            const float progress = static_cast<float>(sample) / 100.0f;
            if (!SceneSupport::stageActive(progress, dependency.objectMinimum,
                                           dependency.objectMaximum))
                continue;
            ++implications;
            if (!SceneSupport::stageActive(progress, dependency.supportMinimum,
                                           dependency.supportMaximum))
                ++violations;
        }
    }
    const bool valid = violations == 0u;
    output << "Phase 12.6 support-dependency validation\n"
           << "  dependencies: " << SceneSupport::dependencies().size() << '\n'
           << "  active implications checked: " << implications << '\n'
           << "  visibility violations: " << violations << '\n'
           << (valid ? "Stage dependency checks passed.\n"
                     : "Stage dependency checks failed.\n");
    return valid;
}
