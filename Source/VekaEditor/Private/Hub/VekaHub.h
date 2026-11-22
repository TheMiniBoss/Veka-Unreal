// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaElement.h"

class UVekaTheme;
class UVekaView;

/**
 * Veka Hub (Phase 4d): a nomad tab (Window > Tools, the level editor toolbar, the view editor toolbar), written in
 * VekaUI. Its DesignLab shows every component, Colora color, symbol, text style, animation, effect and morph, with
 * live settings, in Light / Dark and in any theme of the project (built-in, VT_Alpha13...). Examples, documentation
 * and changelog come with 4e.
 */
namespace Veka::Editor::Hub
{
	extern const FName TabId;

	enum class ESection : uint8
	{
		Components,
		Colors,
		Symbols,
		Typography,
		Animations,
		Effects,
		Morph,
		Examples,
		Documentation,
		Changelog,
		Count
	};

	FText SectionTitle(ESection Section);

	/** The Hub's root view, opened on InitialSection, with Themes in its theme picker (after the built-in one). */
	FVekaElement MakeHub(ESection InitialSection, TArray<UVekaTheme*> Themes);
	/** The project's theme assets (asset registry, loaded). */
	TArray<UVekaTheme*> GatherThemes();

	/** An example view of the plugin (Samples/Veka): its Veka source, a title and the first comment as description. */
	struct FExample
	{
		FName Name;
		FText Title;
		FText Description;
		FString Source;
	};
	TArray<FExample> GetExamples();
	/**
	 * Creates the example as a Veka View in /Game/VekaExamples (or finds it there) and opens it in the view editor.
	 * The new asset is left unsaved (the user saves it). nullptr if its source does not parse.
	 */
	UVekaView* OpenExample(const FExample& Example, bool bOpenEditor = true);

	/** A release of Docs/CHANGELOG.md (`## title`, `- entries`). */
	struct FChangelogRelease
	{
		FString Title;
		TArray<FString> Entries;
	};
	TArray<FChangelogRelease> GetChangelog();

	/** Tab spawner and toolbar entries (editor module startup / shutdown). */
	void Register();
	void Unregister();
}
