# Phase 12.6 - Grounding, Support, and Spatial Stability Repair

## Objective

Phase 12.6 is a structural cleanup pass. It does not add a new scene, renderer,
lighting model, or cinematic system. Its purpose is to make every visible load-bearing
object read as physically supported throughout the 0, 25, 50, 75, and 100 percent
construction states.

The visual audit found three recurring causes: elevated ramp slabs with sparse posts,
scaffold roots stored at elevated world coordinates, and temporary stones/props whose
visibility outlived the structure beneath them. Those were repaired at their source
rather than hidden by camera changes.

## Support model

Every relevant object is classified as one of:

- **Ground-supported:** its bottom contacts desert, quarry floor, or a documented
  ground slab.
- **Structure-supported:** its footprint overlaps a visible ramp fill, pyramid course,
  scaffold platform, work deck, sledge, or lifting rig.
- **Intentionally suspended:** a rope, rig-controlled load, or lever-lifted stone whose
  suspension is explicit and documented.

SceneSupport provides reusable CPU queries for stage visibility, transformed object
bottom height, rotated horizontal footprints, terrain surfaces, stepped-ramp support,
vertical gaps, and stage dependencies. Ordinary structure contact uses a -0.08 to
+0.08 world-unit tolerance. The lower quarry ladder permits a 0.10-unit terrain
clearance because of its leaned rail geometry.

## Repaired object families

### Ramps

Every ramp descriptor now generates a 64-segment stepped earth fill from its declared
support surface to the slab underside. The horizontal landing uses one solid fill.
The previous side stairs were removed because they floated beside sloped ramps. Edge
rails, rollers, and timber details remain. The late unsupported upper connector was
removed; fewer correct access structures are preferable to a visually impossible one.

### Scaffolds, ladders, and walkways

All three remaining scaffold groups begin at ground level. Successive modules inherit
support from the platform below without scale contamination. Platform height was
corrected so the next pole tier begins exactly at its top. Ladder feet use ground or
documented quarry/quay surfaces, and access walkways now meet scaffold or ramp surfaces.
The west walkway has four explicit ground posts.

### Upper work zone

The former collection of permanent floating stones, rails, anchors, and lever racks is
now a staged work deck. Eight ground-founded posts and two crossbeams support the deck.
Prepared stones, rails, alignment posts, ropes, and lever storage appear only while the
deck exists (62-90 percent).

### Frontier stones and workers

Animated frontier stones retain horizontal footprint overlap with a stable lower
pyramid course. Their diagnostic lift is limited to 0.04 units; queued and approach
offsets were reduced so the block never loses support. Ground staging stones now use a
dedicated lane with bottom Y=0. Construction-crew roots are evaluated on the active ramp
surface, not beside it in empty space. Unsupported static worker elevations were removed.

## Temporal support dependencies

Visibility uses the same half-open stage rule in rendering and validation. An object may
only be active when its required support is active. The dependency table covers ramp
bodies/fills, scaffold/ground relationships, ladders/scaffolds, the upper deck/posts,
upper props/deck, frontier blocks/lower courses, and explicitly suspended ropes/loads.

## Validation

The following commands are OpenGL-independent:

    build\PyramidGiza.exe --validate-supports
    build\PyramidGiza.exe --validate-grounding
    build\PyramidGiza.exe --validate-stage-dependencies

At 0/25/50/75/100 percent, support validation checks active ramp samples, frontier
blocks, and scaffold bases. Results are 84 checked stage samples total, zero unsupported
samples, and a worst absolute gap of 0.061 units. Grounding validation checks 44 ramp,
ladder, queue, and upper-platform contacts with zero excessive gaps or penetrations.
Dependency validation checks 1,019 active implications across 19 documented
dependencies with zero violations.

Visual validation uses presets 1, 2, and 6 in filled and wireframe modes at the five
construction checkpoints. Wireframe makes the stepped ramp fill, scaffold poles, deck
posts, and block-course overlap directly visible.

## Preserved constraints

Filled geometry remains indexed triangles through glDrawElements(GL_TRIANGLES, ...)
or the existing indexed instanced equivalent. The four shared primitive meshes, VAO /
VBO / EBO ownership, CCW winding, outward normals, inverse-transpose normal matrices,
culling, shadows, textures, particles, model/view/projection, and CVV/NDC pipeline are
unchanged. No imported model, legacy OpenGL path, per-instance GPU mesh, physics engine,
or hidden collision system was added.

## Remaining limitations

Ramp earth fill is represented by small cuboid steps rather than terrain deformation.
Frontier movement is an explanatory rigid-block slide, not a dynamics simulation.
Ropes and lever loads remain deliberately suspended visualizations. Historical access
structures remain plausible graphics reconstructions rather than archaeological claims.
