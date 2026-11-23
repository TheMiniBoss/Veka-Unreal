// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "View/VekaViewAsset.h"
#include "VekaSampleView.generated.h"

class UVekaHostWidget;

/** One article of the sample shop. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSampleItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	int32 Price = 10;

	/** Veka symbol name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	FName Symbol = TEXT("cube");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	bool bNew = false;
};

/** View model of the sample shop view: what the game would give it. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, meta = (DisplayName = "Veka Sample Shop Model"))
class VEKAUI_API UVekaSampleViewModel : public UVekaViewModel
{
	GENERATED_BODY()

public:
	UVekaSampleViewModel();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	FString PlayerName = TEXT("Aria");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "0", ClampMax = "1"))
	float Health = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "0"))
	int32 Gold = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample")
	TArray<FVekaSampleItem> Items;
};

/** What the sample Widget Blueprint's event graph calls (Veka.CreateSampleAssets wires it to OnVekaEvent). */
UCLASS()
class VEKAUI_API UVekaSampleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** "Buy" (payload = price): takes the gold from the host's sample model. */
	UFUNCTION(BlueprintCallable, Category = "VekaUI|Sample")
	static void HandleShopEvent(UVekaHostWidget* Host, FName EventName, const FVekaValue& Payload);
};

namespace Veka::Samples
{
	/** Component: one article (params title / price / symbol, slot "badge", event "buy" with the price). */
	VEKAUI_API FVekaDocument MakeShopItemDocument();

	/**
	 * The shop: player name, gold and health from the model, one ShopItem component per model item (disabled when
	 * too expensive, "NEW" badge in the slot), a gift-wrap toggle, the last article bought; emits "Buy" (price).
	 */
	VEKAUI_API FVekaDocument MakeShopDocument(UVekaView* ShopItem);

	/** New asset content: a title and a counter. */
	VEKAUI_API FVekaDocument MakeStarterDocument();
}
