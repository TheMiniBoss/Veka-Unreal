// Copyright TheMiniBoss. All Rights Reserved.

#include "Slate/SVekaHost.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "VekaBackdrop.h"
#include "Rendering/DrawElements.h"
#include "VekaUIStats.h"
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

int32 SVekaHost::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	SCOPE_CYCLE_COUNTER(STAT_VekaPaint);
	INC_DWORD_STAT(STAT_VekaHostsPainted);
	const TSharedRef<FVekaBackdropLayer> Backdrops = Veka::Render::MakeBackdropLayer();
	int32 MaxLayer;
	{
		const Veka::Backdrop::FLayerScope Scope(*Backdrops);
		MaxLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
	}
	// The first backdrop drawn takes the layer's capture (it sees the host's own content drawn before it).
	return MaxLayer;
}
