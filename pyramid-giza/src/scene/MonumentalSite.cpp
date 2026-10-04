#include "scene/MonumentalSite.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "objects/Scaffold.h"
#include "objects/Sledge.h"
#include "scene/PyramidLayout.h"
#include "scene/PyramidInterior.h"

namespace
{
bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::vec3 rampForward(const RampDescriptor& ramp)
{
    const glm::vec3 direction = ramp.top - ramp.base;
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length <= 1.0e-5f)
        throw std::invalid_argument("Ramp endpoints must define a positive length");
    return direction / length;
}

glm::vec3 rampRight(const RampDescriptor& ramp)
{
    glm::vec3 right = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, rampForward(ramp));
    if (glm::length(right) <= 1.0e-5f)
        right = {1.0f, 0.0f, 0.0f};
    return glm::normalize(right);
}

glm::vec3 rampUp(const RampDescriptor& ramp)
{
    return glm::normalize(glm::cross(rampForward(ramp), rampRight(ramp)));
}
} // namespace

const WorldScale& MonumentalSite::scale()
{
    static const WorldScale value;
    return value;
}

const std::vector<RampDescriptor>& MonumentalSite::ramps()
{
    static const std::vector<RampDescriptor> values{
        {"MainHaulingLow", {0.0f, 0.30f, 45.0f}, {0.0f, 2.35f, 0.80f},
         scale().mainRampWidth, 0.65f, MaterialId::RampEarth, false,
         0.0f, 0.30f, 1.45f, 0.0f, false, "Early low-course haul"},
        {"MainHaulingMiddle", {0.0f, 0.32f, 45.0f}, {0.0f, 5.25f, -1.70f},
         scale().mainRampWidth, 0.68f, MaterialId::RampEarth, false,
         0.30f, 0.62f, 1.45f, 0.0f, false, "Lower-middle haul"},
        {"MainHaulingRamp", {0.0f, 0.35f, 45.0f}, {0.0f, 8.20f, -4.20f},
         scale().mainRampWidth, 0.70f, MaterialId::RampEarth, true,
         0.62f, 0.90f, 1.35f, 0.0f, false, "Hero and upper-middle haul"},
        {"MainLanding", {0.0f, 8.35f, -4.10f}, {0.0f, 8.35f, -5.25f},
         7.8f, 0.40f, MaterialId::RampEarth, false,
         0.62f, 0.90f, 0.25f, 0.0f, true, "Controlled pyramid-face landing"},
        upperRampA(),
        upperRampB(),
        targetLevelLanding(),
        {"WestAccessRamp", {-63.0f, 0.30f, -25.0f}, {-40.0f, 4.30f, -25.0f},
         4.5f, 0.60f, MaterialId::RampEarth, false,
         0.05f, 0.62f, 1.40f, 0.0f, false, "West worker access"},
        // A short timber bridge joins the receiving deck to the stepped east
        // quarry wall. The exit then follows that wall's crest and emerges
        // north of the rough-stone repository.
        {"QuarryExitDeckExtension", {-107.6f, -2.70f, -9.0f}, {-106.2f, -2.40f, -9.4f},
         3.0f, 0.20f, MaterialId::DarkWood, false,
         0.0f, 1.01f, 0.10f, -7.5f, false, "Receiving-deck exit skid"},
        {"QuarryDeckConnector", {-106.2f, -2.44f, -9.4f}, {-102.5f, 0.36f, -2.0f},
         3.4f, 0.28f, MaterialId::Wood, false,
         0.0f, 1.01f, 0.25f, -7.5f, false, "Receiving deck bridge"},
        {"QuarryExitRamp", {-102.5f, 0.17f, -2.0f}, {-97.0f, -0.33f, 25.0f},
         7.5f, 0.65f, MaterialId::RampEarth, false,
         0.0f, 1.01f, 0.40f, 0.0f, false, "East-wall quarry exit"}
    };
    return values;
}

const RampDescriptor& MonumentalSite::mainRamp()
{
    return *findRamp("MainHaulingRamp");
}

