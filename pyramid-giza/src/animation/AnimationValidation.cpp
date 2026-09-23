#include "animation/AnimationValidation.h"

#include <cmath>
#include <ostream>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>

#include "animation/ConstructionAnimation.h"
#include "objects/Sledge.h"
#include "scene/MonumentalSite.h"

namespace
{
constexpr float epsilon = 1.0e-3f;

bool near(float left, float right, float tolerance = epsilon)
{
    return std::abs(left - right) <= tolerance;
}

bool vectorNear(const glm::vec3& left, const glm::vec3& right, float tolerance = epsilon)
{
    return glm::length(left - right) <= tolerance;
}

bool matrixNear(const glm::mat4& left, const glm::mat4& right, float tolerance = epsilon)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!near(left[column][row], right[column][row], tolerance))
                return false;
    return true;
}

glm::vec3 translationOf(const glm::mat4& matrix)
{
    return glm::vec3{matrix[3]};
}

const ObjectPart& findSledgePart(const std::vector<ObjectPart>& parts, const char* name)
{
    for (const ObjectPart& part : parts)
        if (part.name == name)
            return part;
    throw std::logic_error("Required sledge part missing");
}

void report(std::ostream& output, const char* label, bool passed)
{
    output << "  " << label << ": " << (passed ? "PASS" : "FAIL") << '\n';
}
} // namespace

