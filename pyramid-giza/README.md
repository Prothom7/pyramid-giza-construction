# Pyramid at Giza - Construction Site

Course project status:

- **Phase 1 - Complete:** reusable indexed primitive geometry
- **Phase 2 - Complete:** static Giza world
- **Phase 3 - Complete:** composite workers and equipment
- **Phase 4 - Complete:** hierarchical articulated workers
- **Phase 5 - Complete:** coordinated 28.5-second construction animation
- **Phase 5.5 - Complete:** monumental scale and expanded site
- **Phase 5.6 - Complete:** quarry, stone logistics, Nile, and environmental context
- **Phase 6 - Complete:** camera navigation, inspection modes, and guided presentation
- **Phase 6.5 - Complete:** pre-Phase 7 machinery, access, workshop, repair, and river-logistics enrichment

The application is now a full ancient industrial landscape. A 7,561-block unfinished
pyramid remains the focal point, while a recessed open-cut quarry, four extraction bays,
three stone repositories, loading station, long haul road, worker-access infrastructure,
an experimental rope-redirection rig, Nile/floodplain, 18 trees, support camp, and a
secondary stylized Sphinx-context landmark explain the wider site. Phase 6.5 adds three
new experimental rope-redirection rigs (four frames total), 14 anchor posts, eight
ladders, four access walkways, a tool workshop, stone inspection station, sledge repair
yard, organized lever racks, an upper-platform handling cluster, and a Nile landing with
two primitive-built boats.

All objects reuse four uploaded primitive meshes (plane, cube, cylinder, sphere). Filled
rendering remains indexed `GL_TRIANGLES`; scene instances only supply model transforms and
materials. The original Phase 5 animation remains a focused local transport stage within
the longer static quarry-to-pyramid logistics chain.

Phase 6 adds three movement speeds, smooth/instant curated presets, pyramid orbit,
transport follow, a seven-shot guided demo, safe FOV zoom, and debug pose output without
changing world geometry or the rendering pipeline.

The enrichment pass adds 327 static primitive instances and four support workers. The
maximum draw count is approximately 9,463, while all scene objects still share the same
four uploaded meshes. Pulley/roller rigs are explicitly presented as speculative
graphics demonstrations rather than historically certain Khufu-era machinery.

## Build and run

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
build\PyramidGiza.exe
```

## Controls

- `W/A/S/D`: move; `Q/E`: move down/up; mouse: look
- `Shift + movement`: fast (56 units/s); `Ctrl + movement`: precision (7 units/s)
- mouse wheel: FOV zoom; in orbit mode: radius
- `1`: monumental pyramid overview
- `2`: pyramid base
- `3`: quarry overview
- `4`: extraction bays
- `5`: repositories and transport
- `6`: scaffold and upper construction
- `7`: complete site overview
- `8`: Nile and floodplain
- `9`: Sphinx-context landmark
- `Shift + 1-9`: instant preset instead of the normal one-second transition
- `0`: reset camera to overview and normal speed
- `O`: toggle pyramid orbit
- `T`: toggle animated transport follow
- `G`: start/stop the guided seven-shot demo
- `K`: print camera position, yaw, pitch, FOV, and mode
- `C`: toggle back-face culling; `F`: toggle filled/wireframe
- `Space`: pause/resume; `N`: next state; `R`: reset; `L`: loop
- `M`: coordinated animation/pose-preview mode; `+/-`: animation speed
- `P`: cycle debug pose; `Esc`: exit

## Validation

```powershell
build\PyramidGiza.exe --validate-geometry
build\PyramidGiza.exe --validate-scene
build\PyramidGiza.exe --validate-composites
build\PyramidGiza.exe --validate-hierarchy
build\PyramidGiza.exe --validate-animation
build\PyramidGiza.exe --validate-site
build\PyramidGiza.exe --validate-industrial
build\PyramidGiza.exe --validate-camera
build\PyramidGiza.exe --validate-enrichment
build\PyramidGiza.exe --smoke-test --preset 1
```

`--wireframe`, `--no-cull`, `--animation-time SECONDS`, `--camera-mode
free|orbit|follow|demo`, and `--capture output.ppm` support deterministic runtime checks.
Build output and captures are ignored by Git.

## Documentation

- [Phase 1](docs/PHASE1_PRIMITIVE_FOUNDATION.md)
- [Phase 2](docs/PHASE2_STATIC_GIZA_WORLD.md)
- [Phase 3](docs/PHASE3_COMPOSITE_OBJECTS.md)
- [Phase 4](docs/PHASE4_HIERARCHICAL_MODELING.md)
- [Phase 5](docs/PHASE5_CONSTRUCTION_ANIMATION.md)
- [Phase 5.5](docs/PHASE5_5_MONUMENTAL_SCALE.md)
- [Phase 5.6](docs/PHASE5_6_QUARRY_LOGISTICS_ENVIRONMENT.md)
- [Phase 6](docs/PHASE6_CAMERA_NAVIGATION.md)
- [Phase 6.5 object enrichment](docs/PHASE6_5_OBJECT_ENRICHMENT.md)

Excel-compatible records are stored in `docs/*.csv`, including the quarry, extraction,
repository, logistics, environment, lifting-mechanism, ramp, world-scale, enrichment,
pulley-rig, river-landing, workshop/repair, and scaffold-access tables.
