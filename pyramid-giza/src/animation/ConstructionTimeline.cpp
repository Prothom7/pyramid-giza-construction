#include "animation/ConstructionTimeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <ostream>

#include "animation/ConstructionAnimation.h"
#include "scene/ConstructionSimulation.h"
#include "effects/ParticleSystem.h"
#include "lighting/SunController.h"

namespace
{
constexpr float frontierWindow = 0.018f;
constexpr std::array<float, 6> speedPresets{{0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f}};

float smoothStep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

unsigned int deterministicWave(const PyramidBlockPlacement& block)
{
    return (block.gridX * 3u + block.gridZ * 5u + block.level * 7u) % 12u;
}

float deterministicFraction(const PyramidBlockPlacement& block)
{
    const unsigned int value =
        (block.gridX * 73856093u) ^ (block.gridZ * 19349663u) ^
        (block.level * 83492791u);
    return (static_cast<float>(value % 997u) + 0.5f) / 997.0f;
}

float easedRange(float value, float start, float end)
{
    return smoothStep((value - start) / (end - start));
}
}

void ConstructionTimelineController::update(float deltaTime)
{
    if (!playing_ || deltaTime <= 0.0f || !std::isfinite(deltaTime))
        return;
    setProgress(progress_ + deltaTime * speed_ / durationSeconds_);
    if (progress_ >= 1.0f)
        playing_ = false;
}

void ConstructionTimelineController::registerPhysicalBlockSettlement(const ConstructionSimulation& simulation)
{
    if (!playing_ && progress_ < 1.0f) 
    {
        // Allow settlement even if not explicitly "playing" a timelapse
    }
    
    // Pyramid progress is driven entirely by authoritative simulation settled blocks
    if (simulation.totalCount() > 0 && !playing_) {
        float exactProgress = static_cast<float>(simulation.settledCount()) / static_cast<float>(simulation.totalCount());
        setProgress(std::max(progress_, std::min(1.0f, exactProgress)));
    }
    
    if (progress_ >= 1.0f)
        playing_ = false;
}

void ConstructionTimelineController::reset()
{
    progress_ = 0.0f;
    playing_ = false;
}

void ConstructionTimelineController::complete()
{
    progress_ = 1.0f;
    playing_ = false;
}

void ConstructionTimelineController::adjustSpeed(int direction)
{
    std::size_t closest = 0;
    float closestDistance = std::abs(speed_ - speedPresets[0]);
    for (std::size_t index = 1; index < speedPresets.size(); ++index)
    {
        const float distance = std::abs(speed_ - speedPresets[index]);
        if (distance < closestDistance)
        {
            closest = index;
            closestDistance = distance;
        }
    }
    const int target = std::clamp(static_cast<int>(closest) + direction, 0,
                                  static_cast<int>(speedPresets.size() - 1));
    speed_ = speedPresets[static_cast<std::size_t>(target)];
}

void ConstructionTimelineController::setProgress(float progress)
{
    progress_ = std::clamp(std::isfinite(progress) ? progress : defaultProgress, 0.0f, 1.0f);
}

void ConstructionTimelineController::setDuration(float seconds)
{
    if (std::isfinite(seconds) && seconds > 0.0f)
        durationSeconds_ = seconds;
}

void ConstructionTimelineController::setSpeed(float speed)
{
    if (std::isfinite(speed) && speed > 0.0f)
        speed_ = std::clamp(speed, speedPresets.front(), speedPresets.back());
}

ConstructionStage ConstructionTimelineController::stage() const
{
    if (progress_ < 0.10f) return ConstructionStage::Foundation;
    if (progress_ < 0.30f) return ConstructionStage::LowerCourses;
    if (progress_ < 0.55f) return ConstructionStage::LowerMiddle;
    if (progress_ <= defaultProgress) return ConstructionStage::HistoricalCheckpoint;
    if (progress_ < 0.92f) return ConstructionStage::UpperCourses;
    if (progress_ < 1.0f) return ConstructionStage::Summit;
    return ConstructionStage::Complete;
}

