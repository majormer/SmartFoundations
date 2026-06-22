// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Features/Walk/SFWalkTypes.h"
#include "Features/Walk/SFWalkCommitSpec.h"
#include "SFWalkService.generated.h"

class USFSubsystem;
class USFWalkConveyance;
class AFGHologram;

/**
 * Smart Walking — the conveyance-agnostic walk core (Slice 0 skeleton).
 *
 * Owns the Path as DATA: an ordered list of Segments (the source of truth) plus the origin frame.
 * Committed holograms are a PROJECTION of that list, world-anchored and rebuildable from any index
 * forward. "Committed/frozen" means excluded from the grid reposition + auto-connect sweep
 * (walk holograms are spawned standalone via SpawnHologramFromRecipe, never as grid children, and
 * the subsystem early-returns from ApplyAxisScaling while walk mode is active) — NOT immutable.
 *
 * Slice 0 places a placeholder cross-section (one hologram of the seed's recipe per segment).
 * The real Belt cross-section + ISFWalkConveyance adapter arrive in Slice 2.
 *
 * See docs/Sprints/CONCEPT_SmartWalking.md ("Locked design & implementation plan").
 */
UCLASS()
class SMARTFOUNDATIONS_API USFWalkService : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(USFSubsystem* InSubsystem);

    /** True while a Path is being walked. */
    bool IsActive() const { return bActive; }

    /** Which conveyance the active walk is laying (belt vs pipe) - for the HUD badge + panel labels. */
    ESFWalkConveyanceType GetConveyanceType() const { return ConveyanceType; }

    /**
     * Begin a walk seeded from the held build-gun hologram. Captures the origin frame from the seed,
     * seeds the first active segment, and spawns its placeholder. Returns false if it can't start.
     */
    bool EnterWalk(AFGHologram* SeedHologram);

    /** End the walk. bCommit just discards the local PREVIEW: the actual build is the staged commit spec the
     *  server reconstructs (BuildCommitSpec -> Server_StageWalkCommit -> ReconstructWalkCommitOnServer), so the
     *  client side only needs to tear down its preview holograms either way. */
    void ExitWalk(bool bCommit);

    /** Slice 3: snapshot the live walk session into a wire-safe, PARAMETERS-ONLY commit spec (origin frame +
     *  per-segment deltas + belt mode/tier + seed class + summed preview cost). Returns an invalid spec (bValid
     *  false = clear) when there is nothing to commit. Client-side; staged via USFRCO::Server_StageWalkCommit. */
    FSFWalkCommitSpec BuildCommitSpec() const;

    /** Slice 3 (SERVER, at the construct seam): reconstruct the walk poles + belts from a staged commit spec,
     *  AddChild'd to the seed hologram and pre-wired, so the vanilla scope() cascade builds them (mirrors
     *  USFExtendService::ReconstructCommitOnServer). Re-derives every frame from the deltas via AccumulateFrame -
     *  uses LOCAL state only, never the live member session. Returns the number of child holograms spawned. */
    int32 ReconstructWalkCommitOnServer(AFGHologram* SeedHologram, const FSFWalkCommitSpec& Spec);

    /** Commit-on-scale: lock the active segment and start a new active one inheriting the heading. */
    void CommitActiveAndAdvance();

    /** Destructive back-up: pop the active segment and re-activate the previous one. */
    void BackUp();

    /** Set the four authored adjusters on the active segment and re-derive downstream (Slice 1 uses this). */
    void SetActiveAdjusters(float Advance, float TurnDegrees, float Rise, float Shift);

    /** Set the four adjusters on ANY segment by index and re-derive downstream from it — the back-end for editing a
     *  committed segment in the Walk panel table. No-op if the index is invalid or no walk is active. */
    void SetSegmentAtIndex(int32 Index, float Advance, float TurnDegrees, float Rise, float Shift);

    /** Add deltas to the active segment's adjusters (steering) and re-derive downstream. */
    void NudgeActive(float dAdvance, float dTurn, float dRise, float dShift);

    /** Belt-bus cross-section control (Scale Y = lanes, Scale Z = stacks). MVP drives this UNIFORMLY: the delta is
     *  applied to every segment and to the inherited default for new segments, then all holograms are rebuilt.
     *  Counts clamp to >= 1. (Future per-segment mode would apply the delta to the active segment only.) */
    void AdjustCrossSection(int32 DeltaLanes, int32 DeltaStacks);

    /** World frame at the END of the given segment index (forward kinematics from the origin frame).
     *  bTrace turns on per-step + result logging (passed true only from the placement paths so the hot
     *  UI-refresh callers don't spam the log). */
    FTransform FrameAtIndex(int32 EndIndex, bool bTrace = false) const;

    /** The Head: the active (last) segment's exit frame — what the Smart Camera latches (Slice 1). */
    FTransform GetHeadFrame() const;

    /** Number of segments currently in the Path. */
    int32 GetSegmentCount() const { return Segments.Num(); }

    /** Read-only per-segment snapshot for the Walk widget (authored adjusters + derived exit heading). */
    TArray<FSFWalkSegmentView> GetSegmentViews() const;

    /** Index of the active (editable head) segment, or INDEX_NONE. */
    int32 GetActiveIndex() const { return ActiveIndex; }

    /** Re-route every segment's spanning element (belt or pipe) with the current routing settings (e.g. after the
     *  routing mode changes via the auto-connect settings) — the path/frames are unchanged, only the spans re-route. */
    void RerouteSpans();

    /** Per-frame (driven by the subsystem Tick while walk is active) re-clear of FGCDInitializing + force HMS_OK on
     *  the walk's STANDALONE preview holograms. Mirrors how the grid/Extend keep their un-ticked clone previews cyan
     *  (ResetConstructDisqualifiers then SetPlacementMaterialState). These holograms are deliberately NOT AddChild'd
     *  (that crashes the build gun's disqualifier recursion), so nothing else ever clears the init flag. */
    void RefreshWalkValidity();

    /** World frame at the END of segment EndIndex for an arbitrary segment list + origin (pure forward kinematics).
     *  bTrace turns on per-step + result logging for diagnosing where each segment lands. */
    static FTransform AccumulateFrame(const TArray<FSFWalkSegment>& Segs, const FTransform& Origin, int32 EndIndex, bool bTrace = false);

    /**
     * Deterministic self-test of the core forward-kinematics (no world/seed needed): advance, turn, advance,
     * and verify the head lands where the math says. Returns "PASS ..." / "FAIL ..." with the computed head.
     */
    UFUNCTION(BlueprintCallable, Category = "Smart Walking")
    static FString RunSelfTest();

