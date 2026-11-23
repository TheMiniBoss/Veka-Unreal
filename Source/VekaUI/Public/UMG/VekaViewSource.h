// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/VekaElement.h"
#include "VekaViewSource.generated.h"

/**
 * Something that produces a Veka view for a host. Subclass in C++ to expose a C++ view to UMG:
 *
 *   UCLASS() class UMyHudSource : public UVekaViewSource
 *   {
 *       GENERATED_BODY()
 *       virtual FVekaElement BuildRoot() const override { return Veka::View<FMyHud>(); }
 *   };
 *
 * Phase 2: the UVekaView asset (made in the Veka Editor) is also a view source.
 */
UCLASS(Abstract, BlueprintType)
class VEKAUI_API UVekaViewSource : public UObject
{
	GENERATED_BODY()

public:
	virtual FVekaElement BuildRoot() const { return FVekaElement(); }
};
