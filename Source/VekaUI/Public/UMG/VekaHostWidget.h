// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Core/VekaElement.h"
#include "Core/VekaTypes.h"
#include "VekaHostWidget.generated.h"

class SVekaHost;
class UVekaViewSource;

/**
 * Legacy / bridge: puts a Veka view inside any Widget Blueprint or Canvas. The Veka Editor is the main
 * workflow; this widget is for existing UMG screens.
 */
UCLASS(meta = (DisplayName = "Veka Host"))
class VEKAUI_API UVekaHostWidget : public UWidget
{
	GENERATED_BODY()

public:
	/** C++ view source to display (Phase 2: also UVekaView assets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	TSubclassOf<UVekaViewSource> ViewClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	EVekaColorScheme ColorScheme = EVekaColorScheme::Dark;

	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void SetViewClass(TSubclassOf<UVekaViewSource> InViewClass);

	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void SetColorScheme(EVekaColorScheme InColorScheme);

	/** Rebuilds the view from its source (state is reset). */
	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void Reload();

	/** C++: show an element directly (overrides ViewClass). */
	void SetRootElement(const FVekaElement& Root);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FVekaElement BuildRoot();
	void ApplyEnvironment();

	UPROPERTY(Transient)
	TObjectPtr<UVekaViewSource> SourceInstance;

	TSharedPtr<SVekaHost> MyHost;
	FVekaElement CppRoot;
	bool bHasCppRoot = false;
};
