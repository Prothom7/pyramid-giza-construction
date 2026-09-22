# Pyramid at Giza — Construction Site

Phase 1 provides a reusable modern OpenGL primitive foundation. It intentionally shows a
primitive validation scene rather than the final Giza environment.

## Build and run

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\PyramidGiza.exe
```

Controls: `W/A/S/D` move, mouse looks, `C` toggles back-face culling, `F` toggles
wireframe, and `Esc` exits.

CPU-only geometry validation is available through:

```powershell
.\build\PyramidGiza.exe --validate-geometry
```

See [docs/PHASE1_PRIMITIVE_FOUNDATION.md](docs/PHASE1_PRIMITIVE_FOUNDATION.md) for the
architecture, mathematics, exact primitive counts, and viva notes.
