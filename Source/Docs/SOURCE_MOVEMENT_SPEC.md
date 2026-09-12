# SOURCE MOVEMENT SPECIFICATION

Reverse-engineering отчёт по movement/player-physics из `C:\Users\Nurgisa\Documents\Kisak-Strike-master`.

**Status:** Phase 1 (archaeology) + Phase 2 (behavioral spec) + Phase 3 (architecture proposal).
**Implementation:** не начата (ожидает подтверждения).

Authoritative reference — только предоставленный код. Все утверждения ниже снабжены ссылками
`file:line`. Там, где значение не выводится из кода, стоит маркер `UNKNOWN`.

---

## 0. Перечень прочитанных файлов

| Файл | Роль |
|---|---|
| `game/shared/gamemovement.h` (342) | интерфейс `CGameMovement`, duck-константы, inline `TracePlayerBBox` |
| `game/shared/gamemovement.cpp` (5357) | базовая реализация всей физики игрока |
| `game/shared/igamemovement.h` (139) | `CMoveData` — контракт вход/выход движения |
| `game/shared/cstrike15/cs_gamemovement.cpp` (1467) | `CCSGameMovement` — CS:GO-оверрайды (accel, jump, duck, stamina) |
| `game/shared/movevars_shared.cpp` (71) | физические ConVar-константы |
| `game/shared/shareddefs.h` | `TICK_INTERVAL`, duck-тайминги, fall-константы, hull-макросы |
| `game/shared/cstrike15/cs_shareddefs.cpp` | `CS_PLAYER_SPEED_*` |
| `game/shared/cstrike15/cs_gamerules.cpp:212-226` | `g_CSViewVectors` — размеры hull |
| `game/shared/cstrike15/cs_player_shared.cpp` | `GetPlayerMaxSpeed`, `PhysicsSolidMaskForEntity` |
| `game/server/player_command.cpp` (482) | серверный per-tick цикл: `SetupMove`/`ProcessMovement`/`FinishMove` |
| `game/server/cstrike15/cs_playermove.cpp` (94) | CS-специфичная обвязка цикла |
| `game/client/prediction.cpp` (2241) | клиентский prediction / replay |
| `game/client/in_main.cpp` (2102) | сборка `CUserCmd` (forwardmove/sidemove) |
| `public/coordsize.h` | `DIST_EPSILON`, `COORD_RESOLUTION` |
| `public/const.h` | `DEFAULT_TICK_INTERVAL_PC` |
| `public/bspflags.h` | `MASK_PLAYERSOLID` |
| `public/mathlib/mathlib.h` | `SimpleSpline`, `Approach` |
| `vphysics/physics_material.cpp:609` | default surface friction |

Важно: `portal_gamemovement.*` и `FullTossMove`/`IsometricMove` к CS-игроку не применяются
(`MOVETYPE_WALK` / `MOVETYPE_LADDER` / `MOVETYPE_NOCLIP` / `MOVETYPE_OBSERVER`).

---

## 1. SOURCE MOVEMENT MAP

`B` = base `CGameMovement`, `CS` = переопределено в `CCSGameMovement`.

| Source Function | Файл:строка | Purpose | Читает/пишет state | UE Equivalent (наш) |
|---|---|---|---|---|
| `CPlayerMove::RunCommand` | player_command.cpp:318 | 1 usercmd = 1 tick; `frametime = TICK_INTERVAL`, `curtime = m_nTickBase*TICK_INTERVAL`; инкремент `m_nTickBase` | tickBase, buttons latch | `FSourceMovementSim::RunCommand` |
| `CPlayerMove::SetupMove` | player_command.cpp:133 | entity → `CMoveData` | origin, velocity, angles, oldButtons | `FSourceMoveData::FromState` |
| `CPlayerMove::FinishMove` | player_command.cpp:209 | `CMoveData` → entity; `m_nOldButtons = m_nButtons` | origin, velocity, bodyPitch | `FSourceMoveData::ToState` |
| `CheckMovingGround` | player_command.cpp:~90 | conveyor → baseVelocity; `(1 + dt*0.5) * baseVelocity` импульс | baseVelocity, FL_BASEVELOCITY | `ApplyBaseVelocity` (Phase 6) |
| `ProcessMovement` | gamemovement.cpp:1325 (B), cs:421 (CS) | вход; `frametime *= LaggedMovementValue`; `m_flMaxSpeed = GetPlayerMaxSpeed()`; `m_iSpeedCropped = 0` | — | `FSourceMovementSim::ProcessMovement` |
| `PlayerMove` | gamemovement.cpp:4994 (B), cs:451 (CS) | главный диспетчер тика | всё | `Sim::PlayerMove` |
| `CheckParameters` | gamemovement.cpp:1137 (B), **cs:169 (CS)** | duck-spam штраф, walk-модификатор, speed factors, stamina-скейл, клэмп входа до `m_flMaxSpeed`, углы, stacking | m_flDuckSpeed, m_flMaxSpeed, fwd/side/up, angles, m_iMoveState | `Sim::CheckParameters` |
| `ReduceTimers` | gamemovement.cpp:1244 (B), **cs:581 (CS)** | stamina recovery, затем msec-таймеры | m_flStamina, m_nDuckTimeMsecs… | `Sim::ReduceTimers` |
| `CheckStuck` | gamemovement.cpp:3765 | jitter-таблица расстыковки; на клиенте выключено | origin | `Sim::CheckStuck` (Phase 6) |
| `UpdateDuckJumpEyeOffset` | gamemovement.cpp:4578 | legacy duck-jump eye interp (в CS не активен) | viewOffset | порт как есть |
| `Duck` | gamemovement.cpp:4773 (B), **cs:964 (CS)** | continuous duck state machine через `m_flDuckAmount` | m_flDuckAmount, m_bDucked, m_bDucking, FL_DUCKING, FL_ANIMDUCKING, origin, viewOffset | `Sim::Duck` |
| `CanUnduck` | gamemovement.cpp:4493 (B), **cs:850 (CS)** | hull-трасса standing-hull | — | `Sim::CanUnduck` |
| `FinishDuck` | gamemovement.cpp:4635 (B), **cs:926 (CS)** | хард-переход в ducked hull; сдвиг origin | origin, flags, viewOffset, m_flLastDuckTime | `Sim::FinishDuck` |
| `FinishUnDuck` | gamemovement.cpp:4532 (B), **cs:891 (CS)** | хард-переход в standing hull | origin, flags, viewOffset | `Sim::FinishUnDuck` |
| `SetDuckedEyeOffset` | gamemovement.cpp:4707 | `SimpleSpline` интерполяция eye height | viewOffset | `Sim::SetDuckedEyeOffset` |
| `HandleDuckingSpeedCrop` | gamemovement.cpp:4731 (B), **cs:829 (CS)** | умножает fwd/side/up **и `m_flMaxSpeed`** на `GetDuckSpeedModifier` | fwd/side/up, m_flMaxSpeed, m_iSpeedCropped | `Sim::HandleDuckingSpeedCrop` |
| `FixPlayerCrouchStuck` | gamemovement.cpp:4466 | до 36 шагов по ±1 по Z при застревании | origin | `Sim::FixPlayerCrouchStuck` |
| `LadderMove` | gamemovement.cpp:3157 (B), cs:536 (CS) | attach/detach/движение по лестнице | movetype, velocity, m_vecLadderNormal, gravity | `Sim::LadderMove` (Phase 7) |
| `FullWalkMove` | gamemovement.cpp:2287 | пайплайн walk/air/water | всё | `Sim::FullWalkMove` |
| `StartGravity` | gamemovement.cpp:1490 | `vel.z -= g*0.5*dt`; + baseVelocity.z*dt; сброс baseVelocity.z | velocity | `Sim::StartGravity` |
| `FinishGravity` | gamemovement.cpp:1937 | `vel.z -= g*0.5*dt` | velocity | `Sim::FinishGravity` |
| `CheckJumpButton` | gamemovement.cpp:2629 (B), **cs:627 (CS)** | прыжок; `PreventBunnyJumping`; stamina-скейл; `FinishGravity` внутри | velocity.z, groundEntity, m_flStamina, oldButtons | `Sim::CheckJumpButton` |
| `PreventBunnyJumping` | **cs:603 (CS)** | клэмп \|v\| до `1.1 * player->m_flMaxspeed` | velocity | `Sim::PreventBunnyJumping` |
| `Friction` | gamemovement.cpp:1865 | ground friction с `stopspeed`-полом | velocity, m_outWishVel | `Sim::Friction` |
| `WalkMove` | gamemovement.cpp:2150 | wishdir/wishspeed, `Accelerate`, доп. клэмп до maxspeed, direct-trace → `StepMove` → `StayOnGround` | velocity, origin | `Sim::WalkMove` |
| `Accelerate` | gamemovement.cpp:2075 (B), **cs:1251 (CS)** | ground acceleration | velocity, m_flGroundAccelLinearFracLastTime, m_vecTrailingVelocity | `Sim::Accelerate` |
| `AirMove` | gamemovement.cpp:2006 | wishdir/wishspeed в воздухе, `AirAccelerate`, `TryPlayerMove` | velocity, origin | `Sim::AirMove` |
| `AirAccelerate` | gamemovement.cpp:1960 | air acceleration с cap `wishspd=30` | velocity, m_outWishVel | `Sim::AirAccelerate` |
| `CanAccelerate` | gamemovement.cpp:2055 (B), cs:432 (CS) | gate по `State_Get()`/waterjump | — | `Sim::CanAccelerate` |
| `TryPlayerMove` | gamemovement.cpp:2850 | 4-bump slide solver, до `MAX_CLIP_PLANES=5` плоскостей | origin, velocity | `Sim::TryPlayerMove` |
| `ClipVelocity` | gamemovement.cpp:3506 | проекция на плоскость + overbounce + anti-penetration итерация | out | `Sim::ClipVelocity` |
| `StepMove` | gamemovement.cpp:1758 | down-attempt vs up-attempt, выбор по 2D-дистанции | origin, velocity, m_outStepHeight | `Sim::StepMove` |
| `StayOnGround` | gamemovement.cpp:2114 | второй snap-down после WalkMove | origin | `Sim::StayOnGround` |
| `CategorizePosition` | gamemovement.cpp:4147 | ground detection + snap-down + surfaceFriction | groundEntity, origin, m_surfaceFriction, waterLevel | `Sim::CategorizePosition` |
| `TracePlayerBBoxForGround` | gamemovement.cpp:4049 | 4 квадрантные под-box трассы | pm | `Query::TraceHullForGround` |
| `CheckValidStandableGroundCandidate` | gamemovement.cpp:4128 | `normal.z >= 0.7` или «это игрок» | — | `Sim::IsStandableGround` |
| `SetGroundEntity` | gamemovement.cpp:3985 | смена ground; baseVelocity-обмен; **`vel.z = 0`** | groundEntity, baseVelocity, velocity.z, m_surfaceFriction | `Sim::SetGroundEntity` |
| `CategorizeGroundSurface` | gamemovement.cpp:1074 | `m_surfaceFriction = min(props.friction*1.25, 1)` | m_surfaceFriction, m_chTextureType | `Sim::CategorizeGroundSurface` |
| `CheckVelocity` | gamemovement.cpp:3410 | NaN-sanity + **per-axis** клэмп ±`sv_maxvelocity` | velocity, origin | `Sim::CheckVelocity` |
| `CheckFalling` | gamemovement.cpp:4337 | landing: damage/sound/punch, `OnLand`, сброс `m_flFallVelocity` | m_flFallVelocity, m_flStamina | `Sim::CheckFalling` |
| `CheckWater` / `GetWaterCheckPosition` | gamemovement.cpp:3935 / 3910 | 3-точечный water level | waterLevel, waterType | `Sim::CheckWater` (Phase 7) |
| `WaterMove` / `WaterJump` / `CheckWaterJump` | gamemovement.cpp:1614/1591/1514 | вода | velocity, origin | Phase 7 |
| `FullNoClipMove` | gamemovement.cpp:2525 | noclip | velocity, origin | Phase 7 |
| `FullObserverMove` | gamemovement.cpp:2415 | observer | velocity, origin | Phase 7 |
| `OnJump` / `OnLand` | **cs:1234 / cs:1243 (CS)** | stamina-штрафы | m_flStamina | `Sim::OnJump/OnLand` |
| `TracePlayerBBox` | gamemovement.h:308 | AABB sweep с hull по `m_bDucked` | — | `Query::TracePlayerHull` |
| `TestPlayerPosition` | gamemovement.cpp:939 | нуль-длины трасса (точечный тест) | — | `Query::TestPlayerHull` |
| `PlayerSolidMask` | gamemovement.cpp:806 (B), **cs:130 (CS)** | content mask (+team bits) | — | `FSourceContentMask` |

---

## 2. MOVEMENT STATE MACHINE

