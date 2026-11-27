// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaColoraModifiers.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "VekaBackdrop.h"
#include "Rendering/DrawElements.h"

void FVekaThemeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<const UVekaTheme*>(VekaEnv::Theme(), Theme.Get());
}

void FVekaColorAdjustModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	// Environment modifiers run outermost first, so the existing matrix is the outer one: apply ours first.
	const FVekaColorMatrix Mine = FVekaColorMatrix::Make(Adjust, Amount);
	const FVekaColorMatrix* Outer = VekaEnv::GetColorMatrix(Environment);
	Environment.Set<FVekaColorMatrix>(VekaEnv::ColorMatrix(), Outer ? Mine.Then(*Outer) : Mine);
}

void FVekaBlendModeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<EVekaBlendMode>(VekaEnv::BlendMode(), Mode);
}

bool FVekaBlendModeModifier::PaintsContentTwice() const
{
	return UsesBackdrop(Mode) && Veka::Render::GetBackdropQuality() != EVekaBackdropQuality::Off;
}

int32 FVekaBlendModeModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	Elements.Reset();
	if (!PaintsContentTwice() || Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	const FSlateRenderTransform& Transform = Context.Geometry.GetAccumulatedRenderTransform();
	const FVector2f A = Transform.TransformPoint(FVector2f(Rect.Position));
	const FVector2f B = Transform.TransformPoint(FVector2f(Rect.Position + Rect.Size));
	FVekaBlendParams Params;
	Params.Min = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
	Params.Max = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
	Params.Mode = static_cast<int32>(Mode) - static_cast<int32>(EVekaBlendMode::Multiply);
	Params.Clip = Veka::Render::CurrentClip(Context.DrawElements);
	Elements = MakeShared<FVekaBlendElements>(Veka::Render::MakeBlendElements(Params));
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Elements->Begin);
	return Context.LayerId + 1;
}

int32 FVekaBlendModeModifier::PaintBetween(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (!Elements.IsValid())
	{
		return Context.LayerId;
	}
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Elements->Between);
	return Context.LayerId + 1;
}

int32 FVekaBlendModeModifier::PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (!Elements.IsValid())
	{
		return Context.LayerId;
	}
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Elements->End);
	Elements.Reset();
	return Context.LayerId + 1;
}
