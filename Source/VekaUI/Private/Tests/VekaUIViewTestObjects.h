// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "View/VekaValue.h"
#include "VekaUIViewTestObjects.generated.h"

/** Test helper: receives OnVekaEvent through a dynamic delegate, like a Blueprint event does. */
UCLASS(Transient, NotBlueprintable)
class UVekaTestEventSink : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleEvent(FName EventName, const FVekaValue& Payload)
	{
		Events.Add(EventName);
		Payloads.Add(Payload);
	}

	TArray<FName> Events;
	TArray<FVekaValue> Payloads;
};
