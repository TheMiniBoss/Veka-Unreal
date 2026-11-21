// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/VekaPaint.h"
#include "VekaPaintAsset.generated.h"

/**
 * A named paint of the project (any FVekaPaint: mesh gradient, gradient, image...). Views use it by its asset name where
 * a Colora token goes: `.background(.Aurora)`. Loaded paints register themselves; the editor loads all of them.
 */
UCLASS(BlueprintType)
class VEKAUI_API UVekaPaintAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Paint")
	FVekaPaint Paint;

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif
};

namespace Veka::Paints
{
	VEKAUI_API void Register(UVekaPaintAsset* Asset);
	VEKAUI_API void Unregister(UVekaPaintAsset* Asset);
	/** Paint of a registered asset by asset name, or nullptr. */
	VEKAUI_API const FVekaPaint* Find(FName Name);
	VEKAUI_API TArray<FName> GetAllNames();
	/** Bumped when a registered paint changes (views that use it re-evaluate). */
	VEKAUI_API uint32 GetRevision();
	VEKAUI_API void NotifyChanged();
}
