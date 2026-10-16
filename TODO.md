# TODO

## PLAN — tracks, laps, fences, map editor, admin/creative mode (2026-10-16)

Asked for together; planned as phases so each one ships working before
the next starts. Phase 1 fixes what's broken now; every later phase
builds on the one before it.

### Phase 1 — fix laps (bug fix, do first) — DONE

- [x] Replaced halfway-arming with **8 ordered checkpoints** along the
      centreline (index 0 = finish). A lap only counts if all 7 others
      were crossed in sequence since the last finish.
- [x] Teleport / respawn **invalidate the current lap**. Detected
      generically (a road-position jump bigger than one checkpoint's
      spacing in a single frame, not a hook into every respawn call site)
      so it catches minimap teleport, Backspace respawn, and the internal
      auto-respawn-when-stuck alike. Shows "INVALID" in red on the HUD;
      no time/best/ghost recorded for that lap.
- [x] **Visible start/finish gate** (striped pillars + banner) and small
      flag markers at each checkpoint, computed live from
      `road::Centreline()`, no new assets.
- [x] HUD: "Checkpoint N/7" readout while a lap is in progress.

### Phase 2 — multiple tracks

- [ ] A `Track` data type: name, control points, road width, fence
      layout, start point. Replace the single hardcoded `CONTROL[]` in
      `Road.cpp` with a list of tracks.
- [ ] Built-in tracks: **Small** (short tight loop), **Long** (current
      circuit), **Longest** (big sweeping loop using the open world).
- [ ] Track picker in the PLAY / mode-select screen (same pattern as the
      car picker). Best lap + ghost saved **per track per car**.
- [ ] Everything that reads the circuit (road mesh, fences, minimap, AI
      traffic, auto-drive, prop clearance) already goes through
      `road::Centreline()` — it just needs to rebuild on track change.

### Phase 3 — fences with gaps

- [ ] Re-enable fence collision (undoing the "no fence" change), but
      fences become **segments with gaps**: each track defines openings
      (start/end along the centreline) where you can leave the road into
      the open world.
- [ ] Gaps drawn visibly (posts end, maybe a gate/sign) so they read as
      intentional exits, not missing geometry.

### Phase 4 — custom maps + `.map` files

- [ ] A `.map` file format: plain text, name-keyed lines (same robust style
      as the fixed `settings.cfg`, never positional), versioned header.
      Holds: track control points, fence segments + gaps, placed props
      (trees/rocks), placed cars (AI/parked), mountains/hills, map border
      size, spawn point, world style.
- [ ] Save/load: maps live in `Documents/3D Car Game/Maps/` by default;
      **autosave** every N seconds while editing to `<name>.autosave.map`.
- [ ] Main menu: **PLAY → Custom Map** lists `.map` files in that folder.
- [ ] **Open a `.map` by double-clicking it**: the game accepts a file
      path as a command-line argument (`CarGame3D.exe "path.map"`) and
      jumps straight to car select → drive on that map. Windows file
      association (registry entry for `.map` → the exe) set up by an
      optional "Associate .map files" button in Settings, since it writes
      to the user's registry and should be opt-in, not done silently.
      (Note: `.map` is a generic extension other software also uses —
      worth considering `.cgmap` to avoid hijacking it.)

### Phase 5 — creative mode (map editor) + admin mode

- [ ] **Creative mode**: free-fly editor camera, placement tools:
      road control points (drag to reshape the track live), fence
      segments + gaps, trees/rocks, parked/AI cars, raise/lower terrain
      brush for hills/mountains, map border size. Undo, save, test-drive.
- [ ] **Admin mode** (toggle in Settings, or launched with `--admin`):
      enables the cheats that shouldn't be in normal play.
- [ ] **Minimap teleport moves to admin/creative only** — in normal play
      it's disabled (it's also what breaks laps today).
- [ ] Terrain becomes data-driven (base noise + per-map edits) instead of
      only the hardcoded `HeightAt()` styles — needed for the terrain brush.

### Order and why

1 → 2 → 3 → 4 → 5. Laps must be trustworthy before there are several
tracks to set records on; tracks must be data (Phase 2) before a file
format can save them (Phase 4); and the editor (Phase 5) is just a UI that
writes that same data. Phase 5 is the biggest by far (an editor is its own
application inside the game) and should be split further when we get there.

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
- [x] **Terrain resolution doubled** (RES 200 -> 400), tiled into a 4x4
      grid since raylib's `Mesh::indices` is a hardcoded `unsigned short*`
      (a real hard cap, not something a "32-bit index switch" in
      `MeshBuilder` could fix -- that was a mistaken premise in the old
      version of this doc; raylib itself would need forking).