### 2.1 MoveType (верхний уровень) — `PlayerMove`, gamemovement.cpp:5087

```
                   LadderMove() == true
   MOVETYPE_WALK ─────────────────────────────▶ MOVETYPE_LADDER
        ▲                                              │
        │  LadderMove() == false (gamemovement.cpp:5075-5083)
        └──────────────────────────────────────────────┘
                 IN_JUMP && curtime >= m_ignoreLadderJumpTime
                 → velocity = 270 * ladderNormal, movetype = WALK   (:3290-3299)

   MOVETYPE_NOCLIP    → FullNoClipMove(sv_noclipspeed, sv_noclipaccelerate)
   MOVETYPE_OBSERVER  → FullObserverMove
   MOVETYPE_NONE      → nothing
```
Для живого CS-игрока актуальны только `WALK` и `LADDER`.

### 2.2 Ground state — вычисляется в `CategorizePosition` (gamemovement.cpp:4147)

```
        vel.z > 250 (PlayerMove, :5043)              ┌──────────┐
  ┌────────────────────────────────────────────────▶ │ AIRBORNE │
  │  vel.z - groundVel.z > 140 (CategorizePos :4187) └──────────┘
  │  no standable plane found (:4247)                      │
┌─┴────────┐                                               │ standable plane:
│ GROUNDED │◀──────────────────────────────────────────────┘ normal.z >= 0.7
└──────────┘                                                 OR hit entity IsPlayer()
     │                                                       → SetGroundEntity(&pm): vel.z = 0
     │ bMoveToEndPos: snap origin вниз до pm.endpos (:4313)
     └─ StayOnGround() после WalkMove (:2281) — второй snap
```

**Нет отдельного состояния `Sliding`.** Крутой склон (`normal.z < 0.7`) = AIRBORNE:
`SetGroundEntity(NULL)`, и если `vel.z > 0` **и** трассировки не нашли опору →
`m_surfaceFriction = 0.25` (:4249-4253). При `vel.z > 140` срабатывает ранняя ветка и friction остаётся 1.0.
Скольжение по склону — эмерджентный результат `TryPlayerMove` + gravity, а не состояние.

### 2.3 Duck state machine — CS-версия (`CCSGameMovement::Duck`, cs:964)

Континуальная, ведущая переменная — `m_flDuckAmount ∈ [0,1]`. Три независимых бита
(`m_bDucked`, `m_bDucking`, `FL_DUCKING|FL_ANIMDUCKING`) обновляются **несинхронно** — это часть поведения.

```
                    IN_DUCK (после DuckingEnabled()-фильтра)
 STANDING ──────────────────────────────────────────────▶ DUCKING_DOWN
 duckAmount=0                                             m_bDucking = true (:1103-1107)
 m_bDucked=false                                           duckAmount += dt * (m_flDuckSpeed*0.8)
 !FL_DUCKING                                               (*0.4 если m_bIsDefusing)
     ▲                                                    FL_ANIMDUCKING при duckAmount>=0.1 (:1134)
     │                                                            │
     │                                            duckAmount>=1.0 OR !onGround (:1124)
     │                                                            ▼
     │                                                    FinishDuck() (cs:926)
     │                                                    m_bDucked=true, m_bDucking=false
     │                                                    duckAmount=1, FL_DUCKING|FL_ANIMDUCKING
     │                                                    origin: onGround → 0; air → +9
     │                                                    m_flLastDuckTime = curtime
     │                                                            │
     │                                                       ┌─ DUCKED ─┐
     │                                                       └──────────┘
     │                                                            │ !IN_DUCK
     │                                                            ▼
     │                                                 CanUnduck()? ── нет ──┐
     │                                                       │ да            │ duckAmount=1
     │                                                       ▼               │ m_bDucked=true
     │                                              UNDUCKING                │ m_bDucking=false
     │                              m_bDucked = false СРАЗУ (:1157)  ◀───────┘ FL_* снова ставятся
     │                              duckAmount -= dt*MAX(1.5, m_flDuckSpeed)
     │                              FL_* снимаются при duckAmount <= 0.75 (:1172)
     │                                                       │
     └───────────────── duckAmount<=0 OR !onGround ───────────┘
                        FinishUnDuck() (cs:891): origin onGround → 0; air → -9
```

Гейты на вход (`DuckingEnabled`, cs:149): `m_flDuckSpeed >= 1.5` **и**
(`FL_DUCKING` уже стоит **или** `curtime >= m_flLastDuckTime + sv_timebetweenducks`).
Если гейт закрыт — `IN_DUCK` снимается из `m_nButtons` в `CheckParameters` (cs:199-202).

### 2.4 Jump gate — `CCSGameMovement::CheckJumpButton` (cs:627)

```
deadflag                            → oldButtons |= IN_JUMP, false
m_flWaterJumpTime != 0              → декремент, false
taunt + thirdperson                 → false
waterLevel >= WL_Waist              → SetGroundEntity(NULL), vel.z = 100/80, false
groundEntity == NULL                → oldButtons |= IN_JUMP, false       (нет воздушного прыжка)
(oldButtons & IN_JUMP) && !sv_autobunnyhopping → false                    (нет pogo)
────────── прыжок разрешён ──────────
!sv_enablebunnyhopping → PreventBunnyJumping()
SetGroundEntity(NULL)
standingOnFallingPlayer  → vel.z = 0
else duckUntilOnGround || m_bDucking || FL_DUCKING || standingOnOtherPlayer
                         → vel.z  = jumpFactor * sv_jump_impulse     (ПЕРЕЗАПИСЬ)
else                     → vel.z += jumpFactor * sv_jump_impulse     (ДОБАВЛЕНИЕ)
stamina > 0              → vel.z *= clamp(1 - stamina/100, 0, 1)
FinishGravity()
m_outWishVel.z += vel.z - startz ;  OnJump(m_outWishVel.z)
oldButtons |= IN_JUMP
```

---

## 3. PHYSICS PIPELINE (точный порядок операций)

### 3.1 Per-command (серверный тик) — `CPlayerMove::RunCommand`, player_command.cpp:318

```
 1  flTimeAllowedForProcessing = ConsumeMovementTimeForUserCmdProcessing(TICK_INTERVAL)
    если < TICK_INTERVAL → команда ОТБРАСЫВАЕТСЯ целиком (:323-338)
 2  StartCommand(player, ucmd)
 3  curtime   = m_nTickBase * TICK_INTERVAL
    frametime = paused ? 0 : TICK_INTERVAL
 4  ucmd->buttons |= m_afButtonForced;  &= ~m_afButtonDisabled
 5  UpdateButtonState(ucmd->buttons)
 6  CheckMovingGround(player, TICK_INTERVAL)       // conveyor/baseVelocity
 7  pl.v_angle = ucmd->viewangles                  // если fixangle == NONE
 8  RunPreThink / RunThink
 9  SetupMove(player, ucmd, helper, moveData)
10  g_pGameMovement->ProcessMovement(player, moveData)   ◀── вся физика
11  FinishMove(player, ucmd, moveData)
12  moveHelper->ProcessImpacts()
13  RunPostThink
14  FinishCommand
15  if (frametime > 0) ++m_nTickBase
```

### 3.2 `ProcessMovement` → `PlayerMove` (gamemovement.cpp:1325, 4994)

```
ProcessMovement:
  m_nTraceCount = 0
  frametime *= GetLaggedMovementValue()
  ResetGetWaterContentsForPointCache()
  m_iSpeedCropped = SPEED_CROPPED_RESET
  mv->m_flMaxSpeed = player->GetPlayerMaxSpeed()
  PlayerMove()
  FinishMove()            // m_nOldButtons = m_nButtons
  frametime = restored

PlayerMove (CS: cs:451 оборачивает — если !CanMove(), обнуляет ввод и снимает
            IN_JUMP|IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT):
   1  CheckParameters()                      ◀── CS-override
   2  m_outWishVel = 0 ;  m_outJumpVel = 0
   3  ResetTouchList()
   4  ReduceTimers()                         ◀── CS-override (stamina recovery)
   5  AngleVectors(viewangles) → m_vecForward/m_vecRight/m_vecUp
   6  if (movetype не в {NOCLIP,NONE,ISOMETRIC,OBSERVER} && !deadflag)
          if (CheckInterval(STUCK)) if (CheckStuck()) return     // тик прерывается!
   7  if (movetype != WALK || m_bGameCodeMovedPlayer || !sv_optimizedmovement)
          CategorizePosition()
      else
          if (vel.z > 250) SetGroundEntity(NULL)                 ◀── КВИРК Q7
   8  m_nOldWaterLevel = GetWaterLevel()
   9  if (groundEntity == NULL) m_flFallVelocity = -vel.z        ◀── латч для landing
  10  m_nOnLadder = 0
  11  UpdateStepSound(...)                                       // только звук
  12  UpdateDuckJumpEyeOffset()
  13  Duck()                                  ◀── CS-override; меняет hull и origin
  14  if (!deadflag && !FL_ONTRAIN) LadderMove()
  15  switch (movetype) → FullWalkMove() | FullLadderMove() | FullNoClipMove() | FullObserverMove()
  ── CS post (cs:463-507): восстановление m_flVelocityModifier, view offset, m_bHasMovedSinceSpawn
```

### 3.3 `FullWalkMove` (gamemovement.cpp:2287) — основной путь

```
 1  if (!CheckWater()) StartGravity()                    // vel.z -= 0.5*g*dt
 2  if (m_flWaterJumpTime) { WaterJump(); TryPlayerMove(); CheckWater(); return }
 3  if (waterLevel >= WL_Waist) { ...water branch...; return-ish }
    ── НЕ в воде: ──
 4  if (buttons & IN_JUMP) CheckJumpButton()    else  oldButtons &= ~IN_JUMP
 5  if (groundEntity != NULL) { vel.z = 0 ; m_flFallVelocity = 0 ; Friction() }
 6  CheckVelocity()
 7  if (groundEntity != NULL) { WalkMove() ; m_bHasWalkMovedSinceLastJump = true }
    else                      { AirMove() }
 8  CategorizePosition()                                 // ground detect + snap + surfaceFriction
 9  CheckVelocity()
10  if (!CheckWater()) FinishGravity()                   // vel.z -= 0.5*g*dt
11  if (groundEntity != NULL) vel.z = 0
12  CheckFalling()                                       // landing FX + OnLand (stamina)
13  splash-переход по m_nOldWaterLevel
```

Порядок 1 → 5 → 7 → 8 → 10 означает: **gravity применяется половинами до и после движения**
(Verlet-подобная схема), а ground detection — **после** перемещения.

### 3.4 `WalkMove` (gamemovement.cpp:2150)

```
 1  AngleVectors(viewangles) → forward, right, up
 2  oldground = groundEntity                          // латч ДО движения
 3  fmove = m_flForwardMove ; smove = m_flSideMove
 4  forward.z = 0 ; right.z = 0 ; normalize оба
    (при g_bMovementOptimizations normalize только если z != 0 — см. Q-список)
 5  wishvel.xy = forward.xy*fmove + right.xy*smove ; wishvel.z = 0
 6  wishdir = wishvel ; wishspeed = VectorNormalize(wishdir)
 7  if (wishspeed != 0 && wishspeed > m_flMaxSpeed) → scale wishvel, wishspeed = m_flMaxSpeed
 8  vel.z = 0 ; Accelerate(wishdir, wishspeed, sv_accelerate) ; vel.z = 0
 9  if (|vel|² > m_flMaxSpeed²) vel *= m_flMaxSpeed/|vel|      ◀── ЖЁСТКИЙ ground cap (Q22)
10  vel += baseVelocity
11  spd = |vel| ; if (spd < 1) { vel = 0 ; vel -= baseVelocity ; return }   ◀── Q23
12  dest = (origin.x + vel.x*dt, origin.y + vel.y*dt, origin.z)
13  TracePlayerBBox(origin → dest) → pm
14  m_outWishVel += wishdir * wishspeed
15  if (pm.fraction == 1) { origin = pm.endpos ; vel -= baseVelocity ; StayOnGround() ; return }
16  if (oldground == NULL && waterLevel == WL_NotInWater) { vel -= baseVelocity ; return }
17  if (m_flWaterJumpTime) { vel -= baseVelocity ; return }
18  StepMove(dest, pm)
19  vel -= baseVelocity
20  StayOnGround()
```

### 3.5 `AirMove` (gamemovement.cpp:2006)

```
 1  AngleVectors(viewangles)
 2  forward.z = 0 ; right.z = 0 ; normalize (БЕЗУСЛОВНО, в отличие от WalkMove)
 3  wishvel.xy = forward.xy*fmove + right.xy*smove ; wishvel.z = 0
 4  wishdir = normalize(wishvel) ; wishspeed = длина
 5  clamp wishspeed до m_flMaxSpeed
 6  AirAccelerate(wishdir, wishspeed, sv_airaccelerate)
 7  vel += baseVelocity ; TryPlayerMove() ; vel -= baseVelocity
```
Воздушного cap-а скорости **нет** (в отличие от WalkMove шага 9).

