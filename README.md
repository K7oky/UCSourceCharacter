<h1 align=center>Source Movement for Unreal Engine 5</h1>

<div align=center>

![Unreal Engine](https://img.shields.io/badge/Unreal-5.6-informational?style=for-the-badge&labelColor=101418&color=9ccbfb)
![Language](https://img.shields.io/badge/C%2B%2B-20-informational?style=for-the-badge&labelColor=101418&color=b9c8da)
![Tests](https://img.shields.io/badge/automation_tests-62-informational?style=for-the-badge&labelColor=101418&color=d3bfe6)
![Platform](https://img.shields.io/badge/platform-Win64-informational?style=for-the-badge&labelColor=101418&color=96f1f1)

**English** · [Русский](README.ru.md)

</div>

<div align=center>

**Surf** — riding a plane steeper than the walkable limit, and keeping the speed on the way out

![Surf](public/surf.gif)

**Bunnyhop** — strafe gain with the jump speed cap removed

![Bunnyhop](public/bhop-ezgif.com-crop.gif)

</div>


> A **behavioural replica** of Counter-Strike: Global Offensive player movement, running inside
> Unreal Engine 5.6 — bunnyhopping, air-strafing, surf, crouch-jumps, stamina, and the original
> engine's bugs, reproduced on purpose.

This is not "CS-like movement". It is a line-by-line port of `CGameMovement` and `CCSGameMovement`
from the Source engine, with the operation order, the single-precision arithmetic, and roughly
**58 catalogued quirks** preserved verbatim. Where Source has a bug, this has the same bug, tagged
with its quirk id and covered by a regression test.

---

## Table of contents

- [Why not `UCharacterMovementComponent`](#why-not-ucharactermovementcomponent)
- [What works](#what-works)
- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Architecture](#architecture)
- [Audio](#audio)
- [Console reference](#console-reference)
- [Config files](#config-files)
- [Importing CS2 maps](#importing-cs2-maps)
- [Debugging](#debugging)
- [Testing](#testing)
- [Extending it](#extending-it)
- [Known gaps](#known-gaps)
- [License](#license)
- [Credits](#credits)

---

## Why not `UCharacterMovementComponent`

Unreal's character movement and Source's are not the same algorithm with different constants — they
disagree about what a collision response *is*. Source slides along up to five clip planes per tick
with a hard-coded `DIST_EPSILON` push-out; Unreal depenetrates with Chaos and has its own step/floor
logic. Bolting CS numbers onto `UCharacterMovementComponent` gets something that feels *roughly*
like CS but fails on exactly the cases that matter: surf ramps, edge-bugs, crouch-jump height.

So none of it is used. The following are **deliberately absent** from the movement path:

| Not used | Replaced by |
|---|---|
| `ACharacter`, `UCharacterMovementComponent` | `ASourcePlayerPawn` + `USourceMovementComponent` |
| `UCapsuleComponent` | Source AABB hull (32×32×72 standing, 32×32×54 ducked) |
| `SafeMoveUpdatedComponent`, `SlideAlongSurface`, `ComputeSlideVector`, `TwoWallAdjust` | `TryPlayerMove` + `ClipVelocity` (4-bump solver, `MAX_CLIP_PLANES = 5`) |
| `FindFloor`, `WalkableFloorAngle`, `StepUp`/`StepDown` | `CategorizePosition`, `TracePlayerBBoxForGround`, `StayOnGround`, `StepMove` |
| UE gravity / friction / acceleration | `sv_gravity`, `sv_friction`, `sv_accelerate`, `sv_airaccelerate` |

`USourceMovementComponent` derives from `UPawnMovementComponent` **only** for the plumbing
(`UpdatedComponent` wiring, `PawnOwner`, the tick). Not one of `UMovementComponent`'s movement
helpers is called.

---

## What works

<!-- Add feature GIFs here -->

**Movement**
- Ground acceleration / friction with Source's `control` floor at `sv_stopspeed`
- Air acceleration with the 30 u/s budget cap — the actual air-strafe mechanism
- Bunnyhopping, `sv_autobunnyhopping`, and `PreventBunnyJumping`'s 1.1× speed lock
- Surf: ramps steeper than the walkable limit are slid along, with speed retained on exit
- Jump with stamina cost, ground-factor and the "ducked jumps overwrite vertical velocity" rule
- Continuous CS:GO duck (`m_flDuckAmount`), crouch-jump, duck-spam penalty, unduck ceiling checks
- Step up/down, `StayOnGround`, ground categorisation against the literal `normal.z >= 0.7`

**Systems around it**
- Fixed 64 or 128 tick simulation, decoupled from frame rate
- Real CS:GO console: `sv_*` / `cl_*` variables behind an `sv_cheats` gate that *reverts* on 0
- `bind` / `unbind` / `bindlist` with Source key names and the full `+command` alias set
- `exec` and `.cfg` files in a `cfg/` folder, including `autoexec.cfg`
- Footsteps, jump and landing audio driven by physical materials ([assets not included](#audio))
- Collision against ordinary Unreal colliders via Chaos, plus an analytic brush backend for tests

**Not implemented** — see [Known gaps](#known-gaps): water, spectator, networking.

---

## Requirements

- **Unreal Engine 5.6** (Win64)
- **Visual Studio 2022** with the *Game development with C++* workload
- Optional, for reading the port against its source: a local checkout of
  [Kisak-Strike](https://github.com/SwagSoftware/Kisak-Strike). Every ported function carries a
  `file.cpp:line` reference into that tree — chiefly `game/shared/gamemovement.cpp` and
  `game/shared/cstrike15/cs_gamemovement.cpp`.

> [!NOTE]
> The physics **must** be compiled with precise floating-point semantics (MSVC `/fp:precise`).
> UnrealBuildTool does not pass `/fp:fast` unless a target asks for it, so the default is already
> correct — but if anyone enables fast math project-wide, the golden tests will start failing.
> Do not "fix" those tests; fix the flags. This is documented at the top of `SourceMovement.Build.cs`.

---

## Quick start

### 1. Build

```bash
Build_Editor.bat
```

Pass `rebuild` for a full rebuild. Set `UE_ROOT` first if the engine is not installed in the default location:

```bash
set UE_ROOT=D:\UE_5.6
```

> [!WARNING]
> Close the editor before building. Live Coding holds the module DLL and the build will refuse to
> run with `Unable to build while Live Coding is active`.

### 2. Maps

No maps are included. Workshop geometry belongs to its authors and to Valve, so `Content/maps/` is
excluded from version control; [Importing CS2 maps](#importing-cs2-maps) documents the pipeline used
to produce it.

Because of that, `EditorStartupMap` and `GameDefaultMap` point at `/Engine/Maps/Entry`, an empty map
every engine install has. A fresh clone therefore opens to an empty level: create or import one,
then repoint both in **Project Settings** → **Maps & Modes**.

Development used two maps, one per movement discipline:

| Map | Suggested path | Exercises |
|---|---|---|
| `surf_utopia_njv` | `Content/maps/surf/` | Ramps steeper than the walkable limit. Air control, ramp exits, speed retention |
| `bhop_emevaelx3` | `Content/maps/bhop/` | Flat blocks and gaps. Jump timing, strafe gain, the `PreventBunnyJumping` cap |

An imported map needs a **PlayerStart** and the **GameMode Override** set to
`SourceMovementGameModeBase`, as described in the next section.

Surf and bhop are built around non-default console settings; each level's intent is matched by a
config preset described in [Config files](#config-files):

```
exec surf
```

### 3. Level setup

1. Place a **PlayerStart** where the player should spawn.
2. Open **World Settings** → **Game Mode** → **GameMode Override** and pick
   **`SourceMovementGameModeBase`**. It spawns `ASourcePlayerPawn` automatically.
3. Give the level geometry with collision that blocks the `Pawn` channel. Anything that blocks
   `ECC_Pawn` blocks the player — no special setup, no custom channel, no extra component.
4. Press Play.

That is the whole setup. `W A S D`, `SPACE`, `CTRL`, `SHIFT` and the mouse work immediately.

> [!IMPORTANT]
> **Scale.** `SourceUnits::UnitsToUU` is `1.0`, so **1 Source unit = 1 Unreal unit** and the player
> is **72 UU tall** — roughly a third of Unreal's 180 cm mannequin. Level geometry must be authored
> at Source scale, or the player will appear extremely small relative to the level. This is why the
> sample levels look small next to Unreal's starter content.
>
> To map Source units to real centimetres instead, change that one constant to `2.54`.
> It is deliberately the only conversion factor in the project — there are no scattered multipliers
> to hunt down.

### 4. Verify

Open the console (`` ` `` or `~`) and:

```
sv_cheats 1
cl_showpos 2
```

You now have a live readout of speed, ground state, wish direction, stamina, friction, trace counts
and the ground normal with its slope angle.

---

## Architecture

The single most important rule in this codebase:

> **The simulation knows nothing about Unreal.**
> `FSourceMovementSim::ProcessMovement` is a pure function of
> `(state, movedata, params, world, frametime)`. No `UObject`, no `AActor`, no `UWorld`.
> This is what makes it deterministic, replayable and testable without PIE — and it is what a
> networking layer would need.

### Layers

```
┌─ Unreal ────────────────────────────────────────────────────────────────┐
│  ASourcePlayerPawn            camera, key bindings, view update         │
│  USourceMovementComponent     fixed timestep, CVar snapshot, transform  │
│  USourceMovementAudioComponent  plays what the sim decided to play      │
│  USourceHullComponent         query-only proxy of the AABB              │
│  USourceLocalPlayer           claims `exec` before the engine does      │
└────────────────────────────┬────────────────────────────────────────────┘
                             │  FSourceInputState / FSourceMovementState
                             │  FSourceMovementParams / FSourceMovementEvents
┌────────────────────────────▼────────────────────────────────────────────┐
│  FSourceMovementSim           THE PORT. No Unreal types. 4000 lines.    │
│    _Move        friction, acceleration, walk/air move, jump, stamina    │
│    _Collision   TryPlayerMove, ClipVelocity, StepMove, CategorizePos    │
│    _Duck        the CS:GO continuous duck state machine                 │
│    _Audio       UpdateStepSound — decides, never plays                  │
└────────────────────────────┬────────────────────────────────────────────┘
                             │  ISourceWorldQuery  (FSourceTraceResult only)
┌────────────────────────────▼────────────────────────────────────────────┐
│  FSourceWorldQuery_UE        Chaos sweeps against real level collision  │
│  FSourceWorldQuery_Analytic  brush world; the reference implementation  │
└─────────────────────────────────────────────────────────────────────────┘
```

The movement code never sees an `FHitResult` — only `FSourceTraceResult`. That abstraction is not
decoration: it is what lets the same solver run against Chaos in game and against an exact analytic
brush world in tests, and it is how the Chaos backend gets validated.

### File map

| Path | What lives there |
|---|---|
| `Core/SourceScalar.h` | `srcfloat`, `FastSqrt`, `SinCos` — the float contract |
| `Core/SourceVector.h`, `SourceAngles.h` | `FSrcVec3`, `FSrcAngles`, verbatim `AngleVectors` / `VectorNormalize` |
| `Core/SourceMovementState.h` | `FSourceMovementState`, `FSourceMoveData`, `FSourceMovementEvents`, telemetry |
| `Core/SourceMovementParams.h` | Every physics constant, each with a Source line reference |
| `Core/SourceMovementSim.*` | The port itself, split across five `.cpp` by subsystem |
| `Core/ISourceWorldQuery.h` | The trace abstraction |
| `World/SourceWorldQuery_UE.*` | Chaos backend — reconstructs Source's plane maths from sweeps |
| `World/SourceWorldQuery_Analytic.*` | `CM_ClipBoxToBrush` port; exact, used as the oracle |
| `Input/SourceKButton.h` | `kbutton_t` with the fractional `KeyState()` (0 / 0.25 / 0.5 / 0.75 / 1) |
| `Input/SourceBindManager.*` | `bind`, `exec`, config files, the `+command` alias table |
| `Actors/*` | The Unreal-facing shell |
| `SourceMovementCVars.*` | All 40 console variables and the `sv_cheats` gate |
| `SourceUnits.h` | The **only** place units and the Y-axis flip are converted |

### Two invariants to know before modifying the codebase

**Timestep.** One `FSourceUserCmd` is one tick, always at `1/TickRateHz`, never at the frame delta.
The component accumulates real time and runs whole steps. Input is sampled **per step, not per
frame**, because `KeyState()` consumes impulse bits — sampling per frame would turn a key tapped
mid-frame into a full-strength press and would replay differently.

**Root rotation.** `UpdatedComponent` is held axis-aligned; the player's orientation lives only in
`State.ViewAngles`. The camera is a child, so any rotation left on the root would be applied to the
view a second time — and the solver sweeps a world-axis-aligned AABB that ignores actor rotation
anyway. A body mesh that must face the view belongs on a **child** component.

---

## Audio

No audio assets ship with this repository. The sounds used during development are Valve's and are
excluded from version control, so a fresh clone has fully working movement and no sound. The first
footstep that would have played writes a log line naming the missing asset.

Movement does not depend on audio in any way; detaching the audio component changes no physics.

The original CS2 sounds can be extracted with
[Source 2 Viewer](https://github.com/ValveResourceFormat/ValveResourceFormat), the same tool used for
map geometry in [Importing CS2 maps](#importing-cs2-maps). Player footsteps, landing impacts and the
jump launch live under `sounds/player/` inside the game's VPK archives; the CT-prefixed concrete set
is what the default paths below expect.

### How the split works

The **simulation decides when a sound happens**; the component only plays it. That is not an
arbitrary layering choice: footstep cadence in CS is tick-driven state (`m_flStepSoundTime` counts
down in milliseconds against `frametime`), and deciding it on the render tick would make footstep
spacing depend on frame rate, which it does not in CS.

```
FSourceMovementSim  ──► FSourceMovementEvents ──► USourceMovementAudioComponent
   UpdateStepSound        bStepSound                picks a clip, plays it
   CheckJumpButton        bJumpSound
   OnLand                 bLandSound
```

Events are drained **once per simulation step**, not per frame: at 128 tick a single frame can
contain two footsteps.

### Wiring up custom sounds

1. Import the required waves anywhere in `Content/`.
2. Select the player pawn and its **`MovementAudio`** component.
3. Fill in **`Default Sounds`**:

   | Field | Expects |
   |---|---|
   | `Footsteps` | An array of step variants. CS ships 17 per surface; any number works, one is fine |
   | `Land` | Landing impacts, picked at random |
   | `Jump Launch` | A single clip, Source's `Default.WalkJump` |

4. Optionally set **`Surf Wind Loop`**. It must be marked **Looping** on the Sound Wave, or wrapped
   in a looping Sound Cue — the component logs a warning if it is not, because a wind bed that plays
   once is indistinguishable from a missing asset.

Clips are picked at random with an immediate-repeat guard, so back-to-back identical footsteps do
not happen.

### Per-surface sounds

Surfaces are keyed by Unreal's **`EPhysicalSurface`**, which is the closest analogue of Source's
`surfaceproperties.txt`:

1. **Project Settings → Physics → Physical Surfaces** — name a surface type, e.g. `Metal`.
2. Create a **Physical Material** and set its **Surface Type** to it.
3. Assign that Physical Material to the surface's Material.
4. On the audio component, add an entry to **`Surface Sounds`** for that surface type.

Anything without a Physical Material resolves to `SurfaceType_Default` and falls back to
`Default Sounds`.

> [!NOTE]
> The surface id is the `EPhysicalSurface` value, deliberately **not** the asset's `GetUniqueID()`.
> A unique id is only stable within one process, so a table could not be authored against it at all.

### Default asset paths

The component ships with soft paths pre-filled, pointing at the layout used during development. Place
the assets at these paths and everything wires itself up with no editing; assets stored elsewhere
must be assigned manually.

```
Content/sounds/player/footsteps/concrete/concrete_ct_01 … concrete_ct_17
Content/sounds/player/land/concrete/land, land2, land3, land4
Content/sounds/player/land/concrete/jump_launch_01
Content/sounds/player/surf/slow_wind_lp_a_02
```

Unresolved soft paths are harmless — they load to null and the sound is skipped.

### When sounds actually fire

Ported from `CCSPlayer::UpdateStepSound` over `CBasePlayer::UpdateStepSound`, so the rules are CS's,
not generic ones:

| Rule | Value |
|---|---|
| **Walking is completely silent** | `+speed` / `SHIFT` suppresses footsteps at any speed |
| Minimum audible speed | `260 × 0.52 = 135.2` u/s, built from a constant — `sv_maxspeed` does not move it |
| Step interval | 300 ms running, 400 ms walking, × `sv_footstep_sound_frequency`, +100 ms ducked |
| Ducking | Volume × 0.65 |
| Jump | Launch clip, plus a forced full-volume footstep above 126 u/s |
| Landing | Only above **270 u/s** fall velocity, then a land clip plus a surface footstep |

The walk latch is **hysteretic**: pressing `SHIFT` at a full sprint does not silence footsteps instantly,
the player stays audible until they decelerate to within 25 u/s of the walk cap. That is Source
behaviour, not a bug.

### Mixing

| Where | What |
|---|---|
| `sv_footstep_volume_walk` / `_run` | Feed the simulation's own `fvol` (0.2 / 0.5 on concrete) |
| `sv_footstep_sound_frequency` | Step interval scale — **lower is more frequent** |
| `sv_footsteps 0` | Off entirely |
| Component: `Footstep`/`Land`/`Jump VolumeScale` | Pure mix multipliers for volume adjustments |
| Component: `Land Velocity For Full Volume` | Landing volume ramps from the 270 u/s threshold up to this |
| Component: `Surf Wind Speed Range`, `Volume Scale` | Wind bed, driven by speed |

---

## Console reference

Open the console with `` ` `` or `~`.

### The cheat gate

```
sv_cheats 1
```

Almost every `sv_*` and `cl_*` variable here is gated. With `sv_cheats 0` a write is refused with
Source's own message, and — as in Source — dropping it back to `0` **reverts every modified variable
to its default** rather than merely blocking further changes.

> [!NOTE]
> This is a deliberate divergence. In real CS:GO most movement variables are
> `FCVAR_NOTIFY|FCVAR_REPLICATED` but **not** cheat-protected. Each declaration in
> `SourceMovementCVars.cpp` records its true Source flags, so restoring accurate gating is a
> one-table edit.

### Variables

![Console variables](public/cvars.png)

<details><summary>Every console variable, with defaults</summary>

**Physics**

| Variable | Default | Notes |
|---|---|---|
| `sv_gravity` | 800 | |
| `sv_friction` | 5.2 | |
| `sv_stopspeed` | 80 | Floor for the friction `control` term |
| `sv_accelerate` | 5.5 | |
| `sv_airaccelerate` | 12 | **The surf knob.** Surf servers run 100–800 |
| `sv_maxspeed` | 320 | Upper bound; cannot exceed `CS_PLAYER_SPEED_RUN` in practice |
| `sv_maxvelocity` | 3500 | **Per-axis** clamp, not a magnitude clamp |
| `sv_stepsize` | 18 | |
| `sv_bounce` | 0 | |
| `sv_jump_impulse` | 301.993377 | `sqrt(2 · g · h)` |
| `sv_staminajumpcost` | 0.08 | |
| `sv_staminalandcost` | 0.05 | |
| `sv_staminarecoveryrate` | 60 | Applied by `ReduceTimers`, **before** the jump reads stamina |
| `sv_staminamax` | 80 | Not the same as `STAMINA_RANGE`, which is 100 |
| `sv_timebetweenducks` | 0.4 | |
| `sv_crouch_spam_penalty` | 2.0 | |
| `sv_rollangle` / `sv_rollspeed` | 0 / 200 | |

**Behaviour switches**

| Variable | Default | Notes |
|---|---|---|
| `sv_enablebunnyhopping` | 0 | `1` removes `PreventBunnyJumping` — the matchmaking speed lock |
| `sv_autobunnyhopping` | 0 | `1` re-jumps while jump is held. **Independent of the above** |
| `sv_enablebunnyhopping_maxspeedfactor` | 1.1 | The cap factor: 1.1 × 260 = 286 u/s |
| `sv_accelerate_use_weapon_speed` | 1 | |
| `sv_optimizedmovement` | 1 | Skips the leading `CategorizePosition` for walking players |

**Audio**

| Variable | Default | Notes |
|---|---|---|
| `sv_footsteps` | 1 | |
| `sv_footstep_sound_frequency` | 0.97 | Scales the interval, so **lower is faster** |
| `sv_footstep_volume_walk` / `_run` | 0.2 / 0.5 | Non-Source; stand in for the missing surface table |

**Client**

| Variable | Default | Notes |
|---|---|---|
| `cl_forwardspeed` / `cl_sidespeed` / `cl_backspeed` | 450 | |
| `cl_upspeed` | 320 | |
| `cl_pitchdown` / `cl_pitchup` | 89 | |
| `sensitivity` | 2.5 | |
| `m_yaw` / `m_pitch` | 0.022 | Degrees per mouse count |
| `cl_showpos` | 0 | `1` basic, `2` adds friction, traces, planes, ground normal |
| `cl_sourcemove_drawhull` | 0 | Draws the AABB actually being swept |
| `cl_sourcemove_drawtraces` | 0 | `1` hits only, `2` every sweep |

</details>

### Binding keys

Source syntax, Source key names:

```
bind "z" "+jump"
bind "MOUSE4" "+duck"
bind "SPACE" "+jump; +duck"
bind "p" "cl_showpos 1"
```

A bind only ever names the `+` half — the engine synthesises `-jump` on release, exactly as Source
does. Anything not starting with `+` is a plain command and fires on press only. Semicolons separate
multiple commands.

Key names are CS's (`SPACE`, `CTRL`, `MOUSE1`, `MWHEELUP`, `UPARROW`, `KP_END`, …) so a real CS:GO
config pasted in will resolve. Unreal's own key names work as a fallback.

> [!TIP]
> **The mouse wheel works**, which is what scroll-bhop needs:
> `bind "MWHEELUP" "+jump"`. A wheel notch has no hold phase — the press and the release land in the
> same frame — so the button bit comes from Source's `state & 3` (down *or* pressed this frame)
> rather than from the held bit alone. One notch produces exactly one jump command.

| Command | Effect |
|---|---|
| `bind <key> <cmd>` | Bind. With no `<cmd>`, prints the current binding |
| `unbind <key>` / `unbindall` | Remove |
| `bindlist` / `key_listboundkeys` | Print everything |
| `bind_defaults` | Restore the CS:GO defaults |
| `noclip` | Toggle noclip on the possessed pawn. Cheat-gated |
| `exec <file>` | Run a config from `cfg/` |
| `host_writeconfig [file]` | Save binds + changed variables |
| `echo <text>` | Print (Source has it, Unreal does not) |

Defaults: `W A S D` move, `SPACE` jump, `CTRL` duck, `SHIFT` walk, `TAB` scores. Weapon and
interaction verbs (`+attack`, `+use`, `+reload`) are not implemented — this is a movement port.

> [!NOTE]
> `exec` cannot be a normal console command. `ULocalPlayer::Exec` claims that name before
> `IConsoleManager` is ever consulted and rewrites the path to `../../Binaries/`. `USourceLocalPlayer`
> intercepts one level higher — that is the only reason it exists.

---

## Config files

Configs live in `<Project>/cfg/`, same as CS:GO, with the same syntax: one command per line, `//`
comments, quoted arguments.

| File | Role |
|---|---|
| `autoexec.cfg` | **Yours.** Runs last at startup, never overwritten. A commented template is created on first run |
| `config.cfg` | Generated by `host_writeconfig` and on shutdown. Do not hand-edit |
| `surf.cfg` | Surf preset — high air control, bunnyhopping unlocked |
| `bhop.cfg` | Bunnyhop preset — autohop, no speed cap, no stamina |
| `kz.cfg` | KZ / climb preset — no stamina, no speed cap, manual hop timing |
| `csgo.cfg` | Restores matchmaking defaults |

Startup order is `config.cfg` then `autoexec.cfg`, so settings in `autoexec.cfg` take precedence.

### Surf

```
exec surf
```

Surf launching is **emergent** — it falls out of `TryPlayerMove` and `ClipVelocity`, and needs no
special code. What stops it on matchmaking settings is `sv_airaccelerate 12`: with that little air
control, the required speed for a strong ramp exit is never built. The preset sets it to 800 and
unlocks bunnyhopping.

> [!IMPORTANT]
> A ramp only surfs if it is **steeper than 45.573°**. That threshold is the literal `normal.z >= 0.7`
> from `gamemovement.cpp:4229` — it is not configurable, in Source or here. Shallower ramps are
> walkable floor, and the player will simply walk up them. Check with `cl_showpos 1`, which prints
> the ground normal and slope angle.

---

### KZ and bhop

```
exec kz
exec bhop
```

Both switch **stamina off** (`sv_staminamax 0`), which is the single change that makes repeated
jumping work at all: with matchmaking stamina, each jump is scaled down by the last one and a chain
dies after a few hops. Both also set `sv_enablebunnyhopping 1` to remove the 286 u/s cap applied at
the moment the player leaves the ground.

They differ in exactly two places: `kz.cfg` keeps `sv_autobunnyhopping 0` for manual hop timing, and runs `sv_airaccelerate 100` instead of 1000.

> [!IMPORTANT]
> **Importing a workshop KZ map.** Hammer units *are* Source units, and `UnitsToUU` is `1.0`, so
> geometry brought in at **1:1 scale is already correct** — do not rescale it. If the import
> pipeline converts to centimetres the map will be 2.54× too large relative to the player, and every
> gap will be uncleanable while looking perfectly normal.
>
> Ladders are not implemented, so any KZ map with a required ladder climb cannot be finished.

---

## Importing CS2 maps

No map assets ship with this repository -- the surf and KZ maps used during development belong to
their Workshop authors and to Valve. The pipeline below is what they were brought in with, including
the parts that are not obvious.

### Export

CS2 is Source 2, so the files are compiled (`.vmap_c`, `.vmdl_c`) and Hammer will not open them. Use
[Source 2 Viewer](https://valveresourceformat.github.io/). Subscribed Workshop maps land in
`steamapps/workshop/content/730/<id>/`; the `publish_data.txt` next to the `.vpk` carries the map's
title, which is how to identify the numeric folders.

```bash
Source2Viewer-CLI.exe -i "<id>_dir.vpk" -o "exported/" -d --gltf_export_materials
```

Export as **`gltf`, not `glb`** -- GLB is capped at 2 GB by the spec and map exports do not fit.

### Scale

The exported geometry is in **raw Source units**, and `UnitsToUU` is `1.0`, so the correct import
scale is **1:1**. glTF is defined in metres, though, so an importer may helpfully multiply by 100.

Check rather than assume: place an `ASourcePlayerPawn` next to the geometry. It is 72 units tall. If
the map looks like a stadium, that is the metre conversion.

### Collision

Unreal's automatic collision generates convex primitives. A surf ramp is a concave angled surface
that no convex hull can represent, so the generated collision is a box around the whole chunk and the
player slides on it instead of on the ramp. In the mesh editor this shows as
`Num Collision Primitives: 1`.

Imported map geometry therefore uses the triangle mesh directly:

1. Select the imported meshes in the Content Browser.
2. **Asset Actions → Bulk Edit via Property Matrix**.
3. Set **Collision Complexity** to `Use Complex Collision As Simple`.

The triangle mesh then serves as the simple collision, and the solver's sweeps — issued with
`bTraceComplex = false` — resolve against the real ramp surface. `bTraceComplex` itself is left
unchanged.

Disabling **Auto Generate Collision** in the import options avoids creating the hulls in the first
place.

> [!NOTE]
> The `_physics` export is not world collision. Source 2 keeps a separate physics mesh, which makes
> it look like the right source for collision, but on `surf_utopia_njv` it contained only entity
> volumes: 75 `trigger_teleport`, 5 `trigger_multiple`, 3 `func_water` and 2
> `post_processing_volume`. Collision comes from the visual meshes.
>
> The file remains useful: the teleport volumes are the stage transitions, needed for the map to
> function as a surf map rather than as static geometry.

Several mesh classes need **No Collision** instead, or they become invisible obstacles: shadow
proxies (`*_shadow`), sky glow cards (`sun_disc_glow`) and water surfaces (`fancy_water`). These are
typically one or two triangles with a bounding box hundreds of units across.

### Seams

A map exports as dozens of separate meshes. Their vertices are welded — on `surf_utopia_njv`
adjacent chunks share hundreds of exactly equal positions — but each mesh remains a separate
collision component, and a sweep crossing from one to the next can report an edge normal rather than
a face normal. The symptom is intermittently catching on a junction while surfing.

**Tools → Merge Actors** on the ramp geometry removes the cause: a single mesh has no cross-mesh
edges, and Chaos resolves internal edges within one triangle mesh far more reliably. This is the
first thing to rule out when a seam appears to catch.

### What does not come across

Triggers, teleports, start and finish zones, and every other entity are game logic, not geometry.
glTF carries none of it.

---

## Debugging

| Command | Shows |
|---|---|
| `cl_showpos 1` | Speed, ground state, wish direction, ground normal + slope angle |
| `cl_showpos 2` | Adds stamina, friction, trace count, collision planes |
| `cl_sourcemove_drawhull 1` | The AABB the solver actually sweeps — not a mesh, the real hull |
| `cl_sourcemove_drawtraces 1` | Sweeps that hit |
| `cl_sourcemove_drawtraces 2` | Every sweep, including misses |
| `cl_sourcemove_tracelog 1` | Dumps the plane reconstruction whenever a sweep blocks with zero movement — the signature of a player pinned by bad collision geometry |

All of them require `sv_cheats 1`.

`USourceHullComponent` is a **query-only proxy**. It exists so other systems have something to hit;
the solver never reads it. If the drawn hull and the proxy ever disagree, trust the drawn hull.

---

## Testing

```bash
Run_SourceMovement_Tests.bat
```

67 automation tests, all deterministic and **UWorld-free** — the world is an analytic brush set and
the simulation is a plain object, so they run headless and give bit-identical results on every
machine.

| Group | Covers |
|---|---|
| `Math.*` | `VectorNormalize`, `AngleVectors` against hand-computed values |
| `Friction.*`, `GroundAccel.*`, `AirAccel.*` | Closed-form velocity checks at both tick rates |
| `Jump.*`, `SpeedLock.*` | Stamina, pogo lock, `PreventBunnyJumping` |
| `Duck.*` | The full duck state machine, crouch-jump geometry, spam penalty |
| `Footsteps.*` | Cadence, the `+speed` silence rule, the 135.2 u/s threshold |
| `View.*` | View yaw applied exactly once through all three layers |
| `KButton.*` | Held bit vs impulse bits — the rule that makes mouse-wheel binds work |
| `ClipVelocity.*`, `Ground.*`, `WalkMove.*` | Collision response and ground settling |
| `Seam.*` | Crossing the join between two floor brushes — no pinning, no stalled ticks |
| `NoClip.*` | Passes through solid, ignores gravity, and the two non-positive acceleration branches |
| `Determinism.*`, `Invariants.*` | Same inputs → same result; never in solid, speed bounded |

Expected values are **derived from the Source code**, not measured from this implementation. That
distinction is the whole point: a test that records what the code currently does cannot detect a
port error. When a test fails, check the derivation in its comment before changing the number.

---

## Extending it

Honest answer, split two ways.

### Around the simulation — easy

Every interesting boundary is a real seam:

| Goal | Action |
|---|---|
| Use different collision | Implement `ISourceWorldQuery` (7 pure virtuals). Two implementations exist as reference |
| Use Enhanced Input | Assign `USourceMovementComponent::InputProvider` — a `TFunction`. The pawn need not change |
| React to jumps/steps/landings | Subscribe to `USourceMovementComponent::OnMovementEvents` |
| Add a console variable | Three lines in `SourceMovementCVars.cpp`: declare, `Gate()`, `ApplyToParams` |
| Add a `+command` alias | One line in `RegisterButtons` |
| Add a console command | A branch in `ExecuteCommand` and a `RegisterVerb` call |
| Add sounds for a surface | Data only. Assign a Physical Material, add an entry to `SurfaceSounds` |
| Add a test | ~20 lines against `FSourceTestFixture` |

### Inside the simulation — deliberately hard

`FSourceMovementSim` is ~4000 lines with **zero virtual functions**, no hooks and no policy objects.
You cannot subclass it to change behaviour, and that is on purpose: every extension point in a
verbatim port is a place where someone silently diverges from the original and the golden tests stop
meaning anything.

Changing movement means **editing the port**, which means reading the Source original first. Every
function carries its `file.cpp:line` reference for direct comparison. This is the real cost of the
design, and it buys a simulation that can be checked against CS:GO.

For per-game movement tweaks, the intended route is a **new console variable** feeding
`FSourceMovementParams`, not a subclass.

---

## Known gaps

Stated plainly so nobody discovers these the hard way.

| Gap | Status |
|---|---|
| **Networking / client-side prediction** | **Not implemented.** The structures are ready for it — pure simulation function, POD state, one command per tick — but there is no prediction, reconciliation or rollback |
| Water (`CheckWater`, `WaterMove`, `WaterJump`) | Stub. Call sites exist and are marked `PHASE 7` |
| Spectator, toss move | Stub |
| `CheckStuck` | Not ported (the 54-entry jitter table) |
| Moving platforms / `baseVelocity` | Not implemented |
| Touch list | Not implemented |
| Cross-mesh collision edges | The Chaos backend reconstructs one contact plane per sweep, where `CM_ClipBoxToBrush` takes the maximum over a whole brush. A sweep crossing between two separate collision meshes can pick up an edge normal. Mitigated in the backend, and removed entirely by merging geometry -- see [Importing CS2 maps](#importing-cs2-maps) |
| Surface properties table | Empty. Surfaces differ in **sound** but share friction and jump factor (`UNKNOWN-2`) |
| Falling damage | Deferred — game rules, not movement |

Nine values could not be derived from the provided Source tree and are marked `UNKNOWN-n` in the
code and in the spec rather than guessed at.

---

## Documentation

`Docs/SOURCE_MOVEMENT_SPEC.md` is the authoritative specification: the Source function map, the
state machines, the pipeline order, all 58 quirks, every constant with its origin, the architecture
rationale and the phase plan. It is written in Russian.

When the code and the spec disagree, the **Source original wins**, and both get corrected.

---

## License

MIT, with one scope limit stated plainly in [LICENSE](LICENSE): the grant covers the original work
here -- the Unreal integration, the collision backends, the console and bind systems, the audio
layer, the tests and the tooling. It does not cover the ported movement algorithms, which are
Valve's, and it cannot, because they are not the copyright holder's to license.

For commercial use, read that file before integrating the project.

---

## Credits

The movement is a port of the Source engine's `CGameMovement` / `CCSGameMovement`, read from the
[Kisak-Strike](https://github.com/SwagSoftware/Kisak-Strike) source tree. All movement design credit
belongs to Valve; this repository is a study of that work, not an original design.

Asset extraction uses [Source 2 Viewer](https://github.com/ValveResourceFormat/ValveResourceFormat).

Footstep, landing and wind audio are CS:GO assets and are **not** part of this repository.