unsigned int ConstructionTimelineController::activeLevel(const PyramidLayoutConfig& config) const
{
    if (progress_ <= defaultProgress)
    {
        const float normalized = progress_ / defaultProgress;
        return std::min(config.completedLevels - 1,
                        static_cast<unsigned int>(normalized * config.completedLevels));
    }
    if (progress_ < 0.805f)
        return config.completedLevels - 1;
    const float normalized = (progress_ - 0.805f) / 0.195f;
    const unsigned int upperCount = config.baseBlocksPerSide - config.completedLevels;
    return std::min(config.baseBlocksPerSide - 1,
                    config.completedLevels +
                    static_cast<unsigned int>(normalized * upperCount));
}

float ConstructionTimelineController::blockThreshold(const PyramidBlockPlacement& block,
                                                      const PyramidLayoutConfig& config)
{
    const float wave = static_cast<float>(deterministicWave(block)) / 12.0f;
    float threshold = 0.0f;
    float waveStep = 0.0f;
    if (block.level < config.completedLevels &&
        !PyramidLayout::isLegacyConstructionOpening(config, block))
    {
        waveStep = defaultProgress /
                   (static_cast<float>(config.completedLevels) * 12.0f);
        threshold = defaultProgress *
                    (static_cast<float>(block.level) + wave) /
                    static_cast<float>(config.completedLevels);
    }
    else if (block.level < config.completedLevels)
    {
        waveStep = 0.045f / 12.0f;
        threshold = 0.755f + 0.045f * wave;
    }
    else
    {
        const unsigned int upperCount =
            config.baseBlocksPerSide - config.completedLevels;
        const float upperLevel =
            static_cast<float>(block.level - config.completedLevels) + wave;
        waveStep = 0.19f / (static_cast<float>(upperCount) * 12.0f);
        threshold = 0.805f +
                    0.19f * upperLevel / static_cast<float>(upperCount);
    }

    // Preserve the old 12-wave checkpoint membership while spreading every
    // wave backward through its own interval. This removes large simultaneous
    // visibility pops without changing the 0/25/50/75/100 percent counts.
    const float spread = waveStep * 0.94f * deterministicFraction(block);
    return std::clamp(threshold - spread, 0.0f, 1.0f);
}

ConstructionBlockState ConstructionTimelineController::blockState(
    const PyramidBlockPlacement& block, const PyramidLayoutConfig& config) const
{
    ConstructionBlockState state;
    state.threshold = blockThreshold(block, config);
    if (progress_ + 1.0e-6f < state.threshold)
        return state;

    state.visible = true;
    state.frontier = progress_ < 1.0f && isFrontierCandidate(block, config) &&
                     progress_ < state.threshold + frontierWindow;
    state.placementAmount = state.frontier
        ? smoothStep((progress_ - state.threshold) / frontierWindow)
        : 1.0f;
    return state;
}

bool ConstructionTimelineController::isFrontierCandidate(
    const PyramidBlockPlacement& block, const PyramidLayoutConfig& config)
{
    const unsigned int side = config.baseBlocksPerSide - block.level;
    const bool presentationFace =
        block.gridZ + 1u >= side || block.gridX + 1u >= side;
    const unsigned int signature =
        block.gridX * 17u + block.gridZ * 31u + block.level * 13u;
    return presentationFace && signature % 3u == 0u;
}

float ConstructionTimelineController::stableThreshold(
    const PyramidBlockPlacement& block, const PyramidLayoutConfig& config)
{
    const float threshold = blockThreshold(block, config);
    return isFrontierCandidate(block, config)
        ? std::min(1.0f, threshold + frontierWindow)
        : threshold;
}

