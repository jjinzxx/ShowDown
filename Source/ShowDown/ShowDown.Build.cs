// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ShowDown : ModuleRules
{
	public ShowDown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "HTTP", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore", "OnlineSubsystem", "OnlineSubsystemEOS", "OnlineSubsystemUtils", "VoiceChat", "AudioCaptureCore", "AudioCapture", "LevelSequence", "MovieScene" });
		PrivateDependencyModuleNames.Add("MoviePlayer");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMGEditor", "Kismet", "AssetRegistry" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true

		StageRuntimeDirectory("Binaries/ThirdParty/Whisper");
		StageRuntimeFile("Content/LocalModels/Whisper/ggml-base.bin");
		StageRuntimeDirectory("Binaries/ThirdParty/eSpeakNG");
	}

	private void StageRuntimeDirectory(string RelativeDirectory)
	{
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string AbsoluteDirectory = Path.GetFullPath(Path.Combine(ProjectRoot, RelativeDirectory));
		if (!Directory.Exists(AbsoluteDirectory))
		{
			return;
		}

		foreach (string File in Directory.EnumerateFiles(AbsoluteDirectory, "*", SearchOption.AllDirectories))
		{
			RuntimeDependencies.Add(File, StagedFileType.NonUFS);
		}
	}

	private void StageRuntimeFile(string RelativeFile)
	{
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string AbsoluteFile = Path.GetFullPath(Path.Combine(ProjectRoot, RelativeFile));
		if (!File.Exists(AbsoluteFile))
		{
			return;
		}

		RuntimeDependencies.Add(AbsoluteFile, StagedFileType.NonUFS);
	}
}