- [ ] **Denser station counts, wheel segments, prop density** (car bodies,
      fences, scenery) -- same 65535-vertex ceiling applies to any of
      these if they ever grow that large; tile or split the same way the
      terrain was if one does.
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
- [x] Garage cars now cached per-roster-slot instead of reloading on
      every click -- also fixed a second real cost: colour-only changes
      were triggering a full reload even though the paint tint was never
      baked into geometry. Both fixed with `Car::SetPaint` + a
      `vector<unique_ptr<Car>>` cache in `main.cpp`.
- [x] AI traffic now fires nitro (1.5-3s burst, 1.5x cruise speed, random
      5-14s cooldown per car) — it previously had no boost mechanic at all.

## Open — world/gameplay, still owed

- [x] **Water, nature, fence.** Water got a real scrolling ripple-texture
      layer (downloaded CC BY 4.0 textures) on top of the existing
      procedural wave shader. Nature got 4 real oak tree models (same CC
      BY 4.0 source, decimated, ~12% of tree spawns). Fence: collision
      removed per explicit request ("open world everywhere no fence") --
      still drawn, just doesn't block the car anymore.
- [x] **Open world.** WORLD_SIZE 1600 -> 4000 (~6.25x area), mountain ring
      pushed out to match, terrain tile grid and prop count scaled with
      it. The circuit itself is unchanged. This is the bounded, safe part
      of "chunk loading like Minecraft" -- built upfront at load time,
      not dynamically streamed.
- [ ] **True chunk streaming.** A genuinely bigger/unbounded world,
      generated and loaded in chunks around the player as they drive and
      unloaded behind them, the way the user actually asked for. A much
      larger, separate undertaking: a chunk coordinate system, lazy
      per-chunk terrain/prop generation (not the current build-everything-
      at-Load() approach), amortized loading spread across frames so
      crossing a chunk boundary doesn't hitch, and a decision about what
      happens to the fixed circuit/lap-timer gameplay in a world that's no
      longer a bounded, fully-loaded map. Deliberately not attempted
      blind in one pass -- deserves its own dedicated session.
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
- [x] **Frustum (cone) culling for props**, on top of the existing distance
      cull. `World::Draw` now skips anything outside a cone derived from
      the camera's fovy/aspect (+18deg margin), unless it's within 18
      units regardless of angle.

## Open — car visuals (procedural path only — not relevant while every car
## uses a real kit/licensed mesh, kept in case the procedural loft is ever
## brought back)

- [ ] No real boot/decklid step, rear screen not colour-distinct from the
      bodywork, wheels sit flush rather than tucked under a flared arch.
      All in `CarBody.cpp`'s loft, currently unused by any roster car.

## Open — technical

- [x] The 65535-vertex cap is permanent (raylib's own `Mesh::indices` is a
      hardcoded `unsigned short*`) -- not fixable by a "32-bit switch" in
      `MeshBuilder`. Terrain tiled to work within it (see above); anything
      else that grows past it needs the same treatment.
- [x] Garage car-load caching (see above).
- [x] Git repository initialized, pushed to GitHub (private,
      `syedammarali294-hub/3D-car-game`).

## Open — audio

- [x] Full engine audio (RPM-following pitch, gear-shift clunk), tyre
      squeal, collision thump, ambient wind — all done in earlier sessions.
- [x] Water splash on entering the lake -- sound (synthesised, speed-
      scaled) and a matching visual (14 gravity-faded droplet particles),
      both edge-detected so they fire once per entry.

## Done (earlier sessions, high level — see git log for detail)

Gearbox (auto/manual, per-class gear counts, rev limiter), tabbed Settings,
persisted options, lap timer + minimap + ghost replay, Traffic Mode AI,
auto-drive, drift visual slip + tyre marks, per-car upgrade system (Engine/
Handling/Nitro), How To Play (Settings tab), 13 procedural cars + 3
licensed cars, fullscreen letterboxing (no black-flash, UI-correct at any
window size), persistent per-car odometer, animated UI, screen-fade
transitions, mountain/lake boundaries, respawn-to-nearest-road, auto-respawn
when stuck, first-drive hint panel.
