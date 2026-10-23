# Smart Camera 2.0 player guide

> **Unreleased draft.** Prepared for the coordinated Smart! 35.0.0 release. This candidate is not compatible with the current Smart! package on SMR. Smart Camera can work by itself; the coordinated Smart! version is 35.0.0.

Smart Camera gives you additional views while building. It follows a normal hologram without Smart!, accepts Smart's grid/frontier framing when a compatible Smart! is engaged, and lets other mods supply independent camera views. It does not change placement permissions, resource costs or your world's buildable data.

## Start using it

1. Load a game and select an ordinary buildable with the build gun. The built-in view follows that hologram; conveyor lifts use their endpoint view.
2. With a compatible Smart! installed and engaged, Smart supplies its build-preview view. Equivalent default/Smart views do not appear twice.
3. Open **Mods → Smart! Camera** to configure the built-in view and registered cameras. Other mods' friendly names/cameras appear automatically after they register in the loaded world; no typed IDs or manual profile are required.
4. Disable **Built-in build gun view** if you only want cameras supplied by other mods. Registered views have separate Enabled controls.

### Built-in view with Smart! assistance off

![Smart Camera’s built-in view follows a single Large Fan hologram while Smart! building assistance is switched off.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/camera-built-in.png)

Smart Camera’s built-in view follows a single Large Fan hologram while Smart! building assistance is switched off.

This example has Smart! installed but its session assistance disabled. The inset is supplied by Smart Camera itself, rather than Smart!’s grid camera. Re-select the building after switching assistance off.

## Controls

| Default control | Action |
|---|---|
| `[` | Cycle the active building camera's angle, including Off |
| Hold `]` + mouse wheel | Adjust building-camera distance; scroll up moves closer, down moves farther |
| `F8` | Enter live layout Edit Mode |
| Drag a viewport's title bar | Move that registered viewport |
| Drag any L-shaped corner | Resize that registered viewport |
| **Done / Lock** | Save pending layout changes and return to building |

Rebind the three Camera actions in the native **Options → Keybindings → Smart! Camera Controls** section. These controls do not require a numpad. Outside Edit Mode, the viewport does not intercept ordinary building clicks. Mods that supply absolute camera poses own their aiming controls; Camera's build-angle/zoom keys do not replace them.

## Angles and lifts

Building modes are **Left45, Front45, Right45, Back45, TopDown, ForwardLevel** and lift-only **Periscope**; **Off** hides the building view. Periscope looks outward from the selected conveyor-lift endpoint, helping you check its direction and alignment while setting height. It is skipped when cycling an ordinary non-lift preview. Adjustable building-camera distance is 5–50m.

An explicit angle override can replace automatic lift framing. To restore automatic behavior without losing your layout, choose **Use mod framing and distance** in the all-saves angle menu, or **Use global framing and distance** for this save. Changing layout alone preserves inherited framing and distance.

## Move and resize views

Press **F8** while a view is visible. Each active viewport gets its mod/camera title and four corner handles. Drag to place it where it helps your build; release to finish the preview gesture. Camera position in the world and the hologram's native placement are independent of the overlay's screen rectangle.

![Live layout title bar and resize handles](https://raw.githubusercontent.com/majormer/SmartCamera/master/docs/images/2.0/edit-mode.png)

The editor uses the last selected **All saves / This save** scope, initially All saves. Choose **All saves / This save** in the editor, or choose the scope in Camera settings before **Edit viewport layout after Resume**. Changing the editor scope keeps your current previews. An explicit All saves edit includes this save and any other existing saved profiles for that camera. Moving or resizing with All saves selected updates its position and size across those profiles. Other settings, such as brightness or Enabled, keep their own values. Choose This save to change the layout only for this world. Press **Done / Lock** to save all pending edits, or **Cancel** / Escape to discard them. When Smart! is installed, the same Done/Lock and Cancel actions also complete its HUD appearance preview; the HUD preferences still apply across saves. Pressing F8 again saves and exits. Inactive views stay in settings, but need an active subject to appear in the live layout editor.

## Settings and persistence

Registered cameras show **Enabled**, **Active/Inactive**, screen X/Y, width/height, scale, update rate and brightness. Applicable building views also show angle and distance. Values apply live and save immediately; no world reload is required.

| Setting | Meaning and range |
|---|---|
| Position X / Y | Normalized screen position 0–1 from the upper-left corner |
| Width / height | Render-target dimensions 64–2048 pixels per axis |
| Scale | Display scale 0.1–2; effective screen size is dimensions × scale |
| Update rate | 5–60 captures/sec; 20 is the default suggestion |
| Brightness | -3 to +3 EV; positive values brighten the picture. Reduce it if the image looks washed out. |
| Building view distance | 500–5000 cm (5–50m), when the consumer uses building angles |

Start with consumer suggestions, then adjust only what you need. Higher resolutions, update rates and more active views can increase rendering cost.

**All saves** applies the edited field globally and to that camera's existing saved overrides. For example, disabling a camera under All saves disables it in the current world too, even if you previously saved a layout there. Changing brightness does not overwrite saved window positions, and changing position does not re-enable a disabled camera. **This save** edits only the loaded native save's profile; other saves and global values are untouched. Later edits use whichever scope you explicitly choose. Preferences are local to your installation/player; they are not replicated settings for everyone on the server. A save name is not the identity key, so do not rename files to manage Camera profiles.

**Use mod suggestions** under All saves resets that camera's complete profile across all saves, including its saved overrides. **Use global settings** under This save removes only this save's override. Other cameras are unchanged. The framing/distance inheritance option resets only those two controls and preserves layout. Legacy numeric profiles retain their explicit settings until adjusted or reset.

## Multiple mods

Each mod can register several cameras with stable namespaced string IDs. Friendly mod and camera names are visible even when the view is inactive. A missing subject, disabled view or open native menu can make it inactive without removing its settings entry.

The included **Space Elevator Camera** reference uses `SpaceElevatorCamera:player-look`. Its origin stays above the native Space Elevator; turning the player rotates the view through 360°, and looking up/down changes pitch. Player movement does not translate it. No elevator means no rendered reference view. This is a separate example consumer, not a built-in Camera behavior or a dependency on Smart!.

![The layout editor shows two independent views: Smart!’s build preview and Space Elevator Camera. Their title bars and corner handles identify the views being edited.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/camera-multiple-views.png)

The layout editor shows two independent views: Smart!’s build preview and Space Elevator Camera. Their title bars and corner handles identify the views being edited.

The small build view keeps the two Fan previews in frame while the independent Space Elevator view looks over a different part of the world. Move and resize each window separately in Edit Mode; **Done / Lock** saves the chosen layout.

## Menus, multiplayer and troubleshooting

Native pause/interaction menus hide the pictures and suspend captures. Resume restores eligible views. Edit Mode is the deliberate exception so you can adjust live rectangles.

Rendering happens on each local client, including standalone and multiplayer clients; a dedicated server does not render these views. Use matching released packages as required by the mod manager. Ordinary saves remain usable without Camera or Smart!.

If no view appears, check the loaded world's registration list, Enabled state, built-in toggle, active subject and angle (Off hides a building view). If lift framing differs from expectations, restore framing/distance inheritance. If a consumer's camera disappears, that mod may no longer have a valid subject. Hardware-specific black-startup and older latest-building reports remain separately tracked; this guide does not promise that every reported graphics/network issue is resolved.

Developers: see the [consumer API guide](Smart-Camera-Integration) and [complete reference plugin](https://github.com/majormer/SmartCamera/tree/master/docs/examples/SpaceElevatorCamera).

---

_Last updated: 2026-10-22 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
