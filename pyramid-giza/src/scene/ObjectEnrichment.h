#pragma once

#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "scene/SceneTypes.h"

enum class RopeRigType
{
    HorizontalRedirection,
    AFrameLift,
    RampAssist
};

struct RopeRigDescriptor
{
    const char* id;
    const char* zone;
    RopeRigType type;
    glm::vec3 center;
    float yawDegrees;
    float width;
    float height;
    float depth;
    unsigned int wheelCount;
    const char* purpose;
};

struct AnchorPostDescriptor
{
    const char* id;
    const char* zone;
    glm::vec3 base;
    float height;
    float diameter;
};

struct LadderDescriptor
{
    const char* id;
    const char* zone;
    glm::vec3 base;
    float yawDegrees;
    float leanDegrees;
    float width;
    float height;
    unsigned int rungCount;
    const char* connectedTo;
};

struct BoatDescriptor
{
    static constexpr float hullCenterLocalY = 0.26f;
    static constexpr float hullHeight = 0.42f;
    static constexpr float draft = 0.16f;
    static constexpr float waterlineLocalY =
        hullCenterLocalY - 0.5f * hullHeight + draft;
    static constexpr float bowSampleFraction = 0.32f;
    static constexpr float sideSampleFraction = 0.25f;

    const char* id;
    glm::vec3 center;
    float yawDegrees;
    float length;
    float width;
    bool mooredAtLanding;
};

struct BoatMooringDescriptor
{
    glm::vec3 boatAttachmentLocal;
    glm::vec3 shoreAnchor;
};

class ObjectEnrichment
{
public:
    static const std::vector<RopeRigDescriptor>& ropeRigs();
    static const std::vector<AnchorPostDescriptor>& anchorPosts();
    static const std::vector<LadderDescriptor>& ladders();
    static const std::vector<BoatDescriptor>& boats();
    static std::vector<SceneObject> boatLocalParts(const BoatDescriptor& boat);
    static std::vector<BoatMooringDescriptor> boatMoorings(const BoatDescriptor& boat);

    static constexpr std::size_t workshopClusters = 1;
    static constexpr std::size_t repairStations = 1;
    static constexpr std::size_t inspectionStations = 1;
    static constexpr std::size_t riverLandings = 1;
    static constexpr std::size_t accessWalkways = 4;
    static constexpr std::size_t expectedStaticInstances = 373;
    static constexpr std::size_t addedSupportWorkers = 4;
    static constexpr std::size_t expectedAddedDrawCalls = 417;
    static constexpr std::size_t maximumAddedDrawCalls = 420;
};

const char* ropeRigTypeName(RopeRigType type);
bool validateObjectEnrichment(std::ostream& output);
