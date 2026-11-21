// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "VekaViewFactory.generated.h"

/** Content Browser > Add > VekaUI > Veka View. Starts from the sample shop document (Phase 3: the Veka Editor). */
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
