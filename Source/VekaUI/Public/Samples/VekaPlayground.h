// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaElement.h"
#include "UMG/VekaViewSource.h"
#include "VekaPlayground.generated.h"

namespace Veka::Samples
{
	/** Phase 1 demo: stacks, shapes, strokes, text wrapping, grid, scroll, state and a spring animation. */
	VEKAUI_API FVekaElement Playground();

	/** The playground's galleries, one per domain (the Veka Hub's DesignLab shows them). */
	VEKAUI_API FVekaElement ControlsGallery();
	VEKAUI_API FVekaElement ColoraGallery();
	VEKAUI_API FVekaElement SymbolsGallery();
	VEKAUI_API FVekaElement TypographyGallery();
	VEKAUI_API FVekaElement AnimaGallery();
	VEKAUI_API FVekaElement MorphGallery();
}

/** Lets a Veka Host widget show the playground (set ViewClass = VekaPlaygroundViewSource). */
UCLASS(meta = (DisplayName = "Veka Playground"))
class VEKAUI_API UVekaPlaygroundViewSource : public UVekaViewSource
{
	GENERATED_BODY()

public:
	virtual FVekaElement BuildRoot() const override { return Veka::Samples::Playground(); }
};
