// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

using UnrealBuildTool;

public class FootSyncMarkerGenerator : ModuleRules
{
	public FootSyncMarkerGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimationBlueprintLibrary",
			"AnimationModifiers"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"DeveloperSettings"
		});
	}
}
