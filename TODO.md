# TODO

Status as of 2026-10-14.

## In progress — fixing shattered Ultra Graphics car models

The 3 downloaded models (BMW M3 E30, M3 Competition, S1000RR) displayed as
fragmented/shattered shards in-game, not solid car bodies. Root cause: the
source FBX/glb is 58-143 separate mesh pieces merged with no shared
vertices between them, then decimated in multiple compounding passes --
each pass on an already-damaged mesh tore more holes. Measured: raw merged
mesh had a 23.8% boundary-edge ratio (edges touching only one triangle --
a healthy closed mesh is near 0%); the shipped shattered version was worse.

Plan (completed):
- [x] Weld near-coincident vertices across the separate source pieces
      before decimating -- stitches panel seams into shared topology.
      Boundary-edge ratio (unwelded -> welded -> after one decimation
      pass): E30 23.8% -> 2.0% -> 2.7% (25k faces); M3 Competition
      13.1% -> 3.8% -> 4.2% (79k faces); S1000RR 21.5% -> 2.0% -> 1.5%
      (22k faces). All three healthy now, not fragmented.
- [x] One decimation pass per model, not chained/repeated.
- [x] Redeployed all 3 .obj files, kept the v/vt/vn fix from the earlier
      crash bug intact.
- [x] Rebuilt and re-ran the automated roster self-test -- all 16 cars
      (including the 3 Ultra models) load and draw without crashing.
- [x] Committed and pushed.
- [ ] Still owed, deferred from earlier: water/nature/fence improvements
      ("make it crashable but not this bad" was about the car models, not
      these -- separate, not started this pass).

**Target hardware is unchanged: low-end (Intel HD 4000 class.)** The renderer
stays flat-shaded with a hard draw-distance cull.

## Later — rendering, only if the target ever moves up

Not scheduled. These assume a discrete GPU, which the stated target does not
have, so none of it should be started while low-end is the goal. Kept as a
list because the look is a lighting gap more than a geometry gap, ordered by
impact per unit of work:

- [ ] **Real directional shadows.** A single shadow map from the sun, sampled
      in `scene.fs`. Nothing in the scene casts a shadow right now, which is
      the single biggest reason it reads as a toy.
- [ ] **A proper material model.** Replace the flat N·L term with something
      with a specular lobe, so paint looks like paint and glass looks like
      glass. Per-material roughness/metalness, even as two constants.
- [ ] **Screen-space ambient occlusion**, or at minimum baked contact darkening
      where props and the car meet the ground.
- [ ] **Environment reflections on the car.** A small cubemap of the sky,
      refreshed rarely, would carry most of the "realistic car" impression.
- [ ] **Smooth (averaged) normals** on the car body. `MeshBuilder` currently
      emits one normal per face, which hard-facets every curve the new ring
      profile produces. Averaging normals across the loft would make the
      shoulder read as a curve for free. **This one is free on any GPU and is
      the best remaining suspect for the car-visuals problem above — worth
      doing now, unlike the rest of this section.**
- [ ] **Post-processing stack**: tonemapping, bloom on the lights and sun,
      slight vignette. Render to an HDR target first.
- [ ] **Denser geometry budget.** Station counts, wheel segments, terrain
      resolution and prop density were all chosen for the old target and can
      roughly double. Needs the 32-bit index switch below first.
- [ ] **Anisotropic/mipped textures on the road** instead of flat vertex
      colour, so the surface has grain at speed.
- [ ] **MSAA is requested but quality is otherwise fixed.** A quality preset
      (Low / High) in Settings would be the prerequisite for any of the above,
      so the low-end path stays the default.

## Not a reference — `nitrex boys thing/Realistic Car Game.zip`

Inspected read-only from the archive's metadata on 2026-10-14. The `.exe` was
not run and nothing was decompiled.

### What it is

| | |
| --- | --- |
| Engine | Unity 6 (6000.0.24f1), built Oct 2024 |
| Company | `DefaultCompany` — the Unity default, never changed |
| Pipelines | Both URP and HDRP runtimes shipped |
| Graphics | D3D12, DLSS (`nvngx_dlss.dll`), NVIDIA module, threaded gfx jobs |
| Packages | Post-processing, VisualEffectGraph, Cinemachine, Splines, TerrainTools, RoadArchitect, TextMeshPro, Input System, Burst |
| Physics | `UnityEngine.VehiclesModule` — Unity's stock WheelCollider |
| **Their own code** | **`Assembly-CSharp.dll`, 59 KB** |
| Their art | ~870 MB packed in `sharedassets0.assets.resS` |

