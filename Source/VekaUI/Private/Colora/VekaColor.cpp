// Copyright TheMiniBoss. All Rights Reserved.

#include "Colora/VekaColor.h"

namespace
{
	constexpr float HueEpsilon = 1.0e-4f;

	/** Rec. 709 luma weights (on gamma-encoded values, as CSS filters do). */
	constexpr float LumR = 0.2126f;
	constexpr float LumG = 0.7152f;
	constexpr float LumB = 0.0722f;
}

// --- Matrix ------------------------------------------------------------------------------------------------

FVekaColorMatrix::FVekaColorMatrix()
{
	FMemory::Memzero(M);
	M[0][0] = M[1][1] = M[2][2] = M[3][3] = 1.f;
}

FVekaColorMatrix FVekaColorMatrix::Make(EVekaColorAdjust Adjust, float Amount)
{
	FVekaColorMatrix Out;
	switch (Adjust)
	{
	case EVekaColorAdjust::Brightness:
		Out.M[0][4] = Out.M[1][4] = Out.M[2][4] = Amount;
		break;

	case EVekaColorAdjust::Contrast:
		for (int32 Row = 0; Row < 3; ++Row)
		{
			Out.M[Row][Row] = Amount;
			Out.M[Row][4] = 0.5f * (1.f - Amount);
		}
		break;

	case EVekaColorAdjust::Saturation:
	case EVekaColorAdjust::Grayscale:
	{
		// Grayscale(g) = Saturation(1 - g).
		const float S = Adjust == EVekaColorAdjust::Saturation ? Amount : 1.f - FMath::Clamp(Amount, 0.f, 1.f);
		const float Lum[3] = { LumR, LumG, LumB };
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				Out.M[Row][Col] = (1.f - S) * Lum[Col] + (Row == Col ? S : 0.f);
			}
		}
		break;
	}

	case EVekaColorAdjust::HueRotation:
	{
		// Same matrix as CSS hue-rotate / SVG feColorMatrix type="hueRotate".
		const float Radians = FMath::DegreesToRadians(Amount);
		const float C = FMath::Cos(Radians);
		const float S = FMath::Sin(Radians);
		const float Values[3][3] = {
			{ 0.213f + C * 0.787f - S * 0.213f, 0.715f - C * 0.715f - S * 0.715f, 0.072f - C * 0.072f + S * 0.928f },
			{ 0.213f - C * 0.213f + S * 0.143f, 0.715f + C * 0.285f + S * 0.140f, 0.072f - C * 0.072f - S * 0.283f },
			{ 0.213f - C * 0.213f - S * 0.787f, 0.715f - C * 0.715f + S * 0.715f, 0.072f + C * 0.928f + S * 0.072f },
		};
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				Out.M[Row][Col] = Values[Row][Col];
			}
		}
		break;
	}
	}
	return Out;
}

bool FVekaColorMatrix::IsIdentity() const
{
	return *this == FVekaColorMatrix();
}

FVekaColorMatrix FVekaColorMatrix::Then(const FVekaColorMatrix& Outer) const
{
	// Result = Outer * Inner, in homogeneous 5x5 form (last row 0 0 0 0 1).
	FVekaColorMatrix Out;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 5; ++Col)
		{
			float Sum = Col == 4 ? Outer.M[Row][4] : 0.f;
			for (int32 K = 0; K < 4; ++K)
			{
				Sum += Outer.M[Row][K] * M[K][Col];
			}
			Out.M[Row][Col] = Sum;
		}
	}
	return Out;
}

FLinearColor FVekaColorMatrix::Apply(const FLinearColor& Linear) const
{
	const float In[4] = { Veka::Colora::LinearToSRGB(Linear.R), Veka::Colora::LinearToSRGB(Linear.G), Veka::Colora::LinearToSRGB(Linear.B), Linear.A };
	float Out[4];
	for (int32 Row = 0; Row < 4; ++Row)
	{
		Out[Row] = M[Row][4];
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Out[Row] += M[Row][Col] * In[Col];
		}
		Out[Row] = FMath::Clamp(Out[Row], 0.f, 1.f);
	}
	return FLinearColor(Veka::Colora::SRGBToLinear(Out[0]), Veka::Colora::SRGBToLinear(Out[1]), Veka::Colora::SRGBToLinear(Out[2]), Out[3]);
}

