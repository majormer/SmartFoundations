#include "Features/Restore/SFRestoreService.h"
#include "Features/Restore/SFRestoreTypes.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFGridStateService.h"
#include "Services/SFRecipeManagementService.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendTypes.h"
#include "SmartFoundations.h"

// Satisfactory includes
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGBuildDescriptor.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Hologram/FGHologram.h"
#include "FGCharacterPlayer.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"

// Engine includes
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "TimerManager.h"

namespace
{
	FSFCounterState BuildCounterStateFromPreset(const FSFCounterState& CurrentState, const FSFRestorePreset& Preset)
	{
		FSFCounterState State = CurrentState;

		if (Preset.CaptureFlags.bGrid)
		{
			State.GridCounters = Preset.GridCounters;
		}

		if (Preset.CaptureFlags.bSpacing)
		{
			State.SpacingX = Preset.SpacingX;
			State.SpacingY = Preset.SpacingY;
			State.SpacingZ = Preset.SpacingZ;
		}

		if (Preset.CaptureFlags.bSteps)
		{
			State.StepsX = Preset.StepsX;
			State.StepsY = Preset.StepsY;
		}

		if (Preset.CaptureFlags.bStagger)
		{
			State.StaggerX = Preset.StaggerX;
			State.StaggerY = Preset.StaggerY;
			State.StaggerZX = Preset.StaggerZX;
			State.StaggerZY = Preset.StaggerZY;
		}

		if (Preset.CaptureFlags.bRotation)
		{
			State.RotationZ = Preset.RotationZ;
		}

		return State;
	}

	bool IsActiveHologramReadyForPreset(AFGHologram* ActiveHologram, const FSFRestorePreset& Preset)
	{
		if (!ActiveHologram || !IsValid(ActiveHologram) || !ActiveHologram->GetBuildClass())
		{
			return false;
		}

		if (!Preset.BuildingClassName.IsEmpty())
		{
			UClass* ActiveRecipe = ActiveHologram->GetRecipe();
			if (!ActiveRecipe || ActiveRecipe->GetName() != Preset.BuildingClassName)
			{
				return false;
			}
		}

		return true;
	}

