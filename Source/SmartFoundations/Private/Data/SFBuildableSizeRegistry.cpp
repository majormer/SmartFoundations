#include "Data/SFBuildableSizeRegistry.h"
#include "FGBuildableHologram.h"
#include "Buildables/FGBuildable.h"
#include "FGClearanceData.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"

// Static member initialization
TMap<FString, FSFBuildableSizeProfile> USFBuildableSizeRegistry::KnownProfiles;
FSFBuildableSizeProfile USFBuildableSizeRegistry::FallbackProfile;
bool USFBuildableSizeRegistry::bIsInitialized = false;
FString CurrentSourceFile = TEXT("SFBuildableSizeRegistry.cpp");

// Style names used for variant inheritance (shared across all variant resolution)
const TArray<FString> USFBuildableSizeRegistry::StyleNames = {
	TEXT("Asphalt"),
	TEXT("Concrete"),
	TEXT("ConcretePolished"),
	TEXT("PolishedConcrete"),
	TEXT("Polished"),
	TEXT("Metal"),
	TEXT("Grip"),
	TEXT("Steel")
};

void USFBuildableSizeRegistry::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}
	
	SF_LOG_ADAPTER(Normal, TEXT("Initializing Buildable Size Registry..."));
	
	// Clear any existing profiles
	KnownProfiles.Empty();
	
	// Register all known vanilla buildable profiles
	RegisterDefaultProfiles();
	
	// Set up fallback profile (conservative 8x8x4m foundation size for unknown buildables)
	FallbackProfile = FSFBuildableSizeProfile(
		TEXT("__fallback__"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m in cm
		false,
		FVector::ZeroVector,
		true,
		false,
		TEXT("Unknown/Modded")
	);
	
	bIsInitialized = true;
	
	SF_LOG_ADAPTER(Normal, TEXT("✅ Buildable Size Registry initialized with %d profiles"), KnownProfiles.Num());
}

void USFBuildableSizeRegistry::RegisterDefaultProfiles()
{
	RegisterFoundations();
	RegisterRamps();
	RegisterWalls();
	RegisterWalkways();
	RegisterBarriers();
	RegisterArchitectureDisabled();
	RegisterStorage();
	RegisterOrganization();
	RegisterProduction();
	RegisterExtractors();
	RegisterSpecial();
	RegisterPower();
	RegisterLogistics();
	RegisterTransport();
	
	// ===================================
	// RAMPS & STAIRS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Ramps.cpp
	
	// ===================================
	// WALLS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Walls.cpp
	
	// ===================================
	// WALKWAYS & CATWALKS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Walkways.cpp
	
	// ===================================
	// ARCHITECTURE (DISABLED)
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Architecture_Disabled.cpp
	
	// ===================================
	// PRODUCTION BUILDINGS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Production.cpp
	
	// ===================================
	// RESOURCE EXTRACTORS (NO SCALING)
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Extractors.cpp
	
	// ===================================
	// SPECIAL BUILDINGS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Special.cpp
	
	// ===================================
	// POWER BUILDINGS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Power.cpp
	
	// ===================================
	// LOGISTICS
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Logistics.cpp

	// ===================================
	// TRANSPORT
	// ===================================
	// MOVED TO SFBuildableSizeRegistry_Transport.cpp
}

void USFBuildableSizeRegistry::RegisterProfile(
	const FString& ClassName,
	const FVector& Size,
	bool bSwapOnRotation,
	bool bSupportsScaling,
	const FString& Inheritance,
	bool bValidated,
	const FVector& AnchorOffset
)
{
	extern FString CurrentSourceFile;
	
	FSFBuildableSizeProfile Profile(
		ClassName,
		Size,
		bSwapOnRotation,
		AnchorOffset,  // Pivot offset compensation (Z for attachment types)
		bSupportsScaling,
		bValidated,
		Inheritance,
		CurrentSourceFile  // Track which file registered this profile
	);
	
	KnownProfiles.Add(ClassName, Profile);
	
	// Log anchor offset if non-zero
	if (!AnchorOffset.IsZero())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, 
			TEXT("Registered buildable profile: %s | Size: %.0fx%.0fx%.0f cm | AnchorOffset: %s | SwapOnRotation: %s | Validated: %s"),
			*ClassName,
			Size.X, Size.Y, Size.Z,
			*AnchorOffset.ToString(),
			bSwapOnRotation ? TEXT("true") : TEXT("false"),
			bValidated ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, 
			TEXT("Registered buildable profile: %s | Size: %.0fx%.0fx%.0f cm | SwapOnRotation: %s | Validated: %s"),
			*ClassName,
			Size.X, Size.Y, Size.Z,
			bSwapOnRotation ? TEXT("YES") : TEXT("NO"),
			bValidated ? TEXT("YES") : TEXT("NO"));
	}
}

