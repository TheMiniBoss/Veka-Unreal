// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaBackdrop.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderGraphEvent.h"
#include "RenderingThread.h"
#include "HAL/IConsoleManager.h"

DECLARE_GPU_STAT_NAMED(VekaBackdrop, TEXT("Veka Backdrop"));

static TAutoConsoleVariable<int32> CVarVekaBackdropQuality(
	TEXT("r.Veka.Backdrop.Quality"),
	3,
	TEXT("Quality of Veka backdrops (backdropBlur, frostedGlass, glass):\n")
	TEXT(" 0: off (a tinted fill instead of the blur)\n")
	TEXT(" 1: low (pyramid levels sampled directly, up to 4 levels)\n")
	TEXT(" 2: medium (up to 5 levels)\n")
	TEXT(" 3: high (default, up to 6 levels)\n")
	TEXT(" 4: epic (up to 7 levels)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

namespace
{
	BEGIN_SHADER_PARAMETER_STRUCT(FVekaKawaseParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FVector2f, InputTexelSize)
		SHADER_PARAMETER(FVector2f, OutputTexelSize)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FVekaKawaseDownPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FVekaKawaseDownPS);
		SHADER_USE_PARAMETER_STRUCT(FVekaKawaseDownPS, FGlobalShader);
		using FParameters = FVekaKawaseParameters;
	};

	class FVekaKawaseUpPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FVekaKawaseUpPS);
		SHADER_USE_PARAMETER_STRUCT(FVekaKawaseUpPS, FGlobalShader);
		using FParameters = FVekaKawaseParameters;
	};

	class FVekaBackdropCompositePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FVekaBackdropCompositePS);
		SHADER_USE_PARAMETER_STRUCT(FVekaBackdropCompositePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LowerTexture)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, UpperTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(float, LevelMix)
			SHADER_PARAMETER(FVector4f, ElementRect)
			SHADER_PARAMETER(FVector4f, CaptureRect)
			SHADER_PARAMETER(FVector4f, CornerRadii)
			SHADER_PARAMETER(FVector4f, Tint)
			SHADER_PARAMETER(float, Saturation)
			SHADER_PARAMETER(float, Noise)
			SHADER_PARAMETER(FVector4f, Distortion)
			SHADER_PARAMETER(float, Opacity)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	class FVekaFillPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FVekaFillPS);
		SHADER_USE_PARAMETER_STRUCT(FVekaFillPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector4f, FillColor)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	class FVekaBlendPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FVekaBlendPS);
		SHADER_USE_PARAMETER_STRUCT(FVekaBlendPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BehindTexture)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OverBlackTexture)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OverWhiteTexture)
			SHADER_PARAMETER(FIntPoint, RegionMin)
			SHADER_PARAMETER(int32, Mode)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
}

IMPLEMENT_GLOBAL_SHADER(FVekaFillPS, "/Plugin/VekaUI/Private/VekaBackdrop.usf", "FillPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVekaBlendPS, "/Plugin/VekaUI/Private/VekaBackdrop.usf", "BlendPS", SF_Pixel);

namespace
{
	class FVekaGlassPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FVekaGlassPS);
		SHADER_USE_PARAMETER_STRUCT(FVekaGlassPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LowerTexture)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, UpperTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(float, LevelMix)
			SHADER_PARAMETER(FVector4f, CaptureRect)
			SHADER_PARAMETER_ARRAY(FVector4f, ShapeRects, [FVekaGlassParams::MaxShapes])
			SHADER_PARAMETER_ARRAY(FVector4f, ShapeRadii, [FVekaGlassParams::MaxShapes])
			SHADER_PARAMETER(int32, NumShapes)
			SHADER_PARAMETER(float, Smoothness)
			SHADER_PARAMETER(float, Refraction)
			SHADER_PARAMETER(float, Bezel)
			SHADER_PARAMETER(float, Aberration)
			SHADER_PARAMETER(float, Highlight)
			SHADER_PARAMETER(FVector2f, LightPosition)
			SHADER_PARAMETER(FVector4f, Tint)
			SHADER_PARAMETER(float, Saturation)
			SHADER_PARAMETER(float, Opacity)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
}

