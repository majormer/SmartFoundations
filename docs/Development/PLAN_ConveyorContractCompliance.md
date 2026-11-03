---
title: Extend, Restore and Upgrade — Native Conveyor Compliance
type: DESIGN
status: In progress; bounded stock belt/lift Windows standalone validation; full acceptance pending
category: Development
tags: [conveyors, chain-actors, extend, restore, upgrade, authority, lifecycle]
---

# Native conveyor compliance plan

Make Smart's Extend, Restore and Upgrade consumers preserve the native conveyor
contracts in the [research paper](../Reference/THESIS_Belts_ChainActors_Placement.md).
The recovered native baseline is Satisfactory CL 513885. The paper identifies
the Smart source revision it audited; implementation must compare the current
call sites with that baseline before treating a source finding as unchanged.

This is an implementation and acceptance plan, not a claim that Smart is already
compliant. It does not authorize a release or implement a general chain-repair
mod. Preserve ordinary vanilla construction, material costs and vanilla-loadable
saves. Existing corrupt ownership is a separate recovery problem: reject an
unsafe operation with an actionable result rather than attempting undocumented
global repair.

## Acceptance contract

| Concern | Required result |
|---|---|
| Authority and identity | Only authority changes topology, cargo or payment. Requests and deferred work belong to an exact operation, world generation and actor/component identities. |
| Native lifecycle | Configuration, construction, BeginPlay, native registration, wiring and migration follow the selected adapter's actual ordering. No duplicate registration or invocation of a destroyed chain. |
| Topology | Intended reciprocal edges, effective input/output ports, passthrough references, cached traversal and native ownership agree after the applicable native phases. Preserve unrelated groups and pending requests. |
| Cargo | Account for each supported descriptor, count and actual typed state across all affected inventories, local arrays, chains and native refund/overflow destinations. Do not count client visual copies as authoritative cargo. |
| Materials | Charge and refund through one identified owner, exactly once. A quote or reservation is not evidence of actual settlement. Construction materials and in-transit cargo have separate ledgers. |
| Completion | Report success only after the operation's effects and obligations are verified. Pending migration, outstanding callbacks, uncertain payment and unresolved identities remain outstanding work. |
| Persistence and clients | Coherent results survive save/load and reconstruct on joining clients. Client peer-pointer availability is not an authoritative reciprocity oracle. |
| Scope | Support native belts, lifts, relevant passthroughs and ordinary/smart/programmable/priority distributors according to their distinct contracts; do not infer support for arbitrary modded subclasses. |

The paper's limitations remain binding. Linux payment debit was not established;
runtime typed-payload preservation was not certified; same-process reconnect
failed while fresh-client rejoin succeeded. Controlled segment multipart used a
supported diagnostic setting, not default-budget exhaustion. These are not Smart
acceptance passes. Close a limitation when an implementation depends on it, or
define and enforce an explicit unsupported boundary.

## 1. Map current consumers and freeze the acceptance matrix

Reuse the paper's nineteen source-entry dispositions and operation checkpoints.
For every current caller, record its admission path, native adapter, lifecycle
phase, identity map, wiring callback, registration responsibility, cargo owner,
payer and terminal-result owner. Include synchronous and deferred paths; do not
audit only the main service method.

Start with these existing responsibilities:

- `Features/Extend`: captured value topology, clone roles, built-actor mapping,
  replay and wiring.
- `Features/Restore/SFRestoreService`: preset validation and delegation through
  `ReplayRestoreCloneTopology`.
- `Core/Net/SFGameInstanceModule_SpecHooks`: authoritative staged construction,
  returned-child mapping and final wiring.
- `Subsystem/SFSubsystem_OnActorSpawned`: deferred wiring that can overlap the
  synchronous path.
- `Features/Upgrade`: selection, reservation, replacement adapters, settings,
  connections, refunds and overflow.
- `Services/SFChainActorService`: invalidation, re-registration, deferred purge
  and direct native metadata edits.

These paths are relative to `Source/SmartFoundations/Private/`. Follow the
[organization policy](CodeOrganization.md): feature-specific work stays in its
slice; shared construction/authority infrastructure serves multiple consumers.

**Exit:** every reachable consumer has a named contract and an acceptance oracle.
Preserve historical failure receipts and do-not-retest boundaries. Reopen native
analysis only for a concrete missing premise, changed binary or distinct branch.

## 2. Establish shared operation ownership and the mutation boundary

