#include "Services/SFHintBarService.h"
#include "SmartFoundations.h"
#include "SFSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"
#include "InputAction.h"
#include "FGInputLibrary.h"
#include "Input/FGInputMappingContext.h"

// Paths to vanilla Blueprint assets used for Widget_BuildMode hint injection
static const TCHAR* WidgetBuildModePath = TEXT("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/Widget_BuildMode.Widget_BuildMode_C");
static const TCHAR* KeybindingHintStructPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/KeybindingHints/Struct_KeybindingHint");

// Smart! input action asset paths
static const TCHAR* ActionPath_ModifierScaleX   = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Modifier_ScaleX.IA_Smart_Modifier_ScaleX");
static const TCHAR* ActionPath_ModifierScaleY   = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Modifier_ScaleY.IA_Smart_Modifier_ScaleY");
static const TCHAR* ActionPath_SpacingMode      = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Spacing_Mode.IA_Smart_Spacing_Mode");
static const TCHAR* ActionPath_StepsMode        = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Steps_Mode.IA_Smart_Steps_Mode");
static const TCHAR* ActionPath_StaggerMode      = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Stagger_Mode.IA_Smart_Stagger_Mode");
static const TCHAR* ActionPath_RotationMode     = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_Rotation_Mode.IA_Smart_Rotation_Mode");
static const TCHAR* ActionPath_CycleAxis        = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_CycleAxis.IA_Smart_CycleAxis");
static const TCHAR* ActionPath_RecipeMode       = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_RecipeMode.IA_Smart_RecipeMode");
static const TCHAR* ActionPath_ToggleSettings   = TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ToggleSettingsForm.IA_Smart_ToggleSettingsForm");
static const TCHAR* MappingContextPath          = TEXT("/SmartFoundations/SmartFoundations/Input/Contexts/MC_Smart_BuildGunBuild.MC_Smart_BuildGunBuild");

void USFHintBarService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

	// Load the UserDefinedStruct for Struct_KeybindingHint
	KeybindingHintStruct = LoadObject<UScriptStruct>(nullptr, KeybindingHintStructPath);
	if (KeybindingHintStruct)
	{
		// Find the Action and KeyBinding FText properties by prefix (GUID-suffixed names)
		for (TFieldIterator<FProperty> It(KeybindingHintStruct); It; ++It)
		{
			FString PropName = It->GetName();
			if (PropName.StartsWith(TEXT("Action_")))
			{
				ActionProperty = *It;
			}
			else if (PropName.StartsWith(TEXT("KeyBinding_")))
			{
				KeyBindingProperty = *It;
			}
		}
		UE_LOG(LogSmartFoundations, Log, TEXT("HintBarService: Struct loaded (Action=%s, KeyBinding=%s)"),
			ActionProperty ? *ActionProperty->GetName() : TEXT("NULL"),
			KeyBindingProperty ? *KeyBindingProperty->GetName() : TEXT("NULL"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("HintBarService: Failed to load Struct_KeybindingHint"));
	}

	// Load input actions and mapping context for keybind resolution
	LoadInputActions();
}

void USFHintBarService::Shutdown()
{
	if (Subsystem.IsValid())
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			World->GetTimerManager().ClearTimer(HintTickTimer);
		}
	}
	CachedBuildModeWidget.Reset();
	Subsystem.Reset();
	UE_LOG(LogSmartFoundations, Log, TEXT("HintBarService: Shutdown"));
}

void USFHintBarService::LoadInputActions()
{
	auto LoadIA = [](const TCHAR* Path) -> UInputAction*
	{
		return LoadObject<UInputAction>(nullptr, Path);
	};

	IA_ModifierScaleX    = LoadIA(ActionPath_ModifierScaleX);
	IA_ModifierScaleY    = LoadIA(ActionPath_ModifierScaleY);
	IA_SpacingMode       = LoadIA(ActionPath_SpacingMode);
	IA_StepsMode         = LoadIA(ActionPath_StepsMode);
	IA_StaggerMode       = LoadIA(ActionPath_StaggerMode);
	IA_RotationMode      = LoadIA(ActionPath_RotationMode);
	IA_CycleAxis         = LoadIA(ActionPath_CycleAxis);
	IA_RecipeMode        = LoadIA(ActionPath_RecipeMode);
	IA_ToggleSettingsForm = LoadIA(ActionPath_ToggleSettings);

	SmartMappingContext = LoadObject<UFGInputMappingContext>(nullptr, MappingContextPath);

	int32 Loaded = 0;
	if (IA_ModifierScaleX) ++Loaded;
	if (IA_ModifierScaleY) ++Loaded;
	if (IA_SpacingMode) ++Loaded;
	if (IA_StepsMode) ++Loaded;
	if (IA_StaggerMode) ++Loaded;
	if (IA_RotationMode) ++Loaded;
	if (IA_CycleAxis) ++Loaded;
	if (IA_RecipeMode) ++Loaded;
	if (IA_ToggleSettingsForm) ++Loaded;

	bActionsLoaded = (Loaded > 0);
	UE_LOG(LogSmartFoundations, Log, TEXT("HintBarService: Loaded %d/9 input actions, MappingContext=%s"),
		Loaded, SmartMappingContext ? TEXT("OK") : TEXT("MISSING"));
}

