---
title: Smart Multiplayer — Client/Server Construction Model
type: DESIGN
date: 2026-06-09
status: Draft
category: Features
related:
  - ./PLAN_MultiplayerSupport_Revalidation_1.2.md
  - ./REF_SMLMultiplayerNotes.md
  - ../Scaling/IMPL_Scaling_CurrentFlow.md
  - ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md
  - ../Extend/IMPL_Extend_CurrentFlow.md
issues:
  - 176  # UMBRELLA — multiplayer / dedicated server support
  - 334  # auto-connect spawns client-only actors
---

# Smart Multiplayer — Client/Server Construction Model

## Why this doc exists

Slice 0 established that Smart's current multi-buildable construction does not work in
multiplayer past a hard ceiling, and root-caused why. This doc proposes the **networking model**
to replace it — derived from how the base game and established SML conventions already do
MP-safe mass placement — and adapts that model to Smart's harder requirement: every placed
buildable must remain **individually interactable** and **wired to its neighbours**.

This is a model/architecture doc, not an implementation plan. It defines the boundary and the
data that crosses it; per-feature implementation slices come after.

## The two problems we must solve together

1. **The construction ceiling (Slice 0).** Smart builds an arrangement as a parent hologram with
   N child holograms, then commits the whole tree through the vanilla build-gun fire. That fire
   serializes the entire hologram tree (children included) into a single reliable
   `Server_ConstructHologram` message. The message is bounded by the engine's reliable-bunch byte
   ceiling (**65536 bytes**); past ~135 children it fails **all-or-nothing**, and a dropped
   oversized message never reaches the server, so the orphaned client previews are never cleaned
   up. The payload is **O(N)** in the number of placed buildables — the architecture scales the
   wire cost with the build size, which is the root defect.

