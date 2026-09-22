# Phase 2: Static Giza World

## Goal and freeze point

Phase 2 turns the Phase 1 primitive showcase into a recognizable, static ancient Giza
construction site without beginning worker or construction animation. Phase 1 was cleanly
built, tested, committed, and pushed before this work began. Its rollback/freeze commit is:

```text
1a743b6 Complete Phase 1 primitive geometry foundation
```

The Phase 1 `Mesh`, `MeshData`, `PrimitiveGenerator`, `GeometryValidation`, shader,
camera, VAO/VBO/EBO ownership, CCW winding, lighting, and indexed `GL_TRIANGLES` path are
preserved. The temporary `ShowcaseScene` was replaced by `StaticGizaScene`; there is no
second active renderer.

## Spatial composition

```text
                         NORTH / -Z

                    unfinished pyramid
                         [blocks]
                             ^
                    supported main ramp
                             ^

       quarry          prepared stockpile       work frame
      cut stones          regular blocks        poles/beams

                  large transformed desert plane
                         SOUTH / +Z
```

The main pyramid is centered at `(0,0,-10)`. The ramp approaches its front (`+Z`) face.
The quarry occupies the left side, the prepared stockpile sits to the right of the ramp,
and the wooden work frame is farther right. The overview camera makes all relationships
visible immediately; presets provide closer inspection.

## Scene architecture

`PyramidLayout` is CPU-only. It converts a compact configuration into deterministic block
placements and statistics without requiring OpenGL. `StaticGizaScene` creates its three
GPU meshes once, builds a vector of static `SceneObject` records once, and renders those
records each frame.

```text
PyramidLayoutConfig
        |
        v
PyramidBlockPlacement[] ---- CPU validation
        |
        v
SceneObject { primitive, model, color }
        |
        v
shared Plane / Cube / Cylinder Mesh
        |
        v
glDrawElements(GL_TRIANGLES)
```

Scene construction is separated into `buildGround`, `buildPyramid`, `buildRamp`,
`buildQuarry`, `buildStockpile`, and `buildConstructionProps`. `main.cpp` remains focused
on application lifecycle, input, camera, and projection.

## Main pyramid algorithm

Default parameters are:

```text
baseBlocksPerSide = 15
completedLevels   = 10
partialFromLevel  = 6
blockWidth        = 1.15
blockHeight       = 0.62
blockDepth        = 1.15
horizontalSpacing = 0.07
origin            = (0, 0, -10)
```

For level `l`, the nominal number of blocks per side is:

```text
side(l) = baseBlocksPerSide - l
```

The block center step is `block dimension + horizontalSpacing`. Each level uses
`(side-1)/2` as its local centering offset:

```text
x = origin.x + (xIndex - centerOffset) * (blockWidth + spacing)
y = origin.y + (level + 0.5) * blockHeight
z = origin.z + (zIndex - centerOffset) * (blockDepth + spacing)
```

This produces centered, shrinking square courses. Levels 0–5 are complete. From level 6,
a deterministic opening removes central blocks from one or two front rows. The opening
widens gradually and meets the top of the ramp. Five planned upper levels are entirely
absent. The result is ordered and readable but visibly unfinished.

Actual level counts are:

```text
225, 196, 169, 144, 121, 100, 78, 62, 39, 28
```

There are 1,162 rendered pyramid blocks. Ten completely filled square levels would have
1,185, so the construction opening removes 23 additional blocks. A finished 15-level
structure would extend above the current 6.2-unit height.

## Block reuse

All 1,162 pyramid blocks use one `cube_` `Mesh`, which owns exactly one VAO, one VBO, and
one EBO. A block is only a model matrix and color choice. No block creates, uploads, or
owns geometry. The same cube mesh also supplies quarry stones, prepared stones, the ramp
deck/rails, crossbeams, and wooden frame beams.

The Phase 2 scene uploads only three reusable meshes:

- plane: desert ground
- cube: stones, structural beams, and ramp components
- cylinder: cross-logs, supports, poles, and loose logs