Those last two rows are the whole story. The only file containing code they
wrote is 59 KB — a few thousand lines at most — against 136 engine assemblies
and 870 MB of art. It looks the way it does because **HDRP does the rendering
and asset packs do the art**. The roads are RoadArchitect (a free Unity
asset), the car physics are Unity's stock WheelCollider, and the shadows,
ambient occlusion, reflections and tonemapping are engine features switched
on, with DLSS clawing back the frame rate on a discrete GPU.

It is therefore **not a bar to measure this project against**: it targets a
high-end PC, and its 59 KB is glue between pre-built systems, whereas this
project *is* those systems — renderer, mesh generation, terrain, road splines,
water, physics and collision, written from scratch in C++ on top of raylib,
which supplies a window and a draw call and nothing else.

### Yes, it ships models — and we are not taking them

The asset files do contain real meshes and textures. Visible in the tables:

- **Car parts** — `Brake Disc`, `Wheel Black`, `brakeLights`, plus paint and
  glass materials.
- **Trees** — `Coconut_Palm_Tree01` through `12`, with `Pine_*_Billboard_LOD3`
  billboards for the far distance.
- **Rocks** — `Rock_Overgrown_A`–`D`, `Rock_06_A`, `Rock_A_03`, `Rock_C_02`,
  each with a full LOD chain (`_LOD00` … `_LOD04`) and anchor transforms.
- `StaticLightingSky`, baked lighting data, `Car Engine Sound`.

These are licensed third-party asset-store art, serialised into Unity's
binary format. Extracting them would need a ripping tool, would be taking
someone else's licensed art, and would break this project's rule that all
geometry is original. **Do not extract them.** The scenery here stays
procedural plus CC0 Kenney models, and the cars stay procedural.

### Models: staying procedural

Asked directly whether to swap the three named cars (Stormfront GT,
Nightrunner RZ, Duneblade X) onto imported geometry instead of the lofted
mesh in `CarBody.cpp`. Two options existed: Nitrex Boys' Unity asset-store
cars (off the table regardless, see above — no license to extract them) and
Kenney's Car Kit (CC0, FBX, already sitting in `downloads/carkit/`, fully
usable). **Decision: no to both, for the named cars.** Stay procedural and
fix the shading/shape bugs already tracked below instead of swapping engines
mid-project. Kenney's car kit remains available if incidental traffic or
parked background cars are ever added — those don't need to be original the
way the three player cars do.

### The one idea actually worth stealing

- [ ] **LOD chains on props.** Every rock and tree in their build ships three
      to five LOD levels plus a billboard for the far distance. Our props are
      a single mesh culled by hard distance, which is why ~270 of them are
      drawn at full detail every frame. Two levels plus a billboard would cost
      little and is squarely within the low-end target — unlike the rest of
      their technique list, which needs the GPU we are not targeting (see
      "Later — rendering" above).

## Open — car visuals

This is the one that keeps coming back. Three passes have each fixed a real
cause and each time the result still read as "blocky" at chase distance.

- [ ] **Car still looks bad from behind in Chase cam.** The rear three-quarter
      view is the worst angle. Fixed so far: the two-row loft (vertical slabs
      + flat lid), the full-width flat end caps, and the roof rails that
      floated off the crown of the new rounded roof. What is probably left:
  - [ ] **No real boot/decklid step.** The tail currently falls away as one
        continuous curve from the roof. A real car has a distinct deck plane
        with a lip at its trailing edge. Needs its own station group rather
        than `tailDrop` alone.
  - [ ] **Rear screen is not distinct from the bodywork.** The cabin loft
        blends glass into paint across the shoulder, so from behind the
        backlight and the boot are nearly the same tone and the shape reads
        flat. Needs a hard colour break at the beltline on the rear face.
  - [ ] **Wheels sit flush with the flanks.** They want to be tucked slightly
        under a visibly flared arch, with a dark shadow gap above each tyre,
        to create the depth cue the silhouette is missing.
  - [ ] **No contact shadow.** The car reads as floating because nothing
        darkens the road beneath it. A single dark quad under the body,
        draped to the road normal, would do most of the work.
  - [ ] **Flat lighting on a flat-shaded mesh.** With one directional term and
        no specular, curvature only reads through vertex colour. A cheap
        rim/fresnel term in `scene.fs` would make the shoulder line visible.
  - [ ] Judge each change from the **Orbit** camera, not Chase — Chase hides
        the profile that most of these are about.

