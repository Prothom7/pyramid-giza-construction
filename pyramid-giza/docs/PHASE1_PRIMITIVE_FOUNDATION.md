# Phase 1: Primitive Geometry Foundation

## Scope and repository assessment

This phase replaces the partial Giza scene with a small primitive showcase. The retained
infrastructure is C++17, CMake, OpenGL 3.3 Core, GLFW 3.6, GLAD 0.1.36, GLM 1.1, the
camera concept, compiler-warning configuration, and repository metadata. Vendored
`stb_image.h` and the sand texture remain available for a later texture phase but are not
part of the active renderer.

The previous `main.cpp` owned raw arrays and GPU handles for a texture-only cube and a
large ground plane. It duplicated each triangle's vertices, used `glDrawArrays`, had no
EBOs, normals, lighting, face culling, or reusable mesh abstraction, and rendered many
blocks produced by the old `Pyramid` scene class. That active scene implementation was
replaced, and `src/pyramid.cpp` / `src/pyramid.h` were removed so there is only one
rendering architecture.

Phase 1 deliberately does **not** contain the final Giza scene, workers, quarry, sledges,
animation, textures, shadows, or imported models.

## Architecture

```text
PrimitiveGenerator (CPU mathematics)
               |
         MeshData / Vertex
               |
    geometry validation checks
               |
 Mesh::upload -> VAO + VBO + EBO
               |
 glDrawElements(GL_TRIANGLES)
               |
 model -> view -> projection
               |
     clip coordinates -> NDC
```

- `Vertex` stores position, normal, and a reserved texture coordinate.
- `MeshData` stores CPU vertices and 32-bit triangle indices without depending on an
  OpenGL context.
- `PrimitiveGenerator` creates topology and attributes.
- `GeometryValidation` checks bounds, index validity, triangle degeneracy, unit normals,
  expected counts, and compatibility between CCW geometric normals and vertex normals.
- `Mesh` owns one VAO, VBO, and EBO. It is move-only, uploads once, draws with
  `glDrawElements`, and deletes all three resources in its destructor.
- `ShowcaseScene` owns one mesh per primitive and only supplies model matrices and colors
  per draw. The same cube mesh is drawn three times, including as non-uniform cuboids.
- `Shader` owns a checked GLSL program and deletes it through RAII.

No VAO/VBO/EBO is created per triangle or per transformed instance.

## Graphics pipeline

1. The CPU generator creates unique **render vertices** and triangle indices.
2. `Mesh::upload` copies `Vertex` records to the VBO and indices to the EBO. The VAO
   records the position, normal, and UV layouts plus the associated EBO.
3. `Mesh::draw` binds the VAO and calls `glDrawElements(GL_TRIANGLES, ...)`.
4. The vertex shader transforms object position with `projection * view * model` and
   transforms normals with the inverse-transpose normal matrix.
5. Clipping and perspective division produce normalized device coordinates.
6. Triangle rasterization interpolates world position and normal.
7. The fragment shader uses ambient, diffuse, and specular terms so reversed or otherwise
   incorrect normals are visually obvious.

Only `GL_TRIANGLES` is used for filled geometry. There is no immediate mode,
`GL_TRIANGLE_FAN`, GLUT/GLU geometry, imported model, or hidden mesh library.

## Coordinate systems and the CVV

The coordinate stages are distinct:

```text
Object/Model Coordinates
        | model matrix
World Coordinates
        | view matrix
View/Eye Coordinates
        | projection matrix
Clip Coordinates
        | divide x, y, z by w
NDC / Canonical View Volume (approximately -1 to +1 on x, y, z)
```

The canonical meshes are centered at the origin and remain inside approximately
`[-0.5,+0.5]`. This is a convenient **object-space** convention, not a world-space limit.
Translation, rotation, and scale may put an object anywhere in world space. The view and
perspective projection matrices determine whether it maps into the OpenGL CVV/NDC range
`-1 <= x,y,z <= +1` after perspective division. World coordinates are never clamped.

GLM post-multiplies the transform calls used here, so the constructed model matrix is
`M = T * R * S`: a local vertex is scaled, then rotated, then translated.

## Indexed rendering and render vertices

An index names a complete render vertex, not merely a position. Therefore:

```text
unique position != always unique render vertex
```

A cube has only eight geometric corner positions, but a corner belongs to three faces
with three different flat normals (and potentially different UVs). The cube consequently
uses 24 render vertices—four per face—and 36 indices. This is required attribute
duplication, while the two triangles of each face still reuse their four vertices.

The same rule separates cylinder side vertices from cap vertices: their positions can
coincide, but radial side normals differ from vertical cap normals. The cylinder side also
duplicates its seam sample because texture `u=0` and `u=1` are different attributes.

## Winding, faces, culling, and normals

OpenGL is configured with:

```cpp
glEnable(GL_CULL_FACE);
glCullFace(GL_BACK);
glFrontFace(GL_CCW);
```

For every triangle, `(v1 - v0) x (v2 - v0)` points toward the exterior and agrees with
the supplied normals. Thus a front face appears counter-clockwise when viewed from its
exterior. Reversing two indices reverses both the geometric normal and front/back result.