	TSharedPtr<FJsonObject> VecToJson(const FSFVec3& Vec)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("x"), Vec.X);
		Obj->SetNumberField(TEXT("y"), Vec.Y);
		Obj->SetNumberField(TEXT("z"), Vec.Z);
		return Obj;
	}

	void JsonToVec(const TSharedPtr<FJsonObject>& Obj, FSFVec3& OutVec)
	{
		if (!Obj.IsValid()) return;
		double Val = 0.0;
		if (Obj->TryGetNumberField(TEXT("x"), Val)) OutVec.X = static_cast<float>(Val);
		if (Obj->TryGetNumberField(TEXT("y"), Val)) OutVec.Y = static_cast<float>(Val);
		if (Obj->TryGetNumberField(TEXT("z"), Val)) OutVec.Z = static_cast<float>(Val);
	}

	TSharedPtr<FJsonObject> RotToJson(const FSFRot3& Rot)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("pitch"), Rot.Pitch);
		Obj->SetNumberField(TEXT("yaw"), Rot.Yaw);
		Obj->SetNumberField(TEXT("roll"), Rot.Roll);
		return Obj;
	}

	void JsonToRot(const TSharedPtr<FJsonObject>& Obj, FSFRot3& OutRot)
	{
		if (!Obj.IsValid()) return;
		double Val = 0.0;
		if (Obj->TryGetNumberField(TEXT("pitch"), Val)) OutRot.Pitch = static_cast<float>(Val);
		if (Obj->TryGetNumberField(TEXT("yaw"), Val)) OutRot.Yaw = static_cast<float>(Val);
		if (Obj->TryGetNumberField(TEXT("roll"), Val)) OutRot.Roll = static_cast<float>(Val);
	}

	TSharedPtr<FJsonObject> TransformToJson(const FSFTransform& Transform)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetObjectField(TEXT("location"), VecToJson(Transform.Location));
		Obj->SetObjectField(TEXT("rotation"), RotToJson(Transform.Rotation));
		return Obj;
	}

	void JsonToTransform(const TSharedPtr<FJsonObject>& Obj, FSFTransform& OutTransform)
	{
		if (!Obj.IsValid()) return;
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Obj->TryGetObjectField(TEXT("location"), Child) && Child) JsonToVec(*Child, OutTransform.Location);
		if (Obj->TryGetObjectField(TEXT("rotation"), Child) && Child) JsonToRot(*Child, OutTransform.Rotation);
	}

	TSharedPtr<FJsonObject> ConnectionRefToJson(const FSFConnectionRef& Ref)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("target"), Ref.Target);
		Obj->SetStringField(TEXT("connector"), Ref.Connector);
		return Obj;
	}

	void JsonToConnectionRef(const TSharedPtr<FJsonObject>& Obj, FSFConnectionRef& OutRef)
	{
		if (!Obj.IsValid()) return;
		Obj->TryGetStringField(TEXT("target"), OutRef.Target);
		Obj->TryGetStringField(TEXT("connector"), OutRef.Connector);
	}

	TSharedPtr<FJsonObject> ConnectionsToJson(const FSFConnections& Connections)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetObjectField(TEXT("conveyorAny0"), ConnectionRefToJson(Connections.ConveyorAny0));
		Obj->SetObjectField(TEXT("conveyorAny1"), ConnectionRefToJson(Connections.ConveyorAny1));
		return Obj;
	}

	void JsonToConnections(const TSharedPtr<FJsonObject>& Obj, FSFConnections& OutConnections)
	{
		if (!Obj.IsValid()) return;
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Obj->TryGetObjectField(TEXT("conveyorAny0"), Child) && Child) JsonToConnectionRef(*Child, OutConnections.ConveyorAny0);
		if (Obj->TryGetObjectField(TEXT("conveyorAny1"), Child) && Child) JsonToConnectionRef(*Child, OutConnections.ConveyorAny1);
	}

	TSharedPtr<FJsonObject> SplinePointToJson(const FSFSplinePoint& Point)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetObjectField(TEXT("local"), VecToJson(Point.Local));
		Obj->SetObjectField(TEXT("world"), VecToJson(Point.World));
		Obj->SetObjectField(TEXT("arriveTangent"), VecToJson(Point.ArriveTangent));
		Obj->SetObjectField(TEXT("leaveTangent"), VecToJson(Point.LeaveTangent));
		return Obj;
	}

	void JsonToSplinePoint(const TSharedPtr<FJsonObject>& Obj, FSFSplinePoint& OutPoint)
	{
		if (!Obj.IsValid()) return;
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Obj->TryGetObjectField(TEXT("local"), Child) && Child) JsonToVec(*Child, OutPoint.Local);
		if (Obj->TryGetObjectField(TEXT("world"), Child) && Child) JsonToVec(*Child, OutPoint.World);
		if (Obj->TryGetObjectField(TEXT("arriveTangent"), Child) && Child) JsonToVec(*Child, OutPoint.ArriveTangent);
		if (Obj->TryGetObjectField(TEXT("leaveTangent"), Child) && Child) JsonToVec(*Child, OutPoint.LeaveTangent);
	}

	TSharedPtr<FJsonObject> SplineDataToJson(const FSFSplineData& Spline)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("length"), Spline.Length);
		TArray<TSharedPtr<FJsonValue>> Points;
		for (const FSFSplinePoint& Point : Spline.Points)
		{
			Points.Add(MakeShared<FJsonValueObject>(SplinePointToJson(Point)));
		}
		Obj->SetArrayField(TEXT("points"), Points);
		return Obj;
	}

	void JsonToSplineData(const TSharedPtr<FJsonObject>& Obj, FSFSplineData& OutSpline)
	{
		if (!Obj.IsValid()) return;
		double Val = 0.0;
		if (Obj->TryGetNumberField(TEXT("length"), Val)) OutSpline.Length = static_cast<float>(Val);
		OutSpline.Points.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
		if (Obj->TryGetArrayField(TEXT("points"), Points) && Points)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Points)
			{
				FSFSplinePoint Point;
				JsonToSplinePoint(Value.IsValid() ? Value->AsObject() : nullptr, Point);
				OutSpline.Points.Add(Point);
			}
		}
	}

	TSharedPtr<FJsonObject> LiftDataToJson(const FSFLiftData& Lift)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("height"), Lift.Height);
		Obj->SetBoolField(TEXT("isReversed"), Lift.bIsReversed);
		Obj->SetObjectField(TEXT("topTransform"), TransformToJson(Lift.TopTransform));
		Obj->SetObjectField(TEXT("bottomTransform"), TransformToJson(Lift.BottomTransform));
		TArray<TSharedPtr<FJsonValue>> Passthroughs;
		for (const FString& CloneId : Lift.PassthroughCloneIds)
		{
			Passthroughs.Add(MakeShared<FJsonValueString>(CloneId));
		}
		Obj->SetArrayField(TEXT("passthroughCloneIds"), Passthroughs);
		return Obj;
	}

	void JsonToLiftData(const TSharedPtr<FJsonObject>& Obj, FSFLiftData& OutLift)
	{
		if (!Obj.IsValid()) return;
		double Val = 0.0;
		if (Obj->TryGetNumberField(TEXT("height"), Val)) OutLift.Height = static_cast<float>(Val);
		Obj->TryGetBoolField(TEXT("isReversed"), OutLift.bIsReversed);
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Obj->TryGetObjectField(TEXT("topTransform"), Child) && Child) JsonToTransform(*Child, OutLift.TopTransform);
		if (Obj->TryGetObjectField(TEXT("bottomTransform"), Child) && Child) JsonToTransform(*Child, OutLift.BottomTransform);
		OutLift.PassthroughCloneIds.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Passthroughs = nullptr;
		if (Obj->TryGetArrayField(TEXT("passthroughCloneIds"), Passthroughs) && Passthroughs)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Passthroughs)
			{
				OutLift.PassthroughCloneIds.Add(Value.IsValid() ? Value->AsString() : FString());
			}
		}
	}

	TSharedPtr<FJsonObject> CloneHologramToJson(const FSFCloneHologram& Holo)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("hologramId"), Holo.HologramId);
		Obj->SetStringField(TEXT("role"), Holo.Role);
		Obj->SetStringField(TEXT("sourceId"), Holo.SourceId);
		Obj->SetStringField(TEXT("sourceClass"), Holo.SourceClass);
		Obj->SetStringField(TEXT("sourceChain"), Holo.SourceChain);
		Obj->SetNumberField(TEXT("sourceSegmentIndex"), Holo.SourceSegmentIndex);
		Obj->SetStringField(TEXT("hologramClass"), Holo.HologramClass);
		Obj->SetStringField(TEXT("buildClass"), Holo.BuildClass);
		Obj->SetStringField(TEXT("recipeClass"), Holo.RecipeClass);
		Obj->SetObjectField(TEXT("transform"), TransformToJson(Holo.Transform));
		Obj->SetObjectField(TEXT("splineData"), SplineDataToJson(Holo.SplineData));
		Obj->SetObjectField(TEXT("liftData"), LiftDataToJson(Holo.LiftData));
		Obj->SetBoolField(TEXT("hasSplineData"), Holo.bHasSplineData);
		Obj->SetBoolField(TEXT("hasLiftData"), Holo.bHasLiftData);
		Obj->SetObjectField(TEXT("sourceConnections"), ConnectionsToJson(Holo.SourceConnections));
		Obj->SetObjectField(TEXT("cloneConnections"), ConnectionsToJson(Holo.CloneConnections));
		Obj->SetBoolField(TEXT("constructible"), Holo.bConstructible);
		Obj->SetBoolField(TEXT("previewOnly"), Holo.bPreviewOnly);
		Obj->SetNumberField(TEXT("thickness"), Holo.Thickness);
		Obj->SetNumberField(TEXT("userFlowLimit"), Holo.UserFlowLimit);
		Obj->SetStringField(TEXT("connectedPowerPoleHologramId"), Holo.ConnectedPowerPoleHologramId);
		Obj->SetNumberField(TEXT("powerPoleMaxConnections"), Holo.PowerPoleMaxConnections);
		Obj->SetBoolField(TEXT("isLaneSegment"), Holo.bIsLaneSegment);
		Obj->SetStringField(TEXT("laneFromDistributorId"), Holo.LaneFromDistributorId);
		Obj->SetStringField(TEXT("laneFromConnector"), Holo.LaneFromConnector);
		Obj->SetStringField(TEXT("laneToDistributorId"), Holo.LaneToDistributorId);
		Obj->SetStringField(TEXT("laneToConnector"), Holo.LaneToConnector);
		Obj->SetStringField(TEXT("laneSegmentType"), Holo.LaneSegmentType);
		Obj->SetObjectField(TEXT("laneStartNormal"), VecToJson(Holo.LaneStartNormal));
		Obj->SetObjectField(TEXT("laneEndNormal"), VecToJson(Holo.LaneEndNormal));
		return Obj;
	}

	void JsonToCloneHologram(const TSharedPtr<FJsonObject>& Obj, FSFCloneHologram& OutHolo)
	{
		if (!Obj.IsValid()) return;
		double Val = 0.0;
		Obj->TryGetStringField(TEXT("hologramId"), OutHolo.HologramId);
		Obj->TryGetStringField(TEXT("role"), OutHolo.Role);
		Obj->TryGetStringField(TEXT("sourceId"), OutHolo.SourceId);
		Obj->TryGetStringField(TEXT("sourceClass"), OutHolo.SourceClass);
		Obj->TryGetStringField(TEXT("sourceChain"), OutHolo.SourceChain);
		if (Obj->TryGetNumberField(TEXT("sourceSegmentIndex"), Val)) OutHolo.SourceSegmentIndex = static_cast<int32>(Val);
		Obj->TryGetStringField(TEXT("hologramClass"), OutHolo.HologramClass);
		Obj->TryGetStringField(TEXT("buildClass"), OutHolo.BuildClass);
		Obj->TryGetStringField(TEXT("recipeClass"), OutHolo.RecipeClass);
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Obj->TryGetObjectField(TEXT("transform"), Child) && Child) JsonToTransform(*Child, OutHolo.Transform);
		if (Obj->TryGetObjectField(TEXT("splineData"), Child) && Child) JsonToSplineData(*Child, OutHolo.SplineData);
		if (Obj->TryGetObjectField(TEXT("liftData"), Child) && Child) JsonToLiftData(*Child, OutHolo.LiftData);
		Obj->TryGetBoolField(TEXT("hasSplineData"), OutHolo.bHasSplineData);
		Obj->TryGetBoolField(TEXT("hasLiftData"), OutHolo.bHasLiftData);
		if (Obj->TryGetObjectField(TEXT("sourceConnections"), Child) && Child) JsonToConnections(*Child, OutHolo.SourceConnections);
		if (Obj->TryGetObjectField(TEXT("cloneConnections"), Child) && Child) JsonToConnections(*Child, OutHolo.CloneConnections);
		Obj->TryGetBoolField(TEXT("constructible"), OutHolo.bConstructible);
		Obj->TryGetBoolField(TEXT("previewOnly"), OutHolo.bPreviewOnly);
		if (Obj->TryGetNumberField(TEXT("thickness"), Val)) OutHolo.Thickness = static_cast<float>(Val);
		if (Obj->TryGetNumberField(TEXT("userFlowLimit"), Val)) OutHolo.UserFlowLimit = static_cast<float>(Val);
		Obj->TryGetStringField(TEXT("connectedPowerPoleHologramId"), OutHolo.ConnectedPowerPoleHologramId);
		if (Obj->TryGetNumberField(TEXT("powerPoleMaxConnections"), Val)) OutHolo.PowerPoleMaxConnections = static_cast<int32>(Val);
		Obj->TryGetBoolField(TEXT("isLaneSegment"), OutHolo.bIsLaneSegment);
		Obj->TryGetStringField(TEXT("laneFromDistributorId"), OutHolo.LaneFromDistributorId);
		Obj->TryGetStringField(TEXT("laneFromConnector"), OutHolo.LaneFromConnector);
		Obj->TryGetStringField(TEXT("laneToDistributorId"), OutHolo.LaneToDistributorId);
		Obj->TryGetStringField(TEXT("laneToConnector"), OutHolo.LaneToConnector);
		Obj->TryGetStringField(TEXT("laneSegmentType"), OutHolo.LaneSegmentType);
		if (Obj->TryGetObjectField(TEXT("laneStartNormal"), Child) && Child) JsonToVec(*Child, OutHolo.LaneStartNormal);
		if (Obj->TryGetObjectField(TEXT("laneEndNormal"), Child) && Child) JsonToVec(*Child, OutHolo.LaneEndNormal);
	}

	TSharedPtr<FJsonObject> CloneTopologyToJson(const FSFCloneTopology& Topology)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("schemaVersion"), Topology.SchemaVersion);
		Obj->SetObjectField(TEXT("worldOffset"), VecToJson(Topology.WorldOffset));
		Obj->SetStringField(TEXT("sourceFactoryId"), Topology.SourceFactoryId);
		Obj->SetStringField(TEXT("parentBuildClass"), Topology.ParentBuildClass);
		Obj->SetObjectField(TEXT("parentTransform"), TransformToJson(Topology.ParentTransform));
		TArray<TSharedPtr<FJsonValue>> Children;
		for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
		{
			Children.Add(MakeShared<FJsonValueObject>(CloneHologramToJson(Holo)));
		}
		Obj->SetArrayField(TEXT("childHolograms"), Children);
		return Obj;
	}

	bool JsonToCloneTopology(const TSharedPtr<FJsonObject>& Obj, FSFCloneTopology& OutTopology)
	{
		if (!Obj.IsValid()) return false;
		OutTopology = FSFCloneTopology();
		Obj->TryGetStringField(TEXT("schemaVersion"), OutTopology.SchemaVersion);
		Obj->TryGetStringField(TEXT("sourceFactoryId"), OutTopology.SourceFactoryId);
		Obj->TryGetStringField(TEXT("parentBuildClass"), OutTopology.ParentBuildClass);
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Obj->TryGetObjectField(TEXT("worldOffset"), Child) && Child) JsonToVec(*Child, OutTopology.WorldOffset);
		if (Obj->TryGetObjectField(TEXT("parentTransform"), Child) && Child) JsonToTransform(*Child, OutTopology.ParentTransform);
		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		if (Obj->TryGetArrayField(TEXT("childHolograms"), Children) && Children)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Children)
			{
				FSFCloneHologram Holo;
				JsonToCloneHologram(Value.IsValid() ? Value->AsObject() : nullptr, Holo);
				OutTopology.ChildHolograms.Add(Holo);
			}
		}
		return OutTopology.ChildHolograms.Num() > 0;
	}
}