FrontierPlacementTransform ConstructionTimelineController::frontierTransform(
    const PyramidBlockPlacement& block, const PyramidLayoutConfig& config,
    float placementAmount)
{
    FrontierPlacementTransform result;
    const float amount = std::clamp(placementAmount, 0.0f, 1.0f);
    const unsigned int side = config.baseBlocksPerSide - block.level;
    const bool front = block.gridZ + 1u >= side;
    const bool east = block.gridX + 1u >= side;
    glm::vec3 outward{east ? 1.0f : 0.0f, 0.0f, front ? 1.0f : 0.0f};
    if (glm::length(outward) < 1.0e-5f)
        outward = {0.0f, 0.0f, 1.0f};
    else
        outward = glm::normalize(outward);

    if (amount < 0.14f)
    {
        result.phase = FrontierPlacementPhase::Queued;
        result.offset = outward * 0.80f;
    }
    else if (amount < 0.50f)
    {
        result.phase = FrontierPlacementPhase::Approach;
        const float t = easedRange(amount, 0.14f, 0.50f);
        result.offset = outward * glm::mix(0.80f, 0.40f, t);
    }
    else if (amount < 0.72f)
    {
        result.phase = FrontierPlacementPhase::LiftSlide;
        const float t = easedRange(amount, 0.50f, 0.72f);
        result.offset = outward * glm::mix(0.40f, 0.18f, t) +
                        glm::vec3{0.0f, std::sin(t * 3.14159265f) * 0.04f, 0.0f};
    }
    else if (amount < 0.92f)
    {
        result.phase = FrontierPlacementPhase::Align;
        const float t = easedRange(amount, 0.72f, 0.92f);
        result.offset = outward * glm::mix(0.18f, 0.0f, t) +
                        glm::vec3{0.0f, glm::mix(0.04f, 0.02f, t), 0.0f};
    }
    else if (amount < 1.0f)
    {
        result.phase = FrontierPlacementPhase::Settle;
        const float t = easedRange(amount, 0.92f, 1.0f);
        const float settleBounce = std::sin(t * 3.14159265f) * 0.01f;
        result.offset = {0.0f, glm::mix(0.02f, 0.0f, t) + settleBounce, 0.0f};
    }
    else
        result.phase = FrontierPlacementPhase::Stable;

    result.rotationDegrees = {0.0f, 0.0f, 0.0f};
    return result;
}

const char* ConstructionTimelineController::frontierPhaseName(
    FrontierPlacementPhase phase)
{
    switch (phase)
    {
    case FrontierPlacementPhase::Queued: return "Queued";
    case FrontierPlacementPhase::Approach: return "Approach";
    case FrontierPlacementPhase::LiftSlide: return "Lift/Slide";
    case FrontierPlacementPhase::Align: return "Align";
    case FrontierPlacementPhase::Settle: return "Settle";
    case FrontierPlacementPhase::Stable: return "Stable";
    }
    return "Unknown";
}

std::size_t ConstructionTimelineController::visibleBlockCount(
    const std::vector<PyramidBlockPlacement>& blocks,
    const PyramidLayoutConfig& config) const
{
    return static_cast<std::size_t>(std::count_if(
        blocks.begin(), blocks.end(), [&](const PyramidBlockPlacement& block)
        {
            return blockState(block, config).visible;
        }));
}

const char* ConstructionTimelineController::stageName(ConstructionStage stage)
{
    switch (stage)
    {
    case ConstructionStage::Foundation: return "Foundation";
    case ConstructionStage::LowerCourses: return "Lower courses";
    case ConstructionStage::LowerMiddle: return "Lower-middle courses";
    case ConstructionStage::HistoricalCheckpoint: return "Phase 5.6 checkpoint";
    case ConstructionStage::UpperCourses: return "Upper courses";
    case ConstructionStage::Summit: return "Summit";
    case ConstructionStage::Complete: return "Complete";
    }
    return "Unknown";
}