The conveyor Upgrade entry now retains request identity and value intent through
the UI/RPC, queues admission through the shared coordinator, expands native owner
leases before effects, and retains correlated terminal results. Cancellation does
not discard unresolved ownership, and stale client results cannot complete a newer
request. The initial physical adapter now uses native replacement/retirement,
immediate per-owner cargo comparison, measured full-target debit, earned original
refund and overflow checks, followed by native post-phase topology/membership
verification. It does not invoke the legacy global repair path. This source path
has bounded Windows standalone evidence for stock belt and empty/cargo lift
replacement, including fresh-process persistence of the cargo lift and saved
payer. This does not certify all native lifetimes, consumers or network modes.
Typed cargo, actual passthrough/child references,
designer and nonordinary group shapes still reject before effects pending their
required adapters. Those gates do not reduce the final support contract. Native
callback execution, request delivery, resource/cargo outcomes and all required
branches still need acceptance evidence.

The request protocol now carries an authority-issued world generation rather than
assigning the current generation only after receipt. Context synchronization is
separate from intent capture and retains no operation. Admission, replay intent,
cancellation and result correlation compare the supplied generation. Verify this
over actual RPCs, including stale delivery, before treating the protocol as qualified.

Introduce an operation record shared by these consumers, with value intent,
expected graph, affected cohort, exact returned identities, resource obligations,
deferred work and observed results. Use explicit states such as admitted,
materializing, awaiting native settlement, settling, completed, canceled before
effects, partially applied and uncertain. State names do not themselves provide
atomicity or rollback.

Serialize conflicting work over overlapping affected cohorts. Revalidate actor
and world generations when queued work executes. Deduplicate repeated network
requests and arbitrate synchronous versus deferred wiring so each effect has one
owner. A weak pointer rejecting a callback does not settle earlier effects.

Choose the native mutation seam from the paper's pre-handler and worker-completion
contract. Establish receiver lifetime and the completion fence actually required
by the selected path; being on the game thread or returning from a synchronous
loop is insufficient. Do not assume that fence completes independent next-tick
timers, replicated reconstruction or every external writer.

**Exit:** meaningful tests cover conflicting operations, duplicate delivery,
stale generations and late callbacks. Each rejection proves either no effects
occurred or records the remaining effects and obligations honestly.

## 3. Replace unsafe chain manipulation for coherent operations

Resolve the affected native cohort before mutation, including connected members
and current cargo owners rather than only selected actors. Replace the paper's
identified hazards as one coordinated call graph:

- Reinstalling or rebuilding a chain after requesting its destruction.
- Calling a reverting path on poisoned segment owners while describing it as
  non-reverting cleanup.
- Editing group membership/order without reconciling cached traversal and member
  tick state, or using endpoint order opposite native traversal.
- Clearing a global pending list and rebuilding only Smart's selected subset.
- Treating client-chain tick deregistration or one array scrub as authoritative
  topology teardown.

Use the applicable native lifecycle and migration mechanisms, with their real
preconditions. There is no universal remove/edit/add recipe. BeginPlay may have
registered the conveyor already; identity bookkeeping is not native registration.
Retain genuine pending native transitions rather than repeatedly rebuilding them.

**Exit:** supported coherent operations preserve unrelated requests and complete
membership metadata. Unsafe or contradictory input is rejected without invoking
the hazardous recovery paths. Remove obsolete fallback callers only after their
replacement and rejection behavior are covered.

## 4. Make Extend and Restore use one materialization contract

Extend captures value intent; Restore validates preset values and delegates to the
same replay/materialization path. Neither clones historical chain pointers,
bucket indices, actor identities, paid resources or in-transit cargo.

Authority validates the plan that will actually be built: compatible recipes and
classes, geometry, child tree, effective ports, intended edges, resources and
placement eligibility. Preserve native configuration and lifecycle ordering.
Map only the actual returned actors to unique clone roles; ambiguous mappings
fail explicitly rather than selecting nearby actors. A returned Respline
replacement must supersede the old identity everywhere the operation uses it.

Apply additional wiring only where needed, using actual effective ports. Keep
lift-to-passthrough and passthrough-to-lift references consistent without treating
those references as direct factory connections. Reconcile synchronous wiring and
actor-spawn callbacks through the operation record. Keep that record until graph,
membership, cargo changes caused by existing neighbors and payment are settled.

Preserve Smart's whole-grid placement promise. Validate preventable failures
before effects; do not invent native rollback. If an unexpected failure can leave
partial construction, establish a safe compensation strategy or gate that route
before presenting it as an all-or-nothing implementation.

**Exit:** Extend and Restore share materialization invariants and truthful terminal
reporting. Preset replay does not duplicate cargo or material credit; stale or
incompatible presets cannot bypass authority validation.

## 5. Reconcile Upgrade adapters and resource settlement