// ============================================================================
// Initialization
// ============================================================================

void USFRestoreService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore] Service initialized"));
}

// ============================================================================
// Capture
// ============================================================================

FSFRestorePreset USFRestoreService::CaptureCurrentState(
	const FString& Name,
	const FSFRestoreCaptureFlags& CaptureFlags) const
{
	FSFRestorePreset Preset;
	Preset.Name = Name;
	Preset.CaptureFlags = CaptureFlags;
	Preset.Version = SF_RESTORE_PRESET_VERSION;

	const FDateTime Now = FDateTime::UtcNow();
	Preset.CreatedAt = Now.ToIso8601();
	Preset.UpdatedAt = Preset.CreatedAt;

	if (!Subsystem.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("[SmartRestore] CaptureCurrentState: No subsystem"));
		return Preset;
	}

	// Building class — captured from the build gun's active recipe (not the production recipe).
	// The build gun recipe (e.g. "Recipe_Constructor_C") determines what building to place,
	// while the production recipe (e.g. "Recipe_Screw_C") determines what the building crafts.
	{
		UWorld* World = Subsystem->GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AFGCharacterPlayer* Character = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		AFGBuildGun* BuildGun = Character ? Character->GetBuildGun() : nullptr;
		if (BuildGun)
		{
			UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(
				BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
			if (BuildState)
			{
				UClass* BuildRecipe = BuildState->GetActiveRecipe();
				if (BuildRecipe)
				{
					Preset.BuildingClassName = BuildRecipe->GetName();
				}
			}
		}
	}

	// Counter state fields
	const FSFCounterState& State = Subsystem->GetCounterState();

	if (CaptureFlags.bGrid)
	{
		Preset.GridCounters = State.GridCounters;
	}

	if (CaptureFlags.bSpacing)
	{
		Preset.SpacingX = State.SpacingX;
		Preset.SpacingY = State.SpacingY;
		Preset.SpacingZ = State.SpacingZ;
	}

	if (CaptureFlags.bSteps)
	{
		Preset.StepsX = State.StepsX;
		Preset.StepsY = State.StepsY;
	}

	if (CaptureFlags.bStagger)
	{
		Preset.StaggerX = State.StaggerX;
		Preset.StaggerY = State.StaggerY;
		Preset.StaggerZX = State.StaggerZX;
		Preset.StaggerZY = State.StaggerZY;
	}

	if (CaptureFlags.bRotation)
	{
		Preset.RotationZ = State.RotationZ;
	}

	// Production recipe (separate from the building recipe used to select what to build)
	if (CaptureFlags.bRecipe)
	{
		TSubclassOf<UFGRecipe> ActiveRecipe = Subsystem->GetActiveRecipe();
		if (ActiveRecipe)
		{
			// Use read-only accessor to avoid rebuilding the cache (which resets CurrentRecipeIndex)
			if (USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService())
			{
				const TArray<TSubclassOf<UFGRecipe>>& FilteredRecipes = RecipeSvc->GetSortedFilteredRecipes();
				for (const TSubclassOf<UFGRecipe>& Recipe : FilteredRecipes)
				{
					if (Recipe == ActiveRecipe)
					{
						Preset.RecipeClassName = ActiveRecipe->GetName();
						break;
					}
				}
			}
		}
	}

	// Auto-connect runtime settings
	if (CaptureFlags.bAutoConnect)
	{
		const auto& AC = Subsystem->GetAutoConnectRuntimeSettings();
		Preset.AutoConnect.bBeltEnabled = AC.bEnabled;
		Preset.AutoConnect.BeltTierMain = AC.BeltTierMain;
		Preset.AutoConnect.BeltTierToBuilding = AC.BeltTierToBuilding;
		Preset.AutoConnect.bChainDistributors = AC.bChainDistributors;
		Preset.AutoConnect.BeltRoutingMode = AC.BeltRoutingMode;
		Preset.AutoConnect.bPipeEnabled = AC.bPipeAutoConnectEnabled;
		Preset.AutoConnect.PipeTierMain = AC.PipeTierMain;
		Preset.AutoConnect.PipeTierToBuilding = AC.PipeTierToBuilding;
		Preset.AutoConnect.bPipeIndicator = AC.bPipeIndicator;
		Preset.AutoConnect.PipeRoutingMode = AC.PipeRoutingMode;
		Preset.AutoConnect.bPowerEnabled = AC.bConnectPower;
		Preset.AutoConnect.PowerGridAxis = AC.PowerGridAxis;
		Preset.AutoConnect.PowerReserved = AC.PowerReserved;
	}

	if (USFExtendService* ExtendSvc = Subsystem->GetExtendService())
	{
		if (ExtendSvc->IsRestoredCloneTopologyActive())
		{
			TSharedPtr<FSFCloneTopology> CloneTopology = ExtendSvc->GetLastCloneTopology();
			if (CloneTopology.IsValid() && CloneTopology->ChildHolograms.Num() > 0)
			{
				Preset.bHasExtendTopology = true;
				Preset.ExtendCloneTopology = *CloneTopology;
				UE_LOG(LogSmartFoundations, Log,
					TEXT("[SmartRestore] CaptureCurrentState included staged Extend topology: preset='%s' childHolograms=%d"),
					*Name,
					Preset.ExtendCloneTopology.ChildHolograms.Num());
			}
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore] Captured preset '%s' (building: %s)"),
		*Name, *Preset.BuildingClassName);

	return Preset;
}