bool FVekaColorMatrix::operator==(const FVekaColorMatrix& Other) const
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 5; ++Col)
		{
			if (!FMath::IsNearlyEqual(M[Row][Col], Other.M[Row][Col], 1.0e-6f))
			{
				return false;
			}
		}
	}
	return true;
}

// --- Conversions -------------------------------------------------------------------------------------------

namespace Veka::Colora
{
	float LinearToSRGB(float Value)
	{
		if (Value <= 0.f)
		{
			return 0.f;
		}
		return Value <= 0.0031308f ? Value * 12.92f : 1.055f * FMath::Pow(Value, 1.f / 2.4f) - 0.055f;
	}

	float SRGBToLinear(float Value)
	{
		if (Value <= 0.f)
		{
			return 0.f;
		}
		return Value <= 0.04045f ? Value / 12.92f : FMath::Pow((Value + 0.055f) / 1.055f, 2.4f);
	}

	FVekaOkLab LinearToOkLab(const FLinearColor& C)
	{
		const float L = 0.4122214708f * C.R + 0.5363325363f * C.G + 0.0514459929f * C.B;
		const float M = 0.2119034982f * C.R + 0.6806995451f * C.G + 0.1073969566f * C.B;
		const float S = 0.0883024619f * C.R + 0.2817188376f * C.G + 0.6299787005f * C.B;

		const float L_ = FMath::Sign(L) * FMath::Pow(FMath::Abs(L), 1.f / 3.f);
		const float M_ = FMath::Sign(M) * FMath::Pow(FMath::Abs(M), 1.f / 3.f);
		const float S_ = FMath::Sign(S) * FMath::Pow(FMath::Abs(S), 1.f / 3.f);

		FVekaOkLab Out;
		Out.L = 0.2104542553f * L_ + 0.7936177850f * M_ - 0.0040720468f * S_;
		Out.A = 1.9779984951f * L_ - 2.4285922050f * M_ + 0.4505937099f * S_;
		Out.B = 0.0259040371f * L_ + 0.7827717662f * M_ - 0.8086757660f * S_;
		return Out;
	}

	FLinearColor OkLabToLinear(const FVekaOkLab& Lab, float Alpha)
	{
		const float L_ = Lab.L + 0.3963377774f * Lab.A + 0.2158037573f * Lab.B;
		const float M_ = Lab.L - 0.1055613458f * Lab.A - 0.0638541728f * Lab.B;
		const float S_ = Lab.L - 0.0894841775f * Lab.A - 1.2914855480f * Lab.B;

		const float L = L_ * L_ * L_;
		const float M = M_ * M_ * M_;
		const float S = S_ * S_ * S_;

		return FLinearColor(
			+4.0767416621f * L - 3.3077115913f * M + 0.2309699292f * S,
			-1.2684380046f * L + 2.6097574011f * M - 0.3413193965f * S,
			-0.0041960863f * L - 0.7034186147f * M + 1.7076147010f * S,
			Alpha);
	}

	FVekaOkLCH OkLabToOkLCH(const FVekaOkLab& Lab)
	{
		FVekaOkLCH Out;
		Out.L = Lab.L;
		Out.C = FMath::Sqrt(Lab.A * Lab.A + Lab.B * Lab.B);
		float Hue = FMath::RadiansToDegrees(FMath::Atan2(Lab.B, Lab.A));
		Out.H = Hue < 0.f ? Hue + 360.f : Hue;
		return Out;
	}

	FVekaOkLab OkLCHToOkLab(const FVekaOkLCH& Lch)
	{
		const float Radians = FMath::DegreesToRadians(Lch.H);
		FVekaOkLab Out;
		Out.L = Lch.L;
		Out.A = Lch.C * FMath::Cos(Radians);
		Out.B = Lch.C * FMath::Sin(Radians);
		return Out;
	}

