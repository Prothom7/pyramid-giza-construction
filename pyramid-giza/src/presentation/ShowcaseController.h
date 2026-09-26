#pragma once

#include <array>
#include <cstddef>
#include <iosfwd>

#include <glm/glm.hpp>

#include "camera/CameraController.h"

enum class ShowcaseMode
{
    Manual,
    Playing,
    Paused,
    Complete
};

enum class ShowcaseConstructionAction
{
    SetCheckpoint,
    KeepCheckpoint,
    StartTimelapse,
    CompletePyramid
};

struct ShowcaseShot
{
    const char* id = "Unnamed";
    const char* purpose = "";
    float startTime = 0.0f;
    float duration = 1.0f;
    glm::vec3 startPosition{0.0f};
    glm::vec3 endPosition{0.0f};
    glm::vec3 startTarget{0.0f};
    glm::vec3 endTarget{0.0f};
    float startFov = 50.0f;
    float endFov = 50.0f;
    float startSunTime = 8.0f;
    float endSunTime = 8.0f;
    ShowcaseConstructionAction constructionAction =
        ShowcaseConstructionAction::KeepCheckpoint;
    float constructionValue = 0.75f;
    float constructionSpeed = 1.0f;
    bool resetHero = false;
    bool hardCut = false;
};

struct ShowcaseFrame
{
    std::size_t shotIndex = 0;
    float showcaseTime = 0.0f;
    float shotProgress = 0.0f;
    CameraPose camera;
    glm::vec3 cameraTarget{0.0f};
    float constructionProgress = 0.75f;
    float constructionSpeed = 1.0f;
    bool constructionPlaying = false;
    float heroTime = 0.0f;
    bool heroPlaying = true;
    float sunTime = 8.0f;
    bool complete = false;
};

class ShowcaseController
{
public:
    static constexpr float minimumSpeed = 0.25f;
    static constexpr float maximumSpeed = 4.0f;
    static constexpr std::size_t shotCount = 14;

    void start(float time = 0.0f);
    void cancel();
    void togglePaused();
    void update(float deltaTime);
    void seek(float time);
    void setSpeed(float speed);

    ShowcaseMode mode() const { return mode_; }
    bool controlsCamera() const { return mode_ != ShowcaseMode::Manual; }
    bool playing() const { return mode_ == ShowcaseMode::Playing; }
    bool paused() const { return mode_ == ShowcaseMode::Paused; }
    bool complete() const { return mode_ == ShowcaseMode::Complete; }
    float time() const { return time_; }
    float speed() const { return speed_; }
    std::size_t shotIndex() const { return shotIndex_; }
    bool consumeShotChanged();
    bool consumeCompleted();
    ShowcaseFrame frame() const;

    static const std::array<ShowcaseShot, shotCount>& shots();
    static float totalDuration();
    static ShowcaseFrame evaluate(float time);
    static const char* modeName(ShowcaseMode mode);
    static const char* constructionActionName(ShowcaseConstructionAction action);

private:
    static std::size_t findShot(float time);

    ShowcaseMode mode_ = ShowcaseMode::Manual;
    float time_ = 0.0f;
    float speed_ = 1.0f;
    std::size_t shotIndex_ = 0;
    bool shotChanged_ = false;
    bool completedThisUpdate_ = false;
};

bool validatePhase12ShowcaseTimeline(std::ostream& output);
bool validatePhase12ShowcaseCamera(std::ostream& output);
bool validatePhase12ShowcaseState(std::ostream& output);
