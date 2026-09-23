# Pyramid at Giza - Construction Site

Course project status:

- **Phase 1 - Complete:** reusable indexed primitive geometry foundation
- **Phase 2 - Complete:** static Giza construction world
- **Phase 3 - Complete:** composite workers and construction equipment
- **Phase 4 - Complete:** hierarchical joints and articulated rigid-part workers
- **Phase 5 - Complete:** coordinated, time-based construction animation
- **Phase 5.5 - Complete:** monumental scale and expanded construction site

The application now presents a large organized construction environment built only from
the project's indexed primitive meshes. Its 81.64-unit unfinished pyramid contains 7,561
procedurally placed blocks and rises 20.35 worker-heights above the site. Three ramps,
22 reusable scaffold modules, a distant quarry, cutting and stockpile stages, transport
lanes, timber storage, shelters, 25 workers, and six sledges establish the larger world.

The existing 28.5-second hero sequence remains active: workers approach and pull a loaded
sledge, climb the main ramp, and prepare a lever-assisted placement.

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
- `1`: monumental overview
- `2`: pyramid base
- `3`: quarry and cutting zone
- `4`: main ramp
- `5`: hero transport animation
- `6`: scaffold and upper work zone
- `7`: wide site overview
- `C`: toggle back-face culling
- `F`: toggle filled/wireframe rendering
- `Space`: pause/resume the coordinated animation
- `N`: advance to the next animation state
- `L`: toggle looping
- `M`: toggle coordinated animation / Phase 4 pose demonstration
- `+/-`: increase/decrease animation speed from 0.25x to 4x
- `P`: cycle the Phase 4 demo pose
- `R`: reset the construction sequence to Idle
- `Esc`: exit

## Validation commands

```powershell
build\PyramidGiza.exe --validate-geometry
build\PyramidGiza.exe --validate-scene
build\PyramidGiza.exe --validate-composites
build\PyramidGiza.exe --validate-hierarchy
build\PyramidGiza.exe --validate-animation
build\PyramidGiza.exe --validate-site
build\PyramidGiza.exe --smoke-test --preset 1
```

`--wireframe`, `--no-cull`, `--animation-time SECONDS`, and `--capture output.ppm`
are available for deterministic render-state checks. Generated build files and captures
are ignored by Git.

## Documentation

- [Phase 1 primitive foundation](docs/PHASE1_PRIMITIVE_FOUNDATION.md)
- [Phase 2 static world](docs/PHASE2_STATIC_GIZA_WORLD.md)
- [Phase 3 composite objects](docs/PHASE3_COMPOSITE_OBJECTS.md)
- [Phase 4 hierarchy](docs/PHASE4_HIERARCHICAL_MODELING.md)
- [Phase 5 animation](docs/PHASE5_CONSTRUCTION_ANIMATION.md)
- [Phase 5.5 monumental scale](docs/PHASE5_5_MONUMENTAL_SCALE.md)

Excel-compatible Phase 5.5 records are in `docs/world_scale.csv`,
`docs/site_zones.csv`, `docs/ramp_network.csv`, `docs/scaffold_layout.csv`, and
`docs/environment_instances.csv`.