const RampDescriptor* MonumentalSite::findRamp(const char* id)
{
    for (const RampDescriptor& ramp : ramps())
        if (std::string(ramp.id) == id)
            return &ramp;
    return nullptr;
}

RampFrame MonumentalSite::rampFrame(const RampDescriptor& ramp)
{
    const glm::vec3 delta = ramp.top - ramp.base;
    const float horizontal = glm::length(glm::vec2{delta.x, delta.z});
    return {rampForward(ramp), rampRight(ramp), rampUp(ramp),
            glm::length(delta), glm::degrees(std::atan2(delta.y, horizontal))};
}

bool MonumentalSite::rampActive(const RampDescriptor& ramp, float progress)
{
    const float value = std::clamp(progress, 0.0f, 1.0f);
    return value >= ramp.minimumProgress &&
           (value < ramp.maximumProgress ||
            (value >= 1.0f && ramp.maximumProgress > 1.0f));
}

const std::vector<ScaffoldPlacement>& MonumentalSite::scaffolds()
{
    static const std::vector<ScaffoldPlacement> values{
        {"FrontWest", {-24.0f, 0.0f, 2.8f}, 0.0f, 3, 3,
         "Early west-face access", 0.05f, 0.48f},
        {"FrontEast", {12.0f, 0.0f, 2.8f}, 0.0f, 3, 3,
         "Lower-middle east-face access", 0.20f, 0.62f},
        {"RampTop", {7.5f, 0.0f, -4.8f}, 0.0f, 3, 2,
         "Ground-founded hero-ramp landing access", 0.62f, 0.90f}
    };
    return values;
}

const std::vector<UpperWorkDeckPanel>& MonumentalSite::upperWorkDeckPanels()
{
    // The center opening leaves headroom for a loaded sledge on the rising
    // ramp. The end panel joins the ramp landing to the two work-deck wings.
    static const std::vector<UpperWorkDeckPanel> panels{
        {{-2.0f, 8.375f, -5.05f}, {30.0f, 0.35f, 1.90f}},
        {{-10.25f, 8.375f, 0.95f}, {13.50f, 0.35f, 10.10f}},
        {{8.25f, 8.375f, 0.95f}, {9.50f, 0.35f, 10.10f}}
    };
    return panels;
}

const UpperAccessLayout& MonumentalSite::upperAccessLayout()
{
    // The flat landing begins at the ramp's high end. Extending it east to
    // x=-42 would put a level deck above the final sloping ramp section.
    static const UpperAccessLayout layout{
        {{-12.75f, 8.375f, 9.25f}, {11.5f, 0.35f, 8.5f}},
        {{-49.5f, 12.80976f, 6.0f}, {9.0f, 0.35f, 16.0f}},
        {-28.05f, -63.90f},
        {-9.0f, 0.0f}, {-14.5f, 5.5f}, 5.5f,
        {-45.0f, 5.5f}, 5.5f, glm::radians(108.14056f)
    };
    return layout;
}

const RampDescriptor& MonumentalSite::upperRampA()
{
    // RampDescriptor endpoints are slab centers. The declared endpoints here
    // yield visible top heights of 8.55 and 12.98476 metres.
    constexpr float rise = 4.4347643f;
    // A 20 cm slab overlap closes the numerical end-face seam at the landing.
    const float centerOffset = 0.35f * std::cos(std::atan(rise / 28.2f));
    static const RampDescriptor ramp{
        "UpperRampA", {-17.0f, 8.55f - centerOffset, 11.0f},
        {-45.2f, 8.55f + rise - centerOffset, 11.0f},
        7.0f, 0.70f, MaterialId::Wood, false,
        0.62f, 0.90f, 0.30f, 0.0f, false, "Westward upper sledge ramp"
    };
    return ramp;
}

