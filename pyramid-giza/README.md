# Pyramid at Giza — Construction Site

Course project status:

- **Phase 1 — Complete:** reusable indexed primitive geometry foundation
- **Phase 2 — Complete:** static Giza construction world
- **Phase 3 — Complete:** static composite workers and construction equipment
- **Phase 4 — Complete:** hierarchical joints and articulated rigid-part workers

Phase 1 is frozen at Git commit `1a743b6`; Phase 2 is frozen at `718ccb0`. The current
application builds a recognizable inhabited construction site from canonical indexed
triangle meshes: desert, unfinished block-built pyramid, ramp, quarry, stockpile, seven
articulated workers, two static sledges, a transported stone, lever, mallet, and wooden
supports. One foreground worker provides a stationary hierarchy demonstration.

## Build and run

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
build\PyramidGiza.exe
```

## Controls

- `W/A/S/D`: move forward/left/back/right
- `Q/E`: move down/up
- mouse: look
- `1`: overview
- `2`: pyramid and ramp
- `3`: quarry
- `4`: ramp and staging area
- `5`: workers and loaded sledge
- `C`: toggle back-face culling
- `F`: toggle filled/wireframe rendering
- `Space`: pause/resume the articulated-worker preview
- `P`: cycle the demo worker through action and diagnostic poses
- `R`: reset the demo worker to Standing
- `Esc`: exit

## Validation commands

```powershell
build\PyramidGiza.exe --validate-geometry
build\PyramidGiza.exe --validate-scene
build\PyramidGiza.exe --validate-composites
build\PyramidGiza.exe --validate-hierarchy
build\PyramidGiza.exe --smoke-test --preset 1
```

`--wireframe`, `--no-cull`, and `--capture output.ppm` are available for automated
render-state checks. Generated PPM captures are ignored by Git.

See [docs/PHASE1_PRIMITIVE_FOUNDATION.md](docs/PHASE1_PRIMITIVE_FOUNDATION.md) and
[docs/PHASE2_STATIC_GIZA_WORLD.md](docs/PHASE2_STATIC_GIZA_WORLD.md).
Phase 3 composition, local transforms, and reuse are documented in
[docs/PHASE3_COMPOSITE_OBJECTS.md](docs/PHASE3_COMPOSITE_OBJECTS.md).
Phase 4 joint frames, pivot mathematics, limits, and pose controls are documented in
[docs/PHASE4_HIERARCHICAL_MODELING.md](docs/PHASE4_HIERARCHICAL_MODELING.md).
