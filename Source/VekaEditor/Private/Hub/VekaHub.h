// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaElement.h"

class UVekaTheme;

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

	/** Tab spawner and toolbar entries (editor module startup / shutdown). */
	void Register();
	void Unregister();
}
