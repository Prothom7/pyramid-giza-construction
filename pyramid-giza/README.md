# Pyramid at Giza — Construction Site

Course project status:

- **Phase 1 — Complete:** reusable indexed primitive geometry foundation
- **Phase 2 — Complete:** static Giza construction world

Phase 1 is frozen at Git commit `1a743b6`. The current application builds a recognizable
static construction site from canonical triangle meshes: desert, an unfinished block-built
pyramid, ramp, quarry, prepared-stone stockpile, and wooden construction structures.

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
- `C`: toggle back-face culling
- `F`: toggle filled/wireframe rendering
- `Esc`: exit

## Validation commands

```powershell
build\PyramidGiza.exe --validate-geometry
build\PyramidGiza.exe --validate-scene
build\PyramidGiza.exe --smoke-test --preset 1
```

`--wireframe`, `--no-cull`, and `--capture output.ppm` are available for automated
render-state checks. Generated PPM captures are ignored by Git.

See [docs/PHASE1_PRIMITIVE_FOUNDATION.md](docs/PHASE1_PRIMITIVE_FOUNDATION.md) and
[docs/PHASE2_STATIC_GIZA_WORLD.md](docs/PHASE2_STATIC_GIZA_WORLD.md).