private:
    /** Spawn the full NumLanes×NumStacks cross-section of pole holograms for a segment at its derived frame, then
     *  link their belts. Poles are flat-indexed (lane * NumStacks + stack). */
    void SpawnSegmentHologram(int32 Index);

    /** Spawn + render-prep one standalone preview pole at a world Pose (the per-pole worker used by the bus loop). */
    AFGHologram* SpawnOnePole(const FTransform& Pose, int32 Index, int32 Lane, int32 Stack);

    /** World pose of a cross-section cell at SIGNED indices (±N = N cells right/left of, or above/below, center)
     *  relative to a segment's CENTER frame — lateral for SignedLane, vertical for SignedStack, via the established
     *  CalculateChildPosition offsets. */
    FTransform CrossSectionPose(const FTransform& Center, int32 SignedLane, int32 SignedStack) const;

    /** The pole a belt connects FROM for cross-section cell PoleIndex of segment Index: the previous segment's pole
     *  at the SAME cell; for segment 0 only cell 0 links back (to the single held seed), other cells start here. */
    AFGHologram* PredecessorAnchorAt(int32 Index, int32 PoleIndex) const;

    /** Create or re-route every spanning element (belt or pipe, one per cross-section cell) spanning the predecessor
     *  poles → this segment. */
    void UpdateSegmentSpans(int32 Index);

    /** Reposition the holograms of every segment from StartIndex forward to their derived frames. */
    void RepositionFrom(int32 StartIndex);

    /** Destroy + respawn ALL segments' holograms (used when the cross-section count changes — pole count differs). */
    void RebuildHolograms();

    /** (Re)build the ORIGIN cross-section at the seed frame: cell (0,0) IS the held seed pole; the other cells are
     *  spawned so segment 0 connects back to a full-width origin (not a single seed). Flat-indexed like a segment. */
    void RebuildOriginHolograms();

    /** Destroy and forget a segment's holograms. */
    void DestroySegmentHolograms(FSFWalkSegment& Segment);

    /** Tear down all preview holograms and clear the segment list. */
    void ClearAll();

    /** True if the player can afford the whole walk (all non-seed holograms' summed cost vs inventory + central storage;
     *  always true under No Build Cost). Drives the red/cyan preview state in RefreshWalkValidity. */
    bool CanAffordWalk() const;

    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** The build-gun hologram the walk was seeded from (for recipe/owner/instigator context). */
    UPROPERTY()
    TWeakObjectPtr<AFGHologram> SeedHologram;

    /** Origin frame (the seed's transform at EnterWalk). Segment 0 advances forward from here. */
    FTransform OriginFrame = FTransform::Identity;

    /** The Path: ordered segment list — the source of truth. */
    UPROPERTY()
    TArray<FSFWalkSegment> Segments;

    /** The ORIGIN cross-section poles at the seed frame, flat-indexed (lane*|stacks|+stack). [0] = the held seed
     *  pole itself (never destroyed by the walk); the rest are walk-spawned so the bus starts full-width. */
    TArray<TWeakObjectPtr<AFGHologram>> OriginHolograms;

    /** The conveyance adapter that produces the spanning element between anchors (belt or pipe). */
    UPROPERTY()
    TObjectPtr<USFWalkConveyance> Conveyance;

    /** Which conveyance the active walk lays (belt vs pipe), selected from the seed buildable at EnterWalk;
     *  travels in the commit spec so the server reconstructs the matching adapter. */
    ESFWalkConveyanceType ConveyanceType = ESFWalkConveyanceType::Belt;

    /** Active (editable head) segment index. */
    int32 ActiveIndex = INDEX_NONE;

    /** Current belt-bus cross-section (MVP uniform), SIGNED like the grid's GridCounters (|val| = count, sign =
     *  side): applied to every segment + inherited by new ones. AdjustCrossSection steps these with the grid's
     *  forbidden-0/-1 skip, so Scale-Y grows one side per scroll and flips through center. */
    int32 CrossSectionLanes = 1;
    int32 CrossSectionStacks = 1;

    bool bActive = false;
};
