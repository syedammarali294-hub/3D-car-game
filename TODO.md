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

### Phase 2 — multiple tracks — DONE

- [x] `track::TrackDef`/`TrackId` (`Track.hpp`/`.cpp`): name, description,
      control points, fence gaps. Replaced the single hardcoded `CONTROL[]`
      that used to live directly in `Road.cpp`.
- [x] Built-in tracks: **Small** (short tight loop, clear of the lake),
      **Long** (the original circuit), **Longest** (big sweeping loop in
      the open world).
- [x] Track picker on the mode-select screen (cycles Small → Long →
      Longest → each custom `.cgmap` found in the Maps folder).
- [x] Road mesh, fences, minimap, AI traffic, auto-drive, and prop
      clearance all already read through `road::Centreline()`, so they
      rebuild automatically on track change — no per-system wiring needed.

### Phase 3 — fences with gaps — DONE

- [x] Fence collision re-enabled, but as **segments with deliberate gaps**:
      each `TrackDef` carries `fenceGaps` (start/end fractions along the
      lap) where `Fence::Load`/`ResolveCollisions` skip building/blocking,
      so leaving the circuit only happens where a track intentionally
      allows it.
- [x] Fixed a real visual bug this session: rails between two posts used
      only their endpoints' terrain height with nothing sampled in
      between, so a steep local slope stretched a rail into a thin
      near-vertical sliver pointing into the sky. `Fence::Load` now skips
      connecting a post pair whose heights differ by more than
      `MAX_RAIL_RISE` (2.5 units) — the posts themselves are unaffected,
      only the implausible rail between them is dropped.

### Phase 4 — custom maps + `.cgmap` files — DONE