Flat faces share a constant face normal. Cylinder sides use radial normals. Sphere normals
are `normalize(position)` because the sphere is centered at the origin. The disk/top cap
use `+Y`; the bottom cylinder cap and pyramid base use `-Y`.

Non-uniform scaling cannot correctly transform a normal with the ordinary model matrix.
The CPU supplies:

```cpp
transpose(inverse(mat3(model)))
```

as `normalMatrix`. The showcase's stretched cuboid is an explicit visual test of this.

## Primitive construction and exact default counts

| Primitive | Generation | Vertices | Indices | Triangles |
|---|---|---:|---:|---:|
| Triangle | three XY-plane points, `+Z` normal | 3 | 3 | 1 |
| Plane | four XZ-plane corners, two indexed triangles, `+Y` normal | 4 | 6 | 2 |
| Cube | six faces with four render vertices per flat-normal domain | 24 | 36 | 12 |
| Pyramid | four separately normaled side triangles plus four base vertices | 16 | 18 | 6 |
| Disk (32) | center plus 32 angular rim samples | 33 | 96 | 32 |
| Cylinder (32) | indexed side plus independent top and bottom caps | 132 | 384 | 128 |
| Sphere (32 x 32) | two poles plus 31 rings of 33 seam-aware vertices | 1025 | 5952 | 1984 |

### Triangle and plane

The triangle verifies the minimum VBO/EBO pipeline. The horizontal plane uses four
vertices rather than six; indices `(0,2,1)` and `(0,3,2)` face `+Y`. It can become any
rectangle later through model scaling.

### Cube

Each face is a four-vertex indexed quad split into two triangles. All positions use
`+/-0.5`. Per-face render vertices preserve hard edges and correct flat lighting. Model
scaling turns the same cube mesh into later pyramid blocks, stones, sledge parts, and
wooden structures.

### Pyramid

The apex is `(0,+0.5,0)` and the square base lies at `y=-0.5`. Each triangular side has
its own outward cross-product normal. The base is a four-vertex quad split into two CCW
triangles as seen from below.

### Disk

For segment `i`:

```text
theta = 2*pi*i/segments
x = radius*cos(theta)
z = radius*sin(theta)
```

The center and rim are stored in a normal indexed mesh. Indices form center/rim/rim
triangles with modulo wraparound; rendering still uses `GL_TRIANGLES`, not
`GL_TRIANGLE_FAN`.

### Cylinder and surface of revolution

The cylinder samples the same angular formula at `y=-height/2` and `y=+height/2`.
Adjacent samples form two side triangles. Side normals are `(cos(theta),0,sin(theta))`.
Independent center/rim vertices form the `+Y` top cap and `-Y` bottom cap. For `S`
segments the default construction has `4S+4` vertices, `12S` indices, and `4S`
triangles.

This is “drawing by rotating”: a vertical two-point profile is rotated around the Y axis,
then adjacent angular copies are connected.

### Sphere and angular revolution

For latitude angle `phi` and longitude angle `theta`:

```text
x = radius*sin(phi)*cos(theta)
y = radius*cos(phi)
z = radius*sin(phi)*sin(theta)
normal = normalize(position)
```

There is one vertex at each pole and indexed rings between them. Pole triangles are
created once without zero-area quads or duplicated degenerate triangles. For latitude
count `L` and longitude count `S`, counts are `2+(L-1)S` vertices,
`6S(L-1)` indices, and `2S(L-1)` triangles.

## Showcase and controls

The two-row showcase contains triangle, plane, cube/cuboids, pyramid, disk, cylinder, and
sphere. Fixed and gentle animated rotations reveal multiple faces. Three differently
transformed blocks prove that one cube VAO/VBO/EBO is reusable.

- `W/A/S/D`: camera forward/left/back/right
- mouse: yaw and pitch
- `C`: toggle back-face culling
- `F`: toggle fill/wireframe (chosen so `W` remains camera-forward)
- `Esc`: exit

## Build and validation

From the project directory on the provided MinGW setup:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\PyramidGiza.exe
```

Useful validation modes:

```powershell
.\build\PyramidGiza.exe --validate-geometry
.\build\PyramidGiza.exe --smoke-test
.\build\PyramidGiza.exe --smoke-test --capture phase1_showcase.ppm
```

The CPU test checks all seven primitive counts, every index, non-degenerate area, normal
length, object-space bounds, and winding/normal agreement. The smoke test creates a real
OpenGL context, compiles and links GLSL, uploads/draws every mesh for three frames, and
fails if `glGetError` reports an error. The optional PPM capture is ignored by Git.

## Geometry records

- `docs/geometry_coordinates.csv`: manually defined triangle, plane, cube, and pyramid
  render vertices.
- `docs/geometry_indices.csv`: their exact triangle indices and faces.
- `docs/procedural_geometry.csv`: formulas, parameters, and expected counts for disk,
  cylinder, and sphere.

These CSV files open directly in Excel without adding a spreadsheet-library dependency.

## Support for later Giza phases

Later scene objects should hold a reference to one shared primitive mesh plus a model
matrix/material, never another copy of identical geometry. The current split keeps scene
placement independent of primitive mathematics and GPU ownership, so blocks, quarry
stones, wooden frames, sledges, and other objects can reuse these exact meshes without
changing the Phase 1 rendering foundation.