## Open — respawn

- [x] **Respawn drops the car somewhere other than where it was.** Fixed:
      Backspace now calls `Car::RespawnNearestRoad()`, which snaps to
      `road::NearestSample(x, z)` facing that sample's forward vector, instead
      of always teleporting to the fixed start line. `Car::Respawn()` also now
      clears `speed_`, `steerAngle_`, `wheelSpin_` and `bodyRoll_`, and lifts
      the car 0.25 units above the terrain height so it settles onto the road
      rather than starting clipped into it.
  - [ ] Respawn-to-start-line specifically (not nearest point) still has no
        separate key or menu entry — only the first `Load()` of a car uses
        `road::StartPoint()`. Add one if it turns out to be wanted alongside
        nearest-point recovery.
  - [ ] An automatic respawn after being stuck or upside down for a few
        seconds is still unimplemented — `RespawnNearestRoad()` exists now,
        nothing calls it except the respawn key.

## Open — FPS

- [x] **Found and fixed a real, identified double-render.** While the Garage
      screen was open with a car already loaded, `main.cpp` was drawing the
      full background world (terrain, road, fences, ~270 props, lake) into
      the window *and* rendering the garage preview car into its own render
      target — two full 3D passes every frame, introduced by this session's
      own garage-preview feature. Fixed: the background world no longer
      draws while `inGarage` is true.
- [x] **Dropped MSAA 4x entirely.** It was on by default
      (`FLAG_MSAA_4X_HINT`) despite the stated low-end target; 4x
      antialiasing is a real, well-known cost on integrated GPUs like the
      Intel HD 4000 class this project targets, and the flat-shaded
      low-poly look was chosen specifically so it doesn't need smoothing to
      read well. Removed rather than made optional, since there was no
      Settings UI for it anyway.
- [ ] **The original drop (26 FPS while actually driving, not in the
      garage) is still not confirmed fixed.** The two items above are real
      and justified independently, but neither was verified against that
      specific screenshot's scenario with a profiler -- I can't see a live
      frame without the user's screen. If it's still low after the two
      fixes above, next things to check, in order: (1) temporarily comment
      out the two `mb.SmoothNormals(...)` calls in `CarBody.cpp` and
      rebuild, to rule out the normals pass or the STATIONS 17->25 increase;
      (2) confirm the running .exe is actually the `--config Release` build,
      not a stale Debug one; (3) profile prop count / `DRAW_DISTANCE` --
      both were tuned before the vertex-count increases this session.

## Open — world and gameplay
## Open — world and gameplay

- [ ] Lap timing and a start/finish line. The circuit is closed but nothing
      measures a lap.
- [ ] Traffic or AI opponents. The world is currently empty of other cars.
- [ ] Engine and tyre audio. There is no sound at all yet.
- [ ] Car damage or at least a collision thump — hitting a tree currently
      just scrubs speed silently.
- [ ] Day/night cycle. `Lighting.cpp` already centralises the sky and fog
      colours, so this is mostly a time uniform.
- [ ] Minimap or a circuit overview on the pause screen.
- [ ] Save the selected car alongside the key bindings.
- [x] **Fullscreen (F11).** Fixed: `main.cpp` now calls `ToggleFullscreen()`
      on F11, checked every frame regardless of menu/driving state.
      Deliberately used the classic video-mode toggle rather than
      `ToggleBorderlessWindowed()` — every 2D draw in this project (HUD, all
      of `Menu.cpp`) is positioned against the fixed `config::SCREEN_WIDTH/
      HEIGHT`, not `GetScreenWidth()`, so resizing the real framebuffer to
      the monitor's own resolution would misalign the UI against a 3D view
      that fills whatever the new size is. `ToggleFullscreen()` keeps the
      logical size the same, so this risk doesn't apply — not yet confirmed
      against every monitor since I can't see the rendered window myself,
      but it's the lower-risk of the two for this codebase.
  - [ ] Resolution / display options in Settings (picking a size, not just
        toggling fullscreen at the existing fixed size) still unimplemented.
