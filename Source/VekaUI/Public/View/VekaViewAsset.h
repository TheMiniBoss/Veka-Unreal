// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/Ticker.h"
#include "Core/VekaElement.h"
#include "UMG/VekaViewSource.h"
#include "View/VekaValue.h"
#include "View/VekaDocument.h"
#include "VekaViewAsset.generated.h"

class FVekaNode;
class UVekaViewModel;
class FVekaTimelinePlayer;

/**
 * Data the game gives to an asset view, observed by it: subclass in Blueprint, add variables, assign it to a Veka Host
 * (ViewModel). The view reads them by name (`model.Health`). Changes are seen on their own (checked ten times a
 * second); call NotifyChanged to update at once.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class VEKAUI_API UVekaViewModel : public UObject
{
	GENERATED_BODY()

public:
	/** Refreshes the views that read this model now. */
	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	void NotifyChanged();

	/** Value of a property by name (FVekaValue conversion). */
	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	FVekaValue GetValue(FName Property) const;

	/** Sets a property by name and refreshes the views. False if there is no such property or the type does not fit. */
	UFUNCTION(BlueprintCallable, Category = "VekaUI")
	bool SetValue(FName Property, const FVekaValue& Value);

	/** Hash of all property values (change detection). */
	uint32 ComputeHash() const;

	FSimpleMulticastDelegate OnChanged;
};

/**
 * Values, model and event sink of one mounted asset view. Shared by the host (SetState / GetState) and the
 * interpreter (reads, actions); a change re-evaluates the view.
 */
class VEKAUI_API FVekaViewStore : public TSharedFromThis<FVekaViewStore>
{
public:
	~FVekaViewStore();

	FVekaValue Get(FName Name) const;
	bool Contains(FName Name) const { return Values.Contains(Name); }
	/** Sets a state; re-evaluates the view if the value changed. */
	void Set(FName Name, const FVekaValue& Value);
	/** Initial values: declared ones not set yet (SetState before the view mounts wins). */
	void Declare(FName Name, const FVekaValue& Initial);

	void SetModel(UVekaViewModel* InModel);
	/** SetModel without re-evaluating (used during an evaluation). */
	void SetModelSilently(UVekaViewModel* InModel);
	UVekaViewModel* GetModel() const { return Model.Get(); }
	FVekaValue GetModelValue(FName Property) const;

	void Emit(FName Event, const FVekaValue& Payload) const;
	TFunction<void(FName, const FVekaValue&)> OnEvent;
	/** Called after a state changed value (the editor's Test mode logs it). */
	TFunction<void(FName, const FVekaValue&)> OnValueChanged;

	/** Timelines of the mounted view by name (the editor's Timeline tab plays and scrubs them). */
	TMap<FName, TSharedPtr<FVekaTimelinePlayer>> Timelines;
	/** Timelines already auto-played for this store. */
	TSet<FName> StartedTimelines;

	/** The interpreted view registers its node here (re-evaluated on changes). */
	void Attach(FVekaNode& Node);
	void Invalidate();

	const TMap<FName, FVekaValue>& GetValues() const { return Values; }

private:
	bool PollModel(float DeltaTime);

	TMap<FName, FVekaValue> Values;
	TWeakObjectPtr<UVekaViewModel> Model;
	FDelegateHandle ModelChangedHandle;
	FTSTicker::FDelegateHandle PollHandle;
	uint32 ModelHash = 0;
	TArray<TWeakPtr<FVekaNode>> Views;
};

/** A view made in the Veka Editor (Phase 3) or in C++: an AST interpreted at runtime. Put it in a Veka Host. */
UCLASS(BlueprintType)
class VEKAUI_API UVekaView : public UVekaViewSource
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VekaUI")
	FVekaDocument Document;

#if WITH_EDITORONLY_DATA
	/** View model the Veka Editor previews this view with (`model.X` values in the viewport). Editor only. */
	UPROPERTY(EditAnywhere, Category = "Preview")
	TSubclassOf<UVekaViewModel> PreviewModelClass;

	/**
	 * Veka code as last written in the Code tab (its layout and blank lines). Shown again only while it still parses to
	 * Document; otherwise the Code tab prints the document. Editor only.
	 */
	UPROPERTY()
	FString SourceText;
#endif

	/** Element of the view bound to a store (state, model and events). */
	FVekaElement BuildRoot(const TSharedRef<FVekaViewStore>& Store) const;

	/** Store with the declared initial values. */
	virtual FVekaElement BuildRoot() const override;

	/** Older documents are converted to the current format. */
	virtual void PostLoad() override;
};

namespace Veka::Interpreter
{
	/** Element of a document bound to a store. */
	VEKAUI_API FVekaElement MakeView(const FVekaDocument& Document, const TSharedRef<FVekaViewStore>& Store);
}
