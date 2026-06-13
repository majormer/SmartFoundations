# Smart Camera

Smart Camera is a companion mod for Smart!. It adds a picture-in-picture camera view while building so you can inspect a hologram from another angle without running around the build site.

This page is a temporary home for Smart Camera documentation. Long term, Smart Camera may be merged into Smart!, or its source may be published separately like Smart!.

> Screenshot placeholder: Smart Camera PiP viewport showing the far end of a long Smart! foundation grid.

## Requirements

Smart Camera currently depends on:

- SmartFoundations v25.0.0 or newer.
- SML 3.12.x.
- Satisfactory 1.2 / game version 491125 or newer.

Smart Camera works in multiplayer on dedicated servers, the same as Smart!. Install matching versions on the server and on every client.

## What It Does

Smart Camera creates a small overlay viewport while a Smart! hologram is active. The camera tracks the active hologram, and for large Smart! grids it can follow the furthest/top-most relevant grid position instead of only the starting hologram.

Use it for:

- Checking long foundation or wall lines.
- Aligning distant grid edges.
- Inspecting vertical builds.
- Checking conveyor lift direction.
- Building from angles your player character cannot easily see.

## Camera Modes

Smart Camera has these modes:

| Mode | Use |
|------|-----|
| Off | Hide the camera overlay |
| Left 45 | Isometric-style view from the left |
| Front 45 | Front angled view |
| Right 45 | Isometric-style view from the right |
| Back 45 | Back angled view |
| Top Down | Bird's-eye alignment view |
| Forward Level | Ground-level forward view |
| Periscope | Conveyor-lift-focused view from the lift end |

> Screenshot placeholder: comparison strip showing Top Down, Front 45, and Periscope modes.

## Controls

| Key | Action |
|-----|--------|
| `[` | Cycle camera mode |
| `]` | Hold zoom modifier |
| `]` + mouse wheel | Zoom the PiP camera |

The Smart Camera input mapping directly binds `[` and `]`. Mouse wheel zoom is handled by Smart!'s shared mouse wheel input while `]` is held.

Zoom range is currently 5m to 50m.

Holding the zoom modifier asks Smart! to lock the hologram so zooming does not accidentally rotate the build preview.

## Settings

Smart Camera has settings for:

- Enable or disable the PiP camera.
- Default camera mode.
- Camera viewport scale.
- Camera viewport horizontal position.
- Camera viewport vertical position.

The PiP widget is hit-test invisible, so it should not block normal mouse clicks while you build.

## Current Caveats

- Smart Camera is a separate companion mod, not part of the main Smart! package today.
- It relies on Smart! for hologram/grid tracking.
- Some UI and source-publication details may change if it is merged into Smart! or released as its own source-available repo.
