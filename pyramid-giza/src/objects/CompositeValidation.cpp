#include "objects/CompositeValidation.h"

#include <array>
#include <cmath>
#include <ostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "objects/ConstructionProps.h"
#include "objects/Sledge.h"
#include "objects/Worker.h"

namespace
{
bool validPrimitive(ScenePrimitive primitive)
{
    return primitive == ScenePrimitive::Plane || primitive == ScenePrimitive::Cube ||
           primitive == ScenePrimitive::Cylinder || primitive == ScenePrimitive::Sphere;
}

bool validateParts(const char* label, const std::vector<ObjectPart>& parts,
                   std::size_t expectedCount, std::ostream& output)
{
    bool valid = parts.size() == expectedCount;
    std::unordered_set<std::string> names;
    for (const ObjectPart& objectPart : parts)
    {
        valid = !objectPart.name.empty() && !objectPart.parentHint.empty() && valid;
        valid = names.insert(objectPart.name).second && valid;
        valid = validPrimitive(objectPart.primitive) && valid;
        valid = isFiniteNonSingularTransform(objectPart.localTransform) && valid;
        try
        {
            static_cast<void>(materialDefinition(objectPart.material));
        }
        catch (...)
        {
            valid = false;
        }
    }
    output << "  " << label << ": " << (valid ? "PASS" : "FAIL")
           << " (" << parts.size() << " parts)\n";
    return valid;
}

bool equalTransforms(const std::vector<ObjectPart>& left,
                     const std::vector<ObjectPart>& right)
{
    if (left.size() != right.size())
        return false;
    for (std::size_t part = 0; part < left.size(); ++part)
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (std::abs(left[part].localTransform[column][row] -
                             right[part].localTransform[column][row]) > 1.0e-6f)
                    return false;
    return true;
}
} // namespace

bool validateCompositeObjects(std::ostream& output)
{
    output << "Phase 3 composite-object validation:\n";
    bool valid = true;
    constexpr std::array<WorkerPose, 4> poses{{WorkerPose::Standing, WorkerPose::PullingReady,
                                               WorkerPose::CarryingReady, WorkerPose::LeverReady}};
    std::vector<ObjectPart> standing;
    for (WorkerPose pose : poses)
    {
        const std::vector<ObjectPart> first = Worker::create(pose);
        const std::vector<ObjectPart> second = Worker::create(pose);
        valid = validateParts(Worker::poseName(pose), first, Worker::partCount(), output) && valid;
        const bool deterministic = equalTransforms(first, second);
        output << "    deterministic transforms: " << (deterministic ? "PASS" : "FAIL") << '\n';
        valid = deterministic && valid;
        if (pose == WorkerPose::Standing)
            standing = first;
        else
        {
            const bool distinct = !equalTransforms(standing, first);
            output << "    distinct from standing pose: " << (distinct ? "PASS" : "FAIL") << '\n';
            valid = distinct && valid;
        }
    }

    valid = validateParts("Sledge (unloaded)", Sledge::create(false),
                          Sledge::unloadedPartCount(), output) && valid;
    valid = validateParts("Sledge (loaded)", Sledge::create(true),
                          Sledge::loadedPartCount(), output) && valid;
    valid = validateParts("Lever", ConstructionProps::createLever(), 3, output) && valid;
    valid = validateParts("Mallet", ConstructionProps::createMallet(), 2, output) && valid;
    valid = validateParts("Wooden frame", ConstructionProps::createWoodenFrame(), 5, output) && valid;
    valid = validateParts("Timber roller", ConstructionProps::createRoller(), 1, output) && valid;
    output << "Composite-object validation " << (valid ? "passed." : "failed.") << '\n';
    return valid;
}
