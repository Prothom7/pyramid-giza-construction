# Phase 6 - Camera, Navigation, and Presentation

## Objective

Phase 6 makes the 360 by 300 unit construction landscape comfortable to explore and easy
to present in a viva. It adds navigation and presentation state only. The Phase 5.6 world,
shared meshes, materials, lighting, draw path, and 28.5-second animation remain unchanged.

## Architecture

The stable `Camera` class still owns position, Euler yaw/pitch, direction vectors, and view
matrix construction. `CameraController` now owns presentation state around it:

```text
input / preset / target
          |
          v
 CameraController mode
          |
          v
 position + yaw + pitch + FOV
          |
          v
 Camera::GetViewMatrix + perspective projection
```

`CameraMode` is one of `Free`, `PresetTransition`, `OrbitPyramid`, `FollowTransport`, or
`GuidedDemo`. This avoids competing mode booleans. Presets and demo shots are fixed arrays,
so camera features allocate no per-frame structures and create no geometry or GPU resources.

## Free camera and world-scale movement

Free movement remains `W/A/S/D`, with `Q/E` for vertical motion. All displacement is
`direction * speed * deltaTime`. Normal speed is 24 units/second, Shift fast speed is 56,
and Ctrl precision speed is 7. Ctrl wins if both modifiers are held. Broad safety bounds
keep the camera near the 360 by 300 site while allowing a close quarry view down to y=-6.8.

Mouse look retains explicit yaw/pitch and clamps pitch to [-89, 89], so orientation cannot
invert. There is no roll and no accumulated orientation matrix. The wheel adjusts FOV in
free mode. Key `0` restores preset 1, normal speed, default FOV, and free mode. Key `K`
prints a single tuning line rather than logging every frame.

## Curated presets and transitions

Presets 1-9 separately answer scale, pyramid base, quarry, extraction, logistics, ramp,
whole-site, Nile, and Sphinx questions. Exact values are in `camera_presets.csv`. Normal
number keys start a one-second smoothstep transition; Shift plus a number jumps instantly.
Position, pitch, and FOV interpolate directly. Yaw uses the shortest wrapped angular path,
so a transition across +/-180 degrees does not spin the long way around.

Manual movement or mouse look immediately cancels a preset transition, transport follow,
or guided demo. Manual control therefore always wins. Orbit mouse input is intentionally
handled inside orbit mode; pressing movement keys leaves orbit and returns to free flight.

## Pyramid orbit

`O` toggles orbit around `(0, 20, -42)`, near the pyramid's visual center. Horizontal mouse
movement changes azimuth, vertical movement changes elevation, and the wheel changes radius.
Elevation is clamped to 8-70 degrees and radius to 55-180 units. The look-at helper derives
yaw/pitch from `normalize(target-position)`, keeping the monument centered without entering
the blocks or turning upside down.

## Transport follow

`T` follows the animated loaded sledge from a fixed side/back/up offset `(11, 7, 15)`.
Exponential delta-time smoothing approaches the desired position and a look-at orientation
tracks the load slightly ahead/down the route. The camera reads `transportTarget()` from the
current animation snapshot; it never writes worker, sledge, rope, lever, or timing state.
Animation reset and loop jumps therefore remain finite and are smoothed from the new target.

## Guided demo

`G` starts/stops a seven-shot data-driven tour: monumental overview, extraction, logistics,
hero transport, ramp/scaffolding, Nile, and Sphinx context. Each shot stores a pose,
transition duration, and hold duration. One-second transitions and three-second holds
(four seconds for transport) produce an approximately 29-second looping presentation.
Mouse or movement input cancels it immediately.

## Projection and zoom

Preset FOV values are 48-52 degrees. Wheel zoom is constrained to 30-75 degrees. The near
plane remains 0.7 and Phase 9 raises the far plane to 700: sufficient for the expanded ground, Nile, quarry, wide-site,
and Sphinx views without adopting a depth-precision-damaging extreme ratio. Aspect ratio is
read from the current framebuffer.

## Validation

`--validate-camera` is OpenGL-independent. It checks nine finite presets, pitch/FOV ranges,
exact interpolation endpoints, wrapped yaw, 60-versus-30 step transition equivalence,
real manual cancellation, orbit radius/elevation/look-at, finite follow offsets,
60-versus-30 follow smoothing, three movement speeds, free-movement frame independence,
seven demo shots, and projection limits. Runtime mode checks are available through
`--camera-mode free|orbit|follow|demo` together with existing smoke-test arguments.

## Course constraints and Phase 7 boundary

Filled rendering is still `glDrawElements(GL_TRIANGLES, ...)` with the existing shared
VAO/VBO/EBO meshes, CCW winding, back-face culling, correct normals, and model/view/projection
to CVV/NDC. No imported model, immediate mode, per-camera geometry, or per-instance mesh was
introduced.

Phase 7 remains deliberately unimplemented: no moving sun, shadows, day/night cycle, PBR,
advanced materials, advanced water, particles, physics, or complex cinematic editor.
