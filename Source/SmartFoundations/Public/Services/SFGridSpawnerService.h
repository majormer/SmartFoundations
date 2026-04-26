#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SFGridSpawnerService.generated.h"

class USFSubsystem;

UCLASS()
class USFGridSpawnerService : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USFSubsystem* InSubsystem);

	// Phase 2: Facade methods; initially delegate back to subsystem's existing logic
	void RegenerateChildHologramGrid();
	void UpdateChildPositions();
	void UpdateChildrenForCurrentTransform();
	// Child destroy helpers (centralized)
	void QueueChildForDestroy(class AFGHologram* Child);
	void FlushPendingDestroy();
	void ForceDestroyPendingChildren();
	bool CanSafelyDestroyChildren() const;

private:
	TWeakObjectPtr<USFSubsystem> Subsystem;
};
