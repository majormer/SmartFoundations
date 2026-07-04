using UnrealBuildTool;
using System.IO;
using System;

public class SmartFoundations : ModuleRules
{
	public SmartFoundations(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		// UE 5.6 defaults these off (IWYU-strict). Smart's source bare-includes its own Public
		// subdir headers (e.g. "SFSubsystem.h"), which relied on the legacy flat include paths.
		// Restore them so the mod compiles without rewriting every internal include. (FactoryGame
		// headers still need subdir-qualified includes - handled by qualifying those directly.)
		bLegacyPublicIncludePaths = true;
		bLegacyParentIncludePaths = true;

		// FactoryGame transitive dependencies
		// Not all of these are required, but including the extra ones saves you from having to add them later.
		// Some entries are commented out to avoid compile-time warnings about depending on a module that you don't explicitly depend on.
		// You can uncomment these as necessary when your code actually needs to use them.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject",
			"Engine",
			"DeveloperSettings",
			"ApplicationCore",
			"PhysicsCore",
			"InputCore",
			//"OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemNull",
			//"SignificanceManager",
			"GeometryCollectionEngine",
			//"ChaosVehiclesCore", "ChaosVehicles", "ChaosSolverEngine",
			"AnimGraphRuntime",
			//"AkAudio",
			"AssetRegistry",
			"NavigationSystem",
			//"ReplicationGraph",
			"AIModule",
			"GameplayTasks",
			"SlateCore", "Slate", "UMG",
			//"InstancedSplines",
			"RenderCore",
			"CinematicCamera",
			"Foliage",
			//"Niagara",
			"EnhancedInput", // Uncommented for Smart! input features
			//"GameplayCameras",
			//"TemplateSequence",
			"NetCore",
			"GameplayTags",
			"Json"
		});

		// FactoryGame (base-game) plugins. These are module-only dependencies: link them here,
		// but do NOT list them in SmartFoundations.uplugin's "Plugins" array. They ship with and
		// are enabled by the base game, and a .uplugin dependency entry gets read by SMR/ficsit.app
		// as a *mod* dependency it can't resolve -> "ent: mod not found" on upload (bit us in 33.5.0).
		PublicDependencyModuleNames.AddRange(new string[] {
			"AbstractInstance",  // #456: resolve instanced-mesh hits (belts/pipes/poles) to their owning buildable
			//"InstancedSplinesComponent",
			//"SignificanceISPC"
		});

		// Header stubs
		PublicDependencyModuleNames.AddRange(new string[] {
			"DummyHeaders",
		});

		if (Target.Type == TargetRules.TargetType.Editor) {
			PublicDependencyModuleNames.AddRange(new string[] {/*"OnlineBlueprintSupport",*/ "AnimGraph"});
		}
		PublicDependencyModuleNames.AddRange(new string[] {"FactoryGame", "SML"});
		
		PublicIncludePaths.AddRange(new string[] {
			// ... add public include paths required here ...
		});
		
		PrivateIncludePaths.AddRange(new string[] {
			// ... add private include paths required here ...
		});
		
		PublicDependencyModuleNames.AddRange(new string[] {
			// ... add public dependencies that you statically link with here ...
		});
		
		PrivateDependencyModuleNames.AddRange(new string[] {
			// ... add private dependencies that you statically link with here ...	
		});
		
		DynamicallyLoadedModuleNames.AddRange(new string[] {
			// ... add any modules that your module loads dynamically here ...
		});
	}
}
