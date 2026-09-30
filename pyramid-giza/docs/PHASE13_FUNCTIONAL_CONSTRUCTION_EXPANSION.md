# Phase 13 — Realistic Functional Construction Site Expansion

## Overview

Phase 13 elevates the Giza construction site graphics application from a static visual composition into an integrated, deterministic, functional construction simulation. The entire pipeline connecting rock extraction at the quarry to the placement and settling of finished megalithic blocks on the pyramid frontier is physically and logically connected.

The expansion strictly maintains all previous OpenGL 3.3 Core and C++17 constraints:
- Reusable indexed triangle geometry (`glDrawElements` and `glDrawElementsInstanced`).
- Zero external model loaders (`.obj`, `.fbx`, `.gltf`); all structures and monuments are procedurally generated.
- Back-face culling (`GL_CCW`, `GL_BACK`) and counter-clockwise vertex winding across all procedural meshes.
- Deterministic simulation models with analytical/cellular approximations rather than heavy brute-force physics libraries.

---

## 1. Functional Quarry Resource Pipeline

The quarry is transformed from a static depression into an active extraction environment (`QuarrySystem`).

### 1.1 Resource Deposit Model
Quarry limestone deposits are represented with deterministic states:
- `Natural`: Unquarried limestone bedrock stratum.
- `Marked`: Grid-scribed extraction channel demarcated by site surveyors.
- `Cutting`: Quarrymen actively hammering bronze chisels and driving wooden swelling wedges.
- `Detached`: Bedrock separation event occurred; block severed from floor with visual crack and vertical offset.
- `Shaped`: Masons dressed raw rough block into dimensioned, square-dressed limestone.
- `Staged`: Rolled along timber rollers to the primary quarry staging area (`{-108.0f, -6.2f, -5.0f}`).
- `Transported`: Block hitched onto heavy wooden sledge and dispatched into the logistics route.
- `Depleted`: Quarry bay exhausted.

### 1.2 Extraction Visuals & Activity
- **Channel Seam Widening:** During the `Cutting` state, trench seams around the perimeter of the block widen from 0.04m to 0.22m.
- **Mallet Vibration:** High-frequency sinusoidal oscillation is applied to stones under active chiseling.
- **Dust Emissions:** Intermittent localized particulate bursts trigger at the chisel impact point.
- **Bedrock Separation:** Upon reaching 100% extraction progress, a clear physical displacement of 0.22m elevates the detached megalith.
- **5 Procedural Rock Profiles:**
  1. `SmallAngularLimestone`: Broken rubble chips and chinking stones.
  2. `MediumRoughLimestone`: Natural irregular deposit strata.
  3. `LargeQuarryStone`: Heavy undressed extracted megalith.
  4. `PartiallyShapedBlock`: Semi-dressed stone showing chisel facets.
  5. `FinishedConstructionBlock`: Precision-dressed 2.6m x 1.35m x 2.45m casing block.

---

## 2. Connected Construction Logistics State Machine

`ConstructionLogistics` manages the deterministic end-to-end transport lifecycle across 12 sequential stages:

```text
QUARRY_READY
     ↓
EXTRACTING (quarrymen cut channels and drive wedges)
     ↓
STAGED (dressed block rests at quarry staging yard)
     ↓
SLEDGE_LOADING (levers hoist stone onto heavy wooden sledge)
     ↓
HAULING (gang of workers haul sledge across ground road)
     ↓
RAMP_APPROACH (sledge aligns with base of main monumental ramp)
     ↓
RAMP_ASCENT (workers haul sledge up the inclined ramp incline)
     ↓
LIFT_PREP (sledge docks at gantry lifting platform)
     ↓
LIFTING (pulley hoist cables elevate megalith to upper tier)
     ↓
UPPER_STAGING (rollers position block on upper scaffold platform)
     ↓
PLACEMENT (masons align and lever block into construction frontier)
     ↓
SETTLED (block stably locks into place; advances pyramid timeline)
```

When a transported block transitions into `SETTLED`, it physically increments the construction progress of the pyramid, tightly coupling the logistics simulation to the authoritative `ConstructionTimelineController`.

---

## 3. Sand Physics & Repose Simulation

`SandSimulation` implements a deterministic 36x36 height-field cellular automaton over a 300m x 300m expanse:

- **Wind Transport:** Sand flux migrates downwind according to prevailing desert wind direction and speed.
- **Obstacle Accumulation:** Windward and leeward sides of structures (pyramid foundation, monumental walls, Sphinx plinth) accumulate sandbanks and natural desert drifts.
- **Angle-of-Repose Relaxation:** When the height differential between neighboring cells exceeds `ReposeThreshold` (0.35m), excess sand slides down the gradient into lower cells with deterministic mass conservation (tested within $\Delta < 0.05$ volume drift).
- **Traffic Sweeping:** Active sledge hauling corridors are dynamically cleared of deep sand drifts, reflecting regular site maintenance.

---

## 4. Nile River Flow & Buoyancy Dynamics

`WaterSimulation` provides realistic aquatic dynamics for the Nile floodplain:
- **Directional Flow Field:** Steady southward river flow at 0.85 m/s.
- **Multi-Wave Analytical Surface:** Superposition of trochoidal wave harmonics computing height displacement and surface normals.
- **Buoyant Boat Motion:** Egyptian transport barges dynamically pitch, roll, and bob vertically based on local wave heights and currents.
- **Hydrodynamic Disturbance:** Stern wake ripples and shoreline foam bands visually articulate boat presence and riverbank interaction.