### 3.6 `TryPlayerMove` (gamemovement.cpp:2850) — slide solver

```
numbumps = 4 ; blocked = 0 ; numplanes = 0
original_velocity = primal_velocity = vel
allFraction = 0 ; time_left = frametime ; new_velocity = 0

for bumpcount in 0..3:
  if (|vel| == 0) break
  end = origin + time_left * vel
  pm = TracePlayerBBox(origin → end)            // либо reuse pFirstTrace если end == *pFirstDest
  if (0 < pm.fraction < 1e-4) pm.fraction = 0                          ◀── Q11
  allFraction += pm.fraction
  if (pm.allsolid) { vel = 0 ; return 4 }
  if (pm.fraction > 0):
        if (pm.fraction == 1):
            stuck = TracePlayerBBox(pm.endpos → pm.endpos)
            if (stuck.startsolid || stuck.fraction != 1) { vel = 0 ; break }   ◀── Q12
        origin = pm.endpos ; original_velocity = vel ; numplanes = 0
  if (pm.fraction == 1) break
  AddToTouched(pm, vel)
  if (pm.plane.normal.z > 0.7)                 blocked |= 1            // floor
  if (|pm.plane.normal.z| < 1e-4) { pm.plane.normal.z = 0 ; blocked |= 2 }  ◀── Q14
  time_left -= time_left * pm.fraction
  if (numplanes >= 5) { vel = 0 ; break }                              ◀── MAX_CLIP_PLANES
  planes[numplanes++] = pm.plane.normal

  if (numplanes == 1 && movetype == WALK && groundEntity == NULL):      ◀── воздушная ветка Q13
        for i in planes:
             if (planes[i].z > 0.7) { ClipVelocity(original_velocity, planes[i], new_velocity, 1) ;
                                      original_velocity = new_velocity }
             else                    ClipVelocity(original_velocity, planes[i], new_velocity,
                                                  1 + sv_bounce*(1 - m_surfaceFriction))
        vel = new_velocity ; original_velocity = new_velocity
        // НЕТ проверки vel·primal_velocity <= 0
  else:
        for i in 0..numplanes-1:
             ClipVelocity(original_velocity, planes[i], vel, 1)
             for j in 0..numplanes-1: if (j != i && vel·planes[j] < 0) break
             if (j == numplanes) break                  // эта плоскость всех устраивает
        if (i != numplanes): /* уже лежит в vel */
        else:
             if (numplanes != 2) { vel = 0 ; break }                    ◀── Q16
             dir = normalize(planes[0] × planes[1]) ; vel = dir * (dir·vel)   // crease
        if (vel·primal_velocity <= 0) { vel = 0 ; break }               ◀── anti-oscillation

if (allFraction == 0) vel = 0                                          ◀── Q15
fLateralStoppingAmount = |primal_velocity|₂D - |vel|₂D
→ slam-звук при > 580 / > 1160
return blocked
```

### 3.7 `ClipVelocity` (gamemovement.cpp:3506)

```
angle = normal.z
blocked = 0 ; if (angle > 0) blocked |= 1 ; if (angle == 0) blocked |= 2   ◀── Q18
backoff = (in · normal) * overbounce
for i in 0..2: out[i] = in[i] - normal[i]*backoff
adjust = out · normal
if (adjust < 0) { adjust = MIN(adjust, -DIST_EPSILON) ; out -= normal * adjust }   ◀── Q17
```

### 3.8 `StepMove` (gamemovement.cpp:1758)

```
vecPos = origin ; vecVel = vel                       // сохранить
TryPlayerMove(&vecDestination, &trace)               // «прямая» попытка
vecDownPos = origin ; vecDownVel = vel
origin = vecPos ; vel = vecVel                       // откат

vecEndPos = origin ; if (m_bAllowAutoMovement) vecEndPos.z += stepSize + DIST_EPSILON
trace = TracePlayerBBox(origin → vecEndPos)
if (!trace.startsolid && !trace.allsolid) origin = trace.endpos         ◀── Q21
TryPlayerMove()                                      // «поднятая» попытка

vecEndPos = origin ; if (m_bAllowAutoMovement) vecEndPos.z -= stepSize + DIST_EPSILON
trace = TracePlayerBBox(origin → vecEndPos)
if (trace.plane.normal.z < 0.7) {                    ◀── Q19 (включая «не попали» → normal = 0)
     origin = vecDownPos ; vel = vecDownVel
     m_outStepHeight += max(0, origin.z - vecPos.z) ; return
}
if (!trace.startsolid && !trace.allsolid) origin = trace.endpos
vecUpPos = origin
flDownDist = (vecDownPos.xy - vecPos.xy)²            // только 2D!
flUpDist   = (vecUpPos.xy   - vecPos.xy)²
if (flDownDist > flUpDist) { origin = vecDownPos ; vel = vecDownVel }
else                       { vel.z = vecDownVel.z }                     ◀── Q20
m_outStepHeight += max(0, origin.z - vecPos.z)
```

### 3.9 `CategorizePosition` (gamemovement.cpp:4147)

```
 1  m_surfaceFriction = 1.0
 2  CheckWater()
 3  if (IsObserver()) return
 4  point = origin - (0,0,2) ; bumpOrigin = origin
 5  zvel = vel.z ; bMovingUp = zvel > 0 ; bMovingUpRapidly = zvel > 140
    if (bMovingUpRapidly && ground) bMovingUpRapidly = (zvel - groundVel.z) > 140
 6  bUnderwater = waterLevel >= WL_Eyes
 7  bMoveToEndPos = (movetype == WALK && ground != NULL && !bUnderwater)
    if (bMoveToEndPos) point.z -= m_flStepSize            // итого origin.z - 20
 8  if (bMovingUpRapidly || (bMovingUp && movetype == LADDER)):
        SetGroundEntity(NULL) ; bMoveToEndPos = false
    else:
        pm = TracePlayerBBox(bumpOrigin → point)
        flStandableZ = 0.7
        if (!CheckValidStandableGroundCandidate(pm, 0.7)):
             TracePlayerBBoxForGround(bumpOrigin → point, квадранты, overwriteEndpos = true)
             if (!CheckValidStandableGroundCandidate(pm, 0.7)):
                  SetGroundEntity(NULL)
                  if (vel.z > 0 && movetype != NOCLIP) m_surfaceFriction = 0.25   ◀── Q6
                  bMoveToEndPos = false
             else SetGroundEntity(&pm)
        else SetGroundEntity(&pm)
 9  if (bMoveToEndPos && !pm.startsolid && 0 < pm.fraction < 1) origin = pm.endpos
```

### 3.10 `CCSGameMovement::CheckParameters` (cs:169) — порядок

```
 1  IN_RAWDUCK (== IN_BULLRUSH) = snapshot IN_DUCK
 2  если фронт/спад IN_RAWDUCK → m_flDuckSpeed = MAX(0, m_flDuckSpeed - 2.0)
 3  auto-duck (боты) сбросы/установки IN_DUCK
 4  if (!DuckingEnabled()) buttons &= ~IN_DUCK
 5  m_bDuckOverride → buttons |= IN_DUCK
 6  walkButtonIsDown = IN_SPEED, runButtonIsDown = любая из WASD|IN_RUN
 7  если ducking в любой форме → walkButtonIsDown = false
 8  if (walkButtonIsDown && |v| < m_flMaxSpeed*0.52 + 25) { m_flMaxSpeed *= 0.52 ; m_bIsWalking = true }
 9  speed_squared = fmove² + smove² + upmove²
10  flSpeedFactor = surfaceData->maxSpeedFactor (иначе 1)
11  flSpeedFactor = min(flSpeedFactor, ComputeConstraintSpeedFactor())
12  if (FL_ONGROUND) flSpeedFactor *= m_flVelocityModifier
13  m_flMaxSpeed *= flSpeedFactor
14  if (stamina > 0) { s = clamp(1 - stamina/100,0,1) ; s *= s ; m_flMaxSpeed *= s }   ◀── Q36
15  if (speed_squared > m_flMaxSpeed²) { r = m_flMaxSpeed/sqrt(speed_squared) ;
                                         fmove *= r ; smove *= r ; upmove *= r }
16  FL_FROZEN|FL_ONTRAIN|dead → обнулить ввод
17  DecayViewPunchAngle() ; DecayAimPunchAngle()
18  v_angle = m_vecAngles + viewPunchAngle ; ROLL = CalcRoll(...) ; PITCH/YAW копируются
19  YAW = AngleNormalize(YAW)
20  player-stacking: если сложены > 1 уровня → fmove = m_flMaxSpeed*3, smove = 0, buttons = 0  ◀── Q37
21  m_iMoveState = IDLE/RUN/WALK по комбинации кнопок
```
**`m_flClientMaxSpeed` в CS-версии НЕ используется** (база делала `MIN`, cs-override — нет) — Q34.

### 3.11 `CCSGameMovement::Accelerate` (cs:1251) — живой путь

`SV_ACCELERATE_EXPONENT_TIME == 0` (cs:42) ⇒ `flZeroToMaxSpeedTime == 0` ⇒ вся экспоненциальная
ветка (cs:1335-1378) — **мёртвый код**. Живой путь:

```
if (!CanAccelerate()) return
flStoredAccel = accel                               // = sv_accelerate = 5.5
currentspeed  = vel · wishdir
addspeed      = wishspeed - currentspeed
if (addspeed <= 0) return
bIsDucking = (buttons & IN_DUCK) || m_bDucking || FL_DUCKING
bIsWalking = (buttons & IN_SPEED) && !bIsDucking
flMaxSpeed = 250                                    // ХАРДКОД, не sv_maxspeed
fAccelerationScale = MAX(250, wishspeed)
flGoalSpeed        = fAccelerationScale
if (sv_accelerate_use_weapon_speed && weapon):
      bIsSlowSniperScoped = zoom > 0 && zoomLevels > 1 && weaponMax*0.52 < 110
      flGoalSpeed *= MIN(1, weaponMax/250)
      if ((!duck && !walk) || ((walk||duck) && slowSniper))
            fAccelerationScale *= MIN(1, weaponMax/250)
if (bIsDucking) { if (!slowSniper) fAccelerationScale *= 0.34 ; flGoalSpeed *= 0.34 }
if (bIsWalking) { if (!slowSniper) fAccelerationScale *= 0.52 ; flGoalSpeed *= 0.52 }
if (bIsWalking && currentspeed > flGoalSpeed - 5)
      flStoredAccel *= clamp(1 - MAX(0, currentspeed - (flGoalSpeed-5)) / 5, 0, 1)   ◀── Q40
accelspeed = flStoredAccel * frametime * fAccelerationScale * m_surfaceFriction
if (accelspeed > addspeed) accelspeed = addspeed
vel += accelspeed * wishdir
m_flGroundAccelLinearFracLastTime = curtime
── далее m_vecTrailingVelocity bookkeeping (влияет только на weapon accuracy fishtail)
```

Численный пример (без оружия, `m_surfaceFriction = 1`, 64 tick):
`fAccelerationScale = MAX(250, wishspeed)`. При удержании W wishspeed равен `mv->m_flMaxSpeed = 260`
(команда 450 урезана в `CheckParameters`), поэтому масштаб — **260, а не литерал 250**:
`accelspeed = 5.5 * (1/64) * 260 * 1 = 22.34375` u/tick.
Литерал 250 становится нижней границей только при `wishspeed < 250` (walk, duck, аналоговый стик):
например при walk (`wishspeed = 260*0.52 = 135.2`) масштаб = `MAX(250, 135.2) = 250`, далее ×0.52.

### 3.12 `AirAccelerate` (gamemovement.cpp:1960)

```
wishspd = MIN(wishspeed, 30)                        ◀── cap ТОЛЬКО для бюджета
if (deadflag || m_flWaterJumpTime) return
currentspeed = vel · wishdir
addspeed = wishspd - currentspeed
if (addspeed <= 0) return
accelspeed = sv_airaccelerate * wishspeed * frametime * m_surfaceFriction   ◀── ПОЛНЫЙ wishspeed
if (accelspeed > addspeed) accelspeed = addspeed
vel        += accelspeed * wishdir
m_outWishVel += accelspeed * wishdir
```
Численно при `wishspeed = 260`, `m_surfaceFriction = 1`, 64 tick:
`accelspeed = 12*260/64 = 48.75`, но `addspeed <= 30` ⇒ фактический прирост ≤ 30 u/tick
и он всегда направлен по `wishdir`. Это и есть механика air-strafe.
В окне `0 < vel.z <= 140` (см. уточнение Q6) `m_surfaceFriction = 0.25` ⇒ `accelspeed = 12.1875`,
и тогда прирост ограничен уже им, а не бюджетом 30. При `vel.z > 140` и на спуске friction = 1.0.

