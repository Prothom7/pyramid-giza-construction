#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

enum class QuarryPulleyState
{
    Idle,
    Attach,
    Tension,
    Lift,
    UpperHold,
    GuideToPlatform,
    Lower,
    Release,
    Complete
};

enum class QuarryPulleyLoadSupport
{
    QuarryFloor,
    Suspended,
    DestinationPlatform
};

struct QuarryPulleySnapshot
{
    QuarryPulleyState state = QuarryPulleyState::Idle;
    QuarryPulleyLoadSupport support = QuarryPulleyLoadSupport::QuarryFloor;
    float stateProgress = 0.0f;
    glm::vec3 loadPosition{0.0f};
    glm::vec3 carriagePosition{0.0f};
    glm::vec3 pulleyPoint{0.0f};
    glm::vec3 loadAttachmentPoint{0.0f};
    glm::vec3 ropeEnd{0.0f};
    float ropeLength = 0.0f;
    float wheelRotationDegrees = 0.0f;
    bool ropeAttached = false;
};

class QuarryPulleyAnimationController
{
public:
    void update(float deltaTime);
    void reset();
    void seek(float elapsedTime, bool playing = true);
    void togglePaused() { paused_ = !paused_; }
    void setPaused(bool paused) { paused_ = paused; }
    void setLooping(bool looping) { looping_ = looping; }

    bool paused() const { return paused_; }
    bool looping() const { return looping_; }
    float elapsedTime() const { return static_cast<float>(elapsedTime_); }
    QuarryPulleySnapshot snapshot() const;

    static QuarryPulleySnapshot snapshotAt(float elapsedTime);
    static float sequenceDuration();
    static float stateDuration(QuarryPulleyState state);
    static const char* stateName(QuarryPulleyState state);
    static const char* supportName(QuarryPulleyLoadSupport support);

    static glm::vec3 startLoadPosition();
    static glm::vec3 raisedLoadPosition();
    static glm::vec3 destinationLoadPosition();
    static glm::vec3 frameLeftBase();
    static glm::vec3 frameRightBase();
    static float quarryFloorY() { return -7.45f; }
    static float destinationPlatformTopY() { return -2.60f; }
    static float wheelRadius() { return 0.75f; }

private:
    double elapsedTime_ = 0.0;
    bool paused_ = false;
    bool looping_ = true;
};

bool validateQuarryPulleyAnimation(std::ostream& output);
bool validateQuarryPulleySupport(std::ostream& output);