FSFBuildableSizeProfile USFBuildableSizeRegistry::GetProfile(const UClass* BuildableClass)
{
	if (!bIsInitialized)
	{
		Initialize();
	}
	
	if (!BuildableClass)
	{
		SF_LOG_ADAPTER(Normal, TEXT("GetProfile called with null BuildableClass - returning fallback"));
		return FallbackProfile;
	}
	
	FString ClassName = BuildableClass->GetName();
	return GetProfileByName(ClassName);
}

FSFBuildableSizeProfile USFBuildableSizeRegistry::GetProfileByName(const FString& ClassName)
{
	if (!bIsInitialized)
	{
		Initialize();
	}
	
	if (FSFBuildableSizeProfile* Found = KnownProfiles.Find(ClassName))
	{
		// Log which source file this profile came from
		SF_LOG_ADAPTER(Normal,
			TEXT("PROFILE SOURCE: %s | Size: %.0fx%.0fx%.0f cm | From: %s"),
			*ClassName,
			Found->DefaultSize.X,
			Found->DefaultSize.Y,
			Found->DefaultSize.Z,
			*Found->SourceFile
		);
		return *Found;
	}

	// Try variant inheritance fallback using shared helper
	FString BaseName;
	if (ResolveVariantBaseName(ClassName, BaseName))
	{
		if (FSFBuildableSizeProfile* BaseProfile = KnownProfiles.Find(BaseName))
		{
			SF_LOG_ADAPTER(Normal,
				TEXT("🔗 VARIANT INHERITANCE: %s inherits from %s | Size: %.0fx%.0fx%.0f cm | From: %s"),
				*ClassName,
				*BaseName,
				BaseProfile->DefaultSize.X,
				BaseProfile->DefaultSize.Y,
				BaseProfile->DefaultSize.Z,
				*BaseProfile->SourceFile
			);
			return *BaseProfile;
		}
	}

	// Log warning once per unknown buildable
	static TSet<FString> LoggedUnknownBuildables;
	if (!LoggedUnknownBuildables.Contains(ClassName))
	{
		SF_LOG_ADAPTER(Normal, 
			TEXT("📏 Unknown buildable '%s' - using fallback size (%.0fx%.0fx%.0f cm). Consider measuring and adding to registry."),
			*ClassName,
			FallbackProfile.DefaultSize.X,
			FallbackProfile.DefaultSize.Y,
			FallbackProfile.DefaultSize.Z
		);
		LoggedUnknownBuildables.Add(ClassName);
	}
	
	return FallbackProfile;
}

FVector USFBuildableSizeRegistry::GetSizeForHologram(const AFGBuildableHologram* Hologram)
{
	if (!Hologram)
	{
		SF_LOG_ADAPTER(Normal, TEXT("GetSizeForHologram called with null Hologram - returning fallback"));
		return FallbackProfile.DefaultSize;
	}
	
	// Get the profile for this buildable
	UClass* BuildClass = Hologram->GetBuildClass();
	FSFBuildableSizeProfile Profile = GetProfile(BuildClass);
	
	// Check if we need to swap X and Y based on rotation
	if (Profile.bSwapXYOnRotation)
	{
		FRotator Rotation = Hologram->GetActorRotation();
		if (IsRotated90Degrees(Rotation))
		{
			// Swap X and Y for 90° rotated asymmetric buildings
			return FVector(Profile.DefaultSize.Y, Profile.DefaultSize.X, Profile.DefaultSize.Z);
		}
	}
	
	return Profile.DefaultSize;
}