---

## 5. Procedural Anatomical Sphinx Monument

The secondary placeholder Sphinx is replaced with a 38-component procedural monument (`SphinxMonument`), grounded at `{92.0f, 0.0f, -105.0f}` facing west towards the Nile:

1. **Stone Plinth & Foundation:** Stepped megalithic limestone base.
2. **Recumbent Lion Body:** Powerful torso, haunches, rounded flanks, and tail curled around the right flank.
3. **Forelegs & Paws:** Extended muscular forelegs terminating in individual segmented paws.
4. **Broad Chest & Shoulders:** Sturdy transition anchoring the feline body to the regal torso.
5. **Humanoid Head & Portrait:** Distinctive pharaonic facial silhouette with brow ridge, nose, cheekbones, lips, ears, and ceremonial beard.
6. **Nemes Headdress:** Striped headdress with flared royal wings, chest lappets, and crown plateau.
7. **Uraeus:** Coiled rearing cobra emblem centered on the forehead.
8. **Aeolian Sand Accumulation:** Wind-deposited sand drifts cradling the lower plinth and paws.

---

## 6. OpenGL 3.3 In-Window Simulation HUD

`SimulationHUD` provides an integrated, non-intrusive heads-up display rendered entirely within OpenGL 3.3 Core:
- **Procedural 5x7 Font Atlas:** Complete printable ASCII character set rendered via a generated font texture.
- **Translucent Overlays:** Dark-tinted alpha-blended backdrops.
- **Status Dashboard:** Shows overall pyramid completion percentage, active construction phase, quarry deposit ID and state, logistics hauling state, active worker headcount, simulation toggle states, and real-time FPS.
- **Context Cards:** Dedicated cards for active Quarry operations, Logistics routes, Pulley hoist metrics, and Construction frontier levels.
- **Interactive Controls Panel:** Toggleable help window displaying all application keybindings.

---

## 7. Cursor & Window Focus Management

`CursorController` solves window focus and cursor-trapping issues according to strict operating system conventions:
- **Window Boundaries:** Camera mouse-look is strictly active **only** when the cursor is inside the window (`cursorInsideWindow == true`) and the application has operating system focus (`windowFocused == true`).
- **Leaving Viewport:** When the mouse leaves the application window, camera orientation updates immediately cease and the cursor reverts to OS mode.
- **Focus Lost:** When switching to another application (Alt+Tab), cursor capture is immediately released.
- **Clean Escape Sequence:** Pressing `ESC` while in camera-look mode releases cursor capture, restoring free operating system mouse movement. Pressing `ESC` while already released cleanly exits the application. Clicking into the application window re-engages camera look.

---

## 8. 15-Shot Cinematic Showcase

`ShowcaseController` is extended to 15 synchronized presentation shots totaling exactly 101.0 seconds:
1. `OpeningOverview` (8.0s) — Monumental site scale, ramp, and desert context.
2. `QuarryDescent` (7.0s) — Deep trench terraces and bedrock strata.
3. `RockExtraction` (6.0s) — Quarrymen chiseling seams and driving wedges.
4. `RockShaping` (6.0s) — Masons dressing raw stones into rectangular blocks.
5. `SledgeLoading` (6.0s) — Megalith hoisted and secured to timber sledge.
6. `WorkersHauling` (7.0s) — Gang of haulers pulling across quarry road.
7. `RampAscent` (7.0s) — Ascending the monumental western ramp.
8. `PulleyLift` (7.0s) — Hoisting rig lifting megalith to upper staging tier.
9. `ConstructionFrontier` (7.0s) — Masons maneuvering block on the active level.
10. `BlockPlacement` (6.0s) — Final levering and settling of block into masonry.
11. `PyramidOverview` (7.0s) — Accelerated timeline view of growing structure.
12. `NileFloodplain` (7.0s) — Water flow, buoyant cargo boats, and river landing.
13. `SphinxMonument` (7.0s) — Detailed view of the procedural lion monument.
14. `SandAndWind` (7.0s) — Desert dunes, wind transport, and dust plumes.
15. `FinalSunsetOverview` (6.0s) — Monumental golden-hour site panorama.

---

## 9. Verification & Validation Summary

All 41 CTest regression test suites pass with 100% success:

| Test # | Suite Name | Status |
|:---:|:---|:---:|
| 1–34 | Legacy Phases 1 through 12.8.1 (Geometry, Layout, Lighting, Shadows, Ramps, Pulley, Interior, Facade) | **PASS** |
| 35 | `Phase13QuarryValidation` | **PASS** |
| 36 | `Phase13LogisticsValidation` | **PASS** |
| 37 | `Phase13SandValidation` | **PASS** |
| 38 | `Phase13WaterValidation` | **PASS** |
| 39 | `Phase13SphinxValidation` | **PASS** |
| 40 | `Phase13InputValidation` | **PASS** |
| 41 | `Phase13SimulationHUDValidation` | **PASS** |

### Runtime Benchmark (RTX 3060, OpenGL 3.3 Core):
- **Triangles per frame:** 426,372
- **Draw calls:** 1,918 visible draws / 1,949 shadow depth draws
- **Instance batches:** 9,217 instances
- **Particles:** 512 capacity, single instanced indexed draw
- **CPU collection & submission:** ~32–34 ms
