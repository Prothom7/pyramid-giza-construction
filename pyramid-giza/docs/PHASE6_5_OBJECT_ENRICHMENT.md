# Phase 6.5 - Pre-Phase 7 Object Enrichment

## Objective

The monumental Phase 5.6 world already explained the large quarry-to-pyramid route, but
several work zones still read as broad shapes from close camera positions. This additive
pass gives those zones functional silhouettes: rope handling, access, tool maintenance,
inspection, sledge repair, material storage, and Nile-side transport. The pyramid,
quarry, roads, repositories, camera system, and 28.5-second Phase 5 animation are
preserved rather than rebuilt.

## Placement overview

```text
Nile landing/boats ---- floodplain ---- plateau logistics road
                                             |
Quarry -> extraction assist -> repositories -> inspection/loading -> main ramp
                                                               |          |
                                                          repair yard   upper tools
```

All placement is deterministic. `ObjectEnrichment` owns the reusable descriptors for
rigs, anchor posts, ladders, and boats; `StaticGizaScene` expands the descriptors into
scene instances once during construction.

## Rope-redirection rigs and historical uncertainty

Three new rigs complement the existing Phase 5.6 lifting frame:

1. a horizontal quarry-exit redirection frame with two rollers;
2. an A-frame loading visualization with one suspended block;
3. a ramp-side assist frame placed outside the animated haul lane.

The wheel, axle, frame, brace, rope, and load shapes reuse cube and cylinder meshes.
Ropes use the existing cylinder-between-two-points transform. The code and CSV records
label every new rig **speculative/experimental**. They are graphics demonstrations of
possible rope redirection or lifting, not claims that this exact machinery was used in
Khufu's reign.

Fourteen heavy anchor posts are grouped at the quarry exit, loading station, ramp,
upper platform, and river landing. Each is a shared cylinder plus a transverse securing
peg. No rope physics or active machinery animation was added.

## Ladders and scaffold access

Eight ladders connect quarry terraces, scaffold levels, the west access platform, the
inspection bed, and the quay. Each ladder has two local rails and 5-13 repeated rungs.
Four narrow scaffold walkways add deck-and-rail access. The ladder generator checks
positive dimensions and rung spacing between 0.35 and 0.65 world units.

## Workshop and tool maintenance

The workshop beside the camp contains a workbench, a framed tool rack, six readable
mallet/chisel silhouettes, a primitive sharpening wheel with axle and supports, and
storage crates. These are deliberately coarse at worker scale: their purpose should be
clear without introducing unique handheld meshes.

## Measurement, inspection, and loading

The inspection station between dressing and loading contains a prepared block on a
raised bed, four measuring rods, a rope reference line, and a plumb-line marker. Two
short timber loading rails, waiting material, anchors, and the nearby A-frame clarify
the loading workflow while leaving the hero sledge path open.

## Sledge repair and timber/lever storage

The timber-yard repair station uses a visibly incomplete sledge: two runners, only three
cross members, spare runners, a bench, and repair mallets. Three organized racks store
long lever/timber members at the quarry exit, loading area, and repair yard. This is a
static maintenance interpretation; no extra moving sledge was added.

## River landing and boats

The Nile now has a segmented stone quay, a floodplain approach, four mooring anchors,
landing ladder, cargo crates, timber storage, and a small shelter. Two stylized boats
are assembled from cuboids and cylinders: hull floor, sloped sides, raised ends,
benches, mast/pole, and a simple linen panel. One boat is tied to the quay by two static
rope segments; the other sits offshore. Boats and landing activity are broad logistics
context, not a detailed archaeological boat reconstruction.

## Upper construction and support area

The upper placement zone gains queued blocks, alignment/string posts, block-guide rails,
an upper lever rack, and access ladders/walkways. All elements remain off the animated
arrival centerline. The support zone gains an admin/work table and seating, plus four
new background workers distributed between workshop, repair, and river logistics.

## Historical-certainty classification

- **Higher-confidence visual elements:** ladders, sledges, lever/timber beams, work
  benches, pounding tools, alignment markers, broad landing/boat logistics, storage.
- **Reasonable reconstruction:** modular scaffold access, loading rails, repair yard,
  specialized anchor groupings, and organized inspection areas.
- **Speculative/experimental:** roller/pulley frames and any interpretation of them as
  lifting or rope-redirection machinery.

## Mesh reuse and rendering constraints

No primitive generator or GPU resource type was added. Every new item is an instance of
the already uploaded plane, cube, cylinder, or sphere mesh. There is no VAO/VBO/EBO per
rig, ladder, boat, worker, or prop. Filled rendering remains indexed
`glDrawElements(GL_TRIANGLES, ...)`, with CCW front faces, back-face culling, positive
scales, inverse-transpose normal matrices, and the existing model-view-projection to
clip-space/NDC pipeline.

## Validation and performance

`--validate-enrichment` verifies finite descriptor values, positive/nonzero dimensions,
rig wheel references, ladder rung counts/spacing, valid boats, deterministic data,
hero-route clearance, and a 420-draw safety budget. Runtime `addObject()` validates every
actual model transform and the builder enforces exactly 327 new static primitive
instances.

The pass adds 327 static draws and four 17-part support workers: 395 maximum draws in
total. Maximum scene draws rise from about 9,068 to 9,463 (about 4.4%). Mesh uploads do
not increase.

## Deliberately deferred

Phase 7 remains untouched. There is no moving sun, shadow mapping, lighting overhaul,
water animation, boat movement, pulley rotation, rope physics, imported geometry,
particle system, or new construction sequence.