IMPLEMENT_GLOBAL_SHADER(FVekaGlassPS, "/Plugin/VekaUI/Private/VekaBackdrop.usf", "GlassPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVekaKawaseDownPS, "/Plugin/VekaUI/Private/VekaBackdrop.usf", "KawaseDownPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVekaKawaseUpPS, "/Plugin/VekaUI/Private/VekaBackdrop.usf", "KawaseUpPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVekaBackdropCompositePS, "/Plugin/VekaUI/Private/VekaBackdrop.usf", "CompositePS", SF_Pixel);

namespace
{
	/** Slate absolute position -> pixel of the output texture (the elements matrix covers the whole texture). */
	FVector2f ToPixel(const FVector2f& Position, const ICustomSlateElement::FDrawPassInputs& Inputs)
	{
		const FVector4f Clip = Inputs.ElementsMatrix.TransformFVector4(FVector4f(Position.X + Inputs.ElementsOffset.X, Position.Y + Inputs.ElementsOffset.Y, 0.f, 1.f));
		const float W = FMath::IsNearlyZero(Clip.W) ? 1.f : Clip.W;
		const FVector2f Ndc(Clip.X / W, Clip.Y / W);
		const FIntPoint Extent = Inputs.OutputTexture->Desc.Extent;
		return FVector2f((Ndc.X * 0.5f + 0.5f) * Extent.X, (0.5f - Ndc.Y * 0.5f) * Extent.Y);
	}

	/** A backdrop's rect in output pixels and the pixels per Slate unit. */
	struct FPixelRect
	{
		FVector2f Min;
		FVector2f Max;
		float Scale = 1.f;
	};

	FPixelRect ToPixels(const FVekaBackdropParams& Params, const ICustomSlateElement::FDrawPassInputs& Inputs)
	{
		const FVector2f A = ToPixel(Params.Min, Inputs);
		const FVector2f B = ToPixel(Params.Max, Inputs);
		FPixelRect Out;
		Out.Min = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		Out.Max = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		Out.Scale = (Out.Max.X - Out.Min.X) / FMath::Max(1.f, FMath::Abs(Params.Max.X - Params.Min.X));
		return Out;
	}

	template <typename ShaderType>
	void AddKawasePass(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRHISamplerState* Sampler, FRDGTexture* Input, FRDGTexture* Target, FRDGEventName&& Name)
	{
		typename ShaderType::FParameters* Parameters = GraphBuilder.AllocParameters<typename ShaderType::FParameters>();
		Parameters->InputTexture = Input;
		Parameters->InputSampler = Sampler;
		Parameters->InputTexelSize = FVector2f(1.f / Input->Desc.Extent.X, 1.f / Input->Desc.Extent.Y);
		Parameters->OutputTexelSize = FVector2f(1.f / Target->Desc.Extent.X, 1.f / Target->Desc.Extent.Y);
		Parameters->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ENoAction);
		const FScreenPassTextureViewport Viewport(Target);
		AddDrawScreenPass(GraphBuilder, MoveTemp(Name), FScreenPassViewInfo(GMaxRHIFeatureLevel), Viewport, FScreenPassTextureViewport(Input),
			TShaderMapRef<ShaderType>(ShaderMap), Parameters);
	}

	/**
	 * Slate keeps only a weak pointer to a custom drawer: an element lives until the render thread has drawn its frame.
	 * Elements of earlier frames are released by a render command, which runs after those frames' Slate drawing
	 * (render commands run in order).
	 */
	void KeepUntilRendered(const TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe>& Element)
	{
		check(IsInGameThread());
		struct FKept
		{
			uint64 Frame = 0;
			TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element;
		};
		static TArray<FKept> Kept;
		TArray<TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe>> Released;
		for (int32 Index = Kept.Num() - 1; Index >= 0; --Index)
		{
			if (Kept[Index].Frame < GFrameCounter)
			{
				Released.Add(MoveTemp(Kept[Index].Element));
				Kept.RemoveAtSwap(Index);
			}
		}
		if (!Released.IsEmpty())
		{
			ENQUEUE_RENDER_COMMAND(VekaReleaseBackdrops)([Released = MoveTemp(Released)](FRHICommandListImmediate&) {});
		}
		Kept.Add({ GFrameCounter, Element });
	}
}

