# Phase 3 - Composite Construction Objects

> Historical phase note: Phase 4 replaces the worker's flat local-part evaluation with
> the hierarchy documented in `PHASE4_HIERARCHICAL_MODELING.md`. The sledge/equipment
> composition and shared-mesh principles below remain current; `composite_objects.csv`
> preserves the Phase 3 worker baseline for comparison.

## Objective

Phase 3 keeps the Phase 2 static Giza world and makes it inhabited with recognizable
construction objects assembled from the Phase 1 indexed primitives. It is a modeling
phase: workers, sledges, equipment, and supports are static and contain no animation or
physics.

The Phase 2 freeze point is commit `718ccb087823e97609ae733918965abdb6bb0701`.

## Primitive composition

No composite owns mesh data. Each `ObjectPart` records a primitive identifier, a local
matrix, a material identifier, a name, and a parent hint:

```text
canonical cube / cylinder / sphere
              |
       part local transform
              |
       object root transform
              |
          world matrix
```

`StaticGizaScene` owns one GPU `Mesh` for each primitive type. Every body part and prop
selects one of those meshes at draw time, so adding workers creates transforms and draw
records, not VAOs, VBOs, or EBOs. Rendering remains indexed `GL_TRIANGLES` through
`glDrawElements`.

## Worker construction and coordinates

The worker root is at ground level beneath the pelvis. Positive Y is up; local negative Z
is the worker's nominal forward direction. The whole figure is approximately 2.14 world
units high. A worker has 17 parts:

```text
WorkerRoot
  +-- Pelvis -- Torso -- Neck -- Head -- Headwear
  |              +-- LeftUpperArm  -- LeftForearm  -- LeftHand
  |              `-- RightUpperArm -- RightForearm -- RightHand
  +-- LeftThigh  -- LeftLowerLeg  -- LeftFoot
  `-- RightThigh -- RightLowerLeg -- RightFoot
```

The torso, pelvis, and feet use cuboids. The neck and limb segments use cylinders. The
head and hands use spheres. Skin, clothing, and headwear use centralized material IDs.
The complete baseline dimensions and translations are recorded in
`composite_objects.csv`.

`BodyPart` gives each component a stable identity. `parentHint` documents the intended
future relation without implementing a skeletal engine prematurely. Phase 4 can replace
the current flat local matrices with parent-child joint transforms while preserving the
same part and shared-mesh representation.

## Static poses

Four deterministic poses are available:

- `Standing`: arms relaxed at the sides.
- `PullingReady`: both arms reach forward toward a rope.
- `CarryingReady`: arms are bent outward and forward for load handling.
- `LeverReady`: asymmetric arms reach toward a lever beam.

Only local part matrices differ. Poses contain no time input, interpolation, keyframes,
or motion. CPU validation regenerates every pose twice and confirms identical matrices.

## Sledge and transported stone

The reusable sledge has two long cuboid runners, three cross braces, a platform, a tow
bar, and a thin cylinder representing a straight static rope. The loaded variant adds one
ordinary cube primitive as the transported limestone block:

```text
       transported cube
       +-------------+
       +-------------+
        wood platform
     ===             ===  runners
              |
         straight rope
```

The unloaded definition has 8 parts; the loaded definition has 9. The stone is not a
special mesh and is the same shared cube used by the pyramid and quarry.

## Lever, tools, and supports

The lever consists of a long wooden cylinder beam, a cuboid fulcrum, and a prepared
stone. The mallet combines a cylindrical handle and cuboid head. A reusable wooden frame
uses two cylinder posts, two cuboid beams, and a short hanging-rope cylinder. A horizontal
cylinder is also used as a generic timber roller/construction prop; it does not assert a
specific historical transport method.

## Root transforms and scene placement

Composite definitions never contain Giza-world positions. Scene placement multiplies an
object root by each local part matrix:

```cpp
world = rootTransform * part.localTransform;
```

Seven worker roots are placed at the sledge, quarry, ramp, lever, and stockpile. Two
sledge roots create a loaded transport group and an unloaded quarry sledge. Root Y-axis
rotation changes facing without duplicating models. The important roots are recorded in
`composite_scene_instances.csv`.

Object coordinates remain compact and reusable. World coordinates legitimately exceed
`[-1,+1]`; view and projection matrices map visible world geometry to clip coordinates,
then perspective division maps it into the NDC/CVV range.

## Materials and lighting

`SceneTypes` centralizes sand, stone, earth, wood, skin, clothing, headwear, rope, and
tool materials. A material stores color plus ambient, diffuse, and specular coefficients.
The existing directional light and inverse-transpose normal matrix remain active. This is
particularly important for non-uniformly scaled cylinders and cubes used as limbs and
beams.

## Validation and performance

`--validate-composites` checks part counts, unique part names, valid primitive/material
references, finite non-singular matrices, and deterministic/distinct poses without an
OpenGL context. CTest runs this alongside the Phase 1 geometry and Phase 2 layout tests.

The scene stores static transform data once. It creates no buffers per worker, part,
stone, or sledge and does not regenerate composite definitions per frame. Phase 3 uses
ordinary repeated draw calls to keep the course concepts visible; instancing can be a
later optimization.

## Controls

- `W/A/S/D`: horizontal camera movement
- `Q/E`: down/up
- mouse: look
- `1` to `4`: preserved Phase 2 views
- `5`: worker/sledge close view
- `C`: toggle back-face culling
- `F`: toggle filled/wireframe mode
- `Esc`: exit

## Deliberately deferred

There is no walking, joint animation, sledge movement, rope simulation, block transport,
collision detection, physics, inverse kinematics, skeletal animation, or construction
sequence. Phase 3 supplies the named parts, local spaces, parent hints, and root transforms
needed to introduce hierarchy cleanly in Phase 4.
