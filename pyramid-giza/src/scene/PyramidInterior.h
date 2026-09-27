#pragma once

#include <array>
#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "scene/PyramidLayout.h"
#include "scene/SceneTypes.h"

enum class InteriorSpaceType
{
    Passage,
    Gallery,
    Antechamber,
    TombChamber
};

struct PassageSegment
{
    const char* id;
    InteriorSpaceType type;
    glm::vec3 start;
    glm::vec3 end;
    float width;
    float height;
    float carvingMargin;
    const char* purpose;
};

struct InteriorRoom
{
    const char* id;
    InteriorSpaceType type;
    glm::vec3 center;
    glm::vec3 halfExtents;
    float carvingMargin;
    const char* purpose;
};

struct InteriorPart
{
    const char* id;
    glm::mat4 model{1.0f};
    MaterialId material = MaterialId::Limestone;
    float minimumConstructionProgress = 0.0f;
};

struct InteriorExclusionStats
{
    std::size_t total = 0;
    std::array<std::size_t, 6> byRegion{};
    std::array<unsigned int, 6> minimumLevelByRegion{};
    std::array<unsigned int, 6> maximumLevelByRegion{};
    unsigned int minimumLevel = 0;
    unsigned int maximumLevel = 0;
};

struct InteriorBounds
{
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
};

class PyramidInterior
{
public:
    static constexpr float eyeHeight = 1.70f;
    static constexpr float inspectionLightRange = 12.0f;
    static constexpr float inspectionLightIntensity = 1.15f;

    static const std::array<PassageSegment, 4>& passages();
    static const std::array<InteriorRoom, 2>& rooms();
    static const std::vector<InteriorPart>& architecturalParts();

    static bool blockIntersectsVoid(const PyramidBlockPlacement& block,
                                    std::size_t* regionIndex = nullptr);
    static bool isCutawayBlock(const PyramidBlockPlacement& block);
    static InteriorExclusionStats exclusionStats(
        const std::vector<PyramidBlockPlacement>& blocks);

    static glm::vec3 cameraStartPosition();
    static float cameraStartYaw() { return 90.0f; }
    static float cameraStartPitch() { return -5.0f; }
    static bool isWalkableCameraPosition(const glm::vec3& position);
    static glm::vec3 constrainCamera(const glm::vec3& previous,
                                     const glm::vec3& candidate);
    static InteriorBounds bounds();
    static const char* typeName(InteriorSpaceType type);
};

bool validatePyramidInteriorGeometry(std::ostream& output);
bool validatePyramidInteriorConnectivity(std::ostream& output);
bool validatePyramidInteriorNavigation(std::ostream& output);
bool validatePyramidInterior(std::ostream& output);