// ============================================================================
// Apply
// ============================================================================

bool USFRestoreService::ApplyPreset(const FSFRestorePreset& Preset)
{
	if (!Subsystem.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("[SmartRestore] ApplyPreset: No subsystem"));
		return false;
	}

	ActiveRestorePresetName = Preset.Name;
	bRestoreSessionActive = Preset.bHasExtendTopology;

	// 1. Switch build gun to the preset's building
	if (!Preset.BuildingClassName.IsEmpty())
	{
		if (!Subsystem->SetBuildGunByRecipeName(Preset.BuildingClassName))
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[SmartRestore] ApplyPreset: Failed to switch build gun to '%s'"),
				*Preset.BuildingClassName);
			return false;
		}
	}

	// 2. Apply counter state fields (only those with capture flag set)
	FSFCounterState State = BuildCounterStateFromPreset(Subsystem->GetCounterState(), Preset);

	Subsystem->UpdateCounterState(State);

	// 3. Apply production recipe
	// Find the recipe class by name and use SetActiveRecipeByClass, which
	// resolves the correct index in SortedFilteredRecipes internally.
	if (Preset.CaptureFlags.bRecipe && !Preset.RecipeClassName.IsEmpty())
	{
		if (USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService())
		{
			UWorld* World = Subsystem->GetWorld();
			AFGRecipeManager* RecipeManager = World ? AFGRecipeManager::Get(World) : nullptr;
			if (RecipeManager)
			{
				TArray<TSubclassOf<UFGRecipe>> AllRecipes;
				RecipeManager->GetAllAvailableRecipes(AllRecipes);
				TSubclassOf<UFGRecipe> TargetRecipe = nullptr;
				for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
				{
					if (Recipe && Recipe->GetName() == Preset.RecipeClassName)
					{
						TargetRecipe = Recipe;
						break;
					}
				}

				if (TargetRecipe)
				{
					if (RecipeSvc->SetActiveRecipeByClass(TargetRecipe))
					{
						UE_LOG(LogSmartFoundations, Display,
							TEXT("[SmartRestore] Applied production recipe '%s'"),
							*Preset.RecipeClassName);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("[SmartRestore] Recipe '%s' found but not in filtered list for current building"),
							*Preset.RecipeClassName);
					}
				}
			}
		}
	}

	// 4. Apply auto-connect settings
	if (Preset.CaptureFlags.bAutoConnect)
	{
		Subsystem->SetAutoConnectRuntimeSettingsFromPreset(Preset.AutoConnect);
	}

	if (Preset.bHasExtendTopology)
	{
		ReplayExtendTopologyWhenHologramReady(Preset, 12, 2);
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore] Applied preset '%s'"), *Preset.Name);
	return true;
}

void USFRestoreService::ClearActiveRestoreSession(const TCHAR* Reason)
{
	if (!bRestoreSessionActive && ActiveRestorePresetName.IsEmpty())
	{
		return;
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Cleared active restore session: preset='%s' reason=%s"),
		*ActiveRestorePresetName,
		Reason ? Reason : TEXT("Unknown"));

	bRestoreSessionActive = false;
	ActiveRestorePresetName.Empty();
}

void USFRestoreService::ReplayExtendTopologyWhenHologramReady(const FSFRestorePreset& Preset, int32 AttemptsRemaining, int32 SettleTicksRemaining)
{
	if (!bRestoreSessionActive || ActiveRestorePresetName != Preset.Name)
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady aborted: preset='%s' activePreset='%s' active=%d"),
			*Preset.Name,
			*ActiveRestorePresetName,
			bRestoreSessionActive ? 1 : 0);
		return;
	}

	if (!Subsystem.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: Subsystem invalid for preset '%s'"),
			*Preset.Name);
		return;
	}

	Subsystem->PollForActiveHologram();

	USFExtendService* ExtendSvc = Subsystem->GetExtendService();
	AFGHologram* ActiveHologram = Subsystem->GetActiveHologram();
	if (ExtendSvc && IsActiveHologramReadyForPreset(ActiveHologram, Preset))
	{
		if (SettleTicksRemaining > 0)
		{
			UWorld* World = Subsystem->GetWorld();
			if (!World)
			{
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: World null while settling preset '%s'"),
					*Preset.Name);
				return;
			}

			TWeakObjectPtr<USFRestoreService> WeakThis(this);
			FTimerDelegate SettleDelegate;
			SettleDelegate.BindLambda([WeakThis, Preset, AttemptsRemaining, SettleTicksRemaining]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->ReplayExtendTopologyWhenHologramReady(Preset, AttemptsRemaining, SettleTicksRemaining - 1);
				}
			});
			World->GetTimerManager().SetTimerForNextTick(SettleDelegate);
			UE_LOG(LogSmartFoundations, Log,
				TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: Settling active hologram for preset '%s' ticksRemaining=%d activeHologram=%s recipe=%s"),
				*Preset.Name,
				SettleTicksRemaining,
				*GetNameSafe(ActiveHologram),
				*GetNameSafe(ActiveHologram ? ActiveHologram->GetRecipe() : nullptr));
			return;
		}

		Subsystem->UpdateCounterState(BuildCounterStateFromPreset(Subsystem->GetCounterState(), Preset));
		const bool bReplaySucceeded = ExtendSvc->ReplayRestoreCloneTopology(ActiveHologram, Preset.ExtendCloneTopology);
		UE_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: preset='%s' success=%d activeHologram=%s recipe=%s childHolograms=%d"),
			*Preset.Name,
			bReplaySucceeded ? 1 : 0,
			*GetNameSafe(ActiveHologram),
			*GetNameSafe(ActiveHologram ? ActiveHologram->GetRecipe() : nullptr),
			Preset.ExtendCloneTopology.ChildHolograms.Num());
		return;
	}

	if (AttemptsRemaining <= 0)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: Gave up for preset '%s' (ExtendSvc=%s, ActiveHologram=%s)"),
			*Preset.Name,
			ExtendSvc ? TEXT("valid") : TEXT("null"),
			ActiveHologram ? TEXT("valid") : TEXT("null"));
		return;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: World null for preset '%s'"),
			*Preset.Name);
		return;
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] ReplayExtendTopologyWhenHologramReady: Waiting for matching active hologram for preset '%s' attemptsRemaining=%d activeHologram=%s recipe=%s expectedRecipe=%s"),
		*Preset.Name,
		AttemptsRemaining,
		*GetNameSafe(ActiveHologram),
		*GetNameSafe(ActiveHologram ? ActiveHologram->GetRecipe() : nullptr),
		*Preset.BuildingClassName);

	TWeakObjectPtr<USFRestoreService> WeakThis(this);
	FTimerDelegate RetryDelegate;
	RetryDelegate.BindLambda([WeakThis, Preset, AttemptsRemaining, SettleTicksRemaining]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ReplayExtendTopologyWhenHologramReady(Preset, AttemptsRemaining - 1, SettleTicksRemaining);
		}
	});
	World->GetTimerManager().SetTimerForNextTick(RetryDelegate);
}