	FVector4f ToSpace(const FLinearColor& C, EVekaColorSpace Space)
	{
		const float Alpha = FMath::Clamp(C.A, 0.f, 1.f);
		switch (Space)
		{
		case EVekaColorSpace::SRGB:
			return FVector4f(LinearToSRGB(C.R) * Alpha, LinearToSRGB(C.G) * Alpha, LinearToSRGB(C.B) * Alpha, Alpha);
		case EVekaColorSpace::Linear:
			return FVector4f(C.R * Alpha, C.G * Alpha, C.B * Alpha, Alpha);
		case EVekaColorSpace::OkLab:
		{
			const FVekaOkLab Lab = LinearToOkLab(C);
			return FVector4f(Lab.L * Alpha, Lab.A * Alpha, Lab.B * Alpha, Alpha);
		}
		case EVekaColorSpace::OkLCH:
		default:
		{
			// Hue is not premultiplied (it is an angle).
			const FVekaOkLCH Lch = OkLabToOkLCH(LinearToOkLab(C));
			return FVector4f(Lch.L * Alpha, Lch.C * Alpha, Lch.H, Alpha);
		}
		}
	}

	FLinearColor FromSpace(const FVector4f& V, EVekaColorSpace Space)
	{
		const float Alpha = FMath::Clamp(V.W, 0.f, 1.f);
		if (Alpha <= 1.0e-6f)
		{
			return FLinearColor::Transparent;
		}
		const float Inv = 1.f / Alpha;
		switch (Space)
		{
		case EVekaColorSpace::SRGB:
			return FLinearColor(SRGBToLinear(V.X * Inv), SRGBToLinear(V.Y * Inv), SRGBToLinear(V.Z * Inv), Alpha);
		case EVekaColorSpace::Linear:
			return FLinearColor(V.X * Inv, V.Y * Inv, V.Z * Inv, Alpha);
		case EVekaColorSpace::OkLab:
		{
			const FLinearColor Out = OkLabToLinear(FVekaOkLab{ V.X * Inv, V.Y * Inv, V.Z * Inv }, Alpha);
			return FLinearColor(FMath::Max(Out.R, 0.f), FMath::Max(Out.G, 0.f), FMath::Max(Out.B, 0.f), Alpha);
		}
		case EVekaColorSpace::OkLCH:
		default:
		{
			const FLinearColor Out = OkLabToLinear(OkLCHToOkLab(FVekaOkLCH{ V.X * Inv, V.Y * Inv, V.Z }), Alpha);
			return FLinearColor(FMath::Max(Out.R, 0.f), FMath::Max(Out.G, 0.f), FMath::Max(Out.B, 0.f), Alpha);
		}
		}
	}

	FVector4f LerpInSpace(const FVector4f& A, const FVector4f& B, float T, EVekaColorSpace Space)
	{
		if (Space != EVekaColorSpace::OkLCH)
		{
			return A + (B - A) * T;
		}

		// Achromatic or transparent ends have no meaningful hue: borrow the other end's.
		float HueA = A.Z;
		float HueB = B.Z;
		const bool bNoHueA = A.Y <= HueEpsilon || A.W <= HueEpsilon;
		const bool bNoHueB = B.Y <= HueEpsilon || B.W <= HueEpsilon;
		if (bNoHueA && !bNoHueB)
		{
			HueA = HueB;
		}
		else if (bNoHueB && !bNoHueA)
		{
			HueB = HueA;
		}

		float Delta = FMath::Fmod(HueB - HueA, 360.f);
		if (Delta > 180.f)
		{
			Delta -= 360.f;
		}
		else if (Delta < -180.f)
		{
			Delta += 360.f;
		}
		float Hue = HueA + Delta * T;
		Hue = FMath::Fmod(Hue, 360.f);
		if (Hue < 0.f)
		{
			Hue += 360.f;
		}

		return FVector4f(A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T, Hue, A.W + (B.W - A.W) * T);
	}

	FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, float T, EVekaColorSpace Space)
	{
		return FromSpace(LerpInSpace(ToSpace(A, Space), ToSpace(B, Space), T, Space), Space);
	}
}