FText USFHintBarService::ResolveKeyText(const UInputAction* Action) const
{
	if (!Action)
	{
		return FText::FromString(TEXT("?"));
	}

	// Get the player controller for keybind resolution
	APlayerController* PC = nullptr;
	if (Subsystem.IsValid())
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			PC = World->GetFirstPlayerController();
		}
	}

	if (!PC)
	{
		return FText::FromString(TEXT("?"));
	}

	// Use UFGInputLibrary to resolve the actual bound key
	FKey PrimaryKey;
	TArray<FKey> ModifierKeys;
	bool bFound = UFGInputLibrary::GetCurrentMappingForInputAction(
		PC, Action, PrimaryKey, ModifierKeys, NAME_None, SmartMappingContext);

	if (bFound && PrimaryKey.IsValid())
	{
		FText KeyName = UFGInputLibrary::GetAbbreviatedKeyName(PrimaryKey);
		return KeyName;
	}

	// Fallback: return the action name
	return FText::FromString(Action->GetName());
}

void USFHintBarService::OnHologramRegistered()
{
	bHintsInjected = false;
	InjectSmartHints();

	// Start a repeating timer — Widget_BuildMode rebuilds hints on build mode changes
	if (Subsystem.IsValid())
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HintTickTimer, this, &USFHintBarService::OnHintTickTimer,
				0.1f, true);
		}
	}
}

void USFHintBarService::OnHologramUnregistered()
{
	if (Subsystem.IsValid())
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			World->GetTimerManager().ClearTimer(HintTickTimer);
		}
	}
	CachedBuildModeWidget.Reset();
	bHintsInjected = false;
}

void USFHintBarService::OnHintTickTimer()
{
	bHintsInjected = false; // Force re-check each tick since vanilla may rebuild
	InjectSmartHints();
}

UUserWidget* USFHintBarService::FindWidgetBuildMode() const
{
	// Load the Widget_BuildMode Blueprint generated class
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, WidgetBuildModePath);
	if (!WidgetClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("HintBarService: Failed to load Widget_BuildMode class"));
		return nullptr;
	}

	// Find live instances via TObjectIterator
	// Multiple Widget_BuildMode_C instances exist (Widget_DismantleRefunds, inactive copies, etc.)
	// We MUST pick the VISIBLE one — that's the actively displayed hint bar.
	UUserWidget* VisibleCandidate = nullptr;
	UUserWidget* PopulatedCandidate = nullptr;
	UUserWidget* AnyCandidate = nullptr;

	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!Widget || !IsValid(Widget) || !Widget->IsA(WidgetClass))
		{
			continue;
		}

		// Only consider widgets named Widget_BuildMode* — excludes Widget_DismantleRefunds etc.
		if (!Widget->GetName().StartsWith(TEXT("Widget_BuildMode")))
		{
			continue;
		}

		FArrayProperty* KeybindsProp = CastField<FArrayProperty>(
			Widget->GetClass()->FindPropertyByName(FName(TEXT("mCachedKeybinds"))));
		if (!KeybindsProp)
		{
			continue;
		}

		void* ArrPtr = KeybindsProp->ContainerPtrToValuePtr<void>(Widget);
		FScriptArrayHelper ArrHelper(KeybindsProp, ArrPtr);

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HintBarService: Candidate %s (%s) keybinds=%d visible=%d"),
			*Widget->GetName(), *Widget->GetClass()->GetName(), ArrHelper.Num(), Widget->IsVisible());

		// Priority 1: visible widget with keybinds (the actual active hint bar)
		if (Widget->IsVisible() && ArrHelper.Num() > 0)
		{
			VisibleCandidate = Widget;
			break; // Best possible match
		}

		// Priority 2: any widget with keybinds (may be invisible but populated)
		if (!PopulatedCandidate && ArrHelper.Num() > 0)
		{
			PopulatedCandidate = Widget;
		}

		// Priority 3: any matching widget
		if (!AnyCandidate)
		{
			AnyCandidate = Widget;
		}
	}

	UUserWidget* Result = VisibleCandidate ? VisibleCandidate : (PopulatedCandidate ? PopulatedCandidate : AnyCandidate);
	if (Result)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("HintBarService: Selected %s (visible=%d)"),
			*Result->GetName(), Result->IsVisible());
	}
	return Result;
}