### 3.13 `Friction` (gamemovement.cpp:1865)

```
if (m_flWaterJumpTime) return
speed = |vel|                                       // 3D, но vel.z уже = 0
if (speed < 0.1) return                             // велосити НЕ меняется
drop = 0
if (groundEntity != NULL):
      friction = sv_friction * m_surfaceFriction
      control  = (speed < sv_stopspeed) ? sv_stopspeed : speed       // PC-путь
      drop += control * friction * frametime
newspeed = speed - drop ; if (newspeed < 0) newspeed = 0
if (newspeed != speed) { newspeed /= speed ; vel *= newspeed }
m_outWishVel -= (1 - newspeed) * vel                ◀── Q4 (newspeed тут либо ratio, либо абс.)
```
Численно при `speed <= 80`: `drop = 80 * 5.2/64 = 6.5` u/tick.

---

## 4. COLLISION MODEL

### 4.1 Форма игрока

**Только AABB.** В предоставленном коде **нет** цилиндрического представления игрока.
Источник размеров — `g_CSViewVectors` (cs_gamerules.cpp:212-226):

| Состояние | mins | maxs | высота | eye (view offset) |
|---|---|---|---|---|
| Standing | `(-16,-16,0)` | `(16,16,72)` | 72 | `(0,0,64)` |
| Ducked | `(-16,-16,0)` | `(16,16,54)` | 54 | `(0,0,46)` |
| Observer | `(-10,-10,-10)` | `(10,10,10)` | 20 | — |
| Dead | — | — | — | `(0,0,14)` |

Свойства:
* Origin — в **центре нижней грани** (mins.z == 0). Hull **не вращается** по yaw.
* `VEC_DUCK_HULL_MIN == VEC_HULL_MIN` ⇒ `hullMinDelta = (0,0,0)` ⇒ **на земле duck/unduck
  не сдвигает origin вовсе**; меняется только верхняя грань (72 ↔ 54).
* Выбор hull в `TracePlayerBBox` идёт по `player->m_Local.m_bDucked` (gamemovement.cpp:900-927),
  **не** по `FL_DUCKING` и **не** по `m_flDuckAmount`.
* В воздухе CS сдвигает origin на **половину** разницы высот: `0.5 * (72 - 54) = 9` (Q29) —
  центр AABB сохраняется, ступни поднимаются на 9, голова опускается на 9.
* Одна и та же форма для standing / jumping / falling / ground / air — различие только ducked/нет.
* `TracePlayerBBoxForGround` (gamemovement.cpp:4049) дополнительно трассирует **4 квадрантные
  под-box'а** (половинки AABB) — ближайший аналог «скруглённого» зондирования пола.

### 4.2 Запросы

| Source | Семантика | Маска |
|---|---|---|
| `TracePlayerBBox(start,end,mask,group,pm)` | `Ray_t::Init(start,end,mins,maxs)` → `enginetrace->TraceRay` | `PlayerSolidMask()` |
| `TestPlayerPosition(pos,group,pm)` | нуль-длины трасса (`pos → pos`) | `PlayerSolidMask()` |
| `GameMovementTraceHull(...)` | произвольные mins/maxs | параметр |
| `enginetrace->GetPointContents(p, MASK_WATER)` | точечный contents для воды | `MASK_WATER` |

`PlayerSolidMask` (CS, cs:130): `player->PhysicsSolidMaskForEntity()`, т.е.
`MASK_PLAYERSOLID = CONTENTS_SOLID|MOVEABLE|PLAYERCLIP|WINDOW|MONSTER|GRATE`
плюс `CONTENTS_TEAM1`/`TEAM2` если `!IsTeammateSolid()` (cs_player_shared.cpp:2544).
Для ботов добавляется `CONTENTS_MONSTERCLIP`.

Фильтр `CTraceFilterSkipTwoEntitiesAndCheckTeamMask` (gamemovement.cpp:98-134): пропускает
себя, дружественные снаряды и сущности с тем же team-mask при
`COLLISION_GROUP_PLAYER_MOVEMENT`.

### 4.3 Поля `trace_t`, от которых зависит физика

`fraction`, `startsolid`, `allsolid`, `endpos`, `plane.normal`, `contents`,
`surface.surfaceProps`, `m_pEnt` (+`IsPlayer()`, `DidHitWorld()`), `DidHit()`.

`DIST_EPSILON = 0.03125` (coordsize.h:35) — **движок** отодвигает `endpos` от плоскости на эту
величину внутри `CM_ClipBoxToBrush`. Это поведение **не находится в игровом коде** ⇒ его надо
воспроизвести в нашем query-слое (см. `UNKNOWN-5`).

### 4.4 Пороговые значения плоскостей

| Порог | Значение | Где |
|---|---|---|
| Standable ground | `normal.z >= 0.7` | gamemovement.cpp:4229, 4142 |
| «Floor» в `TryPlayerMove` | `normal.z > 0.7` | :2980 |
| «Wall/step» в `TryPlayerMove` | `abs(normal.z) < 0.0001` → принудительно `z = 0` | :2986 |
| «Floor» в `ClipVelocity` | `normal.z > 0` | :3516 |
| «Wall» в `ClipVelocity` | `normal.z == 0` | :3518 |
| `StayOnGround` | `normal.z >= 0.7` | :2135 |
| `StepMove` down-trace | `normal.z < 0.7` → отказ | :1819 |
| Квадрант `(-x,+y)` | **хардкод `0.7`** вместо параметра | :4096 (Q8) |

`0.7` ⇒ walkable angle = `acos(0.7) ≈ 45.573°`. Никакого `WalkableFloorAngle` нет —
только это число, и оно **не конфигурируемо**.

---

## 5. IDENTIFIED QUIRKS / BUGS (подлежат воспроизведению)

Каждый пункт = будущий regression-тест `REGRESSION-0XX`.

### Gravity / Jump

| ID | Описание | Ссылка |
|---|---|---|
| **Q1** | На тике прыжка `FinishGravity` вызывается **дважды**: внутри `CheckJumpButton` и в конце `FullWalkMove`. Итого за тик прыжка `-1.5·g·dt` по Z вместо `-1.0·g·dt`. | cs:767 + gamemovement.cpp:2386 |
| **Q2** | `startz` в `CheckJumpButton` берётся **после** `StartGravity`, т.е. уже содержит `-0.5·g·dt`; это попадает в stamina-расчёт. | cs:741 |
| **Q3** | CS пишет дельту прыжка в `m_outWishVel.z` (база — в `m_outJumpVel.z`) и передаёт её в `OnJump` ⇒ stamina считается от `m_outWishVel.z`. | cs:769-772 vs gamemovement.cpp:2786-2789 |
| **Q45** | Прыжок в ducked-состоянии **перезаписывает** `vel.z` импульсом, уничтожая накопленную вертикальную скорость; стоя — **добавляет**. | cs:746-760 |
| **Q44** | Прыжок с головы **падающего** игрока даёт `vel.z = 0` (никакого импульса). | cs:742-745 |
| **Q46** | Stamina скейлит `vel.z` **после** импульса, линейно: `*= clamp(1 - stamina/100, 0, 1)`. | cs:762-765 |
| **Q42** | `PreventBunnyJumping` клэмпит **3D**-модуль скорости до `1.1 * player->m_flMaxspeed` (латч = 260 ⇒ 286), а не до per-tick `mv->m_flMaxSpeed`. | cs:603-622 |
| **Q43** | Прыжок требует `groundEntity != NULL`; повтор блокируется `m_nOldButtons & IN_JUMP`, если `sv_autobunnyhopping == 0`. | cs:696-703 |
| **Q58** | Лестница делает `SetGravity(0)`; далее `if (player->GetGravity())` — ложь ⇒ `ent_gravity = 1.0`. Т.е. «нулевая» гравитация фактически = 1.0. | :3273 + :1494 |

### Acceleration / Friction

| ID | Описание | Ссылка |
|---|---|---|
| **Q5** | `AirAccelerate`: бюджет ограничен `wishspd = min(wishspeed, 30)`, но `accelspeed` считается от **полного** `wishspeed`. Основа air-strafe. | :1975-1989 |
| **Q6** | `m_surfaceFriction = 0.25` при `airborne && vel.z > 0`; значение читается тиком позже (переносится через состояние).<br>**УТОЧНЕНО при выводе тестовых значений (Phase 4):** присваивание лежит внутри ветки `else`, т.е. после трассировок. При `vel.z > 140` берётся ранняя ветка `bMovingUpRapidly`, которая делает только `SetGroundEntity(NULL)` и **не трогает** friction, оставляя 1.0 со строки :4153. Поэтому окно ослабленного air-accel — ровно `0 < vel.z <= 140`: сразу после прыжка (≈289 u/s) ускорение полное, четверть включается только на последних ~11 тиках подъёма, на спуске снова 1.0. | :4217-4253 |
| **Q22** | `WalkMove` после `Accelerate` жёстко клэмпит `|vel|` до `m_flMaxSpeed` ⇒ на земле скорость принципиально не накапливается. | :2220-2224 |
| **Q23** | `WalkMove`: при `spd < 1.0` скорость обнуляется и `StayOnGround()` **не вызывается**. | :2231-2237 |
| **Q4** | `Friction`: `m_outWishVel -= (1 - newspeed) * vel`, где `newspeed` — отношение только если оно изменилось; иначе это абсолютная скорость. | :1923-1931 |
| **Q38** | Вся экспоненциальная ветка `Accelerate` мертва (`SV_ACCELERATE_EXPONENT_TIME == 0`). | cs:42, 1335 |
| **Q39** | `if (currentspeed < 0) currentspeed = 0` влияет только на мёртвую ветку; `addspeed` использует незаклампленное значение. | cs:1268 |
| **Q40** | Walk-таперинг делит на константу 5: `flGoalSpeed - (flGoalSpeed - 5)`. | cs:1326 |
| **Q41** | Масштаб ускорения зависит от `GetMaxSpeed()` **активного оружия** (`sv_accelerate_use_weapon_speed 1`), независимо от `m_flMaxSpeed`. | cs:1287-1296 |
| **Q35** | Walk-кап 0.52 применяется только если `|v| < m_flMaxSpeed*0.52 + 25` (гистерезис). | cs:236-245 |
| **Q36** | Stamina-скейл maxspeed **квадратичный**. | cs:283-288 |
| **Q34** | CS-`CheckParameters` потерял базовый `MIN(m_flClientMaxSpeed, m_flMaxSpeed)` ⇒ client maxspeed инертен. | сравнить :1152 и cs:169 |
| **Q49** | Клэмп ввода считает 3-вектор `(fmove, smove, upmove)` ⇒ ненулевой `upmove` урезает горизонтальный ввод. | cs:257-311 |
| **Q37** | Анти-стек: при >1 уровне игроков поверх друг друга — `fmove = maxspeed*3`, `buttons = 0`. | cs:364-384 |

### Collision

