# Phase 12 - Cinematic Showcase

## Objective

Phase 12 adds one repeatable presentation path over the finished Giza simulation. It
does not add geometry or a rendering pass. A teacher can press F5, or launch with
`--showcase`, and watch the site story from quarry extraction through the completed
monument without further input.

The automated presentation lasts 101 seconds at 1x. Fourteen named shots show the
monumental site, quarry, extraction, repositories, loading, ground transport, ramp
haul, upper construction, a 24-second accelerated build, the completed pyramid, Nile,
Sphinx-area context, and a six-second final overview.

## Relationship to existing camera modes

The Phase 6 camera controller remains unchanged in purpose:

- free flight, presets 1-9, orbit, transport follow, and the G guided demo remain;
- G is the lightweight seven-shot camera-only tour;
- F5 is the Phase 12 synchronized camera/simulation presentation;
- W/A/S/D/Q/E, mouse look, wheel input, presets, reset, orbit, follow, or G cancels
  the showcase so manual control always wins.

## Architecture

`ShowcaseController` is a CPU-only coordinator in
`src/presentation/ShowcaseController.*`. Its fixed `std::array<ShowcaseShot, 14>`
stores stable IDs, times, camera endpoints, look targets, FOV, sun interval,
construction action, construction value/speed, and hero-reset metadata. It owns only:

- the independent presentation clock;
- Manual, Playing, Paused, and Complete presentation state;
- current shot selection and one-shot shot/completion notifications.

It controls existing public interfaces on `CameraController` and
`StaticGizaScene`; it does not own or reproduce camera, hero, pyramid, sun, or
particle algorithms.

## Presentation clock and deterministic evaluation

The showcase clock advances by `deltaTime * showcaseSpeed`. Camera and coordinated
subsystem values are pure evaluations of showcase time, so evaluating the same time
twice gives the same shot, camera, construction progress, hero time, and sun time.
Supported rates are 0.25x through 4x; default is 1x.

The five timing domains remain distinct:

1. `SunController`
2. `ConstructionAnimationController`
3. `ConstructionTimelineController`
4. environment/effect time
5. `ShowcaseController`

The fifth clock deliberately coordinates the first four only while showcase mode is
active. Canceling returns to the normal independent update path.

## Camera interpolation and transitions

Within each shot, normalized time is eased with
`t*t*(3-2*t)`. Position, target, FOV, and sun time interpolate between table
endpoints. `CameraController::lookAtPose` derives finite yaw and pitch from the
interpolated target, avoiding yaw-wrap interpolation errors. Selected distant
geographic changes are intentional hard cuts, recorded in `showcase_shots.csv`.

FOV remains between 44 and 52 degrees. Validation samples every shot at start,
midpoint, and end against world bounds, ground, a conservative completed-pyramid
core, and the principal quarry-wall volume. The upper-construction path stays outside
the completed monument even though that shot normally occurs before completion.

## Construction coordination

Early narrative shots use table checkpoints of 25, 35, 40, 50, and 75 percent.
Transport begins only at the established 75-percent logistics state. At 50 seconds,
the existing `ConstructionTimelineController` is set to 18 percent and runs at
3.075x for 24 presentation seconds, reaching 100 percent through the controller's
existing 90-second progression. The near-complete and reveal shots hold 100 percent.
No second block layout or building algorithm exists.

The final state retains the validated complete layout of 7,714 blocks and 28 levels.
Existing ramp, scaffold, repository, worker, frontier, and placement-dust staging
therefore remain authoritative.

## Hero-animation coordination

The existing `ConstructionAnimationController` is reset at the LoadingYard shot
(23 seconds). Its 28.5-second sequence then supplies loading, pullers, rope, loaded
sledge, ramp motion, lever work, and placement. The controller is sought directly
through its public API; no camera-relative sledge motion is fabricated.

Direct seek computes the required hero time rather than simulating previous frames.
Seeking clears event particles and establishes the exact sledge position, which avoids
stale trails and large false motion deltas.

## Sun and lighting coordination

Each shot records a sun-time interval. The presentation starts at 08:00, reaches
approximately 11:30 before the build timelapse, 14:30 at completion, and 17:00 in the
final overview. Smoothstep interpolation avoids direction/color jumps within shots.
The existing directional sun, Blinn-Phong materials, 4096x4096 shadow map, bias, and
PCF remain unchanged.

Explicit command-line choices such as `--no-shadows`, `--no-textures`,
`--no-effects`, lighting debug, shadow debug, and frustum-culling disablement are
respected. Showcase mode never force-enables them.

## Atmospheric effects and environment motion

The quarry/extraction timing exposes mallet impact dust. Ground and ramp shots expose
the existing sledge-runner dust. The build shot uses capped frontier placement puffs,
so 4x testing does not create a dust wall. Nile UV motion and foliage sway continue
from the existing environment clock.

F5 restart sets environment time deterministically and clears the fixed particle pool,
event serial, and emission accumulators. Direct seek intentionally does not replay
historical particles; new deterministic events resume from the selected point. The
512-slot pool, shared indexed quad, and single instanced particle draw are unchanged.

## Controls and command line

- F5: start or restart the synchronized showcase from time zero.
- Shift+F5: cancel and return to manual camera control.
- F6: pause or resume the presentation clock.
- G: retain the older camera-only guided demo.
- I: render statistics plus current showcase shot/time when active.
- `--showcase`: start the full showcase automatically.
- `--showcase-time SECONDS`: direct deterministic seek from 0 through 101.
- `--showcase-speed SCALE`: set 0.25 through 4.0.
- `--validate-showcase`: run all Phase 12 CPU checks.

At completion the controller enters Complete, holds the final camera and completed
pyramid, and does not loop or reset automatically.

## Validation and performance

The validation suite checks shot count, finite values, positive contiguous durations,
101-second total, frame-rate-independent clock progression, finite and safe camera
samples, deterministic evaluation, direct seek, cancel behavior, completion, and the
7,714-block/28-level final pyramid. Three CTest targets expose timeline, camera, and
state checks separately.

The controller performs a fixed-table lookup and a few vector interpolations each
frame. It creates no GPU object, framebuffer, texture, mesh, or render pass. At an
identical scene state, draw counts remain those of Phase 11; only camera and existing
controller state change.

## Course constraints preserved

Opaque geometry remains indexed `GL_TRIANGLES` through `glDrawElements` or
`glDrawElementsInstanced`. Particles remain indexed instanced triangles. Shared
VAO/VBO/EBO meshes, procedural textures, CCW winding, outward normals,
inverse-transpose normal matrices, model/view/projection, and CVV/NDC are preserved.
There is no immediate mode, `glDrawArrays`, `GL_POINTS`, imported model, physics
engine, font dependency, or post-processing pipeline.

## Historical presentation disclaimer

This is a graphics simulation, not a definitive archaeological reconstruction.
Pulley and rope-redirection devices remain explicitly speculative visualizations.
The stylized Sphinx-area landmark is broader Giza context and is not claimed as part
of the depicted Khufu construction operation.

## Limitations

Phase 12 deliberately adds no PBR, normal mapping, reflection/refraction, HDR, bloom,
SSAO, motion blur, depth of field, physics, or volumetric fog. Camera motion uses
smooth eased segments rather than a spline editor, and seek does not reconstruct
already-expired particle history. This phase is the final orchestration layer over the
existing course systems, not another rendering phase.
