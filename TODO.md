# TODO

Status as of 2026-10-15. Rewritten to reflect what's actually true of the
code right now — the previous version had accumulated a lot of stale
"open" items that were already fixed in later sessions.

**Target hardware: raised.** The user explicitly approved dropping the
Intel HD 4000 low-end constraint in favor of implementing the full
rendering wishlist below (shadows, SSAO, reflections, post-processing).
This is a real tradeoff: frame rate on weak integrated GPUs is no longer
the design priority.

## Open — rendering (the big approved-but-not-yet-done items)

These are genuinely substantial, each worth its own focused session with
actual visual verification rather than being rushed blind — shader bugs in
particular are easy to ship broken and hard to catch without seeing a
frame. Not started yet:

- [ ] **Real directional shadows.** A shadow map from the sun, sampled in
      `scene.fs`. The single biggest remaining reason the scene reads as
      flat/toylike.
- [ ] **Screen-space ambient occlusion**, or at minimum baked contact
      darkening beyond the single contact-shadow blob under the car (done
      this session, see below) — props-to-ground, wall corners, etc.
- [ ] **Environment reflections on the car.** A small sky cubemap, cheaply
      refreshed.
- [ ] **Post-processing stack**: tonemapping, bloom, vignette. Needs an
      HDR render target first.
- [ ] **Denser geometry budget** (station counts, wheel segments, terrain
      resolution, prop density) now that the low-end constraint is lifted.
      Needs the 32-bit index switch (`MeshBuilder` is 16-bit, see below)
      if any single batch would exceed 65535 vertices.
- [ ] **Anisotropic/mipped road texture** instead of flat vertex colour.
- [ ] **Quality preset (Low/High) in Settings** so a future low-end pass,
      if ever wanted again, has a real toggle instead of this being an
      all-or-nothing codebase decision.

## Done this session — rendering (the safe, bounded part)

- [x] **Contact shadow under the car.** Soft alpha-gradient triangle fan
      (immediate-mode rlgl, same technique `SkidMarks.cpp` uses), drawn
      before the body. Player car drapes it to the real ground normal; AI
      traffic uses a flat approximation (road is near-flat under them).
- [x] **Smooth normals** — turned out to already exist
      (`MeshBuilder::SmoothNormals`), just wired only to the now-unused
      procedural car-body path (`CarBody.cpp`). Not applicable to anything
      currently rendered (all cars use real kit/licensed meshes; terrain
      computes its own smooth per-vertex normals analytically already).
- [x] **Specular + rim lighting** — also already existed in `scene.fs`
      (Blinn-Phong highlight + a rim/fresnel term). The old TODO claiming
      these were missing was stale.

## Open — licensed car models (Ultra Graphics)

- [x] Fixed: the 3 downloaded models (BMW M3 E30, M3 Competition, S1000RR)
      rendered as shattered/fragmented shards. Root cause: 58-144 separate
      mesh pieces from the source file merged with zero shared vertices,
      then decimated across multiple compounding passes. Fixed by welding
      near-coincident vertices before decimating, and decimating in exactly
      one pass. Verified via boundary-edge ratio (fraction of edges
      touching only one triangle — near 0% is healthy): all three now
      under 5%, down from 24-35% shattered.
- [x] Fixed: crash when selecting any of the 3 models in the Garage —
      raylib's `LoadOBJ` indexes `texcoords[]` with an unchecked `-1` for
      any vertex with no UV. Fixed with placeholder UVs.
- [x] Trimmed M3 Competition further (79k -> 63.5k faces) — it was nearly
      2x the other two and caused a felt stutter switching to it in the
      Garage (`Car::Load` has no cache, so every switch is a real
      synchronous parse + GPU upload).
- [ ] Still not cached — switching between heavy cars in the Garage will
      always have *some* hitch. A small LRU cache of loaded `Car` objects
      would fix this properly if it's still bothering anyone.
- [x] AI traffic now fires nitro (1.5-3s burst, 1.5x cruise speed, random
      5-14s cooldown per car) — it previously had no boost mechanic at all.

## Open — world/gameplay, still owed

- [ ] **Water, nature, fence improvements.** Explicitly asked for, not
      started. No specifics given yet beyond "make fence more advanced" —
      needs a concrete ask (what should water/nature/fences actually do
      differently?) before this can be scoped, let alone built.
- [ ] **A second circuit / track selection.** Everything reads the circuit
      through `road::Centreline()`/`road::StartPoint()` — a second
      `CONTROL` point set + a track picker (same UI pattern as the car
      picker) is mostly data, not new systems.
- [ ] **Traffic cones / a slalom or time-trial mode**, reusing existing
      prop + collision systems.
- [ ] **Desert flora.** Needs an actual cactus/dead-scrub asset — the
      owned Kenney Nature Kit only has temperate trees.
- [ ] **LOD chains on props** (2 levels + a billboard) — ~270 props are
      drawn at full detail with only hard distance culling right now.
- [ ] **Frustum culling for props**, on top of (or instead of) LOD.

## Open — car visuals (procedural path only — not relevant while every car
## uses a real kit/licensed mesh, kept in case the procedural loft is ever
## brought back)

- [ ] No real boot/decklid step, rear screen not colour-distinct from the
      bodywork, wheels sit flush rather than tucked under a flared arch.
      All in `CarBody.cpp`'s loft, currently unused by any roster car.

## Open — technical

- [ ] `MeshBuilder` uses 16-bit indices (65535 vertex cap per batch).
      Needed before "denser geometry" above.
- [ ] Garage car-load caching (see above).
- [x] Git repository initialized, pushed to GitHub (private,
      `syedammarali294-hub/3D-car-game`).

## Open — audio

- [x] Full engine audio (RPM-following pitch, gear-shift clunk), tyre
      squeal, collision thump, ambient wind — all done in earlier sessions.
- [ ] Water splash on entering the lake — still just the "IN WATER" label
      and speed drag, no sound or visual.

## Done (earlier sessions, high level — see git log for detail)

Gearbox (auto/manual, per-class gear counts, rev limiter), tabbed Settings,
persisted options, lap timer + minimap + ghost replay, Traffic Mode AI,
auto-drive, drift visual slip + tyre marks, per-car upgrade system (Engine/
Handling/Nitro), How To Play (Settings tab), 13 procedural cars + 3
licensed cars, fullscreen letterboxing (no black-flash, UI-correct at any
window size), persistent per-car odometer, animated UI, screen-fade
transitions, mountain/lake boundaries, respawn-to-nearest-road, auto-respawn
when stuck, first-drive hint panel.
