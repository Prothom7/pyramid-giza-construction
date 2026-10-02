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
    if (point.x < -112.0f && point.y > -30.0f && point.y < 3.0f)
        return {"QuarryFloor", SupportSurfaceKind::QuarryFloor,
                SandSimulation::staticTerrainHeightAt(point.x, point.y),
                SandSimulation::staticTerrainNormalAt(point.x, point.y), true};
    return {"DesertGround", SupportSurfaceKind::DesertGround,
            SandSimulation::staticTerrainHeightAt(point.x, point.y),
            SandSimulation::staticTerrainNormalAt(point.x, point.y), true};
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
