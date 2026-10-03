#pragma once

#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "scene/SceneTypes.h"

struct WorldScale
{
    float workerHeight = 2.26f;
    float blockWidth = 2.80f;
    float blockHeight = 2.00f;
    float blockDepth = 2.80f;
    float blockSpacing = 0.12f;
    unsigned int pyramidBaseBlocks = 28;
    unsigned int pyramidCompletedLevels = 23;
    unsigned int pyramidPartialFromLevel = 15;
    glm::vec3 pyramidOrigin{0.0f, 0.0f, -42.0f};
    float pyramidTargetHeight = 56.0f;
    float mainRampWidth = 7.0f;
    float scaffoldLevelHeight = 2.70f;
    float quarryDistance = 131.0f;
    float quarryDepth = 7.5f;
    float haulRoadLength = 97.9f;
    float worldWidth = 360.0f;
    float worldDepth = 300.0f;
};

struct RampDescriptor
{
    const char* id;
    glm::vec3 base;
    glm::vec3 top;
    float width;
    float thickness;
    MaterialId material;
    bool animatedRoute;
    float minimumProgress = 0.0f;
    float maximumProgress = 1.01f;
    float sideClearance = 0.75f;
    float supportSurfaceY = 0.0f;
    bool intentionalPyramidContact = false;
    const char* purpose = "";
};

struct RampFrame
{
    glm::vec3 forward{0.0f};
    glm::vec3 right{0.0f};
    glm::vec3 up{0.0f};
    float length = 0.0f;
    float slopeDegrees = 0.0f;
};

struct ScaffoldPlacement
{
    const char* id;
    glm::vec3 origin;
    float rotationY;
    unsigned int levels;
    unsigned int bays;
    const char* purpose;
    float minimumProgress = 0.0f;
    float maximumProgress = 1.01f;
};

struct UpperWorkDeckPanel
{
    glm::vec3 center;
    glm::vec3 size;
};

struct UpperAccessLayout
{
    UpperWorkDeckPanel apron;
    UpperWorkDeckPanel turningLanding;
    glm::vec2 targetParking{-28.05f, -63.90f};
    glm::vec2 departure{-9.0f, 0.0f};
    glm::vec2 apronCenter{-14.5f, 5.5f};
    float apronRadius = 5.5f;
    glm::vec2 turnCenter{-45.0f, 5.5f};
    float turnRadius = 5.5f;
    float turnRadians = 0.0f;
};

struct UpperSupportFooting
{
    glm::vec2 position{0.0f};
    float courseTop = 0.0f;
    bool onCourse = false;
    bool valid = false;
};

struct SiteZoneDescriptor
{
    const char* id;
    glm::vec3 center;
    glm::vec3 extents;
    const char* purpose;
};

class MonumentalSite
{
public:
    static const WorldScale& scale();
    static const std::vector<RampDescriptor>& ramps();
    static const RampDescriptor& mainRamp();
    static const RampDescriptor* findRamp(const char* id);
    static RampFrame rampFrame(const RampDescriptor& ramp);
    static bool rampActive(const RampDescriptor& ramp, float progress);
    static const std::vector<ScaffoldPlacement>& scaffolds();
    static const std::vector<UpperWorkDeckPanel>& upperWorkDeckPanels();
    static const UpperAccessLayout& upperAccessLayout();
    static const RampDescriptor& upperRampA();
    static const RampDescriptor& upperRampB();
    static const RampDescriptor& targetLevelLanding();
    static float occupiedCourseTopUnder(glm::vec2 point, float margin);
    static UpperSupportFooting upperSupportFooting(glm::vec2 preferred,
                                                   float undersideY);
    static const std::vector<SiteZoneDescriptor>& zones();

    static glm::mat4 rampModel(const RampDescriptor& ramp);
    static glm::vec3 rampSurfacePoint(const RampDescriptor& ramp, float progress);
    static float mainRampSurfaceHeight(float z);
    static float mainRampPitchDegrees();
};

bool validateMonumentalSite(std::ostream& output);
