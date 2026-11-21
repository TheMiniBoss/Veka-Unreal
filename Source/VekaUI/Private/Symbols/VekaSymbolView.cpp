// Copyright TheMiniBoss. All Rights Reserved.

#include "Symbols/VekaSymbolView.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintRenderer.h"
#include "Typography/VekaTypography.h"
#include "Vector/VekaPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

bool FVekaSymbolStyle::operator==(const FVekaSymbolStyle& Other) const
{
	return RenderingMode == Other.RenderingMode && Variant == Other.Variant && Scale == Other.Scale && Palette == Other.Palette
		&& bOverrideWeight == Other.bOverrideWeight && Weight == Other.Weight && Effect == Other.Effect && bEffectActive == Other.bEffectActive
		&& EffectTrigger == Other.EffectTrigger && Replace == Other.Replace && ReplaceDuration == Other.ReplaceDuration;
}

void FVekaSymbolStyleModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	FVekaSymbolStyle S = VekaEnv::GetSymbolStyle(Environment);
	const FVekaSymbolStyle& V = Value;
	switch (Field)
	{
	case EVekaSymbolStyleField::RenderingMode: S.RenderingMode = V.RenderingMode; break;
	case EVekaSymbolStyleField::Variant: S.Variant = S.Variant.Merged(V.Variant); break;
	case EVekaSymbolStyleField::Scale: S.Scale = V.Scale; break;
	case EVekaSymbolStyleField::Palette: S.Palette = V.Palette; break;
	case EVekaSymbolStyleField::Weight: S.bOverrideWeight = true; S.Weight = FMath::Clamp(V.Weight, 0.f, 1.f); break;
	case EVekaSymbolStyleField::Effect: S.Effect = V.Effect; S.bEffectActive = V.bEffectActive; S.EffectTrigger = V.EffectTrigger; break;
	case EVekaSymbolStyleField::Replace: S.Replace = V.Replace; S.ReplaceDuration = V.ReplaceDuration; break;
	}
	Environment.Set<FVekaSymbolStyle>(VekaEnv::SymbolStyle(), S);
}

const FVekaSymbolStyle& VekaEnv::GetSymbolStyle(const FVekaEnvironment& Environment)
{
	static const FVekaSymbolStyle Default;
	const FVekaSymbolStyle* Found = Environment.Find<FVekaSymbolStyle>(SymbolStyle());
	return Found ? *Found : Default;
}

float Veka::Symbols::GetSymbolSize(const FVekaEnvironment& Environment)
{
	const Veka::Typography::FResolvedText Resolved = Veka::Typography::Resolve(Environment);
	// Slate font sizes are points (96 dpi): x 4/3 for pixels.
	return FMath::Max(Resolved.Font.Size * (4.f / 3.f), 1.f) * ScaleFactor(VekaEnv::GetSymbolStyle(Environment).Scale);
}

namespace
{
	using namespace Veka::Symbols;

	constexpr double Grid = 24.0;
	const FVector2D GridCenter(12.0, 12.0);

	/** One fill or stroke of a symbol, tessellated once. */
	struct FCachedOp
	{
		FVekaTessellation Mesh;
		FVekaPaint Paint;
		int32 LayerOffset = 0;
	};

	/** Everything the tessellated symbol depends on (not the position on screen nor the opacity). */
	struct FSymbolCacheKey
	{
		const FVekaSymbolDefinition* Definition = nullptr;
		FVekaSymbolVariant Variant;
		FVector2D BoxPosition = FVector2D::ZeroVector;
		FVector2D BoxSize = FVector2D::ZeroVector;
		float StrokePixels = 0.f;
		double RenderScale = 0.0;
		FVekaPaint Foreground;
		FVekaPaint Knockout;
		FVekaSymbolStyle Style;
		float VariableValue = -1.f;
		/** Veka::Symbols::GetRevision: an edited symbol is drawn again. */
		uint32 Revision = 0;

