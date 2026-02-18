# Gw.exe Camera Reconstruction Notes

## Scope
- Binary: `Gw.exe` (`x86`)
- Goal: reconstruct runtime camera setup exactly enough for GWMB parity.
- Focused on gameplay/world camera path (not minimap/UI camera).

## Core Runtime Call Path
1. `GmView_HandleUIMessage @ 0x004DCC90`
2. `GmCam_SetCameraTransform @ 0x004F05B0`
3. `GmCam_UpdateShakeAndTransition @ 0x004ED680`
4. `GmView_UpdateCameraFrustum @ 0x004E76B0`
5. `GmView_UpdateWorldRender @ 0x004E56A0`
6. `Map_update_camera @ 0x006E0C20`
7. `Gr_Trans_LookAt @ 0x0064CB10` and `Frame_SetCameraFovAndZFar @ 0x0060FB30`

## Camera Globals (World Camera)
- `0x00BC2C00`: camera position (vec3)
- `0x00BC2C0C`: camera target (vec3)
- `0x00BC2C64`: camera FOV (float, radians)
- `0x00BC2D48`: global `GmCam` object root

## Important Constants
- `0x009102B4 = 48000.0f` (world camera zFar used by frustum update)
- `0x00906E1C = -1.0f` (used in up/screen-space helpers; gameplay up-vector uses `z = -1`)

## FOV Units
- `GmCam_SetFieldOfView @ 0x004F1030` converts degrees -> radians:
  - `radians = degrees * PI / 180`
- `GmCam` runtime FOV field read by `GmCam_GetCameraTransform`:
  - `this + 0xC0`

## View Setup (Exact Behavior)
- In world rendering (`GmView_UpdateWorldRender` and `GmView_UpdateCameraFrustum`), the up vector passed is:
  - `up = (0, 0, -1)`
- Forward direction:
  - `forward = normalize(target - position)`
- `Map_update_camera` writes normalized direction output (same forward basis used by render path).

## Projection Setup (Gameplay Path)
- `GmView_UpdateCameraFrustum` calls:
  - `Frame_SetCameraFovAndZFar(frameId, fov, zFar=48000, perspectiveMode)`
- Perspective mode:
  - normal gameplay uses mode `2`
  - cinematic/reset-map path can use mode `0`
- In `Gr_Trans_SetPerspective` mode `2`, frustum extents are built as:
  - `halfHeight = zFar * tan(fov * 0.5)`
  - `halfWidth  = halfHeight * aspect`
  - rect = `[-halfWidth, -halfHeight, +halfWidth, +halfHeight]`

## GmCam Transform Source
- `GmCam_GetCameraTransform @ 0x004F21C0`:
  - position source:
    - `this + 0x78` (default), or `this + 0x90` depending on state (`this + 0x10C`)
  - target source:
    - `this + 0xA8`
  - fov source:
    - `this + 0xC0`
  - optional shake offsets:
    - `this + 0x110` block (`0x118..0x14C`)
  - previous-valid fallback cache:
    - `this + 0xF0..0x104` if degenerate transform

## Reconstruction Recipe for GWMB
1. Drive camera from `(position, target, fovRadians)` outputs, not from yaw/pitch integration.
2. Use look-at basis from `position/target` each frame.
3. Use world up equivalent to GW path.
4. Use gameplay projection mode equivalent to GW mode `2` (with `zFar=48000`), unless emulating cinematic mode.
5. Keep FOV in radians internally.

## Coordinate Conversion Note
- Existing GWMB comments for animation/mesh indicate GW->GWMB conversion:
  - `(x, y, z)_GW -> (x, -z, y)_GWMB`
- Under this mapping, GW up `(0,0,-1)` maps to GWMB up `(0,1,0)`.
- If feeding camera values directly from live Gw.exe, convert position/target with the same basis transform before applying in GWMB.

## Remaining Unknowns
- Full semantic meaning of non-gameplay perspective mode conversions (`mode 0/1` conversion path via `GrTrans_CalcPerspectiveByType`).
- Some `GmCam_UpdateCameraState` internals are mode/controller-specific and heavily intertwined with collision/avoidance and controller objects.