// ============================================================================
// JSON Serialization
// ============================================================================

TSharedPtr<FJsonObject> USFRestoreService::PresetToJson(const FSFRestorePreset& Preset) const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	Root->SetStringField(TEXT("name"), Preset.Name);
	Root->SetStringField(TEXT("buildingClassName"), Preset.BuildingClassName);
	Root->SetNumberField(TEXT("version"), Preset.Version);
	Root->SetStringField(TEXT("description"), Preset.Description);
	Root->SetStringField(TEXT("createdAt"), Preset.CreatedAt);
	Root->SetStringField(TEXT("updatedAt"), Preset.UpdatedAt);

	TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
	Metadata->SetStringField(TEXT("description"), Preset.Description);
	Metadata->SetStringField(TEXT("createdAt"), Preset.CreatedAt);
	Metadata->SetStringField(TEXT("updatedAt"), Preset.UpdatedAt);
	Root->SetObjectField(TEXT("metadata"), Metadata);

	// Capture flags
	TSharedPtr<FJsonObject> Flags = MakeShared<FJsonObject>();
	Flags->SetBoolField(TEXT("grid"), Preset.CaptureFlags.bGrid);
	Flags->SetBoolField(TEXT("spacing"), Preset.CaptureFlags.bSpacing);
	Flags->SetBoolField(TEXT("steps"), Preset.CaptureFlags.bSteps);
	Flags->SetBoolField(TEXT("stagger"), Preset.CaptureFlags.bStagger);
	Flags->SetBoolField(TEXT("rotation"), Preset.CaptureFlags.bRotation);
	Flags->SetBoolField(TEXT("recipe"), Preset.CaptureFlags.bRecipe);
	Flags->SetBoolField(TEXT("autoConnect"), Preset.CaptureFlags.bAutoConnect);
	Root->SetObjectField(TEXT("captureFlags"), Flags);

	// Conditional field groups — only serialize fields whose flag is true
	if (Preset.CaptureFlags.bGrid)
	{
		TSharedPtr<FJsonObject> Grid = MakeShared<FJsonObject>();
		Grid->SetNumberField(TEXT("x"), Preset.GridCounters.X);
		Grid->SetNumberField(TEXT("y"), Preset.GridCounters.Y);
		Grid->SetNumberField(TEXT("z"), Preset.GridCounters.Z);
		Root->SetObjectField(TEXT("grid"), Grid);
	}

	if (Preset.CaptureFlags.bSpacing)
	{
		TSharedPtr<FJsonObject> Spacing = MakeShared<FJsonObject>();
		Spacing->SetNumberField(TEXT("x"), Preset.SpacingX);
		Spacing->SetNumberField(TEXT("y"), Preset.SpacingY);
		Spacing->SetNumberField(TEXT("z"), Preset.SpacingZ);
		Root->SetObjectField(TEXT("spacing"), Spacing);
	}

	if (Preset.CaptureFlags.bSteps)
	{
		TSharedPtr<FJsonObject> Steps = MakeShared<FJsonObject>();
		Steps->SetNumberField(TEXT("x"), Preset.StepsX);
		Steps->SetNumberField(TEXT("y"), Preset.StepsY);
		Root->SetObjectField(TEXT("steps"), Steps);
	}

	if (Preset.CaptureFlags.bStagger)
	{
		TSharedPtr<FJsonObject> Stagger = MakeShared<FJsonObject>();
		Stagger->SetNumberField(TEXT("x"), Preset.StaggerX);
		Stagger->SetNumberField(TEXT("y"), Preset.StaggerY);
		Stagger->SetNumberField(TEXT("zx"), Preset.StaggerZX);
		Stagger->SetNumberField(TEXT("zy"), Preset.StaggerZY);
		Root->SetObjectField(TEXT("stagger"), Stagger);
	}

	if (Preset.CaptureFlags.bRotation)
	{
		Root->SetNumberField(TEXT("rotationZ"), Preset.RotationZ);
	}

	if (Preset.CaptureFlags.bRecipe && !Preset.RecipeClassName.IsEmpty())
	{
		Root->SetStringField(TEXT("recipeClassName"), Preset.RecipeClassName);
	}

	if (Preset.CaptureFlags.bAutoConnect)
	{
		TSharedPtr<FJsonObject> AC = MakeShared<FJsonObject>();
		AC->SetBoolField(TEXT("beltEnabled"), Preset.AutoConnect.bBeltEnabled);
		AC->SetNumberField(TEXT("beltTierMain"), Preset.AutoConnect.BeltTierMain);
		AC->SetNumberField(TEXT("beltTierToBuilding"), Preset.AutoConnect.BeltTierToBuilding);
		AC->SetBoolField(TEXT("chainDistributors"), Preset.AutoConnect.bChainDistributors);
		AC->SetNumberField(TEXT("beltRoutingMode"), Preset.AutoConnect.BeltRoutingMode);
		AC->SetBoolField(TEXT("pipeEnabled"), Preset.AutoConnect.bPipeEnabled);
		AC->SetNumberField(TEXT("pipeTierMain"), Preset.AutoConnect.PipeTierMain);
		AC->SetNumberField(TEXT("pipeTierToBuilding"), Preset.AutoConnect.PipeTierToBuilding);
		AC->SetBoolField(TEXT("pipeIndicator"), Preset.AutoConnect.bPipeIndicator);
		AC->SetNumberField(TEXT("pipeRoutingMode"), Preset.AutoConnect.PipeRoutingMode);
		AC->SetBoolField(TEXT("powerEnabled"), Preset.AutoConnect.bPowerEnabled);
		AC->SetNumberField(TEXT("powerGridAxis"), Preset.AutoConnect.PowerGridAxis);
		AC->SetNumberField(TEXT("powerReserved"), Preset.AutoConnect.PowerReserved);
		Root->SetObjectField(TEXT("autoConnect"), AC);
	}

	if (Preset.bHasExtendTopology)
	{
		Root->SetObjectField(TEXT("extendCloneTopology"), CloneTopologyToJson(Preset.ExtendCloneTopology));
	}

	return Root;
}

