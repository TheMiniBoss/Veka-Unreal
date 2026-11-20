// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/VekaElement.h"
#include "Core/VekaEnvironment.h"

class FVekaRuntime;
class FVekaNode;
class FActiveTimerHandle;

/**
 * Slate widget that displays a Veka element tree. Usable anywhere in Slate (editor tabs, game UI) and
 * wrapped for UMG by UVekaHostWidget.
 *
 *   SNew(SVekaHost).Root(Veka::Text(INVTEXT("Hello")))
 *
 * Ticks (through an active timer) only while views are dirty or animations run.
 */
class VEKAUI_API SVekaHost : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaHost) {}
		SLATE_ARGUMENT(FVekaElement, Root)
	SLATE_END_ARGS()

	SVekaHost();
	virtual ~SVekaHost() override;

	void Construct(const FArguments& InArgs);

	void SetRoot(const FVekaElement& Root);
	void SetEnvironment(const FVekaEnvironment& Environment);
	TSharedRef<FVekaRuntime> GetRuntime() const;

private:
	void RequestTick();
	EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);
	void RefreshRootWidget();

	TSharedPtr<FVekaRuntime> Runtime;
	TWeakPtr<FActiveTimerHandle> ActiveTimer;
	TWeakPtr<FVekaNode> DisplayedRoot;
	FDelegateHandle RootChangedHandle;
};