		bool operator==(const FSymbolCacheKey& Other) const
		{
			return Definition == Other.Definition && Revision == Other.Revision && Variant == Other.Variant && BoxPosition.Equals(Other.BoxPosition, 1e-4) && BoxSize.Equals(Other.BoxSize, 1e-4)
				&& StrokePixels == Other.StrokePixels && RenderScale == Other.RenderScale && Foreground == Other.Foreground && Knockout == Other.Knockout
				&& Style == Other.Style && VariableValue == Other.VariableValue;
		}
	};

	struct FVekaSymbolState : public FVekaPrimitiveState
	{
		bool bInitialized = false;

		// Tessellation cache (static symbols: most of them).
		bool bHasCache = false;
		FSymbolCacheKey CachedKey;
		TArray<FCachedOp> CachedOps;
		int32 CachedLayers = 0;

		// Effect.
		int64 LastTrigger = 0;
		bool bLastActive = false;
		EVekaSymbolEffectKind LastKind = EVekaSymbolEffectKind::None;
		double EffectStart = -1.0;
		FVekaSymbolEffect Effect;
		bool bEffectActive = false;
		int32 VariableSteps = 3;

		// Replace.
		FName CurrentName;
		FVekaSymbolVariant CurrentVariant;
		FName PreviousName;
		FVekaSymbolVariant PreviousVariant;
		double ReplaceStart = -1.0;
		float ReplaceDuration = 0.35f;
		EVekaSymbolReplace ReplaceMode = EVekaSymbolReplace::None;

		// Shapes drawn inside a filled outline (knockouts of the .fill variant), per definition.
		TMap<const FVekaSymbolDefinition*, TArray<bool>> Knockouts;

		virtual bool NeedsAnimationFrame(double Now) const override
		{
			if (EffectStart >= 0.0 && !IsEffectFinished(Effect, Now - EffectStart, bEffectActive, VariableSteps))
			{
				return true;
			}
			return ReplaceStart >= 0.0 && Now - ReplaceStart < ReplaceDuration;
		}
	};

	bool InsidePolylines(const TArray<FVekaPolyline>& Polylines, const FVector2D& Point)
	{
		bool bInside = false;
		for (const FVekaPolyline& Polyline : Polylines)
		{
			const TArray<FVector2D>& P = Polyline.Points;
			for (int32 I = 0, J = P.Num() - 1; I < P.Num(); J = I++)
			{
				if ((P[I].Y > Point.Y) != (P[J].Y > Point.Y)
					&& Point.X < (P[J].X - P[I].X) * (Point.Y - P[I].Y) / (P[J].Y - P[I].Y) + P[I].X)
				{
					bInside = !bInside;
				}
			}
		}
		return bInside;
	}

	const TArray<bool>& GetKnockouts(FVekaSymbolState& State, const FVekaSymbolDefinition& Definition)
	{
		if (const TArray<bool>* Found = State.Knockouts.Find(&Definition))
		{
			return *Found;
		}
		TArray<bool> Out;
		Out.SetNumZeroed(Definition.Shapes.Num());
		for (int32 Index = 0; Index < Definition.Shapes.Num(); ++Index)
		{
			if (Definition.Shapes[Index].bFillable || Definition.Shapes[Index].Kind == EVekaSymbolShapeKind::Fill)
			{
				continue;
			}
			FVector2D Centroid = FVector2D::ZeroVector;
			int32 Count = 0;
			for (const FVekaPolyline& Polyline : GetShapeGeometry(Definition, Index))
			{
				for (const FVector2D& Point : Polyline.Points)
				{
					Centroid += Point;
					++Count;
				}
			}
			if (Count == 0)
			{
				continue;
			}
			Centroid /= Count;
			for (int32 Outer = 0; Outer < Index; ++Outer)
			{
				if (Definition.Shapes[Outer].bFillable && InsidePolylines(GetShapeGeometry(Definition, Outer), Centroid))
				{
					Out[Index] = true;
					break;
				}
			}
		}
		return State.Knockouts.Add(&Definition, MoveTemp(Out));
	}