/** Copies the union of the layer's rects once and builds the blur levels its backdrops need. */
class FVekaBackdropCaptureElement : public ICustomSlateElement
{
public:
	explicit FVekaBackdropCaptureElement(const TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe>& InLayer) : Layer(InLayer) {}

	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
	{
		FVekaBackdropLayer& L = *Layer;
		L.bCaptureTried = true;
		L.bCaptured = false;
		L.Levels.Reset();
		FRDGTexture* Output = Inputs.OutputTexture;
		if (!Output || L.IsEmpty() || L.Quality == EVekaBackdropQuality::Off)
		{
			return;
		}
		RDG_EVENT_SCOPE_STAT(GraphBuilder, VekaBackdrop, "VekaBackdrop.Capture %d", L.Num());

		// Union of the rects, each with a margin for its blur, and the levels needed.
		const FIntPoint Extent = Output->Desc.Extent;
		const FIntRect Bounds(FIntPoint::ZeroValue, Extent);
		const int32 MaxLevels = Veka::Render::MaxBlurLevels(L.Quality);
		FIntRect Capture;
		bool bAny = false;
		TBitArray<> NeededLevels(false, MaxLevels + 1);
		for (const FVekaBackdropParams& Item : L.Items)
		{
			const FPixelRect Pixels = ToPixels(Item, Inputs);
			const float Blur = FMath::Max(0.f, Item.BlurRadius * Pixels.Scale);
			const int32 Margin = FMath::CeilToInt(Blur * 1.5f) + 1;
			const FIntRect Rect(FIntPoint(FMath::FloorToInt(Pixels.Min.X) - Margin, FMath::FloorToInt(Pixels.Min.Y) - Margin),
				FIntPoint(FMath::CeilToInt(Pixels.Max.X) + Margin, FMath::CeilToInt(Pixels.Max.Y) + Margin));
			if (bAny)
			{
				Capture.Union(Rect);
			}
			else
			{
				Capture = Rect;
			}
			bAny = true;
			const float Level = FMath::Min(Veka::Render::BlurLevel(Blur), static_cast<float>(MaxLevels));
			NeededLevels[FMath::FloorToInt(Level)] = true;
			NeededLevels[FMath::Min(FMath::CeilToInt(Level), MaxLevels)] = true;
			L.PixelScale = Pixels.Scale;
		}
		for (const TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe>& Glass : L.Glasses)
		{
			for (const FVekaGlassShape& Shape : Glass->Shapes)
			{
				FVekaBackdropParams AsBackdrop;
				AsBackdrop.Min = Shape.Min;
				AsBackdrop.Max = Shape.Max;
				const FPixelRect Pixels = ToPixels(AsBackdrop, Inputs);
				const float Blur = FMath::Max(0.f, Glass->BlurRadius * Pixels.Scale);
				// Room for the blur and for the refraction, which samples outside of the shape.
				const int32 Margin = FMath::CeilToInt(Blur * 1.5f + (Glass->Refraction * (1.f + Glass->Aberration) + Glass->Smoothness) * Pixels.Scale) + 2;
				const FIntRect Rect(FIntPoint(FMath::FloorToInt(Pixels.Min.X) - Margin, FMath::FloorToInt(Pixels.Min.Y) - Margin),
					FIntPoint(FMath::CeilToInt(Pixels.Max.X) + Margin, FMath::CeilToInt(Pixels.Max.Y) + Margin));
				if (bAny)
				{
					Capture.Union(Rect);
				}
				else
				{
					Capture = Rect;
				}
				bAny = true;
				const float Level = FMath::Min(Veka::Render::BlurLevel(Blur), static_cast<float>(MaxLevels));
				NeededLevels[FMath::FloorToInt(Level)] = true;
				NeededLevels[FMath::Min(FMath::CeilToInt(Level), MaxLevels)] = true;
			}
		}
		if (!bAny)
		{
			return;
		}
		Capture.Clip(Bounds);
		const FIntPoint CaptureSize = Capture.Size();
		if (CaptureSize.X < 2 || CaptureSize.Y < 2)
		{
			return;
		}
		int32 TopLevel = 0;
		for (int32 Level = 0; Level <= MaxLevels; ++Level)
		{
			TopLevel = NeededLevels[Level] ? Level : TopLevel;
		}

		const EPixelFormat Format = Output->Desc.Format;
		const ETextureCreateFlags Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
		auto Create = [&](const FIntPoint& Size, const TCHAR* Name)
		{
			return GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Size, Format, FClearValueBinding::Black, Flags), Name);
		};
		FRDGTexture* Copy = Create(CaptureSize, TEXT("VekaBackdrop.Copy"));
		AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(GMaxRHIFeatureLevel), Output, Copy, Capture.Min, FIntPoint::ZeroValue, CaptureSize);

		// Dual Kawase: down to the top level; each needed level k is then brought back up to half resolution
		// (k - 1 upsamples), except in Low quality where the downsampled level is used as is.
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		FRHISamplerState* Sampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		TArray<FRDGTexture*, TInlineAllocator<8>> Down = { Copy };
		for (int32 Index = 1; Index <= TopLevel; ++Index)
		{
			const FIntPoint Size(FMath::Max(1, CaptureSize.X >> Index), FMath::Max(1, CaptureSize.Y >> Index));
			FRDGTexture* Next = Create(Size, TEXT("VekaBackdrop.Down"));
			AddKawasePass<FVekaKawaseDownPS>(GraphBuilder, ShaderMap, Sampler, Down.Last(), Next, RDG_EVENT_NAME("Down %d", Index));
			Down.Add(Next);
		}
		L.Levels.SetNum(TopLevel + 1);
		for (int32 Level = 0; Level <= TopLevel; ++Level)
		{
			if (!NeededLevels[Level])
			{
				continue;
			}
			FRDGTexture* Blurred = Down[Level];
			if (L.Quality != EVekaBackdropQuality::Low)
			{
				for (int32 Index = Level - 1; Index >= 1; --Index)
				{
					FRDGTexture* Up = Create(Down[Index]->Desc.Extent, TEXT("VekaBackdrop.Up"));
					AddKawasePass<FVekaKawaseUpPS>(GraphBuilder, ShaderMap, Sampler, Blurred, Up, RDG_EVENT_NAME("Up %d to %d", Level, Index));
					Blurred = Up;
				}
			}
			L.Levels[Level] = GraphBuilder.ConvertToExternalTexture(Blurred);
		}
		L.CaptureRect = Capture;
		L.bCaptured = true;
	}

