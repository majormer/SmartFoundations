// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "SFWalkTypes.generated.h"

class AFGHologram;

/**
 * Smart Walking — one Segment of a Path.
 *
 * A Segment is one cross-section placement (one anchor). Its AUTHORED data is just the four
 * transform adjustments, expressed in the heading-local frame of the previous segment's end —
 * this struct is the SOURCE OF TRUTH. World frames are DERIVED via forward kinematics
 * (Frame_n = Frame_{n-1} * LocalDelta()), never stored.
 *
 * Convention (locked design, "advance-then-rotate, pivot at the anchor"): the anchor is placed
 * one Advance forward along the CURRENT heading; the Turn then rotates the heading for the NEXT
 * segment. So a segment's Turn shows up as its EXIT heading, and the anchor sits straight ahead.
 *
 * See docs/Sprints/CONCEPT_SmartWalking.md.
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFWalkSegment
{
    GENERATED_BODY()

    /** Forward advance to this anchor along the current heading, in cm. (maps to Spacing) Defaults to the
     *  auto-connect max stackable-pole spacing (MAX_SPACING_FOR_BELT = 5400 cm / 54 m, the safe margin under the
     *  ~56 m belt cap) so the run uses the full belt range like auto-connect until the user shrinks it. */
    UPROPERTY(BlueprintReadWrite, Category = "Smart Walking")
    float Advance = 5400.0f;

    /** Yaw turn applied at the END of this segment (heading for the next leg), degrees, right = positive. (maps to Rotation) */
    UPROPERTY(BlueprintReadWrite, Category = "Smart Walking")
    float TurnDegrees = 0.0f;

    /** Vertical rise of this segment, in cm. (maps to Steps) */
    UPROPERTY(BlueprintReadWrite, Category = "Smart Walking")
    float Rise = 0.0f;

    /** Lateral sidestep perpendicular to heading, in cm. Reserved (single-lane MVP). (maps to Stagger) */
    UPROPERTY(BlueprintReadWrite, Category = "Smart Walking")
    float Shift = 0.0f;

    /** Belt-bus cross-section counters, SIGNED exactly like the grid's GridCounters: |value| = number of LANES
     *  (Scale Y, perpendicular to heading) / vertical STACKS (Scale Z); the SIGN is the side (+ = right/up,
     *  - = left/down). 1 = single centered belt; the forbidden values 0 and -1 are skipped (matching grid
     *  scaling), so a lane grows ONE side per scroll. Stored per-segment so a future per-segment bus is just an
     *  input change; the MVP drives them uniformly. */
    UPROPERTY(BlueprintReadWrite, Category = "Smart Walking")
    int32 NumLanes = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Smart Walking")
    int32 NumStacks = 1;

    /** The preview cross-section pole holograms for this segment — NumLanes×NumStacks of them, flat-indexed by
     *  (lane * NumStacks + stack). Runtime only. */
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AFGHologram>> Holograms;

    /** The spanning belts from the PREVIOUS segment's poles to this segment's, one per (lane,stack), same flat
     *  indexing as Holograms. Runtime only. */
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AFGHologram>> Belts;

    /**
     * Local-frame delta this segment applies to the running frame.
     * Translation = advance/shift/rise along the CURRENT heading; rotation = the heading turn applied AFTER,
     * so composing Frame_{n-1} * LocalDelta() advances along the current heading, then turns for the next leg.
     */
    FTransform LocalDelta() const
    {
        return FTransform(FRotator(0.0f, TurnDegrees, 0.0f), FVector(Advance, Shift, Rise));
    }
};

/**
 * A read-only snapshot of one segment for the Walk widget's segment list/timeline row.
 * Authored adjusters + the DERIVED exit heading (the frontier bearing the camera also tracks).
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFWalkSegmentView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") int32 Index = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") float Advance = 0.0f;      // cm
    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") float TurnDegrees = 0.0f;  // deg, right = +
    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") float Rise = 0.0f;         // cm
    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") float Shift = 0.0f;        // cm
    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") float ExitHeadingDeg = 0.0f; // absolute yaw at this segment's exit
    UPROPERTY(BlueprintReadOnly, Category = "Smart Walking") bool bActive = false;
};
