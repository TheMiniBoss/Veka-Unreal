// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/VekaViewAsset.h"
#include "VekaSampleView.generated.h"

/** View model of the sample shop view: what the game would give it. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, meta = (DisplayName = "Veka Sample Shop Model"))
class VEKAUI_API UVekaSampleViewModel : public UVekaViewModel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	FString PlayerName = TEXT("Aria");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "0", ClampMax = "1"))
	float Health = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "0"))
	int32 Gold = 120;
};

namespace Veka::Samples
{
	/**
	 * A potion shop: reads the model (name, health, gold), has its own state (count, gift wrap), shows a line when
	 * gift wrap is on, and emits "Buy" with the count; the button is disabled when the gold is short.
	 */
	VEKAUI_API FVekaDocument MakeShopDocument();
}
