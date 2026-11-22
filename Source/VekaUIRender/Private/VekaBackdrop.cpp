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
			SHADER_PARAMETER(float, Opacity)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
}

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
		L.bCaptured = false;
		L.Levels.Reset();
		FRDGTexture* Output = Inputs.OutputTexture;
		if (!Output || L.Items.IsEmpty() || L.Quality == EVekaBackdropQuality::Off)
		{
			return;
		}
		RDG_EVENT_SCOPE_STAT(GraphBuilder, VekaBackdrop, "VekaBackdrop.Capture %d", L.Items.Num());

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
		const FVekaBackdropLayer& L = *Layer;
		FRDGTexture* Output = Inputs.OutputTexture;
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

TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> FVekaBackdropLayer::MakeCaptureElement()
{
	if (Items.IsEmpty())
	{
		return nullptr;
	}
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element = MakeShared<FVekaBackdropCaptureElement, ESPMode::ThreadSafe>(AsShared());
	KeepUntilRendered(Element);
	return Element;
}

namespace Veka::Render
{
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