	FVekaPaint WithOpacity(FVekaPaint Paint, float Opacity)
	{
		if (Opacity >= 1.f)
		{
			return Paint;
		}
		Paint.Color.A *= Opacity;
		for (FVekaGradientStop& Stop : Paint.Stops)
		{
			Stop.Color.A *= Opacity;
		}
		return Paint;
	}

	EVekaHierarchy RoleHierarchy(int32 Role)
	{
		return Role <= 0 ? EVekaHierarchy::Primary : Role == 1 ? EVekaHierarchy::Secondary : EVekaHierarchy::Tertiary;
	}

	/** Design grid -> local pixels, with the enclosure inset, the layer effect and the replace scale. */
	struct FPlacement
	{
		FVector2D Origin = FVector2D::ZeroVector;
		double PixelsPerUnit = 1.0;
		double InnerScale = 1.0;
		FVector2D Anchor = GridCenter;
		FVekaSymbolLayerTransform Effect;
		double GlobalScale = 1.0;

		FVector2D Apply(const FVector2D& Point) const
		{
			FVector2D Q = GridCenter + (Point - GridCenter) * InnerScale;
			FVector2D V = (Q - Anchor) * Effect.Scale;
			if (Effect.Rotation != 0.f)
			{
				const double R = FMath::DegreesToRadians(static_cast<double>(Effect.Rotation));
				const double C = FMath::Cos(R);
				const double S = FMath::Sin(R);
				V = FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
			}
			Q = Anchor + V + Effect.Offset;
			Q = GridCenter + (Q - GridCenter) * GlobalScale;
			return Origin + Q * PixelsPerUnit;
		}

		void Apply(const TArray<FVekaPolyline>& In, TArray<FVekaPolyline>& Out) const
		{
			Out.Reset(In.Num());
			for (const FVekaPolyline& Polyline : In)
			{
				FVekaPolyline& Placed = Out.AddDefaulted_GetRef();
				Placed.bClosed = Polyline.bClosed;
				Placed.Points.Reserve(Polyline.Points.Num());
				for (const FVector2D& Point : Polyline.Points)
				{
					Placed.Points.Add(Apply(Point));
				}
			}
		}
	};

	/** Everything a symbol draw needs. */
	struct FSymbolPainter
	{
		FVekaPaintContext& Context;
		const FVekaSymbolStyle& Style;
		FVekaRect Box;
		double PixelsPerUnit = 1.0;
		float StrokePixels = 1.f;
		FVekaPaint Foreground;
		FVekaPaint Knockout;
		int32 NextLayer = 0;
		int32 MaxLayer = 0;
		/** Effect placement per layer (0..2), identity when no effect runs. */
		FVekaSymbolLayerTransform Layers[3];
		float VariableValue = -1.f;
		/** When set, every fill / stroke is kept (tessellation cache). */
		TArray<FCachedOp>* Record = nullptr;

		FSymbolPainter(FVekaPaintContext& InContext, const FVekaSymbolStyle& InStyle) : Context(InContext), Style(InStyle) {}

		FVekaPaint RolePaint(int32 Role, const FVekaSymbolShape* Shape) const
		{
			switch (Style.RenderingMode)
			{
			case EVekaSymbolRenderingMode::Hierarchical:
				return WithOpacity(Foreground, Veka::Colora::HierarchyAlpha(RoleHierarchy(Role)));
			case EVekaSymbolRenderingMode::Palette:
				if (Style.Palette.IsEmpty())
				{
					return WithOpacity(Foreground, Veka::Colora::HierarchyAlpha(RoleHierarchy(Role)));
				}
				return Veka::Colora::Resolve(Style.Palette[FMath::Min(Role, Style.Palette.Num() - 1)], Context.Environment);
			case EVekaSymbolRenderingMode::Multicolor:
				if (Shape && !Shape->ColorToken.IsNone())
				{
					return FVekaPaint(Veka::Colora::ResolveColor(Shape->ColorToken, Context.Environment));
				}
				if (Shape && Shape->Color.A > 0.f)
				{
					return FVekaPaint(Shape->Color);
				}
				return Foreground;
			default:
				return Foreground;
			}
		}

