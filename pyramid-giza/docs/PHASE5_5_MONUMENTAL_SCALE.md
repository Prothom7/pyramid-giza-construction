# Phase 5.5 - Monumental Scale and Site Expansion

## Objective

The Phase 5 scene proved coordinated animation, but its original 18.23-unit pyramid base
was only a few worker-heights tall. Phase 5.5 makes human scale the primary reference and
turns the demonstration area into a large, organized construction site. The scene now
contains a monumental unfinished pyramid, a ramp network, multi-level scaffolding,
separated production zones, grouped labor, additional sledges, timber storage, shelters,
platforms, and readable transport corridors.

This is a visual scale model for a graphics course, not an exact archaeological
reconstruction.

## Worker-based scale standard

The articulated worker is 2.26 world units tall and remains unchanged. Important world
dimensions live in WorldScale rather than being scattered through scene-building code.

    worker height               2.26
    pyramid block           2.80 x 2.00 x 2.80
    pyramid base               81.64 x 81.64
    constructed height         46.00
    main ramp width              7.00
    scaffold level height        2.70
    quarry center distance      72.00
    desert footprint           220 x 190

The constructed pyramid is 20.35 worker-heights tall. Its implied target height is about
56 units, but the upper levels are absent to communicate an unfinished monument.
Exact values and ratios are recorded in world_scale.csv.

## Procedural pyramid

PyramidLayout still uses deterministic nested loops. The base contains 28 blocks per
side, the scene builds 23 levels, and each higher level loses one block per side.
Block spacing is 0.12 units, avoiding coplanar overlap and z-fighting.

The final layout contains 7,561 visible blocks. Lower levels are complete. Starting at
level 15, a centered opening grows on the positive-Z active face while side-edge blocks
remain in place. This produces structured construction access rather than random holes.
All blocks use one shared cube mesh and differ only by model matrix and restrained
limestone material variation.

## Active construction face

The positive-Z face concentrates the main hauling ramp, scaffold towers, ramp-top staging,
worker silhouettes, stockpiles, sledges, and the existing hero animation. The other faces
remain quieter so the primary construction story is readable.

    unfinished upper pyramid
              |
    scaffold - active face - scaffold
              |
      main hauling ramp
              |
    transport and stockpile

## Ramp network

Three ramps are generated from RampDescriptor data:

1. MainHaulingRamp is 46.27 units long, 7 units wide, and rises about 9.77 degrees.
   It carries the Phase 5 animated sledge.
2. WestAccessRamp connects the quarry side to a lower side face.
3. UpperConnector links the active-face platform to a higher work level.

MonumentalSite::rampModel builds an oriented cuboid basis from the two endpoints. Rails
reuse the same algorithm with narrow descriptors. The main ramp adds shared-cylinder
rollers, supports, and cross beams. Exact coordinates are in ramp_network.csv.

## Scaffolding

Scaffold::createModule defines one reusable 13-part rigid module:

    4 cylinder poles
    4 horizontal cuboid beams
    4 diagonal cuboid braces
    1 cuboid platform

The visible geometry is mesh-only scale; no scaffold owns GPU buffers. Three placement
descriptors expand this module through level and bay loops. The two active-face groups
each contain 3 levels by 3 bays, and ramp-top staging contains 2 levels by 2 bays, for
22 modules and 286 part draws. Small world offsets keep scaffold platforms away from
coplanar pyramid surfaces. See scaffold_layout.csv.

## Site zones

The world is organized into nine named zones:

- Pyramid Zone
- Ramp Network
- Scaffold Zone
- Quarry Zone
- Cutting Zone
- Stockpile Zone
- Transport Zone
- Timber Yard
- Work Camp

Each zone has a deterministic center, extent, and purpose in site_zones.csv. This
organization controls density and leaves open hauling corridors instead of random clutter.

## Quarry and preparation flow

The quarry is moved west, about 72 units from the monument. A 7-by-6 rough-block field
uses deterministic scale and rotation patterns. A 6-by-4 stepped cut face and low earth
bank make it read as an extraction area.

The separate cutting area shows a simple production sequence:

    rough quarry block -> cutting bed -> prepared block -> transport lane

It contains five cutting stations and a regular 15-block prepared group.

