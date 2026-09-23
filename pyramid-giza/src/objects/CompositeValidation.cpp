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

bool equalWorkerTransforms(const Worker::EvaluatedPose& left,
                           const Worker::EvaluatedPose& right)
{
    for (std::size_t part = 0; part < left.size(); ++part)
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (std::abs(left[part].model[column][row] -
                             right[part].model[column][row]) > 1.0e-6f)
                    return false;
    return true;
}

bool validateWorkerDefinition(std::ostream& output)
{
    const Worker::Hierarchy& hierarchy = Worker::hierarchy();
    bool valid = hierarchy.size() == Worker::partCount();
    std::unordered_set<int> identities;
    for (std::size_t index = 0; index < hierarchy.size(); ++index)
    {
        const WorkerNode& node = hierarchy[index];
        valid = identities.insert(static_cast<int>(node.id)).second && valid;
        valid = node.parentIndex < static_cast<int>(index) && valid;
        valid = validPrimitive(node.primitive) && valid;
        valid = node.shapeScale.x > 0.0f && node.shapeScale.y > 0.0f &&
                node.shapeScale.z > 0.0f && valid;
    }
    output << "  Worker hierarchy definition: " << (valid ? "PASS" : "FAIL")
           << " (" << hierarchy.size() << " parts)\n";
    return valid;
}
} // namespace

bool validateCompositeObjects(std::ostream& output)
{
    output << "Phase 3 composite-object validation:\n";
    bool valid = validateWorkerDefinition(output);
    constexpr std::array<WorkerPose, 6> poses{{
        WorkerPose::Standing, WorkerPose::PullingReady, WorkerPose::CarryingReady,
        WorkerPose::LeverReady, WorkerPose::ArmsOut, WorkerPose::BentKnees}};
    Worker::EvaluatedPose standing{};
    for (WorkerPose pose : poses)
    {
        const WorkerJointAngles angles = Worker::poseAngles(pose);
        const Worker::EvaluatedPose first = Worker::evaluate(glm::mat4{1.0f}, angles);
        const Worker::EvaluatedPose second = Worker::evaluate(glm::mat4{1.0f}, angles);
        bool poseValid = Worker::jointAnglesWithinLimits(angles);
        for (const WorkerPartTransform& part : first)
            poseValid = isFiniteNonSingularTransform(part.jointWorld) &&
                        isFiniteNonSingularTransform(part.model) && poseValid;
        output << "  " << Worker::poseName(pose) << ": "
               << (poseValid ? "PASS" : "FAIL") << " (17 evaluated parts)\n";
        valid = poseValid && valid;
        const bool deterministic = equalWorkerTransforms(first, second);
        output << "    deterministic hierarchy: " << (deterministic ? "PASS" : "FAIL") << '\n';
        valid = deterministic && valid;
        if (pose == WorkerPose::Standing)
            standing = first;
        else
        {
            const bool distinct = !equalWorkerTransforms(standing, first);
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
