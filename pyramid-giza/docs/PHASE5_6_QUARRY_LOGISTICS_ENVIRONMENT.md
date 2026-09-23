# Phase 5.6 - Quarry, Logistics, and Environmental Context

## Objective and site organization

Phase 5.6 turns the monumental construction scene into a readable ancient industrial
landscape. The pyramid remains dominant. A separated open-cut quarry is the secondary
focus, while repositories and a haul corridor show material flow. The distant Nile,
floodplain, trees, camp, and stylized Sphinx-context landmark provide background depth.

```text
BEDROCK -> CHANNELS -> LEVERING -> QUARRY STAGING -> EXIT RAMP
        -> ROUGH DEPOT -> DRESSING -> FINISHED DEPOT -> LOADING
        -> HAUL ROAD -> PYRAMID STAGING -> MAIN RAMP -> PLACEMENT
```

## Quarry redesign

The quarry center moved to `(-128, 0, -15)`, 130.82 units from the pyramid center. It is
a 64 by 72 unit open cut, 7.5 units deep. A full recessed pit base supports a narrower
42 by 34 active floor. Stepped wall and side volumes form three major working elevations:
upper (-1.5), middle (-3.7), and lower (-5.7). Varied widths and insets avoid a single
box-shaped cavity. Thirty small deterministic stones form three spoil groups.

Four bays make extraction readable:

- Bay A embeds an untouched marked block in bedrock.
- Bay B surrounds an attached block with four separated rock strips; the gaps are channels.
- Bay C tilts a mostly separated block beside a fulcrum and long timber lever.
- Bay D leaves an empty bordered cavity where a block has been removed.

This composition uses shared cubes and cylinders instead of CSG. Quarry workers, mallets,
the lever, rough floor staging, a wooden work platform, and a quarry sledge define the dig
zone. The substantial 7.5-unit-wide exit ramp connects the lower floor to the plateau.
Fifteen paired cube steps provide side access, and restrained cylinder posts suggest rope
guides. All transforms are deterministic.

## Repositories, dressing, and loading

Three generated repositories contain 126 visible stones: 48 rough, 18 partially dressed,
and 60 finished. Rough blocks receive small deterministic yaw variations. The partial yard
has four low cutting beds with paired timber supports. Finished blocks form an ordered
two-level grid. A raised loading platform, waiting blocks, posts, a lever beam, workers,
and deliberately positioned sledges join this sequence to the broad haul road.

Six sledges have clear roles: hero animated load, quarry-floor carrier, rough-depot carrier,
loaded background haul-road carrier, loading-station carrier, and returning empty carrier.
The static logistics chain is long, but the original hero animation intentionally represents
one focused transport stage; it does not spend a minute crossing the entire world.

## Functional pyramid access

The original animated ramp path is unchanged. Its infrastructure now includes retaining
edges, transverse rollers, timber supports, guide posts, twenty side-access steps, a broad
intermediate rest platform, upper waiting stones, lever props, and platforms connecting the
ramp to 22 scaffold modules. These additions leave the moving sledge corridor clear.

## Rope-redirection and lifting demonstration

A wooden four-post frame near the quarry exit uses a top beam, two cylinder wheels/rollers,
two endpoint-aligned cylinder ropes, and one suspended cube stone. Geometry is static and
all pieces reuse existing meshes; no rope or force physics is implied.

Experimental rope-redirection / lifting rig included as a construction-mechanism
visualization. It is not presented as a definitively proven reconstruction of Khufu-era
lifting technology.

## Nile, vegetation, camp, and Sphinx context

The Nile is a shared plane scaled to 330 by 26 at z=-166. A 340 by 20 muted green-brown
floodplain strip provides transition without changing the arid plateau. Water deliberately
has no waves, reflection, refraction, or special shader.

Eighteen deterministic primitive trees use one shared cylinder trunk and five flattened
cube leaves each. Thirteen sit along the floodplain and five identify the camp; none crowd
the active quarry or pyramid ramp. The camp retains two shelters, timber storage and tools,
and adds simple cylinder jars and cube crates. A small inspection shelter sits by the
finished-stone organization area.

The seven-part Sphinx-context landmark at `(92, 0, -105)` uses cubes and one sphere for the
body, chest, head, paws, and headdress. It is deliberately small, low, distant, and visible
mainly from context preset 9. It is a broader Giza contextual landmark / site-reference
zone, not a claim that the finished Great Sphinx belongs to the same exact construction
moment represented by the main scene.

## Coordinates, transforms, and graphics constraints

The expanded world is 360 by 300 units. These are world coordinates, not CVV limits:

```text
canonical primitive -> model/world -> view -> clip -> perspective divide -> NDC/CVV
```

All static content is transform-driven. Only four mesh objects are uploaded: plane, cube,
cylinder, and sphere. Quarry walls, stones, roads, trees, rig parts, and Sphinx parts do not
own GPU buffers. Filled rendering remains `glDrawElements(GL_TRIANGLES, ...)` using shared
VAO/VBO/EBO resources. CCW is front-facing, back faces are culled, all scales are positive,
and inverse-transpose normal matrices protect lighting under non-uniform scaling. No imported
model, legacy immediate mode, or hidden geometry generator is used.

## Animation, performance, and controls

The 28.5-second Phase 5 state sequence, pullers, loaded sledge, dynamic two-segment rope,
mallet attachment, ramp traversal, lever pivot, and placement lift are preserved. Secondary
workers use the inexpensive hierarchical pose preview only. Static geometry is built once;
meshes and buffers are never regenerated per frame.

Current major counts are 7,561 pyramid blocks, 74 quarry objects, 126 repository stones,
35 workers, 6 sledges, 18 trees, 22 scaffold modules, 1 lifting rig, 7 Sphinx parts, and
approximately 9,068 maximum draw calls. Presets 1-9 cover pyramid, quarry, extraction,
logistics, upper work, wide site, Nile, and Sphinx views. Camera movement is 32 units/second;
the perspective range is 0.7 to 550 units.

## Validation and Phase 6 boundary

`--validate-industrial` checks depth, ordered terraces, finite bays, ramp dimensions,
repository count/determinism, logistics ordering, distant Nile, sparse safe tree placement,
and lifting-rig dimensions. Existing geometry, scene, composite, hierarchy, animation, and
monumental-site tests remain active. Wireframe, culling-on/off, and OpenGL smoke modes remain
available.

Phase 6 work is deliberately absent: no shadow mapping, moving sun, PBR, advanced water,
particles, physics, IK, pathfinding, cinematic camera, or final UI has been introduced.