All seven Phase 1 primitives remain generated and tested by the Phase 1 CTest.

## Desert ground

The canonical plane is transformed to a `90 x 72` world-space desert surface centered at
`(0,0,-4)`. No large replacement vertex array and no texture are used. Its sand material
is a centralized color in the Phase 2 palette.

## Main ramp

The ramp is composed only from the existing cube and cylinder meshes. Its deck is a
`4.6 x 0.42 x 19.6` cuboid rotated ten degrees about X and centered at `(0,2,5.35)`.
It rises from the southern ground approach to the level-6 construction opening.

Two wooden rails, ten cross-logs, three crossbeams, and six vertical supports make the
incline and load path visually legible. Support heights are calculated from the same ramp
slope formula, rather than positioned arbitrarily.

## Quarry and stockpile

The quarry contains 16 deterministic loose/cut cuboids with varying non-uniform scales
and small Y rotations. Nine additional blocks form a three-level stepped cut face. This
makes the area visually distinct from a stockpile while remaining simple.

The prepared-stone area contains 15 regularly aligned limestone blocks: twelve on the
ground and three on a second course. Its regularity contrasts with the quarry and makes it
a clear future source for transport work.

## Wooden construction area

The work area uses four cylinder poles, four cube beams, and five horizontal cylinder
logs. These are static site props only; no worker, sledge, physics, or animation logic is
present.

## World coordinates, transforms, and the CVV

Canonical primitive coordinates remain around `[-0.5,+0.5]`. Phase 2 model matrices
place objects at world positions such as pyramid `z=-10`, quarry `x approximately -20`,
and work area `x approximately +17`. This is correct: world space is not the CVV.

```text
canonical object vertex
   -> model matrix (world coordinates)
   -> view matrix (eye coordinates)
   -> projection matrix (clip coordinates)
   -> perspective divide
   -> NDC/CVV approximately [-1,+1]
```

Every placed object uses `T * R * S`. The shader still receives
`transpose(inverse(mat3(model)))`, so non-uniformly scaled quarry stones, ramp sections,
and beams retain correct normals.

## Materials and lighting

One organized palette defines sand, limestone, limestone variation, quarry stone,
prepared stone, ramp earth, wood, and dark wood colors. One static directional,
sun-like light retains the Phase 1 ambient/diffuse/specular shader. Textures, moving sun,
PBR, and shadows are deliberately absent.

## Camera and debugging

- `W/A/S/D`: horizontal navigation
- `Q/E`: down/up
- mouse: yaw/pitch
- `1`: overview
- `2`: pyramid/front ramp
- `3`: quarry
- `4`: ramp/staging area
- `C`: toggle `GL_CULL_FACE`
- `F`: toggle `GL_FILL` / `GL_LINE`
- `Esc`: exit

Automated smoke tests can start with `--wireframe` or `--no-cull`. The interactive keys
use the same OpenGL state changes.

## CPU validation

`Phase2SceneValidation` generates the layout twice and verifies:

- positive configuration and transforms
- finite positions and scales
- deterministic repeated generation
- exact, consistent level height
- centered X/Z bounds at every level
- positive and strictly decreasing level populations
- expected total of 1,162 blocks
- expected footprint and completed height

Scene insertion also rejects non-finite or singular model matrices. No new mesh type was
introduced, so Phase 1 mesh topology tests are reused instead of duplicated.

## Performance

Static layout data is generated once. Three VAO/VBO/EBO sets serve 1,238 scene draw calls:

```text
ground                 1
pyramid blocks      1162
ramp components       22
quarry blocks          25
stockpile blocks       15
construction props     13
total                1238
```

Repeated draw calls are acceptable for this course phase. Shader uniform locations are
cached so the repeated block loop does not query locations thousands of times per frame.
Instanced rendering can be added later without changing layout generation.

## Deliberately deferred

Phase 2 does not contain animated workers, worker bodies/skeletons, moving sledges, block
transport, construction sequencing, moving sun, shadow mapping, textures as a core
feature, particles, physics, collision detection, imported models, or complex UI. These
belong to later phases.
