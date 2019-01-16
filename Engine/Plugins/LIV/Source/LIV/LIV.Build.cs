using System.IO;

namespace UnrealBuildTool.Rules
{
    public class LIV : ModuleRules
    {

#if WITH_FORWARDED_MODULE_RULES_CTOR
        public LIV(ReadOnlyTargetRules Target) : base(Target)
#else
        public LIV(TargetInfo Target)
#endif
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PrivateIncludePaths.AddRange(new string[]
            {
                Path.Combine(ModuleDirectory, "Private")
            });

            PublicIncludePaths.AddRange(new string[]
            {
                "Runtime/Core/Public/HAL",
                "Runtime/Core/Public/Misc",

                Path.Combine(ModuleDirectory, "Public")
            });

            PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "D3D11RHI",
                "Engine",
                "HeadMountedDisplay",
                "InputCore",
                "OpenVR",
                "RenderCore",
                "RHI",
                "SteamVR"
            });

            CreateVersionDefinitions();
        }

        void CreateVersionDefinitions()
        {
#if WITH_FORWARDED_MODULE_RULES_CTOR
            CreateDefinition("UE_4_16_OR_LATER");
#endif

#if UE_4_17_OR_LATER
            CreateDefinition("UE_4_17_OR_LATER");
#endif

#if UE_4_18_OR_LATER
            CreateDefinition("UE_4_18_OR_LATER");
#endif

#if UE_4_19_OR_LATER
            CreateDefinition("UE_4_19_OR_LATER");
#endif

#if UE_4_20_OR_LATER
            CreateDefinition("UE_4_20_OR_LATER");
#endif
        }

        void CreateDefinition(string definition)
        {
#if UE_4_19_OR_LATER
            PublicDefinitions.Add(definition);
#else
            Definitions.Add(definition);
#endif
        }
    }
}