- [x] `.cgmap` file format (`MapFile.hpp`/`.cpp`): plain text, name-keyed
      lines, versioned header (`cgmap_version`). Holds track control
      points, fence gaps, world style, name/description. (Scope note:
      placed props/AI cars/terrain edits are NOT in the format yet — see
      Phase 5's "not yet" list below, same reasoning as before.)
- [x] Save/load: maps live in `Documents/3D Car Game/Maps/` (falls back to
      a local `maps/` folder next to the exe if that can't be created).
- [x] Mode-select screen's track picker lists every `.cgmap` in that
      folder after the 3 built-ins; an **EXPORT .cgmap** button there also
      saves whichever track is currently selected as a starting point.
- [x] **Open a `.cgmap` by double-clicking it**: `main()` takes the file
      path as `argv[1]` and jumps straight to car select on that map
      (`Menu::StartWithCustomMap`). `FileAssoc.cpp` + an opt-in "Associate
      .cgmap files" button in Settings registers the Windows file
      association (never done silently, since it writes to the registry).

### Phase 5 — map editor + admin/spectator mode

- [x] **Admin mode** (toggle in Settings > Car, off by default): `settings::Options::adminMode`,
      persisted to `options.cfg`. Gates everything below.
- [x] **Minimap teleport, NOS refill (F2), unstick (F3)** all gated to
      admin mode — in normal play none of these are available (teleport in
      particular is also what breaks lap validity, by design — see Phase
      1's checkpoint/teleport detection).
- [x] **Dev console** (backtick removed in favor of `/`, Minecraft-chat
      style — opens pre-filled with `/`): `Console.hpp`/`.cpp`. Supports
      Ctrl+V paste. Commands: `/gamemode spectator`, `/gamemode drive`,
      `/respawn`, `/refillnos`, `/help`. Admin-only; closes itself if
      admin mode is turned off while open. Typing in it no longer leaks
      into gameplay hotkeys (pause, F-keys, minimap click) — `Menu::Tick`
      takes a `suppressHotkeys` flag for exactly this.
- [x] **Spectator mode** (`Spectator.hpp`/`.cpp`): free-fly admin camera,
      reuses the player's own drive-key bindings for horizontal movement,
      Shift/Ctrl for straight up/down, RMB-drag to look. Car freezes in
      place while spectating; engine sound simply isn't updated, which is
      all it takes to go silent (see `Engine.hpp`'s own doc comment).
- [x] **Standalone Map Editor** (`MapEditorMain.cpp`, separate `MapEditor`
      CMake target/exe, own Desktop shortcut) — a top-down 2D editor, not
      the originally-envisioned in-3D free-fly editor: place/drag road
      control points, punch fence gaps by clicking along the loop, cycle
      world style, save/open the exact same `.cgmap` files the game reads.
      `P` toggles a preview overlay of the real Catmull-Rom curve
      (`Road.cpp`'s actual algorithm), not just the straight placement
      lines. This satisfies the "separate app to build maps, like a
      Unity/Unreal-style editor" ask via a 2D tool rather than a 3D one —
      genuinely simpler to get right, and done.
- [ ] **Not done / not in `.cgmap` yet**: placing trees/rocks/AI or parked
      cars per-map, a raise/lower terrain brush, map border size, undo. The
      editor only edits what the file format currently holds (road shape +
      fence gaps + world style) — `.cgmap` would need extending before any
      of this has somewhere to write to, and terrain would need to become
      data-driven (see below) before a brush means anything. Still a real,
      separately-sized piece of work if wanted.
- [ ] Terrain becomes data-driven (base noise + per-map edits) instead of
      only the hardcoded `HeightAt()` styles — needed for the terrain
      brush above. Not started.

### Order and why

1 → 2 → 3 → 4 → 5. Laps must be trustworthy before there are several
tracks to set records on; tracks must be data (Phase 2) before a file
format can save them (Phase 4); and the editor (Phase 5) is just a UI that
writes that same data.

**All 5 phases are done as of 2026-10-16**, Phase 5 delivered as a
standalone 2D Map Editor + admin/spectator mode + dev console rather than
the originally-sketched in-3D free-fly editor -- see Phase 5 above for
exactly what that does and doesn't cover. What's left under it (prop/AI
placement, a terrain brush, undo) is real, scoped, separate future work,
not something quietly skipped.

Status as of 2026-10-17. Rewritten to reflect what's actually true of the
code right now — the previous version had accumulated a lot of stale
"open" items that were already fixed in later sessions.

**Genuinely still open, not quietly skipped** (as of 2026-10-17): real
shadows/SSAO/car reflections/bloom (each its own session, see below --
shader work that needs actual visual verification to ship safely, not
something to rush blind), true chunk streaming, LOD chains on props, and
the Map Editor's prop/terrain-brush extensions (Phase 5 above). Traffic
cones/slalom mode and desert flora are now done (see below). Nothing in
this remaining list has been started.

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
- [x] **Quality preset (Low/High) in Settings > Car — DONE (2026-10-17).**
      `settings::Options::highQuality` (default High, matching the raised
      target). Low scales prop density to 45% (`World::Load`'s new
      `densityScale` param) and draw distance to 65%. Switching it while
      playing reloads just the scenery (not terrain/road/fences -- those
      don't depend on it), the same pattern a World-style change already
      used. The Car settings tab needed its own scroll (see Keybinds tab)
      to fit this and everything else on one screen.

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

- [x] **Drive-through-the-road bug, fixed properly (2026-10-17).** Reported
      as "can drive through the road" on a custom 4-point map. Root cause:
      `Road.cpp`'s curve used *uniform* Catmull-Rom parametrization, which
      loops/self-intersects when control points are spaced unevenly --
      which a freehand custom map almost always is (confirmed: the
      reported map's curve self-intersected with the old formula, verified
      zero self-intersections with the fix). Switched to **centripetal
      Catmull-Rom** (Barry & Goldman, alpha=0.5, knots from real inter-
      point distances), applied identically in `Road.cpp` (the real
      centreline) and `MapEditorMain.cpp` (the editor's preview curve) so
      they match exactly. This fixes it for every custom map, not just the
      one reported -- it was never map-specific, it was the parametrization.
- [x] **Water, nature, fence.** Water got a real scrolling ripple-texture
      layer (downloaded CC BY 4.0 textures) on top of the existing
      procedural wave shader -- also fixed this session: the water plane
      spans the whole world and was showing through at any terrain dip
      below sea level, not just the lake; `Terrain::HeightAt` now smooth-
      clamps everywhere outside the lake's own falloff. Nature got 4 real
      oak tree models (same CC BY 4.0 source, decimated, ~12% of tree
      spawns). Fence collision is back (see Phase 3 above) as per-track
      segments with deliberate gaps, not the old all-or-nothing toggle.
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
- [x] **Traffic cones / slalom mode — DONE (2026-10-17).** `Cones.hpp/.cpp`:
      cones placed alternating left/right along `road::Centreline()` (same
      source Fence.cpp/the minimap read from, so it works on any track,
      built-in or custom). Drawn with raylib's own `DrawCylinder`
      (radiusTop=0) -- a real cone shape with no new mesh/shader code.
      `ResolveCollisions` gives a soft push + hit count, never a hard
      block (clipping a cone is a mistake to notice, not a wall). New
      SLALOM button on the mode-select screen; HUD shows hits/total +
      elapsed time (reuses the existing lap timer as the clock).
- [x] **Desert flora — DONE (2026-10-17).** No downloaded cactus asset was
      available, so one is procedural instead (`World.cpp`'s
      `BuildCactusMesh`, same `MeshBuilder::AddBox` approach the game's
      own car bodies/fence posts already use): a trunk + two mirrored
      arms. Desert's non-rock spawns previously fell through to temperate
      kit trees (visibly wrong on dunes); they use the cactus now.
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
- [x] Fixed: both exes now call `ChangeDirectory(GetApplicationDirectory())`
      first thing in `main()`. Without it, double-clicking a `.cgmap` file
      (see Phase 4) made Windows set the working directory to wherever
      that file lives instead of the exe's own folder -- every relative
      shader/texture/model load (`"shaders/..."`, `"assets/..."`) then
      silently failed and fell back to raylib's default white
      material/texture, which is why the ground, trees, and cars would go
      blank white instead of erroring loudly.
- [x] `tools/pushbutton/`: a tiny `127.0.0.1`-only Python server + one-page
      UI (one big red button) that runs `git add -A && git commit && git
      push` on click and streams the output back. Not part of either exe
      build; run with `python tools/pushbutton/server.py`.

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
