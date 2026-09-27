# Phase 12.8.1 - Entrance Facade Repair

## Original visual problem

The original Phase 12.8 entrance used two jamb cuboids and one lintel centered
at z=-81.30. Their front faces reached approximately z=-81.55 even though the
actual nearby pyramid faces are z=-78.44 and z=-76.98. The frame therefore sat
more than three units in front of the generated masonry, and the passage itself
began at z=-83. It read as a separate doorway attached to the pyramid.

## Pyramid-face derivation

The repair uses the same layout mathematics as the block generator:

```text
side(level)       = baseBlocksPerSide - level
zStep             = blockDepth + horizontalSpacing
northBlockCenter  = originZ - 0.5 * (side - 1) * zStep
northFaceZ        = northBlockCenter - 0.5 * blockDepth
```

With 28 base blocks, 2.8-unit block depth, 0.12 spacing, and origin z=-42:

- level 3 lower face: z=-78.44;
- level 4 upper/opening face: z=-76.98.

These values are calculated by `PyramidInterior::northFaceZ`, not duplicated
as portal-placement constants.

## Entrance descriptor

`PyramidEntranceDescriptor` centralizes the 2.60 x 3.00 opening, floor y=7,
levels 3/4, both derived facade planes, 0.30 passage recess, 0.75 reveal depth,
and 0.06 facade tolerance. The entry passage now begins at
`(0, 7, -76.68)`, exactly 0.30 behind the level-4 face.

## Carving and masonry

The authoritative `blockIntersectsVoid` predicate remains in control of
stable batches, frontier batches, visible/shadow passes, and placement dust.
A dedicated entrance classification removes a symmetrical three-block shell
pattern: the central level-3 block and the two central level-4 blocks. All
other north-shell blocks are protected from the general passage OBB.

Six shared-cube pieces replace the old protruding three-part frame:

1. an inset threshold bridging the projecting lower course to the passage;
2. left and right lower-course reveals;
3. left and right upper-course reveal/infill stones;
4. a thin top reveal below the natural next course.

Each front face is 0.05 units inward from its corresponding course. The result
follows the pyramid's 1.46-unit course setback. No chevron was added because a
second decorative layer would weaken the restrained carved-opening appearance.

## Passage, navigation, and light

The threshold overlaps the passage floor without a vertical discontinuity.
The walkable entrance volume connects from 0.20 units outside the lower face
to the recessed passage start, keeps the 1.70 eye height, and rejects lateral
wall penetration. The rest of the passage network, rooms, tomb, and
sarcophagus are unchanged.

The opening uses real excluded blocks; no black rectangle masks masonry.
Directional sunlight and the existing shadow pass create the exterior recess.
The F8 inspection light remains camera-local and is not enabled for ordinary
exterior inspection.

## Validation

`Phase12_8_1EntranceFacadeValidation` verifies:

- both facade planes are derived from the generated layout;
- left/right/top offsets are 0.0500031, within the 0.06 tolerance;
- maximum outward protrusion is zero;
- recess is 0.300003;
- opening clearance is 2.6 x 3.0;
- exactly three intended shell blocks are excluded;
- zero unintended exterior breaches exist;
- outside-to-passage small-step traversal succeeds.

The complete pyramid now has 7,714 conceptual blocks, 255 interior exclusions,
and 7,459 rendered structural blocks. The repair changes entrance
architectural draws from three to six, a net increase of only three normal
draws per visible/shadow pass. All pieces reuse the existing indexed cube mesh.

## Visual audit

Temporary front, left-oblique, right-oblique, low, high, and wireframe captures
were inspected at completed construction. The front surfaces follow their
surrounding courses, the threshold joins the entry floor, the recess remains
legible, and no block/reveal clipping was observed. Front views at 08:00,
12:00, and 17:00 confirmed that geometry, not a fake dark mask, creates the
opening contrast. Captures remain uncommitted and are removed after review.

## Historical qualification

The entrance treatment is a stylized historically inspired reconstruction
adapted to the project's scaled block geometry and is not an exact
archaeological survey.
