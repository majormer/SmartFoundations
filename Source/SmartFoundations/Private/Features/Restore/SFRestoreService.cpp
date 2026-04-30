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
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
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
			// Get filtered recipes for current hologram to find the production recipe
			TArray<TSubclassOf<UFGRecipe>> FilteredRecipes = Subsystem->GetFilteredRecipesForCurrentHologram();
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
	FSFCounterState State = Subsystem->GetCounterState();

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

	Subsystem->UpdateCounterState(State);

	// 3. Apply production recipe
	// NOTE: We cannot use GetFilteredRecipesForCurrentHologram() here because
	// the hologram is updated asynchronously via PollForActiveHologram (100ms timer).
	// After SetBuildGunByRecipeName above, the old hologram is still active.
	// Instead, find the building class from the preset's build gun recipe and
	// check recipe compatibility directly via GetProducedIn.
	if (Preset.CaptureFlags.bRecipe && !Preset.RecipeClassName.IsEmpty())
	{
		if (USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService())
		{
			UWorld* World = Subsystem->GetWorld();
			AFGRecipeManager* RecipeManager = World ? AFGRecipeManager::Get(World) : nullptr;
			if (RecipeManager)
			{
				// Find the building class produced by the preset's build gun recipe
				UClass* TargetBuildingClass = nullptr;
				{
					TArray<TSubclassOf<UFGRecipe>> AllRecipes;
					RecipeManager->GetAllAvailableRecipes(AllRecipes);
					for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
					{
						if (Recipe && Recipe->GetName() == Preset.BuildingClassName)
						{
							for (const FItemAmount& Product : UFGRecipe::GetProducts(Recipe))
							{
								if (Product.ItemClass && Product.ItemClass->IsChildOf(AFGBuildable::StaticClass()))
								{
									TargetBuildingClass = Product.ItemClass;
									break;
								}
							}
							break;
						}
					}
				}

				// Find all production recipes compatible with this building class
				if (TargetBuildingClass)
				{
					TArray<TSubclassOf<UFGRecipe>> AllRecipes;
					RecipeManager->GetAllAvailableRecipes(AllRecipes);
					TArray<TSubclassOf<UFGRecipe>> CompatibleRecipes;
					for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
					{
						if (!Recipe) continue;
						UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
						if (!RecipeCDO) continue;
						TArray<TSubclassOf<UObject>> ProducedIn;
						RecipeCDO->GetProducedIn(ProducedIn);
						for (const TSubclassOf<UObject>& ProducerClass : ProducedIn)
						{
							if (ProducerClass && TargetBuildingClass->IsChildOf(ProducerClass))
							{
								CompatibleRecipes.Add(Recipe);
								break;
							}
						}
					}

					// Find the target production recipe by name in the compatible list
					for (int32 i = 0; i < CompatibleRecipes.Num(); ++i)
					{
						if (CompatibleRecipes[i]->GetName() == Preset.RecipeClassName)
						{
							RecipeSvc->SetActiveRecipeByIndex(i);
							UE_LOG(LogSmartFoundations, Display,
								TEXT("[SmartRestore] Applied recipe '%s' at index %d"),
								*Preset.RecipeClassName, i);
							break;
						}
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

	UE_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore] Applied preset '%s'"), *Preset.Name);
	return true;
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
	Root->SetStringField(TEXT("createdAt"), Preset.CreatedAt);
	Root->SetStringField(TEXT("updatedAt"), Preset.UpdatedAt);

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
	JsonObj->TryGetStringField(TEXT("createdAt"), OutPreset.CreatedAt);
	JsonObj->TryGetStringField(TEXT("updatedAt"), OutPreset.UpdatedAt);

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
		OutPreset.GridCounters.X = static_cast<int32>((*GridObj)->GetNumberField(TEXT("x")));
		OutPreset.GridCounters.Y = static_cast<int32>((*GridObj)->GetNumberField(TEXT("y")));
		OutPreset.GridCounters.Z = static_cast<int32>((*GridObj)->GetNumberField(TEXT("z")));
	}

	const TSharedPtr<FJsonObject>* SpacingObj = nullptr;
	if (OutPreset.CaptureFlags.bSpacing && JsonObj->TryGetObjectField(TEXT("spacing"), SpacingObj) && SpacingObj)
	{
		OutPreset.SpacingX = static_cast<int32>((*SpacingObj)->GetNumberField(TEXT("x")));
		OutPreset.SpacingY = static_cast<int32>((*SpacingObj)->GetNumberField(TEXT("y")));
		OutPreset.SpacingZ = static_cast<int32>((*SpacingObj)->GetNumberField(TEXT("z")));
	}

	const TSharedPtr<FJsonObject>* StepsObj = nullptr;
	if (OutPreset.CaptureFlags.bSteps && JsonObj->TryGetObjectField(TEXT("steps"), StepsObj) && StepsObj)
	{
		OutPreset.StepsX = static_cast<int32>((*StepsObj)->GetNumberField(TEXT("x")));
		OutPreset.StepsY = static_cast<int32>((*StepsObj)->GetNumberField(TEXT("y")));
	}

	const TSharedPtr<FJsonObject>* StaggerObj = nullptr;
	if (OutPreset.CaptureFlags.bStagger && JsonObj->TryGetObjectField(TEXT("stagger"), StaggerObj) && StaggerObj)
	{
		OutPreset.StaggerX = static_cast<int32>((*StaggerObj)->GetNumberField(TEXT("x")));
		OutPreset.StaggerY = static_cast<int32>((*StaggerObj)->GetNumberField(TEXT("y")));
		OutPreset.StaggerZX = static_cast<int32>((*StaggerObj)->GetNumberField(TEXT("zx")));
		OutPreset.StaggerZY = static_cast<int32>((*StaggerObj)->GetNumberField(TEXT("zy")));
	}

	if (OutPreset.CaptureFlags.bRotation)
	{
		JsonObj->TryGetNumberField(TEXT("rotationZ"), OutPreset.RotationZ);
	}

	if (OutPreset.CaptureFlags.bRecipe)
	{
		JsonObj->TryGetStringField(TEXT("recipeClassName"), OutPreset.RecipeClassName);
	}

	const TSharedPtr<FJsonObject>* ACObj = nullptr;
	if (OutPreset.CaptureFlags.bAutoConnect && JsonObj->TryGetObjectField(TEXT("autoConnect"), ACObj) && ACObj)
	{
		(*ACObj)->TryGetBoolField(TEXT("beltEnabled"), OutPreset.AutoConnect.bBeltEnabled);
		OutPreset.AutoConnect.BeltTierMain = static_cast<int32>((*ACObj)->GetNumberField(TEXT("beltTierMain")));
		OutPreset.AutoConnect.BeltTierToBuilding = static_cast<int32>((*ACObj)->GetNumberField(TEXT("beltTierToBuilding")));
		(*ACObj)->TryGetBoolField(TEXT("chainDistributors"), OutPreset.AutoConnect.bChainDistributors);
		OutPreset.AutoConnect.BeltRoutingMode = static_cast<int32>((*ACObj)->GetNumberField(TEXT("beltRoutingMode")));
		(*ACObj)->TryGetBoolField(TEXT("pipeEnabled"), OutPreset.AutoConnect.bPipeEnabled);
		OutPreset.AutoConnect.PipeTierMain = static_cast<int32>((*ACObj)->GetNumberField(TEXT("pipeTierMain")));
		OutPreset.AutoConnect.PipeTierToBuilding = static_cast<int32>((*ACObj)->GetNumberField(TEXT("pipeTierToBuilding")));
		(*ACObj)->TryGetBoolField(TEXT("pipeIndicator"), OutPreset.AutoConnect.bPipeIndicator);
		OutPreset.AutoConnect.PipeRoutingMode = static_cast<int32>((*ACObj)->GetNumberField(TEXT("pipeRoutingMode")));
		(*ACObj)->TryGetBoolField(TEXT("powerEnabled"), OutPreset.AutoConnect.bPowerEnabled);
		OutPreset.AutoConnect.PowerGridAxis = static_cast<int32>((*ACObj)->GetNumberField(TEXT("powerGridAxis")));
		OutPreset.AutoConnect.PowerReserved = static_cast<int32>((*ACObj)->GetNumberField(TEXT("powerReserved")));
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
	MutablePreset.UpdatedAt = FDateTime::UtcNow().ToIso8601();

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

	// Strip metadata fields for compact sharing
	JsonObj->RemoveField(TEXT("createdAt"));
	JsonObj->RemoveField(TEXT("updatedAt"));

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
		return false;
	}

	USFExtendService* ExtendSvc = Subsystem->GetExtendService();
	if (!ExtendSvc)
	{
		return false;
	}

	const FSFExtendTopology& Topology = ExtendSvc->GetCurrentTopology();
	return Topology.bIsValid && Topology.SourceBuilding.IsValid();
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

	const FSFExtendTopology& Topology = ExtendSvc->GetCurrentTopology();
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
					if (Product.ItemClass && Product.ItemClass == BuildingClass)
					{
						Preset.BuildingClassName = Recipe->GetName();
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
		return Preset;
	}

	bOutSuccess = true;
	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Imported preset '%s' from Extend source '%s'"),
		*Name, *GetNameSafe(SourceBuilding));
	return Preset;
}
