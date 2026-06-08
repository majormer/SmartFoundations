---
title: SML / Satisfactory Multiplayer — Grounded Reference Notes
type: REFERENCE
date: 2026-06-08
status: Active
category: Features
related:
  - ./PLAN_MultiplayerSupport_Revalidation_1.2.md
  - ./PLAN_MultiplayerSupport_Matrix.md
---

# SML / Satisfactory Multiplayer — Grounded Reference Notes

Distilled from the official Satisfactory modding documentation
(`satisfactorymodding/Documentation`, cloned locally for grounding; HEAD `ea7cc38` as of 2026-06-08).
Source `.adoc` pages live under the cloned repo's `modules/ROOT/pages/Development/Satisfactory/` and
`.../Development/` trees. This file is the on-our-side summary so MP work stays grounded in real CSS/SML
guidance rather than assumptions. Where this and our own planning docs agree, that agreement is now
*sourced*; where vanilla behavior is uncertain, the docs say "test it" — which is why Slice 0 is a live test.

## Core principle (matches our approach)

> "Mods coded properly for multiplayer will also work in singleplayer — no additional code paths are
> required specifically for singleplayer."

So: no SP-only branches where a server-authoritative path exists. As soon as a mod **handles user input or
uses custom logic to modify game state** (exactly what Smart! does), it needs MP consideration.

## Authority

- Check authority with `AActor::HasAuthority()`. If you lack an actor, use the **GameState** as the actor to
  check against (`UGameplayStatics::GetGameState`).
- **This is the fix shape for #334**: our post-build hooks (`OnPowerPoleBuilt`, pipe deferred-wiring,
  stackable-belt chain registration, dismantle proxy spawn) currently run on every peer with no
  `HasAuthority()` gate — see `PLAN_MultiplayerSupport_Revalidation_1.2.md`.

## Avoid Player Controller index 0 (validates our GetFirstPlayerController audit)

> "Dedicated servers have no player controller, so you should generally avoid using Get Player Controller
> with index 0… it will break if the game ever implements local splitscreen support."

- This is the official basis for our 8 build-path `GetFirstPlayerController()` hazards. UI-only use on a side
  that *has* a valid PC0 can work, but build/authority logic must not depend on PC0.
- **Dedicated-server (#176) consequence:** any path that assumes a local PC0 is outright broken on a dedi
  (no PC0 at all), not merely "wrong player." Our 8 hazard sites must route the requesting PC through the RCO.

## RCO vs Replicated Subsystem (the routing rule)

| Use an **RCO** when… | Use a **Replicated Mod Subsystem** when… |
|---|---|
| RPC goes client → server | RPC goes server → one/more clients |
| Triggered by a client action | Triggered automatically / without client action |
| — | The RPC is **Multicast** |

- **Never multicast on an RCO** — RCOs are owned per-player-controller and don't exist on every connection;
  multicasting one will **softlock and crash** clients. Multicast belongs on a replicated subsystem.
- RCOs are spawned by the server per player controller and network-owned by that controller (that ownership
  is what lets client→server RPCs fire at all).
- Retrieve at runtime: `AFGPlayerController::GetRemoteCallObjectByClass(UMyRCO::StaticClass())`. Can return
  **nullptr** (e.g. before registration completes) — client code needs null-checks + fallback logging.
- Register via the Game Instance Module's `Remote Call Objects` array (Smart! registers `USFRCO` in
  `SmartFoundations.cpp:73`).

### RCO C++ gotcha (we must verify USFRCO complies)

An RCO **must have at least one replicated `UPROPERTY`** and must add it to `GetLifetimeReplicatedProps`,
or Unreal won't replicate the RCO and RPCs silently won't work:

```cpp
UCLASS()
class UDocModRCO : public UFGRemoteCallObject {
    GENERATED_BODY()
public:
    UFUNCTION(Server, WithValidation, Reliable)
    void DoThingRPC(ADocMachine* context, bool bData);

    UPROPERTY(Replicated)
    bool bDummy = true;          // must exist
};

void UDocModRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const {
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UDocModRCO, bDummy);   // must be registered
}
```

