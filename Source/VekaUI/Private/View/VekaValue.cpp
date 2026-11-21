// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaValue.h"

namespace
{
	FInstancedStruct Wrap(const FVekaValue& Value)
	{
		return FInstancedStruct::Make(Value);
	}

	FVekaValue Unwrap(const FInstancedStruct& Item)
	{
		const FVekaValue* Value = Item.GetPtr<FVekaValue>();
		return Value ? *Value : FVekaValue();
	}
}
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "Layout/Margin.h"

FVekaValue FVekaValue::Bool(bool Value) { FVekaValue Out; Out.Type = EVekaValueType::Bool; Out.BoolValue = Value; return Out; }
FVekaValue FVekaValue::Int(int64 Value) { FVekaValue Out; Out.Type = EVekaValueType::Int; Out.IntValue = Value; return Out; }
FVekaValue FVekaValue::Float(double Value) { FVekaValue Out; Out.Type = EVekaValueType::Float; Out.FloatValue = Value; return Out; }
FVekaValue FVekaValue::String(const FString& Value) { FVekaValue Out; Out.Type = EVekaValueType::String; Out.StringValue = Value; return Out; }
FVekaValue FVekaValue::Color(const FLinearColor& Value) { FVekaValue Out; Out.Type = EVekaValueType::Color; Out.ColorValue = Value; return Out; }
FVekaValue FVekaValue::Vector2(const FVector2D& Value) { FVekaValue Out; Out.Type = EVekaValueType::Vector2; Out.VectorValue = Value; return Out; }
FVekaValue FVekaValue::Text(const FText& Value) { FVekaValue Out; Out.Type = EVekaValueType::Text; Out.TextValue = Value; return Out; }
FVekaValue FVekaValue::Array(TArray<FVekaValue> Items)
{
	FVekaValue Out;
	Out.Type = EVekaValueType::Array;
	Out.ArrayValue.Reserve(Items.Num());
	for (const FVekaValue& Item : Items)
	{
		Out.ArrayValue.Add(Wrap(Item));
	}
	return Out;
}
FVekaValue FVekaValue::Struct(TMap<FName, FVekaValue> Fields)
{
	FVekaValue Out;
	Out.Type = EVekaValueType::Struct;
	for (const TPair<FName, FVekaValue>& Pair : Fields)
	{
		Out.StructValue.Add(Pair.Key, Wrap(Pair.Value));
	}
	return Out;
}

FVekaValue FVekaValue::GetField(FName Field) const
{
	const FInstancedStruct* Found = Type == EVekaValueType::Struct ? StructValue.Find(Field) : nullptr;
	return Found ? Unwrap(*Found) : FVekaValue();
}

FVekaValue FVekaValue::GetItem(int64 Index) const
{
	return Type == EVekaValueType::Array && ArrayValue.IsValidIndex(static_cast<int32>(Index)) ? Unwrap(ArrayValue[static_cast<int32>(Index)]) : FVekaValue();
}

FText FVekaValue::AsText() const
{
	return Type == EVekaValueType::Text ? TextValue : FText::FromString(AsString());
}

bool FVekaValue::AsBool() const
{
	switch (Type)
	{
	case EVekaValueType::Bool: return BoolValue;
	case EVekaValueType::Int: return IntValue != 0;
	case EVekaValueType::Float: return FloatValue != 0.0;
	case EVekaValueType::String: return !StringValue.IsEmpty() && StringValue != TEXT("false") && StringValue != TEXT("0");
	case EVekaValueType::Color: return ColorValue.A > 0.f;
	case EVekaValueType::Vector2: return !VectorValue.IsZero();
	case EVekaValueType::Text: return !TextValue.IsEmpty();
	case EVekaValueType::Array: return ArrayValue.Num() > 0;
	case EVekaValueType::Struct: return StructValue.Num() > 0;
	default: return false;
	}
}

