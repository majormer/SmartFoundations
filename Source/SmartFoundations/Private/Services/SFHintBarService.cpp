// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Services/SFHintBarService.h"
#include "Holograms/Core/SFSignSupportMode.h"
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkTypes.h"
#include "Hologram/FGBlueprintHologram.h"
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
		UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Struct loaded (Action=%s, KeyBinding=%s)"),
			ActionProperty ? *ActionProperty->GetName() : TEXT("NULL"),
			KeyBindingProperty ? *KeyBindingProperty->GetName() : TEXT("NULL"));
	}
	else
	{
		UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Failed to load Struct_KeybindingHint"));
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
	UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Shutdown"));
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
	UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Loaded %d/9 input actions, MappingContext=%s"),
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
	// Hint bars are client UI: a dedicated server has no UI assets cooked, so the load below
	// fails on EVERY hint tick and spams Warnings (live 2026-06-10 dedi logs). Warn once, then
	// stay quiet - the nullptr return already no-ops the injection.
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, WidgetBuildModePath);
	if (!WidgetClass)
	{
		static bool bWarnedOnce = false;
		if (!bWarnedOnce)
		{
			bWarnedOnce = true;
			UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Failed to load Widget_BuildMode class (further occurrences suppressed; expected on dedicated servers)"));
		}
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

		UE_LOG(LogSmartUI, VeryVerbose, TEXT("HintBarService: Candidate %s (%s) keybinds=%d visible=%d"),
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
		UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Selected %s (visible=%d)"),
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
			UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: SetKeybindingHints function not found on Widget_BuildMode"));
			return;
		}
		UE_LOG(LogSmartUI, Verbose, TEXT("HintBarService: Found Widget_BuildMode (%s) with SetKeybindingHints"),
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

	// Detect/strip our hints by their LOCTEXT KEY, NOT the displayed text. The Action FTexts are NSLOCTEXT
	// (localized), so matching English label strings failed in every non-English locale → old hints were never
	// stripped and got re-added every 100ms tick → the mCachedKeybinds array grew without bound → SetKeybindingHints
	// instanced ever-more hint widgets → UObject pool exhaustion crash. The "Hint_*" key is locale-independent.
	static const TSet<FString> SmartHintKeys = {
		TEXT("Hint_ScaleX"), TEXT("Hint_ScaleY"), TEXT("Hint_ScaleZ"),
		TEXT("Hint_Spacing"), TEXT("Hint_Steps"), TEXT("Hint_Stagger"), TEXT("Hint_Rotation"),
		TEXT("Hint_CycleMode"), TEXT("Hint_StaggerFamily"), TEXT("Hint_Recipe"), TEXT("Hint_SignSupports"),
		TEXT("Hint_SmartPanel"), TEXT("Hint_UpgradePanel"), TEXT("Hint_WalkPanel"), TEXT("Hint_WalkStart"),
        TEXT("Hint_WalkAdvance"), TEXT("Hint_WalkLanes"), TEXT("Hint_WalkLevels"),
        TEXT("Hint_WalkLength"), TEXT("Hint_WalkRise"), TEXT("Hint_WalkShift"), TEXT("Hint_WalkTurn"),
        TEXT("Hint_AutoConnectSettings")
	};
	auto IsSmartHint = [](const FText& ActionText) -> bool
	{
		const TOptional<FString> Key = FTextInspector::GetKey(ActionText);
		return Key.IsSet() && SmartHintKeys.Contains(Key.GetValue());
	};

	// Check if any Smart! hints are already in the array (by locale-independent key)
	bool bAlreadyPresent = false;
	for (int32 i = 0; i < ArrayHelper.Num(); i++)
	{
		uint8* ElemPtr = ArrayHelper.GetRawPtr(i);
		FText ActionText;
		ActionProperty->GetValue_InContainer(ElemPtr, &ActionText);
		if (IsSmartHint(ActionText))
		{
			bAlreadyPresent = true;
			break;
		}
	}

	// Context: upgrade-capable → Upgrade Panel only, Scaled Extend → hide Scale Z & Stagger,
	// everything else → full set.
	const bool bNowUpgrade = Subsystem.IsValid() && Subsystem->IsUpgradeCapableContext();
	const bool bNowExtend = Subsystem.IsValid() && Subsystem->IsExtendModeActive();
	// [#209] While Stagger is held, Num0 toggles the Stack/Flat family (not the axis) - the hint
	// reflects that. Held-mode is part of the rebuild fingerprint so the label swaps on Y down/up.
	const bool bNowStagger = Subsystem.IsValid() && Subsystem->IsStaggerModeActive();

	const bool bNowSign = Subsystem.IsValid() && SFSignSupportMode::IsAvailable(Subsystem->GetActiveHologram());
    const auto* Walk = Subsystem.IsValid() ? Subsystem->GetWalkService() : nullptr;
    const bool bNowWalk = Walk && Walk->IsActive();
    const bool bRouteOnly = Subsystem.IsValid() && Subsystem->IsWalkingOnlyContext();
    const bool bBus = bNowWalk && Walk->GetConveyanceType() != ESFWalkConveyanceType::Railroad
        && Walk->GetConveyanceType() != ESFWalkConveyanceType::VehiclePath;
    const auto Adapter = Subsystem.IsValid() ? Subsystem->GetCurrentAdapter() : nullptr;
    const bool bScaleX = bNowExtend || !Adapter || Adapter->SupportsScalingAxis(ESFScaleAxis::X);
    const bool bScaleY = bNowExtend || !Adapter || Adapter->SupportsScalingAxis(ESFScaleAxis::Y);
    const bool bScaleZ = bNowExtend || !Adapter || Adapter->SupportsScalingAxis(ESFScaleAxis::Z);
    const bool bSpacing = bNowExtend || !Adapter || Adapter->SupportsFeature(ESFFeature::Spacing);
    // Blueprint seams have their own global preference; U does not edit that setting.
    const bool bConnectionSettings = Subsystem.IsValid() && Subsystem->IsCurrentHologramAutoConnectCapable()
        && !Cast<AFGBlueprintHologram>(Subsystem->GetActiveHologram());
    const uint32 ContextFingerprint = (bNowUpgrade ? 1 : 0) | (bNowExtend ? 2 : 0) | (bNowStagger ? 4 : 0) | (bNowSign ? 8 : 0)
        | (bNowWalk ? 16 : 0) | (bRouteOnly ? 32 : 0) | (bBus ? 64 : 0)
        | (bScaleX ? 128 : 0) | (bScaleY ? 256 : 0) | (bScaleZ ? 512 : 0)
        | (bSpacing ? 1024 : 0) | (bConnectionSettings ? 2048 : 0);
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
			if (IsSmartHint(ActionText))
			{
				ArrayHelper.RemoveValues(i, 1);
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

    auto Wheel = [this](const UInputAction* Action)
    { return FText::FromString(FString::Printf(TEXT("%s + Scroll"), *ResolveKeyText(Action).ToString())); };
    if (bNowWalk)
    {
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkAdvance", "Next / previous segment"), Wheel(IA_ModifierScaleX)});
        if (bBus)
        {
            SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkLanes", "Bus lanes"), Wheel(IA_ModifierScaleY)});
            SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkLevels", "Bus levels"), FText::FromString(ScaleZKeyStr)});
        }
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkLength", "Segment length"), Wheel(IA_SpacingMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkRise", "Segment rise"), Wheel(IA_StepsMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkShift", "Segment shift"), Wheel(IA_StaggerMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkTurn", "Segment turn"), Wheel(IA_RotationMode)});
        if (bConnectionSettings)
            SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_AutoConnectSettings", "Connection settings"), ResolveKeyText(IA_RecipeMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkPanel", "Walking Panel"), ResolveKeyText(IA_ToggleSettingsForm)});
    }
    else if (bRouteOnly)
    {
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_WalkStart", "Start Walking"), ResolveKeyText(IA_ToggleSettingsForm)});
    }
    else if (bNowUpgrade)
    {
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_UpgradePanel", "Upgrade Panel"), ResolveKeyText(IA_ToggleSettingsForm)});
    }
    else
    {
        if (bScaleX) SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_ScaleX", "Scale X"), Wheel(IA_ModifierScaleX)});
        if (bScaleY) SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_ScaleY", "Scale Y"), Wheel(IA_ModifierScaleY)});
        if (bScaleZ) SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_ScaleZ", "Scale Z"), FText::FromString(ScaleZKeyStr)});
        if (bSpacing) SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_Spacing", "Spacing"), Wheel(IA_SpacingMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_Steps", "Steps"), Wheel(IA_StepsMode)});
        if (!bNowExtend) SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_Stagger", "Stagger"), Wheel(IA_StaggerMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_Rotation", "Rotation"), Wheel(IA_RotationMode)});
        SmartHints.Add({bNowStagger ? NSLOCTEXT("SmartFoundations", "Hint_StaggerFamily", "Stack / Flat")
            : NSLOCTEXT("SmartFoundations", "Hint_CycleMode", "Cycle Mode"), ResolveKeyText(IA_CycleAxis)});
        SmartHints.Add({bNowSign ? NSLOCTEXT("SmartFoundations", "Hint_SignSupports", "Sign supports")
            : bConnectionSettings ? NSLOCTEXT("SmartFoundations", "Hint_AutoConnectSettings", "Connection settings")
            : NSLOCTEXT("SmartFoundations", "Hint_Recipe", "Recipe"), ResolveKeyText(IA_RecipeMode)});
        SmartHints.Add({NSLOCTEXT("SmartFoundations", "Hint_SmartPanel", "Smart Panel"), ResolveKeyText(IA_ToggleSettingsForm)});
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
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("HintBarService: Injected %d Smart! hints (total=%d, upgrade=%d, extend=%d)"),
		SmartHints.Num(), ArrayHelper.Num(), bNowUpgrade, bNowExtend);
}
