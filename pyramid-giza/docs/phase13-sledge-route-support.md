# Quarry pulley to sledge handoff

Branch: `repair/phase13-authoritative-construction`
Starting checkpoint: `9e564ded367a613200d125161c001a18998d763d`

The authoritative stone now moves from quarry-floor staging to the quarry pulley before any long haul. The pulley keeps its floor pickup at `(-120.5,-6.65,-10)`, raises the same block to `(-120.5,-0.5,-10)`, and guides it to the receiving deck at `(-111,-1.8,-10)`. Two short wooden loading skids rise from the deck to the sledge bed. The sledge waits on the receiving deck at `(-108.95,-2.635,-10)` and the block finishes at its existing cargo socket `(-108.95,-1.275,-9.98)`.

The physical lifecycle pauses at `SledgeLoading` progress 1.0. It cannot enter `Hauling` or the pyramid-ramp states until the next task builds the supported quarry-exit and one-way desert route. The previous 388.188 m outward-and-return route is retired. The current `Hauling` waypoint data is unreachable placeholder geometry and still starts at the old floor position; it must be replaced before hauling is enabled. The old post-lift placement path is also unreachable and remains for a later task.

Task 10B's support-derived runner placement, visible surface priority, and exposed-desert mobility corrections are retained. The old route audit measured 127.959 m of exposed sand on its outbound corridor, but that distance is historical and is not a claim about a completed one-way haul.

Focused 0.02 s lifecycle validation: block ID 1000, maximum frame displacement 0.1425 m against a 0.1725 m speed bound, one matching simulation payload, supported receiving deck and loading skids, cargo socket error 0.0000 m, no pyramid travel before the lift, and pyramid occupancy unchanged. The full 41-test CTest suite passed. Runtime frames were inspected from two quarry camera angles through the lift and loading stages.
