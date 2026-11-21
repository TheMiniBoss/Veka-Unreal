// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaViewFactory.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "AssetTypeCategories.h"

#define LOCTEXT_NAMESPACE "VekaEditor"

UVekaViewFactory::UVekaViewFactory()
{
	SupportedClass = UVekaView::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVekaViewFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVekaView* View = NewObject<UVekaView>(InParent, InClass, InName, Flags);
	View->Document = Veka::Samples::MakeStarterDocument();
	return View;
}

FText UVekaViewFactory::GetDisplayName() const
{
	return LOCTEXT("VekaViewFactoryName", "Veka View");
}

uint32 UVekaViewFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::UI;
}

#undef LOCTEXT_NAMESPACE