private:
	TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe> Layer;
};

/** One backdrop of a layer: its two blur levels, mixed, inside its rounded rect. */
class FVekaBackdropCompositeElement : public ICustomSlateElement
{
public:
	FVekaBackdropCompositeElement(const TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe>& InLayer, int32 InIndex) : Layer(InLayer), Index(InIndex) {}

	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
	{
		FRDGTexture* Output = Inputs.OutputTexture;
		if (Output && !Layer->bCaptureTried)
		{
			FVekaBackdropCaptureElement(Layer).Draw_RenderThread(GraphBuilder, Inputs);
		}
		const FVekaBackdropLayer& L = *Layer;
		if (!Output || !L.bCaptured || !L.Items.IsValidIndex(Index))
		{
			return;
		}
		const FVekaBackdropParams& Params = L.Items[Index];
		RDG_EVENT_SCOPE_STAT(GraphBuilder, VekaBackdrop, "VekaBackdrop.Composite");

		const FPixelRect Pixels = ToPixels(Params, Inputs);
		FIntRect Element(FIntPoint(FMath::FloorToInt(Pixels.Min.X), FMath::FloorToInt(Pixels.Min.Y)), FIntPoint(FMath::CeilToInt(Pixels.Max.X), FMath::CeilToInt(Pixels.Max.Y)));
		Element.Clip(FIntRect(FIntPoint::ZeroValue, Output->Desc.Extent));
		if (Element.Width() < 1 || Element.Height() < 1)
		{
			return;
		}
		const int32 TopLevel = L.Levels.Num() - 1;
		const float Level = FMath::Clamp(Veka::Render::BlurLevel(FMath::Max(0.f, Params.BlurRadius * Pixels.Scale)), 0.f, static_cast<float>(TopLevel));
		const int32 Lower = FMath::FloorToInt(Level);
		const int32 Upper = FMath::Min(FMath::CeilToInt(Level), TopLevel);
		if (!L.Levels.IsValidIndex(Lower) || !L.Levels[Lower].IsValid() || !L.Levels[Upper].IsValid())
		{
			return;
		}

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		FVekaBackdropCompositePS::FParameters* Parameters = GraphBuilder.AllocParameters<FVekaBackdropCompositePS::FParameters>();
		Parameters->LowerTexture = GraphBuilder.RegisterExternalTexture(L.Levels[Lower]);
		Parameters->UpperTexture = GraphBuilder.RegisterExternalTexture(L.Levels[Upper]);
		Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->LevelMix = Level - Lower;
		Parameters->ElementRect = FVector4f(Pixels.Min.X, Pixels.Min.Y, Pixels.Max.X - Pixels.Min.X, Pixels.Max.Y - Pixels.Min.Y);
		const FIntPoint CaptureSize = L.CaptureRect.Size();
		Parameters->CaptureRect = FVector4f(L.CaptureRect.Min.X, L.CaptureRect.Min.Y, 1.f / CaptureSize.X, 1.f / CaptureSize.Y);
		Parameters->CornerRadii = Params.CornerRadii * Pixels.Scale;
		Parameters->Tint = FVector4f(Params.Tint.R, Params.Tint.G, Params.Tint.B, Params.Tint.A);
		Parameters->Saturation = Params.Saturation;
		Parameters->Noise = Params.Noise;
		Parameters->Distortion = FVector4f(Params.DistortionStrength * Pixels.Scale, 1.f / FMath::Max(1.f, Params.DistortionScale * Pixels.Scale), Params.DistortionTime, 0.f);
		Parameters->Opacity = Params.Opacity;
		Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
		const FScreenPassTextureViewport OutputViewport(Output, Element);
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Composite"), FScreenPassViewInfo(GMaxRHIFeatureLevel), OutputViewport, OutputViewport,
			TShaderMapRef<FScreenPassVS>(ShaderMap), TShaderMapRef<FVekaBackdropCompositePS>(ShaderMap),
			TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI(), Parameters);
	}

