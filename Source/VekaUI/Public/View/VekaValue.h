// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VekaValue.generated.h"

class FProperty;

UENUM(BlueprintType)
enum class EVekaValueType : uint8
{
	None,
	Bool,
	Int,
	Float,
	/** Text, names and enum values (by name). */
	String,
	Color,
	Vector2
};

/**
 * Typed value of the asset views and of the Blueprint bridge (state, parameters, events). Converts loosely, like a
 * scripting language: a number is "true" when not zero, any value prints as a string.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value")
	EVekaValueType Type = EVekaValueType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value", meta = (EditCondition = "Type == EVekaValueType::Bool", EditConditionHides))
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value", meta = (EditCondition = "Type == EVekaValueType::Int", EditConditionHides))
	int64 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value", meta = (EditCondition = "Type == EVekaValueType::Float", EditConditionHides))
	double FloatValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value", meta = (EditCondition = "Type == EVekaValueType::String", EditConditionHides))
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value", meta = (EditCondition = "Type == EVekaValueType::Color", EditConditionHides))
	FLinearColor ColorValue = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Value", meta = (EditCondition = "Type == EVekaValueType::Vector2", EditConditionHides))
	FVector2D VectorValue = FVector2D::ZeroVector;

	static FVekaValue Bool(bool Value);
	static FVekaValue Int(int64 Value);
	static FVekaValue Float(double Value);
	static FVekaValue String(const FString& Value);
	static FVekaValue Color(const FLinearColor& Value);
	static FVekaValue Vector2(const FVector2D& Value);

	bool IsNone() const { return Type == EVekaValueType::None; }
	bool IsNumber() const { return Type == EVekaValueType::Int || Type == EVekaValueType::Float || Type == EVekaValueType::Bool; }

	bool AsBool() const;
	int64 AsInt() const;
	double AsFloat() const;
	FString AsString() const;
	FText AsText() const { return FText::FromString(AsString()); }
	FLinearColor AsColor() const;
	FVector2D AsVector2() const;

	/** Same type and value; numbers compare by value (1 == 1.0). */
	bool operator==(const FVekaValue& Other) const;
	bool operator!=(const FVekaValue& Other) const { return !(*this == Other); }

	/** Reads a reflected property (bool, numbers, strings, names, texts, enums, colors, vectors). */
	static FVekaValue FromProperty(const FProperty* Property, const void* ValuePtr);
	/** Writes into a reflected property; false if the type cannot hold this value. */
	bool ToProperty(const FProperty* Property, void* ValuePtr) const;
};

VEKAUI_API uint32 GetTypeHash(const FVekaValue& Value);

/** Blueprint helpers to make and read Veka values. */
UCLASS()
class VEKAUI_API UVekaValueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Veka Value (Bool)"))
	static FVekaValue MakeVekaBool(bool Value) { return FVekaValue::Bool(Value); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Veka Value (Integer)"))
	static FVekaValue MakeVekaInt(int32 Value) { return FVekaValue::Int(Value); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Veka Value (Float)"))
	static FVekaValue MakeVekaFloat(double Value) { return FVekaValue::Float(Value); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Veka Value (String)"))
	static FVekaValue MakeVekaString(const FString& Value) { return FVekaValue::String(Value); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Veka Value (Color)"))
	static FVekaValue MakeVekaColor(FLinearColor Value) { return FVekaValue::Color(Value); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Veka Value (Vector2D)"))
	static FVekaValue MakeVekaVector2(FVector2D Value) { return FVekaValue::Vector2(Value); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Bool (Veka Value)"))
	static bool VekaToBool(const FVekaValue& Value) { return Value.AsBool(); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Integer (Veka Value)"))
	static int32 VekaToInt(const FVekaValue& Value) { return static_cast<int32>(Value.AsInt()); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Float (Veka Value)"))
	static double VekaToFloat(const FVekaValue& Value) { return Value.AsFloat(); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To String (Veka Value)"))
	static FString VekaToString(const FVekaValue& Value) { return Value.AsString(); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (BlueprintAutocast, DisplayName = "To Color (Veka Value)"))
	static FLinearColor VekaToColor(const FVekaValue& Value) { return Value.AsColor(); }

	UFUNCTION(BlueprintPure, Category = "VekaUI|Value", meta = (DisplayName = "Equal (Veka Value)", CompactNodeTitle = "=="))
	static bool VekaEqual(const FVekaValue& A, const FVekaValue& B) { return A == B; }
};
