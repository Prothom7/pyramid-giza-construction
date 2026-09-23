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
};

struct ScaffoldPlacement
{
    const char* id;
    glm::vec3 origin;
    float rotationY;
    unsigned int levels;
    unsigned int bays;
    const char* purpose;
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
    static const std::vector<ScaffoldPlacement>& scaffolds();
    static const std::vector<SiteZoneDescriptor>& zones();

    static glm::mat4 rampModel(const RampDescriptor& ramp);
    static glm::vec3 rampSurfacePoint(const RampDescriptor& ramp, float progress);
    static float mainRampSurfaceHeight(float z);
    static float mainRampPitchDegrees();
};

bool validateMonumentalSite(std::ostream& output);
