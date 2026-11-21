// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "VekaViewFactory.generated.h"

/** Content Browser > Add > User Interface > Veka View. Starts from a small starter document. */
UCLASS()
class UVekaViewFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVekaViewFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

/** Imports a Veka source file (.vk) as a Veka View: drag it into the Content Browser, or Import. */
UCLASS()
class UVekaViewImportFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVekaViewImportFactory();

	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer,
		const TCHAR* BufferEnd, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
};
