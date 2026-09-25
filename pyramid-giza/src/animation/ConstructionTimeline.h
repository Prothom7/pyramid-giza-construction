#pragma once

#include <cstddef>
#include <iosfwd>

#include "scene/PyramidLayout.h"

enum class ConstructionStage
{
    Foundation,
    LowerCourses,
    LowerMiddle,
    HistoricalCheckpoint,
    UpperCourses,
    Summit,
    Complete
};

struct ConstructionBlockState
{
    bool visible = false;
    bool frontier = false;
    float placementAmount = 0.0f;
    float threshold = 0.0f;
};

class ConstructionTimelineController
{
public:
    static constexpr float defaultProgress = 0.75f;
    static constexpr float defaultDurationSeconds = 90.0f;

    void update(float deltaTime);
    void togglePlayback() { playing_ = !playing_; }
    void reset();
    void complete();
    void adjustSpeed(int direction);

    void setProgress(float progress);
    void setDuration(float seconds);
    void setSpeed(float speed);
    void setPlaying(bool playing) { playing_ = playing; }

    float progress() const { return progress_; }
    float duration() const { return durationSeconds_; }
    float speed() const { return speed_; }
    bool playing() const { return playing_; }

    ConstructionStage stage() const;
    unsigned int activeLevel(const PyramidLayoutConfig& config) const;
    ConstructionBlockState blockState(const PyramidBlockPlacement& block,
                                      const PyramidLayoutConfig& config) const;
    std::size_t visibleBlockCount(const std::vector<PyramidBlockPlacement>& blocks,
                                  const PyramidLayoutConfig& config) const;

    static float blockThreshold(const PyramidBlockPlacement& block,
                                const PyramidLayoutConfig& config);
    static bool isFrontierCandidate(const PyramidBlockPlacement& block);
    static float stableThreshold(const PyramidBlockPlacement& block,
                                 const PyramidLayoutConfig& config);
    static const char* stageName(ConstructionStage stage);

private:
    float progress_ = defaultProgress;
    float durationSeconds_ = defaultDurationSeconds;
    float speed_ = 1.0f;
    bool playing_ = false;
};

bool validateConstructionTimeline(std::ostream& output);
