# Enhanced Input UAsset Spec for AArchSimCharacter

> Sprint S-02 · AS-03b · last updated 2026-06-26

## Why This Doc?

AS-03b added Enhanced Input integration to `AArchSimCharacter`. The C++ code
declares `TObjectPtr<UInputAction>` and `TObjectPtr<UInputMappingContext>` slots
via `UPROPERTY(EditAnywhere …)`, but the actual **UAsset** files (`.uasset`)
must be created inside the UE Editor and then assigned to those slots in a
Blueprint derived from `AArchSimCharacter`.

This document is the complete, step-by-step reference for that Editor-side work.

---

## UAsset Inventory

| Content-browser path | Asset type | Value type | Purpose |
|---|---|---|---|
| `Content/Input/IMC_ArchSimDefault` | `UInputMappingContext` | — | The default IMC pushed by `BeginPlay`. Holds all key→action mappings. |
| `Content/Input/IA_Move` | `UInputAction` | `Axis2D (Vector2D)` | WASD movement (X = right, Y = forward) |
| `Content/Input/IA_Look` | `UInputAction` | `Axis2D (Vector2D)` | Mouse look (X = yaw Δ, Y = pitch Δ) |
| `Content/Input/IA_Jump` | `UInputAction` | `Digital (bool)` | Space key — jump |
| `Content/Input/IA_Sprint` | `UInputAction` | `Digital (bool)` | LeftShift key — sprint hold |
| `Content/Input/IA_Crouch` | `UInputAction` | `Digital (bool)` | LeftCtrl key — crouch toggle |

---

## Step 1 — Create the `Content/Input/` folder

1. Open the **Content Browser** (Ctrl+Space or Window → Content Browser).
2. Right-click the `Content` root → **New Folder** → name it `Input`.

---

## Step 2 — Create Input Action assets

For each row in the table above with type `UInputAction`:

1. In `Content/Input/`, right-click → **Input** → **Input Action**.
2. Name it exactly as shown (e.g. `IA_Move`).
3. Double-click to open the asset.
4. Set **Value Type**:
   - `IA_Move`, `IA_Look` → `Axis2D (Vector2D)`
   - `IA_Jump`, `IA_Sprint`, `IA_Crouch` → `Digital (bool)`
5. Leave all other settings at default. Save (Ctrl+S).

---

## Step 3 — Create the Input Mapping Context

1. In `Content/Input/`, right-click → **Input** → **Input Mapping Context**.
2. Name it `IMC_ArchSimDefault`. Double-click to open.
3. Click the `+` button next to **Mappings** to add each action:

### 3a — IA_Move (WASD)

Add **IA_Move** once per physical key, using **Modifiers** to re-sign axes:

| Physical key | Modifiers on this mapping | Result delivered to IA_Move |
|---|---|---|
| `W` | *(none)* | `(X=0, Y=+1)` — forward |
| `S` | **Negate** | `(X=0, Y=-1)` — backward |
| `D` | **Swizzle Input Axis Values** (order YXZ) | `(X=+1, Y=0)` — right |
| `A` | Swizzle (YXZ) + **Negate** | `(X=-1, Y=0)` — left |

> **Why swizzle?** A keyboard key produces a scalar (float). Swizzle moves it
> from the Y slot into the X slot so that `D` contributes to the X=right
> channel of the Vector2D.

### 3b — IA_Look (Mouse look)

| Input source | Modifiers | Note |
|---|---|---|
| `Mouse X` | *(none)* | feeds X (yaw Δ) |
| `Mouse Y` | **Negate** | feeds Y (pitch Δ); negate prevents inverted look |

> Add **Mouse X** and **Mouse Y** as two separate mappings pointing to the
> same `IA_Look` action.

### 3c — IA_Jump

| Key | Trigger | Modifiers |
|---|---|---|
| `Space Bar` | *(default)* | *(none)* |

### 3d — IA_Sprint

| Key | Trigger | Modifiers |
|---|---|---|
| `Left Shift` | *(default — Hold or Down)* | *(none)* |

### 3e — IA_Crouch

| Key | Trigger | Modifiers |
|---|---|---|
| `Left Ctrl` | *(default)* | *(none)* |

4. Save `IMC_ArchSimDefault` (Ctrl+S).

---

## Step 4 — Assign Assets in the Blueprint

1. In the Content Browser, find (or create) a Blueprint class that inherits
   from `AArchSimCharacter` (e.g. `BP_ArchSimCharacter`).
2. Open the Blueprint. In the **Details** panel (with the Blueprint's CDO
   selected), expand the **ArchSim | Input** category.
3. Assign each slot:

   | Details slot | Asset to assign |
   |---|---|
   | Default Mapping Context | `IMC_ArchSimDefault` |
   | IA Move | `IA_Move` |
   | IA Look | `IA_Look` |
   | IA Jump | `IA_Jump` |
   | IA Sprint | `IA_Sprint` |
   | IA Crouch | `IA_Crouch` |

4. **Compile** and **Save** the Blueprint.

---

## Step 5 — Quick PIE Verification

1. Place `BP_ArchSimCharacter` in a test level (drag from Content Browser).
2. Set it as the **Default Pawn Class** in World Settings, or set it in your
   GameMode's Default Pawn Class (see AS-03c for the full GameMode wire).
3. Press **Play** (PIE).
4. Expected results:

   | Input | Expected behaviour |
   |---|---|
   | `W` / `A` / `S` / `D` | Character moves relative to camera facing |
   | Mouse move | Camera yaw / pitch follows |
   | `Space` (hold then release) | Jump; releasing stops jump |
   | `Left Shift` (hold) | ALS gait switches to Sprinting; release → Running |
   | `Left Ctrl` (tap) | ALS stance toggles Standing ↔ Crouching |

5. If movement works but Sprint / Crouch do not change the ALS animation, ensure
   the ALS animation Blueprint is correctly wired to `DesiredGait` / `DesiredStance`
   (this is handled by ALS out of the box; verify the Anim BP is assigned to the
   character mesh).

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Warning: `DefaultMappingContext is null` in log | IMC slot not assigned in BP | Step 4 above |
| Warning: `IA_Move is null` etc. | IA slot not assigned | Step 4 above |
| No movement on WASD | IMC registered but IA_Move not mapped | Check IMC mappings (Step 3a) |
| Look is inverted vertically | Mouse Y modifier missing | Add **Negate** modifier to Mouse Y mapping |
| Crouch has no effect | ALS Anim BP not wired | Verify character mesh Anim Blueprint |
| Build error: `UEnhancedInputComponent not found` | EnhancedInput plugin disabled | Enable in `.uproject` Plugins section |

---

## Architecture Notes

- `BeginPlay` calls `AddMappingContext(DefaultMappingContext, Priority=0)`.
  If the game needs to swap control schemes at runtime, call
  `RemoveMappingContext` + `AddMappingContext` with a different IMC.
- All IA_ slots are guarded with `IsValid()` in both `BeginPlay` and
  `SetupPlayerInputComponent`; a null slot logs a `Warning` and skips the
  binding rather than crashing.
- `HandleMove` uses the **camera view rotation** (not `GetActorForwardVector`)
  so that W always means "toward the camera", matching ALS locomotion conventions.
- Sprint uses **Started** / **Completed** events (not Triggered/Canceled) so
  the gait switches exactly once per press/release edge, not every tick.
- See `Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp` for the
  canonical ALS input reference implementation.