> **CONFIRMED GAP (2026-06-08):** `USFRCO` (`Public/SFRCO.h`) has **no replicated `UPROPERTY` and no
> `GetLifetimeReplicatedProps` override**. Per the official guidance above, this means the RCO does not
> replicate and **its client→server RPCs do not actually work in MP** — almost certainly why the scaling
> input path mutates local state directly (`SFSubsystem.cpp:938`) and never calls `Server_ApplyScaling`.
> This is a real, concrete fix item, not just a planning note: add a `UPROPERTY(Replicated) bool bDummy`
> and register it in `GetLifetimeReplicatedProps`. Sequence it into Slice 1 (it's a prerequisite for *any*
> Smart RCO route working, including the #334 power-wire request).

- Pass **one context parameter** so the server knows which object to act on, and **revalidate on the server**
  (treat client input as proposed). `Server, WithValidation, Reliable` is the standard signature.

## Inventory / property replication

- Replication Detail Components are **gone** as of 1.0 — replaced by **Conditional Property Replication**
  (CSS-custom, reduces data sent to clients). See `ConditionalPropertyReplication.adoc` for inventory
  components. Relevant if Smart! ever needs to read/replicate inventory state for cost validation.
- **TMaps don't replicate** — replicate an array of structs (or array of values when keys are derivable) and
  rebuild the map in an `OnRep`. Avoid two parallel key/value arrays (arrival not synchronized).

## Local multiplayer test harness (grounds Slice 0)

**Approach A — two game clients (fastest iteration, our default):**
1. Launch two copies of the Steam client detached from the launcher (the docs' Quick Launch Script does this;
   normally Steam blocks a second copy). Flags of note: `-Multiprocess` (don't clobber shared .ini),
   `-CustomConfig=` (more consistent MP GUIDs), `-log -NewConsole`.
2. **Host copy:** load a save; before loading, **Load Settings → Session Type = `IP`**.
3. **Client copy:** Join Game → `127.0.0.1`, or run the `open 127.0.0.1` console command (or
   `-clientAutoJoin` in the script).
- Caveat: this approach yields **inconsistent player GUIDs** across launches. If a stable GUID is needed,
  use a real Epic/Steam copy for one side (requires compiling for both targets).

**Approach B — local dedicated server + client (required to certify #176):**
- Install a local dedi (`FactoryServer.exe -log -EpicPortal -NoSteamClient`), claim it once with a normally
  launched client, then connect via `open 127.0.0.1`. Requires compiling the mod for the **server target**
  too. This is the path for dedicated-server certification (#176), to run **after** listen-server parity holds.

**For our build flow:** Smart! ships a Shipping client DLL via the AGENTS.md golden path. Approach A only
needs the client target, so it fits the existing build loop; Approach B (dedi) adds a server-target compile —
defer until the dedicated-server slice.

## Where the full source pages live (cloned, for deeper reading)

- `Development/Satisfactory/Multiplayer.adoc` — the canonical page (authority, RCO, subsystems, replication).
- `Development/Satisfactory/ConditionalPropertyReplication.adoc` — inventory replication (1.0+).
- `Development/Satisfactory/DedicatedServerAPIDocs.adoc` + `Development/UpdatingToDedi.adoc` — dedi specifics (#176).
- `Development/TestingResources.adoc` — the launch script + MP test approaches above.
- `Development/ModLoader/Subsystems.adoc` — replicated mod subsystem replication-policy field.
- ExampleMod (`Mods/GameFeatures/ExampleMod/` in the SML monorepo) — `Widget_MultiplayerDemoBuilding`,
  `Build_MultiplayerDemoBuilding` (replicated property + RepNotify), `ReplicationExampleSubsystem`
  (multicast), and the `CC_ExampleReplication` chat command (the "don't multicast on an RCO" demo).