int64 FVekaValue::AsInt() const
{
	switch (Type)
	{
	case EVekaValueType::Bool: return BoolValue ? 1 : 0;
	case EVekaValueType::Int: return IntValue;
	case EVekaValueType::Float: return static_cast<int64>(FMath::RoundToDouble(FloatValue));
	case EVekaValueType::String: return FCString::Atoi64(*StringValue);
	case EVekaValueType::Text: return FCString::Atoi64(*TextValue.ToString());
	default: return 0;
	}
}

double FVekaValue::AsFloat() const
{
	switch (Type)
	{
	case EVekaValueType::Bool: return BoolValue ? 1.0 : 0.0;
	case EVekaValueType::Int: return static_cast<double>(IntValue);
	case EVekaValueType::Float: return FloatValue;
	case EVekaValueType::String: return FCString::Atod(*StringValue);
	case EVekaValueType::Text: return FCString::Atod(*TextValue.ToString());
	default: return 0.0;
	}
}

FString FVekaValue::AsString() const
{
	switch (Type)
	{
	case EVekaValueType::Bool: return BoolValue ? TEXT("true") : TEXT("false");
	case EVekaValueType::Int: return FString::Printf(TEXT("%lld"), IntValue);
	case EVekaValueType::Float:
	{
		// Up to 3 decimals, trailing zeros removed: 1.5, 72 (not 72.000003 for a float 0.72 * 100).
		FString Text = FString::Printf(TEXT("%.3f"), FloatValue);
		while (Text.EndsWith(TEXT("0")))
		{
			Text.LeftChopInline(1);
		}
		if (Text.EndsWith(TEXT(".")))
		{
			Text.LeftChopInline(1);
		}
		return Text == TEXT("-0") ? FString(TEXT("0")) : Text;
	}
	case EVekaValueType::String: return StringValue;
	case EVekaValueType::Color: return ColorValue.ToFColor(true).ToHex();
	case EVekaValueType::Vector2: return VectorValue.ToString();
	case EVekaValueType::Text: return TextValue.ToString();
	case EVekaValueType::Array:
	{
		TArray<FString> Items;
		for (const FInstancedStruct& Item : ArrayValue)
		{
			Items.Add(Unwrap(Item).AsString());
		}
		return TEXT("[") + FString::Join(Items, TEXT(", ")) + TEXT("]");
	}
	case EVekaValueType::Struct:
	{
		TArray<FString> Fields;
		for (const TPair<FName, FInstancedStruct>& Pair : StructValue)
		{
			Fields.Add(Pair.Key.ToString() + TEXT(": ") + Unwrap(Pair.Value).AsString());
		}
		return TEXT("{") + FString::Join(Fields, TEXT(", ")) + TEXT("}");
	}
	default: return FString();
	}
}

FLinearColor FVekaValue::AsColor() const
{
	if (Type == EVekaValueType::Color)
	{
		return ColorValue;
	}
	if (Type == EVekaValueType::String && StringValue.StartsWith(TEXT("#")))
	{
		return FLinearColor(FColor::FromHex(StringValue));
	}
	return FLinearColor::White;
}

FVector2D FVekaValue::AsVector2() const
{
	if (Type == EVekaValueType::Vector2)
	{
		return VectorValue;
	}
	const double Value = AsFloat();
	return FVector2D(Value, Value);
}