Keep belt and lift replacement distinct. Belt Upgrade directly exports chain
cargo; lift Upgrade depends on the old owner's state and surrounding registration
order. The paper's export-versus-refund outcomes prohibit assuming either universal
retention on the replacement or universal cargo loss.

For each supported replacement adapter:

1. Capture old identity, affected peers, geometry, settings, group/proxy membership,
   cargo owner and payer before effects; revalidate at execution.
2. Choose the native outer transaction or explicitly owned lower-level route.
   Record which caller constructs, debits, refunds, retires and invokes deferred
   effects; suppress duplicate owners rather than adding another cleanup pass.
3. Preserve the adapter's native configuration and lifecycle order. Establish the
   cargo-ownership transition required by its promise, with a complete affected
   ledger. Native cargo refunds must not also be counted as retained cargo.
4. Replace every old-to-new mapping, including passthrough reverse references and
   any returned replacement identity, before dependent wiring/settings work.
5. Settle actual construction debit, old-material refund, cargo refund and overflow
   delivery independently. Failures retain their real partial/uncertain result;
   reservation release alone is not proof that resources were returned.
6. Verify final topology and ownership after native settlement before reporting
   completion. Retirement and callbacks cannot use the old actor afterward.

Qualify the actual Smart payment route early. The unresolved native Linux debit
observation cannot certify custom Smart accounting. Do not modify unrelated
upgrade families merely to impose a conveyor-specific sequence on them.

**Exit:** one owner for every charge, refund, retirement and callback; supported
cargo outcomes and graph preservation verified for both belts and lifts.

## 6. Validate changed Smart behavior in discriminating slices

Reuse the native baseline and its evidence. New tests prove the changed Smart
consumer; they do not repeat the native placement study. Select cases by distinct
adapter, ownership transition, port assignment or authority premise, not the
Cartesian product of tiers, orientations and platforms.

| Slice | Required evidence |
|---|---|
| Shared orchestration | Conflict handling, request deduplication, stale generations, identity ambiguity, exactly-once effects and cancellation after partial application. Use focused tests and controlled fault injection without reproducing historical corrupt saves. |
| Extend | Fresh and connected populated neighbors; effective belt/lift ports; exact returned-child/Respline mappings; no cloned cargo; actual material debit. |
| Restore | Compatible and rejected presets through the same Extend path; stale preset/request rejection; equivalent final graph and payment behavior. |
| Upgrade | Belt and lift ownership branches represented by the paper, including retained-versus-refunded cargo where materially distinct; actual debit/refund/overflow and old-actor retirement. |
| Variants | Passthrough direction and reverse references; distinct ordinary/smart/programmable/priority routing or mounted-port branches touched by the change. Reuse shared-base proofs for equivalent tiers. |
| Lifecycle | Native pending migration, deferred wiring/settings, interruption and save/load with complete affected-cohort comparisons. |
| Platforms | Qualify the changed authoritative paths in Windows single-player, Windows dedicated and Linux dedicated with a real client. Verify deployed identities, role selection and relevant ABI; reuse established equivalent mechanisms. |
| Client reconstruction | Compare authoritative cargo/graph with eventual client reconstruction on join and fresh rejoin. Test multipart/reset behavior only when the consumer changes that premise; do not infer server damage from an unavailable client peer pointer. |

For supported stateful items, capture actual typed payload and legal native path
selection before asserting preservation. If a path cannot support that contract,
enforce and document the boundary rather than substituting descriptor counts.

Each acceptance receipt records source/binary identity, operation and world
generation, prediction, raw before/after outcomes, contradictions, resource and
cargo ledgers, remaining callbacks and reviewed limits. A build, a crash-free
session or a changed chain count alone is insufficient. Compile the affected
targets and run appropriate focused tests before runtime qualification.

## Delivery order and completion

Land reviewable slices in dependency order: consumer/acceptance mapping; shared
operation and mutation ownership; safe coherent-topology adapters; Extend/Restore;
Upgrade and payment; cross-mode persistence and client validation. A slice may
remain disabled for an unqualified route; existing unsafe behavior must not become
its fallback. Preserve working nonconveyor paths and unrelated local changes.

During execution, keep an ignored checkpoint with immutable case identities,
completed slices, reviewed evidence, failed predictions and the next finite
action. Do not mark a case complete merely because context is being refreshed.
Update this durable plan and the feature implementation references when behavior
actually changes; keep transient commands and machine-specific details out of
committed documentation.

Completion requires every reachable supported consumer to have a reviewed
native-contract mapping and acceptance evidence; no outstanding ownership,
registration, resource or deferred-effect uncertainty on a reported-success
path; preserved save/client behavior; and explicit enforced limits for unsupported
cases. Address reported bugs against that implementation afterward. This plan
does not start implementation, approve a release or certify a repair mod.
