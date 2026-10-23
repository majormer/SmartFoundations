# Smart Panel and HUD

Press **K** with a supported buildable selected. Smart's tools share one window, with the controls that apply to your current building.

![Shared task tabs and aligned placement controls](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/scaling.png)

## Choose a task

| Tab | Use it to |
|---|---|
| **Scaling** | Repeat the held building and edit its grid, spacing, steps, stagger and rotation |
| **Extend** | Select an existing matching factory module, inspect captured companions and repeat it |
| **Walking** | Steer a connected route, or resume a compatible existing bus |
| **Upgrade** | Scan existing infrastructure, choose a tier and review replacement costs |
| **Restore** | Save/load Grid Presets or captured Modules |

Available tasks follow the held building. Railway tracks and Vehicle Paths open Walking directly and do not offer ordinary Scaling. A raw pipe recipe can open Upgrade; to start or resume a pipe walk, equip the matching **stackable support** first.

Switching to Upgrade suspends an active walk. Switching to a task that would discard an unbuilt path asks for confirmation. Read the warning before continuing; declining keeps the path.

## Placement and live settings

Scaling keeps X/Y/Z in aligned columns. These fields stay building-relative even with Player Relative controls enabled. The colored arrows show the actual directions.

**Apply placement immediately** controls placement geometry. When off, edit the grid and transforms, then press **Apply** to update the world preview. Apply does not construct anything. Closing discards unapplied placement edits. **Reset** resets the placement controls.

Connection preferences, production recipes and the applicable native building controls are **live settings**. For example, changing pipe routing rebuilds its preview immediately; wall-support rotation changes the support itself. They are labeled separately and are not waiting for the placement Apply button. Check the preview and material quote before firing the build gun.

## Help and window position

Hover a small **?** button beside a building or section label for help. Click to keep the details expanded; click again to collapse them. Each task explains its own workflow.

Drag the title bar with the left mouse button, or use the existing right-button drag gesture. A manually moved panel remembers its position across saves. The mod settings offer a position reset.

## HUD during building

The HUD shows the active value, colored grid counts and relevant control hints. **Controls: Building Relative** or **Controls: Player Relative** identifies how the wheel and directional keys behave. The label describes controls, not the selected visual theme.

During Walking the HUD identifies the route family and current segment. Hints change to route operations such as advancing, turning and climbing. Remapped controls are reflected in the hints.

![Shared HUD and Camera editor with a single Done / Lock action](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/layout-editor.png)

## Move, resize and theme the HUD

Enter layout **Edit Mode** with **F8** by default, or choose **Edit HUD layout after Resume** in mod settings. Drag the HUD title bar to move it and use its resize handle or Scale field to change its size. Choose from the six themes; the existing multilingual font family is retained.

With Smart Camera installed, the same edit session includes the camera windows. **Done / Lock** saves pending HUD and camera layout edits together. **Cancel** or Escape discards the pending edits. Normal building input and the ordinary scaling HUD return when the editor closes.

HUD appearance and panel positions apply **across all saves on your installation**. The Camera editor's All saves / This save selector applies to camera preferences only. **Reset HUD Appearance** restores the HUD defaults. Outside Edit Mode the HUD is locked and does not intercept building clicks.

See [Controls](Controls), [Smart Walking](Smart-Walking), [Smart Upgrade](Smart-Upgrade), [Smart Restore](Smart-Restore) and [Smart Camera](Smart-Camera).

---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