bool FVekaValue::operator==(const FVekaValue& Other) const
{
	if (IsNumber() && Other.IsNumber() && (Type != EVekaValueType::Bool || Other.Type != EVekaValueType::Bool))
	{
		return AsFloat() == Other.AsFloat();
	}
	const bool bTextLike = (Type == EVekaValueType::Text || Type == EVekaValueType::String) && (Other.Type == EVekaValueType::Text || Other.Type == EVekaValueType::String);
	if (bTextLike && (Type == EVekaValueType::Text || Other.Type == EVekaValueType::Text))
	{
		return AsString() == Other.AsString();
	}
	if (Type != Other.Type)
	{
		return false;
	}
	switch (Type)
	{
	case EVekaValueType::Bool: return BoolValue == Other.BoolValue;
	case EVekaValueType::String: return StringValue == Other.StringValue;
	case EVekaValueType::Color: return ColorValue == Other.ColorValue;
	case EVekaValueType::Vector2: return VectorValue == Other.VectorValue;
	case EVekaValueType::Array:
	{
		if (ArrayValue.Num() != Other.ArrayValue.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < ArrayValue.Num(); ++Index)
		{
			if (Unwrap(ArrayValue[Index]) != Unwrap(Other.ArrayValue[Index]))
			{
				return false;
			}
		}
		return true;
	}
	case EVekaValueType::Struct:
	{
		if (StructValue.Num() != Other.StructValue.Num())
		{
			return false;
		}
		for (const TPair<FName, FInstancedStruct>& Pair : StructValue)
		{
			const FInstancedStruct* Found = Other.StructValue.Find(Pair.Key);
			if (!Found || Unwrap(*Found) != Unwrap(Pair.Value))
			{
				return false;
			}
		}
		return true;
	}
	case EVekaValueType::None: return true;
	default: return AsFloat() == Other.AsFloat();
	}
}

uint32 GetTypeHash(const FVekaValue& Value)
{
	switch (Value.Type)
	{
	case EVekaValueType::Int:
	case EVekaValueType::Float: return GetTypeHash(Value.AsFloat());
	case EVekaValueType::Bool: return Value.BoolValue ? 1u : 2u;
	case EVekaValueType::String: return GetTypeHash(Value.StringValue);
	case EVekaValueType::Color: return GetTypeHash(Value.ColorValue);
	case EVekaValueType::Vector2: return GetTypeHash(Value.VectorValue);
	case EVekaValueType::Text: return GetTypeHash(Value.TextValue.ToString());
	case EVekaValueType::Array:
	{
		uint32 Hash = 0x41u;
		for (const FInstancedStruct& Item : Value.ArrayValue)
		{
			Hash = HashCombine(Hash, GetTypeHash(Unwrap(Item)));
		}
		return Hash;
	}
	case EVekaValueType::Struct:
	{
		uint32 Hash = 0x53u;
		for (const TPair<FName, FInstancedStruct>& Pair : Value.StructValue)
		{
			Hash ^= HashCombine(GetTypeHash(Pair.Key), GetTypeHash(Unwrap(Pair.Value)));
		}
		return Hash;
	}
	default: return 0u;
	}
}

FVekaValue FVekaValue::FromProperty(const FProperty* Property, const void* ValuePtr)
{
	if (!Property || !ValuePtr)
	{
		return FVekaValue();
	}
	if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
	{
		return FVekaValue::Bool(Bool->GetPropertyValue(ValuePtr));
	}
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		const int64 Raw = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
		return FVekaValue::String(EnumProperty->GetEnum()->GetNameStringByValue(Raw));
	}
	if (const FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum)
	{
		return FVekaValue::String(Byte->Enum->GetNameStringByValue(Byte->GetPropertyValue(ValuePtr)));
	}
	if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
	{
		return Numeric->IsFloatingPoint() ? FVekaValue::Float(Numeric->GetFloatingPointPropertyValue(ValuePtr)) : FVekaValue::Int(Numeric->GetSignedIntPropertyValue(ValuePtr));
	}
	if (const FStrProperty* Str = CastField<FStrProperty>(Property))
	{
		return FVekaValue::String(Str->GetPropertyValue(ValuePtr));
	}
	if (const FNameProperty* Name = CastField<FNameProperty>(Property))
	{
		return FVekaValue::String(Name->GetPropertyValue(ValuePtr).ToString());
	}
	if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		return FVekaValue::Text(TextProperty->GetPropertyValue(ValuePtr));
	}
	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
		TArray<FVekaValue> Items;
		Items.Reserve(Helper.Num());
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			Items.Add(FromProperty(ArrayProperty->Inner, Helper.GetRawPtr(Index)));
		}
		return FVekaValue::Array(MoveTemp(Items));
	}
	if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
	{
		if (Struct->Struct == TBaseStructure<FLinearColor>::Get())
		{
			return FVekaValue::Color(*static_cast<const FLinearColor*>(ValuePtr));
		}
		if (Struct->Struct == TBaseStructure<FColor>::Get())
		{
			return FVekaValue::Color(FLinearColor(*static_cast<const FColor*>(ValuePtr)));
		}
		if (Struct->Struct == TBaseStructure<FVector2D>::Get())
		{
			return FVekaValue::Vector2(*static_cast<const FVector2D*>(ValuePtr));
		}
		if (Struct->Struct == FVekaValue::StaticStruct())
		{
			return *static_cast<const FVekaValue*>(ValuePtr);
		}
		// Any other struct: its fields by name (`item.Name`).
		TMap<FName, FVekaValue> Fields;
		for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
		{
			Fields.Add(It->GetFName(), FromProperty(*It, It->ContainerPtrToValuePtr<void>(ValuePtr)));
		}
		return FVekaValue::Struct(MoveTemp(Fields));
	}
	return FVekaValue();
}

