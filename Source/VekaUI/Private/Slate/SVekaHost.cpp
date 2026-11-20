// Copyright TheMiniBoss. All Rights Reserved.

#include "Slate/SVekaHost.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Widgets/SNullWidget.h"

SVekaHost::SVekaHost()
	: Runtime(MakeShared<FVekaRuntime>())
{
	SetCanTick(false);
}

SVekaHost::~SVekaHost()
{
	if (Runtime.IsValid())
	{
		Runtime->OnUpdateRequested = nullptr;
		Runtime->OnRootChanged.Remove(RootChangedHandle);
	}
}

void SVekaHost::Construct(const FArguments& InArgs)
{
	TWeakPtr<SVekaHost> WeakThis = SharedThis(this);
	Runtime->OnUpdateRequested = [WeakThis]()
	{
		if (TSharedPtr<SVekaHost> Host = WeakThis.Pin())
		{
			Host->RequestTick();
		}
	};
	RootChangedHandle = Runtime->OnRootChanged.AddSP(this, &SVekaHost::RefreshRootWidget);

	ChildSlot
	[
		SNullWidget::NullWidget
	];

	if (!InArgs._Root.IsEmpty())
	{
		SetRoot(InArgs._Root);
	}
}

void SVekaHost::SetRoot(const FVekaElement& Root)
{
	Runtime->SetRoot(Root);
	RefreshRootWidget();
}

void SVekaHost::SetEnvironment(const FVekaEnvironment& Environment)
{
	Runtime->SetEnvironment(Environment);
	RefreshRootWidget();
}

TSharedRef<FVekaRuntime> SVekaHost::GetRuntime() const
{
	return Runtime.ToSharedRef();
}

void SVekaHost::RefreshRootWidget()
{
	TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();
	if (Root == DisplayedRoot.Pin())
	{
		return;
	}
	DisplayedRoot = Root;
	ChildSlot
	[
		Root.IsValid() ? Root->GetOrCreateWidget() : SNullWidget::NullWidget
	];
}

void SVekaHost::RequestTick()
{
	if (!ActiveTimer.IsValid())
	{
		ActiveTimer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SVekaHost::HandleActiveTimer));
	}
}

EActiveTimerReturnType SVekaHost::HandleActiveTimer(double InCurrentTime, float InDeltaTime)
{
	const bool bMoreWork = Runtime->Tick(InDeltaTime);
	RefreshRootWidget();
	if (bMoreWork)
	{
		return EActiveTimerReturnType::Continue;
	}
	ActiveTimer.Reset();
	return EActiveTimerReturnType::Stop;
}