| ID | Описание | Ссылка |
|---|---|---|
| **Q11** | `0 < fraction < 1e-4` принудительно → `0`. | :2913-2917 |
| **Q12** | При `fraction == 1` делается повторная нуль-длины трасса в `endpos`; если «застрянем» — `vel = 0` и `break` (позиция при этом уже обновлена не была). | :2936-2950 |
| **Q13** | Воздушная ветка при `numplanes == 1` использует другой путь клиппинга и **пропускает** и multi-plane-проверку, и anti-oscillation `vel·primal <= 0`. | :3017-3037 |
| **Q14** | `abs(normal.z) < 1e-4` ⇒ `normal.z := 0` — мутирует плоскость, которая затем кладётся в `planes[]`. | :2986-2990 |
| **Q15** | `allFraction == 0` ⇒ `vel = 0` (даже если блокировка была «мягкой»). | :3091-3094 |
| **Q16** | Crease-режим работает **только** при `numplanes == 2`; при 3+ — `vel = 0`. | :3064-3075 |
| **Q17** | `ClipVelocity` anti-penetration выталкивает наружу минимум на `DIST_EPSILON`: `adjust = MIN(adjust, -DIST_EPSILON)` — при почти параллельных плоскостях это **добавляет** скорость. | :3532-3539 |
| **Q18** | `blocked`-флаги в `ClipVelocity` используют пороги `> 0` / `== 0`, а `TryPlayerMove` — `> 0.7` / `< 1e-4`. Два разных определения «пола»/«стены». | :3516 vs :2980 |
| **Q19** | `StepMove` down-trace: «не попали» ⇒ `plane.normal == (0,0,0)` ⇒ `normal.z = 0 < 0.7` ⇒ откат к прямой попытке. Зависит от zero-init плоскости. | :1819 |
| **Q20** | Выбор up/down по **2D**-дистанции; при победе «up» берётся только `vel.z` из down-попытки, а `vel.xy` остаётся от up-попытки. | :1842-1853 |
| **Q21** | Результат step-up применяется только при `!startsolid && !allsolid`; иначе движение продолжается с неподнятой позиции, но `TryPlayerMove` всё равно вызывается. | :1800-1803 |
| **Q24** | Reuse первой трассы в `TryPlayerMove` гейтится **точным** сравнением `end == *pFirstDest` (float). | :2891 |
| **Q8** | Квадрант `(-x,+y)` в `TracePlayerBBoxForGround` сравнивает с литералом `0.7` вместо `minGroundNormalZ`. | :4096 |
| **Q9** | `TracePlayerBBoxForGround` возвращает `plane`/`m_pEnt`/`surface` от **под-box**-трассы, но `fraction`/`endpos` — от исходной полной трассы. Ground entity и surfaceFriction могут быть «не оттуда», куда мы приснэпились. | :4058-4125 |
| **Q10** | `CheckValidStandableGroundCandidate` возвращает `true` для игроков при **любом** нормале. | :4138-4139 |
| **Q25** | Ground-трасса удлиняется на `stepSize` и `origin` снэпится в `pm.endpos` ⇒ сход с уступа ≤ 20 units не переводит в airborne, а «приклеивает» к полу. | :4207-4214, :4313-4329 |
| **Q26** | Два разных порога «точно в воздухе»: `250` в `PlayerMove` (:5043) и `140` в `CategorizePosition` (:4183). | — |
| **Q27** | `StayOnGround` — **второй** snap-down после `WalkMove`, с гейтом `flDelta > 0.5*COORD_RESOLUTION (= 0.015625)`. | :2114-2145 |
| **Q51** | `vel.z` обнуляется дважды за тик на земле (перед `Friction` и после `FinishGravity`). | :2359, :2392 |
| **Q52** | `SetGroundEntity(pm != NULL)` обнуляет `vel.z` **внутри** `CategorizePosition`. | :4023-4025 |
| **Q53** | `m_flFallVelocity` латчится в начале `PlayerMove` (`-vel.z`, только если airborne) ⇒ landing damage/stamina считаются от скорости на **начало** тика посадки. | :5053-5056 |
| **Q50** | `CheckStuck` на клиенте отключён (`cl_pred_checkstuck 0`) ⇒ намеренная server/client-расхождение. | :3771 |
| **Q56** | `FullObserverMove` масштабирует `wishvel` на `mv->m_flMaxSpeed/wishspeed`, хотя `wishspeed` присваивает `maxspeed` — несогласованность. | :2484-2488 |

### Duck

| ID | Описание | Ссылка |
|---|---|---|
| **Q28** | `m_bDucked = false` ставится **в начале** unduck-интерполяции, тогда как `FL_DUCKING` снимается только при `duckAmount <= 0.75`. Collision hull становится 72-unit, а accuracy-бонус ещё держится. | cs:1157, 1172 |
| **Q29** | В воздухе сдвиг origin = **половина** разницы высот (9 units), в базовом Source — полная (18). | cs:877/905/941 vs :4512 |
| **Q30** | `!playerTouchingGround` ⇒ немедленный `FinishDuck()` независимо от `m_flDuckAmount` (crouch-jump). | cs:1124 |
| **Q31** | Анти-спам: `-2.0` к `m_flDuckSpeed` на **каждый фронт и спад** IN_DUCK; восстановление `Approach(8.0, …, dt*3)`, ускоренное до `dt*6` если отошёл > 64 units от `m_vecLastPositionAtFullCrouchSpeed`. При `m_flDuckSpeed < 1.5` кнопка IN_DUCK игнорируется целиком. | cs:187, 153, 1070-1090 |
| **Q32** | `sv_timebetweenducks = 0.4` — лок-аут повторного приседания после завершённого duck. | cs:159-161 |
| **Q33** | Скорость приседания `m_flDuckSpeed * 0.8`; вставания — `MAX(1.5, m_flDuckSpeed)` (т.е. вставание всегда ≥ 1.5). Defusing умножает обе на `0.4`. | cs:1115, 1150, 1119, 1154 |
| **Q54** | `HandleDuckingSpeedCrop` вызывается **в конце** `Duck()`, т.е. **после** клэмпа ввода в `CheckParameters`, и умножает как ввод, так и `m_flMaxSpeed`. | cs:1230, 829-848 |
| **Q55** | `m_iSpeedCropped` гарантирует, что duck-crop применяется не более одного раза за команду. | gamemovement.h:29, cs:835 |
| **Q47** | `ReduceTimers` усекает `(int)(1000*frametime)` = 15 при 1/64 ⇒ msec-таймеры теряют 0.625 ms/тик (legacy; CS-Duck их не использует, но `m_nDuckJumpTimeMsecs` читается в `UpdateDuckJumpEyeOffset`). | :1246-1247 |

### Прочее

| ID | Описание | Ссылка |
|---|---|---|
| **Q7** | При `sv_optimizedmovement 1` и `MOVETYPE_WALK` ведущий `CategorizePosition` **пропускается** ⇒ ground state на входе в тик — прошлотиковый; единственная проверка `vel.z > 250`. | :5035-5047 |
| **Q48** | `CheckVelocity` клэмпит **по каждой оси** до ±3500, не по модулю ⇒ диагональ может достигать 6062. | :3437-3446 |
| **Q57** | Лестница: `sv_ladder_scale_speed 0.78` масштабирует итоговую скорость; прыжок с лестницы = `270 * ladderNormal`; `m_ignoreLadderJumpTime = 0.2 s`. | :3290-3299, :3354-3360 |

---

## 6. REQUIRED CONSTANTS

### 6.1 Физические ConVar (movevars_shared.cpp)

| Имя | Значение | Примечание |
|---|---|---|
| `sv_gravity` | `800` | `DEFAULT_GRAVITY_STRING` не-HL2 ветка (:18) |
| `sv_friction` | `5.2` | :44 |
| `sv_stopspeed` | `80` | :23 |
| `sv_accelerate` | `5.5` | :31 |
| `sv_airaccelerate` | `12` | :37 |
| `sv_maxspeed` | `320` | :29 — верхняя граница `GetPlayerMaxSpeed` |
| `sv_maxvelocity` | `3500` | :47, per-axis |
| `sv_bounce` | `0` | :46 |
| `sv_stepsize` | `18` | :52 → `m_Local.m_flStepSize` (baseplayer_shared.cpp:2418) |
| `sv_wateraccelerate` | `10` | :38 (в `WaterMove` фактически используется `sv_accelerate`) |
| `sv_waterfriction` | `1` | :39 (не используется в `WaterMove`) |
| `sv_rollangle` | `0` | :42 ⇒ `CalcRoll` всегда даёт 0 |
| `sv_rollspeed` | `200` | :41 |
| `sv_noclipspeed` / `sv_noclipaccelerate` | `5` / `5` | :25, :24 |
| `sv_specspeed` / `sv_specaccelerate` / `sv_specnoclip` | `3` / `5` / `1` | :27, :26, :28 |
| `sv_backspeed` | `0.6` | :56 — **не используется** в движении |

### 6.2 CS-специфичные ConVar (cs_gamemovement.cpp)

| Имя | Значение | Ссылка |
|---|---|---|
| `sv_jump_impulse` | `301.993377` | :49 (`sqrt(2*800*57)`) |
| `sv_staminajumpcost` | `0.080` | :28 |
| `sv_staminalandcost` | `0.050` | :29 |
| `sv_staminarecoveryrate` | `60` u/s | :30 |
| `sv_staminamax` | `80` | :31 |
| `sv_enablebunnyhopping` | `0` | :596 |
| `sv_autobunnyhopping` | `0` | :597 |
| `sv_timebetweenducks` | `0.4` | :36 |
| `sv_accelerate_use_weapon_speed` | `1` | movevars_shared.cpp:33 |
| `sv_ladder_dampen` | `0.2` | :3142 |
| `sv_ladder_angle` | `-0.707` | :3143 |
| `sv_ladder_scale_speed` | `0.78` | :3144 |
| `sv_extreme_strafe_accuracy_fishtail` | `0` | :46 |
| `sv_optimizedmovement` | `1` | gamemovement.cpp:4989 |
| `cl_pred_checkstuck` | `0` | gamemovement.cpp:3758 |

### 6.3 Compile-time константы

| Имя | Значение | Ссылка |
|---|---|---|
| `TICK_INTERVAL` | `gpGlobals->interval_per_tick`; default PC `1/64 = 0.015625` | const.h:29 — см. `UNKNOWN-1` |
| `STOP_EPSILON` | `0.1` | gamemovement.cpp:32 (объявлен, **не используется**) |
| `MAX_CLIP_PLANES` | `5` | gamemovement.cpp:33 |
| `MINIMUM_MOVE_FRACTION` | `0.0001f` | gamemovement.cpp:86 |
| `EFFECTIVELY_HORIZONTAL_NORMAL_Z` | `0.0001f` | gamemovement.cpp:87 |
| `DIST_EPSILON` | `0.03125` | coordsize.h:35 |
| `COORD_RESOLUTION` | `1/32 = 0.03125` | coordsize.h:19 (`COORD_FRACTIONAL_BITS = 5`) |
| `numbumps` | `4` | gamemovement.cpp:2865 |
| `NON_JUMP_VELOCITY` | `140.0f` | gamemovement.cpp:4183 |
| порог airborne в `PlayerMove` | `250.0f` | gamemovement.cpp:5043 |
| `flStandableZ` | `0.7` | gamemovement.cpp:4229 |
| `flOffset` (ground trace) | `2.0f` | gamemovement.cpp:4171 |
| `GAMEMOVEMENT_DUCK_TIME` | `1000` ms | gamemovement.h:22 |
| `GAMEMOVEMENT_JUMP_TIME` | `510` ms | gamemovement.h:23 |
| `GAMEMOVEMENT_JUMP_HEIGHT` | `21.0f` | gamemovement.h:24 |
| `TIME_TO_DUCK_MSECS` | `200` (CSTRIKE15) | shareddefs.h:96 |
| `TIME_TO_UNDUCK_MSECS` | `200` | shareddefs.h:104 |
| `MAX_CLIMB_SPEED` | `200` | shareddefs.h:92 |
| `WATERJUMP_HEIGHT` | `8` | shareddefs.h:90 |
| `PLAYER_FATAL_FALL_SPEED` | `1024` | shareddefs.h:430 |
| `PLAYER_MAX_SAFE_FALL_SPEED` | `580` | shareddefs.h:431 |
| `PLAYER_LAND_ON_FLOATING_OBJECT` | `200` | shareddefs.h:432 |
| `PLAYER_MIN_BOUNCE_SPEED` | `200` | shareddefs.h:433 |
| `PLAYER_FALL_PUNCH_THRESHOLD` | `350.0f` | shareddefs.h:434 |
| `CATEGORIZE_GROUND_SURFACE_INTERVAL` | `0.3 s` | gamemovement.cpp:68 |
| `CHECK_STUCK_INTERVAL` / `_SP` | `1.0 s` / `0.2 s` | gamemovement.cpp:71, 74 |
| `CHECKSTUCK_MINTIME` | — | `UNKNOWN-8` (в `CheckStuck`, :3840; определение вне прочитанных файлов) |
| `surfaceFriction` scale | `*1.25`, clamp `<= 1.0` | gamemovement.cpp:1084-1086 |
| default surface friction | `0.8` ⇒ `m_surfaceFriction = 1.0` | vphysics/physics_material.cpp:609 |

### 6.4 CS player speed константы (cs_shareddefs.cpp)

| Имя | Значение |
|---|---|
| `CS_PLAYER_SPEED_RUN` | `260.0f` |
| `CS_PLAYER_SPEED_VIP` | `227.0f` |
| `CS_PLAYER_SPEED_SHIELD` | `160.0f` |
| `CS_PLAYER_SPEED_HAS_HOSTAGE` | `200.0f` |
| `CS_PLAYER_SPEED_STOPPED` | `1.0f` |
| `CS_PLAYER_SPEED_OBSERVER` | `900.0f` |
| `CS_PLAYER_SPEED_DUCK_MODIFIER` | `0.34f` |
| `CS_PLAYER_SPEED_WALK_MODIFIER` | `0.52f` |
| `CS_PLAYER_SPEED_CLIMB_MODIFIER` | `0.34f` |
| `CS_PLAYER_DUCK_SPEED_IDEAL` | `8.0f` (init `m_flDuckSpeed`, player.cpp:719) |
| `STAMINA_RANGE` | `100.0` (cs_gamemovement.cpp:27) |
| `get_sv_crouch_spam_penalty` | `2.0f` (cs_gamemovement.cpp:33) |
| `BUNNYJUMP_MAX_SPEED_FACTOR` | `1.1f` (cs_gamemovement.cpp:600) |
| `kVelocityRecoveryRate` | `1/2.5` (cs_gamemovement.cpp:465) |
| `player->m_flMaxspeed` (латч) | `260` (`SetMaxSpeed(CS_PLAYER_SPEED_RUN)`, cs_player.cpp:1490) |

