# Phase 7 - Lighting, Materials, and Moving Sun

## Objective

Phase 7 adds a reusable illumination system to the complete Giza world without changing
its geometry, draw-call organization, camera system, or construction animation. One
directional sun, centralized material properties, a daylight clock, and shader debug
modes now make normals and surface response visible across the entire site. Shadow
mapping is deliberately deferred to Phase 8.

## World-space Blinn-Phong model

The fragment shader performs all lighting calculations in world space. The vertex shader
outputs `WorldPosition` and `WorldNormal`; the camera position and directional sun are
also world-space values.

```text
N = normalize(interpolated world normal)
L = normalize(-sunDirection)
V = normalize(cameraPosition - worldPosition)
H = normalize(L + V)

diffuseFactor  = max(dot(N, L), 0)
specularFactor = diffuseFactor > 0
               ? pow(max(dot(N, H), 0), shininess)
               : 0

FinalColor = Ambient + Diffuse + Specular
```

The three terms are:

```text
Ambient  = ambientIntensity * materialAmbient * baseColor * ambientColor
Diffuse  = sunIntensity * materialDiffuse * diffuseFactor * baseColor * sunColor
Specular = sunIntensity * materialSpecular * specularFactor * sunColor
```

This is Blinn-Phong rather than PBR. It is intentionally compact enough to derive and
explain in a graphics viva.

## Directional sunlight

`DirectionalLight` stores a normalized world-space ray direction, RGB color, and scalar
intensity. A directional source is appropriate because solar rays are effectively
parallel over this scene. The shader negates the stored ray direction to obtain the
surface-to-light vector `L`.

Global sun, ambient, camera, projection, and debug uniforms are uploaded once per frame.
Only model, normal matrix, and material values change per draw. No extra render pass or
per-object CPU light calculation was introduced.

## Time of day and sun trajectory

`SunController` uses daylight hours in `[06:00, 18:00]`. The default is 08:00 with
automatic progression enabled. Automatic time advances at `0.25` simulated hours per
real second (15 simulated minutes per second) and loops from evening to morning.

For normalized daylight progress `p`:

```text
p         = (time - 6) / 12
elevation = 12 degrees + 66 degrees * sin(pi * p)
azimuth   = -105 degrees + 210 degrees * p
```

The scene-to-sun vector is constructed from azimuth/elevation and normalized; the stored
light direction is its negative. This gives a high noon sun and opposite low-angle
morning/evening directions while keeping the daylight sun above the ground. The same
direction/color/intensity state is ready for the Phase 8 light-space shadow calculation.

## Deterministic sun states

- Morning: 08:00, warm light, strong angled contrast.
- Noon: 12:00, high elevation, neutral/bright light and stronger top lighting.
- Evening: 17:00, lower light from the opposite side with a restrained warm tint.

Sun color, intensity, ambient color, ambient intensity, and clear-sky color interpolate
smoothly over the daylight interval. There is no night system, moon, fire, or additional
fake directional light.

## Independent clocks

The sun clock and 28.5-second construction animation are separate. `Space` affects worker
animation only. `U` affects automatic daylight only. Animation reset, looping, and speed
do not modify the sun, and manual sun changes do not modify workers, ropes, sledges,
mallets, levers, or stones.

## Correct transformed normals

Each draw still computes:

```cpp
transpose(inverse(mat3(model)))
```

This inverse-transpose normal matrix is required for the many non-uniformly scaled
blocks, limbs, ramps, poles, boats, pulley frames, and quarry walls. The vertex shader
normalizes the transformed normal, and the fragment shader normalizes it again because
raster interpolation does not preserve unit length. All scene transforms retain positive
scale, so the CCW front-face convention is unchanged.

## Centralized materials

`Material` contains:

```text
baseColor
ambientStrength
diffuseStrength
specularStrength
shininess
```

The 17 explicit presets cover sand, two limestone variations, rough quarry stone,
prepared stone, ramp earth, two timber tones, skin, linen, blue cloth, headwear, rope,
copper-like tools, water, floodplain, and foliage. The complete coefficients and uses
are recorded in `materials.csv`.

- Limestone uses strong diffuse and restrained specular response.
- Raw quarry stone, sand, ramp earth, rope, floodplain, and foliage remain rough.
- Wood is darker and slightly more responsive than earth.
- Skin and cloth remain readable without plastic-like highlights.
- Water has the strongest non-metal specular coefficient and higher shininess, but no
  reflection, refraction, Fresnel, or wave system.
- Tool metal uses a copper-like base and sharper highlight inside the same common shader.

## Lighting debug modes

`V` cycles five fragment-shader outputs:

1. Normal Blinn-Phong
2. Diffuse only
3. Specular only
4. World normals mapped from `[-1,1]` to `[0,1]`
5. Unlit base color

Normal visualization is particularly useful for checking cube faces, cylinders,
spheres, quarry walls, boats, and thin scaffold members. It does not alter or hide mesh
normals.

## Visual response by scene area

- **Pyramid:** morning/evening illuminate opposing faces; noon emphasizes upper-facing
  block surfaces while the stepped silhouette remains visible.
- **Quarry:** directional contrast exposes terrace depth, extraction bays, and wall steps.
- **Workers:** ambient contribution keeps heads, articulated limbs, torso, skin, and cloth
  readable in all presets.
- **Scaffolds and machinery:** correct normal matrices preserve shading on thin,
  non-uniform beams, rollers, anchors, ladders, and ropes.
- **Nile:** common Blinn-Phong lighting gives the water a tighter specular response without
  any separate water pass.
- **Vegetation and Sphinx context:** both use the same coherent sun and remain secondary.

## Controls and test hooks

Interactive controls are recorded in `lighting_controls.csv`. Deterministic runtime tests
also support:

```powershell
build\PyramidGiza.exe --sun-time 8 --static-sun
build\PyramidGiza.exe --sun-time 12 --lighting-mode normals --smoke-test
build\PyramidGiza.exe --auto-sun
```

Valid debug names are `normal`, `diffuse`, `specular`, `normals`, and `unlit`.

## CPU validation

`--validate-lighting` verifies normalized/finite morning, noon, and evening states;
azimuth/elevation changes; 60 x 1/60 versus 30 x 1/30 time progression; deterministic
automatic daylight wrapping; a disabled clock remaining fixed; deterministic manual
presets; all material ranges; inverse-transpose normal orthogonality under non-uniform
scaling; and deterministic debug-mode cycling.

## Performance and preserved constraints

Maximum draw calls remain approximately 9,463. Lighting adds no geometry, mesh upload,
framebuffer pass, or new VAO/VBO/EBO. Filled rendering remains indexed
`glDrawElements(GL_TRIANGLES, ...)` with CCW fronts, back-face culling, reusable
primitive meshes, and the existing model-view-projection to clip-space/NDC pipeline.
Shader compilation/link errors remain fatal and are reported by the existing `Shader`
wrapper.

## Deferred to Phase 8

Phase 7 does **not** create a depth framebuffer, shadow texture, light-space matrix,
shadow comparison, PCF, or cascaded shadow maps. Phase 8 can consume the existing
directional sun state directly when directional-light shadow mapping is implemented.