2. **Authority leakage in post-build wiring (#334).** Auto-connect (power, belts, pipes) defers
   real wiring to post-build hooks that currently run on **every peer with no authority guard**,
   so a client's auto-connect spawns actors the server never sees. Construction and wiring are
   two halves of the same problem: both must be **server-authoritative**.

A correct model has to dissolve **both** at once.

## The proven pattern (engine + SML conventions)

The base game's own multi-placement ("zoop") and the standard SML client/server conventions
already place many buildables in MP without ever hitting problem (1). The reusable ideas:

- **The boundary is drawn at _intent_, not at _objects_.** The client never ships constructed
  geometry across the wire. It ships a **compact description** of what to build.
- **The amount/extent is a compact, replicated property, expanded server-side.** In the engine's
  zoop, the extent is a single `FIntVector` declared `UPROPERTY(ReplicatedUsing=..., CustomSerialization)`
  on the buildable hologram; the server reads it during construction and **expands it into N
  buildables locally**. The wire payload is **O(1)** regardless of N — structurally incapable of
  hitting the 64 KB ceiling.
- **Client/server subsystem split.** A server-authoritative subsystem (replicated, spawn-on-server)
  holds the canonical state and limits; a client-local subsystem (spawn-on-client, never
  replicated) holds transient preview state. Client→server intent travels through a thin Remote
  Call Object whose RPCs carry **only small value parameters** (vectors, ints, class refs, bools).
- **Construction yields real, first-class buildables.** Because the server constructs through the
  normal buildable path, each result is an independent, replicated `AFGBuildable` — selectable,
  dismantlable, clipboard-able, and individually addressable. There is no blob to unpack.
- **The server is the authority.** It clamps requested amounts to its own limits (anti-cheat) and
  validates cost/placement. Preview-only logic is guarded off on dedicated servers.

The relevant takeaway for us is the **model**, not the tooling. The tooling places dumb copies.

## Why Smart cannot simply reuse that tooling

The engine's mass placement and the mods built on it place **independent, identical copies** with
**no logistics between them**. Smart's value is precisely the part that pattern omits:

- the **interactions** between placed objects — auto-connect, manifold belts/pipes, power, lift
  and passthrough stitching;
- per-object **transforms**, spacing, stepping, rotation, and topology-aware placement;
- treating each placed buildable as an individually interactable object afterwards.

So we adopt the **transport and authority model**, and supply our **own server-side expansion +
wiring** for everything the copy-placement path doesn't do.

## The model adapted for Smart

Principle: **the client previews locally and sends a compact reconstruction _spec_; the server
reconstructs the arrangement, constructs each buildable individually, then wires them — all under
authority. Everything replicates natively.**

### Components

1. **Preview — client-local (unchanged).** The child holograms, arrows, HUD, and material states
   stay exactly as they are today. Previews are cosmetic and need no replication. This is already
   MP-safe.

2. **Reconstruction spec — compact and deterministic.** A small, serializable description from
   which the server can rebuild the exact arrangement the client previewed:
   - base build class(es) and the base/anchor transform;
   - per-axis counts, spacing, stepping, rotation (scaling), and/or
   - the captured topology + clone plan (Extend: source chains, distributor/manifold plan,
     belt/pipe/lift roles).
   Smart already produces almost exactly this today (the JSON spawn data and topology capture used
   to build the child holograms). The change is to treat that data as the **authoritative spec
   sent to the server**, rather than as a recipe the client executes into holograms it then ships.

3. **Transport — two viable options.**
   - **(A) Hologram-carried spec (preferred for placement).** Put the spec on a custom parent
     hologram as a `CustomSerialization`-replicated property and **expand it in the hologram's
     server-side `Construct` override** — mirroring how the engine's zoop extent rides the normal
     build-gun fire. Payload stays O(1); no bespoke chunking; construction still triggers from
     real player input (which is the only thing that constructs).
   - **(B) RCO-carried spec.** The client sends the spec through `USFRCO`; the server reconstructs
     and constructs on receipt. More control and decoupled from the build-gun fire, but the server
     must drive construction itself. Best where there is no single hologram to ride (e.g. replays,
     restore, batch operations).

4. **Server construction — individual buildables.** The server reconstructs from the spec and
   constructs **each buildable on its own** through the normal construct path. Results are real,
   replicated, individually interactable actors — not a serialized tree. Failure is **per-buildable**,
   not all-or-nothing, so partial success is recoverable and orphan cleanup is local.

5. **Server-authoritative wiring (this is the #334 fix).** After the buildables exist server-side,
   the server performs auto-connect / belt / pipe / power construction behind `HasAuthority`. The
   connections are real and replicate to all clients. Post-build hooks that currently run on every
   peer get gated to the server.

6. **Authority & validation.** The server validates cost, placement, and limits, and clamps the
   request to its own bounds before constructing. The client cannot forge state.

### Client/server subsystem split

Mirror the proven split: a **server subsystem** (replicated, authoritative limits + state) and a
**client subsystem** (local preview state, never replicated). `USFRCO` is already replication-fixed
(dummy replicated property + `GetLifetimeReplicatedProps`), so the intent channel is ready.

## How today differs from the target

| Layer | Today | Target model |
|---|---|---|
| Preview | client-local child holograms | client-local (unchanged) |
| Wire payload | N serialized child holograms (O(N), 64 KB cap) | compact spec (O(1)) |
| Construction | one atomic server blob | server constructs each buildable individually |
| Interactions | implicit in the blob | server-side wiring under `HasAuthority` |
| Result | all-or-nothing, orphan-prone | N real replicated actors; partial-failure recoverable |

## Mapping to Smart features

- **Scaling (grid).** Spec = base class + base transform + per-axis counts + spacing/stepping +
  rotation. Strong fit for transport option (A).
- **Extend.** Spec = server-side topology walk result + clone count + manifold/role plan. The
  server-side topology-walk RCO already planned for Extend is the front half of this; construction
  + wiring is the back half. Transport (A) or (B).
- **Auto-connect / Upgrade / Restore / Dismantle.** Same principle — route the mutation as intent
  to the server, perform it under authority, let actors replicate. Restore/batch lean toward (B).

## Hard requirements & risks

- **Determinism.** The server's reconstruction from the spec must match the client's preview
  exactly (same spacing/rotation/topology math, same class resolution). Reuse the existing capture
  logic on both sides to guarantee this; do not re-derive independently.
- **Construction trigger.** Only real player input constructs via the build gun. Option (A) rides
  this naturally; option (B) requires the server to drive construction without a build-gun fire —
  validate this path early (it is the same wall Slice 0 hit when driving construction manually).
- **Cost.** Charge cost server-side from the authoritative inventory; preview affordability stays
  client-side and advisory.
- **Spec size.** The spec must stay compact (counts/params/topology), never an inline list of N
  fully-described buildables, or we reintroduce the O(N) payload.
- **Partial failure.** Define per-buildable failure handling and client preview reconciliation
  (the absence of this caused the Slice 0 orphan ghosts).

## Suggested incremental slices

1. **Scaling via hologram-carried spec (A)** — prove O(1) construction of a grid of one building
   type, server-authoritative, individually interactable. Foundations (lightweight) first, then
   factory machines.
2. **#334 server-authoritative wiring** — gate auto-connect/power/belt/pipe post-build hooks behind
   `HasAuthority`; wire server-side; confirm replication.
3. **Extend** — server topology walk → spec → server construction → server wiring.
4. Fold remaining features (Upgrade / Restore / Dismantle) onto the same intent→authority model.

Per the workstream rule, none of this ships until **all** features work in MP; the existing
oversized-grid guard remains the interim safety measure, not a feature.

## Prerequisites already in place

- `USFRCO` replication fix (dummy replicated property + `GetLifetimeReplicatedProps`) — the
  client→server intent channel works.
- Existing client-side capture/spawn data (scaling params, Extend topology/JSON) — the raw
  material for the reconstruction spec.