		/** Inner shapes of a filled outline show the background, unless the mode gives them their own color. */
		bool UsesKnockout(const FVekaSymbolShape* Shape) const
		{
			switch (Style.RenderingMode)
			{
			case EVekaSymbolRenderingMode::Palette: return Style.Palette.Num() < 2;
			case EVekaSymbolRenderingMode::Multicolor: return !Shape || (Shape->ColorToken.IsNone() && Shape->Color.A <= 0.f);
			default: return true;
			}
		}

		FVekaPaintContext NewLayer()
		{
			FVekaPaintContext Layer = Context;
			Layer.LayerId = NextLayer++;
			MaxLayer = FMath::Max(MaxLayer, Layer.LayerId);
			return Layer;
		}

		void Fill(const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint)
		{
			if (Paint.IsVisible())
			{
				FVekaPaintContext Layer = NewLayer();
				if (Record)
				{
					FCachedOp& Op = Record->AddDefaulted_GetRef();
					Veka::PaintRender::TessellateFill(Layer, Polylines, Paint, Box, Op.Mesh);
					Op.Paint = Paint;
					Op.LayerOffset = Layer.LayerId - Context.LayerId;
					Veka::PaintRender::DrawTessellation(Layer, Op.Mesh, Paint, Box);
					return;
				}
				Veka::PaintRender::FillPolylines(Layer, Polylines, Paint, Box);
			}
		}

		void Stroke(const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, float Width, float Trim = 1.f)
		{
			if (!Paint.IsVisible() || Trim <= 0.f)
			{
				return;
			}
			FVekaPaintContext Layer = NewLayer();
			TArray<FVekaPolyline> Trimmed;
			if (Trim < 1.f)
			{
				Veka::PathOps::Trim(Polylines, 0.0, Trim, Trimmed);
			}
			const TArray<FVekaPolyline>& Lines = Trim < 1.f ? Trimmed : Polylines;
			if (Record)
			{
				FCachedOp& Op = Record->AddDefaulted_GetRef();
				Veka::PaintRender::TessellateStroke(Layer, Lines, Paint, Width, Box, EVekaLineCap::Round, EVekaLineJoin::Round, 4.f, Op.Mesh);
				Op.Paint = Paint;
				Op.LayerOffset = Layer.LayerId - Context.LayerId;
				Veka::PaintRender::DrawTessellation(Layer, Op.Mesh, Paint, Box);
				return;
			}
			Veka::PaintRender::StrokePolylines(Layer, Lines, Paint, Width, Box, EVekaLineCap::Round, EVekaLineJoin::Round);
		}

		FPlacement MakePlacement(const FVekaSymbolDefinition& Definition, int32 Role, double InnerScale, double GlobalScale) const
		{
			FPlacement Placement;
			Placement.Origin = Box.Position;
			Placement.PixelsPerUnit = PixelsPerUnit;
			Placement.InnerScale = InnerScale;
			Placement.Anchor = Definition.Anchor;
			Placement.Effect = Layers[FMath::Clamp(Role, 0, 2)];
			Placement.GlobalScale = GlobalScale;
			return Placement;
		}

