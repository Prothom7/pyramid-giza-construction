# Phase 12.5 - Post-showcase Construction Repair

> Phase 12.6 supersedes the support-related implementation details below. The final
> scene uses continuously filled ramps, three ground-founded scaffold groups, and no
> upper connector or ramp-side stairs. See
> [Phase 12.6](PHASE12_6_GROUNDING_AND_SUPPORT_REPAIR.md).

## Objective and diagnosis

This pass repairs two visible presentation defects without replacing Phase 12 or the
rendering architecture: block batches appeared to pop into existence during the build
shot, and temporary access structures could remain too close to later pyramid courses.

Inspection showed that Phase 12 was not seeking construction progress every frame.
`synchronizeShowcase` synchronizes simulation only when a shot starts or the user seeks;
ordinary frames call `StaticGizaScene::update`, which advances
`ConstructionTimelineController::update`. The popping came from coarse twelve-wave
thresholds combined with a sparse frontier that selected hidden or interior blocks. The
geometry problem came from permanent ramp objects plus separate ad-hoc temporary slabs
that did not share one stage-aware descriptor system.

## Natural timelapse playback

The legacy threshold for every block is retained as the end of a small interval. A
deterministic hash spreads that block backward through 94 percent of its own wave step.
This improves temporal distribution without moving any block across a documented 0,
25, 50, 75, or 100 percent checkpoint.

The Phase 12 build shot still enters at 18 percent, plays at 3.075x, and lasts 24
seconds. Shot entry is a one-time synchronization. The next 1,440 simulated 60 Hz frames
advance through `update` to 100 percent. Direct seek remains intentionally quiet and
does not replay historical particles.

## Exterior active frontier

Frontier candidates are restricted to the front or east exterior face and selected
deterministically. A small active set follows six rigid-block presentation phases:

1. Queued outside the active face.
2. Approach toward the course.
3. Lift or slide above the destination.
4. Align over the final grid position.
5. Settle with a small damped vertical indication.
6. Stable at the exact canonical block transform.

Only the model transform changes. Every block still reuses the shared indexed cube mesh,
normal matrix, material, and instanced triangle path. Validation measured 1,317 active
frontier frames out of 1,440 and at most 12 simultaneous frontier blocks.

## Ramp and scaffold redesign

`RampDescriptor` is now authoritative for endpoints, width, thickness, side clearance,
support surface, progress interval, role, and intentional contact. The ramp frame derives
forward, right, up, length, and slope once from the endpoints. Bodies, edge rails,
cross-rollers, vertical supports, crossbeams, and worker-side stairs are all generated
from that same frame and progress interval.

The principal route progresses through low, middle, and hero upper-middle variants. A
short landing joins the hero route. West access serves early stages, an east connector
serves late upper work, and the quarry exit remains terrain-supported. Half-open progress
intervals prevent two obsolete variants from coexisting at stage boundaries.

Scaffold groups now have the same progress-aware lifecycle. Five groups cover early
west, lower-middle east, hero landing, migrating middle, and summit-east access. The
summit group and upper connector are outside the completed pyramid envelope and disappear
before completion.

## Queued stones and construction crews

Three prepared blocks sit beside each active main or upper route, beyond the rail and
load corridor. Four elevated construction workers are identified once when the scene is
built; their root translations follow the currently active ramp side. After temporary
access is removed, they return to a ground support position. Meshes, worker hierarchy,
poses, and GPU resources are unchanged.

## Geometric validation

`RampValidation` is OpenGL-independent. It builds oriented boxes for active ramps,
axis-aligned block boxes, and rotated scaffold-group boxes, then uses a 15-axis OBB
separating-axis test. At 0, 25, 50, 75, and 100 percent it checks:

- every active ramp against every visible pyramid block;
- active ramp pairs, with the designed main-ramp/landing endpoint counted separately;
- active ramps against active scaffold groups;
- hero load-corridor width and rail clearance;
- positive vertical support reach to the declared support surface.

All five checkpoints report zero unintended intersections, zero scaffold conflicts,
zero corridor obstructions, and zero unsupported samples. At 75 percent one designed
ramp-to-landing junction is recorded as an allowed connection.

## Validation commands

```powershell
build\PyramidGiza.exe --validate-timelapse-repair
build\PyramidGiza.exe --validate-ramp-clearance
ctest --test-dir build --output-on-failure
```

The first command also checks exact block counts, monotonic natural playback, completion,
frontier bounds, all placement phases, settlement-event crossings, and quiet direct seek.

## Preserved constraints

Filled geometry remains indexed `GL_TRIANGLES` through `glDrawElements` or
`glDrawElementsInstanced`. No mesh, VAO, VBO, EBO, texture, framebuffer, render pass,
legacy OpenGL path, imported model, or external scene system was added. CCW winding,
outward normals, inverse-transpose normal matrices, culling, shadows, procedural textures,
particles, model/view/projection, and CVV/NDC are unchanged.

## Remaining limitations

The motion is an explanatory rigid-block visualization, not a physics simulation.
Temporary access is a plausible graphics reconstruction rather than an archaeological
claim. Ropes do not deform, workers do not use inverse kinematics, and the repair does
not add collision physics or a new animation timeline.