bool validateConstructionAnimation(std::ostream& output)
{
    output << "Phase 5 coordinated-animation validation:\n";
    bool valid = true;

    ConstructionAnimationController transitions;
    transitions.update(ConstructionAnimationController::stateDuration(ConstructionState::Idle) + 0.25f);
    const bool transitionValid = transitions.state() == ConstructionState::WorkersApproach &&
                                 near(transitions.stateTime(), 0.25f);
    report(output, "Deterministic state transition", transitionValid);
    valid = transitionValid && valid;

    ConstructionAnimationController paused;
    paused.update(8.0f);
    paused.togglePaused();
    const float pausedStateTime = paused.stateTime();
    const float pausedElapsed = paused.elapsedTime();
    const ConstructionAnimationSnapshot pausedBefore = paused.snapshot();
    paused.update(3.0f);
    const ConstructionAnimationSnapshot pausedAfter = paused.snapshot();
    const bool pauseValid = near(paused.stateTime(), pausedStateTime) &&
                            near(paused.elapsedTime(), pausedElapsed) &&
                            matrixNear(pausedBefore.loadedSledgeRoot, pausedAfter.loadedSledgeRoot) &&
                            matrixNear(pausedBefore.workers[0].root, pausedAfter.workers[0].root);
    report(output, "Pause freezes state and transforms", pauseValid);
    valid = pauseValid && valid;

    ConstructionAnimationController resetController;
    resetController.update(19.0f);
    resetController.reset();
    ConstructionAnimationController fresh;
    const ConstructionAnimationSnapshot resetSnapshot = resetController.snapshot();
    const ConstructionAnimationSnapshot freshSnapshot = fresh.snapshot();
    const bool resetValid = resetController.state() == ConstructionState::Idle &&
                            near(resetController.stateTime(), 0.0f) &&
                            near(resetController.elapsedTime(), 0.0f) &&
                            matrixNear(resetSnapshot.loadedSledgeRoot, freshSnapshot.loadedSledgeRoot) &&
                            near(resetSnapshot.leverAngleDegrees, 0.0f) &&
                            near(resetSnapshot.liftedStoneOffset, 0.0f);
    report(output, "Reset restores deterministic start", resetValid);
    valid = resetValid && valid;

    const TransportFrame start = ConstructionAnimationController::transportAt(0.0f);
    const TransportFrame destination = ConstructionAnimationController::transportAt(1.0f);
    const glm::vec3 expectedStart{10.0f, 0.0f, 40.0f};
    const RampDescriptor& mainRamp = MonumentalSite::mainRamp();
    const glm::vec3 expectedDestination{
        mainRamp.top.x, ConstructionAnimationController::rampSurfaceHeight(mainRamp.top.z),
        mainRamp.top.z};
    const bool endpointsValid = vectorNear(start.sledgePosition, expectedStart) &&
                                vectorNear(destination.sledgePosition, expectedDestination) &&
                                glm::distance(start.leftWorkerPosition, start.sledgePosition) > 3.0f &&
                                destination.leftWorkerPosition.z < destination.sledgePosition.z;
    report(output, "Transport path endpoints", endpointsValid);
    valid = endpointsValid && valid;

    const float startDistance = glm::distance(start.leftWorkerPosition, start.sledgePosition);
    const float endDistance = glm::distance(destination.leftWorkerPosition, destination.sledgePosition);
    const bool synchronizationValid = near(startDistance, endDistance, 0.35f) &&
                                      startDistance > 3.0f && endDistance > 3.0f;
    report(output, "Pullers remain synchronized with sledge", synchronizationValid);
    valid = synchronizationValid && valid;

    const std::vector<ObjectPart> loadedSledge = Sledge::create(true);
    const ObjectPart& stone = findSledgePart(loadedSledge, "TransportStone");
    const glm::mat4 startRoot = makeTransform(start.sledgePosition,
                                               {start.rampPitchDegrees, start.headingDegrees, 0.0f},
                                               {1.0f, 1.0f, 1.0f});
    const glm::mat4 endRoot = makeTransform(destination.sledgePosition,
                                             {destination.rampPitchDegrees, destination.headingDegrees, 0.0f},
                                             {1.0f, 1.0f, 1.0f});
    const glm::mat4 stoneStart = startRoot * stone.localTransform;
    const glm::mat4 stoneEnd = endRoot * stone.localTransform;
    const bool stoneAttachmentValid =
        matrixNear(glm::inverse(startRoot) * stoneStart, stone.localTransform) &&
        matrixNear(glm::inverse(endRoot) * stoneEnd, stone.localTransform) &&
        !matrixNear(stoneStart, stoneEnd);
    report(output, "Stone remains rigidly attached to sledge", stoneAttachmentValid);
    valid = stoneAttachmentValid && valid;

    WorkerJointAngles armRaised = Worker::poseAngles(WorkerPose::Standing);
    armRaised.rightShoulder.z = 70.0f;
    armRaised.rightElbow.x = 65.0f;
    const Worker::EvaluatedPose handRest = Worker::evaluate(
        glm::mat4{1.0f}, Worker::poseAngles(WorkerPose::Standing));
    const Worker::EvaluatedPose handMoved = Worker::evaluate(glm::mat4{1.0f}, armRaised);
    const glm::mat4 restHand = handRest[static_cast<std::size_t>(BodyPart::RightHand)].jointWorld;
    const glm::mat4 movedHand = handMoved[static_cast<std::size_t>(BodyPart::RightHand)].jointWorld;
    const glm::mat4 restTool = ConstructionAnimationController::toolAttachmentRoot(restHand);
    const glm::mat4 movedTool = ConstructionAnimationController::toolAttachmentRoot(movedHand);
    const bool toolAttachmentValid =
        matrixNear(glm::inverse(restHand) * restTool, glm::inverse(movedHand) * movedTool) &&
        !matrixNear(restTool, movedTool);
    report(output, "Mallet follows animated hand joint", toolAttachmentValid);
    valid = toolAttachmentValid && valid;

    const WorkerJointAngles blendStart = ConstructionAnimationController::blendPoses(
        Worker::poseAngles(WorkerPose::Standing), Worker::poseAngles(WorkerPose::PullingReady), 0.0f);
    const WorkerJointAngles blendEnd = ConstructionAnimationController::blendPoses(
        Worker::poseAngles(WorkerPose::Standing), Worker::poseAngles(WorkerPose::PullingReady), 1.0f);
    const bool blendValid = near(blendStart.torso.x, 0.0f) && near(blendEnd.torso.x, 10.0f) &&
                            Worker::jointAnglesWithinLimits(blendStart) &&
                            Worker::jointAnglesWithinLimits(blendEnd);
    report(output, "Pose interpolation preserves limits", blendValid);
    valid = blendValid && valid;

    ConstructionAnimationController sixtyHz;
    ConstructionAnimationController thirtyHz;
    for (int step = 0; step < 1140; ++step)
        sixtyHz.update(1.0f / 60.0f);
    for (int step = 0; step < 570; ++step)
        thirtyHz.update(1.0f / 30.0f);
    const ConstructionAnimationSnapshot sixtySnapshot = sixtyHz.snapshot();
    const ConstructionAnimationSnapshot thirtySnapshot = thirtyHz.snapshot();
    const bool frameRateValid = sixtyHz.state() == thirtyHz.state() &&
                                near(sixtyHz.stateTime(), thirtyHz.stateTime(), 0.01f) &&
                                matrixNear(sixtySnapshot.loadedSledgeRoot,
                                           thirtySnapshot.loadedSledgeRoot, 0.02f) &&
                                matrixNear(sixtySnapshot.workers[0].root,
                                           thirtySnapshot.workers[0].root, 0.02f);
    report(output, "Frame-rate-independent playback", frameRateValid);
    valid = frameRateValid && valid;

    const glm::vec3 ropeStart{-1.0f, 1.2f, 0.4f};
    const glm::vec3 ropeEnd{2.0f, 0.5f, -3.0f};
    const glm::mat4 rope = ConstructionAnimationController::cylinderBetween(
        ropeStart, ropeEnd, 0.06f);
    const glm::vec3 ropeLocalStart{rope * glm::vec4{0.0f, -0.5f, 0.0f, 1.0f}};
    const glm::vec3 ropeLocalEnd{rope * glm::vec4{0.0f, 0.5f, 0.0f, 1.0f}};
    const bool ropeValid = isFiniteNonSingularTransform(rope) &&
                           vectorNear(ropeLocalStart, ropeStart, 0.002f) &&
                           vectorNear(ropeLocalEnd, ropeEnd, 0.002f);
    report(output, "Rope cylinder aligns between endpoints", ropeValid);
    valid = ropeValid && valid;

    const glm::mat4 pivotBefore = ConstructionAnimationController::leverPivotFrame(0.0f);
    const glm::mat4 pivotAfter = ConstructionAnimationController::leverPivotFrame(-12.0f);
    const glm::mat4 beamBefore = ConstructionAnimationController::leverBeamModel(0.0f);
    const glm::mat4 beamAfter = ConstructionAnimationController::leverBeamModel(-12.0f);
    const glm::mat4 stoneBefore = ConstructionAnimationController::leverStoneModel(0.0f);
    const glm::mat4 stoneAfter = ConstructionAnimationController::leverStoneModel(0.24f);
    const bool leverValid = vectorNear(translationOf(pivotBefore), translationOf(pivotAfter)) &&
                            !matrixNear(beamBefore, beamAfter) &&
                            near(translationOf(stoneAfter).y - translationOf(stoneBefore).y, 0.24f);
    report(output, "Lever rotates at fulcrum and lifts stone", leverValid);
    valid = leverValid && valid;

    ConstructionAnimationController complete;
    complete.toggleLooping();
    complete.update(ConstructionAnimationController::sequenceDuration());
    const bool completionValid = complete.state() == ConstructionState::Complete &&
                                 complete.paused() &&
                                 near(complete.stateTime(),
                                      ConstructionAnimationController::stateDuration(
                                          ConstructionState::Complete));
    report(output, "Full non-looping sequence reaches Complete", completionValid);
    valid = completionValid && valid;

    ConstructionAnimationController finiteController;
    bool finiteSnapshots = true;
    for (int state = 0; state < 10; ++state)
    {
        const ConstructionAnimationSnapshot sample = finiteController.snapshot();
        finiteSnapshots = isFiniteNonSingularTransform(sample.loadedSledgeRoot) && finiteSnapshots;
        for (const AnimatedWorkerState& worker : sample.workers)
            finiteSnapshots = isFiniteNonSingularTransform(worker.root) &&
                              Worker::jointAnglesWithinLimits(worker.joints) && finiteSnapshots;
        finiteController.advanceState();
    }
    report(output, "All state snapshots are finite and limited", finiteSnapshots);
    valid = finiteSnapshots && valid;

    output << "Coordinated-animation validation " << (valid ? "passed." : "failed.") << '\n';
    return valid;
}