		void Draw(FVekaSymbolState& State, const FVekaSymbolDefinition& Definition, const FVekaSymbolVariant& Variant, double GlobalScale, float GlobalOpacity, float SlashTrim)
		{
			if (GlobalOpacity <= 0.f)
			{
				return;
			}
			const bool bEnclosed = Variant.Enclosure != EVekaSymbolEnclosure::None;
			const double InnerScale = bEnclosed ? 0.62 : 1.0;
			TArray<FVekaPolyline> Placed;

			// Enclosure: stroked, or filled with the symbol knocked out of it.
			if (bEnclosed)
			{
				const bool bMono = Style.RenderingMode == EVekaSymbolRenderingMode::Monochrome || Style.RenderingMode == EVekaSymbolRenderingMode::Multicolor;
				const int32 Role = bMono ? 0 : 1;
				FVekaPath Path;
				if (Variant.Enclosure == EVekaSymbolEnclosure::Circle)
				{
					Path.AddCircle(GridCenter, 10.5);
				}
				else
				{
					Path.AddRoundedRect(FVekaRect(FVector2D(1.5, 1.5), FVector2D(21.0, 21.0)), FVekaCornerRadii(5.f));
				}
				TArray<FVekaPolyline> Outline;
				Path.Flatten(Outline, 0.04);
				const FPlacement Placement = MakePlacement(Definition, Role, 1.0, GlobalScale);
				Placement.Apply(Outline, Placed);
				const FVekaPaint Paint = WithOpacity(RolePaint(Role, nullptr), GlobalOpacity * Placement.Effect.Opacity);
				if (Variant.bFill)
				{
					Fill(Placed, Paint);
				}
				Stroke(Placed, Paint, StrokePixels);
			}

			const TArray<bool>& Knockouts = GetKnockouts(State, Definition);
			const int32 Steps = FMath::Max(Definition.VariableSteps, 1);
			for (int32 Index = 0; Index < Definition.Shapes.Num(); ++Index)
			{
				const FVekaSymbolShape& Shape = Definition.Shapes[Index];
				const int32 Role = static_cast<int32>(Shape.Role);
				const FPlacement Placement = MakePlacement(Definition, Role, InnerScale, GlobalScale);
				float Opacity = GlobalOpacity * Placement.Effect.Opacity;

				if (Shape.VariableIndex >= 0 && Definition.VariableSteps > 0)
				{
					bool bLit = true;
					const float Highlight = Placement.Effect.VariableHighlight >= 0.f ? Placement.Effect.VariableHighlight : Layers[0].VariableHighlight;
					if (Highlight >= 0.f)
					{
						bLit = Shape.VariableIndex < Highlight;
					}
					else if (VariableValue >= 0.f)
					{
						bLit = VariableValue > static_cast<float>(Shape.VariableIndex) / Steps;
					}
					Opacity *= bLit ? 1.f : 0.25f;
				}

				const bool bInsideFill = Variant.bFill && (bEnclosed || Knockouts[Index]);
				const FVekaPaint Paint = WithOpacity(bInsideFill && UsesKnockout(&Shape) ? Knockout : RolePaint(Role, &Shape), Opacity);
				Placement.Apply(GetShapeGeometry(Definition, Index), Placed);

				if (Shape.Kind == EVekaSymbolShapeKind::Fill)
				{
					Fill(Placed, Paint);
					continue;
				}
				if (Variant.bFill && Shape.bFillable && !bEnclosed)
				{
					Fill(Placed, Paint);
				}
				const float Width = StrokePixels * Shape.StrokeScale * (bEnclosed ? 0.9f : 1.f);
				Stroke(Placed, Paint, Width, Placement.Effect.Trim);
			}

			// Slash, with a gap knocked out of what it crosses.
			if (Variant.bSlash && SlashTrim > 0.f)
			{
				FVekaPolyline Line;
				Line.Points = { FVector2D(4.0, 3.5), FVector2D(20.0, 20.5) };
				const TArray<FVekaPolyline> Slash = { Line };
				const FPlacement Placement = MakePlacement(Definition, 0, 1.0, GlobalScale);
				Placement.Apply(Slash, Placed);
				const float Opacity = GlobalOpacity * Placement.Effect.Opacity;
				const float Gap = FMath::Max(1.f, StrokePixels * 0.6f);
				Stroke(Placed, WithOpacity(Knockout, Opacity), StrokePixels + 2.f * Gap, SlashTrim * Placement.Effect.Trim);
				Stroke(Placed, WithOpacity(RolePaint(0, nullptr), Opacity), StrokePixels, SlashTrim * Placement.Effect.Trim);
			}
		}

