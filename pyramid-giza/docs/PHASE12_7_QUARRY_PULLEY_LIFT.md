# Phase 12.7 - Quarry Pulley Rock-Lifting System

## Objective

Phase 12.7 adds one readable quarry-floor-to-platform stone lift without
changing the established renderer or the 28.5-second hero transport sequence.
It turns the earlier static rope-redirection context into a deterministic
graphics demonstration with a supported frame, moving load, trolley, ropes,
and wheel.

> Historical status: this mechanism is a speculative / experimental
> rope-redirection and lifting visualization for graphics demonstration. It is
> not archaeological proof of Khufu-era pulley machinery.

## Placement and support

The rig stands inside the quarry near x=-123.5..-108 and z=-13.2..-6.8.
Four gantry posts terminate at the quarry floor y=-7.45. Diagonal braces join
the bases to the upper frame. The receiving deck is not floating: four posts
and four cross braces carry it from the same quarry floor to a top surface at
y=-2.60. The stone begins with its bottom on the floor and finishes with its
bottom exactly on this deck.

## Animation architecture

Dedicated CPU class:

    QuarryPulleyAnimationController
        -> fixed 14-second clock
        -> QuarryPulleySnapshot
        -> scene model transforms
        -> shared cube/cylinder meshes
        -> frameObjects_
        -> visible pass and shadow pass

The state sequence is:

    Idle -> Attach -> Tension -> Lift -> UpperHold
         -> GuideToPlatform -> Lower -> Release -> Complete

State durations and exact motion ranges are recorded in
`quarry_pulley_animation.csv`. Smoothstep interpolation is deterministic.
The clock is independent of the hero animation, construction timelapse, sun,
effects, and showcase controller. Construction progress up to 85 percent
permits motion; at the completed 100-percent site the rig remains visible in
its idle configuration.

## Rope, trolley, and wheel

The lift rope is regenerated each frame as a thin cylinder between the pulley
point and the current hook/load attachment point. A two-leg sling appears only
while attached. The carriage and stone use canonical primitive geometry and
model transforms; no GPU buffers are created per component.

Wheel angle follows:

    angleRadians = signedTravel / wheelRadius

Signed travel combines tension take-up, vertical lift, trolley guide travel,
and reverse travel during lowering. Thus the wheel turns only as the
rope/trolley system moves and reverses consistently while lowering.

## Support-state contract

- Idle, Attach, Tension: quarry-floor-supported load.
- Lift, UpperHold, GuideToPlatform, Lower: intentionally suspended load.
- Release, Complete: destination-platform-supported load.

Shape scale is never inherited by another component. Every dynamic transform
is assembled independently from one snapshot, and all transforms enter the
same `frameObjects_` list used by both lit and depth passes.

## Controls and showcase

- `F7`: pause/resume only the quarry pulley clock.
- `R`: reset both the hero animation and pulley to Idle.

Phase 12 quarry shots seek this clock from the presentation time: the quarry
overview begins at Idle, extraction detail reaches the lifting motion, and
later quarry coverage reaches lowering/release. Direct showcase seek therefore
produces the same pulley state every time.

## Validation

`--validate-quarry-pulley` checks state ordering, finite snapshots, rope
length, wheel coupling, 30/60 Hz equivalence, reset, and two-cycle loop
stability. `--validate-quarry-pulley-support` checks floor/platform contact,
conditional support categories, grounding, and conservative post/load
clearance. CTest runs both checks.

## Preserved course constraints

All new visible parts reuse the existing cube and cylinder VAO/VBO/EBO
resources. Filled drawing remains indexed `glDrawElements(GL_TRIANGLES)`;
there is no immediate mode, `glDrawArrays`, imported mesh, rope physics,
per-instance GPU mesh, or new render pass. Model/view/projection and CVV/NDC
behavior are unchanged.

## Deliberately deferred

No historically asserted machinery simulation, worker coupling, rope physics,
collision physics, sound, new lighting, or wider construction choreography is
included. This remains a deterministic educational visualization.
