// Copyright TheMiniBoss. All Rights Reserved.

#include "UMG/VekaHostWidget.h"
#include "UMG/VekaViewSource.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"

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
}

FVekaElement UVekaHostWidget::BuildRoot()
{
	if (bHasCppRoot)
	{
		return CppRoot;
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