### 6.5 Input (in_main.cpp)

| Имя | Значение |
|---|---|
| `cl_forwardspeed` / `cl_sidespeed` / `cl_backspeed` | `450` (`MAX_LINEAR_SPEED`, :58) |
| `cl_upspeed` | `320` (:51) |
| `ScaleMovements` | **no-op** (`return;` в начале, :1166) |

⇒ `forwardmove, sidemove ∈ {-450, 0, +450}` на клавиатуре; клэмп до `m_flMaxSpeed` —
исключительно в `CheckParameters`.

### 6.6 UNKNOWN (не выводится из прочитанного кода)

| ID | Что нужно | Где искать | Валидирующий тест |
|---|---|---|---|
| **UNKNOWN-1** | целевой tickrate: `gpGlobals->interval_per_tick` (64 или 128) | запуск сервера / `-tickrate`; default `const.h:29` = 1/64 | `host_timescale 0; status` или печать `interval_per_tick` |
| **UNKNOWN-2** | таблица `surfaceproperties*.txt`: `friction`, `jumpFactor`, `maxSpeedFactor`, `climbable`, `material` | в репозитории **отсутствует** (в VPK игры) | `developer 1` + печать `m_surfaceFriction` стоя на материале |
| **UNKNOWN-3** | `GetMaxSpeed()` по оружиям (влияет и на `m_flMaxSpeed`, и на `fAccelerationScale`) | `cs_weapon_parse.cpp` + weapon scripts (в VPK) | замер терминальной скорости с каждым оружием |
| **UNKNOWN-4** | при каких событиях `m_flVelocityModifier` падает ниже 1.0 | не найдено в прочитанном; default `1.0f` (cs_player.cpp:1530) | стрельба по игроку + печать |
| **UNKNOWN-5** | точная epsilon-семантика `enginetrace->TraceRay` для box-vs-brush / box-vs-displacement (`DIST_EPSILON` back-off, `startsolid`/`allsolid` условия, обработка non-axial brush-ов) | движок (`engine/`), вне игрового кода | дифф-тест: подвести hull к плоскости и сравнить `endpos` |
| **UNKNOWN-6** | `IsGameConsole()` — принимаем `false` (PC). Влияет на `Friction` (`control = stopspeed*2` для нестоящего на консоли) | `tier0` | — |
| **UNKNOWN-7** | `CSGameRules()->IsTeammateSolid()` (`mp_solid_teammates`) | `cs_gamerules` | — |
| **UNKNOWN-8** | `CHECKSTUCK_MINTIME` и таблица `CreateStuckTable`/`GetRandomStuckOffsets` | `gamemovement.cpp` ~3600-3730 (не дочитан) + `player.h` | — |
| **UNKNOWN-9** | `GetLaggedMovementValue()` (множитель `frametime`) — предполагаем `1.0` | `player.h` | — |

> Ни одно из этих значений **не будет выдумано**. До уточнения код будет содержать
> `static_assert`/`checkf` и конфигурационные поля со значением-сентинелом.

---

## 7. PROPOSED UE5.6 ARCHITECTURE

### 7.1 Файловая структура

```
Source/
└── SourceMovement/                         // отдельный Runtime-модуль
    ├── SourceMovement.Build.cs             // зависимости: Core, CoreUObject, Engine, PhysicsCore
    │
    ├── Public/
    │   ├── SourceMovementModule.h
    │   │
    │   ├── Core/                           // ЗОНА БЕЗ UOBJECT — чистая физика
    │   │   ├── SourceScalar.h              // using srcfloat = float; strict-FP helpers
    │   │   ├── SourceVector.h              // FSrcVec3 (3×float), VectorNormalize→len, Dot, Cross,
    │   │   │                               // Length, Length2D, LengthSqr — порт mathlib семантики
    │   │   ├── SourceAngles.h              // FSrcAngles (pitch,yaw,roll) + AngleVectors + AngleNormalize
    │   │   ├── SourceMathCompat.h          // SimpleSpline, Approach, clamp — вербатим-порт
    │   │   ├── SourceMovementTypes.h       // буттоны, MoveType, WaterLevel, FSrcGroundRef
    │   │   ├── SourceTraceResult.h         // FSourceTraceResult
    │   │   ├── ISourceWorldQuery.h         // интерфейс трассировок/contents/surfaceprops
    │   │   ├── SourcePlayerBounds.h        // FSourcePlayerHull (mins/maxs + выбор по bDucked)
    │   │   ├── SourceMovementParams.h      // FSourceMovementParams — все константы §6
    │   │   ├── SourceMovementState.h       // FSourceMovementState, FSourceMoveData, FSourceUserCmd
    │   │   └── SourceMovementSim.h         // FSourceMovementSim — порт CGameMovement+CCSGameMovement
    │   │
    │   ├── World/
    │   │   ├── SourceWorldQuery_UE.h       // бэкенд на Chaos sweep (FCollisionShape::MakeBox)
    │   │   ├── SourceWorldQuery_Analytic.h // бэкенд на Source-семантике (brush-планы) для тестов
    │   │   └── SourceWorldQuery_Flat.h     // синтетический мир для unit-тестов
    │   │
    │   ├── Actors/
    │   │   ├── SourcePlayerPawn.h          // ASourcePlayerPawn : APawn
    │   │   ├── SourceMovementComponent.h   // USourceMovementComponent : UPawnMovementComponent
    │   │   ├── SourceHullComponent.h       // USourceHullComponent : UBoxComponent (ТОЛЬКО как
    │   │   │                               //   query-target для чужих систем; наш sim его не читает)
    │   │   └── SourceViewComponent.h       // eye offset / view punch → камера
    │   │
    │   ├── Input/
    │   │   └── SourceInputTranslator.h     // Enhanced Input → FSourceUserCmd (cl_forwardspeed и т.п.)
    │   │
    │   ├── Net/
    │   │   ├── SourceMovementCommand.h     // FSourceMovementCommand (= FSourceUserCmd + seq)
    │   │   └── SourceMovementSnapshot.h    // FSourceMovementSnapshot + FSourceStateHash
    │   │
    │   └── Debug/
    │       ├── SourceMovementDebug.h       // CVars, DrawDebug, HUD-телеметрия
    │       └── SourceMovementRecorder.h    // FSourceMovementFrame, запись/загрузка трасс
    │
    └── Private/
        ├── ... (соответствующие .cpp)
        └── Tests/
            ├── SourceMovement_UnitTests.cpp        // accel, friction, gravity, jump, air-strafe
            ├── SourceMovement_CollisionTests.cpp   // planes, creases, stairs, slopes
            ├── SourceMovement_DuckTests.cpp
            ├── SourceMovement_ReplayTests.cpp      // golden traces
            └── SourceMovement_DiffTests.cpp        // против дампа из Kisak-Strike
```

### 7.2 Class hierarchy & ownership

```
ASourcePlayerPawn : APawn                       ← запрещённые классы не используются
 ├─ USceneComponent*            RootComponent   (НЕ capsule, НЕ физическое тело)
 ├─ USourceHullComponent*       HullProxy       (UBoxComponent, QueryOnly, только для других)
 ├─ USkeletalMeshComponent*     Mesh            (визуал; offset из eye/hull state)
 ├─ USourceViewComponent*       View            (камера = origin + viewOffset + punch)
 └─ USourceMovementComponent*   Movement   : UPawnMovementComponent
      ├─ FSourceMovementSim            Sim         (plain C++, владеет логикой)
      ├─ FSourceMovementState          State       (POD)
      ├─ FSourceMoveData               MoveData    (POD, per-tick)
      ├─ TUniquePtr<ISourceWorldQuery> Query       (UE-бэкенд в рантайме)
      ├─ const USourceMovementConfig*  Config  → FSourceMovementParams (снимок на старте тика)
      ├─ FSourceCommandBuffer          Commands    (история для replay/prediction)
      └─ FSourceMovementRecorder       Recorder    (опционально)
```

`USourceMovementConfig : UDataAsset` хранит все значения §6 как `UPROPERTY` (для тюнинга в
редакторе), но **снимок** `FSourceMovementParams` берётся один раз на команду — чтобы
симуляция была чистой функцией.

### 7.3 Deterministic simulation boundary

```cpp
// Чистая функция. Никакого UWorld, AActor, UObject.
void FSourceMovementSim::ProcessMovement(
        FSourceMovementState&        InOutState,
        FSourceMoveData&             InOutMove,
        const FSourceMovementParams& Params,
        ISourceWorldQuery&           World,
        srcfloat                     FrameTime);   // == TICK_INTERVAL
```

Внутри — имена 1:1 с Source (`PlayerMove`, `FullWalkMove`, `WalkMove`, `AirMove`,
`TryPlayerMove`, `StepMove`, `ClipVelocity`, `CategorizePosition`, `Duck`, …), тот же порядок,
те же промежуточные переменные. Каждое отклонение помечается:

```cpp
// SOURCE-COMPAT:
// Preserves <behavior> from <file:line>.
// Do not simplify without updating differential tests.
```

### 7.4 World query abstraction

```cpp
struct FSourceTraceResult
{
    bool      bDidHit        = false;   // == (Fraction < 1 || bStartSolid)
    bool      bStartSolid    = false;   // Source trace_t::startsolid
    bool      bAllSolid      = false;   // Source trace_t::allsolid
    bool      bValidPlane    = false;   // была ли реально записана плоскость
    srcfloat  Fraction       = 1.0f;
    FSrcVec3  StartPos       = FSrcVec3::Zero;
    FSrcVec3  EndPos         = FSrcVec3::Zero;   // позиция ORIGIN hull-а, не точка контакта
    FSrcVec3  PlaneNormal    = FSrcVec3::Zero;   // zero-init критичен (Q19)
    srcfloat  PlaneDist      = 0.0f;
    srcfloat  PenetrationDepth = 0.0f;
    int32     Contents       = 0;
    int32     SurfacePropsId = INDEX_NONE;
    FSrcEntityId EntityId    = FSrcEntityId::None();
    bool      bHitWorld      = false;
    bool      bHitPlayer     = false;
};

class ISourceWorldQuery
{
public:
    virtual ~ISourceWorldQuery() = default;

    virtual void TraceHull(const FSrcVec3& Start, const FSrcVec3& End,
                           const FSrcVec3& Mins,  const FSrcVec3& Maxs,
                           int32 ContentMask, const FSourceTraceFilter& Filter,
                           FSourceTraceResult& Out) = 0;

    virtual int32     GetPointContents(const FSrcVec3& Point, int32 Mask) = 0;
    virtual srcfloat  GetSurfaceFriction(int32 SurfacePropsId)   = 0;
    virtual srcfloat  GetSurfaceJumpFactor(int32 SurfacePropsId) = 0;
    virtual srcfloat  GetSurfaceMaxSpeedFactor(int32 SurfacePropsId) = 0;
    virtual FSrcVec3  GetEntityAbsVelocity(FSrcEntityId) = 0;
};
```

**РЕШЕНО: композитный бэкенд.** Статическая brush-геометрия обслуживается
Source-семантикой, динамика — Chaos:

```
SourceWorldQuery_Composite : ISourceWorldQuery
 ├─ SourceWorldQuery_Analytic   // статические brush-планы: порт CM_ClipBoxToBrush семантики
 │                              // → бит-совместимые fraction/endpos/normal/startsolid/allsolid
 └─ SourceWorldQuery_UE         // динамика, skeletal/complex meshes: Chaos box sweep
    TraceHull: трассируем оба, берём МЕНЬШУЮ fraction (как движок комбинирует
               world-trace и entity-list-trace в enginetrace->TraceRay);
               startsolid/allsolid — логическое ИЛИ.
```
Это прямо соответствует устройству `enginetrace->TraceRay`, который тоже объединяет
результат BSP-трассы и трассы по списку сущностей. Порядок комбинирования и правила
приоритета плоскости при равных `fraction` — часть поведения, фиксируется тестами.

`Sim` **никогда** не видит `FHitResult`. Преобразование живёт только в
`SourceWorldQuery_UE.cpp`:

```
FHitResult (Chaos sweep, box shape, FQuat::Identity)
    │  + DIST_EPSILON back-off policy
    │  + startsolid/allsolid маппинг (bStartPenetrating, bBlockingHit, Time==0)
    │  + hull-center offset (mins.z == 0 → центр на +36 / +27)
    │  + SourceSpace ↔ UE world transform
    ▼
FSourceTraceResult
```

### 7.5 Units & space

Один единственный слой (`SourceUnits.h`):