private:
	TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe> Layer;
	int32 Index;
};

/** A glass of a layer: its shapes (merged), refracting the layer capture, with the rim and the tint. */
class FVekaGlassCompositeElement : public ICustomSlateElement
{
public:
	FVekaGlassCompositeElement(const TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe>& InLayer, const TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe>& InGlass)
		: Layer(InLayer), Glass(InGlass) {}

	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
	{
		FRDGTexture* Output = Inputs.OutputTexture;
		if (Output && !Layer->bCaptureTried)
		{
			FVekaBackdropCaptureElement(Layer).Draw_RenderThread(GraphBuilder, Inputs);
		}
		const FVekaBackdropLayer& L = *Layer;
		const FVekaGlassParams& G = *Glass;
		if (!Output || !L.bCaptured || G.Shapes.IsEmpty())
		{
			return;
		}
		RDG_EVENT_SCOPE_STAT(GraphBuilder, VekaBackdrop, "VekaGlass %d", G.Shapes.Num());

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		FVekaGlassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FVekaGlassPS::FParameters>();
		float Scale = 1.f;
		FVector2f BoundsMin(TNumericLimits<float>::Max());
		FVector2f BoundsMax(-TNumericLimits<float>::Max());
		const int32 Count = FMath::Min(G.Shapes.Num(), FVekaGlassParams::MaxShapes);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FVekaBackdropParams AsBackdrop;
			AsBackdrop.Min = G.Shapes[Index].Min;
			AsBackdrop.Max = G.Shapes[Index].Max;
			const FPixelRect Pixels = ToPixels(AsBackdrop, Inputs);
			Scale = Pixels.Scale;
			Parameters->ShapeRects[Index] = FVector4f(Pixels.Min.X, Pixels.Min.Y, Pixels.Max.X, Pixels.Max.Y);
			Parameters->ShapeRadii[Index] = G.Shapes[Index].CornerRadii * Pixels.Scale;
			BoundsMin = FVector2f(FMath::Min(BoundsMin.X, Pixels.Min.X), FMath::Min(BoundsMin.Y, Pixels.Min.Y));
			BoundsMax = FVector2f(FMath::Max(BoundsMax.X, Pixels.Max.X), FMath::Max(BoundsMax.Y, Pixels.Max.Y));
		}
		const int32 Grow = 2;
		FIntRect Region(FIntPoint(FMath::FloorToInt(BoundsMin.X) - Grow, FMath::FloorToInt(BoundsMin.Y) - Grow),
			FIntPoint(FMath::CeilToInt(BoundsMax.X) + Grow, FMath::CeilToInt(BoundsMax.Y) + Grow));
		Region.Clip(FIntRect(FIntPoint::ZeroValue, Output->Desc.Extent));
		if (Region.Width() < 1 || Region.Height() < 1)
		{
			return;
		}
		const int32 TopLevel = L.Levels.Num() - 1;
		const float Level = FMath::Clamp(Veka::Render::BlurLevel(FMath::Max(0.f, G.BlurRadius * Scale)), 0.f, static_cast<float>(TopLevel));
		const int32 Lower = FMath::FloorToInt(Level);
		const int32 Upper = FMath::Min(FMath::CeilToInt(Level), TopLevel);
		if (!L.Levels.IsValidIndex(Lower) || !L.Levels[Lower].IsValid() || !L.Levels[Upper].IsValid())
		{
			return;
		}
		Parameters->LowerTexture = GraphBuilder.RegisterExternalTexture(L.Levels[Lower]);
		Parameters->UpperTexture = GraphBuilder.RegisterExternalTexture(L.Levels[Upper]);
		Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->LevelMix = Level - Lower;
		const FIntPoint CaptureSize = L.CaptureRect.Size();
		Parameters->CaptureRect = FVector4f(L.CaptureRect.Min.X, L.CaptureRect.Min.Y, 1.f / CaptureSize.X, 1.f / CaptureSize.Y);
		Parameters->NumShapes = Count;
		Parameters->Smoothness = G.Smoothness * Scale;
		Parameters->Refraction = G.Refraction * Scale;
		Parameters->Bezel = FMath::Max(1.f, G.Bezel * Scale);
		Parameters->Aberration = G.Aberration;
		Parameters->Highlight = G.Highlight;
		Parameters->LightPosition = ToPixel(G.LightPosition, Inputs);
		Parameters->Tint = FVector4f(G.Tint.R, G.Tint.G, G.Tint.B, G.Tint.A);
		Parameters->Saturation = G.Saturation;
		Parameters->Opacity = G.Opacity;
		Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
		const FScreenPassTextureViewport OutputViewport(Output, Region);
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Glass"), FScreenPassViewInfo(GMaxRHIFeatureLevel), OutputViewport, OutputViewport,
			TShaderMapRef<FScreenPassVS>(ShaderMap), TShaderMapRef<FVekaGlassPS>(ShaderMap),
			TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI(), Parameters);
	}