const RampDescriptor& MonumentalSite::upperRampB()
{
    const UpperAccessLayout& access = upperAccessLayout();
    const glm::vec2 start = access.turnCenter + access.turnRadius *
        glm::vec2{-std::sin(access.turnRadians), std::cos(access.turnRadians)};
    const glm::vec2 end{-29.0f, -61.0f};
    const float horizontal = glm::distance(start, end);
    const float rise = 24.0f - 12.98476f;
    const float centerOffset = 0.35f * std::cos(std::atan2(rise, horizontal));
    static const RampDescriptor ramp{
        "UpperRampB", {start.x, 12.98476f - centerOffset, start.y},
        {end.x, 24.0f - centerOffset, end.y},
        7.0f, 0.70f, MaterialId::Wood, false,
        0.62f, 0.90f, 0.0f, 0.0f, false, "Target-course upper sledge ramp"
    };
    return ramp;
}

const RampDescriptor& MonumentalSite::targetLevelLanding()
{
    const RampDescriptor& ramp = upperRampB();
    const glm::vec2 direction = glm::normalize(glm::vec2{
        ramp.top.x - ramp.base.x, ramp.top.z - ramp.base.z});
    // Favor the open west side. The small lateral offset keeps the flat deck
    // clear of the finished course while still covering the ramp centerline.
    const glm::vec2 west{rampFrame(ramp).right.x, rampFrame(ramp).right.z};
    const glm::vec2 start = glm::vec2{ramp.top.x, ramp.top.z} -
        0.20f * direction + 0.35f * west;
    const glm::vec2 end = glm::vec2{ramp.top.x, ramp.top.z} +
        9.5f * direction + 0.35f * west;
    static const RampDescriptor landing{
        "UpperTargetLanding", {start.x, 23.825f, start.y},
        {end.x, 23.825f, end.y},
        4.4f, 0.35f, MaterialId::Wood, false,
        0.62f, 0.90f, 0.0f, 0.0f, false, "Target-course sledge staging deck"
    };
    return landing;
}

PlacementSkid MonumentalSite::placementSkid(
    const glm::vec3& cargoCenter, const glm::vec3& cargoScale,
    const PyramidBlockPlacement& target, const glm::mat4& parkedSledgeRoot)
{
    if (target.level == 0u)
        throw std::invalid_argument("Placement skid requires an occupied supporting course");
    const float courseTop = target.position.y - 0.5f * target.scale.y;
    const glm::vec2 travel = glm::normalize(glm::vec2{
        target.position.x - cargoCenter.x, target.position.z - cargoCenter.z});
    static const auto layout = PyramidLayout::generateComplete(PyramidLayoutConfig{});
    float westEdge = std::numeric_limits<float>::infinity();
    for (const PyramidBlockPlacement& block : layout)
        if (block.level + 1u == target.level &&
            !PyramidInterior::blockIntersectsVoid(block) &&
            std::abs(block.position.z - target.position.z) <
                0.5f * (block.scale.z + target.scale.z))
            westEdge = std::min(westEdge,
                block.position.x - 0.5f * block.scale.x);
    if (!std::isfinite(westEdge) || travel.x < 0.5f ||
        westEdge <= cargoCenter.x || westEdge >= target.position.x)
        throw std::runtime_error("Assigned target has no reachable west course edge");
    static const auto sledgeParts = Sledge::create(false);
    float deckEdgeDistance = -std::numeric_limits<float>::infinity();
    for (const ObjectPart& part : sledgeParts)
    {
        if (part.name != "Platform") continue;
        const glm::mat4 deck = parkedSledgeRoot * part.localTransform;
        for (float x : {-0.5f, 0.5f})
            for (float z : {-0.5f, 0.5f})
            {
                const glm::vec3 point{deck * glm::vec4{x, 0.0f, z, 1.0f}};
                deckEdgeDistance = std::max(deckEdgeDistance,
                    glm::dot(glm::vec2{point.x - cargoCenter.x,
                                       point.z - cargoCenter.z}, travel));
            }
    }
    const float courseEdgeDistance = (westEdge - cargoCenter.x) / travel.x;
    const float endDistance = courseEdgeDistance + 1.06f / travel.x;
    const float gradeStartDistance = deckEdgeDistance + 0.05f;
    if (!std::isfinite(deckEdgeDistance) ||
        gradeStartDistance >= courseEdgeDistance ||
        endDistance >= glm::distance(glm::vec2{cargoCenter.x, cargoCenter.z},
                                     glm::vec2{target.position.x, target.position.z}) - 0.2f)
        throw std::runtime_error("Placement skid cannot clear the parked sledge and course");
    const glm::vec3 start{cargoCenter.x,
                          cargoCenter.y - 0.5f * cargoScale.y,
                          cargoCenter.z};
    const glm::vec3 flatEnd = start +
        glm::vec3{travel.x * gradeStartDistance, 0.0f,
                  travel.y * gradeStartDistance};
    const float edgeT = (courseEdgeDistance - gradeStartDistance) /
        (endDistance - gradeStartDistance);
    const glm::vec3 courseEdge{westEdge,
        glm::mix(start.y, courseTop, edgeT),
        cargoCenter.z + travel.y * courseEdgeDistance};
    const glm::vec3 end{cargoCenter.x + travel.x * endDistance,
                        courseTop, cargoCenter.z + travel.y * endDistance};
    return {start, flatEnd, courseEdge, end, courseTop};
}