bool USFRestoreService::JsonToPreset(const TSharedPtr<FJsonObject>& JsonObj, FSFRestorePreset& OutPreset) const
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	OutPreset = FSFRestorePreset();

	OutPreset.Name = JsonObj->GetStringField(TEXT("name"));
	OutPreset.BuildingClassName = JsonObj->GetStringField(TEXT("buildingClassName"));
	OutPreset.Version = static_cast<int32>(JsonObj->GetNumberField(TEXT("version")));
	JsonObj->TryGetStringField(TEXT("description"), OutPreset.Description);
	JsonObj->TryGetStringField(TEXT("createdAt"), OutPreset.CreatedAt);
	JsonObj->TryGetStringField(TEXT("updatedAt"), OutPreset.UpdatedAt);
	const TSharedPtr<FJsonObject>* MetadataObj = nullptr;
	if (JsonObj->TryGetObjectField(TEXT("metadata"), MetadataObj) && MetadataObj && (*MetadataObj).IsValid())
	{
		(*MetadataObj)->TryGetStringField(TEXT("description"), OutPreset.Description);
		(*MetadataObj)->TryGetStringField(TEXT("createdAt"), OutPreset.CreatedAt);
		(*MetadataObj)->TryGetStringField(TEXT("updatedAt"), OutPreset.UpdatedAt);
	}

	// Capture flags
	const TSharedPtr<FJsonObject>* FlagsObj = nullptr;
	if (JsonObj->TryGetObjectField(TEXT("captureFlags"), FlagsObj) && FlagsObj && (*FlagsObj).IsValid())
	{
		OutPreset.CaptureFlags.bGrid = (*FlagsObj)->GetBoolField(TEXT("grid"));
		OutPreset.CaptureFlags.bSpacing = (*FlagsObj)->GetBoolField(TEXT("spacing"));
		OutPreset.CaptureFlags.bSteps = (*FlagsObj)->GetBoolField(TEXT("steps"));
		OutPreset.CaptureFlags.bStagger = (*FlagsObj)->GetBoolField(TEXT("stagger"));
		OutPreset.CaptureFlags.bRotation = (*FlagsObj)->GetBoolField(TEXT("rotation"));
		OutPreset.CaptureFlags.bRecipe = (*FlagsObj)->GetBoolField(TEXT("recipe"));
		OutPreset.CaptureFlags.bAutoConnect = (*FlagsObj)->GetBoolField(TEXT("autoConnect"));
	}

	// Conditional fields
	const TSharedPtr<FJsonObject>* GridObj = nullptr;
	if (OutPreset.CaptureFlags.bGrid && JsonObj->TryGetObjectField(TEXT("grid"), GridObj) && GridObj)
	{
		double Val;
		if ((*GridObj)->TryGetNumberField(TEXT("x"), Val)) OutPreset.GridCounters.X = static_cast<int32>(Val);
		if ((*GridObj)->TryGetNumberField(TEXT("y"), Val)) OutPreset.GridCounters.Y = static_cast<int32>(Val);
		if ((*GridObj)->TryGetNumberField(TEXT("z"), Val)) OutPreset.GridCounters.Z = static_cast<int32>(Val);
	}

	const TSharedPtr<FJsonObject>* SpacingObj = nullptr;
	if (OutPreset.CaptureFlags.bSpacing && JsonObj->TryGetObjectField(TEXT("spacing"), SpacingObj) && SpacingObj)
	{
		double Val;
		if ((*SpacingObj)->TryGetNumberField(TEXT("x"), Val)) OutPreset.SpacingX = static_cast<int32>(Val);
		if ((*SpacingObj)->TryGetNumberField(TEXT("y"), Val)) OutPreset.SpacingY = static_cast<int32>(Val);
		if ((*SpacingObj)->TryGetNumberField(TEXT("z"), Val)) OutPreset.SpacingZ = static_cast<int32>(Val);
	}

	const TSharedPtr<FJsonObject>* StepsObj = nullptr;
	if (OutPreset.CaptureFlags.bSteps && JsonObj->TryGetObjectField(TEXT("steps"), StepsObj) && StepsObj)
	{
		double Val;
		if ((*StepsObj)->TryGetNumberField(TEXT("x"), Val)) OutPreset.StepsX = static_cast<int32>(Val);
		if ((*StepsObj)->TryGetNumberField(TEXT("y"), Val)) OutPreset.StepsY = static_cast<int32>(Val);
	}

	const TSharedPtr<FJsonObject>* StaggerObj = nullptr;
	if (OutPreset.CaptureFlags.bStagger && JsonObj->TryGetObjectField(TEXT("stagger"), StaggerObj) && StaggerObj)
	{
		double Val;
		if ((*StaggerObj)->TryGetNumberField(TEXT("x"), Val)) OutPreset.StaggerX = static_cast<int32>(Val);
		if ((*StaggerObj)->TryGetNumberField(TEXT("y"), Val)) OutPreset.StaggerY = static_cast<int32>(Val);
		if ((*StaggerObj)->TryGetNumberField(TEXT("zx"), Val)) OutPreset.StaggerZX = static_cast<int32>(Val);
		if ((*StaggerObj)->TryGetNumberField(TEXT("zy"), Val)) OutPreset.StaggerZY = static_cast<int32>(Val);
	}

	if (OutPreset.CaptureFlags.bRotation)
	{
		double Val = 0.0;
		if (JsonObj->TryGetNumberField(TEXT("rotationZ"), Val)) OutPreset.RotationZ = static_cast<float>(Val);
	}

	if (OutPreset.CaptureFlags.bRecipe)
	{
		JsonObj->TryGetStringField(TEXT("recipeClassName"), OutPreset.RecipeClassName);
	}

	const TSharedPtr<FJsonObject>* ACObj = nullptr;
	if (OutPreset.CaptureFlags.bAutoConnect && JsonObj->TryGetObjectField(TEXT("autoConnect"), ACObj) && ACObj)
	{
		double Val;
		(*ACObj)->TryGetBoolField(TEXT("beltEnabled"), OutPreset.AutoConnect.bBeltEnabled);
		if ((*ACObj)->TryGetNumberField(TEXT("beltTierMain"), Val)) OutPreset.AutoConnect.BeltTierMain = static_cast<int32>(Val);
		if ((*ACObj)->TryGetNumberField(TEXT("beltTierToBuilding"), Val)) OutPreset.AutoConnect.BeltTierToBuilding = static_cast<int32>(Val);
		(*ACObj)->TryGetBoolField(TEXT("chainDistributors"), OutPreset.AutoConnect.bChainDistributors);
		if ((*ACObj)->TryGetNumberField(TEXT("beltRoutingMode"), Val)) OutPreset.AutoConnect.BeltRoutingMode = static_cast<int32>(Val);
		(*ACObj)->TryGetBoolField(TEXT("pipeEnabled"), OutPreset.AutoConnect.bPipeEnabled);
		if ((*ACObj)->TryGetNumberField(TEXT("pipeTierMain"), Val)) OutPreset.AutoConnect.PipeTierMain = static_cast<int32>(Val);
		if ((*ACObj)->TryGetNumberField(TEXT("pipeTierToBuilding"), Val)) OutPreset.AutoConnect.PipeTierToBuilding = static_cast<int32>(Val);
		(*ACObj)->TryGetBoolField(TEXT("pipeIndicator"), OutPreset.AutoConnect.bPipeIndicator);
		if ((*ACObj)->TryGetNumberField(TEXT("pipeRoutingMode"), Val)) OutPreset.AutoConnect.PipeRoutingMode = static_cast<int32>(Val);
		(*ACObj)->TryGetBoolField(TEXT("powerEnabled"), OutPreset.AutoConnect.bPowerEnabled);
		if ((*ACObj)->TryGetNumberField(TEXT("powerGridAxis"), Val)) OutPreset.AutoConnect.PowerGridAxis = static_cast<int32>(Val);
		if ((*ACObj)->TryGetNumberField(TEXT("powerReserved"), Val)) OutPreset.AutoConnect.PowerReserved = static_cast<int32>(Val);
	}

	const TSharedPtr<FJsonObject>* ExtendTopologyObj = nullptr;
	if (JsonObj->TryGetObjectField(TEXT("extendCloneTopology"), ExtendTopologyObj) && ExtendTopologyObj)
	{
		OutPreset.bHasExtendTopology = JsonToCloneTopology(*ExtendTopologyObj, OutPreset.ExtendCloneTopology);
	}

	return true;
}

