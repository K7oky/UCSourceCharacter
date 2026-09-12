// Source-compatible movement simulation for Unreal Engine 5.6

using UnrealBuildTool;

public class SourceMovement : ModuleRules
{
	public SourceMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Needs /fp:precise. Fast math breaks the golden tests: fix the flags, not the tests.

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",

			// Legacy axis bindings only. No EnhancedInput: input must reduce to FSourceInputState.
			"InputCore",

			// Chaos sweeps, plus EPhysicalSurface in a public header, so it cannot be private
			"PhysicsCore"
		});
	}
}
