#include "scene/MonumentalSite.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>

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
    const glm::vec3 direction = ramp.base - ramp.top;
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
        {"MainHaulingRamp", {0.0f, 0.35f, 45.0f}, {0.0f, 8.20f, -0.60f},
         scale().mainRampWidth, 0.70f, MaterialId::RampEarth, true},
        {"WestAccessRamp", {-63.0f, 0.30f, -25.0f}, {-41.6f, 5.30f, -25.0f},
         4.5f, 0.60f, MaterialId::RampEarth, false},
        {"UpperConnector", {9.0f, 8.40f, 0.20f}, {20.0f, 13.20f, -11.0f},
         3.6f, 0.52f, MaterialId::RampEarth, false}
    };
    return values;
}

const RampDescriptor& MonumentalSite::mainRamp()
{
    return ramps().front();
}

const std::vector<ScaffoldPlacement>& MonumentalSite::scaffolds()
{
    static const std::vector<ScaffoldPlacement> values{
        {"FrontWest", {-24.0f, 0.0f, 1.6f}, 0.0f, 3, 3, "Lower active face"},
        {"FrontEast", {12.0f, 0.0f, 1.6f}, 0.0f, 3, 3, "Lower active face"},
        {"RampTop", {-5.0f, 8.55f, -0.2f}, 0.0f, 2, 2, "Ramp-top staging"}
    };
    return values;
}

const std::vector<SiteZoneDescriptor>& MonumentalSite::zones()
{
    static const std::vector<SiteZoneDescriptor> values{
        {"PyramidZone", {0.0f, 23.0f, -42.0f}, {42.0f, 23.0f, 42.0f}, "Main monument"},
        {"RampNetwork", {0.0f, 5.0f, 22.0f}, {34.0f, 8.0f, 24.0f}, "Primary access"},
        {"ScaffoldZone", {0.0f, 8.0f, 1.5f}, {31.0f, 9.0f, 4.0f}, "Active face access"},
        {"QuarryZone", {-72.0f, 2.5f, -20.0f}, {18.0f, 5.0f, 20.0f}, "Stone extraction"},
        {"CuttingZone", {-49.0f, 2.0f, 18.0f}, {12.0f, 4.0f, 12.0f}, "Stone preparation"},
        {"StockpileZone", {14.0f, 2.0f, 33.0f}, {15.0f, 4.0f, 12.0f}, "Prepared transport stones"},
        {"TransportZone", {0.0f, 1.0f, 40.0f}, {32.0f, 2.0f, 8.0f}, "Open hauling corridor"},
        {"TimberYard", {48.0f, 2.0f, 7.0f}, {13.0f, 4.0f, 14.0f}, "Wood and scaffold storage"},
        {"WorkCamp", {51.0f, 2.5f, -25.0f}, {14.0f, 5.0f, 12.0f}, "Shelter and tools"}
    };
    return values;
}

glm::mat4 MonumentalSite::rampModel(const RampDescriptor& ramp)
{
    if (ramp.width <= 0.0f || ramp.thickness <= 0.0f)
        throw std::invalid_argument("Ramp width and thickness must be positive");

    const glm::vec3 forward = rampForward(ramp);
    const glm::vec3 right = rampRight(ramp);
    const glm::vec3 up = rampUp(ramp);
    const float length = glm::distance(ramp.base, ramp.top);
    glm::mat4 model{1.0f};
    model[0] = glm::vec4{right * ramp.width, 0.0f};
    model[1] = glm::vec4{up * ramp.thickness, 0.0f};
    model[2] = glm::vec4{forward * length, 0.0f};
    model[3] = glm::vec4{0.5f * (ramp.base + ramp.top), 1.0f};
    return model;
}

glm::vec3 MonumentalSite::rampSurfacePoint(const RampDescriptor& ramp, float progress)
{
    const float t = std::clamp(progress, 0.0f, 1.0f);
    return glm::mix(ramp.base, ramp.top, t) + rampUp(ramp) * (0.5f * ramp.thickness);
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