## Stockpile stages and transport lanes

Three stone-storage types are visually distinct:

- rough quarry stones use darker material and irregular orientation;
- prepared stones form an orderly three-level stack;
- transport-ready stones line the main corridor;
- a smaller limestone group waits near ramp staging.

Thin raised earth cuboids imply routes from quarry to cutting, cutting to stockpile, and
stockpile to ramp. They sit slightly above the desert to avoid z-fighting and do not
require terrain textures.

## Workers

There are 25 worker instances sharing the same immutable 17-node hierarchy and primitive
meshes:

- 7 hero workers retain full Phase 5 roles and coordinated motion;
- 9 secondary workers use inexpensive stationary sinusoidal pose previews;
- 9 background workers hold static task poses.

Groups are placed at the quarry, cutting beds, transport corridor, ramp, scaffolds, timber
yard, and camp. Worker silhouettes near the pyramid and platforms are deliberate scale
cues.

## Sledges and support areas

The existing animated loaded sledge remains the hero transport. Five static instances
reuse the same sledge definition at the quarry, cutting area, transport lane, ramp base,
and timber yard. No per-sledge mesh or buffer is created.

The timber yard contains 15 stacked cylinder logs, 12 cuboid beams, three wooden frames,
and a static sledge. The work camp has two simple four-pole shelters and grouped tool
clusters. These support the construction story without competing with the pyramid.

## Phase 5 animation preservation

The state machine, 28.5-second duration, hierarchy, joint limits, prop attachment, rope
logic, and lever logic remain unchanged. Only scale-dependent world coordinates changed.

The hero sledge now starts at (10, 0, 40), turns at (0, 0, 47), enters the main ramp at
Z=45, and arrives at the active face near Z=-0.6 and Y=8.545. Puller roots are still
derived from the same transport frame, ropes still join evaluated hands to tow points,
and the stone remains a local child of the sledge. The sequence represents one focused
transport stage, not the entire quarry-to-pyramid journey.

The lever work point moved to (23, 0, 12) so it remains outside the enlarged monument.

## Camera and projection

Movement speed increased from 9 to 22 units per second. Seven presets cover overview,
pyramid base, quarry, main ramp, hero transport, scaffold/upper work, and a wide site
view. The perspective range changed from 0.1-100 to 0.5-350. The larger near plane
preserves more depth precision while the far plane contains the 220-by-190 world.

## Performance

Current maximum scene statistics:

    pyramid blocks       7,561
    workers                  25
    hero workers               7
    secondary workers          9
    background workers         9
    sledges                    6
    scaffold modules          22
    ramps                       3
    maximum draw calls      8,553

The existing repeated-draw architecture remains intentionally simple for course
presentation. Static layout vectors are generated once. Every frame updates only worker,
sledge, rope, tool, and lever transforms. Smoke tests complete normally on the available
NVIDIA GeForce RTX 3060; no formal FPS profiler was added.

## Course constraints preserved

- Filled drawing remains glDrawElements(GL_TRIANGLES).
- Plane, cube, cylinder, and sphere meshes retain indexed VAO/VBO/EBO storage.
- Pyramid blocks, scaffold parts, workers, sledges, ramps, and props reuse shared meshes.
- No imported model, immediate mode, triangle fan rendering, per-instance GPU geometry,
  skinning, inverse kinematics, or negative scale was introduced.
- CCW front faces, back-face culling, depth testing, and wireframe debugging remain active.
- The inverse-transpose normal matrix still handles every non-uniform scale.
- Model, view, projection, clip, perspective divide, and CVV/NDC meanings are unchanged.

## Phase 5.5 CPU validation

The --validate-site option checks:

- positive, coherent world dimensions;
- completed pyramid height of at least 20 worker-heights;
- deterministic 7,561-block layout with finite transforms;
- positive ramp lengths, widths, thicknesses, and finite oriented models;
- deterministic 13-part scaffold module transforms;
- valid scaffold group counts;
- valid finite site-zone centers and extents.

## Deferred to Phase 6 and later

This phase does not add a lighting overhaul, moving sun, shadows, textures, particles,
physics, collision detection, AI, pathfinding, inverse kinematics, advanced LOD, or a
cinematic camera. Lighting remains the existing simple directional-light foundation.
