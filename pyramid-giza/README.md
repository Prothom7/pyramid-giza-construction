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
- **Phase 7 - Complete:** directional sun lighting, material response, and daylight control
- **Phase 8 - Complete:** moving directional-sun shadow mapping with bias and 3x3 PCF
- **Phase 9 - Complete:** scene integrity, independent construction timelapse, dynamic infrastructure, animation refinement, and procedural UV textures

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
visible pass has approximately 9,463 maximum draw calls, while all scene objects still
share the same four uploaded meshes. Pulley/roller rigs are explicitly presented as
speculative graphics demonstrations rather than historically certain Khufu-era machinery.

Phase 7 replaces the earlier fixed light with a world-space directional sun and
centralized ambient/diffuse/specular material properties. The daylight controller
provides deterministic morning, noon, and evening states plus a slow automatic cycle
that is independent of the 28.5-second construction animation. Lighting debug outputs
make diffuse, specular, world-normal, and unlit-base-color behavior easy to inspect.

Phase 8 adds one 4096 x 4096 directional depth map, a stable world-centered orthographic
light camera, slope-aware bias, and manual 3x3 percentage-closer filtering. The existing
sun moves the shadows from long morning silhouettes through shorter noon contact shadows
to long evening shadows in the opposite direction. Static and animated geometry use one
shared per-frame transform list in both passes. With shadows enabled the depth and visible
passes total approximately 18,926 maximum indexed draw calls.

Phase 9 preserves the exact 7,561-block Phase 5.6 scene as the default 75-percent
construction checkpoint and adds a deterministic 90-second timeline from foundation to
the complete 7,714-block pyramid. Temporary ramps, scaffolds, repository stones,
background workers, parked sledges, and speculative pulley wheels respond to the
construction stage. The world ground now covers the calculated content bounds with
34-71 units of horizontal margin and a presentation skirt.

Seven deterministic procedural material textures use the existing UV attribute plus a
shared white fallback. Textures are uploaded once, reused by all matching materials, and
bound separately from the shadow map. The sphere now has an intentional U=0/U=1 seam
duplicate. Maximum estimates are approximately 9,690 visible indexed draws and 19,380
combined shadow plus visible draws.

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
- `U`: toggle automatic daylight motion
- `[` / `]`: move the sun backward/forward by 0.5 simulated hours
- `F1` / `F2` / `F3`: morning (08:00) / noon (12:00) / evening (17:00)
- `V`: cycle normal, diffuse-only, specular-only, world-normal, and unlit lighting output
- `H`: toggle directional shadows
- `J`: toggle normal rendering / shadow-factor visualization

- `X`: toggle procedural material textures
- `B`: play/pause the independent construction timelapse
- `,` / `.`: decrease/increase timelapse speed (0.25x to 8x)
- `Home` / `End`: set construction progress to 0 / 100 percent

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
build\PyramidGiza.exe --validate-lighting
build\PyramidGiza.exe --validate-shadows
build\PyramidGiza.exe --validate-construction
build\PyramidGiza.exe --validate-textures
build\PyramidGiza.exe --validate-layout
build\PyramidGiza.exe --smoke-test --preset 1
```

`--wireframe`, `--no-cull`, `--animation-time SECONDS`, `--camera-mode
free|orbit|follow|demo`, and `--capture output.ppm` support deterministic runtime checks.
`--sun-time HOURS`, `--auto-sun`, `--static-sun`, and `--lighting-mode
normal|diffuse|specular|normals|unlit` support deterministic lighting checks.
`--shadows`, `--no-shadows`, `--shadow-debug-factor`, and
`--shadow-resolution 2048|4096` support deterministic shadow checks.
`--smoke-duration SECONDS` keeps the hidden smoke-test renderer active for a timed
animation regression (up to 60 seconds).
Construction startup options are --construction-progress 0..1, --timelapse, and
--timelapse-speed 0.25..8. Use --textures or --no-textures for A/B runtime checks.
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
- [Phase 7 lighting and sun](docs/PHASE7_LIGHTING_AND_SUN.md)
- [Phase 8 directional shadow mapping](docs/PHASE8_SHADOW_MAPPING.md)
- [Phase 9 integration, timelapse, and textures](docs/PHASE9_INTEGRATION_TIMELAPSE_TEXTURES.md)

Excel-compatible records are stored in `docs/*.csv`, including the quarry, extraction,
repository, logistics, environment, lifting-mechanism, ramp, world-scale, enrichment,
pulley-rig, river-landing, workshop/repair, scaffold-access, material, sun-state, and
lighting-control, shadow-setting, shadow-validation, and shadow-control tables.
Phase 9 adds world-bounds, layout-validation, construction-timeline, construction-stage,
infrastructure-stage, texture, UV-mapping, material-texture, and texture-control tables.
