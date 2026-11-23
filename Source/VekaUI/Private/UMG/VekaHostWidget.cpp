// Copyright TheMiniBoss. All Rights Reserved.

#include "UMG/VekaHostWidget.h"
#include "UMG/VekaViewSource.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "View/VekaViewAsset.h"

#define LOCTEXT_NAMESPACE "VekaUI"

TSharedRef<SWidget> UVekaHostWidget::RebuildWidget()
{
	MyHost = SNew(SVekaHost);
	ApplyEnvironment();
	MyHost->SetRoot(BuildRoot());
	return MyHost.ToSharedRef();
}

void UVekaHostWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyHost.IsValid())
	{
		ApplyEnvironment();
	}
}

void UVekaHostWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyHost.Reset();
	Store.Reset();
}

FVekaElement UVekaHostWidget::BuildRoot()
{
	if (bHasCppRoot)
	{
		return CppRoot;
	}
	if (View)
	{
		return View->BuildRoot(GetOrCreateStore());
	}
	if (ViewClass)
	{
		if (!SourceInstance || SourceInstance->GetClass() != ViewClass)
		{
			SourceInstance = NewObject<UVekaViewSource>(this, ViewClass);
		}
		return SourceInstance->BuildRoot();
	}
	return FVekaElement();
}

void UVekaHostWidget::ApplyEnvironment()
{
	if (!MyHost.IsValid())
	{
		return;
	}
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Environment.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), ColorScheme);
	Environment.Set<const UVekaTheme*>(VekaEnv::Theme(), Theme.Get());
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
	MyHost->SetEnvironment(Environment);
}

TSharedRef<FVekaViewStore> UVekaHostWidget::GetOrCreateStore()
{
	if (!Store.IsValid())
	{
		Store = MakeShared<FVekaViewStore>();
		for (const TPair<FName, FVekaValue>& Pair : InitialState)
		{
			Store->Set(Pair.Key, Pair.Value);
		}
		Store->SetModel(ViewModel);
		const TWeakObjectPtr<UVekaHostWidget> WeakThis = this;
		Store->OnEvent = [WeakThis](FName Event, const FVekaValue& Payload)
		{
			if (UVekaHostWidget* Pinned = WeakThis.Get())
			{
				Pinned->OnVekaEvent.Broadcast(Event, Payload);
			}
		};
	}
	return Store.ToSharedRef();
}

void UVekaHostWidget::SetState(FName Name, const FVekaValue& Value)
{
	GetOrCreateStore()->Set(Name, Value);
}

FVekaValue UVekaHostWidget::GetState(FName Name) const
{
	if (Store.IsValid() && Store->Contains(Name))
	{
		return Store->Get(Name);
	}
	const FVekaValue* Initial = InitialState.Find(Name);
	if (Initial)
	{
		return *Initial;
	}
	if (View)
	{
		for (const FVekaStateDeclaration& State : View->Document.States)
		{
			if (State.Name == Name)
			{
				return State.Initial;
			}
		}
	}
	return FVekaValue();
}

void UVekaHostWidget::SetView(UVekaView* InView)
{
	View = InView;
	bHasCppRoot = false;
	Reload();
}

void UVekaHostWidget::SetViewModel(UVekaViewModel* InViewModel)
{
	ViewModel = InViewModel;
	if (Store.IsValid())
	{
		Store->SetModel(InViewModel);
	}
}

void UVekaHostWidget::SetViewClass(TSubclassOf<UVekaViewSource> InViewClass)
{
	ViewClass = InViewClass;
	bHasCppRoot = false;
	Reload();
}

void UVekaHostWidget::SetTheme(UVekaTheme* InTheme)
{
	Theme = InTheme;
	ApplyEnvironment();
}

void UVekaHostWidget::SetColorScheme(EVekaColorScheme InColorScheme)
{
	ColorScheme = InColorScheme;
	ApplyEnvironment();
}

void UVekaHostWidget::Reload()
{
	if (MyHost.IsValid())
	{
		// A fresh root element: the runtime reuses nodes of the same type; force new state with a new host.
		MyHost->SetRoot(FVekaElement());
		Store.Reset();
		MyHost->SetRoot(BuildRoot());
	}
}

void UVekaHostWidget::SetRootElement(const FVekaElement& Root)
{
	CppRoot = Root;
	bHasCppRoot = true;
	if (MyHost.IsValid())
	{
		MyHost->SetRoot(CppRoot);
	}
}

#if WITH_EDITOR
const FText UVekaHostWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "VekaUI");
}
#endif

#undef LOCTEXT_NAMESPACE