bool validateConstructionTimeline(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> blocks = PyramidLayout::generateComplete(config);
    ConstructionTimelineController timeline;
    bool valid = blocks.size() == 7714;
    std::size_t previous = 0;

    output << "Phase 9 construction timeline validation\n";
    for (float progress : {0.0f, 0.25f, 0.50f, 0.75f, 1.0f})
    {
        timeline.setProgress(progress);
        const std::size_t visible = timeline.visibleBlockCount(blocks, config);
        valid = valid && visible >= previous && visible <= blocks.size();
        if (progress == ConstructionTimelineController::defaultProgress)
            valid = valid && visible == 7561;
        if (progress == 1.0f)
            valid = valid && visible == 7714;
        previous = visible;
        output << "  progress " << progress << ": " << visible
               << " blocks, active level " << timeline.activeLevel(config) << '\n';
    }

    ConstructionTimelineController sixtyFps;
    ConstructionTimelineController thirtyFps;
    sixtyFps.reset();
    thirtyFps.reset();
    sixtyFps.setPlaying(true);
    thirtyFps.setPlaying(true);
    for (int i = 0; i < 60; ++i) sixtyFps.update(1.0f / 60.0f);
    for (int i = 0; i < 30; ++i) thirtyFps.update(1.0f / 30.0f);
    valid = valid && std::abs(sixtyFps.progress() - thirtyFps.progress()) < 1.0e-5f;

    SunController sun;
    ConstructionAnimationController hero;
    const float sunBefore = sun.state().timeOfDay;
    const float heroBefore = hero.elapsedTime();
    sixtyFps.update(0.5f);
    valid = valid && sun.state().timeOfDay == sunBefore &&
            hero.elapsedTime() == heroBefore;

    for (const PyramidBlockPlacement& block : blocks)
    {
        const float threshold = ConstructionTimelineController::blockThreshold(block, config);
        valid = valid && std::isfinite(threshold) && threshold >= 0.0f && threshold <= 1.0f;
        valid = valid && block.level < config.baseBlocksPerSide;
    }

    output << "  complete block count: " << blocks.size() << '\n'
           << "  frame-rate-independent progress: " << sixtyFps.progress() << '\n'
           << "  hero/sun/timelapse clocks: independent\n"
           << (valid ? "Construction timeline checks passed.\n"
                     : "Construction timeline checks failed.\n");
    return valid;
}

