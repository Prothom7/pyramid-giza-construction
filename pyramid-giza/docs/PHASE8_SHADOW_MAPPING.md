# Phase 8 - Directional-Light Shadow Mapping

## Objective

Phase 8 adds real-time shadows to the Phase 7 directional sun while preserving the
existing geometry, materials, cameras, and 28.5-second construction animation. The
implementation is a classic OpenGL 3.3 two-pass shadow map: one depth-only pass from
the sun and one camera pass that compares each fragment against that stored depth.

## Two-pass pipeline

    PASS 1 - sun/depth
    mesh -> model/world -> light view -> orthographic projection
         -> light clip/NDC -> depth attachment -> shadow texture

    PASS 2 - camera/color
    mesh -> model/world -> camera view/projection -> visible fragment
    world position -> light-space matrix -> light clip/NDC -> [0,1] texture coordinates
                   -> bias + 3x3 PCF comparison -> visibility
                   -> ambient + visibility * (diffuse + specular)

Both passes call the same indexed Mesh::draw implementation, which executes
glDrawElements(GL_TRIANGLES, ...).

## Shared scene transforms

StaticGizaScene::collectFrameObjects evaluates the complete scene once per frame. The
resulting list contains the static environment plus current articulated workers,
sledge, transported stone, dynamic ropes, hand-attached mallet, lever, and lifted
block. Both passes iterate that exact list. Animated casters therefore cannot use stale
or independently reconstructed transforms.

## Shadow resource

ShadowMap owns exactly one framebuffer and one 2D depth texture and deletes both
through RAII while the OpenGL context is active. Initialization verifies
GL_FRAMEBUFFER_COMPLETE and throws a visible runtime error on failure.

The default map is 4096 x 4096 using GL_DEPTH_COMPONENT24. Raw depth storage is about
48 MiB before driver-specific alignment. A 2048 map was also tested and remains
available through --shadow-resolution 2048; 4096 was retained because it produces
clearer block/scaffold edges and remained practical on the project RTX 3060.

Manual PCF uses GL_NEAREST filtering. S/T use GL_CLAMP_TO_BORDER with border depth
1.0, so samples outside the frustum do not create false shadows. Texture comparison
mode is disabled because comparison is explicit in GLSL.

## Directional light camera

Phase 8 consumes the existing Phase 7 SunState and does not duplicate sun motion.
sunDirection is the world-space direction in which rays travel from sun to scene.
For a stable center at (-20, 18, -8):

    lightPosition = shadowCenter - sunDirection * 320
    lightView     = lookAt(lightPosition, shadowCenter, safeUp)

World Y is normally the up reference. When the high sun is nearly parallel to Y,
world Z is selected instead, preventing a singular lookAt near noon. CPU tests exercise
this path.

## Orthographic projection

A directional source uses one orthographic projection:

    left/right = -235 / +235
    bottom/top = -235 / +235
    near/far   = 1 / 700

The fixed 470 x 470 coverage is independent of the camera, so viewer movement does not
refit the shadow map. It spans the pyramid, ramps, quarry, repositories, logistics,
Nile edge, Sphinx context, and support areas. The light-space matrix is computed once
per frame as lightProjection * lightView and changes whenever the sun changes.

## Depth comparison

The lit vertex shader emits lightSpaceMatrix * worldPosition. The fragment shader
divides by W, maps NDC from [-1,+1] into [0,1], rejects coordinates outside the valid
volume, then applies:

    shadow = 1 when currentDepth - bias > closestStoredDepth
    shadow = 0 otherwise

Ambient remains unshadowed:

    final = ambient + visibility * (diffuse + specular)
    visibility = 1 - shadow * 0.90

This prevents shadowed workers and machinery from becoming pure black.

## Bias and PCF

The slope-aware bias is:

    bias = max(0.0025 * (1 - dot(normal, toLight)), 0.00035)

These values control acne across 7,561 stepped blocks without obvious detachment under
workers, sledges, blocks, poles, or anchor posts. No polygon offset is used.

The shader samples the current texel and eight neighbors, performs nine comparisons,
and averages them. This 3 x 3 percentage-closer filter reduces jagged edges without
the expense or blur of a large kernel.

## Culling and state restoration

The depth pass inherits the active culling state, normally back-face culling with CCW
fronts. Front-face depth culling was rejected because it can remove thin ropes, limbs,
ladders, and scaffold members. If control C disables culling, both passes honor it.

The visible pass may be wireframe, but depth is temporarily rendered as filled indexed
triangles. Polygon mode, default framebuffer, and window viewport are restored. The
previously sampled depth texture is unbound before it becomes a depth attachment.

## Debugging and controls

- H toggles shadow influence and skips the depth pass when shadows are off.
- J toggles normal rendering and shadow-factor visualization.
- Factor debug uses white for lit and dark for shadowed.
- V retains the five independent Phase 7 lighting modes.
- Normals and unlit modes ignore shadows.
- Diffuse-only and specular-only modes apply visibility to their term.

CLI hooks are --validate-shadows, --shadows, --no-shadows,
--shadow-debug-factor, and --shadow-resolution 2048 or 4096.

## Moving-sun results

- 08:00: long angled shadows expose the pyramid, scaffolds, workers, machinery, and
  quarry terraces.
- 12:00: high-sun shadows concentrate beneath blocks, workers, sledges, and platforms.
- 17:00: long shadows extend in the opposite Z direction from morning.

The matrix and depth map are updated each frame, while the sun and construction clocks
remain independent.

## Scene results

- The pyramid casts a monumental ground shadow and self-shadows between steps.
- Workers, hero sledge, transported stone, mallet, lever, and ropes use current
  animation transforms in both passes.
- Scaffolds, ladders, pulley rigs, anchors, workshop, and repair objects cast their
  primitive silhouettes without special category shaders.
- Quarry terraces, bays, workers, and ramp gain strong depth cues.
- Trees cast geometric shadows; the Nile receives shadows through the common shader.
- The Sphinx context casts and receives shadows but remains secondary.
- Very thin ropes can be intermittent at site scale; the frustum is not distorted only
  to guarantee every rope texel.

## Performance

The visible pass remains approximately 9,463 draw calls. The depth pass has the same
maximum, for approximately 18,926 combined calls. There are no caster exclusions.
Shadows add no meshes, per-object buffers, or per-object FBOs. Runtime smoke tests at
4096 completed interactively on the RTX 3060; there is no formal FPS counter.

## Validation and constraints

--validate-shadows checks bounds/settings, finite morning/noon/evening matrices,
high-sun up-vector safety, moving-sun matrix changes, and shared animated transforms.
Runtime initialization validates framebuffer completeness. The visual matrix covers
pyramid, quarry, ramp, Nile, Sphinx, all animation-state families, shadow disable,
factor debug, normals, wireframe, culling on/off, and 2048/4096 maps.

Both passes retain shared VAO/VBO/EBO meshes, indexed triangles, CCW fronts, outward
normals, inverse-transpose normal matrices, model/view/projection, and CVV/NDC. No
imported geometry or legacy OpenGL was added.

## Deliberately deferred

This is one fixed world-centered directional shadow map. Distant fine detail has less
resolution than a cascade. Cascaded shadow maps, point-light cubemaps, VSM/EVSM, night
lighting, PBR, SSAO, advanced water, and final optimization remain outside Phase 8.
