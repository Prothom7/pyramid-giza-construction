#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/MonumentalSite.h"

enum class SupportCategory
{
    GroundSupported,
    StructureSupported,
    IntentionallySuspended
};

enum class SupportSurfaceKind
{
    None,
    DesertGround,
    QuarryFloor,
    PyramidCourse,
    RampFill,
    ScaffoldPlatform,
    WorkPlatform,
    Sledge,
    LiftingRig
};

struct SupportSurface
{
    const char* id = "None";
    SupportSurfaceKind kind = SupportSurfaceKind::None;
    float height = 0.0f;
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    bool valid = false;
};

struct HorizontalFootprint
{
    glm::vec2 center{0.0f};
    glm::vec2 halfExtents{0.0f};
    float yawDegrees = 0.0f;
};

struct StageSupportDependency
{
    const char* objectId;
    const char* supportId;
    SupportCategory category;
    float objectMinimum;
    float objectMaximum;
    float supportMinimum;
    float supportMaximum;
    const char* notes;
};

class SandSimulation;

namespace SceneSupport
{
void setTerrainSource(const SandSimulation* terrain);
constexpr float minimumContactGap = -0.08f;
constexpr float maximumContactGap = 0.08f;

bool stageActive(float progress, float minimum, float maximum);
float transformedBottomY(const glm::mat4& model,
                         const glm::vec3& canonicalHalfExtents =
                             glm::vec3{0.5f});
bool footprintContains(const HorizontalFootprint& support,
                       const glm::vec2& point, float margin = 0.0f);
SupportSurface terrainAt(const glm::vec2& point);
SupportSurface rampFillAt(const RampDescriptor& ramp, float rampProgress);
float rampUndersideY(const RampDescriptor& ramp, float rampProgress);
float verticalGap(float objectBottom, const SupportSurface& support);
const std::vector<StageSupportDependency>& dependencies();
const char* categoryName(SupportCategory category);
const char* surfaceKindName(SupportSurfaceKind kind);
} // namespace SceneSupport

bool validatePhase12_6Supports(std::ostream& output);
bool validatePhase12_6Grounding(std::ostream& output);
bool validatePhase12_6StageDependencies(std::ostream& output);
