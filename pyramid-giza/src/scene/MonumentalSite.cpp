#include "scene/MonumentalSite.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "objects/Scaffold.h"
#include "scene/PyramidLayout.h"

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