- [x] Reverse gear feel — fixed: braking from a stop now eases into reverse
      at 0.45x forward acceleration instead of using the full brake-power
      curve, which previously made reverse launch as fast as forward drive.
- [ ] The car can drive up the mountain ring unchecked; it needs either a
      slope limit on traction or an invisible boundary.
- [ ] Nothing stops the car entering the lake beyond an "IN WATER" label.
      Decide: float, drown-and-respawn, or a soft wall at the shoreline.
- [x] Speedometer sign — fixed: the HUD now shows a red "R" next to the
      readout whenever speed is negative, instead of silently showing a
      positive number while reversing. Still km/h only (no mph option) and no
      needle/gauge visual, which is a separate, smaller ask than the original
      bug.
- [ ] Garage shows stats as plain text. Bars comparing the three cars would
      make the choice mean something.
- [x] Settings rebind cancel — fixed: ESC now backs out of "Press any
      key..." instead of being bindable, and the prompt says so.

## Open — technical

- [ ] `MeshBuilder` uses 16-bit indices, which caps any single batch at 65535
      vertices. The terrain resolution is already chosen to stay under it.
      Switch to 32-bit indices if a batch ever needs to grow.
- [ ] Props are culled by distance only. A frustum check would cut the ~270
      drawn props down substantially.
- [ ] No git repository yet. Worth initialising before the next round of
      changes.

## Done — mountain wall, lake wall (this session)

- [x] **Mountain can no longer be climbed.** `Car::Update` now rejects a
      move outright when the destination's terrain normal is too steep
      (`normal.y < 0.55`), on top of the existing slope-vs-speed drag.
- [x] **Lake wall.** Once the car is more than 1 unit below `WATER_LEVEL`
      it's pushed back toward shore instead of being able to drive clean
      across the lake bed. Shallow water at the shoreline is untouched
      (still just the existing drag/"IN WATER" label).

## Done — gearbox, settings tabs, audio options (concurrent session, same day)

Landed on disk while this session was running, from a second session working
on the same project. Read and verified rather than overwritten; the combined
build is green.

- [x] **Real gearbox.** `Gearbox.hpp/.cpp`: per-class gear counts (5 everyday
      / 7 sports / 8 hypercar, set per car in `CarDatabase.cpp` via
      `CarSpec::gears`), per-gear top speed and RPM curve, a rev limiter, and
      an accel multiplier that's weak lugging at low RPM and cuts at
      redline. Automatic mode shifts near the redline / drops a gear when
      well under the next one down; manual mode binds to new
      `ACTION_GEAR_UP`/`ACTION_GEAR_DOWN` actions (default Left Shift / Left
      Ctrl), refusing a downshift that would over-rev.
- [x] **Settings is now tabbed** (Keybinds / Sound / Car / Gear) instead of
      one long list -- needed once Sound sliders and the World-style picker
      joined key rebinding on the same screen.
- [x] **Persisted Options** (`Settings.hpp/.cpp`, `options.cfg`): sound
      on/off, master/engine/effects volume sliders, gear-shift-sound toggle,
      manual-vs-automatic, gear HUD on/off, mph vs km/h. Saved once on
      mouse-release (not every drag frame) and on leaving Settings.
- [x] **Engine audio now follows RPM**, not just raw speed -- `Engine.hpp`'s
      `EngineSound::Update` takes gearbox RPM as well as speed factor, so the
      pitch climbs through each gear and drops on an upshift instead of one
      smooth sweep. Added `audio::PlayShiftClunk()`, a short mechanical
      clunk on every gear change, silenced by the Sound tab's toggle.
  - [ ] Not yet re-tested by either session against a live screen -- the gear
        feel (ratios, shift points, RPM-to-pitch mapping) is tuned by ear in
        code and needs an actual drive to confirm it feels right, not just
        that it compiles.

## Done — engine audio

