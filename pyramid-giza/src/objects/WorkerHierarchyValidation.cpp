#include "objects/WorkerHierarchyValidation.h"

#include <cmath>
#include <ostream>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "objects/Worker.h"

namespace
{
constexpr float epsilon = 1.0e-4f;

const WorkerPartTransform& part(const Worker::EvaluatedPose& pose, BodyPart id)
{
    for (const WorkerPartTransform& candidate : pose)
        if (candidate.id == id)
            return candidate;
    throw std::logic_error("Required body part missing from evaluated worker");
}

bool matrixNear(const glm::mat4& left, const glm::mat4& right)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (std::abs(left[column][row] - right[column][row]) > epsilon)
                return false;
    return true;
}

bool matrixDifferent(const glm::mat4& left, const glm::mat4& right)
{
    return !matrixNear(left, right);
}

bool vectorNear(const glm::vec3& left, const glm::vec3& right)
{
    return glm::length(left - right) <= epsilon;
}

void report(std::ostream& output, const char* name, bool passed)
{
    output << "  " << name << ": " << (passed ? "PASS" : "FAIL") << '\n';
}
} // namespace

bool validateWorkerHierarchy(std::ostream& output)
{
    output << "Phase 4 hierarchical-worker validation:\n";
    bool valid = true;
    const WorkerJointAngles standingAngles = Worker::poseAngles(WorkerPose::Standing);
    const Worker::EvaluatedPose standing = Worker::evaluate(glm::mat4{1.0f}, standingAngles);

    // Test A: shoulder affects its complete arm chain, never the opposite arm.
    WorkerJointAngles shoulderAngles = standingAngles;
    shoulderAngles.leftShoulder.z = -70.0f;
    const Worker::EvaluatedPose shoulder = Worker::evaluate(glm::mat4{1.0f}, shoulderAngles);
    const bool shoulderPropagation =
        matrixDifferent(part(standing, BodyPart::LeftUpperArm).model,
                        part(shoulder, BodyPart::LeftUpperArm).model) &&
        matrixDifferent(part(standing, BodyPart::LeftForearm).model,
                        part(shoulder, BodyPart::LeftForearm).model) &&
        matrixDifferent(part(standing, BodyPart::LeftHand).model,
                        part(shoulder, BodyPart::LeftHand).model) &&
        matrixNear(part(standing, BodyPart::RightUpperArm).model,
                   part(shoulder, BodyPart::RightUpperArm).model) &&
        matrixNear(part(standing, BodyPart::RightForearm).model,
                   part(shoulder, BodyPart::RightForearm).model) &&
        matrixNear(part(standing, BodyPart::RightHand).model,
                   part(shoulder, BodyPart::RightHand).model);
    report(output, "Test A - shoulder propagates only down left arm", shoulderPropagation);
    valid = shoulderPropagation && valid;

    // Test B: elbow leaves the upper arm frame fixed but moves forearm and hand.
    WorkerJointAngles elbowAngles = standingAngles;
    elbowAngles.leftElbow.x = 75.0f;
    const Worker::EvaluatedPose elbow = Worker::evaluate(glm::mat4{1.0f}, elbowAngles);
    const bool elbowPropagation =
        matrixNear(part(standing, BodyPart::LeftUpperArm).jointWorld,
                   part(elbow, BodyPart::LeftUpperArm).jointWorld) &&
        matrixNear(part(standing, BodyPart::LeftUpperArm).model,
                   part(elbow, BodyPart::LeftUpperArm).model) &&
        matrixDifferent(part(standing, BodyPart::LeftForearm).model,
                        part(elbow, BodyPart::LeftForearm).model) &&
        matrixDifferent(part(standing, BodyPart::LeftHand).model,
                        part(elbow, BodyPart::LeftHand).model);
    report(output, "Test B - elbow moves forearm and hand only", elbowPropagation);
    valid = elbowPropagation && valid;

    // Test C: knee leaves thigh fixed but moves lower leg and foot.
    WorkerJointAngles kneeAngles = standingAngles;
    kneeAngles.leftKnee.x = 65.0f;
    const Worker::EvaluatedPose knee = Worker::evaluate(glm::mat4{1.0f}, kneeAngles);
    const bool kneePropagation =
        matrixNear(part(standing, BodyPart::LeftThigh).model,
                   part(knee, BodyPart::LeftThigh).model) &&
        matrixDifferent(part(standing, BodyPart::LeftLowerLeg).model,
                        part(knee, BodyPart::LeftLowerLeg).model) &&
        matrixDifferent(part(standing, BodyPart::LeftFoot).model,
                        part(knee, BodyPart::LeftFoot).model);
    report(output, "Test C - knee moves lower leg and foot only", kneePropagation);
    valid = kneePropagation && valid;

    WorkerJointAngles hipAngles = standingAngles;
    hipAngles.leftHip.x = -35.0f;
    const Worker::EvaluatedPose hip = Worker::evaluate(glm::mat4{1.0f}, hipAngles);
    const bool hipPropagation =
        matrixDifferent(part(standing, BodyPart::LeftThigh).model,
                        part(hip, BodyPart::LeftThigh).model) &&
        matrixDifferent(part(standing, BodyPart::LeftLowerLeg).model,
                        part(hip, BodyPart::LeftLowerLeg).model) &&
        matrixDifferent(part(standing, BodyPart::LeftFoot).model,
                        part(hip, BodyPart::LeftFoot).model) &&
        matrixNear(part(standing, BodyPart::RightThigh).model,
                   part(hip, BodyPart::RightThigh).model);
    report(output, "Hip rotation propagates down one leg", hipPropagation);
    valid = hipPropagation && valid;

    WorkerJointAngles ankleAngles = standingAngles;
    ankleAngles.leftAnkle.x = 25.0f;
    const Worker::EvaluatedPose ankle = Worker::evaluate(glm::mat4{1.0f}, ankleAngles);
    const bool anklePropagation =
        matrixNear(part(standing, BodyPart::LeftLowerLeg).model,
                   part(ankle, BodyPart::LeftLowerLeg).model) &&
        matrixDifferent(part(standing, BodyPart::LeftFoot).model,
                        part(ankle, BodyPart::LeftFoot).model);
    report(output, "Ankle rotation affects foot only", anklePropagation);
    valid = anklePropagation && valid;

    WorkerJointAngles torsoAngles = standingAngles;
    torsoAngles.torso.y = 30.0f;
    const Worker::EvaluatedPose torso = Worker::evaluate(glm::mat4{1.0f}, torsoAngles);
    const bool torsoPropagation =
        matrixNear(part(standing, BodyPart::Pelvis).model,
                   part(torso, BodyPart::Pelvis).model) &&
        matrixDifferent(part(standing, BodyPart::Torso).model,
                        part(torso, BodyPart::Torso).model) &&
        matrixDifferent(part(standing, BodyPart::Neck).model,
                        part(torso, BodyPart::Neck).model) &&
        matrixDifferent(part(standing, BodyPart::Head).model,
                        part(torso, BodyPart::Head).model) &&
        matrixDifferent(part(standing, BodyPart::Headwear).model,
                        part(torso, BodyPart::Headwear).model);
    report(output, "Torso rotation carries neck/head/headwear", torsoPropagation);
    valid = torsoPropagation && valid;

    // Test D: root translation shifts every visible part by the same vector.
    const glm::vec3 rootShift{3.0f, 2.0f, -4.0f};
    const Worker::EvaluatedPose moved = Worker::evaluate(
        glm::translate(glm::mat4{1.0f}, rootShift), standingAngles);
    bool rootPropagation = true;
    for (std::size_t index = 0; index < standing.size(); ++index)
    {
        const glm::vec3 before{standing[index].model[3]};
        const glm::vec3 after{moved[index].model[3]};
        rootPropagation = vectorNear(after - before, rootShift) && rootPropagation;
    }
    report(output, "Test D - root transform moves every body part", rootPropagation);
    valid = rootPropagation && valid;

    // Every child joint origin must be parentJoint * local joint offset. Shape scale is absent.
    bool pivotsValid = true;
    const Worker::Hierarchy& hierarchy = Worker::hierarchy();
    for (std::size_t index = 1; index < hierarchy.size(); ++index)
    {
        const WorkerNode& node = hierarchy[index];
        const glm::mat4 expected = glm::translate(
            standing[static_cast<std::size_t>(node.parentIndex)].jointWorld, node.jointOffset);
        pivotsValid = vectorNear(glm::vec3{expected[3]}, glm::vec3{standing[index].jointWorld[3]}) &&
                      pivotsValid;
    }
    report(output, "Joint pivots follow unscaled parent frames", pivotsValid);
    valid = pivotsValid && valid;

    WorkerJointAngles outside;
    outside.pelvis = outside.torso = outside.neck = outside.head = {999.0f, -999.0f, 999.0f};
    outside.leftShoulder = outside.leftElbow = outside.leftWrist = {999.0f, -999.0f, 999.0f};
    outside.rightShoulder = outside.rightElbow = outside.rightWrist = {999.0f, -999.0f, 999.0f};
    outside.leftHip = outside.leftKnee = outside.leftAnkle = {999.0f, -999.0f, 999.0f};
    outside.rightHip = outside.rightKnee = outside.rightAnkle = {999.0f, -999.0f, 999.0f};
    const WorkerJointAngles clamped = Worker::clampJointAngles(outside);
    const bool limitsValid = !Worker::jointAnglesWithinLimits(outside) &&
                             Worker::jointAnglesWithinLimits(clamped);
    report(output, "Joint limits clamp invalid input", limitsValid);
    valid = limitsValid && valid;

    bool posesValid = true;
    for (WorkerPose pose : {WorkerPose::Standing, WorkerPose::PullingReady,
                            WorkerPose::CarryingReady, WorkerPose::LeverReady,
                            WorkerPose::ArmsOut, WorkerPose::BentKnees})
    {
        const WorkerJointAngles angles = Worker::poseAngles(pose);
        const Worker::EvaluatedPose first = Worker::evaluate(glm::mat4{1.0f}, angles);
        const Worker::EvaluatedPose second = Worker::evaluate(glm::mat4{1.0f}, angles);
        posesValid = Worker::jointAnglesWithinLimits(angles) && posesValid;
        for (std::size_t index = 0; index < first.size(); ++index)
            posesValid = matrixNear(first[index].model, second[index].model) &&
                         isFiniteNonSingularTransform(first[index].model) && posesValid;
    }
    report(output, "Rest/diagnostic poses are finite and deterministic", posesValid);
    valid = posesValid && valid;

    const bool feetGrounded =
        std::abs(part(standing, BodyPart::LeftFoot).model[3].y - 0.10f) < epsilon &&
        std::abs(part(standing, BodyPart::RightFoot).model[3].y - 0.10f) < epsilon;
    report(output, "Rest-pose feet meet local ground", feetGrounded);
    valid = feetGrounded && valid;

    output << "Hierarchical-worker validation " << (valid ? "passed." : "failed.") << '\n';
    return valid;
}