float MonumentalSite::placementSupportHeight(
    const PlacementSkid& skid, glm::vec2 blockCenter,
    float halfExtentAlongRoute)
{
    const glm::vec2 start{skid.start.x, skid.start.z};
    const glm::vec2 end{skid.end.x, skid.end.z};
    const glm::vec2 direction = glm::normalize(end - start);
    const float railLength = glm::distance(start, end);
    const float flatLength = glm::distance(start,
        glm::vec2{skid.gradeStart.x, skid.gradeStart.z});
    const float centerDistance = glm::dot(blockCenter - start, direction);
    // The rear of the rigid block stays on the higher rail until it clears
    // that point. Then the occupied course carries the whole footprint.
    if (centerDistance - halfExtentAlongRoute >= railLength)
        return skid.courseTop;
    const float rearContact = std::clamp(
        centerDistance - halfExtentAlongRoute, 0.0f, railLength);
    if (rearContact <= flatLength) return skid.start.y;
    return glm::mix(skid.start.y, skid.end.y,
                    (rearContact - flatLength) / (railLength - flatLength));
}

float MonumentalSite::occupiedCourseTopUnder(glm::vec2 point, float margin)
{
    static const auto blocks = PyramidLayout::generateComplete(PyramidLayoutConfig{});
    float top = -std::numeric_limits<float>::infinity();
    for (const PyramidBlockPlacement& block : blocks)
    {
        if (block.level >= 12u)
            continue;
        if (std::abs(point.x - block.position.x) + margin <= 0.5f * block.scale.x &&
            std::abs(point.y - block.position.z) + margin <= 0.5f * block.scale.z &&
            !PyramidInterior::blockIntersectsVoid(block))
            top = std::max(top, block.position.y + 0.5f * block.scale.y);
    }
    return top;
}

UpperSupportFooting MonumentalSite::upperSupportFooting(glm::vec2 preferred,
                                                         float undersideY)
{
    static const auto blocks = PyramidLayout::generateComplete(PyramidLayoutConfig{});
    constexpr float offsets[]{0.0f, -0.20f, 0.20f, -0.40f, 0.40f,
                              -0.60f, 0.60f};
    UpperSupportFooting best;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (float dx : offsets)
        for (float dz : offsets)
        {
            const glm::vec2 point = preferred + glm::vec2{dx, dz};
            const float course = occupiedCourseTopUnder(point, 0.23f);
            const bool onCourse = std::isfinite(course);
            const float foundation = onCourse ? course : 0.0f;
            if (foundation >= undersideY - 0.15f) continue;
            bool penetrates = false;
            for (const PyramidBlockPlacement& block : blocks)
            {
                if (block.level >= 12u)
                    continue;
                if (std::abs(point.x - block.position.x) >=
                        0.5f * block.scale.x + 0.19f ||
                    std::abs(point.y - block.position.z) >=
                        0.5f * block.scale.z + 0.19f)
                    continue;
                const float bottom = block.position.y - 0.5f * block.scale.y;
                const float top = block.position.y + 0.5f * block.scale.y;
                if (top > foundation + 0.005f && bottom < undersideY - 0.005f)
                    if (!PyramidInterior::blockIntersectsVoid(block))
                    { penetrates = true; break; }
            }
            const float distance = glm::dot(point - preferred, point - preferred);
            if (!penetrates && distance < bestDistance)
            {
                best = {point, course, onCourse, true};
                bestDistance = distance;
            }
        }
    return best;
}