- [x] **Driving sounds, fully synthesised in code.** New `Engine.hpp/.cpp`
      (`audio::` namespace). No sampled/downloaded audio anywhere -- a
      phase-accumulated sawtooth + sub-octave drone fed through raylib's
      `AudioStream` every frame, pitch following speed (single-gear
      approximation, no real RPM/gearbox model), tyre squeal (filtered
      noise) layered in while holding the handbrake above 5 m/s, and a
      separate decaying-noise thump `Sound` played on the first frame of
      any tree/fence collision (edge-detected via `Car::JustHitObstacle()`,
      so leaning on an obstacle plays one thump, not one per frame).
      `audio::Init()/Shutdown()` wrap `InitAudioDevice()/CloseAudioDevice()`
      in `main()`.
  - [ ] Single-gear pitch model is a rough approximation, not a real
        RPM/gearbox simulation -- acceptable for now, a real one would need
        an actual gear-ratio table and shift logic.
  - [ ] No ambient sound yet (wind at speed, tyre roll on different
        surfaces, water splash entering the lake).
  - [ ] `PlayCollisionThump`'s `Sound` handle is deliberately leaked per
        call (documented in the code) -- fine for occasional thumps, would
        need a small pre-baked pool if collisions ever become very frequent.

## Done — garage drag-rotate, auto-respawn, first-drive hint

- [x] **Garage drag-to-rotate.** `Menu` now owns the preview angle
      (`previewAngle_`); `TickGarage` detects a click-drag over the preview
      rectangle and rotates manually from mouse delta, auto-orbiting again
      once released. `main.cpp` reads `ui.GaragePreviewAngle()` instead of
      incrementing its own local angle (one frame of lag between a drag and
      the texture reflecting it, imperceptible).
- [x] **Auto-respawn when stuck.** `Car::Update` tracks `stuckTimer_`:
      throttle held with `|speed| < 0.4` for 3.5s triggers
      `RespawnNearestRoad()` automatically. Releasing the throttle or
      actually moving resets the timer immediately, so it only fires while
      actively trying (and failing) to drive, never on a car someone parked
      on purpose.
- [x] **First-drive controls hint.** A dismissible panel listing every
      bound key, shown once on the very first Drive each session, fading out
      after 6s or dismissed early by any drive input or a click.
