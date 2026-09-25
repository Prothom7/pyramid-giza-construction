#include "animation/ConstructionTimeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <ostream>

#include "animation/ConstructionAnimation.h"
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
}

void ConstructionTimelineController::update(float deltaTime)
{
    if (!playing_ || deltaTime <= 0.0f || !std::isfinite(deltaTime))
        return;
    setProgress(progress_ + deltaTime * speed_ / durationSeconds_);
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
    if (block.level < config.completedLevels &&
        !PyramidLayout::isLegacyConstructionOpening(config, block))
    {
        return defaultProgress *
               (static_cast<float>(block.level) + wave) /
               static_cast<float>(config.completedLevels);
    }
    if (block.level < config.completedLevels)
        return 0.755f + 0.045f * wave;

    const unsigned int upperCount = config.baseBlocksPerSide - config.completedLevels;
    const float upperLevel = static_cast<float>(block.level - config.completedLevels) + wave;
    return 0.805f + 0.19f * upperLevel / static_cast<float>(upperCount);
}

ConstructionBlockState ConstructionTimelineController::blockState(
    const PyramidBlockPlacement& block, const PyramidLayoutConfig& config) const
{
    ConstructionBlockState state;
    state.threshold = blockThreshold(block, config);
    if (progress_ + 1.0e-6f < state.threshold)
        return state;

    state.visible = true;
    state.frontier = progress_ < 1.0f && isFrontierCandidate(block) &&
                     progress_ < state.threshold + frontierWindow;
    state.placementAmount = state.frontier
        ? smoothStep((progress_ - state.threshold) / frontierWindow)
        : 1.0f;
    return state;
}

bool ConstructionTimelineController::isFrontierCandidate(
    const PyramidBlockPlacement& block)
{
    const unsigned int signature =
        block.gridX * 17u + block.gridZ * 31u + block.level * 13u;
    return signature % 23u == 0u;
}

float ConstructionTimelineController::stableThreshold(
    const PyramidBlockPlacement& block, const PyramidLayoutConfig& config)
{
    const float threshold = blockThreshold(block, config);
    return isFrontierCandidate(block)
        ? std::min(1.0f, threshold + frontierWindow)
        : threshold;
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
