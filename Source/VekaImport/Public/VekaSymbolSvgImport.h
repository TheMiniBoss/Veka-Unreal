// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Symbols/VekaSymbol.h"

namespace Veka::Import
{
	/**
	 * Minimal SVG -> symbol import: <path d>, <circle>, <rect>, <line>, <polyline>, <polygon>, scaled from the
	 * viewBox to the 24 x 24 grid. Elements with a fill become Fill shapes, the others Stroke shapes.
	 * Roles come from class / id / data-role containing "primary", "secondary" or "tertiary".
	 */
	VEKAIMPORT_API bool ImportSymbolSvg(const FString& SvgText, FName Name, FVekaSymbolDefinition& OutDefinition, FString* OutError = nullptr);
}