private:
	TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe> Layer;
	TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe> Glass;
};

namespace
{
	/** A backdrop outside of a layer: its own capture, then its composite. */
	class FVekaStandaloneBackdropElement : public ICustomSlateElement
	{
	public:
		explicit FVekaStandaloneBackdropElement(const TSharedRef<FVekaBackdropLayer, ESPMode::ThreadSafe>& Layer) : Capture(Layer), Composite(Layer, 0) {}

		virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
		{
			Capture.Draw_RenderThread(GraphBuilder, Inputs);
			Composite.Draw_RenderThread(GraphBuilder, Inputs);
		}

	private:
		FVekaBackdropCaptureElement Capture;
		FVekaBackdropCompositeElement Composite;
	};
}

TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> FVekaBackdropLayer::Add(const FVekaBackdropParams& Params)
{
	check(IsInGameThread());
	Items.Add(Params);
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element = MakeShared<FVekaBackdropCompositeElement, ESPMode::ThreadSafe>(AsShared(), Items.Num() - 1);
	KeepUntilRendered(Element);
	return Element;
}

TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> FVekaBackdropLayer::AddGlass(const TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe>& Params)
{
	check(IsInGameThread());
	Glasses.Add(Params);
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element = MakeShared<FVekaGlassCompositeElement, ESPMode::ThreadSafe>(AsShared(), Params);
	KeepUntilRendered(Element);
	return Element;
}

TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> FVekaBackdropLayer::MakeCaptureElement()
{
	if (IsEmpty())
	{
		return nullptr;
	}
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element = MakeShared<FVekaBackdropCaptureElement, ESPMode::ThreadSafe>(AsShared());
	KeepUntilRendered(Element);
	return Element;
}

namespace
{
	/** What a blend keeps between its elements (render thread). */
	struct FVekaBlendState
	{
		FVekaBlendParams Params;
		FIntRect Region;
		TRefCountPtr<IPooledRenderTarget> Behind;
		TRefCountPtr<IPooledRenderTarget> OverBlack;
	};

	FIntRect BlendRegion(const FVekaBlendParams& Params, const ICustomSlateElement::FDrawPassInputs& Inputs)
	{
		FVekaBackdropParams AsBackdrop;
		AsBackdrop.Min = Params.Min;
		AsBackdrop.Max = Params.Max;
		const FPixelRect Pixels = ToPixels(AsBackdrop, Inputs);
		FIntRect Region(FIntPoint(FMath::FloorToInt(Pixels.Min.X), FMath::FloorToInt(Pixels.Min.Y)), FIntPoint(FMath::CeilToInt(Pixels.Max.X), FMath::CeilToInt(Pixels.Max.Y)));
		Region.Clip(FIntRect(FIntPoint::ZeroValue, Inputs.OutputTexture->Desc.Extent));
		return Region;
	}

	FRDGTexture* CopyRegion(FRDGBuilder& GraphBuilder, FRDGTexture* Output, const FIntRect& Region, const TCHAR* Name)
	{
		FRDGTexture* Copy = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Region.Size(), Output->Desc.Format, FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable), Name);
		AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(GMaxRHIFeatureLevel), Output, Copy, Region.Min, FIntPoint::ZeroValue, Region.Size());
		return Copy;
	}

	void FillRegion(FRDGBuilder& GraphBuilder, FRDGTexture* Output, const FIntRect& Region, const FLinearColor& Color)
	{
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		FVekaFillPS::FParameters* Parameters = GraphBuilder.AllocParameters<FVekaFillPS::FParameters>();
		Parameters->FillColor = FVector4f(Color.R, Color.G, Color.B, Color.A);
		Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
		const FScreenPassTextureViewport Viewport(Output, Region);
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Fill"), FScreenPassViewInfo(GMaxRHIFeatureLevel), Viewport, Viewport,
			TShaderMapRef<FScreenPassVS>(ShaderMap), TShaderMapRef<FVekaFillPS>(ShaderMap), TStaticBlendState<>::GetRHI(), Parameters);
	}

	enum class EBlendStep : uint8 { Begin, Between, End };

	/** One step of a blend: Begin keeps what is behind and clears to black; Between keeps the content over black and clears to white; End composites. */
	class FVekaBlendElement : public ICustomSlateElement
	{
	public:
		FVekaBlendElement(const TSharedRef<FVekaBlendState, ESPMode::ThreadSafe>& InState, EBlendStep InStep) : State(InState), Step(InStep) {}

		virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
		{
			FRDGTexture* Output = Inputs.OutputTexture;
			FVekaBlendState& S = *State;
			if (!Output)
			{
				return;
			}
			RDG_EVENT_SCOPE_STAT(GraphBuilder, VekaBackdrop, "VekaBlend %d", static_cast<int32>(Step));
			if (Step == EBlendStep::Begin)
			{
				S.Region = BlendRegion(S.Params, Inputs);
				S.Behind = nullptr;
				S.OverBlack = nullptr;
				if (S.Region.Width() < 1 || S.Region.Height() < 1)
				{
					return;
				}
				S.Behind = GraphBuilder.ConvertToExternalTexture(CopyRegion(GraphBuilder, Output, S.Region, TEXT("VekaBlend.Behind")));
				FillRegion(GraphBuilder, Output, S.Region, FLinearColor::Black);
				return;
			}
			if (!S.Behind.IsValid())
			{
				return;
			}
			if (Step == EBlendStep::Between)
			{
				S.OverBlack = GraphBuilder.ConvertToExternalTexture(CopyRegion(GraphBuilder, Output, S.Region, TEXT("VekaBlend.OverBlack")));
				FillRegion(GraphBuilder, Output, S.Region, FLinearColor::White);
				return;
			}
			if (!S.OverBlack.IsValid())
			{
				return;
			}
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			FVekaBlendPS::FParameters* Parameters = GraphBuilder.AllocParameters<FVekaBlendPS::FParameters>();
			Parameters->BehindTexture = GraphBuilder.RegisterExternalTexture(S.Behind);
			Parameters->OverBlackTexture = GraphBuilder.RegisterExternalTexture(S.OverBlack);
			Parameters->OverWhiteTexture = CopyRegion(GraphBuilder, Output, S.Region, TEXT("VekaBlend.OverWhite"));
			Parameters->RegionMin = S.Region.Min;
			Parameters->Mode = S.Params.Mode;
			Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
			const FScreenPassTextureViewport Viewport(Output, S.Region);
			AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Blend"), FScreenPassViewInfo(GMaxRHIFeatureLevel), Viewport, Viewport,
				TShaderMapRef<FScreenPassVS>(ShaderMap), TShaderMapRef<FVekaBlendPS>(ShaderMap), TStaticBlendState<>::GetRHI(), Parameters);
		}

	private:
		TSharedRef<FVekaBlendState, ESPMode::ThreadSafe> State;
		EBlendStep Step;
	};
}