		void DrawBadge(int32 Badge)
		{
			const double Side = Box.Size.X;
			const FVekaPaint Red = FVekaPaint(Veka::Colora::ResolveColor(TEXT("Red"), Context.Environment));
			TArray<FVekaPolyline> Polylines;
			auto FillPath = [this, &Polylines](const FVekaPath& Path, const FVekaPaint& Paint)
			{
				Polylines.Reset();
				Path.Flatten(Polylines, 0.25);
				Fill(Polylines, Paint);
			};

			if (Badge < 0)
			{
				const double R = Side * 0.17;
				const FVector2D Center(Box.Position.X + Side - R * 0.9, Box.Position.Y + R * 0.9);
				FVekaPath Ring;
				Ring.AddCircle(Center, R + FMath::Max(1.0, Side * 0.05));
				FillPath(Ring, Knockout);
				FVekaPath Dot;
				Dot.AddCircle(Center, R);
				FillPath(Dot, Red);
				return;
			}

			const FString Label = Badge > 99 ? TEXT("99+") : FString::FromInt(Badge);
			FSlateFontInfo Font = VekaEnv::GetFont(Context.Environment);
			Font.Size = FMath::Max(6.f, static_cast<float>(Side * 0.3 * 0.75));
			Font.LetterSpacing = 0;
			const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);
			const double H = Side * 0.46;
			const double W = FMath::Max(H, TextSize.X + H * 0.55);
			const FVekaRect Capsule(FVector2D(Box.Position.X + Side - W + H * 0.25, Box.Position.Y - H * 0.25), FVector2D(W, H));
			const double Gap = FMath::Max(1.0, Side * 0.05);
			FVekaPath Ring;
			Ring.AddRoundedRect(FVekaRect(Capsule.Position - FVector2D(Gap, Gap), Capsule.Size + FVector2D(Gap, Gap) * 2.0), FVekaCornerRadii(static_cast<float>(H * 0.5 + Gap)));
			FillPath(Ring, Knockout);
			FVekaPath Body;
			Body.AddRoundedRect(Capsule, FVekaCornerRadii(static_cast<float>(H * 0.5)));
			FillPath(Body, Red);

			FVekaPaintContext Layer = NewLayer();
			const FVekaRect TextRect(Capsule.Position + (Capsule.Size - TextSize) * 0.5, TextSize);
			FSlateDrawElement::MakeText(Layer.DrawElements, Layer.LayerId, Layer.MakePaintGeometry(TextRect), Label, Font, Layer.DrawEffects, Layer.Tint(FLinearColor::White));
		}
	};
}

TSharedPtr<FVekaPrimitiveState> FVekaSymbolView::CreateState() const
{
	return MakeShared<FVekaSymbolState>();
}

FVector2D FVekaSymbolView::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	const double Side = GetSymbolSize(Context.Environment);
	return FVector2D(Side, Side);
}