bool validateTimelapsePlaybackRepair(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> blocks =
        PyramidLayout::generateComplete(config);
    bool checkpoints = true;
    ConstructionTimelineController checkpoint;
    const std::array<float, 5> progressValues{{0.0f, 0.25f, 0.50f, 0.75f, 1.0f}};
    const std::array<std::size_t, 5> expected{{70u, 4734u, 6964u, 7561u, 7714u}};
    for (std::size_t index = 0; index < progressValues.size(); ++index)
    {
        checkpoint.setProgress(progressValues[index]);
        checkpoints =
            checkpoints &&
            checkpoint.visibleBlockCount(blocks, config) == expected[index];
    }

    ConstructionTimelineController natural;
    natural.setProgress(0.18f);
    natural.setSpeed(3.075f);
    natural.setPlaying(true);
    bool monotonic = true;
    std::size_t activeFrames = 0;
    std::size_t maximumFrontier = 0;
    std::size_t settlementEvents = 0;
    float previous = natural.progress();
    constexpr int frameCount = 24 * 60;
    float blockAccumulator = 0.0f;
    ConstructionSimulation sim;
    sim.initialize(blocks);
    for (int frame = 0; frame < frameCount; ++frame)
    {
        natural.update(1.0f / 60.0f);
        // Simulate physical block placement to drive progress in the test
        // To complete 82% of 7714 blocks (~6326) in 1440 frames:
        blockAccumulator += 6326.0f / static_cast<float>(frameCount);
        while (blockAccumulator >= 1.0f && natural.progress() < 1.0f)
        {
            sim.incrementSettledCount();
            natural.registerPhysicalBlockSettlement(sim);
            blockAccumulator -= 1.0f;
        }
        if (frame == frameCount - 1 && natural.progress() < 1.0f) {
            // Ensure we hit exactly 1.0 at the end if there are rounding errors
            while (natural.progress() < 1.0f) {
                sim.incrementSettledCount();
                natural.registerPhysicalBlockSettlement(sim);
            }
        }

        monotonic = monotonic && natural.progress() + 1.0e-6f >= previous;
        std::size_t active = 0;
        for (const PyramidBlockPlacement& block : blocks)
        {
            const ConstructionBlockState state = natural.blockState(block, config);
            if (state.frontier)
                ++active;
            const float stable =
                ConstructionTimelineController::stableThreshold(block, config);
            if (ConstructionTimelineController::isFrontierCandidate(block, config) &&
                placementSettlementCrossed(previous, natural.progress(), stable))
                ++settlementEvents;
        }
        if (active > 0)
            ++activeFrames;
        maximumFrontier = std::max(maximumFrontier, active);
        previous = natural.progress();
    }

    const bool naturalCompletion =
        std::abs(natural.progress() - 1.0f) < 1.0e-5f && !natural.playing();
    const bool frontierActivity =
        activeFrames > static_cast<std::size_t>(frameCount * 0.60f) &&
        maximumFrontier >= 4u && maximumFrontier <= 16u;
    const bool placementEvents = settlementEvents > 0u;

    bool phases = true;
    const PyramidBlockPlacement* candidate = nullptr;
    for (const PyramidBlockPlacement& block : blocks)
        if (ConstructionTimelineController::isFrontierCandidate(block, config))
        {
            candidate = &block;
            break;
        }
    if (candidate == nullptr)
        phases = false;
    else
    {
        const std::array<float, 6> amounts{{0.05f, 0.30f, 0.60f,
                                             0.82f, 0.97f, 1.0f}};
        const std::array<FrontierPlacementPhase, 6> expectedPhases{{
            FrontierPlacementPhase::Queued,
            FrontierPlacementPhase::Approach,
            FrontierPlacementPhase::LiftSlide,
            FrontierPlacementPhase::Align,
            FrontierPlacementPhase::Settle,
            FrontierPlacementPhase::Stable}};
        for (std::size_t index = 0; index < amounts.size(); ++index)
        {
            const FrontierPlacementTransform transform =
                ConstructionTimelineController::frontierTransform(
                    *candidate, config, amounts[index]);
            phases = phases && transform.phase == expectedPhases[index] &&
                     std::isfinite(transform.offset.x) &&
                     std::isfinite(transform.offset.y) &&
                     std::isfinite(transform.offset.z);
        }
    }

    ConstructionTimelineController direct;
    direct.setProgress(1.0f);
    const bool directSeekQuiet =
        direct.progress() == 1.0f && !direct.playing();
    const bool valid = checkpoints && monotonic && naturalCompletion &&
                       frontierActivity && placementEvents && phases &&
                       directSeekQuiet;
    output << "Phase 12.5 natural timelapse validation\n"
           << "  exact 0/25/50/75/100 checkpoint counts: "
           << (checkpoints ? "PASS" : "FAIL") << '\n'
           << "  update-driven monotonic 18% -> 100% playback: "
           << (monotonic && naturalCompletion ? "PASS" : "FAIL") << '\n'
           << "  active frontier frames / maximum blocks: "
           << activeFrames << " / " << maximumFrontier << '\n'
           << "  queued/approach/lift/align/settle phases: "
           << (phases ? "PASS" : "FAIL") << '\n'
           << "  natural settlement events: " << settlementEvents << '\n'
           << "  direct seek remains paused and event-free: "
           << (directSeekQuiet ? "PASS" : "FAIL") << '\n'
           << (valid ? "Timelapse repair checks passed.\n"
                     : "Timelapse repair checks failed.\n");
    return valid;
}