bool FVekaValue::ToProperty(const FProperty* Property, void* ValuePtr) const
{
	if (!Property || !ValuePtr)
	{
		return false;
	}
	if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		BoolProperty->SetPropertyValue(ValuePtr, AsBool());
		return true;
	}
	auto EnumValue = [this](const UEnum* Enum, int64& Out)
	{
		if (Type == EVekaValueType::String)
		{
			const int64 Found = Enum->GetValueByNameString(StringValue);
			if (Found == INDEX_NONE)
			{
				return false;
			}
			Out = Found;
			return true;
		}
		Out = AsInt();
		return true;
	};
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		int64 Raw = 0;
		if (!EnumValue(EnumProperty->GetEnum(), Raw))
		{
			return false;
		}
		EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Raw);
		return true;
	}
	if (const FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum)
	{
		int64 Raw = 0;
		if (!EnumValue(Byte->Enum, Raw))
		{
			return false;
		}
		Byte->SetPropertyValue(ValuePtr, static_cast<uint8>(Raw));
		return true;
	}
	if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
	{
		if (Numeric->IsFloatingPoint())
		{
			Numeric->SetFloatingPointPropertyValue(ValuePtr, AsFloat());
		}
		else
		{
			Numeric->SetIntPropertyValue(ValuePtr, AsInt());
		}
		return true;
	}
	if (const FStrProperty* Str = CastField<FStrProperty>(Property))
	{
		Str->SetPropertyValue(ValuePtr, AsString());
		return true;
	}
	if (const FNameProperty* Name = CastField<FNameProperty>(Property))
	{
		Name->SetPropertyValue(ValuePtr, FName(*AsString()));
		return true;
	}
	if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		TextProperty->SetPropertyValue(ValuePtr, AsText());
		return true;
	}
	if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
	{
		if (Struct->Struct == TBaseStructure<FLinearColor>::Get())
		{
			*static_cast<FLinearColor*>(ValuePtr) = AsColor();
			return true;
		}
		if (Struct->Struct == TBaseStructure<FVector2D>::Get())
		{
			*static_cast<FVector2D*>(ValuePtr) = AsVector2();
			return true;
		}
		if (Struct->Struct == FMargin::StaticStruct())
		{
			const FVector2D V = AsVector2();
			*static_cast<FMargin*>(ValuePtr) = FMargin(static_cast<float>(V.X), static_cast<float>(V.Y));
			return true;
		}
		if (Struct->Struct == FVekaValue::StaticStruct())
		{
			*static_cast<FVekaValue*>(ValuePtr) = *this;
			return true;
		}
	}
	return false;
}