```cpp
namespace SourceUnits
{
    // РЕШЕНО: 1.0 → 1 Source unit == 1 UU. Игрок 72 UU высотой.
    // На границе с UE никакого умножения ⇒ отсутствует дополнительный источник float-шума
    // в трассировках. Геометрия уровней авторится в Source-единицах.
    // Константа остаётся именованной: при смене масштаба правится ОДНО место.
    inline constexpr double UnitsToUU = 1.0;

    FVector   ToUEPosition (const FSrcVec3&);   // (x, -y, z) * UnitsToUU
    FSrcVec3  ToSrcPosition(const FVector&);
    FVector   ToUEDirection(const FSrcVec3&);   // (x, -y, z), без масштаба
    FSrcVec3  ToSrcNormal  (const FVector&);
    void      ToUEBox(const FSrcVec3& Mins, const FSrcVec3& Maxs,
                      FVector& OutCenterOffset, FVector& OutExtent);
}
```

Вся математика физики — в Source-пространстве (X forward, Y **left**, Z up), Source-единицах и
`float`. Y-инверсия и масштаб применяются **только** на границе с UE (трассировки + публикация
трансформа). Это позволяет портировать `AngleVectors` вербатим и не переписывать знаки в
cross-product'ах.

### 7.6 Timestep policy

```
UE render frame (переменный)
   └─ USourceMovementComponent::TickComponent
        ├─ SourceInputTranslator: сэмплирует Enhanced Input → копит «сырое» состояние
        └─ FixedStepAccumulator += DeltaTime
             while (Accumulator >= TickInterval && Steps < MaxStepsPerFrame)
                 cmd = BuildUserCmd(tickBase)        // аналог CInput::CreateMove
                 Sim.RunCommand(State, cmd, Params, *Query, TickInterval)
                 Recorder.Push(tickBase, cmd, State)
                 Accumulator -= TickInterval ; ++tickBase
        └─ публикация: RootComponent->SetWorldLocation(ToUEPosition(State.Origin))
                       HullProxy->SetBoxExtent(...) ; View->Apply(State)
```
**РЕШЕНО: tickrate конфигурируем, поддерживаются и 64, и 128.**
`FSourceMovementParams::TickInterval` — параметр, не константа. `frametime` внутри симуляции
**всегда** равен `TickInterval` — никакой зависимости от кадра рендера. Интерполяция визуала
между тиками — отдельный слой, физику не трогает.

Следствия для реализации:
* ни одно численное значение в коде симуляции не зашивается «под 64» — всё через `TickInterval`;
* golden-тесты §8.1 **параметризованы** по tickrate и прогоняются дважды (1/64 и 1/128);
* `ReduceTimers` усечение `(int)(1000*frametime)` даёт `15` при 64 и `7` при 128 —
  это разное поведение legacy-таймеров, и оно сохраняется как есть (Q47);
* quirk'и, зависящие от тика (Q1 двойная gravity, Q5/Q6 air-accel бюджет, stamina-накопление),
  дают **разное observable behaviour** на 64 и 128 — это свойство Source, не дефект порта.
  Regression-тесты фиксируют оба набора ожиданий.

### 7.7 Networking boundary

```
FSourceMovementCommand { uint32 Seq; uint32 TickBase; FSourceUserCmd Cmd; }
FSourceMovementSnapshot{ uint32 AckSeq; FSourceMovementState State; uint32 Hash; }

Client:  BuildCmd → Sim.RunCommand → push(Seq, State) → SendCmd(Server)
Server:  RecvCmd  → Sim.RunCommand (authoritative) → SendSnapshot(AckSeq, State, Hash)
Client:  RecvSnapshot → if (Hash != Stored[AckSeq].Hash)
                            State = Snapshot.State
                            for s in (AckSeq+1 .. LastSeq): Sim.RunCommand(State, Cmd[s])
```

Сейчас реализуется только слой структур + `FSourceStateHash` (детерминированный хэш POD-состояния).
Физика от него не зависит — добавление полноценного prediction не потребует её переписывания.

### 7.8 Testing boundary

```
ISourceWorldQuery
   ├─ SourceWorldQuery_Flat       → unit-тесты (плоскость z=0, параметризуемые ramp/step/box)
   ├─ SourceWorldQuery_Analytic   → статика в рантайме И эталон в тестах
   ├─ SourceWorldQuery_UE         → динамика в рантайме
   └─ SourceWorldQuery_Composite  → рантайм (Analytic + UE)
```
`_Analytic` играет двойную роль: он и рантайм-бэкенд для статики, и эталон, против которого
проверяется `_UE`. Тест `CrossBackend_Consistency` гоняет одну и ту же геометрию (box/ramp/step,
выраженную и как brush-планы, и как UE-примитив) через оба бэкенда и сравнивает
`FSourceTraceResult` поле за полем — так расхождения Chaos-семантики ловятся **без** внешнего
оракула.
Тесты линкуются только с `Core/` — без `UWorld`, без PIE, поэтому гоняются в
`UE_EDITOR` automation и в CI.

---

## 8. TESTING STRATEGY

### 8.1 Уровень 1 — Closed-form unit tests (нет зависимости от геометрии)

Эти случаи имеют точное аналитическое решение за тик, его можно вычислить независимо
(скрипт/таблица) и зафиксировать как golden:

| Тест | Вход | Ожидание (64 tick, surfaceFriction = 1) |
|---|---|---|
| `Gravity_FreeFall_1Tick` | airborne, v=0 | `vel.z == -12.5` (`-0.5g·dt` дважды) |
| `Friction_Below_StopSpeed` | ground, v=(50,0,0) | `drop = 6.5` → `v.x ≈ 43.5` |
| `Friction_Above_StopSpeed` | ground, v=(200,0,0) | `drop = 200*5.2/64 = 16.25` → `183.75` |
| `Friction_Below_Threshold` | `\|v\| = 0.05` | скорость не меняется |
| `GroundAccel_FromRest_W` | fmove=450 → clamp 260 | `+22.34375` u/tick по wishdir (`5.5/64 * MAX(250, 260)`) |
| `GroundAccel_Terminal` | 1 s W | сходится к 260 (cap Q22) |
| `GroundAccel_AddSpeedCap` | `currentspeed` близко к wishspeed | `accelspeed == addspeed` |
| `AirAccel_Budget30` | airborne, W, v=0 | `+30` u/tick (не 48.75) |
| `AirAccel_Ascending` | `0 < vel.z <= 140` предыдущий тик | `accelspeed = 12.1875` (Q6) |
| `AirAccel_FastRise` | `vel.z > 140` предыдущий тик | `m_surfaceFriction` остаётся `1.0` (Q6, ранняя ветка) |
| `AirAccel_NoGain_WOnly` | W, v уже вдоль wishdir > 30 | прирост 0 |
| `AirStrafe_Optimal` | yaw-свип + A | монотонный рост `\|v\|₂D` |
| `Jump_Standing_FromRest` | ground, IN_JUMP | `vel.z == 301.993377 - 18.75` (Q1) |
| `Jump_Ducked_Overwrite` | ducked + IN_JUMP при `vel.z != 0` | `vel.z` перезаписан (Q45) |
| `Jump_Stamina_Accumulation` | 5 прыжков подряд | stamina → ~80, высота прыжка падает |
| `Jump_PogoBlocked` | IN_JUMP зажат | второй прыжок не происходит (Q43) |
| `PreventBunnyJumping_Cap` | `\|v\| = 400`, прыжок | `\|v\| == 286` |
| `CheckVelocity_PerAxis` | v = (5000,5000,0) | `(3500,3500,0)`, `\|v\| ≈ 4949` (Q48) |
| `Input_DiagonalClamp` | W+D | `fmove == smove == 260/√2` |
| `Input_UpmoveStealsBudget` | W + upmove=320 | горизонтальный ввод урезан (Q49) |
| `WalkMove_FreezeBelow1` | `\|v\| = 0.5` | `v == 0`, `StayOnGround` не вызван (Q23) |

Покрытие комбинаций ввода (требование §11 брифа): `W, A, S, D, W+A, W+D, A+S, D+S, W+S, A+D,
W+A+S+D`, каждая — на земле и в воздухе, на малой/большой скорости.

### 8.2 Уровень 2 — Collision / geometry tests (`SourceWorldQuery_Flat`)

Синтетический мир описывается декларативно: набор полуплоскостей/AABB.

| Группа | Кейсы |
|---|---|
| One plane | пол, стена, наклон 30°/44°/46°/60°/89° |
| Two planes | пол+стена, стена+стена 90°, 170° (почти параллельные), 10° (острый угол) |
| Three planes | угол комнаты, пол+2 стены → проверка Q16 (`numplanes != 2` ⇒ `v = 0`) |
| Near-parallel | две плоскости с `dot ≈ -0.999` → проверка Q17 (инъекция `DIST_EPSILON`) |
| Stairs | прямой подъём, диагональный, спуск, 17/18/19-unit ступень, край ступени, ступень+стена, ступень+склон |
| Slopes | walkable 45.5° граница (`normal.z = 0.6999`/`0.7001`), переход склон→пол, convex/concave |
| Edges | сход с уступа 10/18/20/25 units (Q25), ходьба по краю, угол 1×1 |
| Penetration | старт внутри геометрии (`startsolid`), `allsolid` |
| High speed | 3000 u/s в стену (проверка 4 bumps), 3500 по диагонали |

### 8.3 Уровень 3 — Deterministic replay (golden traces)

`FSourceMovementFrame`:
```cpp
struct FSourceMovementFrame
{
    uint32   Tick;
    FSourceUserCmd Input;          // fmove, smove, upmove, buttons, viewangles
    FSrcVec3 Origin, Velocity, BaseVelocity;
    FSrcVec3 GroundNormal;
    FSrcEntityId GroundEntity;
    uint32   Flags;                // FL_ONGROUND/FL_DUCKING/FL_ANIMDUCKING
    uint8    bDucked : 1, bDucking : 1;
    srcfloat DuckAmount, DuckSpeed, Stamina, SurfaceFriction, FallVelocity;
    srcfloat MaxSpeed, WishSpeed;
    FSrcVec3 WishDir;
    uint8    MoveType, WaterLevel;
    uint32   Blocked;              // возврат TryPlayerMove
    uint32   StateHash;
};
```
Формат файла — CSV (человекочитаемый diff) + бинарный `.srcmv` для объёма.
Консольные команды: `source.movement.record <file>`, `source.movement.play <file>`,
`source.movement.compare <file>`.

### 8.4 Уровень 4 — Differential testing против Kisak-Strike

> **СТАТУС: ОТЛОЖЕНО.** Внешний оракул (инструментированная сборка Kisak-Strike) сейчас
> недоступен. Harness, формат CSV и команды сравнения **реализуются сразу** — чтобы подключение
> оракула позже не требовало изменений в коде симуляции. До этого момента роль эталона играют:
>
> 1. **closed-form ожидания** (§8.1) — gravity, friction, ground/air acceleration, jump,
>    stamina, clamp'ы и duck-тайминги имеют точное аналитическое решение за тик, выводимое
>    напрямую из исходника; они фиксируются как golden-числа и проверяются **бит-в-бит**;
> 2. **`SourceWorldQuery_Analytic`** (§7.4) — порт Source-семантики clip-box-to-brush, против
>    которого валидируется Chaos-бэкенд (`CrossBackend_Consistency`, §8.6);
> 3. **инвариантные тесты** (§8.6) — свойства, не требующие эталонных чисел.
>
> Риск, который этим **не** закрывается: расхождение нашего `_Analytic` с настоящим
> `enginetrace` в редких случаях (displacement-поверхности, non-axial brush'и,
> `startsolid`/`allsolid` на границах). Он остаётся открытым до появления оракула и зафиксирован
> как `UNKNOWN-5`.

**Оракул (когда будет доступен).** В коде уже есть инфраструктура дампа: `COM_Log(file, fmt, ...)`
(gamemovement.cpp:650) и `DiffPrint`/`CDiffManager` (gamemovement.cpp:167-536,
активны при `PREDICTION_ERROR_CHECK_LEVEL > 0`). Предлагаемый минимальный патч в
Kisak-Strike (вне нашего UE-проекта, ~30 строк):

* в `CCSGameMovement::ProcessMovement` — дамп всех полей `FSourceMovementFrame`
  в начале и конце каждой команды через `COM_Log("srcmv.csv", ...)`;
* вход задаётся детерминированно: скрипт `cfg` с фиксированной последовательностью
  `+forward/+moveright/+jump/+duck` и `cl_pitchspeed/cl_yawspeed` или
  воспроизведение записанного демо.

Затем тот же CSV подаётся в UE-харнесс:

```
Kisak-Strike (instrumented)                 UE (FSourceMovementSim)
        │ srcmv.csv: per-tick input + state          │ читает input-колонки
        └──────────────────┬─────────────────────────┘
                           ▼
              SourceMovement_DiffTests
   per-tick: ΔOrigin, ΔVelocity, ΔGroundState, ΔGroundNormal,
             ΔMoveType, ΔDuckState, ΔStamina, ΔSurfaceFriction
   отчёт: abs error, rel error, FIRST DIVERGENT TICK + контекст ±5 тиков
```

