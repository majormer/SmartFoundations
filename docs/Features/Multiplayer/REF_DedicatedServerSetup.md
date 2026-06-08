---
title: Dedicated Server Setup (Windows + Linux) — for #176 certification
type: REFERENCE
date: 2026-06-08
status: Active
category: Features
related:
  - ./PLAN_MultiplayerSupport_Revalidation_1.2.md
  - ./REF_SMLMultiplayerNotes.md
issues:
  - 176  # Dedicated server support (umbrella)
---

# Dedicated Server Setup (Windows + Linux)

Grounded in the official Satisfactory modding docs (`ForUsers/DedicatedServerSetup.adoc`,
`Development/TestingResources.adoc`, `Development/UpdatingToDedi.adoc`) and the Satisfactory wiki
dedicated-server guide. This is the reference for standing up a dedi to certify Smart! on dedicated servers
(#176).

## Read this first — sequencing

- **Slice 0 (client scaled-placement parity) does NOT need a dedicated server.** It uses **Approach A**: two
  game clients, one hosting a **listen server** (host sets Session Type = `IP`, client runs `open
  127.0.0.1`). See `REF_SMLMultiplayerNotes.md`. Run Slice 0 first on a listen server; it exercises both
  host-authority and client-request paths.
- A dedicated server is **Approach B**, needed to *certify* #176. A dedi has **no player controller index 0**
  at all — so it is the strict test for our `GetFirstPlayerController` build-path hazards (they don't just
  pick the wrong player on a dedi, they have no PC to pick). Do this **after** listen-server parity holds.

## Hard prerequisite: Smart! must be compiled for the SERVER target

> "Do NOT naively copy-paste your client's mods folder to a server — this will not work. The compiled files
> used by the game client will not work on dedicated servers."

- The client Shipping DLL from the AGENTS.md golden path is **client-only**. A dedi needs the mod built and
  packaged for the **server platform target** (Windows Server and/or Linux Server).
- 31.0.2 shipped a **Windows dedicated-server package** (so the mod can be version-matched on a Windows
  dedi) but it is **uncertified**. **Linux server is a separate target compile** and is not yet produced —
  standing up a Linux dedi will surface whether the Linux server target even builds/cooks cleanly (likely the
  first concrete #176 work item on the Linux side).
- Packaging the server targets is an Alpakit step (maintainer-driven, like the client package). Confirm
  Alpakit's server-platform options are enabled for SmartFoundations before attempting a dedi install.

## A. Windows self-hosted dedicated server (SteamCMD)

1. **Install SteamCMD.** Download `steamcmd.zip`, extract to e.g. `C:\steamcmd`, run `steamcmd.exe` once to
   self-update.
2. **Install the dedicated server** (Steam app **1690800**, separate from the game app 526870):
   ```
   steamcmd +force_install_dir C:\SatisfactoryDedicatedServer +login anonymous +app_update 1690800 validate +quit
   ```
   For the experimental branch: `+app_update 1690800 -beta experimental validate`.
3. **Launch** (minimum args from the docs):
   ```
   FactoryServer.exe -log -EpicPortal -NoSteamClient
   ```
   A local dedi doesn't need Steam/Epic auth to be reachable on the LAN.
4. **Claim + load a save.** Use a normally-launched client (Steam/Epic) for the one-time claim. A locally
   installed dedi shares your personal save folder, so *uploading* a save fails — instead select a save by
   editing the server's **session name** (Satisfactory wiki: "Loading a save file").
5. **Connect** via the server browser or `open 127.0.0.1` (or `open <LAN-IP>` from another machine).
6. **Firewall:** default game/query/beacon ports `7777/15777/15000` UDP (plus the Steam range) if joining
   from another box.

## B. Linux self-hosted dedicated server (SteamCMD)

1. **Install SteamCMD** (Debian/Ubuntu example):
   ```
   sudo apt update && sudo apt install -y steamcmd      # or the distro's package / the tarball
   ```
2. **Install the dedicated server:**
   ```
   steamcmd +force_install_dir ~/SatisfactoryDedicatedServer +login anonymous +app_update 1690800 validate +quit
   ```
3. **Launch:**
   ```
   ~/SatisfactoryDedicatedServer/FactoryServer.sh -log
   ```
   Run under `systemd` (or `tmux`/`screen`) for persistence. A minimal unit:
   ```ini
   [Unit]
   Description=Satisfactory Dedicated Server
   After=network.target
   [Service]
   ExecStart=/home/<user>/SatisfactoryDedicatedServer/FactoryServer.sh -log
   User=<user>
   Restart=on-failure
   [Install]
   WantedBy=multi-user.target
   ```
4. **Claim + connect** the same way (claim from a normal client, then `open <server-ip>`).
5. **Ports:** same `7777/15777/15000` UDP; open them in `ufw`/firewalld if remote.

## C. Installing Smart! on the dedi (after the server target is packaged)

Use **Satisfactory Mod Manager (SMM ≥ 3.0)** or **ficsit-cli** — never hand-copy client mod files.
- **Shut the server down first** (running server locks mod files).
- **Local install:** SMM/ficsit-cli often auto-detect a Steam/Epic dedi and tag it `DS` in the dropdown;
  otherwise point the tool at the folder containing `FactoryServer.exe`/`FactoryServer.sh` (local path on the
  same machine, or SFTP/SMB for a remote box — SFTP preferred).
- **Server↔client version consistency is mandatory** — clients must run the exact same mod set/version or the
  join is rejected. Manage client + server from the **same SMM/ficsit-cli profile** to avoid drift.
- **Mod config:** no remote config UI on dedis — configure Smart! client-side and copy the config files to the
  server; mismatched configs can misbehave. (Relevant for Smart!'s `Smart_Config`.)
- Restart the server after any mod change.

## D. Local test loop for #176 (Approach B)

Per `Development/TestingResources.adoc`: run the dedi locally (`FactoryServer.exe -log -EpicPortal
-NoSteamClient`), claim once with a normal client, then connect the launch-script client via `open
127.0.0.1`. Every iteration needs the **server-target** mod recompiled and redeployed (Alpakit can copy to a
network path like `//host/share/satisfactory` for a remote box), and the server restarted to reload files.

## Current state (2026-06-08) — Windows dedi already installed, first blocker found

A Windows dedicated server is **already installed** at
`<SteamLibrary>\steamapps\common\SatisfactoryDedicatedServer` and is **version-matched to the client**:
`CL491125 / GameVersion 1.2.2.2 / branch rel-main-1.2.0` (Steam app 1690800, buildid 23300422). It is
**unclaimed** (no SaveGames, no Session/Admin config yet).

**First concrete #176 blocker — stale server-side SML.** Launching the vanilla server
(`FactoryServer.exe -log -unattended -NoSteamClient`) **crashes on boot**:

```
Failed to load '...\Mods\SML\Binaries\Win64\FactoryServer-SML-Win64-Shipping.dll' (GetLastError=127)
  Missing import: PSAPI.DLL
Plugin 'SML' failed to load because module 'SML' could not be loaded.
```

Cause: the server has **SML 3.11.3 (DLL dated 2026-01-02, `GameVersion ">=416835"` = pre-1.2)** installed,
but the server binaries are the **June 1.2 build (CL491125)**. `GetLastError=127` is ERROR_PROC_NOT_FOUND —
PSAPI.DLL *is* present on the system, so this is the stale pre-1.2 SML DLL being ABI-incompatible with the
1.2 engine (export mismatch), not a missing system file. The client/dev env is on **SML 3.12.0** (and the
dev env already has a current server-target SML DLL dated 2026-06-07).

**Fix (mod-manager, maintainer-driven — do NOT hand-copy binaries):** update the server install's SML to the
**3.12.0 / 1.2 server target** via SMM or ficsit-cli pointed at the server folder. Re-applying the managed
profile should pull the correct `WindowsServer` (and later `LinuxServer`) target binaries. Do **not** copy
the dev-env `FactoryServer-SML-Win64-Shipping.dll` over the stale one by hand — the cooked pak/BuildId must
match (same trap as the content-cook BuildId rule in AGENTS.md). After SML 3.12.0 loads cleanly (vanilla-ish
boot), the SmartFoundations **server target** (31.0.2 Windows server package) can be applied on top.

Sequence to a working modded Windows dedi:
1. Mod manager → server install → update **SML → 3.12.0** (server target). Boot vanilla-clean.
2. Claim the server from a normal game client (set admin password + server name, create/select a save).
3. Apply the **SmartFoundations server package** at the version matching the client under test.
4. Connect client via `open 127.0.0.1`; run the MP tests.

## Open setup decisions (to confirm before standing one up)

- **Where:** local Windows box (same machine as dev), a separate Linux box/VM, or both? (Both eventually, for
  target coverage; pick the first to certify.)
- **Branch:** stable vs experimental dedi (must match the client branch we test against).
- **Server-target packaging:** confirm Alpakit produces the Windows *and* Linux server packages for
  SmartFoundations; if Linux doesn't build/cook, that's the first Linux #176 task.