bool USFBuildableSizeRegistry::HasProfile(const UClass* BuildableClass)
{
	if (!bIsInitialized)
	{
		Initialize();
	}

	if (!BuildableClass)
	{
		return false;
	}

	FString ClassName = BuildableClass->GetName();

	// First check direct registry lookup
	if (KnownProfiles.Contains(ClassName))
	{
		return true;
	}

	// Check if variant inheritance can find a base profile
	FString BaseName;
	if (ResolveVariantBaseName(ClassName, BaseName))
	{
		return KnownProfiles.Contains(BaseName);
	}

	return false;
}

bool USFBuildableSizeRegistry::IsRotated90Degrees(const FRotator& Rotation)
{
	// Normalize yaw to 0-360 range
	float Yaw = FMath::Fmod(Rotation.Yaw + 360.0f, 360.0f);

	// Check if within tolerance of 90° or 270°
	constexpr float Tolerance = 5.0f;  // 5 degree tolerance

	bool bIs90 = FMath::Abs(Yaw - 90.0f) < Tolerance;
	bool bIs270 = FMath::Abs(Yaw - 270.0f) < Tolerance;

	return bIs90 || bIs270;
}

bool USFBuildableSizeRegistry::ResolveVariantBaseName(const FString& ClassName, FString& OutBaseName)
{
	// ========================================================================
	// VARIANT RESOLUTION: Maps style variants to their base registered profiles
	// This allows e.g., Build_Foundation_Asphalt_8x4_C to inherit from
	// Build_Foundation_8x4_01_C without explicit registration.
	// ========================================================================

	// Special case: SteelWall prefix (walls only)
	// Window/Gate variants map to their non-Orange bases, others to Orange
	if (ClassName.StartsWith(TEXT("Build_SteelWall_")))
	{
		if (ClassName.Contains(TEXT("_Window_")))
		{
			OutBaseName = ClassName.Replace(TEXT("Build_SteelWall_8x4_Window_"), TEXT("Build_Wall_Window_8x4_"));
		}
		else if (ClassName.Contains(TEXT("_Gate_")))
		{
			OutBaseName = ClassName.Replace(TEXT("Build_SteelWall_8x4_Gate_"), TEXT("Build_Wall_Gate_8x4_"));
		}
		else
		{
			OutBaseName = ClassName.Replace(TEXT("Build_SteelWall_"), TEXT("Build_Wall_Orange_"));
		}
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: Wall_Steel middle pattern (corner walls)
	if (ClassName.Contains(TEXT("Build_Wall_Steel_")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Steel_"), TEXT("_Orange_"));
		if (OutBaseName.Contains(TEXT("_Corner_2_C")))
		{
			OutBaseName = OutBaseName.Replace(TEXT("_Corner_2_C"), TEXT("_Corner_02_C"));
		}
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: Wall_Concrete middle pattern with inconsistent corner numbering
	if (ClassName.Contains(TEXT("Build_Wall_Concrete_")) && ClassName.Contains(TEXT("_Corner_")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Concrete_"), TEXT("_Orange_"));
		if (OutBaseName.Contains(TEXT("_Corner_2_C")))
		{
			OutBaseName = OutBaseName.Replace(TEXT("_Corner_2_C"), TEXT("_Corner_02_C"));
		}
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: WallSet_Steel prefix (walls only)
	if (ClassName.StartsWith(TEXT("Build_WallSet_Steel_")))
	{
		OutBaseName = ClassName.Replace(TEXT("Build_WallSet_Steel_"), TEXT("Build_Wall_Orange_"));
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: _Steel suffix at end (walls only)
	if (ClassName.Contains(TEXT("_Steel_C")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Steel_C"), TEXT("_C"));
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: ConveyorHole -> Conveyor (walls only)
	if (ClassName.Contains(TEXT("_ConveyorHole_")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Concrete_8x4_ConveyorHole_"), TEXT("_Conveyor_8x4_"));
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: Concrete Window walls (walls only)
	if (ClassName.Contains(TEXT("_Concrete_8x4_Window_")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Concrete_8x4_Window_"), TEXT("_Window_8x4_"));
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: CDoor/SDoor -> Door (walls only)
	if (ClassName.Contains(TEXT("_CDoor_")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Concrete_CDoor_"), TEXT("_Door_")).Replace(TEXT("_C"), TEXT("_01_C"));
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	if (ClassName.Contains(TEXT("_SDoor_")))
	{
		OutBaseName = ClassName.Replace(TEXT("_Concrete_SDoor_"), TEXT("_Door_")).Replace(TEXT("_C"), TEXT("_03_C"));
		if (KnownProfiles.Contains(OutBaseName))
		{
			return true;
		}
	}

	// Special case: Stairs use _FicsitSet_ pattern
	if (ClassName.Contains(TEXT("_Stair_")))
	{
		for (const FString& Style : StyleNames)
		{
			FString StylePattern = FString::Printf(TEXT("_%s_"), *Style);
			if (ClassName.Contains(StylePattern))
			{
				OutBaseName = ClassName.Replace(*StylePattern, TEXT("_FicsitSet_"));
				OutBaseName = OutBaseName.Replace(TEXT("_C"), TEXT("_01_C"));
				if (KnownProfiles.Contains(OutBaseName))
				{
					return true;
				}
			}
		}
	}

	// Generic style variant pattern
	for (const FString& Style : StyleNames)
	{
		FString StylePattern = FString::Printf(TEXT("_%s_"), *Style);
		if (ClassName.Contains(StylePattern))
		{
			int32 StyleIndex = ClassName.Find(StylePattern, ESearchCase::IgnoreCase);
			if (StyleIndex != INDEX_NONE)
			{
				FString Prefix = ClassName.Left(StyleIndex);
				FString SuffixWithStyle = ClassName.Mid(StyleIndex + StylePattern.Len());

				// Try multiple base patterns
				TArray<FString> BasePatterns = {
					FString::Printf(TEXT("%s_%s_01_C"), *Prefix, *SuffixWithStyle.Left(SuffixWithStyle.Len() - 2)),
					FString::Printf(TEXT("%s_Ficsit_%s"), *Prefix, *SuffixWithStyle),
					FString::Printf(TEXT("%s_%s"), *Prefix, *SuffixWithStyle)
				};

				for (const FString& BaseName : BasePatterns)
				{
					if (KnownProfiles.Contains(BaseName))
					{
						OutBaseName = BaseName;
						return true;
					}
				}
			}
		}
	}

	return false;
}

float USFBuildableSizeRegistry::GetRampUnitHeight(const FSFBuildableSizeProfile& Profile)
{
	// Catwalk stairs descend 4m per 4m X step (negative because they go down)
	// Example: Build_CatwalkStairs_C needs -400cm X step for proper alignment
	// Check this BEFORE the "Ramp" check since stairs don't have "Ramp" in their name
	if (Profile.BuildableClassName.Contains(TEXT("CatwalkStairs")))
	{
		return -400.0f;  // -4m X step for catwalk stairs
	}
	
	// Check if this is a ramp by name
	if (!Profile.BuildableClassName.Contains(TEXT("Ramp")))
	{
		return 0.0f;  // Not a ramp or stair
	}
	
	// Walkway/Catwalk ramps descend 2m per 4m X step (negative because they go down)
	// Example: Build_WalkwayRamp_C and Build_CatwalkRamp_C need -200cm X step for proper alignment
	if (Profile.BuildableClassName.Contains(TEXT("WalkwayRamp")) || 
	    Profile.BuildableClassName.Contains(TEXT("CatwalkRamp")))
	{
		return -200.0f;  // -2m X step for walkway/catwalk ramps
	}
	
	// Double ramps use half the total height as the unit step
	// Example: Build_RampDouble_8x2_C has Z=800cm total, but each "layer" is 400cm
	// Also handles Build_Ramp_8x8x8_C (alternative naming for 8m tall double ramp)
	if (Profile.BuildableClassName.Contains(TEXT("RampDouble")) || 
	    Profile.BuildableClassName.Contains(TEXT("_8x8x8_")))
	{
		return Profile.DefaultSize.Z * 0.5f;
	}
	
	// Normal ramps use the full height as the unit step
	// Example: Build_Ramp_8x1_01_C has Z=400cm, one layer = 400cm
	return Profile.DefaultSize.Z;
}

// ============================================================================
// CDO Query Functions
// ============================================================================

bool USFBuildableSizeRegistry::TryGetSizeFromMeshBounds(UClass* BuildClass, FVector& OutSize)
{
	if (!BuildClass || !BuildClass->IsChildOf(AFGBuildable::StaticClass()))
	{
		return false;
	}

	const AFGBuildable* CDO = Cast<AFGBuildable>(BuildClass->GetDefaultObject());
	if (!CDO)
	{
		return false;
	}

	// Get the actor's bounding box (includes all mesh components)
	FBox BoundingBox = CDO->GetComponentsBoundingBox(true);
	if (!BoundingBox.IsValid)
	{
		return false;
	}

	OutSize = BoundingBox.GetSize();

	// Validate reasonableness (50cm to 5000cm per axis)
	if (OutSize.X > 50.f && OutSize.X < 5000.f &&
	    OutSize.Y > 50.f && OutSize.Y < 5000.f &&
	    OutSize.Z > 50.f && OutSize.Z < 5000.f)
	{
		return true;
	}

	return false;
}

bool USFBuildableSizeRegistry::TryGetSizeFromClearanceBox(UClass* BuildClass, FVector& OutSize)
{
	if (!BuildClass || !BuildClass->IsChildOf(AFGBuildable::StaticClass()))
	{
		return false;
	}

	const AFGBuildable* CDO = Cast<AFGBuildable>(BuildClass->GetDefaultObject());
	if (!CDO)
	{
		return false;
	}

	// Query clearance data
	TArray<FFGClearanceData> ClearanceData;
	CDO->GetClearanceData_Implementation(ClearanceData);

	if (ClearanceData.Num() == 0)
	{
		return false;
	}

	// Use the first clearance box that is valid for snapping
	for (const FFGClearanceData& Data : ClearanceData)
	{
		if (!Data.ExcludeForSnapping && Data.IsValid())
		{
			FBox TransformedBox = Data.GetTransformedClearanceBox();
			OutSize = TransformedBox.GetSize();

			// Validate reasonableness (50cm to 5000cm per axis)
			if (OutSize.X > 50.f && OutSize.X < 5000.f &&
			    OutSize.Y > 50.f && OutSize.Y < 5000.f &&
			    OutSize.Z > 50.f && OutSize.Z < 5000.f)
			{
				return true;
			}
		}
	}

	return false;
}

bool USFBuildableSizeRegistry::GetSizeWithFallback(UClass* BuildClass, FVector& OutSize, FString& OutSource)
{
	if (!BuildClass)
	{
		OutSize = GetDefaultSize();
		OutSource = TEXT("Default (no class)");
		return false;
	}

	// 1. Try registry profile first (most accurate)
	if (HasProfile(BuildClass))
	{
		FSFBuildableSizeProfile Profile = GetProfile(BuildClass);
		OutSize = Profile.DefaultSize;
		OutSource = TEXT("Registry");
		return true;
	}

	// 2. Try clearance box + mesh bounds (use maximum per axis to prevent overlaps)
	FVector ClearanceSize;
	FVector MeshSize;
	bool bHasClearance = TryGetSizeFromClearanceBox(BuildClass, ClearanceSize);
	bool bHasMesh = TryGetSizeFromMeshBounds(BuildClass, MeshSize);

	if (bHasClearance && bHasMesh)
	{
		// Use the larger value per axis to prevent visual overlaps
		OutSize.X = FMath::Max(ClearanceSize.X, MeshSize.X);
		OutSize.Y = FMath::Max(ClearanceSize.Y, MeshSize.Y);
		OutSize.Z = FMath::Max(ClearanceSize.Z, MeshSize.Z);
		OutSource = TEXT("ClearanceBox+Mesh");
		return true;
	}
	else if (bHasClearance)
	{
		OutSize = ClearanceSize;
		OutSource = TEXT("ClearanceBox");
		return true;
	}
	else if (bHasMesh)
	{
		OutSize = MeshSize;
		OutSource = TEXT("MeshBounds");
		return true;
	}

	// 3. Use default fallback
	OutSize = GetDefaultSize();
	OutSource = TEXT("Default");
	return false;
}

FVector USFBuildableSizeRegistry::GetDefaultSize()
{
	return FVector(800.0f, 800.0f, 400.0f);  // Standard 8m foundation
}