// ============================================================================
// File I/O
// ============================================================================

FString USFRestoreService::GetPresetsDir() const
{
	return FPaths::Combine(FPaths::ProjectUserDir(), TEXT("SmartFoundations"), TEXT("Presets"));
}

FString USFRestoreService::SanitizeFileName(const FString& Name) const
{
	FString Sanitized;
	for (const TCHAR Ch : Name)
	{
		if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-') || Ch == TEXT(' '))
		{
			Sanitized += Ch;
		}
	}
	// Replace spaces with underscores for the filename
	Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
	if (Sanitized.IsEmpty())
	{
		Sanitized = TEXT("unnamed");
	}
	return Sanitized;
}

FString USFRestoreService::GetPresetFilePath(const FString& Name) const
{
	return FPaths::Combine(GetPresetsDir(), SanitizeFileName(Name) + TEXT(".json"));
}

bool USFRestoreService::SavePreset(const FSFRestorePreset& Preset)
{
	// Update timestamp
	FSFRestorePreset MutablePreset = Preset;
	MutablePreset.Version = SF_RESTORE_PRESET_VERSION;
	const FString NowIso = FDateTime::UtcNow().ToIso8601();
	if (MutablePreset.CreatedAt.IsEmpty())
	{
		MutablePreset.CreatedAt = NowIso;
	}
	MutablePreset.UpdatedAt = NowIso;

	const FString FilePath = GetPresetFilePath(MutablePreset.Name);
	const FString Dir = FPaths::GetPath(FilePath);

	// Check for filename collisions — different preset names can map to the same file
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FilePath))
	{
		FString ExistingJson;
		if (FFileHelper::LoadFileToString(ExistingJson, *FilePath))
		{
			TSharedPtr<FJsonObject> ExistingObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingJson);
			if (FJsonSerializer::Deserialize(Reader, ExistingObj) && ExistingObj.IsValid())
			{
				FString ExistingName = ExistingObj->GetStringField(TEXT("name"));
				if (ExistingName != MutablePreset.Name)
				{
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("[SmartRestore] SavePreset: Name collision — '%s' would overwrite '%s' at %s"),
						*MutablePreset.Name, *ExistingName, *FilePath);
					return false;
				}

				FSFRestorePreset ExistingPreset;
				if (JsonToPreset(ExistingObj, ExistingPreset))
				{
					if (!ExistingPreset.CreatedAt.IsEmpty())
					{
						MutablePreset.CreatedAt = ExistingPreset.CreatedAt;
					}
					if (MutablePreset.Description.IsEmpty() && !ExistingPreset.Description.IsEmpty())
					{
						MutablePreset.Description = ExistingPreset.Description;
					}
				}
			}
		}
	}

	// Ensure directory exists
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}

	// Serialize to JSON
	TSharedPtr<FJsonObject> JsonObj = PresetToJson(MutablePreset);
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	// Write to file
	if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] Failed to save preset '%s' to %s"),
			*MutablePreset.Name, *FilePath);
		return false;
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Saved preset '%s' to %s"),
		*MutablePreset.Name, *FilePath);
	return true;
}

bool USFRestoreService::DeletePreset(const FString& Name)
{
	const FString FilePath = GetPresetFilePath(Name);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*FilePath))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] Preset '%s' not found at %s"), *Name, *FilePath);
		return false;
	}

	if (!PlatformFile.DeleteFile(*FilePath))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] Failed to delete preset '%s' at %s"), *Name, *FilePath);
		return false;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore] Deleted preset '%s'"), *Name);
	return true;
}

TArray<FSFRestorePreset> USFRestoreService::LoadAllPresets() const
{
	TArray<FSFRestorePreset> Presets;
	const FString Dir = GetPresetsDir();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		return Presets;
	}

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(Dir, TEXT("*.json")), true, false);

	for (const FString& FileName : FoundFiles)
	{
		const FString FilePath = FPaths::Combine(Dir, FileName);
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[SmartRestore] Failed to read preset file %s"), *FilePath);
			continue;
		}

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[SmartRestore] Failed to parse preset file %s"), *FilePath);
			continue;
		}

		FSFRestorePreset Preset;
		if (JsonToPreset(JsonObj, Preset))
		{
			Presets.Add(MoveTemp(Preset));
		}
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Loaded %d presets from %s"), Presets.Num(), *Dir);
	return Presets;
}

FSFRestorePreset USFRestoreService::LoadPreset(const FString& Name, bool& bOutFound) const
{
	bOutFound = false;
	const FString FilePath = GetPresetFilePath(Name);

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return FSFRestorePreset();
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return FSFRestorePreset();
	}

	FSFRestorePreset Preset;
	bOutFound = JsonToPreset(JsonObj, Preset);
	return Preset;
}

TArray<FString> USFRestoreService::GetPresetNames() const
{
	TArray<FString> Names;
	const FString Dir = GetPresetsDir();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		return Names;
	}

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(Dir, TEXT("*.json")), true, false);

	for (const FString& FileName : FoundFiles)
	{
		const FString FilePath = FPaths::Combine(Dir, FileName);
		FString JsonString;
		if (FFileHelper::LoadFileToString(JsonString, *FilePath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				FString PresetName;
				if (JsonObj->TryGetStringField(TEXT("name"), PresetName) && !PresetName.IsEmpty())
				{
					Names.Add(PresetName);
					continue;
				}
			}
		}
		Names.Add(FPaths::GetBaseFilename(FileName));
	}

	return Names;
}

bool USFRestoreService::PresetExists(const FString& Name) const
{
	const FString FilePath = GetPresetFilePath(Name);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.FileExists(*FilePath);
}

// ============================================================================
// Export / Import (compact string)
// ============================================================================

FString USFRestoreService::ExportToString(const FSFRestorePreset& Preset) const
{
	TSharedPtr<FJsonObject> JsonObj = PresetToJson(Preset);

	// Strip volatile timestamp metadata for compact sharing; keep description.
	JsonObj->RemoveField(TEXT("createdAt"));
	JsonObj->RemoveField(TEXT("updatedAt"));
	JsonObj->RemoveField(TEXT("metadata"));

	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	// Base64 encode
	FString Encoded = FBase64::Encode(JsonString);

	return SF_RESTORE_EXPORT_PREFIX + Encoded;
}

FSFRestorePreset USFRestoreService::ImportFromString(const FString& Encoded, bool& bOutSuccess) const
{
	bOutSuccess = false;

	if (!Encoded.StartsWith(SF_RESTORE_EXPORT_PREFIX))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromString: Unknown format prefix"));
		return FSFRestorePreset();
	}

	// Strip prefix, decode Base64
	const FString Base64Part = Encoded.Mid(SF_RESTORE_EXPORT_PREFIX.Len());
	FString JsonString;
	if (!FBase64::Decode(Base64Part, JsonString))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromString: Base64 decode failed"));
		return FSFRestorePreset();
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromString: JSON parse failed"));
		return FSFRestorePreset();
	}

	FSFRestorePreset Preset;
	bOutSuccess = JsonToPreset(JsonObj, Preset);

	if (bOutSuccess)
	{
		// Set fresh timestamps for imported presets
		const FDateTime Now = FDateTime::UtcNow();
		Preset.CreatedAt = Now.ToIso8601();
		Preset.UpdatedAt = Preset.CreatedAt;
	}

	return Preset;
}