void USFHintBarService::InjectSmartHints()
{
	if (!KeybindingHintStruct || !ActionProperty || !KeyBindingProperty)
	{
		return;
	}

	// Find the Widget_BuildMode instance — re-discover if cached widget went stale or invisible
	if (!CachedBuildModeWidget.IsValid() || !CachedBuildModeWidget->IsVisible())
	{
		CachedBuildModeWidget = FindWidgetBuildMode();
		if (!CachedBuildModeWidget.IsValid())
		{
			return;
		}
		SetKeybindingHintsFunc = CachedBuildModeWidget->FindFunction(FName(TEXT("SetKeybindingHints")));
		if (!SetKeybindingHintsFunc)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("HintBarService: SetKeybindingHints function not found on Widget_BuildMode"));
			return;
		}
		UE_LOG(LogSmartFoundations, Log, TEXT("HintBarService: Found Widget_BuildMode (%s) with SetKeybindingHints"),
			*CachedBuildModeWidget->GetName());
	}

	if (!SetKeybindingHintsFunc)
	{
		return;
	}

	// Read current mCachedKeybinds to check if Smart! hints already present
	FArrayProperty* CachedKeybindsProp = CastField<FArrayProperty>(
		CachedBuildModeWidget->GetClass()->FindPropertyByName(FName(TEXT("mCachedKeybinds"))));
	if (!CachedKeybindsProp)
	{
		return;
	}

	void* ArrayPtr = CachedKeybindsProp->ContainerPtrToValuePtr<void>(CachedBuildModeWidget.Get());
	FScriptArrayHelper ArrayHelper(CachedKeybindsProp, ArrayPtr);

	// Known Smart! hint labels for detection and stripping
	static const TArray<FString> SmartLabels = {
		TEXT("Scale X"), TEXT("Scale Y"), TEXT("Scale Z"),
		TEXT("Spacing"), TEXT("Steps"), TEXT("Stagger"), TEXT("Rotation"),
		TEXT("Cycle Mode"), TEXT("Recipe"), TEXT("Smart Panel"), TEXT("Upgrade Panel")
	};

	// Check if any Smart! hints are already in the array
	bool bAlreadyPresent = false;
	for (int32 i = 0; i < ArrayHelper.Num(); i++)
	{
		uint8* ElemPtr = ArrayHelper.GetRawPtr(i);
		FText ActionText;
		ActionProperty->GetValue_InContainer(ElemPtr, &ActionText);
		const FString ActionStr = ActionText.ToString();
		for (const FString& Label : SmartLabels)
		{
			if (ActionStr == Label)
			{
				bAlreadyPresent = true;
				break;
			}
		}
		if (bAlreadyPresent) break;
	}

	// Context: upgrade-capable → Upgrade Panel only, Scaled Extend → hide Scale Z & Stagger,
	// everything else → full set.
	const bool bNowUpgrade = Subsystem.IsValid() && Subsystem->IsUpgradeCapableContext();
	const bool bNowExtend = Subsystem.IsValid() && Subsystem->IsExtendModeActive();

	const uint32 ContextFingerprint = (bNowUpgrade ? 1 : 0) | (bNowExtend ? 2 : 0);
	const bool bContextChanged = (ContextFingerprint != LastContextFingerprint);

	if (bAlreadyPresent && !bContextChanged)
	{
		bHintsInjected = true;
		return;
	}

	// Context changed or vanilla rebuilt — strip old Smart! hints before re-injecting
	LastContextFingerprint = ContextFingerprint;
	if (bAlreadyPresent)
	{
		for (int32 i = ArrayHelper.Num() - 1; i >= 0; --i)
		{
			uint8* ElemPtr = ArrayHelper.GetRawPtr(i);
			FText ActionText;
			ActionProperty->GetValue_InContainer(ElemPtr, &ActionText);
			const FString ActionStr = ActionText.ToString();
			for (const FString& Label : SmartLabels)
			{
				if (ActionStr == Label)
				{
					ArrayHelper.RemoveValues(i, 1);
					break;
				}
			}
		}
	}

	// Resolve keybind text from the mapping context
	const FText KeyX = ResolveKeyText(IA_ModifierScaleX);
	const FText KeyY = ResolveKeyText(IA_ModifierScaleY);

	// Build Scale Z key text by combining both modifier keys
	const FString ScaleZKeyStr = FString::Printf(TEXT("%s+%s + Scroll"), *KeyX.ToString(), *KeyY.ToString());

	// Build hints: upgrade-capable → only panel hint, everything else → full set
	struct FSmartHint { FText Action; FText Key; };
	TArray<FSmartHint> SmartHints;

	if (!bNowUpgrade)
	{
		// Smart! hint set — Scale Z and Stagger are hidden during Scaled Extend
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_ScaleX", "Scale X"),
			FText::FromString(FString::Printf(TEXT("%s + Scroll"), *KeyX.ToString())) });
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_ScaleY", "Scale Y"),
			FText::FromString(FString::Printf(TEXT("%s + Scroll"), *KeyY.ToString())) });
		if (!bNowExtend)
		{
			SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_ScaleZ", "Scale Z"),
				FText::FromString(ScaleZKeyStr) });
		}
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_Spacing", "Spacing"),
			FText::FromString(FString::Printf(TEXT("%s + Scroll"), *ResolveKeyText(IA_SpacingMode).ToString())) });
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_Steps", "Steps"),
			FText::FromString(FString::Printf(TEXT("%s + Scroll"), *ResolveKeyText(IA_StepsMode).ToString())) });
		if (!bNowExtend)
		{
			SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_Stagger", "Stagger"),
				FText::FromString(FString::Printf(TEXT("%s + Scroll"), *ResolveKeyText(IA_StaggerMode).ToString())) });
		}
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_Rotation", "Rotation"),
			FText::FromString(FString::Printf(TEXT("%s + Scroll"), *ResolveKeyText(IA_RotationMode).ToString())) });
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_CycleMode", "Cycle Mode"),
			ResolveKeyText(IA_CycleAxis) });
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_Recipe", "Recipe"),
			ResolveKeyText(IA_RecipeMode) });
		SmartHints.Add({ NSLOCTEXT("SmartFoundations", "Hint_SmartPanel", "Smart Panel"),
			ResolveKeyText(IA_ToggleSettingsForm) });
	}
	else
	{
		// Upgrade-capable items (belts, lifts, pipes, wires) — only the panel hint
		SmartHints.Add({ FText::FromString(TEXT("Upgrade Panel")),
			ResolveKeyText(IA_ToggleSettingsForm) });
	}

	// Add Smart! hints to the array
	for (const FSmartHint& Hint : SmartHints)
	{
		int32 NewIndex = ArrayHelper.AddValue();
		uint8* ElemPtr = ArrayHelper.GetRawPtr(NewIndex);

		KeybindingHintStruct->InitializeDefaultValue(ElemPtr);

		ActionProperty->SetValue_InContainer(ElemPtr, &Hint.Action);
		KeyBindingProperty->SetValue_InContainer(ElemPtr, &Hint.Key);
	}

	// Call SetKeybindingHints via ProcessEvent to trigger visual update
	uint8* ParamBuffer = (uint8*)FMemory_Alloca(SetKeybindingHintsFunc->ParmsSize);
	FMemory::Memzero(ParamBuffer, SetKeybindingHintsFunc->ParmsSize);

	for (TFieldIterator<FProperty> ParamIt(SetKeybindingHintsFunc); ParamIt; ++ParamIt)
	{
		if (ParamIt->HasAnyPropertyFlags(CPF_Parm))
		{
			ParamIt->CopyCompleteValue(ParamIt->ContainerPtrToValuePtr<void>(ParamBuffer), ArrayPtr);
			break;
		}
	}

	CachedBuildModeWidget->ProcessEvent(SetKeybindingHintsFunc, ParamBuffer);

	bHintsInjected = true;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HintBarService: Injected %d Smart! hints (total=%d, upgrade=%d, extend=%d)"),
		SmartHints.Num(), ArrayHelper.Num(), bNowUpgrade, bNowExtend);
}
