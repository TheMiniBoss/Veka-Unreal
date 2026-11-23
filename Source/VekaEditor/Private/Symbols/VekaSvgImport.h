// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Symbols/VekaSymbol.h"

namespace Veka::Editor::Svg
{
	/**
	 * Symbol from an SVG file: every path / rect / circle / ellipse / line / polyline / polygon becomes a layer, fitted
	 * into the 24 x 24 design grid (viewBox kept centered). A filled element is a Fill layer, an element with only a
	 * stroke is a Stroke layer (its width becomes the layer's stroke scale, so it follows the symbol weight).
	 * Roles: `data-role="secondary"` or a class "secondary" / "tertiary", else from the opacity (<= 0.45 tertiary,
	 * <= 0.75 secondary). Variable layers: `data-variable="0"`, "1"... Explicit fill colors are kept for multicolor.
	 * Groups pass their style and translate / scale / matrix transforms down (rotation and skew are ignored, with a
	 * warning). Text, images and gradients are skipped with a warning.
	 */
	bool ImportSymbol(const FString& SvgText, FVekaSymbolDefinition& OutDefinition, TArray<FString>& OutWarnings, FString* OutError = nullptr);
}
