# Phase 9 - Integration, Construction Timelapse, and Textures

## Objective

Phase 9 integrates the complete course project instead of replacing earlier systems. It
audits world coverage, improves construction motion, adds an independent long-form
pyramid timeline, changes temporary infrastructure with construction progress, and uses
the UV channel that has existed since Phase 1. The original 28.5-second hero transport,
directional sun, shadow map, camera modes, and primitive-only renderer remain intact.

## Scene integrity

The thirteen authoritative site zones produce a content AABB from
(-170, -11, -179) to (170, 46, 52). The expanded plateau and low perimeter skirt cover
(-213, -0.62, -213) to (213, 0, 123). Horizontal safety margins are therefore
43 left, 43 right, 34 back, and 71 front.

Four slabs still leave the recessed quarry opening uncovered. The border is a shallow
earth skirt rather than an abrupt single-plane edge. The camera far plane is 700; the
ground diagonal is about 542.56. The Phase 8 light volume remains sufficient for the
content bounds. The audit samples the full hero transport path and records 16 intended
workflow-zone overlaps, with zero unexplained overlaps.

## Construction timeline

ConstructionTimelineController owns a clock that is independent of both
ConstructionAnimationController and SunController:

    delta time -> timelapse speed / 90 seconds -> progress in [0, 1]

Block thresholds are derived from course level and a deterministic twelve-wave grid
signature. No random state or per-frame geometry generation is used. The first 23
courses and the documented construction opening reproduce the previous site exactly at
progress 0.75: 7,561 visible blocks. The opening closes, five upper courses are added,
and progress 1.0 contains all 7,714 blocks.

Only selected frontier blocks interpolate upward and inward for readable placement
motion. Most completed blocks switch once at their deterministic batch threshold. This
keeps CPU work and visual noise bounded.

## Construction stages

1. Foundation, 0.00-0.10
2. Lower courses, 0.10-0.30
3. Lower-middle courses, 0.30-0.55
4. Phase 5.6 historical checkpoint, 0.55-0.75
5. Upper courses, 0.75-0.92
6. Summit, 0.92-1.00
7. Complete, 1.00

Temporary ramps, three migrating scaffold modules, repository stones, two parked
sledges, selected background workers, and speculative pulley wheels use stage ranges.
At completion temporary high-level equipment disappears while permanent access and
environment context remain, so the final site stays coherent.

## Animation refinement

The hero sequence remains 28.5 seconds and follows its original path. Cubic easing is
retained between states and along the ground/ramp transition. Pulling now uses a slower,
braced gait, stronger forward torso lean, bent knees, asymmetric phase offsets, and a
stronger uphill pose. The quarry mallet uses anticipation, a short fast strike, and
recovery. The lever uses a preparation interval before full effort. Background workers
continue deterministic phase-offset loops. Pulley wheel transforms use construction
progress plus the existing hero time; no rope physics or mesh regeneration is involved.

## Texture architecture

Texture is an RAII OpenGL object. TextureLibrary uploads one shared image per TextureId
and binds it on texture unit 1; the Phase 8 shadow depth map stays on unit 0. Procedural
images are deterministic 256-by-256 RGB data with linear mipmap filtering and repeat
wrapping. Approximate texture memory including mip levels is 1,792 KiB.

Material now stores TextureId, UV scale, UV offset, and blend. One image is reused by
every material instance that references it. Sand, limestone, quarry stone, wood, cloth,
water, and foliage each have a restrained pattern. Skin and tool metal deliberately use
the shared white texture with zero blend.

The fragment shader samples:

    sample at TexCoord * scale + offset
    surface = mix(base color, base color * sample * 1.12, blend)

Texture sampling occurs before the existing ambient, diffuse, specular, and shadow
calculation. X toggles textures without changing lighting or geometry.

## UV mapping

Plane, cube, pyramid, disk, cylinder, and sphere UVs are validated as finite values in
[0,1]. The cube duplicates render vertices per face because both normals and UVs differ.
The cylinder duplicates its side seam. The sphere now stores 33 longitude samples for
each of 31 rings: the first and last positions match while U is 0 and 1. The two pole
vertices remain singular, so no degenerate pole quads are introduced.

## Rendering constraints preserved

All filled objects still use shared VAO/VBO/EBO meshes and
glDrawElements(GL_TRIANGLES). No immediate mode, glDrawArrays, built-in solid geometry,
imported model, per-instance GPU mesh, skeletal skinning, or external scene engine was
added. Model, view, projection, clip-space, perspective divide, and CVV/NDC behavior is
unchanged. Both shadow and visible passes consume the same evaluated frame-object list.

## Controls

- B: play/pause construction timelapse
- comma / period: slower / faster construction timelapse
- Home / End: construction progress 0 / 100 percent
- X: textures on/off
- Existing camera, hero animation, sun, lighting debug, culling, wireframe, and shadow
  controls are unchanged.

CLI controls include --construction-progress 0..1, --timelapse,
--timelapse-speed 0.25..8, --textures, --no-textures, --validate-construction,
--validate-textures, and --validate-layout.

## Validation and performance

Fourteen CTests cover Phases 1-9. Construction checkpoints at 0, 25, 50, 75, and 100
percent contain 70, 4,734, 6,964, 7,561, and 7,714 blocks. Timeline integration is
frame-rate independent and its clock does not modify sun or hero time. Procedural pixels
are deterministic, all material registry references are valid, UV seams are verified,
and the OpenGL matrix covers textured/untextured, shadowed/unshadowed, early/checkpoint/
complete, wireframe, and culling-off modes.

The maximum estimate is about 9,690 visible indexed draws, or 19,380 combined shadow
plus visible draws. All meshes and textures are uploaded once; timeline updates only
evaluate transforms and visibility.

## Deliberately deferred

No PBR, normal mapping, texture-atlas editor, cascaded shadows, moving boats, rope
physics, collision physics, imported assets, or cinematic authoring system is included.