Бюджеты расхождения (предлагаемые, уточняются после первых прогонов):
* closed-form участки (gravity/friction/accel/jump) — **бит-в-бит** (`== 0`);
* коллизии на аналитической геометрии — `< 1e-4` units;
* коллизии через Chaos-бэкенд — `< 0.03125` (DIST_EPSILON) на тик, без накопления дрейфа
  за 1000 тиков;
* `ground state` / `duck state` / `movetype` — **точное совпадение**, расхождение = провал.

Если Chaos-бэкенд не удержит бюджет на лестницах/складках — переключаем рантайм на
`SourceWorldQuery_Analytic` (Source-семантика clip-box-to-brush по brush-планам уровня).
Это заранее предусмотрено архитектурой и является основным риском проекта.

### 8.5 Уровень 5 — Regression tests

Один тест на каждый `Q*` из §5, имя = `REGRESSION-0NN_<Quirk>`:

```
REGRESSION-001  Q1   DoubleFinishGravityOnJumpTick
REGRESSION-002  Q2   JumpStartZIncludesHalfGravity
REGRESSION-003  Q3   JumpDeltaWrittenToOutWishVel
REGRESSION-004  Q4   FrictionOutWishVelRatioMisuse
REGRESSION-005  Q5   AirAccelUsesUncappedWishspeed
REGRESSION-006  Q6   SurfaceFrictionQuarterWhileAscending
REGRESSION-007  Q7   OptimizedMovementSkipsLeadingCategorize
REGRESSION-008  Q8   GroundQuadrantHardcodedThreshold
REGRESSION-009  Q9   GroundQuadrantMixedTraceFields
REGRESSION-010  Q10  PlayersAlwaysStandable
REGRESSION-011  Q11  TinyFractionForcedToZero
REGRESSION-012  Q12  WillBecomeStuckZeroesVelocity
REGRESSION-013  Q13  AirborneSinglePlaneSkipsAntiOscillation
...
REGRESSION-058  Q58  LadderSetGravityZeroMeansOne
```
Каждый — с полем `ExpectedSourceBehavior` (ссылка `file:line`) в комментарии, чтобы
будущие оптимизации (Phase 8) не могли «починить» баг незаметно.

### 8.6 Уровень 6 — Invariant & cross-backend tests (оракул не требуется)

Компенсируют отсутствие внешнего эталона. Проверяют свойства, а не конкретные числа.

**Cross-backend consistency.** Одна геометрия, выраженная дважды (как brush-планы для
`_Analytic` и как UE-примитив для `_UE`), прогоняется через оба бэкенда; сравниваются все поля
`FSourceTraceResult`:

| Кейс | Что ловит |
|---|---|
| AABB-куб, свип по нормали / под 45° / почти параллельно грани | базовая epsilon-политика, `endpos` back-off |
| Ramp 30°/44.9°/45.6°/60° | точность `plane.normal`, порог 0.7 |
| Step 17/18/19 units, свип вплотную | поведение `StepMove` на границе `stepSize` |
| Внутренний угол 90°, 10°, 170° | согласованность при мульти-контакте |
| Старт внутри геометрии на 0.001 / 0.5 / 10 units | маппинг `bStartPenetrating` → `startsolid`/`allsolid` |
| Нулевой длины свип (`TestPlayerPosition`) | `fraction == 1` vs `startsolid` |

**Инварианты симуляции** (fuzz по случайному вводу, 10⁵ тиков, любая геометрия):

| Инвариант | Обоснование |
|---|---|
| `origin` никогда не оказывается внутри solid после тика | `TestPlayerPosition(origin)` должен давать `fraction == 1` |
| `\|velocity[i]\| <= sv_maxvelocity` после каждого `CheckVelocity` | :3437 |
| на земле после `WalkMove`: `\|velocity\|₂D <= m_flMaxSpeed + ε` | Q22 |
| `m_flDuckAmount ∈ [0,1]`, и `duckAmount == 1 ⟺ FL_DUCKING\|FL_ANIMDUCKING` | cs:1192-1201 (оригинальные Assert'ы) |
| `m_flStamina ∈ [0, sv_staminamax]` | cs:1238 |
| `m_surfaceFriction ∈ {0.25} ∪ (0, 1]` | :1084, :4252 |
| `m_bDucked == true ⟹ hull == ducked` | :900-927 |
| замороженный ввод ⇒ состояние сходится к фиксированной точке (не осциллирует) | anti-oscillation :3081 |
| одинаковые `(state, cmd)` ⇒ побитово одинаковый результат (детерминизм) | требование replay/prediction |
| проигрывание записанной трассы воспроизводит её побитово | требование §26 брифа |

Оригинальные Assert'ы из CS-кода (cs:928, cs:1112, cs:1192-1209) портируются как
`checkf` — они и есть инварианты, которые Valve считала обязательными.

**Симметрия.** Зеркальный ввод (`sidemove → -sidemove`, `yaw → -yaw`) должен давать зеркальную
траекторию на зеркальной геометрии. Ловит перепутанные знаки в `AngleVectors`, cross-product'ах
и квадрантных трассах.

### 8.7 Debug visualization & telemetry

CVars (регистрируются в `SourceMovementDebug.cpp`):

| CVar | Что |
|---|---|
| `source.movement.debug` | мастер-переключатель |
| `source.movement.drawhull` | текущий AABB (standing/ducked, цвет по `m_bDucked`) |
| `source.movement.drawtraces` | все трассы тика: start/end/fraction/normal |
| `source.movement.drawplanes` | `planes[]` из `TryPlayerMove` + crease-направление |
| `source.movement.drawvelocity` | velocity, wishdir, wishvel, primal_velocity |
| `source.movement.drawground` | ground normal, ground trace, snap-дельта, квадрантные под-box'ы |
| `source.movement.drawstep` | vecPos / vecDownPos / vecUpPos / финальная позиция |
| `source.collision.debug` | сводка по bump'ам: `numplanes`, `blocked`, `allFraction` |
| `source.movement.telemetry` | HUD: Velocity, Speed, HorizontalSpeed, WishSpeed, WishDir, AccelSpeed, GroundState, GroundNormal, SurfaceFriction, Friction, CollisionCount, CollisionPlanes, StepHeight, DuckState/DuckAmount/DuckSpeed, Stamina, MaxSpeed, TickBase, Blocked |
| `source.movement.record` / `.play` / `.compare` | replay |
| `source.movement.freeze` | пошаговое исполнение тиков |

---

## 9. Phase plan (предлагаемый порядок реализации)

| Phase | Содержание | Definition of Done |
|---|---|---|
| **4** | модуль + `Core/` (векторы, углы, params, state, hull), `ISourceWorldQuery`, `SourceWorldQuery_Flat`, `ASourcePlayerPawn`, `USourceMovementComponent`, fixed timestep, `StartGravity/FinishGravity`, `Friction`, `Accelerate`, `AirAccelerate`, `WalkMove`, `AirMove`, `CheckJumpButton`, `PreventBunnyJumping`, `CheckVelocity`, `CheckParameters`, `ReduceTimers` | §8.1 зелёный |
| **5** | `TryPlayerMove`, `ClipVelocity`, `StepMove`, `StayOnGround`, `CategorizePosition`, `TracePlayerBBoxForGround`, `SetGroundEntity`, `CategorizeGroundSurface`; бэкенды `_UE`, `_Analytic`, `_Composite`; `FSourceMovementFrame` + `record/play/compare` | §8.2 зелёный на `_Flat`; §8.6 `CrossBackend_Consistency` в бюджете; replay воспроизводится побитово |
| **6** | `Duck` (CS state machine), `CanUnduck`, `Finish(Un)Duck`, `SetDuckedEyeOffset`, `HandleDuckingSpeedCrop`, `FixPlayerCrouchStuck`, `CheckStuck`, `CheckFalling`, `OnJump/OnLand`, stamina, baseVelocity/conveyor | §8.2 duck-группа + все edge-cases §6 брифа |
| **7** | `LadderMove`, `CheckWater`/`WaterMove`/`WaterJump`, `FullNoClipMove`, `FullObserverMove` | паритет на лестницах/воде |
| **8** | Все `REGRESSION-*` (58 кейсов × 2 tickrate), инвариантный fuzz, diff-harness под внешний оракул (подключается, когда появится) | §8.3, §8.5, §8.6 зелёные |
| **9** | Net-слой: `FSourceMovementCommand/Snapshot`, client prediction + server reconciliation + replay | расхождение клиент/сервер = 0 при стабильном пинге |
| **10** | Оптимизация (trace-reuse, SIMD, аллокации) — **только** при неизменных diff/regression | ни один тест не изменил ожидание |

---

## 10. Принятые решения

| # | Вопрос | Решение | Следствия |
|---|---|---|---|
| 1 | Tickrate (`UNKNOWN-1`) | **Конфигурируем; поддерживаются 64 и 128** | `TickInterval` — поле `FSourceMovementParams`, не константа. Golden-тесты параметризованы и прогоняются дважды. Ни одно число в симуляции не зашито «под 64». См. §7.6. |
| 2 | Unit scale | **`UnitsToUU = 1.0`** (1 Source unit = 1 UU) | Игрок 72 UU. Умножения на границе нет ⇒ нет лишнего float-шума в трассировках. Геометрия авторится в Source-единицах. См. §7.5. |
| 3 | Источник геометрии | **Композитный: UE-геометрия + `_Analytic` для статики** | `SourceWorldQuery_Composite` объединяет результаты как `enginetrace->TraceRay` (min fraction, OR по solid-флагам). `_Analytic` одновременно рантайм-бэкенд и эталон для `_UE`. См. §7.4, §8.6. |
| 4 | Внешний оракул (§8.4) | **Недоступен сейчас; harness делаем сразу** | Эталон на текущем этапе = closed-form числа + `_Analytic` + инварианты (§8.6). Формат CSV, `FSourceMovementFrame` и команды `record/play/compare` реализуются в Phase 4–5, чтобы подключение оракула позже не трогало симуляцию. |

### Решения, принятые по умолчанию (легко переопределяются, помечены в коде)

| # | Вопрос | Принятое значение | Обоснование |
|---|---|---|---|
| 5a | `surfaceproperties` (`UNKNOWN-2`) | `friction = 0.8` ⇒ `m_surfaceFriction = 1.0`; `jumpFactor = 1.0`; `maxSpeedFactor = 1.0`; `climbable = 0` | Это значение `"default"` из `vphysics/physics_material.cpp:609`. Реализуется как `USourceSurfacePropsTable` (DataAsset) с одной записью `default` — подстановка реальной таблицы из VPK не потребует изменений кода. |
| 5b | Weapon max speed (`UNKNOWN-3`) | «без оружия»: `m_flMaxSpeed = CS_PLAYER_SPEED_RUN = 260`, `fAccelerationScale` без weapon-скейла | Путь `if (sv_accelerate_use_weapon_speed && csWeapon)` при `csWeapon == nullptr` не выполняется (cs:1287). Это корректная ветка оригинала, а не упрощение. Интерфейс `ISourceWeaponSpeedProvider` заложен сразу. |
| 5c | `m_flVelocityModifier` (`UNKNOWN-4`) | `1.0` постоянно | Инициализация `cs_player.cpp:1530`; понижающий источник в прочитанном коде отсутствует. Поле присутствует в state и участвует в `CheckParameters` (cs:277) — только не меняется. |
| 5d | `GetLaggedMovementValue()` (`UNKNOWN-9`) | `1.0` | Множитель `frametime` в `ProcessMovement` (:1335). Поле сохраняется в params. |
| 5e | `IsGameConsole()` (`UNKNOWN-6`) | `false` (PC) | Влияет на `Friction`: PC-путь `control = (speed < sv_stopspeed) ? sv_stopspeed : speed` (:1911). |
| 5f | `IsTeammateSolid()` (`UNKNOWN-7`) | `true` (тимейты solid) | Влияет только на team-биты `PlayerSolidMask` (cs_player_shared.cpp:2544); при отсутствии команд маска = `MASK_PLAYERSOLID`. |
| 6 | Цилиндр игрока (§3 брифа) | **Отсутствует в Source — реализуется только AABB** | Предоставленный код использует исключительно AABB-свип (`Ray_t::Init(start, end, mins, maxs)`, gamemovement.h:313). Ближайший аналог «скруглённого» зондирования — 4 квадрантных под-box'а в `TracePlayerBBoxForGround` (:4049). Добавление цилиндра изменило бы поведение ⇒ противоречит цели. |

Все шесть значений по умолчанию помечаются в коде как

```cpp
// SOURCE-COMPAT / UNKNOWN-N:
// Default derived from <file:line>. Replace with real data when available;
// do NOT silently change — see Docs/SOURCE_MOVEMENT_SPEC.md §10.
```
