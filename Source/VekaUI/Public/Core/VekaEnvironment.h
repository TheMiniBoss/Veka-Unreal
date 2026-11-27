// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaTypes.h"

/** Type-erased environment value. */
struct FVekaEnvValueBase
{
	virtual ~FVekaEnvValueBase() = default;
	virtual bool Equals(const FVekaEnvValueBase& Other) const = 0;
};

template <typename T>
struct TVekaEnvValue final : public FVekaEnvValueBase
{
	T Value;

	explicit TVekaEnvValue(const T& InValue) : Value(InValue) {}

	virtual bool Equals(const FVekaEnvValueBase& Other) const override
	{
		if constexpr (requires(const T& A, const T& B) { A == B; })
		{
			// Same key always stores the same type, see FVekaEnvironment.
			return Value == static_cast<const TVekaEnvValue<T>&>(Other).Value;
		}
		else
		{
			return false;
		}
	}
};

/**
 * Values inherited down the view tree (theme, color scheme, font, foreground color...).
 * Copied by value per node; the map is shared until written (copy on write) and values are immutable pointers.
 * A given key must always be used with the same C++ type.
 */
class VEKAUI_API FVekaEnvironment
{
public:
	template <typename T>
	void Set(FName Key, const T& Value)
	{
		Mutable().Add(Key, MakeShared<TVekaEnvValue<T>>(Value));
	}

	template <typename T>
	const T* Find(FName Key) const
	{
		if (const TSharedPtr<const FVekaEnvValueBase>* Found = Values.IsValid() ? Values->Find(Key) : nullptr)
		{
			return &static_cast<const TVekaEnvValue<T>*>(Found->Get())->Value;
		}
		return nullptr;
	}

	template <typename T>
	T Get(FName Key, const T& Default) const
	{
		const T* Found = Find<T>(Key);
		return Found ? *Found : Default;
	}

	bool Contains(FName Key) const { return Values.IsValid() && Values->Contains(Key); }
	void Remove(FName Key)
	{
		if (Contains(Key))
		{
			Mutable().Remove(Key);
		}
	}
	int32 Num() const { return Values.IsValid() ? Values->Num() : 0; }

	bool operator==(const FVekaEnvironment& Other) const;
	bool operator!=(const FVekaEnvironment& Other) const { return !(*this == Other); }

private:
	using FMap = TMap<FName, TSharedPtr<const FVekaEnvValueBase>>;

	/** The map to write into: copied first if another environment shares it (copy on write). */
	FMap& Mutable()
	{
		if (!Values.IsValid())
		{
			Values = MakeShared<FMap>();
		}
		else if (!Values.IsUnique())
		{
			Values = MakeShared<FMap>(*Values);
		}
		return *Values;
	}

	/** Shared between copies until one of them writes: copying an environment (every node, every evaluation) is a pointer copy. */
	TSharedPtr<FMap> Values;
};

/** Built-in environment keys. */
namespace VekaEnv
{
	/** FLinearColor — default color for text, symbols and strokes. */
	inline FName ForegroundColor() { static const FName Name(TEXT("Veka.ForegroundColor")); return Name; }
	/** FVekaPaint (resolved) — set with the foreground color when `.Foreground` gets a paint; gradients color text per glyph. */
	inline FName ForegroundPaint() { static const FName Name(TEXT("Veka.ForegroundPaint")); return Name; }
	/** Deprecated key (Phase 1): fonts now come from the typography (VekaEnv::Typography, `.Font(...)`). */
	inline FName Font() { static const FName Name(TEXT("Veka.Font")); return Name; }
	/** EVekaColorScheme — light or dark. */
	inline FName ColorScheme() { static const FName Name(TEXT("Veka.ColorScheme")); return Name; }

	VEKAUI_API FLinearColor GetForegroundColor(const FVekaEnvironment& Environment);
	/** Font of Text in this environment (typography resolved: family, style, weight, size, tracking). */
	VEKAUI_API FSlateFontInfo GetFont(const FVekaEnvironment& Environment);
	VEKAUI_API EVekaColorScheme GetColorScheme(const FVekaEnvironment& Environment);

	/** Default root environment: dark scheme, white foreground, Body text style (LT Flode Neue). */
	VEKAUI_API FVekaEnvironment MakeDefault();
}
