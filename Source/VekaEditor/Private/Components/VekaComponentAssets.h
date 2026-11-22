// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AssetDefinitionDefault.h"
#include "VekaComponentAssets.generated.h"

struct FVekaSharedStyle;
class UVekaView;

namespace Veka::Editor::Components
{
	/** Parses Style.Modifiers into Style.Compiled (errors in Style.CompileErrors, Compiled emptied). */
	void CompileStyle(FVekaSharedStyle& Style);

	/** One entry of the palette's component groups. */
	struct FComponentEntry
	{
		TObjectPtr<UVekaView> View;
		/** Library name, or "Project" for views in no library. */
		FText Group;
	};

	/**
	 * The project's views usable as components (asset registry; library views first, under the library's name). Except
	 * is left out (a view cannot contain itself).
	 */
	TArray<FComponentEntry> GatherComponents(const UVekaView* Except = nullptr);

	/** "title: Text, count: Int = 3 · slots: default, footer" (for tooltips and completion). */
	FString DescribeComponent(const UVekaView& View);
}

/** Content Browser > Add > User Interface > Veka Component Library. */
UCLASS()
class UVekaComponentLibraryFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVekaComponentLibraryFactory();
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

UCLASS()
class UAssetDefinition_VekaComponentLibrary : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
