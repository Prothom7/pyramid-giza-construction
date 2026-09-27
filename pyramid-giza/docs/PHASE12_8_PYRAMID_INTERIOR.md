# Phase 12.8 - Pyramid Interior, Passage Network, and Tomb Chamber

## Objective and continuation

Phase 12.8 resumes the interrupted implementation that began after commit
`6d61145`. It adds a real empty-space route through the completed 28-level
pyramid without replacing the GPU-instanced construction system. The north
(back) entrance is deliberately away from the main south-side construction
ramp.

Phase 12.8.1 subsequently refined the entrance facade without changing the
remaining route. The interior is a stylized historically inspired graphics reconstruction
adapted to this project's scaled 28-level pyramid geometry. It is not presented
as an exact archaeological survey.

## Layout

```text
North facade (z = -76.98), passage start (z = -76.68)
        |
EntryDescending
        |
AscendingPassage
        |
GrandGallery
        |
AntechamberConnector
        |
Antechamber
        |
TombChamber + open stone sarcophagus
```

Four passage volumes form one continuous route. The first descends from
`(0, 7, -76.68)` to `(0, 4.5, -67)`; the second rises to
`(0, 11, -55)`; the taller gallery rises to `(0, 16, -43)`; and a
level connector ends at `(0, 16, -39.5)`. The antechamber is 5 x 4 x 5.6
units. The tomb chamber is 10 x 6 x 15 units.

## Real void classification

`PyramidInterior::blockIntersectsVoid` is the one authoritative predicate.
Each passage supplies a deterministic local frame:

```text
forward = normalize(end - start)
right   = normalize(worldUp x forward)
up      = normalize(forward x right)
```

A block's half-extents are projected onto those axes for passage OBB overlap.
Rooms use AABB overlap. Every region adds a 0.24-unit carving margin so block
faces cannot clip through clean architectural surfaces.

The conceptual construction schedule remains 7,714 generated blocks:

| Quantity | Count |
|---|---:|
| Generated blocks | 7,714 |
| Interior-excluded blocks | 255 |
| Rendered structural blocks at completion | 7,459 |

The 0/25/50/75/100 percent schedule still means
70/4,734/6,964/7,561/7,714 scheduled blocks. Interior-designated blocks are
filtered rather than constructed and later removed. The corresponding rendered
structural counts are 70/4,644/6,709/7,306/7,459.

## Instance and shadow integration

The same predicate filters:

- all eight stable pyramid instance batches;
- both dynamic frontier batches;
- every construction progress value;
- the visible pass; and
- the directional-shadow pass.

The visible and shadow passes consume the same filtered batches. Therefore an
excluded block cannot reappear during timelapse and cannot survive as an
invisible shadow caster. No per-block VAO/VBO/EBO was introduced.

## Architectural surfaces and tomb

Thirty-six staged cube instances provide thin floors, walls, ceilings, the
stepped entrance reveal, room shells, and an open sarcophagus. They all reuse the
existing indexed cube mesh and Limestone, LimestoneVariation, QuarryStone, or
PreparedStone materials. The sarcophagus uses one base plus four sides; its
base rests on the tomb floor at y=16.

## Construction integration

Interior surfaces have deterministic minimum construction progress values from
0.18 to 0.72. Their transforms enter the ordinary `stagedObjects_` list, so
they share the existing visible/shadow object path and support normal frustum
culling. The conceptual historical block counts remain unchanged; only the
rendered instance population is reduced.

## Navigation

`F8` toggles `CameraMode::InteriorWalk`. It stores the exterior camera pose,
places the eye inside the north entrance, and restores the exterior pose when
disabled. W/A/S/D use a 3.2-unit/s normal interior speed (1.44 precision,
5.6 fast). Candidate movement is accepted only inside the union of passage and
room walkable volumes. Passage floor height is interpolated along its slope,
maintaining a 1.70-unit eye height automatically. Mouse look and FOV zoom remain
available.

## Cutaway

`F9` toggles a deterministic inspection cutaway. Blocks with x > 0.25 and
y >= 2 are omitted, exposing the passage route while leaving the construction
data unchanged. Rebuilding the shared instance batches also removes these
blocks from the shadow pass. Cutaway is an inspection feature, not a geometry
mutation.

## Inspection light

Interior mode enables a short-range, warm camera-following inspection light.
Its range is 12 units and intensity is 1.15. Quadratic-style attenuation keeps
it local. It is additive to the existing directional sun, Blinn-Phong
materials, textures, and directional shadow map; it has no extra shadow pass.

## Validation and performance

CPU validation checks 7,714 generated blocks, exclusion arithmetic at all five
construction checkpoints, 764 sampled
clearance points, one shell-reaching entry region containing three deliberately
removed blocks, sarcophagus support,
passage/room connectivity, 14 representative camera positions, a deterministic
small-step entrance-to-tomb traversal, confinement,
and finite light settings. Phase 12.8.1 adds three entrance draws over the
original implementation: six portal parts replace the old three. Across the
complete interior there are 36 normal indexed draws per visible/shadow pass
while 255 completed-pyramid instances are removed. The net completed-pyramid
instance change is -219 in each pass, with no new mesh upload.

The project still uses CCW outward-facing indexed triangles, shared
VAO/VBO/EBO resources, model/view/projection, inverse-transpose normal matrices,
and the CVV/NDC pipeline.

## Deliberate limitations

There is no generic CSG, capsule physics, hieroglyphic decoration, archaeological
survey accuracy, new shadow map, or interior cinematic. The existing showcase
is unchanged; F8 and F9 provide stable manual inspection.