- [x] **Investigated the kit-car wheel-animation regression properly**
      (read raylib's actual `LoadOBJ` in `rmodels.c`). Confirmed: it does
      split an OBJ into separate meshes per `g` group/material boundary, but
      it does **not** preserve the group names anywhere in the `Model`
      struct afterward -- so there is no reliable way to find "which
      mesh index is the wheel" without hardcoding a guess per file that
      could silently break on a kit update. Not worth shipping blind;
      real fix would mean writing a small custom OBJ pre-pass that reads
      `tinyobj_shape_t.name` directly (bypassing `LoadModel`) to split out
      and hide the baked-in wheel groups by name before upload. Left as an
      open item below rather than guessed at.

## Done — lap timer, minimap, desert prop fix

- [x] **Lap timer.** `road::NearestSampleIndex()` (new) finds the closest
      circuit sample to the car each frame; `main.cpp` tracks it crossing
      from the last ~10% of the circuit back to the first ~10%, "armed" only
      after passing the halfway point since the last crossing (so sitting
      near the line can't rack up laps). Shows current lap number + time and
      the best completed lap, top-right. Resets whenever the car or world is
      reloaded (new car, new colour, new World style).
- [x] **Minimap.** Top-right corner, drawn from `road::Centreline()` (the
      same data fences already use) projected into a small 2D box, with a
      dot + heading tick for the car. No new world data, pure 2D projection.
- [x] **Desert prop fix.** `World::Load` now checks `terrain::GetStyle()`:
      desert scatters ~90% rocks instead of ~22%, and skips ~60% of
      candidate spots outright for open ground. Does not add real desert
      flora (cacti etc.) -- the Kenney kit only has temperate trees, so
      desert is deliberately sparse/rock-only rather than wrong-looking
      trees on dunes. A proper desert prop set is a separate asset need, not
      a code change, left below.

## More feature ideas (not started — backlog, roughly ordered by how self-contained each is)

- [ ] **Desert flora.** Needs an actual cactus/dead-scrub asset (not in the
      currently-owned Kenney Nature Kit) before desert scenery can be
      anything but sparse rocks.
## Done — FPS: the real scenery bottleneck, and real car models

- [x] **Found and fixed the actual scenery draw-call cost.** `World::Load`
      scatters up to `PROP_TARGET` (1100) trees/rocks, and `World::Draw` was
      calling `DrawModelEx` once per visible prop -- up to ~1100 individual
      draw calls a frame, each a separate GPU state change, for real
      imported OBJ meshes (not the lightweight procedural geometry the rest
      of the project uses). This is very likely the actual cause of the
      driving-time FPS drop, more so than anything flagged before it. Fixed:
      `World::Draw` now buckets visible props by model type and issues one
      `DrawMeshInstanced` call per type (6 total) instead of one call per
      prop.
- [x] **Switched all 10 cars to Kenney Car Kit models (CC0,
      `assets/models/car/`)**, already present in the project but never
      wired up. `Car::Load` now calls `LoadModel()` on a real OBJ per car
      instead of `carbody::BuildBody`, auto-scaled to each car's
      `Design::length` so wheelbase/cabin/camera-framing values stay valid.
      Paint colour is applied as a draw-time tint instead of baked into
      vertex colours. The procedural loft in `CarBody.cpp` is left in place,
      unused, in case it's wanted again.
  - [ ] **Known regression: wheel spin/steer animation is gone.** The kit's
        body OBJs bundle their own (static) wheels in the same mesh, so
        there's no separate wheel model left to rotate the way the
        procedural cars did. Real fix: hide/strip the baked-in wheel groups
        from the body mesh at load time (raylib's OBJ loader keeps `g`
        groups as separate sub-meshes, so this is filterable) and instance
        the kit's own `wheel-*.obj` at the same 4 hub positions the
        procedural path used, with the same spin/steer transform code
        already in `Car::Draw`.
  - [ ] **Several of the 10 cars share a base mesh with a different paint
        colour and scale** (`sedan-sports` used 3x, `race`/`race-future`
        used 2x each), since the kit only ships 5 plain, livery-free
        vehicles (`sedan-sports`, `hatchback-sports`, `race`, `race-future`,
        `suv` -- the rest of the kit is taxi/police/ambulance/delivery/
        truck/tractor liveries, left unused on purpose since a tint can't
        remove baked-in markings). Not confirmed how distinct the reused
        shapes read at in-game camera distance -- worth a look next time
        the game is actually seen running.
  - [ ] Garage preview and HUD text haven't been re-checked against the new
        model proportions (e.g. hood/bumper camera eye height, which still
        reads off `Design` values tuned for the procedural shape).
- [x] **Dropped MSAA 4x.** Same change as above, restated here since it
      compounds with the instancing fix on low-end hardware.

## Done — UI smoothness pass

- [x] **Animated buttons.** `ButtonAt()` in `Menu.cpp` used to snap instantly
      between its hover and idle colours/size. It now tracks a persistent
      per-button hover progress (keyed by label text, 0..1, eased with a
      smoothstep) and eases the fill colour, border colour, a slight
      grow-and-lift, and the label's vertical position over it, plus a brief
      outline flash on click. Applies everywhere `ButtonAt` is used — main
      menu, garage, settings, pause, credits — without touching each call
      site.
- [x] **Screen-switch fade.** Changing `screen_` (Drive/Garage/Settings/
      Credits/Pause/menu-back) used to be an instant cut. `Menu::Tick()` now
      fades each new screen in from black over ~0.2s whenever `screen_`
      changes, drawn as an overlay after that screen's own `TickX()` so it
      doesn't need every draw call in the file touched individually.

## More feature ideas (not started — backlog, roughly ordered by how self-contained each is)

- [ ] **Lap timer + start/finish line.** The circuit is closed but nothing
      times a lap. Needs: a finish-line trigger (a segment across the road at
      `road::StartPoint()`), a stopwatch while driving, a best-lap readout on
      the HUD and/or pause screen, and persisting the best lap per car+colour
      combo the way key bindings are already persisted.
- [ ] **Ghost/replay of your best lap.** Once lap timing exists: record
      position+heading samples during the best lap, play them back as a
      semi-transparent car on the next attempt. Builds directly on the lap
      timer above.
- [ ] **A second circuit / track selection.** Everything that reads the
      circuit goes through `road::Centreline()`/`road::StartPoint()` — a
      second named `CONTROL` point set and a track picker (reusing the same
      UI pattern as the car picker) is mostly data, not new systems.
- [ ] **Traffic cones / a slalom or time-trial mode** using the existing prop
      and collision systems rather than new ones.
- [ ] **Engine + tyre + collision audio.** There is no sound at all. raylib's
      audio device is trivial to bring up; the harder part is sourcing or
      synthesizing an engine loop that pitches with RPM/speed convincingly at
      low cost.
- [ ] **Minimap.** A small top-down render of `road::Centreline()` plus a dot
      for the car, drawn in a HUD corner — mostly 2D line-drawing from data
      that already exists.
- [ ] **Garage: spin the preview car with drag**, not just the automatic
      orbit — click-drag on the preview pane to manually rotate it, same
      camera math as the auto-orbit but driven by mouse delta instead of time.
- [ ] **Tutorial / first-launch hint overlay** — a one-time dismissible
      panel listing the controls, shown once before the first Drive.
- [ ] **A proper pause-screen "how to play"** page, separate from Credits,
      since Credits currently has to do double duty for licensing text and
      nothing else.

## Done — today's second pass (more cars, garage, shortcuts, fullscreen)

- [x] **Roster expanded from 3 to 10 cars.** Added seven new procedural
      silhouettes in `CarDatabase.cpp` — Lonewolf SV (hot hatch), Embercoupe
      GTX (muscle coupe), Vortexa Hyper (mid-engine hypercar), Glacier Line
      (executive sedan), Ridgeback RS (rally coupe), Solstice Roadster
      (two-seat roadster), Obsidian Phantom (grand tourer) — alongside the
      original three. Same approach as before: parametric `Design` structs
      through the existing lofted-mesh pipeline, nothing imported, no badges.
- [x] **Garage: live 3D preview.** The old 10-wide card strip would have been
      wider than the screen with ten cars, so the Garage screen was rebuilt
      around one large preview pane instead of a row of cards. `main.cpp`
      renders the selected car into its own `RenderTexture2D` (480x320) each
      frame the Garage is open, on a second `lighting::SceneShader` instance
      so it never shares the `viewPos` uniform with the real driving camera,
      with the camera orbiting the car over time. `Menu::SetGaragePreview()`
      hands the resulting texture to `Menu::TickGarage()`, which draws it
      with `DrawTexturePro` (flipped, since render textures come out
      upside-down). `<`/`>` buttons cycle the ten cars; stat bars (top speed /
      accel / handling, normalised against the roster's range) replace the
      old raw-number readout.
- [x] **Garage: colour picker.** Eight swatches (`cardb::PaintPalette()`) plus
      each car's own factory colour at index 0. `Car::Load()` now takes a
      `Color paintOverride` parameter rather than always using the roster's
      colour, applied by copying that car's `Design` and overwriting `.paint`
      before building the mesh — accent and glass colours stay as designed.
      main.cpp's car-swap-on-Drive check now also reloads on a colour change,
      the same way it already did on a car change.
- [x] **Main menu keyboard shortcuts.** D (Drive), G (Garage), S (Settings),
      C (Credits) alongside the existing mouse buttons, live only on
      `SCREEN_MAIN` — once driving, those letters go back to being steering/
      other actions, so there's no conflict with gameplay input.
- [x] **Fullscreen (F11).** See the entry under "Open — gameplay and
      technical" above for the implementation note and the one thing left
      unconfirmed about it.

## Done

- [x] Menu buttons invisible. raylib's immediate-mode 2D calls only land in
      the frame being presented; `Menu::Update()` ran before `BeginDrawing()`,
      so everything it drew was wiped by the next `ClearBackground()`. Merged
      input and drawing into one `Menu::Tick()` called inside the frame
      bracket.
- [x] ESC did nothing. raylib binds ESC to close-window by default, so
      `WindowShouldClose()` consumed it first. `SetExitKey(KEY_NULL)` in
      `main()`.
- [x] Pause screen with Resume / Settings / Main Menu, on P and ESC, with M
      as a shortcut back to the main menu.
- [x] FPS counter moved to the top-left under the speedo, always on, colour
      coded by frame rate.
- [x] Road overlapped the mountain ring — the circuit climbed the mountainside
      and the car slid down a bare cliff at 170 km/h. Control points rescaled
      to a ~450 unit radius against a 560 unit mountain start.
- [x] Fences were visual only. `Fence::ResolveCollisions` now pushes the car
      back using the nearest centreline sample's lateral offset.
- [x] Tree collision, with props kept clear of the road when scattered.
- [x] Hood camera showed a black screen — the eye sat inside the sealed,
      opaque cabin shell. Moved above the roofline.
- [x] Main menu was near-black. Now dims the live scene when there is one, or
      falls back to a sky gradient before the first drive.
- [x] Five camera modes on C: Chase, Hood, Bumper, Orbit, Cinematic.
- [x] Garage, rebindable Settings and Credits screens.
- [x] Respawn moved off R (broken key) to Backspace.
