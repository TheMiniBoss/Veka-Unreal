// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Core/VekaElement.h"
#include "Core/VekaTypes.h"
#include "View/VekaValue.h"
#include "VekaHostWidget.generated.h"

class SVekaHost;
class UVekaViewSource;
class UVekaTheme;
class UVekaView;
class UVekaViewModel;
class FVekaViewStore;

/** An asset view emitted an event (`emit`): its name and payload. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVekaEventSignature, FName, EventName, const FVekaValue&, Payload);

/**
 * Legacy / bridge: puts a Veka view inside any Widget Blueprint or Canvas. The Veka Editor is the main
 * workflow; this widget is for existing UMG screens.
 */
UCLASS(meta = (DisplayName = "Veka Host"))
class VEKAUI_API UVekaHostWidget : public UWidget
{
	GENERATED_BODY()

public:
	/** Asset view to display (made in the Veka Editor, or in C++). Takes precedence over ViewClass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	TObjectPtr<UVekaView> View;

	/** State values set before the view starts (override the view's own initial values). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	TMap<FName, FVekaValue> InitialState;

	/** Data the asset view reads (`model.Health`) and watches. Pick a class and edit its values here, or set one from Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "VekaUI")
	TObjectPtr<UVekaViewModel> ViewModel;

	/** Events the asset view emits (`emit("Buy", count)`). */
	UPROPERTY(BlueprintAssignable, Category = "VekaUI")
	FVekaEventSignature OnVekaEvent;

	/** C++ view source to display. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	TSubclassOf<UVekaViewSource> ViewClass;

	/** Sets a state of the asset view (re-evaluates it). Works before the view is shown too. */
	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void SetState(FName Name, const FVekaValue& Value);

	/** Current value of a state of the asset view (None if unknown). */
	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	FVekaValue GetState(FName Name) const;

	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void SetView(UVekaView* InView);

	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void SetViewModel(UVekaViewModel* InViewModel);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	EVekaColorScheme ColorScheme = EVekaColorScheme::Dark;

	/** Colora theme of the whole view (nullptr = built-in defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	TObjectPtr<UVekaTheme> Theme;

	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void SetTheme(UVekaTheme* InTheme);

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

	/** Store of the asset view (state, model, events); recreated by Reload. */
	TSharedRef<FVekaViewStore> GetOrCreateStore();

	TSharedPtr<SVekaHost> MyHost;
	TSharedPtr<FVekaViewStore> Store;
	FVekaElement CppRoot;
	bool bHasCppRoot = false;
};
