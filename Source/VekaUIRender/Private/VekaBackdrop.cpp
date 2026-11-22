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

DECLARE_GPU_STAT_NAMED(VekaBackdrop, TEXT("Veka Backdrop"));

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
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector4f, ElementRect)
			SHADER_PARAMETER(FVector4f, CaptureRect)
			SHADER_PARAMETER(FVector4f, CornerRadii)
			SHADER_PARAMETER(FVector4f, Tint)
			SHADER_PARAMETER(float, Saturation)
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

	class FVekaBackdropElement : public ICustomSlateElement
	{
	public:
		explicit FVekaBackdropElement(const FVekaBackdropParams& InParams) : Params(InParams) {}

		virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
		{
			FRDGTexture* Output = Inputs.OutputTexture;
			if (!Output)
			{
				return;
			}
			RDG_EVENT_SCOPE_STAT(GraphBuilder, VekaBackdrop, "VekaBackdrop");

			// Element and capture rects, in output pixels.
			const FVector2f A = ToPixel(Params.Min, Inputs);
			const FVector2f B = ToPixel(Params.Max, Inputs);
			const FVector2f ElementMin(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
			const FVector2f ElementMax(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
			const float Scale = (ElementMax.X - ElementMin.X) / FMath::Max(1.f, FMath::Abs(Params.Max.X - Params.Min.X));
			const float Blur = FMath::Max(0.f, Params.BlurRadius * Scale);
			const FIntPoint Extent = Output->Desc.Extent;
			const FIntRect Bounds(FIntPoint::ZeroValue, Extent);
			FIntRect Element(FIntPoint(FMath::FloorToInt(ElementMin.X), FMath::FloorToInt(ElementMin.Y)), FIntPoint(FMath::CeilToInt(ElementMax.X), FMath::CeilToInt(ElementMax.Y)));
			Element.Clip(Bounds);
			if (Element.Width() < 1 || Element.Height() < 1)
			{
				return;
			}
			const int32 Margin = FMath::CeilToInt(Blur * 1.5f);
			FIntRect Capture(Element.Min - FIntPoint(Margin), Element.Max + FIntPoint(Margin));
			Capture.Clip(Bounds);
			const FIntPoint CaptureSize = Capture.Size();

			// Copy of what is under the element.
			const EPixelFormat Format = Output->Desc.Format;
			const ETextureCreateFlags Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
			FRDGTexture* Level = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(CaptureSize, Format, FClearValueBinding::Black, Flags), TEXT("VekaBackdrop.Capture"));
			AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(GMaxRHIFeatureLevel), Output, Level, Capture.Min, FIntPoint::ZeroValue, CaptureSize);

			// Dual Kawase pyramid: down to 1 / 2^Levels, then back up to half resolution.
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			FRHISamplerState* Sampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			const int32 Levels = Blur > 0.f ? Veka::Render::BlurLevels(Blur) : 0;
			TArray<FRDGTexture*, TInlineAllocator<8>> Pyramid = { Level };
			for (int32 Index = 1; Index <= Levels; ++Index)
			{
				const FIntPoint Size(FMath::Max(1, CaptureSize.X >> Index), FMath::Max(1, CaptureSize.Y >> Index));
				FRDGTexture* Down = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Size, Format, FClearValueBinding::Black, Flags), TEXT("VekaBackdrop.Down"));
				AddKawasePass<FVekaKawaseDownPS>(GraphBuilder, ShaderMap, Sampler, Pyramid.Last(), Down, RDG_EVENT_NAME("Down %d", Index));
				Pyramid.Add(Down);
			}
			FRDGTexture* Blurred = Pyramid.Last();
			for (int32 Index = Levels - 1; Index >= 1; --Index)
			{
				const FIntPoint Size = Pyramid[Index]->Desc.Extent;
				FRDGTexture* Up = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Size, Format, FClearValueBinding::Black, Flags), TEXT("VekaBackdrop.Up"));
				AddKawasePass<FVekaKawaseUpPS>(GraphBuilder, ShaderMap, Sampler, Blurred, Up, RDG_EVENT_NAME("Up %d", Index));
				Blurred = Up;
			}

			// Composite inside the rounded rect.
			FVekaBackdropCompositePS::FParameters* Parameters = GraphBuilder.AllocParameters<FVekaBackdropCompositePS::FParameters>();
			Parameters->InputTexture = Blurred;
			Parameters->InputSampler = Sampler;
			Parameters->ElementRect = FVector4f(ElementMin.X, ElementMin.Y, ElementMax.X - ElementMin.X, ElementMax.Y - ElementMin.Y);
			Parameters->CaptureRect = FVector4f(Capture.Min.X, Capture.Min.Y, 1.f / CaptureSize.X, 1.f / CaptureSize.Y);
			Parameters->CornerRadii = Params.CornerRadii * Scale;
			Parameters->Tint = FVector4f(Params.Tint.R, Params.Tint.G, Params.Tint.B, Params.Tint.A);
			Parameters->Saturation = Params.Saturation;
			Parameters->Opacity = Params.Opacity;
			Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
			const TShaderMapRef<FVekaBackdropCompositePS> Composite(ShaderMap);
			const FScreenPassTextureViewport OutputViewport(Output, Element);
			AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Composite"), FScreenPassViewInfo(GMaxRHIFeatureLevel), OutputViewport, OutputViewport,
				TShaderMapRef<FScreenPassVS>(ShaderMap), Composite, TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI(), Parameters);
		}

	private:
		template <typename ShaderType>
		static void AddKawasePass(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRHISamplerState* Sampler, FRDGTexture* Input, FRDGTexture* Target, FRDGEventName&& Name)
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

		FVekaBackdropParams Params;
	};
}

namespace Veka::Render
{
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> MakeBackdropElement(const FVekaBackdropParams& Params)
	{
		// Slate keeps only a weak pointer to a custom drawer: the element lives until the render thread has drawn its
		// frame. Elements of earlier frames are released by a render command, which runs after those frames' Slate
		// drawing (render commands run in order).
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
		TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Element = MakeShared<FVekaBackdropElement, ESPMode::ThreadSafe>(Params);
		Kept.Add({ GFrameCounter, Element });
		return Element;
	}

	int32 BlurLevels(float BlurRadius)
	{
		// Each dual Kawase level roughly doubles the radius covered (about 1.5 px at the first level).
		return FMath::Clamp(FMath::CeilToInt(FMath::Log2(FMath::Max(BlurRadius, 1.f) / 1.5f)), 1, 7);
	}
}
