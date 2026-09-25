# Phase 10 - Renderer Optimization

## Objective and preserved baseline

Phase 10 reduces CPU draw-submission overhead without changing the Phase 9 world. The
7,714-block final pyramid, 90-second construction timeline, 28.5-second hero animation,
procedural textures, moving sun, 4096 x 4096 shadow map, materials, cameras, and scene
layout are preserved. Phase 9 required about 9,690 visible draws and 19,380 visible plus
shadow draws at its maximum.

The primary optimization is indexed GPU instancing. Ordinary rendering maps one object
to one draw. Instancing combines many compatible transforms with one shared indexed
mesh and one draw:

```text
Shared cube vertices / normals / UVs
              |
        shared VAO + VBO + EBO
              +
 instance model / normal / UV VBO
              |
 glDrawElementsInstanced(GL_TRIANGLES)
              |
 thousands of pyramid blocks
```

No geometry is copied per block. Non-instanced objects continue to call
`glDrawElements(GL_TRIANGLES)`. Both paths therefore remain triangle-only and indexed.

## Instance data and attributes

`InstanceData` contains a model matrix, CPU-computed inverse-transpose normal matrix,
and a packed UV scale/offset. Computing the normal matrix once per instance avoids a
matrix inverse for every vertex and preserves lighting under non-uniform scale.

The shared mesh keeps locations 0-2. A batch-level instance VBO supplies model columns
at 3-6, normal-matrix columns at 7-9, and UV transform at 10. Every instance attribute
uses `glVertexAttribDivisor(location, 1)`; geometry attributes retain divisor zero.
Startup queries `GL_MAX_VERTEX_ATTRIBS` and requires at least 11 locations.

`InstanceBatch` owns one RAII-managed instance VBO. Static groups use `GL_STATIC_DRAW`.
The two small frontier groups use `GL_DYNAMIC_DRAW`, grow capacity only when necessary,
and otherwise update with `glBufferSubData`. The instance VBO is deleted at shutdown.

## Pyramid batching and construction timeline

The complete deterministic layout is partitioned by two compatible limestone materials
and four seven-level spatial chunks: eight static batches total. Each batch is sorted by
the threshold at which a block becomes stable. `upper_bound` selects a prefix count, so
the full static buffer is uploaded once and construction changes only the instance count.

About one in 23 scheduled blocks is a moving frontier candidate. While active, it is
excluded from the stable prefix and uploaded to one of two small dynamic material
batches. When its short placement window finishes, it enters the static prefix. Thus a
frontier block is never duplicated or double-shadowed. At 100 percent all 7,714 blocks
are stable. The same batches and active counts are used by the visible and depth passes.

The required totals remain exact:

| Construction | Stable + frontier blocks |
|---:|---:|
| 0% | 70 |
| 25% | 4,734 |
| 50% | 6,964 |
| 75% | 7,561 |
| 100% | 7,714 |

The pyramid needs at most eight static plus two frontier draws per pass. Workers,
sledges, ropes, tools, scaffold parts, repositories, trees, and other unique or animated
objects deliberately remain on the normal renderer. This keeps Phase 10 focused and
avoids rewriting stable animation/composite systems. They remain candidates for later
secondary batching if profiling justifies it.

## Visible and shadow shaders

`basic_instanced.vert` reads per-instance transforms but reuses the Phase 9 fragment
shader, so lighting, texture sampling, shadow PCF, and debug modes have one source of
truth. `shadow_depth_instanced.vert` applies the same instance model matrices in the
depth pass and reuses the existing depth fragment shader. Texture scale/offset remains
equivalent to Phase 9 and is applied once through the per-instance UV attribute.

## Frustum culling

```text
projection * view (or light-space matrix)
                    |
       six normalized frustum planes
                    |
 conservative transformed bounding sphere
                    |
       outside -> skip; intersect/inside -> draw
```

Ordinary scene objects use a conservative sphere derived from their canonical primitive
radius and the linear transform's Frobenius-norm bound (safe even with inherited shear).
Pyramid instances are tested by the same eight spatial
chunks plus two frontier groups, avoiding 7,714 CPU tests per frame. The camera frustum
is used only for the visible pass. A separately extracted orthographic light frustum is
used for shadow casters, with a larger safety margin; camera visibility never controls
shadow eligibility. `Y` toggles frustum culling for direct comparison and
`--no-frustum-culling` supplies a deterministic CLI control.

## State-change reduction and statistics

Visible ordinary objects are sorted by material then primitive. The renderer uploads a
material only when it changes and binds a texture only when its texture ID changes.
Shadow objects are sorted by primitive. Shader selection is stable: instanced depth,
normal depth, instanced lit, then normal lit.

Per-frame `RenderStats` records visible/shadow draws, submitted instances and triangles,
culled objects/instances, material changes, texture binds, and CPU collection/submission
time. Press `I`, use `--render-stats`, or run `--benchmark-render`. CPU time is a
single-process submission measurement, not GPU frame time; no FPS claim is made.

## Measured result

Measurements used preset 1, 1280 x 720, static 08:00 sun, shadows/textures enabled,
and an NVIDIA GeForce RTX 3060. Values vary with construction-stage infrastructure and
camera visibility.

| Metric | Phase 9 maximum | Phase 10 at 75% | Phase 10 at 100% |
|---|---:|---:|---:|
| Visible draws | about 9,690 | 1,942 | 1,760 |
| Shadow draws | about 9,690 | 1,954 | 1,772 |
| Combined draws | about 19,380 | 3,896 | 3,532 |
| Visible submitted instances | not tracked | 9,496 | 9,466 |
| Texture binds | not tracked | 13 | 13 |
| Rejected ordinary objects | not tracked | 12 | 12 |

The measured combined reduction is about 79.9% at the default 75% scene and 81.8% at
100%. The structural worst-case estimate is 1,986 draws per pass, a 79.5% reduction from
the Phase 9 visible maximum. The pixel comparison with camera/light culling enabled and
disabled differed in only 29 channel values across two separately timed smoke processes;
the visual inspection showed no missing geometry or shadows.

## Validation and course constraints

CPU tests verify checkpoint totals, representative Phase 9 model/UV equivalence, finite
non-singular instance and normal matrices, six finite normalized frustum planes,
inside/outside/boundary classifications, transformed bounds, and the <=10 pyramid-draw
architecture. Runtime checks verify instance buffers, both passes, construction stages,
textures, shadows, debug modes, culling, wireframe, cameras, animation, and OpenGL errors.

The model-view-projection and CVV/NDC pipeline is unchanged. Winding remains CCW,
back-face culling remains supported, and normals remain outward. There is no immediate
mode, non-indexed rendering, imported geometry, per-block geometry buffer, LOD,
occlusion query, compute shader, SSBO, or indirect drawing.

## Limitations and next phase

Only the dominant pyramid population is instanced in this phase. Group bounding spheres
are intentionally conservative, so some offscreen instances may still be submitted.
There is no occlusion culling, LOD, indirect drawing, PBR, particles, advanced water, or
post-processing. Those remain separate future decisions; they are not required for the
OpenGL 3.3 course architecture.
