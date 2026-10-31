// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Features/Walk/SFWalkCommitSpec.h"

class AFGHologram;
class AController;
class USceneComponent;
struct FNetConstructionID;

enum class ESFWalkOriginSelection : uint8 { NewWalk, Existing, Rejected };

/** Native read-only selection shared by preview and authority. No temporary buildable materialization. */
namespace SFWalkExistingOrigin
{
    SMARTFOUNDATIONS_API ESFWalkOriginSelection Discover(AFGHologram* Seed, AController* Controller,
        ESFWalkConveyanceType Type, FSFWalkCommitSpec& Origin, FString& Reason);
    SMARTFOUNDATIONS_API USceneComponent* Resolve(const FSFWalkExistingEndpoint& Endpoint,
        ESFWalkConveyanceType Type, UWorld* World);
    SMARTFOUNDATIONS_API void SetAnchorPose(AFGHologram* Hologram, const FTransform& Pose);
    SMARTFOUNDATIONS_API bool IsNativeStartPosition(AFGHologram* Span, const FVector& Position);
    /** Read before native construction can retire the preview registry. */
    SMARTFOUNDATIONS_API USceneComponent* GetSelectedStart(AFGHologram* Span);
    SMARTFOUNDATIONS_API bool ConnectNativeStart(AFGHologram* Span, AFGBuildable* BuiltSpan);
    /** Belt success means exact links retained for native-phase settlement, not connected yet. */
    SMARTFOUNDATIONS_API bool ConnectNativeStart(AFGHologram* Span, AFGBuildable* BuiltSpan,
        const FNetConstructionID* ConstructionId);
    SMARTFOUNDATIONS_API bool ValidateGeometry(const FSFWalkCommitSpec& Spec, const FVector& ConnectorOffset);
    SMARTFOUNDATIONS_API bool Validate(const FSFWalkCommitSpec& Spec, UWorld* World);
    SMARTFOUNDATIONS_API bool TryConnectorOffset(AFGHologram* Seed, ESFWalkConveyanceType Type, FVector& Offset);
}