int32 FVekaSymbolView::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!Context.State || Bounds.Size.X <= 0.0 || Bounds.Size.Y <= 0.0 || !FSlateApplication::IsInitialized())
	{
		return Context.LayerId;
	}
	FVekaSymbolState& State = static_cast<FVekaSymbolState&>(*Context.State);
	const FVekaSymbolStyle& Style = VekaEnv::GetSymbolStyle(Context.Environment);
	const double Now = FPlatformTime::Seconds();

	FVekaSymbolVariant NameVariant;
	const FVekaSymbolDefinition* Definition = Resolve(Name, NameVariant);
	const FVekaSymbolVariant Effective = Style.Variant.Merged(Variant).Merged(NameVariant);

	// Effect: a new trigger value or activation (re)starts it.
	if (!State.bInitialized)
	{
		State.EffectStart = Style.bEffectActive && Style.Effect.Kind != EVekaSymbolEffectKind::None ? Now : -1.0;
		State.CurrentName = Name;
		State.CurrentVariant = Effective;
		State.bInitialized = true;
	}
	else
	{
		if (Style.Effect.Kind != State.LastKind)
		{
			State.EffectStart = Style.bEffectActive && Style.Effect.Kind != EVekaSymbolEffectKind::None ? Now : -1.0;
		}
		else if (Style.EffectTrigger != State.LastTrigger || (Style.bEffectActive && !State.bLastActive))
		{
			State.EffectStart = Now;
		}
		else if (!Style.bEffectActive && State.bLastActive)
		{
			State.EffectStart = -1.0;
		}

		if (Name != State.CurrentName || Effective != State.CurrentVariant)
		{
			if (Style.Replace != EVekaSymbolReplace::None)
			{
				State.PreviousName = State.CurrentName;
				State.PreviousVariant = State.CurrentVariant;
				State.ReplaceStart = Now;
				State.ReplaceMode = Style.Replace;
				State.ReplaceDuration = FMath::Max(Style.ReplaceDuration, 0.01f);
			}
			State.CurrentName = Name;
			State.CurrentVariant = Effective;
		}
	}
	State.LastKind = Style.Effect.Kind;
	State.LastTrigger = Style.EffectTrigger;
	State.bLastActive = Style.bEffectActive;
	State.Effect = Style.Effect;
	State.bEffectActive = Style.bEffectActive;
	State.VariableSteps = Definition ? FMath::Max(Definition->VariableSteps, 1) : 3;

	// Box: square, centered in the bounds.
	const double Side = FMath::Min(Bounds.Size.X, Bounds.Size.Y);
	FSymbolPainter Painter(Context, Style);
	Painter.Box = FVekaRect(Bounds.Position + (Bounds.Size - FVector2D(Side, Side)) * 0.5, FVector2D(Side, Side));
	Painter.PixelsPerUnit = Side / Grid;
	const float Weight = Style.bOverrideWeight ? Style.Weight : WeightFromFont(Veka::Typography::Resolve(Context.Environment).Weight);
	Painter.StrokePixels = static_cast<float>(StrokeWidth(Weight) * Painter.PixelsPerUnit);
	Painter.Foreground = FVekaPaint(VekaEnv::GetForegroundColor(Context.Environment));
	if (const FVekaPaint* ForegroundPaint = Context.Environment.Find<FVekaPaint>(VekaEnv::ForegroundPaint()))
	{
		Painter.Foreground = *ForegroundPaint;
	}
	Painter.Foreground = Veka::Colora::Resolve(Painter.Foreground, Context.Environment);
	Painter.Knockout = FVekaPaint(Veka::Colora::ResolveColor(TEXT("Background"), Context.Environment));
	Painter.NextLayer = Context.LayerId;
	Painter.MaxLayer = Context.LayerId;
	Painter.VariableValue = VariableValue;
	if (State.EffectStart >= 0.0)
	{
		for (int32 Layer = 0; Layer < 3; ++Layer)
		{
			Painter.Layers[Layer] = EvaluateEffect(Style.Effect, Layer, Now - State.EffectStart, Style.bEffectActive, State.VariableSteps);
		}
	}

	if (!Definition)
	{
		// Unknown name: the placeholder, so the hole is visible.
		FVekaSymbolVariant Ignored;
		if (const FVekaSymbolDefinition* Placeholder = Resolve(TEXT("placeholder"), Ignored))
		{
			Painter.Draw(State, *Placeholder, FVekaSymbolVariant(), 1.0, 0.6f, 0.f);
		}
		return Painter.MaxLayer;
	}

	const double Progress = State.ReplaceStart >= 0.0 ? (Now - State.ReplaceStart) / State.ReplaceDuration : 1.0;
	FVekaSymbolVariant PreviousVariant;
	const FVekaSymbolDefinition* Previous = Progress < 1.0 ? Resolve(State.PreviousName, PreviousVariant) : nullptr;
	if (!Previous)
	{
		State.ReplaceStart = -1.0;
		const FVector2f Scale = Context.Geometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector();
		FSymbolCacheKey Key;
		Key.Definition = Definition;
		Key.Variant = Effective;
		Key.BoxPosition = Painter.Box.Position;
		Key.BoxSize = Painter.Box.Size;
		Key.StrokePixels = Painter.StrokePixels;
		Key.RenderScale = FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y));
		Key.Foreground = Painter.Foreground;
		Key.Knockout = Painter.Knockout;
		Key.Style = Style;
		Key.VariableValue = VariableValue;
		Key.Revision = Veka::Symbols::GetRevision();
		const bool bStatic = State.EffectStart < 0.0;
		if (bStatic && State.bHasCache && State.CachedKey == Key)
		{
			// Same symbol as last frame: draw the kept meshes, no tessellation.
			for (const FCachedOp& Op : State.CachedOps)
			{
				FVekaPaintContext Layer = Context;
				Layer.LayerId = Context.LayerId + Op.LayerOffset;
				Veka::PaintRender::DrawTessellation(Layer, Op.Mesh, Op.Paint, Painter.Box);
			}
			Painter.MaxLayer = Context.LayerId + State.CachedLayers;
			Painter.NextLayer = Painter.MaxLayer + 1;
		}
		else
		{
			State.CachedOps.Reset();
			State.bHasCache = bStatic;
			Painter.Record = bStatic ? &State.CachedOps : nullptr;
			Painter.Draw(State, *Definition, Effective, 1.0, 1.f, 1.f);
			Painter.Record = nullptr;
			State.CachedKey = Key;
			State.CachedLayers = Painter.MaxLayer - Context.LayerId;
		}
	}
	else
	{
		State.bHasCache = false;
		const double P = FMath::Clamp(Progress, 0.0, 1.0);
		const double E = P * P * (3.0 - 2.0 * P);
		const FVekaSymbolVariant& Old = State.PreviousVariant;
		if (State.ReplaceMode == EVekaSymbolReplace::MagicReplace && Previous == Definition)
		{
			if (Old.bFill == Effective.bFill && Old.Enclosure == Effective.Enclosure)
			{
				// Only the slash differs: it draws itself in (or out).
				FVekaSymbolVariant WithSlash = Effective;
				WithSlash.bSlash = true;
				Painter.Draw(State, *Definition, WithSlash, 1.0, 1.f, static_cast<float>(Effective.bSlash ? E : 1.0 - E));
			}
			else
			{
				Painter.Draw(State, *Definition, Old, FMath::Lerp(1.0, 0.85, E), static_cast<float>(1.0 - E), 1.f);
				Painter.Draw(State, *Definition, Effective, FMath::Lerp(0.85, 1.0, E), static_cast<float>(E), 1.f);
			}
		}
		else
		{
			// Down-up: the old one shrinks away, the new one pops in.
			const double Out = FMath::Clamp(P / 0.5, 0.0, 1.0);
			const double In = FMath::Clamp((P - 0.25) / 0.75, 0.0, 1.0);
			const double Back = 1.0 + 2.70158 * FMath::Pow(In - 1.0, 3.0) + 1.70158 * FMath::Pow(In - 1.0, 2.0);
			Painter.Draw(State, *Previous, Old, FMath::Lerp(1.0, 0.4, Out), static_cast<float>(1.0 - Out), 1.f);
			Painter.Draw(State, *Definition, Effective, FMath::Lerp(0.4, 1.0, Back), static_cast<float>(FMath::Clamp(In * 2.0, 0.0, 1.0)), 1.f);
		}
	}

	if (Badge != 0)
	{
		Painter.DrawBadge(Badge);
	}
	return Painter.MaxLayer;
}