namespace Veka::Render
{
	FVekaBlendElements MakeBlendElements(const FVekaBlendParams& Params)
	{
		const TSharedRef<FVekaBlendState, ESPMode::ThreadSafe> State = MakeShared<FVekaBlendState, ESPMode::ThreadSafe>();
		State->Params = Params;
		FVekaBlendElements Out;
		Out.Begin = MakeShared<FVekaBlendElement, ESPMode::ThreadSafe>(State, EBlendStep::Begin);
		Out.Between = MakeShared<FVekaBlendElement, ESPMode::ThreadSafe>(State, EBlendStep::Between);
		Out.End = MakeShared<FVekaBlendElement, ESPMode::ThreadSafe>(State, EBlendStep::End);
		KeepUntilRendered(Out.Begin);
		KeepUntilRendered(Out.Between);
		KeepUntilRendered(Out.End);
		return Out;
	}

	TSharedRef<FVekaBackdropLayer> MakeBackdropLayer()
	{
		TSharedRef<FVekaBackdropLayer> Layer = MakeShared<FVekaBackdropLayer, ESPMode::ThreadSafe>();
		Layer->SetQuality(GetBackdropQuality());
		return Layer;
	}

	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> MakeBackdropElement(const FVekaBackdropParams& Params)
	{
		const TSharedRef<FVekaBackdropLayer> Layer = MakeBackdropLayer();
		Layer->AddItem(Params);
		TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element = MakeShared<FVekaStandaloneBackdropElement, ESPMode::ThreadSafe>(Layer);
		KeepUntilRendered(Element);
		return Element;
	}

	float BlurLevel(float BlurRadius)
	{
		// Level 1 covers about 1.5 px, and each level doubles it: radius = 1.5 * (2^level - 1).
		return BlurRadius <= 0.f ? 0.f : FMath::Log2(1.f + BlurRadius / 1.5f);
	}

	int32 BlurLevels(float BlurRadius)
	{
		return FMath::Clamp(FMath::CeilToInt(BlurLevel(BlurRadius)), 0, MaxBlurLevels(EVekaBackdropQuality::Epic));
	}

	int32 MaxBlurLevels(EVekaBackdropQuality Quality)
	{
		switch (Quality)
		{
		case EVekaBackdropQuality::Off: return 0;
		case EVekaBackdropQuality::Low: return 4;
		case EVekaBackdropQuality::Medium: return 5;
		case EVekaBackdropQuality::High: return 6;
		default: return 7;
		}
	}

	EVekaBackdropQuality GetBackdropQuality()
	{
		return static_cast<EVekaBackdropQuality>(FMath::Clamp(CVarVekaBackdropQuality.GetValueOnAnyThread(), 0, 4));
	}
}
