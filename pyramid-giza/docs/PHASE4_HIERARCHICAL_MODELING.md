# Phase 4 - Hierarchical Modeling and Articulated Workers

## Objective

Phase 4 converts the Phase 3 worker from independently placed primitives into a rigid-part
parent-child hierarchy. A shoulder rotation now carries its forearm and hand; an elbow
rotation carries the forearm and hand without moving the upper arm; a knee carries the
lower leg and foot. The Giza environment, sledges, and equipment remain intact and static.

Phase 4 deliberately does not implement world-space walking, sledge transport, coordinated
construction sequences, physics, inverse kinematics, or skinned meshes.

## Hierarchy

```text
WorkerRoot
  `-- Pelvis
      |-- Torso
      |   |-- Neck
      |   |   `-- Head
      |   |       `-- Headwear
      |   |-- LeftUpperArm
      |   |   `-- LeftForearm
      |   |       `-- LeftHand
      |   `-- RightUpperArm
      |       `-- RightForearm
      |           `-- RightHand
      |-- LeftThigh
      |   `-- LeftLowerLeg
      |       `-- LeftFoot
      `-- RightThigh
          `-- RightLowerLeg
              `-- RightFoot
```

`WorkerNode` stores a stable `BodyPart`, parent index, primitive, joint offset, bind
rotation, mesh shape offset/scale, and base material. Nodes are in parent-first order, so
evaluation is deterministic and iterative rather than a general-purpose skeletal engine.
The one immutable 17-node definition is shared by all seven worker instances.

## Local and world transforms

A joint offset is relative to the parent joint frame. A part's visible primitive is then
offset and scaled relative to its own joint. With GLM column-vector convention, Phase 4
uses:

```text
JointWorld[i]
  = ParentJointWorld
  * Translate(jointOffset)
  * RotateY
  * RotateX
  * RotateZ

PartModel[i]
  = JointWorld[i]
  * Translate(shapeOffset)
  * Scale(shapeScale)
```

The root node uses the scene's worker root matrix as `ParentJointWorld`. The complete path
is therefore scene/world root -> joint frames -> mesh-only shape transform -> shared
primitive -> view -> projection -> clip space -> NDC/CVV.

## Joint pivots

Long primitives are not rotated around their centers. The upper arm joint is at the
shoulder and its cylinder extends down by half its length. The forearm joint begins at the
elbow, the hand at the wrist, the thigh at the hip, the lower leg at the knee, the foot at
the ankle, and the head at the neck. Exact offsets are in `worker_hierarchy.csv`.

For example:

```text
Shoulder joint frame
       |
       +-- mesh offset to upper-arm center
       |
       `-- elbow joint offset (full upper-arm length)
               |
               +-- mesh offset to forearm center
               `-- wrist joint offset
```

## Preventing scale contamination

`jointWorld` and `model` are separate matrices. Children inherit only `jointWorld`.
Cylinder length/radius and cuboid proportions appear only in `model`. Therefore a thin,
long upper-arm cylinder cannot squash or stretch the forearm joint frame. CPU validation
checks every child origin against `parentJointWorld * Translate(jointOffset)`.

## Rest/bind pose

`Standing` is the neutral bind pose: all joint-angle values are zero and the hierarchy
table supplies the local joint/shape offsets. The worker root sits on local ground Y=0;
the feet have centers at Y=0.10 and height 0.20. Approximate total height is 2.26 world
units. No action pose is baked into the geometry definition.

## Joint angles and limits

`WorkerJointAngles` centralizes pelvis, torso, neck/head, shoulders, elbows/wrists,
hips, knees, and ankles. Values are degrees. `clampJointAngles` applies deterministic
per-axis limits before evaluation; non-finite matrices are rejected. Limits are purposely
simple rather than a claim of exact biomechanics. See `worker_joint_limits.csv`.

Typical ranges include:

- elbows: 0 to 145 degrees of X bend;
- knees: 0 to 130 degrees of X bend;
- shoulders: broad pitch/yaw/raise ranges;
- hips: -90 to 60 degrees of pitch;
- head: +/-70 degrees yaw and smaller pitch/roll;
- ankles and wrists: restricted three-axis adjustment.

No negative shape scale is used, so the established CCW front-face convention is
preserved.

## Pose system

Action poses are joint-angle presets, not independent part matrices:

- `Standing`: neutral bind/rest pose;
- `PullingReady`: forward torso, reaching shoulders/elbows, staggered legs;
- `CarryingReady`: symmetric raised/bent arms;
- `LeverReady`: asymmetric reach and braced knees;
- `ArmsOut`: diagnostic T-pose;
- `BentKnees`: diagnostic crouch.

Meaningful deviations from rest are listed in `worker_poses.csv`. Each scene worker owns
its root, pose/joint state, and material style while sharing the immutable hierarchy and
primitive meshes.

## Propagation examples

Shoulder evaluation starts at the torso's unscaled joint frame. Its rotation changes the
upper-arm model and the elbow joint frame, so forearm and hand follow automatically.
Elbow rotation is applied after the full upper-arm-length joint offset; it changes the
forearm and wrist/hand but cannot change the already evaluated upper arm.

The leg chain is equivalent: the hip moves thigh, knee, lower leg, ankle, and foot; the
knee moves only lower leg, ankle, and foot; the ankle changes the foot only. Torso motion
carries neck, head, headwear, and arms while leaving the pelvis unchanged.

## Root transforms and mesh reuse

Each worker root contains its Giza-world translation and Y rotation. Evaluating a different
root relocates all 17 parts consistently. Workers never create GPU resources: all heads
and hands use the scene's one sphere mesh, all limbs use one cylinder mesh, and all
torsos/pelves/feet use one cube mesh. Rendering remains indexed
`glDrawElements(GL_TRIANGLES, ...)` with VAO/VBO/EBO resources created once.

The inverse-transpose normal matrix is still calculated from each final part model, so
non-uniform limb, torso, foot, and headwear scales light correctly.

## Articulation preview and controls

The stockpile worker is the Phase 4 demonstration worker. A small stationary sine-based
overlay moves its head, left shoulder, elbow, and knee. It proves live propagation without
moving the worker root or any sledge.

- `Space`: pause/resume the articulation clock;
- `P`: cycle Standing, PullingReady, CarryingReady, LeverReady, ArmsOut, BentKnees;
- `R`: reset the demo worker to Standing at time zero;
- `1` to `5`: camera presets;
- `C`: toggle culling;
- `F`: toggle wireframe;
- existing camera and `Esc` controls are unchanged.

## CPU validation

The Phase 4 test requires no OpenGL context. It verifies shoulder, elbow, hip, knee,
ankle, torso/head, and root propagation; parent-first pivot placement; absence of shape
scale in child frames; finite/non-singular matrices; grounded rest feet; joint clamping;
and deterministic action/diagnostic poses.

## Preparation for Phase 5

Phase 5 can change per-instance `WorkerJointAngles` over time while reusing the current
hierarchy evaluator. It can also animate worker root transforms separately. Phase 4 does
not yet add locomotion paths, foot planting, rope/sledge motion, stone loading, lever
sequences, group coordination, physics, skinning, or a general animation engine.