const std::vector<SiteZoneDescriptor>& MonumentalSite::zones()
{
    static const std::vector<SiteZoneDescriptor> values{
        {"PyramidZone", {0.0f, 23.0f, -42.0f}, {42.0f, 23.0f, 42.0f}, "Main monument"},
        {"RampNetwork", {0.0f, 5.0f, 22.0f}, {34.0f, 8.0f, 24.0f}, "Primary access"},
        {"ScaffoldZone", {0.0f, 8.0f, 1.5f}, {31.0f, 9.0f, 4.0f}, "Active face access"},
        {"QuarryZone", {-128.0f, -3.5f, -15.0f}, {32.0f, 7.5f, 36.0f}, "Open-cut extraction"},
        {"RoughRepository", {-82.0f, 2.0f, 12.0f}, {16.0f, 4.0f, 14.0f}, "Rough stone depot"},
        {"CuttingZone", {-58.0f, 2.0f, 26.0f}, {13.0f, 4.0f, 12.0f}, "Stone dressing"},
        {"FinishedRepository", {-34.0f, 2.0f, 35.0f}, {14.0f, 5.0f, 13.0f}, "Finished stone depot"},
        {"LoadingStation", {-10.0f, 2.0f, 42.0f}, {13.0f, 4.0f, 10.0f}, "Sledge loading"},
        {"TransportZone", {-45.0f, 1.0f, 31.0f}, {50.0f, 2.0f, 10.0f}, "Long haul corridor"},
        {"TimberYard", {48.0f, 2.0f, 7.0f}, {13.0f, 4.0f, 14.0f}, "Wood and scaffold storage"},
        {"WorkCamp", {51.0f, 2.5f, -25.0f}, {14.0f, 5.0f, 12.0f}, "Shelter and tools"},
        {"NileContext", {0.0f, 0.0f, -155.0f}, {170.0f, 1.0f, 24.0f}, "Water and floodplain"},
        {"SphinxContext", {92.0f, 2.0f, -105.0f}, {16.0f, 5.0f, 12.0f}, "Secondary Giza landmark"}
    };
    return values;
}

glm::mat4 MonumentalSite::rampModel(const RampDescriptor& ramp)
{
    if (ramp.width <= 0.0f || ramp.thickness <= 0.0f)
        throw std::invalid_argument("Ramp width and thickness must be positive");

    const RampFrame frame = rampFrame(ramp);
    glm::mat4 model{1.0f};
    model[0] = glm::vec4{frame.right * ramp.width, 0.0f};
    model[1] = glm::vec4{frame.up * ramp.thickness, 0.0f};
    model[2] = glm::vec4{frame.forward * frame.length, 0.0f};
    model[3] = glm::vec4{0.5f * (ramp.base + ramp.top), 1.0f};
    return model;
}

glm::vec3 MonumentalSite::rampSurfacePoint(const RampDescriptor& ramp, float progress)
{
    const float t = std::clamp(progress, 0.0f, 1.0f);
    return glm::mix(ramp.base, ramp.top, t) +
           rampFrame(ramp).up * (0.5f * ramp.thickness);
}

float MonumentalSite::mainRampSurfaceHeight(float z)
{
    const RampDescriptor& ramp = mainRamp();
    const float denominator = ramp.top.z - ramp.base.z;
    const float progress = std::abs(denominator) > 1.0e-6f
                               ? (z - ramp.base.z) / denominator
                               : 0.0f;
    return rampSurfacePoint(ramp, progress).y;
}

float MonumentalSite::mainRampPitchDegrees()
{
    const RampDescriptor& ramp = mainRamp();
    const glm::vec3 delta = ramp.top - ramp.base;
    const float horizontal = glm::length(glm::vec2{delta.x, delta.z});
    return glm::degrees(std::atan2(delta.y, horizontal));
}

