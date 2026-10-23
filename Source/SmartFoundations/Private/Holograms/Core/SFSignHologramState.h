// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"
class AFGHologram;
class AFGSignPoleHologram;
struct FSFScalingSpec;

/** Shared sign/pole state for grid previews and authority construction expansion. */
namespace SFSignHologramState
{
	/** Native children exist; preserve the quote through local construction or client serialization. */
	class FLocalConstructionScope
	{
	public:
		explicit FLocalConstructionScope(AFGHologram* InParent);
		~FLocalConstructionScope();
		FLocalConstructionScope(const FLocalConstructionScope&) = delete;
		FLocalConstructionScope& operator=(const FLocalConstructionScope&) = delete;
		const AFGHologram* Parent;
		TArray<FItemAmount> Cost;
		const FLocalConstructionScope* Previous;
	private:
		TWeakObjectPtr<AFGSignPoleHologram> Support;
		bool bWasDisabled = false;
	};
	bool IsLocalGridConstruction(AFGHologram* Parent);
	/** Active client grid whose native child tree is taking the legacy serialization path. */
	bool IsClientGridSerialization(AFGHologram* Parent);
	bool GetLocalConstructionCost(const AFGHologram* Parent, TArray<FItemAmount>& Cost);
	/** Direct-mounted signs construct in the snap step with their native support disabled. */
	bool IsDirectMount(AFGHologram* Hologram);
	void Sync(AFGHologram* Parent, AFGHologram* Child);
	/** Apply a proxy's captured support state to a native cell during local construction. */
	bool ApplyProxyConstruction(AFGHologram* Parent, AFGHologram* Child, bool bSupportWanted,
		double SupportHeight, const FTransform& SupportPose);
	/** Sync the active grid and hide an excluded parent pole without changing native placement steps. */
	void SyncGridPreview(AFGHologram* Parent);
	/** Shared cell policy for preview and inline/deferred authority reconstruction. */
	void ApplyGridCell(AFGHologram* Parent, AFGHologram* Child, const FIntVector& Cell, const FSFScalingSpec& Spec);
	bool AreGridSupportsValid(AFGHologram* Parent);
	/** Reject an owned grid at the final height step, preserving native first-step input. */
	bool ShouldRejectGridPreview(AFGHologram* Parent);
	/** Remove only the hidden native parent pole from vanilla preview aggregation. */
	bool AdjustPreviewCost(const AFGHologram* Parent, TArray<FItemAmount>& Cost);
	/** Price signs and the support boundary separately; native parent remains the height-step template. */
	bool GetGridCost(const AFGHologram* Parent, const FSFScalingSpec& Spec, TArray<FItemAmount>& Cost);
	/** Called after expansion/template capture, immediately before native construction. */
	void ApplyParentConstruction(AFGHologram* Parent, const FSFScalingSpec& Spec);
}
