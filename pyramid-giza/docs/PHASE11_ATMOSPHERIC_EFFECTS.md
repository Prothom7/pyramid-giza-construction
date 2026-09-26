# Phase 11 - Atmospheric Construction Effects

## Objective and baseline

Phase 11 makes the optimized construction site feel active without replacing its
course-oriented renderer. Phase 10 reduced the complete scene from about 19,380 combined
draws to about 3,532 through indexed pyramid instancing, conservative frustum culling,
and state sorting. Atmospheric effects were added after that optimization so a bounded
visual layer could be measured without hiding an inefficient geometry pipeline.

The result adds sledge dust, mallet-impact dust, frontier-block settlement puffs,
low-density work-zone dust, slow Nile texture motion, and subtle tree-foliage sway.
The pyramid, workers, and construction sequence remain the focus.

## Indexed billboard particle architecture

Particles share one canonical quad containing four vertices, six indices, and two
counter-clockwise triangles:

```text
0 ---- 3
|    / |
|  /   |
1 ---- 2

indices: 0,1,2 and 0,2,3
```

The quad has one VAO/VBO/EBO. One preallocated dynamic instance VBO stores only:

```text
center.xyz + size
color.rgb + alpha
rotation
```

All active particles are submitted by one
`glDrawElementsInstanced(GL_TRIANGLES, ...)` call. No particle owns a GPU resource.
The instance buffer is allocated once at the fixed pool capacity and updated only with
active instance records.

The vertex shader constructs a world-space camera-facing position:

```text
world = center
      + rotatedCorner.x * cameraRight * size
      + rotatedCorner.y * cameraUp    * size
clip  = projection * view * vec4(world, 1)
```

The fragment shader samples a deterministic 64 x 64 procedural radial-falloff texture,
multiplies particle color by a simple daylight/ambient tint, and discards nearly
transparent edge fragments. There are no downloaded effect assets.

## Pool, determinism, update, and sorting

The default CPU pool contains 512 reusable slots; the CLI permits 64 through 2048.
Emission searches for a dead slot and safely rejects overflow. It never allocates or
deallocates particles per event. Random-looking variations are hashes of event ID,
particle index, and channel, so identical events reproduce identical particles without
`std::random_device` or wall-clock seeds.

Each live particle stores position, velocity, age/lifetime, start/end size and alpha,
color, rotation/angular velocity, horizontal drag, gravity, type, and an alive flag.
Integration is delta-time based. Horizontal motion uses analytic exponential drag;
vertical motion uses constant acceleration. Particles grow and fade over normalized age.
Before upload, active particles are sorted back-to-front by squared camera distance.

## Transparent render state

Particles render after the opaque lit scene and are excluded from the directional
shadow-depth pass. Depth testing stays enabled, while depth writes are disabled for the
transparent draw. Alpha blending uses source alpha and one-minus-source-alpha. Face
culling is temporarily disabled so billboards remain visible, and particles stay filled
while the opaque scene is in wireframe mode.

The particle renderer restores blend enable/functions/equations, depth mask, culling,
polygon mode, active texture and binding, shader program, VAO, and array-buffer binding.
The valid OpenGL Core polygon restoration is
`glPolygonMode(GL_FRONT_AND_BACK, previousPolygonMode[0])`.

Particles are hidden in world-normal and shadow-factor debug views so those diagnostic
outputs remain unambiguous. The normal material texture toggle does not disable the
independent procedural particle texture.

## Effect systems

### Sledge dust

The loaded sledge emits from approximate rear-left and rear-right runner positions only
during moving transport states. Emission rate is proportional to measured world speed,
capped at 22 particles per second, and is zero below the stationary threshold. Large
position jumps are rejected. Sand-colored particles move slightly backward and upward,
use drag and gravity, live 0.75-1.35 seconds, begin at 0.13-0.25 world units, grow by
2.7, and start at alpha 0.22.

### Mallet-impact dust

The existing quarry-mallet cycle is observed at its 0.62 impact phase. An integer
cycle-crossing test emits exactly one 14-particle limestone-colored burst per strike,
never one burst per frame. Particles live 0.45-0.95 seconds, begin at 0.08-0.17 units,
grow by 2.1, and start at alpha 0.28.

### Frontier settlement dust

Construction timeline stable-threshold crossings are observed for frontier candidates.
Each actual forward settlement emits six small broad particles at the block. Direct
Home/End or CLI progress jumps clear effects and never replay historical placements.
At most four placement events (24 particles) are emitted in one update, keeping an 8x
timelapse bounded. Lifetimes are 0.40-0.82 seconds, initial sizes 0.09-0.18 units,
growth 2.4, and starting alpha 0.20.

