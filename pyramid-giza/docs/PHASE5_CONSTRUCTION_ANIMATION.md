# Phase 5 - Coordinated Construction Animation

## Objective

Phase 5 turns the Phase 4 articulated workers and Phase 3 equipment into one simple,
repeatable construction story. Two workers approach a loaded sledge, pull it across the
desert and up the ramp, then a lever operator demonstrates a small placement lift. Quarry
and support workers continue short role-specific motions. This is a deterministic course
demonstration, not a physics simulation.

The pyramid, desert, quarry, stockpile, ramp, second sledge, frames, and all shared
primitive meshes are preserved.

## Architecture

```text
deltaTime * playbackSpeed
          |
ConstructionAnimationController
          |
          +-- current state and local state time
          +-- shared transport progress
          +-- seven worker roots and joint angles
          +-- loaded-sledge root and rope visibility
          `-- lever angle and placement-stone offset
                         |
                  StaticGizaScene::draw
                         |
          shared Worker / Sledge / primitive meshes
```

`ConstructionAnimationController` is CPU-only. It owns no OpenGL handles and creates no
buffers. `ConstructionAnimationSnapshot` is the small per-frame result consumed by the
scene. Static layout data is still built once; each update evaluates matrices and joint
angles only.

## States and timing

The ordered states are:

```text
Idle -> WorkersApproach -> PullReady -> PullGround -> ApproachRamp
     -> RampPull -> Arrival -> LeverPreparation -> PlacementReady -> Complete
```

The sequence lasts 28.5 seconds at 1x. Each state has a fixed duration recorded in
`animation_states.csv`. State transitions consume any leftover delta time, so a large
frame does not lose time. Playback speed is clamped to 0.25x-4x. The final state holds for
two seconds and then resets cleanly when looping is enabled; disabling looping pauses on
the completed frame.

A smooth interpolation, `t*t*(3-2*t)`, removes sudden starts and stops inside a state.
Pose fields use linear interpolation followed by the existing Phase 4 joint-limit clamp.

## Transport path and synchronization

The loaded sledge uses one normalized progress value:

```text
stockpile start (6, 0, 9.15)
        |
ground turn (0, 0, 13.5)
        |
ramp entry (0, rampHeight, 12.5)
        |
ramp top (0, rampHeight, -1)
```

Ground pulling uses progress 0.00-0.35, the turn/ramp approach uses 0.35-0.45, and ramp
pulling uses 0.45-1.00. Heading comes from the current horizontal path direction. Ramp
pitch is 10 degrees. The ramp surface is:

```text
y = 2.0 - (z - 5.35) * tan(10 degrees) + 0.24
```

Both puller positions are derived from the same sledge frame: 3.7 units ahead along the
travel direction and 0.48 units to either side. This single source of truth prevents the
workers from drifting away from the sledge. Exact points and formulas are in
`animation_paths.csv`.

## Rigid attachment and ropes

The loaded stone remains a local child of the sledge:

```text
stoneWorld = animatedSledgeRoot * stoneLocalTransform
```

It cannot slide independently while the sledge turns or climbs. Each rope is a shared
cylinder scaled and oriented between an evaluated hand-joint position and a sledge tow
point. Thus rope endpoints follow both the articulated hand and moving vehicle without a
new rope mesh or buffer.

## Worker roles and articulated motion

The seven existing workers retain stable roles. Pullers use the Phase 4 `PullingReady`
pose plus a sinusoidal gait. The ramp guide follows beside the load on the incline. The
quarry worker swings a mallet using shoulder, elbow, and torso joints; the mallet root is
attached to the evaluated right-hand joint. The carrier and stockpile workers use small
carrying motions. The lever operator blends from Standing to `LeverReady`.

All angles are clamped by `Worker::clampJointAngles`. Workers share the immutable 17-node
hierarchy definition and the same cube, cylinder, and sphere GPU meshes. Per-instance
state consists only of a root matrix, joint angles, and material choice. Roles and initial
coordinates are listed in `worker_animation_roles.csv`.

## Lever pivot

The lever root is near the work area at `(13.3, 0, -1.6)`. Its joint frame first moves to
the fulcrum, then rotates, and only then offsets/scales the beam:

```text
leverRoot * translateToFulcrum * rotateZ * beamLocalShape
```

The mesh-only scale does not affect the fulcrum. During `LeverPreparation` the beam
changes by -12 degrees. During `PlacementReady`, a nearby stone rises 0.24 world units as
a deliberately small proof of coordinated placement.

## Coordinate spaces

Animation changes world-space root transforms and local joint rotations; it does not edit
canonical vertices. The complete route is:

```text
canonical primitive -> local equipment/body part -> articulated or equipment root
-> world -> view -> projection -> clip -> perspective divide -> NDC/CVV
```

World coordinates may be much larger than `[-1,+1]`. Only visible coordinates after the
perspective divide are interpreted in the canonical view volume.

## Rendering and mesh reuse

Filled geometry still ends at `glDrawElements(GL_TRIANGLES, ...)`. Existing VAO/VBO/EBO
objects are allocated once for reusable primitive meshes. Animation allocates no GPU
resources, uploads no duplicate worker or sledge geometry, and introduces no imported
models, skinning, inverse kinematics, negative scales, or legacy OpenGL.

The inverse-transpose normal matrix remains active for non-uniformly scaled bodies,
equipment, ropes, and stones. CCW front faces and back-face culling are unchanged.

## Controls

- `Space`: pause or resume
- `R`: reset to Idle
- `N`: advance one state
- `L`: toggle looping
- `M`: toggle coordinated Phase 5 animation and the Phase 4 pose demonstration
- `+/-`: change speed between 0.25x and 4x
- `P`: cycle Phase 4 debug poses
- `1`-`6`: camera presets; preset 6 focuses on the lever
- `C`: toggle culling
- `F`: toggle wireframe

For deterministic inspection, `--animation-time SECONDS` initializes a known sequence
time before a smoke-test frame.

## Validation

The OpenGL-independent Phase 5 test covers deterministic transitions, pause, reset, path
endpoints, worker/sledge synchronization, rigid stone attachment, hand-attached mallet,
joint-limited pose blending, frame-rate independence, rope endpoint alignment, lever
pivot behavior, final-state completion, and finite matrices through every state.

Runtime smoke tests inspect filled, wireframe, and culling-disabled modes at ground pull,
ramp pull, and placement/lever times. `glGetError` is checked before exit.

## Deliberately deferred

Phase 5 does not add physics, collision detection, inverse kinematics, foot planting,
rope simulation, loading/unloading logic, permanent pyramid-block placement, advanced
materials, textures, shadows, particles, or camera cinematics. The small lever lift is a
coordination proof rather than a complete stone-installation system.
