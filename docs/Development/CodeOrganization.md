---
title: Code Organization Policy
type: POLICY
date: 2026-06-12
status: Active
category: Development
---

# Code Organization Policy

This document defines how Smart!'s C++ source is organized: where code lives, how
files are named and split, and how single-player and multiplayer concerns are kept
separable. It exists so the codebase stays cheap to own over a long support life —
bugs are easy to locate (multiplayer-only ones especially), and adding a feature is a
deliberate act of filling in known slots rather than splicing into shared files.

> **Status:** This is the **target** state. The tree does not fully conform yet; the
> gaps are listed in [§7](#7-current-gaps). Migration is tracked as separate work — new
> and changed code should follow this policy now; existing code is brought into
> conformance feature by feature.

---

## 1. Principles

1. **Locality beats cleverness.** Everything one feature needs should sit together. If
   fixing a feature means opening five folders, the layout has failed.
2. **Multiplayer is structural, not incidental.** A multiplayer-only bug must have a
   small, obvious search space. Where SP and MP behavior diverge is documented, named,
   and greppable — never something you have to reconstruct by reading the whole feature.
3. **New features are outlined, not spliced.** There is a known shape a feature takes
   (§6). A new system lands in that shape from day one, including its multiplayer story.
4. **Shared means shared.** The shared/infrastructure layer holds only code used by two
   or more features. Feature-specific code never accumulates there.

---

## 2. The two axes: feature slices over a shared core

Code is organized on two axes:

- **Vertical — `Features/<Name>/`:** a self-contained slice owning everything *specific*
  to that feature: its services, its holograms, its UI, its data, and its multiplayer
  seam.
- **Horizontal — the shared layer:** `Core/`, `Subsystem/`, `Services/`, `Shared/`,
  `Holograms/`, `UI/`, `HUD/`, `Input/`, `Data/` — code used across features.

**The deciding rule:**

> Code used by exactly one feature lives **in that feature**.
> Code used by two or more features lives in the **shared layer**.
> When a second feature starts needing a feature-local component, promote it to the
> shared layer in the same change that introduces the second use — never reach across
> into another feature's slice.

This keeps the shared layer small and intentional, and keeps each feature legible on its
own.

**A note on holograms (a common misread):** the hologram base hierarchy
(`ASFSmartHologram` → `ASFFactoryHologram` → the child-hologram tree) and any hologram used
by two or more features are legitimately **shared** and stay in `Holograms/`. Do not push base
classes down into a feature slice. Only a hologram specific to exactly one feature (e.g. that
feature's own preview hologram) belongs in its `Features/<Name>/Holograms/`.

---

## 3. Directory structure

`Private/` and `Public/` mirror each other exactly. A subtree may exist under `Public/`
with no `Private/` counterpart **only** for header-only code (interfaces, constants,
templates); such a subtree must be obvious from its contents. The reverse
(`Private/`-only) is normal for implementation with no exported header.

Canonical layout (both `Private/` and `Public/`):

```
SmartFoundations/
  Core/                 module class, logging, constants, small shared helpers
    Net/                shared networking foundation: RCO base, network helpers,
                        the construct/replication hook registrar
  Data/                 shared data assets and registries
  Shared/               cross-feature building blocks (e.g. Conduits)
  Subsystem/            the central subsystem and cross-feature services
  Services/             shared utility services (chain-actor, radar pulse, ...)
  Holograms/            SHARED hologram base classes and cross-feature holograms only
  UI/                   shared UI shell (Smart Panel, settings form)
  HUD/                  shared HUD infrastructure
  Input/                shared input infrastructure
  Features/
    <Name>/             one vertical slice
      <Name>Service.*   the feature's logic
      Holograms/        holograms specific to this feature
      UI/               UI specific to this feature
      Data/             data specific to this feature
      Net/              this feature's multiplayer seam (§5)
```

**No loose translation units.** Every `.cpp`/`.h` lives under a domain subdirectory.
Nothing sits directly at the `Private/` or `Public/` root — **with one exception: the module
identity and logging core** (`SmartFoundations.h`/`.cpp` — the `IMPLEMENT_MODULE` class and the
`LogSmartFoundations` category — and its companion `SFLogMacros.h`). These are the conventional UE
module-root files, `SmartFoundations.h` is included by ~90 TUs for the log category, and relocating
them buys nothing but churn. They stay at the module root by design.

---

## 4. Files, naming, and splitting

- **One class, one primary file**, named for the class (`SFExtendService.cpp/.h`).
- **When a class must be split**, use **concern-named partials**:
  `SFExtendService_Wiring.cpp`, `SFExtendService_Topology.cpp`. The suffix names *what
  the partial contains*. **Ordinal splits (`_Part2`, `_2`) are not allowed** — they tell
  a reader nothing about where to look.
- **Private implementation headers** (shared declarations between a class's partials, not
  exported) are named `<Class>Impl.h` and live beside the implementation in `Private/`.
- **Soft size budget: ~1,500 lines.** Crossing it is a signal to split by concern, not a
  hard limit. The goal is comprehension, not a line count — a cohesive 1,700-line file is
  better than four arbitrary 400-line fragments. Split when *concerns* separate cleanly.
- **Header hygiene:** public headers expose the minimum surface; prefer forward
  declarations; keep implementation includes in the `.cpp`.

---

## 5. Single-player / multiplayer organization

Multiplayer behavior is made locatable three ways. Together they answer "where is the
multiplayer code for this feature?" without reading the feature end to end.

### 5.1 Extractable seams have a home

Code that exists *only* for networking is moved out of feature logic into a `Net/` area:

- **Shared net foundation → `Core/Net/`:** the RCO base, network/authority helpers, the
  registrar that installs construct/replication hooks.
- **Per-feature seam → `Features/<Name>/Net/`:** that feature's RCO endpoints, its
  construct-message hooks, its server-side spec/commit staging and expansion.

A reader looking for a feature's networking opens one folder.

### 5.2 Inline authority branches follow a tagged taxonomy

Some divergence cannot be extracted — an authority check lives in the middle of a build
path. These branches are marked with one **semantic** tag set so a single `grep` for
`[MP-` finds every divergence, and the suffix says what kind:

| Tag | Meaning |
|---|---|
| `[MP-AUTH]`   | server/authority-only code path |
| `[MP-CLIENT]` | client-only code path |
| `[MP-SEAM]`   | the client → server boundary (RCO call, construct message, staging) |
| `[MP-REPL]`   | replication / `OnRep` / `CustomSerialization` concern |

This replaces ad-hoc, issue-scoped tags (`[MP-334]`, `[EXTEND-MP]`, ...). Reference an
issue number *in addition* to the semantic tag when useful, never instead of it.

### 5.3 Every diverging feature carries a divergence map

A feature whose behavior differs under client vs. authority declares it in a structured
header block at the top of its main file (and, for complex features, a short note in its
`Net/` area):

```cpp
// SP/MP DIVERGENCE MAP — Extend
//  1. Cost charge      — [MP-AUTH] server scales per-cell cost from the staged spec
//  2. Clone topology   — [MP-SEAM] client stages commit via RCO; server reconstructs
//  3. Designer context — [MP-REPL] mBlueprintDesigner does not cross the construct
//                        message; re-derived authoritatively at the construct seam
```

The map is the **entry point for a multiplayer-only bug**: instead of reading the whole
feature, you read its handful of divergence points and go straight to the relevant one.
Every multiplayer fix in a feature should leave its divergence map accurate.

---

## 6. Adding a new feature

A new feature is created by instantiating the slice, not by editing shared files. The
shape below is the template; a feature only includes the parts it needs.

1. **Create `Features/<Name>/`** (mirrored in `Private/` and `Public/`).
2. **Logic:** `<Name>Service` (and concern-named partials if it grows).
3. **Holograms / UI / Data:** feature-specific ones go in the feature's `Holograms/`,
   `UI/`, `Data/`. Shared bases stay in the shared layer (§2 rule).
4. **Registration:** wire the feature in at the central subsystem's documented
   registration point — the subsystem discovers features through that seam, so adding one
   does not mean editing unrelated subsystem code.
5. **Multiplayer story, up front (§5):** decide the authority model before writing the
   build path. Network-only code goes in `Features/<Name>/Net/`; unavoidable inline
   branches get `[MP-*]` tags; the feature ships with a divergence map even if it starts
   nearly empty.
6. **Docs:** an `IMPL_<Name>_CurrentFlow.md` under `docs/Features/<Name>/`.

A feature that introduces a whole new player-interaction system (its own input, HUD,
state, and an authority story) is exactly the case this shape is for: it becomes a
predictable slice with a `Net/` seam and a divergence map, rather than threads woven by
hand through the subsystem and the HUD.

---

## 7. Adoption

New and changed code follows this policy now. Existing code is brought into conformance
**feature by feature**, each migration its own reviewable change so behavior stays verifiable
at every step.

The current conformance backlog — what does not yet conform, and in what order it is migrated —
is **transient state**. It lives with the migration effort (its tracking issue / working doc),
**not** in this policy. That separation is deliberate: a policy states the timeless target, so it
never goes stale the moment a migration lands.