### Ambient construction dust

An intentionally sparse 1.25-particle/second source selects deterministic active zones.
Early progress uses the quarry and loading yard; middle progress adds the main ramp;
late progress adds the current upper level. The rate is capped at two emissions per
update. Particles live 1.6-2.8 seconds, begin at 0.16-0.30 units, grow by 2.5, and use
alpha 0.075. This is local activity, not world fog.

## Nile and vegetation motion

The Nile keeps its Phase 9 plane, UVs, procedural texture, Blinn-Phong material, and
shadow/light calculations. When effects are enabled, its material offset adds:

```text
u += wrappedEnvironmentTime * 0.0018
v += wrappedEnvironmentTime * 0.00065
```

Environment time wraps every 400 seconds for numerical stability. No water geometry,
reflection, refraction, normal map, or fluid simulation was introduced.

Each tree trunk remains fixed. Its five foliage primitives rotate around the trunk-top
pivot by at most 1.35 degrees:

```text
angle = 1.35 * sin(environmentTime * 0.72 + deterministicTreePhase)
```

The transformed foliage is written into the shared per-frame object list before both
the shadow and visible passes, so moving foliage and its shadow agree.

## Four independent time domains

- `SunController` advances or fixes daylight and only supplies lighting state.
- `ConstructionAnimationController` drives the 28.5-second hero sequence; effects
  observe sledge motion and mallet phase.
- `ConstructionTimelineController` drives the 90-second pyramid build; effects observe
  forward settlement thresholds and construction stage.
- Environment/effect time advances with application delta time while effects are
  enabled; it drives ambient emissions, Nile UVs, tree sway, and already-emitted particle
  aging.

Effects read these controllers but never modify them. Pausing hero animation stops new
hero motion events, and pausing the construction timeline stops placement events.
Already-emitted particles and ambient environment effects continue to age/update.

Hero reset clears all dust, resets its emission accumulator/event sequence, and
resynchronizes the previous sledge position. Construction reset, completion, or direct
progress selection clears particles and placement history. Disabling effects clears the
pool and freezes Nile/tree environment time; enabling starts cleanly.

## Lighting, shadows, and performance

Dust receives a cheap tint derived from sun color/intensity and ambient light, remaining
coherent at 08:00, 12:00, and 17:00 without full per-particle Blinn-Phong evaluation.
Dust never casts a hard shadow. Water preserves its lit material response, and foliage
continues to cast moving geometry shadows.

The pool uses 43,008 bytes on the CPU at capacity 512. The instance buffer capacity is
18,432 bytes, and the 64 x 64 RGB particle texture with estimated mip storage is 16,384
bytes. Representative 75-percent/noon operation used one particle draw, 11-15 active
instances, about 0.003 ms CPU particle update, roughly 1,943 visible draws and 1,954
shadow draws. A completed accelerated timelapse ended with seven particles, one particle
draw, 1,763 visible draws, and 1,774 shadow draws; its observed peak was 358 of 512
slots. Measurements are samples, not FPS claims.

## Validation

Three Phase 11 CTests cover deterministic emission, finite particles, lifetime bounds,
30-Hz/60-Hz equivalence, safe pool overflow, predictable clear/reuse, one-shot mallet
crossings, placement crossings, stationary/moving sledge behavior, stable wrapped water
offsets, and bounded deterministic tree sway. Runtime checks cover construction
0/25/50/75/100 percent, the full hero duration, accelerated 0-to-100 timelapse, all nine
cameras, effects on/off, daylight presets, material/shadow/debug controls, wireframe,
culling controls, and OpenGL errors.

## Preserved course constraints

- Normal geometry remains `glDrawElements(GL_TRIANGLES)`.
- Pyramid and particle batches use
  `glDrawElementsInstanced(GL_TRIANGLES)`.
- VAO/VBO/EBO ownership remains shared and RAII-managed.
- Particle geometry is an indexed quad; there are no `GL_POINTS`,
  `glDrawArrays`, immediate mode, GLUT/GLU geometry, or imported models.
- Model/view/projection and the clip-to-NDC/CVV pipeline are unchanged.
- There are no compute shaders, SSBOs, physics, rope simulation, or fluid simulation.

## Limitations and next phase

Dust is a restrained CPU simulation with approximate sorting, not volumetric smoke.
Water motion is UV scrolling rather than physical waves. Foliage uses rigid primitive
sway rather than deforming vegetation. PBR, normal mapping, reflection/refraction,
volumetric effects, physics, post-processing, and final cinematic polish remain
deliberately deferred. Phase 12 may build on this foundation, but it is not part of
Phase 11.