bool validateMonumentalSite(std::ostream& output)
{
    const WorldScale& world = MonumentalSite::scale();
    const PyramidLayoutConfig pyramidConfig;
    const std::vector<PyramidBlockPlacement> first = PyramidLayout::generate(pyramidConfig);
    const std::vector<PyramidBlockPlacement> second = PyramidLayout::generate(pyramidConfig);
    const PyramidLayoutStats pyramid = PyramidLayout::statistics(pyramidConfig, first);

    bool valid = world.workerHeight > 0.0f && world.blockWidth > 0.0f &&
                 world.blockHeight > 0.0f && world.blockDepth > 0.0f;
    valid = valid && std::abs(world.scaffoldLevelHeight - Scaffold::levelHeight()) < 1.0e-6f;
    valid = valid && pyramid.completedHeight >= 20.0f * world.workerHeight;
    valid = valid && world.pyramidTargetHeight > pyramid.completedHeight;
    valid = valid && pyramid.baseWidth >= 80.0f && pyramid.totalBlocks >= 3000;
    valid = valid && first.size() == second.size() && first.size() == 7561;
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        valid = valid && finiteVector(first[index].position) && finiteVector(first[index].scale);
        valid = valid && first[index].position == second[index].position;
    }

    bool rampsValid = MonumentalSite::ramps().size() >= 2;
    for (const RampDescriptor& ramp : MonumentalSite::ramps())
    {
        rampsValid = rampsValid && finiteVector(ramp.base) && finiteVector(ramp.top) &&
                     ramp.width > world.workerHeight && ramp.thickness > 0.0f &&
                     glm::distance(ramp.base, ramp.top) > 1.0f &&
                     isFiniteNonSingularTransform(MonumentalSite::rampModel(ramp));
    }

    const std::vector<ObjectPart> scaffoldParts = Scaffold::createModule();
    const std::vector<ObjectPart> scaffoldRepeat = Scaffold::createModule();
    bool scaffoldValid = !scaffoldParts.empty() &&
                         scaffoldParts.size() == scaffoldRepeat.size() &&
                         scaffoldParts.size() == Scaffold::partCount();
    for (std::size_t index = 0; index < scaffoldParts.size(); ++index)
    {
        scaffoldValid = scaffoldValid &&
                        isFiniteNonSingularTransform(scaffoldParts[index].localTransform);
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                scaffoldValid = scaffoldValid &&
                                std::abs(scaffoldParts[index].localTransform[column][row] -
                                         scaffoldRepeat[index].localTransform[column][row]) <
                                    1.0e-6f;
    }
    for (const ScaffoldPlacement& placement : MonumentalSite::scaffolds())
        scaffoldValid = scaffoldValid && finiteVector(placement.origin) &&
                        placement.levels > 0 && placement.bays > 0;

    bool zonesValid = MonumentalSite::zones().size() >= 8;
    for (const SiteZoneDescriptor& zone : MonumentalSite::zones())
        zonesValid = zonesValid && finiteVector(zone.center) && finiteVector(zone.extents) &&
                     glm::all(glm::greaterThan(zone.extents, glm::vec3{0.0f}));

    valid = valid && rampsValid && scaffoldValid && zonesValid;
    output << "Phase 5.5 monumental-site validation\n"
           << "  worker height: " << world.workerHeight << "\n"
           << "  pyramid footprint: " << pyramid.baseWidth << " x " << pyramid.baseDepth << "\n"
           << "  completed height: " << pyramid.completedHeight << " ("
           << pyramid.completedHeight / world.workerHeight << " worker-heights)\n"
           << "  pyramid blocks: " << pyramid.totalBlocks << "\n"
           << "  ramps: " << MonumentalSite::ramps().size() << "\n"
           << "  scaffold groups: " << MonumentalSite::scaffolds().size() << "\n"
           << "  site zones: " << MonumentalSite::zones().size() << "\n"
           << (valid ? "Monumental-site checks passed.\n"
                     : "Monumental-site checks failed.\n");
    return valid;
}
