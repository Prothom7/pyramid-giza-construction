# Pyramid at Giza — Construction Site

A C++17 / OpenGL 3.3 Core graphics project showing a Giza construction site. The normal physical simulation follows one authoritative limestone block from quarry extraction through pulley lifting, sledge loading, desert haul, the main and upper ramps, a supported local unload, and settlement into the pyramid. The separate 125-second showcase presents the wider site from morning through night.

## Build and run

On Windows with CMake and MinGW installed:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j4
ctest --test-dir build --output-on-failure
build\PyramidGiza.exe
```

Use `build\PyramidGiza.exe --showcase` for the 18-shot presentation. `--showcase-time 0..125` seeks to a presentation time; `--showcase-speed 0.25..4` changes playback speed. `--sun-time HOURS --static-sun` starts a reproducible time-of-day view. The default run is normal physical mode.

## Controls

| Keys | Action |
| --- | --- |
| W/A/S/D, Q/E, mouse | Move, descend/ascend, look |
| Shift / Ctrl while moving | Fast / precise movement |
| Wheel | Field-of-view zoom (orbit radius in orbit mode) |
| 1–9 | Curated camera presets; Shift+1–9 selects instantly |
| 0, O, T, G | Reset camera, orbit, transport follow, guided camera demo |
| F5, Shift+F5, F6 | Start/restart, cancel, pause/resume showcase |
| U, [ / ], F1/F2/F3 | Automatic sun, adjust time, 08:00/12:00/17:00 presets |
| Space, N, R, L | Pause/resume animation, next state, reset, loop |
| B, Home/End, , / . | Timelapse play/pause, start/end, slower/faster |
| F7, F8, F9 | Quarry pulley pause, pyramid interior, cutaway |
| F10, F11, Tab | Debug overlay, simulation HUD, HUD help |
| C, F, H, J, V, X | Culling, wireframe, shadows, shadow debug, lighting debug, textures |
| Esc | Release mouse; press again to exit |

The in-window help panel lists the complete current bindings. Click the viewport to recapture the mouse.

## Scene and rendering

- **Construction:** Block 1000 has one physical transform owner at a time: quarry, pulley, sledge cargo socket, supported unloading route, then permanent pyramid occupancy. The transport includes sand tracks, the main ramp, Ramp A, Ramp B, and a 24 m target landing. Reset restores the first-block cycle. Cinematic timelapse is separate from the physical path.
- **Ground and monuments:** A terrain and dynamic-sand simulation support quarry and site props. The procedural Sphinx and its grounded base are scene landmarks. Indexed pyramid blocks use instanced rendering; the passage and chamber remain inspectable.
- **Nile and boats:** One shaped, indexed Nile mesh animates from the same wave field used by the bounded `waterSurfaceAt` query. Both boats follow the surface for height, pitch, and roll. The supply boat navigates a one-way route and leaves a deterministic stern wake; the cargo boat remains moored. Showcase Nile hard cuts explicitly replay that one-way trip for presentation; normal physical mode never loops it.
- **Atmosphere:** One SunController drives 24-hour directional lighting, sky gradient, moon/stars, cloud tint, and distance haze. Eight deterministic cloud groups drift and wrap. Three grounded night fires have animated flames, embers, smoke, and local point lights.
- **Materials and techniques:** Procedural textures and restrained stone/wood variation complement Blinn–Phong lighting, a directional shadow map with PCF, frustum culling, instanced geometry, and indexed triangle meshes. Water adds flow-aligned normal detail, modest Fresnel, and night highlights without changing its physical surface query.

The complete CTest suite includes construction settlement/reset, Nile and boat coupling, day/night, fires, sky, clouds, haze, Sphinx, renderer structure, and showcase validation. For a quick real-OpenGL check, run `build\PyramidGiza.exe --smoke-test --preset 1`. Captures and build output live under ignored `build/`.

Historical phase notes and design records remain in [docs](docs/).