// ============================================================================
// Extend Integration
// ============================================================================

bool USFRestoreService::IsLastExtendAvailable() const
{
	if (!Subsystem.IsValid())
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore] IsLastExtendAvailable: Subsystem invalid"));
		return false;
	}

	USFExtendService* ExtendSvc = Subsystem->GetExtendService();
	if (!ExtendSvc)
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore] IsLastExtendAvailable: Extend service null"));
		return false;
	}

	const FSFExtendTopology& Topology = ExtendSvc->GetLastExtendTopology();
	TSharedPtr<FSFCloneTopology> CloneTopology = ExtendSvc->GetLastCloneTopology();
	const bool bAvailable = Topology.bIsValid
		&& Topology.SourceBuilding.IsValid()
		&& CloneTopology.IsValid()
		&& CloneTopology->ChildHolograms.Num() > 0;
	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] IsLastExtendAvailable: available=%d sourceTopologyValid=%d sourceBuilding=%s cloneTopologyValid=%d cloneChildren=%d"),
		bAvailable ? 1 : 0,
		Topology.bIsValid ? 1 : 0,
		*GetNameSafe(Topology.SourceBuilding.Get()),
		CloneTopology.IsValid() ? 1 : 0,
		CloneTopology.IsValid() ? CloneTopology->ChildHolograms.Num() : 0);
	return bAvailable;
}

FSFRestorePreset USFRestoreService::ImportFromLastExtend(
	const FString& Name,
	const FSFRestoreCaptureFlags& CaptureFlags,
	bool& bOutSuccess) const
{
	bOutSuccess = false;

	if (!Subsystem.IsValid())
	{
		return FSFRestorePreset();
	}

	USFExtendService* ExtendSvc = Subsystem->GetExtendService();
	if (!ExtendSvc)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromLastExtend: No Extend service"));
		return FSFRestorePreset();
	}

	const FSFExtendTopology& Topology = ExtendSvc->GetLastExtendTopology();
	TSharedPtr<FSFCloneTopology> InitialCloneTopology = ExtendSvc->GetLastCloneTopology();
	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] ImportFromLastExtend start: name='%s' sourceTopologyValid=%d sourceBuilding=%s cloneTopologyValid=%d cloneChildren=%d flags(grid=%d spacing=%d steps=%d stagger=%d rotation=%d recipe=%d autoConnect=%d)"),
		*Name,
		Topology.bIsValid ? 1 : 0,
		*GetNameSafe(Topology.SourceBuilding.Get()),
		InitialCloneTopology.IsValid() ? 1 : 0,
		InitialCloneTopology.IsValid() ? InitialCloneTopology->ChildHolograms.Num() : 0,
		CaptureFlags.bGrid ? 1 : 0,
		CaptureFlags.bSpacing ? 1 : 0,
		CaptureFlags.bSteps ? 1 : 0,
		CaptureFlags.bStagger ? 1 : 0,
		CaptureFlags.bRotation ? 1 : 0,
		CaptureFlags.bRecipe ? 1 : 0,
		CaptureFlags.bAutoConnect ? 1 : 0);
	if (!Topology.bIsValid || !Topology.SourceBuilding.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromLastExtend: No valid Extend topology cached"));
		return FSFRestorePreset();
	}

	// Start with a normal capture of current state
	FSFRestorePreset Preset = CaptureCurrentState(Name, CaptureFlags);

	// Override building class — find the build-gun recipe that produces this building type.
	// BuildingClassName must be a recipe class name (e.g. "Recipe_Constructor_C"), not a
	// building UClass name (e.g. "Build_ConstructorMk1_C"), because ApplyPreset passes it
	// to SetBuildGunByRecipeName which searches available recipes by name.
	AFGBuildable* SourceBuilding = Topology.SourceBuilding.Get();
	if (SourceBuilding)
	{
		// Clear the BuildingClassName captured from the current build gun — we want
		// the Extend source building's recipe instead
		Preset.BuildingClassName.Empty();

		UClass* BuildingClass = SourceBuilding->GetClass();
		UWorld* World = Subsystem->GetWorld();
		AFGRecipeManager* RecipeManager = World ? AFGRecipeManager::Get(World) : nullptr;
		if (RecipeManager)
		{
			TArray<TSubclassOf<UFGRecipe>> AllRecipes;
			RecipeManager->GetAllAvailableRecipes(AllRecipes);
			for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
			{
				if (!Recipe) continue;
				for (const FItemAmount& Product : UFGRecipe::GetProducts(Recipe))
				{
					TSubclassOf<UFGBuildDescriptor> BuildDescriptorClass = Product.ItemClass
						? TSubclassOf<UFGBuildDescriptor>(Product.ItemClass)
						: nullptr;
					TSubclassOf<AActor> ProductBuildClass = BuildDescriptorClass
						? UFGBuildDescriptor::GetBuildClass(BuildDescriptorClass)
						: nullptr;
					if (ProductBuildClass && BuildingClass->IsChildOf(ProductBuildClass))
					{
						Preset.BuildingClassName = Recipe->GetName();
						UE_LOG(LogSmartFoundations, Log,
							TEXT("[SmartRestore] ImportFromLastExtend: Matched source building '%s' to build recipe '%s' via descriptor '%s'"),
							*GetNameSafe(SourceBuilding),
							*Preset.BuildingClassName,
							*GetNameSafe(Product.ItemClass));
						break;
					}
				}
				if (!Preset.BuildingClassName.IsEmpty())
				{
					break;
				}
			}
		}

		if (Preset.BuildingClassName.IsEmpty())
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[SmartRestore] ImportFromLastExtend: Could not find recipe for building '%s'"),
				*GetNameSafe(SourceBuilding));
		}
	}

	// Override recipe from the source factory's production recipe (if applicable)
	if (CaptureFlags.bRecipe)
	{
		if (AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(SourceBuilding))
		{
			TSubclassOf<UFGRecipe> FactoryRecipe = Manufacturer->GetCurrentRecipe();
			if (FactoryRecipe)
			{
				Preset.RecipeClassName = FactoryRecipe->GetName();
			}
		}
	}

	if (Preset.BuildingClassName.IsEmpty())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromLastExtend: Failed — no building recipe found for '%s'"),
			*GetNameSafe(SourceBuilding));
		return FSFRestorePreset();
	}

	TSharedPtr<FSFCloneTopology> CloneTopology = ExtendSvc->GetLastCloneTopology();
	if (CloneTopology.IsValid() && CloneTopology->ChildHolograms.Num() > 0)
	{
		Preset.bHasExtendTopology = true;
		Preset.ExtendCloneTopology = *CloneTopology;
		UE_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore] Captured Extend clone topology with %d child holograms"),
			Preset.ExtendCloneTopology.ChildHolograms.Num());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] ImportFromLastExtend: No clone topology available to capture"));
		return FSFRestorePreset();
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] ImportFromLastExtend success: preset='%s' buildingRecipe='%s' productionRecipe='%s' childHolograms=%d"),
		*Name,
		*Preset.BuildingClassName,
		*Preset.RecipeClassName,
		Preset.ExtendCloneTopology.ChildHolograms.Num());
	bOutSuccess = true;
	return Preset;
}
