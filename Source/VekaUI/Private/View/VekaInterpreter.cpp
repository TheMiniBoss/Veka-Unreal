// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaViewAsset.h"
#include "View/VekaDocument.h"
#include "View/VekaCatalog.h"
#include "View/VekaComponentLibrary.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "Modifiers/VekaColoraModifiers.h"
#include "Core/VekaView.h"
#include "Core/VekaNode.h"
#include "Core/VekaSchema.h"
#include "Core/VekaPaint.h"
#include "Anima/VekaImplicit.h"
#include "Anima/VekaTimeline.h"
#include "Colora/VekaPaintAsset.h"
#include "Anima/VekaTransition.h"
#include "Controls/VekaControls.h"
#include "Symbols/VekaSymbolView.h"
#include "VekaBuilders.h"
#include "VekaUILog.h"
#include "Primitives/VekaCanvas.h"
#include "Colora/VekaTheme.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

// --- View model ----------------------------------------------------------------------------------------------

void UVekaViewModel::NotifyChanged()
{
	OnChanged.Broadcast();
}

FVekaValue UVekaViewModel::GetValue(FName Property) const
{
	const FProperty* Found = GetClass()->FindPropertyByName(Property);
	return Found ? FVekaValue::FromProperty(Found, Found->ContainerPtrToValuePtr<void>(this)) : FVekaValue();
}

bool UVekaViewModel::SetValue(FName Property, const FVekaValue& Value)
{
	const FProperty* Found = GetClass()->FindPropertyByName(Property);
	if (!Found || !Value.ToProperty(Found, Found->ContainerPtrToValuePtr<void>(this)))
	{
		return false;
	}
	NotifyChanged();
	return true;
}

uint32 UVekaViewModel::ComputeHash() const
{
	uint32 Hash = 0;
	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		if (It->GetOwnerClass() == UObject::StaticClass())
		{
			continue;
		}
		FString Text;
		It->ExportTextItem_InContainer(Text, this, nullptr, nullptr, PPF_None);
		Hash = HashCombine(Hash, GetTypeHash(Text));
	}
	return Hash;
}

// --- Store ---------------------------------------------------------------------------------------------------

FVekaViewStore::~FVekaViewStore()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	}
	if (UVekaViewModel* Pinned = Model.Get())
	{
		Pinned->OnChanged.Remove(ModelChangedHandle);
	}
}

FVekaValue FVekaViewStore::Get(FName Name) const
{
	const FVekaValue* Found = Values.Find(Name);
	return Found ? *Found : FVekaValue();
}

void FVekaViewStore::Set(FName Name, const FVekaValue& Value)
{
	FVekaValue& Slot = Values.FindOrAdd(Name);
	if (Slot == Value && Slot.Type == Value.Type)
	{
		return;
	}
	Slot = Value;
	Invalidate();
	if (OnValueChanged)
	{
		OnValueChanged(Name, Value);
	}
}

void FVekaViewStore::Declare(FName Name, const FVekaValue& Initial)
{
	if (!Values.Contains(Name))
	{
		Values.Add(Name, Initial);
	}
}

void FVekaViewStore::SetModel(UVekaViewModel* InModel)
{
	SetModelSilently(InModel);
	Invalidate();
}

void FVekaViewStore::SetModelSilently(UVekaViewModel* InModel)
{
	if (UVekaViewModel* Old = Model.Get())
	{
		Old->OnChanged.Remove(ModelChangedHandle);
	}
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		PollHandle.Reset();
	}
	Model = InModel;
	if (InModel)
	{
		const TWeakPtr<FVekaViewStore> Weak = AsShared();
		ModelChangedHandle = InModel->OnChanged.AddLambda([Weak]()
		{
			if (TSharedPtr<FVekaViewStore> Pinned = Weak.Pin())
			{
				Pinned->ModelHash = Pinned->Model.IsValid() ? Pinned->Model->ComputeHash() : 0;
				Pinned->Invalidate();
			}
		});
		ModelHash = InModel->ComputeHash();
		// Blueprint "Set" nodes do not notify: look for changes ten times a second.
		PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float DeltaTime)
		{
			TSharedPtr<FVekaViewStore> Pinned = Weak.Pin();
			return Pinned.IsValid() && Pinned->PollModel(DeltaTime);
		}), 0.1f);
	}
}

bool FVekaViewStore::PollModel(float DeltaTime)
{
	UVekaViewModel* Pinned = Model.Get();
	if (!Pinned)
	{
		return false;
	}
	const uint32 Hash = Pinned->ComputeHash();
	if (Hash != ModelHash)
	{
		ModelHash = Hash;
		Invalidate();
	}
	return true;
}

FVekaValue FVekaViewStore::GetModelValue(FName Property) const
{
	const UVekaViewModel* Pinned = Model.Get();
	return Pinned ? Pinned->GetValue(Property) : FVekaValue();
}

void FVekaViewStore::Emit(FName Event, const FVekaValue& Payload) const
{
	if (OnEvent)
	{
		OnEvent(Event, Payload);
	}
}

void FVekaViewStore::Attach(FVekaNode& Node)
{
	Views.RemoveAll([](const TWeakPtr<FVekaNode>& Weak) { return !Weak.IsValid(); });
	const TWeakPtr<FVekaNode> Weak = Node.AsShared();
	if (!Views.Contains(Weak))
	{
		Views.Add(Weak);
	}
}

void FVekaViewStore::Invalidate()
{
	for (const TWeakPtr<FVekaNode>& Weak : Views)
	{
		if (TSharedPtr<FVekaNode> Node = Weak.Pin())
		{
			Node->MarkDirty();
		}
	}
}

// --- Interpreter ---------------------------------------------------------------------------------------------

/** A name literal compared or looked up often: made once, then compared as an integer. */
#define VEKA_FNAME(Literal) ([]() -> const FName& { static const FName Cached(TEXT(Literal)); return Cached; }())

namespace
{
	using namespace Veka;

	template <typename TEnum>
	TEnum EnumFrom(const FVekaValue& Value, TEnum Default)
	{
		if (Value.IsNone())
		{
			return Default;
		}
		const UEnum* Enum = StaticEnum<TEnum>();
		if (Value.Type == EVekaValueType::String)
		{
			const int64 Found = Enum->GetValueByNameString(Value.StringValue);
			return Found == INDEX_NONE ? Default : static_cast<TEnum>(Found);
		}
		return static_cast<TEnum>(Value.AsInt());
	}

	FVekaPaint PaintFrom(const FVekaValue& Value)
	{
		if (Value.Type == EVekaValueType::Color)
		{
			return FVekaPaint(Value.ColorValue);
		}
		if (Value.Type == EVekaValueType::String)
		{
			if (Value.StringValue.StartsWith(TEXT("#")))
			{
				return FVekaPaint(Hex(Value.StringValue));
			}
			// A paint asset of the project (`.background(.Aurora)`), else a Colora token.
			if (const FVekaPaint* Named = Veka::Paints::Find(FName(*Value.StringValue)))
			{
				return *Named;
			}
			return FVekaPaint::FromToken(FName(*Value.StringValue));
		}
		return FVekaPaint(FLinearColor::White);
	}

	/** Writes argument values into a reflected struct (schema fallback). Names match case-insensitively. */
	void ApplyArguments(const UScriptStruct* Struct, void* Memory, const TArray<TPair<FName, FVekaValue>>& Arguments)
	{
		for (const TPair<FName, FVekaValue>& Argument : Arguments)
		{
			const FProperty* Property = Struct->FindPropertyByName(Argument.Key);
			if (!Property)
			{
				for (TFieldIterator<FProperty> It(Struct); It; ++It)
				{
					if (It->GetName().Equals(Argument.Key.ToString(), ESearchCase::IgnoreCase))
					{
						Property = *It;
						break;
					}
				}
			}
			if (!Property)
			{
				UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: %s has no property '%s'"), *Struct->GetName(), *Argument.Key.ToString());
				continue;
			}
			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Memory);
			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (StructProperty && StructProperty->Struct == FVekaPaint::StaticStruct())
			{
				*static_cast<FVekaPaint*>(ValuePtr) = PaintFrom(Argument.Value);
				continue;
			}
			if (!Argument.Value.ToProperty(Property, ValuePtr))
			{
				UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: cannot set %s.%s from a %s"), *Struct->GetName(), *Property->GetName(),
					*StaticEnum<EVekaValueType>()->GetNameStringByValue(static_cast<int64>(Argument.Value.Type)));
			}
		}
	}

	class FInterpreter
	{
	public:
		FInterpreter(const TSharedRef<const FVekaDocument>& InDocument, const TSharedRef<FVekaViewStore>& InStore)
			: DocumentRef(InDocument), Document(*InDocument), Store(InStore)
		{
		}

		TMap<FName, FVekaValue> Locals;
		/** Parameters of the view (component call arguments; for a root view, store values or defaults). */
		TMap<FName, FVekaValue> Params;
		/** Content the caller gave for each slot (built in the caller's scope). */
		TMap<FName, TArray<FVekaElement>> SlotContents;
		int32 ComponentDepth = 0;
		/** Styles applied inside styles (a style can use another one). */
		int32 StyleDepth = 0;
		/** Asset document the elements are tagged with (the editor maps viewport clicks back to its nodes). */
		const void* Origin = nullptr;
		/** Players of the document's timelines (SetupTimelines). */
		TMap<FName, FVekaTimelineHandle> Timelines;
		/** `time`: a timeline that never stops (only when the document reads it): the view re-evaluates every frame. */
		TOptional<FVekaTimelineHandle> Clock;
		/** `time` while a canvas draws (seconds since the store was made): the canvas repaints without re-evaluating the view. */
		TOptional<double> TimeOverride;

		double CurrentTime() const
		{
			if (TimeOverride.IsSet())
			{
				return TimeOverride.GetValue();
			}
			return Clock.IsSet() && Clock->GetPlayer().IsValid() ? Clock->GetPlayer()->GetTime() : 0.0;
		}

		/**
		 * Players of the document's timelines, kept by the view node: cues become events that run their actions, autoplay
		 * starts once per store, and the store lists them (the editor's Timeline tab).
		 */
		void SetupTimelines(FVekaContext& Ctx)
		{
			if (UsesTime(Document.Root, false))
			{
				// An hour that loops (float time stays precise); playing, it re-evaluates the view every frame.
				FVekaTimelineData Data;
				Data.Length = 3600.f;
				Data.bLoop = true;
				Clock = Ctx.Timeline(TEXT("__clock"), Data);
				if (Clock->GetPlayer().IsValid() && !Clock->GetPlayer()->IsPlaying())
				{
					Clock->Play();
				}
			}
			for (const FVekaDocumentTimeline& Timeline : Document.Timelines)
			{
				FVekaTimelineData Data = Timeline.Data;
				Data.Events.Reset();
				for (int32 Index = 0; Index < Timeline.Cues.Num(); ++Index)
				{
					Data.AddEvent(Timeline.Cues[Index].Time, FName(*FString::Printf(TEXT("cue%d"), Index)));
				}
				const FVekaTimelineHandle Handle = Ctx.Timeline(FName(*(TEXT("__timeline.") + Timeline.Name.ToString())), Data);
				Timelines.Add(Timeline.Name, Handle);
				Store->Timelines.Add(Timeline.Name, Handle.GetPlayer());
			}
			for (const FVekaDocumentTimeline& Timeline : Document.Timelines)
			{
				TArray<TFunction<void()>> Cues;
				for (const FVekaTimelineCue& Cue : Timeline.Cues)
				{
					Cues.Add(MakeActions(Cue.Actions));
				}
				const FVekaTimelineHandle& Handle = Timelines[Timeline.Name];
				Handle.OnEvent([Cues](FName Event)
				{
					const int32 Index = FCString::Atoi(*Event.ToString().Mid(3));
					if (Cues.IsValidIndex(Index) && Cues[Index])
					{
						Cues[Index]();
					}
				});
				if (Timeline.bAutoPlay && !Store->StartedTimelines.Contains(Timeline.Name))
				{
					Store->StartedTimelines.Add(Timeline.Name);
					Handle.Stop();
					Handle.Play();
				}
			}
		}

		/** A number while evaluating, and whether it is an Int (same rules as Binary). */
		struct FNumber
		{
			double Value = 0.0;
			bool bInt = false;
		};

		/**
		 * Numeric expressions without FVekaValue at each step (an animation evaluates hundreds every frame). False when the
		 * expression is not purely numeric: Eval then takes the general path, with the same results.
		 */
		bool EvalNumber(int32 Index, FNumber& Out, int32 Depth = 0) const
		{
			if (!Document.Expressions.IsValidIndex(Index) || Depth > 64)
			{
				return false;
			}
			const FVekaAstExpr& Expr = Document.Expressions[Index];
			auto Operand = [&](int32 Position, FNumber& Value)
			{
				return Expr.Operands.IsValidIndex(Position) && Expr.Operands[Position] != Index && EvalNumber(Expr.Operands[Position], Value, Depth + 1);
			};
			auto FromValue = [&Out](const FVekaValue* Value)
			{
				if (!Value || (Value->Type != EVekaValueType::Int && Value->Type != EVekaValueType::Float))
				{
					return false;
				}
				Out.bInt = Value->Type == EVekaValueType::Int;
				Out.Value = Out.bInt ? static_cast<double>(Value->IntValue) : Value->FloatValue;
				return true;
			};
			switch (Expr.Kind)
			{
			case EVekaExprKind::Literal: return FromValue(&Expr.Value);
			case EVekaExprKind::Local: return FromValue(Locals.Find(Expr.Name));
			case EVekaExprKind::Time:
				Out = { CurrentTime(), false };
				return true;
			case EVekaExprKind::Unary:
			{
				FNumber A;
				if (Expr.Op != EVekaExprOp::Negate || !Operand(0, A))
				{
					return false;
				}
				Out = { -A.Value, A.bInt };
				return true;
			}
			case EVekaExprKind::Call:
			{
				if (Expr.Name == VEKA_FNAME("pi"))
				{
					Out = { UE_DOUBLE_PI, false };
					return true;
				}
				const FVekaCatalogFunction* Function = Veka::Catalog::FindFunction(Expr.Name);
				if (!Function || Function->bColor)
				{
					return false;
				}
				TArray<double, TInlineAllocator<4>> Arguments;
				for (int32 Position = 0; Position < Expr.Operands.Num(); ++Position)
				{
					FNumber Argument;
					if (!Operand(Position, Argument))
					{
						return false;
					}
					Arguments.Add(Argument.Value);
				}
				Out = { Veka::Catalog::CallFunction(Expr.Name, Arguments), false };
				return true;
			}
			case EVekaExprKind::Ternary:
			{
				bool bCondition = false;
				FNumber Left;
				FNumber Right;
				const FVekaAstExpr* Condition = Expr.Operands.IsValidIndex(0) && Document.Expressions.IsValidIndex(Expr.Operands[0]) ? &Document.Expressions[Expr.Operands[0]] : nullptr;
				if (Condition && Condition->Kind == EVekaExprKind::Binary && Condition->Operands.Num() == 2
					&& EvalNumber(Condition->Operands[0], Left, Depth + 1) && EvalNumber(Condition->Operands[1], Right, Depth + 1))
				{
					switch (Condition->Op)
					{
					case EVekaExprOp::Less: bCondition = Left.Value < Right.Value; break;
					case EVekaExprOp::LessEqual: bCondition = Left.Value <= Right.Value; break;
					case EVekaExprOp::Greater: bCondition = Left.Value > Right.Value; break;
					case EVekaExprOp::GreaterEqual: bCondition = Left.Value >= Right.Value; break;
					default: return false;
					}
				}
				else
				{
					return false;
				}
				return Operand(bCondition ? 1 : 2, Out);
			}
			case EVekaExprKind::Binary:
			{
				FNumber A;
				FNumber B;
				if (!Operand(0, A) || !Operand(1, B))
				{
					return false;
				}
				const bool bInts = A.bInt && B.bInt;
				switch (Expr.Op)
				{
				case EVekaExprOp::Add: Out = { A.Value + B.Value, bInts }; return true;
				case EVekaExprOp::Subtract: Out = { A.Value - B.Value, bInts }; return true;
				case EVekaExprOp::Multiply: Out = { A.Value * B.Value, bInts }; return true;
				case EVekaExprOp::Divide:
					if (B.Value == 0.0)
					{
						Out = { 0.0, false };
						return true;
					}
					Out = { A.Value / B.Value, bInts && static_cast<int64>(A.Value) % static_cast<int64>(B.Value) == 0 };
					return true;
				case EVekaExprOp::Modulo:
					if (B.Value == 0.0)
					{
						Out = { 0.0, false };
						return true;
					}
					Out = { bInts ? static_cast<double>(static_cast<int64>(A.Value) % static_cast<int64>(B.Value)) : FMath::Fmod(A.Value, B.Value), bInts };
					return true;
				case EVekaExprOp::Min: Out = { FMath::Min(A.Value, B.Value), bInts }; return true;
				case EVekaExprOp::Max: Out = { FMath::Max(A.Value, B.Value), bInts }; return true;
				default: return false;
				}
			}
			default:
				return false;
			}
		}

		FVekaValue Eval(int32 Index) const
		{
			if (!Document.Expressions.IsValidIndex(Index))
			{
				return FVekaValue();
			}
			const FVekaAstExpr& Expr = Document.Expressions[Index];
			if (Expr.Kind == EVekaExprKind::Binary || Expr.Kind == EVekaExprKind::Call || Expr.Kind == EVekaExprKind::Unary || Expr.Kind == EVekaExprKind::Ternary)
			{
				FNumber Number;
				if (EvalNumber(Index, Number))
				{
					return Number.bInt ? FVekaValue::Int(static_cast<int64>(Number.Value)) : FVekaValue::Float(Number.Value);
				}
			}
			auto Operand = [&](int32 Position) { return Expr.Operands.IsValidIndex(Position) && Expr.Operands[Position] != Index ? Eval(Expr.Operands[Position]) : FVekaValue(); };
			switch (Expr.Kind)
			{
			case EVekaExprKind::Literal: return Expr.Value;
			case EVekaExprKind::State: return Store->Get(Expr.Name);
			case EVekaExprKind::Model: return Store->GetModelValue(Expr.Name);
			case EVekaExprKind::Local:
			{
				const FVekaValue* Found = Locals.Find(Expr.Name);
				return Found ? *Found : FVekaValue();
			}
			case EVekaExprKind::Param:
			{
				if (const FVekaValue* Found = Params.Find(Expr.Name))
				{
					return *Found;
				}
				if (Store->Contains(Expr.Name))
				{
					return Store->Get(Expr.Name);
				}
				const FVekaParamDeclaration* Declaration = Document.FindParam(Expr.Name);
				return Declaration ? Declaration->Default : FVekaValue();
			}
			case EVekaExprKind::Member:
			{
				const FVekaValue Base = Operand(0);
				if (Base.Type == EVekaValueType::Array && Expr.Name == VEKA_FNAME("count"))
				{
					return FVekaValue::Int(Base.Num());
				}
				return Base.GetField(Expr.Name);
			}
			case EVekaExprKind::Timeline:
			{
				// Every track now, as a struct (`Intro.fade` takes a field).
				TMap<FName, FVekaValue> Fields;
				const FVekaTimelineHandle* Handle = Timelines.Find(Expr.Name);
				if (const FVekaDocumentTimeline* Timeline = Document.FindTimeline(Expr.Name))
				{
					for (const FVekaTimelineTrack& Track : Timeline->Data.Tracks)
					{
						Fields.Add(Track.Name, FVekaValue::Float(Handle ? Handle->Value(Track.Name) : Track.Evaluate(0.f)));
					}
				}
				return FVekaValue::Struct(MoveTemp(Fields));
			}
			case EVekaExprKind::Call:
			{
				if (Expr.Name == VEKA_FNAME("pi"))
				{
					return FVekaValue::Float(UE_DOUBLE_PI);
				}
				TArray<double, TInlineAllocator<4>> Arguments;
				for (int32 Position = 0; Position < Expr.Operands.Num(); ++Position)
				{
					Arguments.Add(Operand(Position).AsFloat());
				}
				const FVekaCatalogFunction* Function = Veka::Catalog::FindFunction(Expr.Name);
				if (Function && Function->bColor)
				{
					return FVekaValue::Color(Veka::Catalog::CallColorFunction(Expr.Name, Arguments));
				}
				return FVekaValue::Float(Veka::Catalog::CallFunction(Expr.Name, Arguments));
			}
			case EVekaExprKind::Time: return FVekaValue::Float(CurrentTime());
			case EVekaExprKind::Unary:
			{
				const FVekaValue A = Operand(0);
				return Expr.Op == EVekaExprOp::Negate
					? (A.Type == EVekaValueType::Int ? FVekaValue::Int(-A.IntValue) : FVekaValue::Float(-A.AsFloat()))
					: FVekaValue::Bool(!A.AsBool());
			}
			case EVekaExprKind::Binary: return Binary(Expr.Op, Operand(0), Operand(1));
			case EVekaExprKind::Ternary: return Operand(0).AsBool() ? Operand(1) : Operand(2);
			case EVekaExprKind::Format:
			{
				if (Expr.Value.Type == EVekaValueType::Text)
				{
					// Localizable pattern: translated with its {0} placeholders, then formatted for the culture.
					FFormatOrderedArguments Arguments;
					for (int32 Position = 0; Position < Expr.Operands.Num(); ++Position)
					{
						const FVekaValue Argument = Operand(Position);
						if (Argument.Type == EVekaValueType::Int)
						{
							Arguments.Add(FFormatArgumentValue(Argument.IntValue));
						}
						else if (Argument.Type == EVekaValueType::Float)
						{
							Arguments.Add(FFormatArgumentValue(Argument.FloatValue));
						}
						else
						{
							Arguments.Add(FFormatArgumentValue(Argument.AsText()));
						}
					}
					return FVekaValue::Text(FText::Format(Expr.Value.TextValue, Arguments));
				}
				FString Out = Expr.Value.StringValue;
				for (int32 Position = 0; Position < Expr.Operands.Num(); ++Position)
				{
					Out.ReplaceInline(*FString::Printf(TEXT("{%d}"), Position), *Operand(Position).AsString());
				}
				return FVekaValue::String(Out);
			}
			}
			return FVekaValue();
		}

		static FVekaValue Binary(EVekaExprOp Op, const FVekaValue& A, const FVekaValue& B)
		{
			const bool bInts = A.Type == EVekaValueType::Int && B.Type == EVekaValueType::Int;
			auto Number = [bInts](double Value) { return bInts ? FVekaValue::Int(static_cast<int64>(Value)) : FVekaValue::Float(Value); };
			switch (Op)
			{
			case EVekaExprOp::Add:
				if (A.Type == EVekaValueType::String || B.Type == EVekaValueType::String || A.Type == EVekaValueType::Text || B.Type == EVekaValueType::Text)
				{
					return FVekaValue::String(A.AsString() + B.AsString());
				}
				return bInts ? FVekaValue::Int(A.IntValue + B.IntValue) : FVekaValue::Float(A.AsFloat() + B.AsFloat());
			case EVekaExprOp::Subtract: return bInts ? FVekaValue::Int(A.IntValue - B.IntValue) : FVekaValue::Float(A.AsFloat() - B.AsFloat());
			case EVekaExprOp::Multiply: return bInts ? FVekaValue::Int(A.IntValue * B.IntValue) : FVekaValue::Float(A.AsFloat() * B.AsFloat());
			case EVekaExprOp::Divide:
				if (B.AsFloat() == 0.0)
				{
					return FVekaValue::Float(0.0);
				}
				return bInts && A.IntValue % B.IntValue == 0 ? FVekaValue::Int(A.IntValue / B.IntValue) : FVekaValue::Float(A.AsFloat() / B.AsFloat());
			case EVekaExprOp::Modulo:
				if (B.AsFloat() == 0.0)
				{
					return FVekaValue::Float(0.0);
				}
				return bInts ? FVekaValue::Int(A.IntValue % B.IntValue) : FVekaValue::Float(FMath::Fmod(A.AsFloat(), B.AsFloat()));
			case EVekaExprOp::Equal: return FVekaValue::Bool(A == B);
			case EVekaExprOp::NotEqual: return FVekaValue::Bool(A != B);
			case EVekaExprOp::Less: return FVekaValue::Bool(A.AsFloat() < B.AsFloat());
			case EVekaExprOp::LessEqual: return FVekaValue::Bool(A.AsFloat() <= B.AsFloat());
			case EVekaExprOp::Greater: return FVekaValue::Bool(A.AsFloat() > B.AsFloat());
			case EVekaExprOp::GreaterEqual: return FVekaValue::Bool(A.AsFloat() >= B.AsFloat());
			case EVekaExprOp::And: return FVekaValue::Bool(A.AsBool() && B.AsBool());
			case EVekaExprOp::Or: return FVekaValue::Bool(A.AsBool() || B.AsBool());
			case EVekaExprOp::Min: return Number(FMath::Min(A.AsFloat(), B.AsFloat()));
			case EVekaExprOp::Max: return Number(FMath::Max(A.AsFloat(), B.AsFloat()));
			case EVekaExprOp::Index: return A.Type == EVekaValueType::Struct ? A.GetField(FName(*B.AsString())) : A.GetItem(B.AsInt());
			default: return FVekaValue();
			}
		}

		/** Elements of a node (If / ForEach give several, flattened by a fragment). */
		FVekaElement Build(int32 NodeIndex, int32 Depth = 0)
		{
			if (!Document.Nodes.IsValidIndex(NodeIndex) || Depth > 64)
			{
				return FVekaElement();
			}
			const FVekaAstNode& Node = Document.Nodes[NodeIndex];
			switch (Node.Kind)
			{
			case EVekaAstNodeKind::If:
			{
				TArray<FVekaElement> Elements;
				const bool bThen = Eval(Node.Expr).AsBool();
				// Ids keep the two branches apart (an insertion transition plays when the branch changes).
				for (const int32 Child : bThen ? Node.Children : Node.ElseChildren)
				{
					Elements.Add(Build(Child, Depth + 1).Id(FName(bThen ? VEKA_FNAME("then") : VEKA_FNAME("else"), Child + 1)));
				}
				return FVekaElement::MakeFragment(MoveTemp(Elements));
			}
			case EVekaAstNodeKind::ForEach:
			{
				TArray<FVekaElement> Elements;
				const FVekaValue Source = Eval(Node.Expr);
				const bool bArray = Source.Type == EVekaValueType::Array;
				const int64 Count = FMath::Clamp<int64>(bArray ? Source.Num() : Source.AsInt(), 0, 1000);
				const TMap<FName, FVekaValue> SavedLocals = Locals;
				for (int64 Item = 0; Item < Count; ++Item)
				{
					Locals.Add(Node.LoopVariable, bArray ? Source.GetItem(Item) : FVekaValue::Int(Item));
					if (!Node.IndexVariable.IsNone())
					{
						Locals.Add(Node.IndexVariable, FVekaValue::Int(Item));
					}
					TArray<FVekaElement> Body;
					for (const int32 Child : Node.Children)
					{
						Body.Add(Build(Child, Depth + 1));
					}
					Elements.Add(FVekaElement::MakeFragment(MoveTemp(Body)).Id(FName(VEKA_FNAME("item"), static_cast<int32>(Item) + 1)));
				}
				Locals = SavedLocals;
				return FVekaElement::MakeFragment(MoveTemp(Elements));
			}
			case EVekaAstNodeKind::Let:
			{
				// Seen by the next views of the block (the parser keeps the name to them).
				Locals.Add(Node.LoopVariable, Eval(Node.Expr));
				return FVekaElement::MakeFragment(TArray<FVekaElement>());
			}
			case EVekaAstNodeKind::Slot:
			{
				const TArray<FVekaElement>* Content = SlotContents.Find(Node.Type);
				return FVekaElement::MakeFragment(Content ? TArray<FVekaElement>(*Content) : TArray<FVekaElement>());
			}
			default:
				break;
			}

			FVekaElement Element = Node.Component ? MakeComponent(Node, Depth) : MakeElement(Node, Depth);
			for (const FVekaAstModifier& Modifier : Node.Modifiers)
			{
				if (Modifier.Name == VEKA_FNAME("style"))
				{
					Element = ApplyStyle(MoveTemp(Element), Modifier);
				}
				else if (Modifier.Name != VEKA_FNAME("on"))
				{
					Element = ApplyModifier(MoveTemp(Element), Modifier);
				}
			}
			return MoveTemp(Element).Source(Origin ? Origin : &Document, NodeIndex);
		}

		/** Runs Actions with the interpreter's scope plus `payload` (component event handlers). */
		TFunction<void(const FVekaValue&)> MakeHandler(const TArray<FVekaAstAction>& Actions) const;
		FVekaElement MakeComponent(const FVekaAstNode& Node, int32 Depth);
		FVekaElement MakeCanvas(const FVekaAstNode& Node);

		/** True if `time` is read in Node's subtree (bInsideCanvas: also inside canvases; else their drawing is skipped). */
		bool UsesTime(int32 NodeIndex, bool bInsideCanvas, int32 Depth = 0) const
		{
			if (!Document.Nodes.IsValidIndex(NodeIndex) || Depth > 64)
			{
				return false;
			}
			const FVekaAstNode& Node = Document.Nodes[NodeIndex];
			if (ExprUsesTime(Node.Expr))
			{
				return true;
			}
			for (const FVekaAstArgument& Argument : Node.Arguments)
			{
				if (ExprUsesTime(Argument.Expr))
				{
					return true;
				}
			}
			for (const FVekaAstModifier& Modifier : Node.Modifiers)
			{
				for (const FVekaAstArgument& Argument : Modifier.Arguments)
				{
					if (ExprUsesTime(Argument.Expr))
					{
						return true;
					}
				}
			}
			if (!bInsideCanvas && Node.Kind == EVekaAstNodeKind::Element && Node.Type == VEKA_FNAME("Canvas"))
			{
				return false;
			}
			for (const int32 Child : Node.Children)
			{
				if (UsesTime(Child, bInsideCanvas, Depth + 1))
				{
					return true;
				}
			}
			for (const int32 Child : Node.ElseChildren)
			{
				if (UsesTime(Child, bInsideCanvas, Depth + 1))
				{
					return true;
				}
			}
			return false;
		}

		bool ExprUsesTime(int32 Index, int32 Depth = 0) const
		{
			if (!Document.Expressions.IsValidIndex(Index) || Depth > 64)
			{
				return false;
			}
			const FVekaAstExpr& Expr = Document.Expressions[Index];
			if (Expr.Kind == EVekaExprKind::Time)
			{
				return true;
			}
			for (const int32 Operand : Expr.Operands)
			{
				if (Operand != Index && ExprUsesTime(Operand, Depth + 1))
				{
					return true;
				}
			}
			return false;
		}

		/** Runs a canvas's draw commands (Dot, Line, Glow, with let / if / ForEach between them). */
		void DrawNodes(const TArray<int32>& Children, FVekaCanvasDrawing& Drawing, TMap<FString, FLinearColor>& ColorCache, int32 Depth = 0)
		{
			if (Depth > 32)
			{
				return;
			}
			for (const int32 Child : Children)
			{
				if (!Document.Nodes.IsValidIndex(Child))
				{
					continue;
				}
				const FVekaAstNode& Node = Document.Nodes[Child];
				switch (Node.Kind)
				{
				case EVekaAstNodeKind::Let:
					Locals.Add(Node.LoopVariable, Eval(Node.Expr));
					break;
				case EVekaAstNodeKind::If:
					DrawNodes(Eval(Node.Expr).AsBool() ? Node.Children : Node.ElseChildren, Drawing, ColorCache, Depth + 1);
					break;
				case EVekaAstNodeKind::ForEach:
				{
					const FVekaValue Source = Eval(Node.Expr);
					const bool bArray = Source.Type == EVekaValueType::Array;
					const int64 Count = FMath::Clamp<int64>(bArray ? Source.Num() : Source.AsInt(), 0, 100000);
					for (int64 Item = 0; Item < Count; ++Item)
					{
						Locals.Add(Node.LoopVariable, bArray ? Source.GetItem(Item) : FVekaValue::Int(Item));
						if (!Node.IndexVariable.IsNone())
						{
							Locals.Add(Node.IndexVariable, FVekaValue::Int(Item));
						}
						DrawNodes(Node.Children, Drawing, ColorCache, Depth + 1);
					}
					break;
				}
				case EVekaAstNodeKind::Element:
					DrawMark(Node, Drawing, ColorCache);
					break;
				default:
					break;
				}
			}
		}

		void DrawMark(const FVekaAstNode& Node, FVekaCanvasDrawing& Drawing, TMap<FString, FLinearColor>& ColorCache)
		{
			auto Find = [&Node](FName Name) -> int32
			{
				for (const FVekaAstArgument& Argument : Node.Arguments)
				{
					if (Argument.Name == Name)
					{
						return Argument.Expr;
					}
				}
				return INDEX_NONE;
			};
			auto Number = [&](FName Name, double Default)
			{
				const int32 Index = Find(Name);
				if (Index == INDEX_NONE)
				{
					return Default;
				}
				FNumber Value;
				return EvalNumber(Index, Value) ? Value.Value : Eval(Index).AsFloat();
			};
			auto Color = [&](FName Name)
			{
				const int32 Index = Find(Name);
				if (Index == INDEX_NONE)
				{
					return FLinearColor::White;
				}
				const FVekaValue Value = Eval(Index);
				if (Value.Type == EVekaValueType::Color)
				{
					return Value.ColorValue;
				}
				// Theme colors and paints resolve once per drawing.
				const FString Key = Value.AsString();
				if (const FLinearColor* Cached = ColorCache.Find(Key))
				{
					return *Cached;
				}
				const FLinearColor Resolved = Veka::Colora::Resolve(PaintFrom(Value), Drawing.GetEnvironment()).GetAverageColor();
				ColorCache.Add(Key, Resolved);
				return Resolved;
			};
			auto WithOpacity = [](FLinearColor Color, double Opacity)
			{
				Color.A *= static_cast<float>(FMath::Clamp(Opacity, 0.0, 1.0));
				return Color;
			};
			const FName Type = Node.Type;
			if (Type == VEKA_FNAME("Dot"))
			{
				Drawing.Dot(FVector2D(Number(VEKA_FNAME("x"), 0.0), Number(VEKA_FNAME("y"), 0.0)), Number(VEKA_FNAME("radius"), 1.0),
					WithOpacity(Color(VEKA_FNAME("color")), Number(VEKA_FNAME("opacity"), 1.0)));
			}
			else if (Type == VEKA_FNAME("Line"))
			{
				const FLinearColor Base = Color(VEKA_FNAME("color"));
				const double Opacity = Number(VEKA_FNAME("opacity"), 1.0);
				Drawing.Line(FVector2D(Number(VEKA_FNAME("x1"), 0.0), Number(VEKA_FNAME("y1"), 0.0)), FVector2D(Number(VEKA_FNAME("x2"), 0.0), Number(VEKA_FNAME("y2"), 0.0)),
					Number(VEKA_FNAME("width"), 1.0), WithOpacity(Base, Number(VEKA_FNAME("fromOpacity"), Opacity)), WithOpacity(Base, Opacity));
			}
			else if (Type == VEKA_FNAME("Glow"))
			{
				Drawing.Glow(FVector2D(Number(VEKA_FNAME("x"), 0.0), Number(VEKA_FNAME("y"), 0.0)), Number(VEKA_FNAME("radius"), 10.0),
					WithOpacity(Color(VEKA_FNAME("color")), Number(VEKA_FNAME("opacity"), 1.0)));
			}
		}

	private:
		FVekaValue Arg(const TArray<FVekaAstArgument>& Arguments, FName Name) const
		{
			for (const FVekaAstArgument& Argument : Arguments)
			{
				if (Argument.Name == Name)
				{
					return Eval(Argument.Expr);
				}
			}
			return FVekaValue();
		}

		bool HasArg(const TArray<FVekaAstArgument>& Arguments, FName Name) const
		{
			return Arguments.ContainsByPredicate([Name](const FVekaAstArgument& Argument) { return Argument.Name == Name; });
		}

		/** State an argument is bound to (`isOn: wifi`): the state expression's name. */
		FName BoundState(const TArray<FVekaAstArgument>& Arguments, FName Name) const
		{
			for (const FVekaAstArgument& Argument : Arguments)
			{
				if (Argument.Name == Name && Document.Expressions.IsValidIndex(Argument.Expr) && Document.Expressions[Argument.Expr].Kind == EVekaExprKind::State)
				{
					return Document.Expressions[Argument.Expr].Name;
				}
			}
			return NAME_None;
		}

		template <typename T>
		TVekaBinding<T> Binding(FName State, TFunction<T(const FVekaValue&)> Read, TFunction<FVekaValue(const T&)> Write, const T& Fallback) const
		{
			if (State.IsNone())
			{
				return TVekaBinding<T>::Constant(Fallback);
			}
			const TSharedRef<FVekaViewStore> InStore = Store;
			return TVekaBinding<T>([InStore, State, Read]() { return Read(InStore->Get(State)); }, [InStore, State, Write](const T& Value) { InStore->Set(State, Write(Value)); });
		}

		TFunction<void()> MakeActions(const TArray<FVekaAstAction>& Actions) const
		{
			if (Actions.IsEmpty())
			{
				return nullptr;
			}
			const TSharedRef<const FVekaDocument> Doc = DocumentRef;
			const TSharedRef<FVekaViewStore> InStore = Store;
			const TMap<FName, FVekaValue> CapturedLocals = Locals;
			const TMap<FName, FVekaValue> CapturedParams = Params;
			const TMap<FName, FVekaTimelineHandle> CapturedTimelines = Timelines;
			return [Doc, InStore, CapturedLocals, CapturedParams, CapturedTimelines, Actions]()
			{
				FInterpreter Runner(Doc, InStore);
				Runner.Locals = CapturedLocals;
				Runner.Params = CapturedParams;
				Runner.Timelines = CapturedTimelines;
				for (const FVekaAstAction& Action : Actions)
				{
					auto Run = [&]()
					{
						switch (Action.Kind)
						{
						case EVekaActionKind::Set: InStore->Set(Action.Target, Runner.Eval(Action.Expr)); break;
						case EVekaActionKind::Toggle: InStore->Set(Action.Target, FVekaValue::Bool(!InStore->Get(Action.Target).AsBool())); break;
						case EVekaActionKind::Increment:
							InStore->Set(Action.Target, Binary(EVekaExprOp::Add, InStore->Get(Action.Target), Action.Expr == INDEX_NONE ? FVekaValue::Int(1) : Runner.Eval(Action.Expr)));
							break;
						case EVekaActionKind::Emit: InStore->Emit(Action.Target, Runner.Eval(Action.Expr)); break;
						default:
							if (const FVekaTimelineHandle* Timeline = Runner.Timelines.Find(Action.Target))
							{
								switch (Action.Kind)
								{
								case EVekaActionKind::Play: Timeline->Play(); break;
								case EVekaActionKind::Pause: Timeline->Pause(); break;
								case EVekaActionKind::Stop: Timeline->Stop(); break;
								case EVekaActionKind::Reverse: Timeline->Reverse(); break;
								case EVekaActionKind::Seek: Timeline->Seek(static_cast<float>(Runner.Eval(Action.Expr).AsFloat())); break;
								default: break;
								}
							}
							break;
						}
					};
					if (Action.bAnimated)
					{
						Veka::WithAnimation(FVekaAnimationSpec::Snappy(), Run);
					}
					else
					{
						Run();
					}
				}
			};
		}

		TArray<FVekaElement> BuildChildren(const FVekaAstNode& Node, int32 Depth)
		{
			TArray<FVekaElement> Children;
			for (const int32 Child : Node.Children)
			{
				Children.Add(Build(Child, Depth + 1));
			}
			return Children;
		}

		FVekaElement Shape(FVekaElement Shape, const FVekaAstNode& Node) const
		{
			return HasArg(Node.Arguments, VEKA_FNAME("fill")) ? MoveTemp(Shape).Fill(PaintFrom(Arg(Node.Arguments, VEKA_FNAME("fill")))) : MoveTemp(Shape);
		}

		FVekaElement MakeElement(const FVekaAstNode& Node, int32 Depth)
		{
			const TArray<FVekaAstArgument>& A = Node.Arguments;
			const FName Type = Node.Type;
			auto Float = [&](FName Name, float Default) { return HasArg(A, Name) ? static_cast<float>(Arg(A, Name).AsFloat()) : Default; };
			auto String = [&](const TCHAR* Name) { return Arg(A, Name).AsText(); };

			if (Type == VEKA_FNAME("VStack"))
			{
				return VStack(Float(VEKA_FNAME("spacing"), 8.f), EnumFrom(Arg(A, VEKA_FNAME("alignment")), EVekaHAlign::Center)).Children(BuildChildren(Node, Depth));
			}
			if (Type == VEKA_FNAME("HStack"))
			{
				return HStack(Float(VEKA_FNAME("spacing"), 8.f), EnumFrom(Arg(A, VEKA_FNAME("alignment")), EVekaVAlign::Center)).Children(BuildChildren(Node, Depth));
			}
			if (Type == VEKA_FNAME("ZStack"))
			{
				return ZStack().Children(BuildChildren(Node, Depth));
			}
			if (Type == VEKA_FNAME("Text"))
			{
				return Text(String(TEXT("text")));
			}
			if (Type == VEKA_FNAME("Symbol"))
			{
				return Symbol(FName(*Arg(A, VEKA_FNAME("name")).AsString()), Float(VEKA_FNAME("value"), -1.f));
			}
			if (Type == VEKA_FNAME("Label"))
			{
				return Label(String(TEXT("title")), FName(*Arg(A, VEKA_FNAME("symbol")).AsString()));
			}
			if (Type == VEKA_FNAME("Button"))
			{
				TFunction<void()> Action = MakeActions(Node.Actions);
				const EVekaButtonRole Role = EnumFrom(Arg(A, VEKA_FNAME("role")), EVekaButtonRole::Normal);
				TArray<FVekaElement> Children = BuildChildren(Node, Depth);
				if (!Children.IsEmpty())
				{
					return Button(Children[0], MoveTemp(Action), Role);
				}
				return HasArg(A, VEKA_FNAME("symbol"))
					? Button(String(TEXT("title")), FName(*Arg(A, VEKA_FNAME("symbol")).AsString()), MoveTemp(Action), Role)
					: Button(String(TEXT("title")), MoveTemp(Action), Role);
			}
			if (Type == VEKA_FNAME("Toggle"))
			{
				return Toggle(String(TEXT("title")), Binding<bool>(BoundState(A, TEXT("isOn")), [](const FVekaValue& V) { return V.AsBool(); },
					[](const bool& V) { return FVekaValue::Bool(V); }, Arg(A, VEKA_FNAME("isOn")).AsBool()));
			}
			if (Type == VEKA_FNAME("Slider"))
			{
				return Slider(Binding<float>(BoundState(A, TEXT("value")), [](const FVekaValue& V) { return static_cast<float>(V.AsFloat()); },
					[](const float& V) { return FVekaValue::Float(V); }, static_cast<float>(Arg(A, VEKA_FNAME("value")).AsFloat())),
					Float(VEKA_FNAME("min"), 0.f), Float(VEKA_FNAME("max"), 1.f), Float(VEKA_FNAME("step"), 0.f));
			}
			if (Type == VEKA_FNAME("Stepper"))
			{
				const FName State = BoundState(A, TEXT("value"));
				const bool bInt = Store->Get(State).Type == EVekaValueType::Int;
				return Stepper(String(TEXT("title")), Binding<float>(State, [](const FVekaValue& V) { return static_cast<float>(V.AsFloat()); },
					[bInt](const float& V) { return bInt ? FVekaValue::Int(FMath::RoundToInt64(V)) : FVekaValue::Float(V); }, static_cast<float>(Arg(A, VEKA_FNAME("value")).AsFloat())),
					Float(VEKA_FNAME("min"), 0.f), Float(VEKA_FNAME("max"), 10.f), Float(VEKA_FNAME("step"), 1.f), static_cast<int32>(Float(VEKA_FNAME("decimals"), 0.f)));
			}
			if (Type == VEKA_FNAME("TextField"))
			{
				return TextField(String(TEXT("placeholder")), Binding<FString>(BoundState(A, TEXT("text")), [](const FVekaValue& V) { return V.AsString(); },
					[](const FString& V) { return FVekaValue::String(V); }, Arg(A, VEKA_FNAME("text")).AsString()));
			}
			if (Type == VEKA_FNAME("ProgressView"))
			{
				return ProgressView(Float(VEKA_FNAME("value"), -1.f), String(TEXT("label")));
			}
			if (Type == VEKA_FNAME("Gauge"))
			{
				return Gauge(Float(VEKA_FNAME("value"), 0.f), Float(VEKA_FNAME("min"), 0.f), Float(VEKA_FNAME("max"), 1.f), String(TEXT("label")), String(TEXT("current")));
			}
			if (Type == VEKA_FNAME("Divider"))
			{
				return Divider();
			}
			if (Type == VEKA_FNAME("Spacer"))
			{
				return Spacer(Float(VEKA_FNAME("min"), 8.f));
			}
			if (Type == VEKA_FNAME("Rectangle"))
			{
				return Shape(Rectangle(), Node);
			}
			if (Type == VEKA_FNAME("RoundedRectangle"))
			{
				return Shape(RoundedRectangle(Float(VEKA_FNAME("radius"), 8.f)), Node);
			}
			if (Type == VEKA_FNAME("Capsule"))
			{
				return Shape(Capsule(), Node);
			}
			if (Type == VEKA_FNAME("Circle"))
			{
				return Shape(Circle(), Node);
			}
			if (Type == VEKA_FNAME("Canvas"))
			{
				return MakeCanvas(Node);
			}
			if (Type == VEKA_FNAME("Dot") || Type == VEKA_FNAME("Line") || Type == VEKA_FNAME("Glow"))
			{
				UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: %s draws only inside a Canvas"), *Type.ToString());
				return FVekaElement();
			}
			if (Type == VEKA_FNAME("Ellipse"))
			{
				return Shape(Ellipse(), Node);
			}

			// Any primitive of the schema (Veka's or the user's), properties set by reflection.
			if (const FVekaSchemaEntry* Entry = FVekaSchema::Get().Find(Type, EVekaSchemaKind::Primitive))
			{
				FInstancedStruct Instance;
				Instance.InitializeAs(Entry->Struct);
				TArray<TPair<FName, FVekaValue>> Values;
				for (const FVekaAstArgument& Argument : A)
				{
					Values.Emplace(Argument.Name, Eval(Argument.Expr));
				}
				ApplyArguments(Entry->Struct, Instance.GetMutableMemory(), Values);
				return FVekaElement::MakePrimitive(MoveTemp(Instance), BuildChildren(Node, Depth));
			}

			UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: unknown component '%s'"), *Type.ToString());
			return Text(FText::FromString(FString::Printf(TEXT("Unknown '%s'"), *Type.ToString()))).Foreground(FLinearColor(1.f, 0.25f, 0.25f));
		}

		/** `.style(.Name)`: the modifiers of a shared style (UVekaComponentLibrary), evaluated in the style's own document. */
		FVekaElement ApplyStyle(FVekaElement Element, const FVekaAstModifier& Modifier) const
		{
			const FName StyleName(*Arg(Modifier.Arguments, VEKA_FNAME("name")).AsString());
			const FVekaSharedStyle* Style = Veka::Components::FindStyle(StyleName);
			if (!Style)
			{
				UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: unknown style '%s'"), *StyleName.ToString());
				return Element;
			}
			if (!Style->Compiled.Nodes.IsValidIndex(Style->Compiled.Root) || StyleDepth > 8)
			{
				return Element;
			}
			FInterpreter Inner(MakeShared<const FVekaDocument>(Style->Compiled), Store);
			Inner.StyleDepth = StyleDepth + 1;
			for (const FVekaAstModifier& Applied : Style->Compiled.Nodes[Style->Compiled.Root].Modifiers)
			{
				Element = Applied.Name == VEKA_FNAME("style") ? Inner.ApplyStyle(MoveTemp(Element), Applied) : Inner.ApplyModifier(MoveTemp(Element), Applied);
			}
			return Element;
		}

		FVekaElement ApplyModifier(FVekaElement Element, const FVekaAstModifier& Modifier) const
		{
			const TArray<FVekaAstArgument>& A = Modifier.Arguments;
			const FName Name = Modifier.Name;
			auto Float = [&](FName ArgName, double Default) { return HasArg(A, ArgName) ? Arg(A, ArgName).AsFloat() : Default; };
			auto Value = [&](FName ArgName) { return Arg(A, ArgName); };

			if (Name == VEKA_FNAME("onTap"))
			{
				TFunction<void()> Action = MakeActions(Modifier.Actions);
				return Action ? MoveTemp(Element).OnTap(MoveTemp(Action)) : MoveTemp(Element);
			}
			if (Name == VEKA_FNAME("padding"))
			{
				if (HasArg(A, VEKA_FNAME("horizontal")) || HasArg(A, VEKA_FNAME("vertical")))
				{
					return MoveTemp(Element).Padding(FMargin(static_cast<float>(Float(VEKA_FNAME("horizontal"), 0.0)), static_cast<float>(Float(VEKA_FNAME("vertical"), 0.0))));
				}
				return MoveTemp(Element).Padding(static_cast<float>(Float(VEKA_FNAME("value"), 16.0)));
			}
			if (Name == VEKA_FNAME("frame"))
			{
				if (Value(VEKA_FNAME("fillWidth")).AsBool())
				{
					return MoveTemp(Element).FillWidth(EnumFrom(Value(VEKA_FNAME("alignment")), EVekaHAlign::Leading) == EVekaHAlign::Leading ? FVekaAlignment::Leading() : FVekaAlignment());
				}
				return MoveTemp(Element).Frame(Float(VEKA_FNAME("width"), -1.0), Float(VEKA_FNAME("height"), -1.0));
			}
			if (Name == VEKA_FNAME("background"))
			{
				return MoveTemp(Element).Background(PaintFrom(Value(VEKA_FNAME("paint"))), FVekaCornerRadii(static_cast<float>(Float(VEKA_FNAME("radius"), 0.0))));
			}
			if (Name == VEKA_FNAME("foreground"))
			{
				return MoveTemp(Element).Foreground(PaintFrom(Value(VEKA_FNAME("paint"))));
			}
			if (Name == VEKA_FNAME("border"))
			{
				return MoveTemp(Element).Border(PaintFrom(Value(VEKA_FNAME("paint"))), static_cast<float>(Float(VEKA_FNAME("width"), 1.0)), FVekaCornerRadii(static_cast<float>(Float(VEKA_FNAME("radius"), 0.0))));
			}
			if (Name == VEKA_FNAME("backdropBlur") || Name == VEKA_FNAME("frostedGlass"))
			{
				const bool bFrosted = Name == VEKA_FNAME("frostedGlass");
				FVekaBackdropModifier Backdrop = bFrosted ? FVekaBackdropModifier::FrostedGlass() : FVekaBackdropModifier();
				Backdrop.Radius = static_cast<float>(Float(bFrosted ? TEXT("blur") : TEXT("radius"), Backdrop.Radius));
				Backdrop.CornerRadii = FVekaCornerRadii(static_cast<float>(Float(VEKA_FNAME("cornerRadius"), 0.0)));
				if (HasArg(A, VEKA_FNAME("tint")))
				{
					Backdrop.Tint = PaintFrom(Value(VEKA_FNAME("tint")));
				}
				Backdrop.TintAmount = static_cast<float>(Float(VEKA_FNAME("tintAmount"), Backdrop.TintAmount));
				Backdrop.Saturation = static_cast<float>(Float(VEKA_FNAME("saturation"), Backdrop.Saturation));
				Backdrop.Noise = static_cast<float>(Float(VEKA_FNAME("noise"), Backdrop.Noise));
				return MoveTemp(Element).AddModifier(FInstancedStruct::Make(Backdrop));
			}
			if (Name == VEKA_FNAME("blendMode"))
			{
				FVekaBlendModeModifier Blend;
				Blend.Mode = EnumFrom(Value(VEKA_FNAME("mode")), EVekaBlendMode::Normal);
				return MoveTemp(Element).AddModifier(FInstancedStruct::Make(Blend));
			}
			if (Name == VEKA_FNAME("liquidDistortion"))
			{
				FVekaLiquidDistortionModifier Distortion;
				Distortion.Strength = static_cast<float>(Float(VEKA_FNAME("strength"), Distortion.Strength));
				Distortion.Scale = static_cast<float>(Float(VEKA_FNAME("scale"), Distortion.Scale));
				Distortion.Speed = static_cast<float>(Float(VEKA_FNAME("speed"), Distortion.Speed));
				Distortion.CornerRadii = FVekaCornerRadii(static_cast<float>(Float(VEKA_FNAME("cornerRadius"), 0.0)));
				return MoveTemp(Element).AddModifier(FInstancedStruct::Make(Distortion));
			}
			if (Name == VEKA_FNAME("glass") || Name == VEKA_FNAME("glassGroup"))
			{
				FVekaGlassMaterial Material;
				Material.Blur = static_cast<float>(Float(VEKA_FNAME("blur"), Material.Blur));
				Material.Refraction = static_cast<float>(Float(VEKA_FNAME("refraction"), Material.Refraction));
				Material.Bezel = static_cast<float>(Float(VEKA_FNAME("bezel"), Material.Bezel));
				Material.Aberration = static_cast<float>(Float(VEKA_FNAME("aberration"), Material.Aberration));
				Material.Highlight = static_cast<float>(Float(VEKA_FNAME("highlight"), Material.Highlight));
				if (HasArg(A, VEKA_FNAME("tint")))
				{
					Material.Tint = PaintFrom(Value(VEKA_FNAME("tint")));
				}
				Material.TintAmount = static_cast<float>(Float(VEKA_FNAME("tintAmount"), Material.TintAmount));
				if (Name == VEKA_FNAME("glass"))
				{
					FVekaGlassModifier Glass;
					Glass.CornerRadii = FVekaCornerRadii(static_cast<float>(Float(VEKA_FNAME("cornerRadius"), 0.0)));
					Glass.Material = Material;
					return MoveTemp(Element).AddModifier(FInstancedStruct::Make(Glass));
				}
				FVekaGlassGroupModifier Group;
				Group.Smoothness = static_cast<float>(Float(VEKA_FNAME("smoothness"), Group.Smoothness));
				Group.Material = Material;
				return MoveTemp(Element).AddModifier(FInstancedStruct::Make(Group));
			}
			if (Name == VEKA_FNAME("opacity"))
			{
				return MoveTemp(Element).Opacity(static_cast<float>(Float(VEKA_FNAME("value"), 1.0)));
			}
			if (Name == VEKA_FNAME("clip"))
			{
				return MoveTemp(Element).Clip();
			}
			if (Name == VEKA_FNAME("offset"))
			{
				return MoveTemp(Element).Offset(FVector2D(Float(VEKA_FNAME("x"), 0.0), Float(VEKA_FNAME("y"), 0.0)));
			}
			if (Name == VEKA_FNAME("scale"))
			{
				return MoveTemp(Element).Scale(static_cast<float>(Float(VEKA_FNAME("value"), 1.0)));
			}
			if (Name == VEKA_FNAME("rotation"))
			{
				return MoveTemp(Element).Rotation(static_cast<float>(Float(VEKA_FNAME("degrees"), 0.0)));
			}
			if (Name == VEKA_FNAME("font"))
			{
				return MoveTemp(Element).Font(EnumFrom(Value(VEKA_FNAME("style")), EVekaTextStyle::Body));
			}
			if (Name == VEKA_FNAME("fontWeight"))
			{
				return MoveTemp(Element).FontWeight(EnumFrom(Value(VEKA_FNAME("weight")), EVekaFontWeight::Regular));
			}
			if (Name == VEKA_FNAME("fontSize"))
			{
				return MoveTemp(Element).FontSize(static_cast<float>(Float(VEKA_FNAME("size"), 14.0)));
			}
			if (Name == VEKA_FNAME("disabled"))
			{
				return MoveTemp(Element).Disabled(HasArg(A, VEKA_FNAME("value")) ? Value(VEKA_FNAME("value")).AsBool() : true);
			}
			if (Name == VEKA_FNAME("buttonStyle"))
			{
				return MoveTemp(Element).ButtonStyle(EnumFrom(Value(VEKA_FNAME("style")), EVekaButtonStyleKind::Bordered));
			}
			if (Name == VEKA_FNAME("toggleStyle"))
			{
				return MoveTemp(Element).ToggleStyle(EnumFrom(Value(VEKA_FNAME("style")), EVekaToggleStyleKind::Switch));
			}
			if (Name == VEKA_FNAME("controlSize"))
			{
				return MoveTemp(Element).ControlSize(EnumFrom(Value(VEKA_FNAME("size")), EVekaControlSize::Regular));
			}
			if (Name == VEKA_FNAME("symbolRenderingMode"))
			{
				return MoveTemp(Element).SymbolRenderingMode(EnumFrom(Value(VEKA_FNAME("mode")), EVekaSymbolRenderingMode::Monochrome));
			}
			if (Name == VEKA_FNAME("animation"))
			{
				return MoveTemp(Element).AnimationKey(FVekaAnimationSpec::Snappy(), static_cast<int64>(GetTypeHash(Value(VEKA_FNAME("value")))));
			}
			if (Name == VEKA_FNAME("transition"))
			{
				const FString Kind = Value(VEKA_FNAME("kind")).AsString();
				const FVekaTransition Transition = Kind == TEXT("scale") ? FVekaTransition::Combined(FVekaTransition::Opacity(), FVekaTransition::Scale(0.6f))
					: Kind == TEXT("slide") ? FVekaTransition::Slide()
					// From (and back to) above, fading: a card that drops in.
					: Kind == TEXT("move") ? FVekaTransition::Combined(FVekaTransition::Opacity(), FVekaTransition::Move(EVekaEdge::Top))
					// Comes into focus: blurred, slightly smaller and transparent at first.
					: Kind == TEXT("blur") ? FVekaTransition::Combined(FVekaTransition::Opacity(), FVekaTransition::Combined(FVekaTransition::Blur(14.f), FVekaTransition::Scale(0.9f)))
					: FVekaTransition::Opacity();
				return MoveTemp(Element).Transition(Transition);
			}

			if (const FVekaSchemaEntry* Entry = FVekaSchema::Get().Find(Name, EVekaSchemaKind::Modifier))
			{
				FInstancedStruct Instance;
				Instance.InitializeAs(Entry->Struct);
				TArray<TPair<FName, FVekaValue>> Values;
				for (const FVekaAstArgument& Argument : A)
				{
					Values.Emplace(Argument.Name, Eval(Argument.Expr));
				}
				ApplyArguments(Entry->Struct, Instance.GetMutableMemory(), Values);
				return MoveTemp(Element).AddModifier(MoveTemp(Instance));
			}
			UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: unknown modifier '%s'"), *Name.ToString());
			return Element;
		}

		TSharedRef<const FVekaDocument> DocumentRef;
		const FVekaDocument& Document;
		TSharedRef<FVekaViewStore> Store;
	};

	/** Handlers of a component's events, refreshed at every evaluation of the caller. */
	struct FComponentHandlers
	{
		TMap<FName, TFunction<void(const FVekaValue&)>> ByEvent;
		TWeakPtr<FVekaViewStore> Parent;
	};

	/**
	 * A view used as a component: its own store (its states), the caller's parameters, slots and handlers. Events
	 * without a handler go up to the caller's store (so to the host).
	 */
	class FVekaComponentView : public FVekaView
	{
		VEKA_VIEW(FVekaComponentView)

	public:
		TObjectPtr<UVekaView> Asset;
		TSharedPtr<const FVekaDocument> Document;
		/** The component asset's document (tag of its elements). */
		const void* Origin = nullptr;
		TMap<FName, FVekaValue> Params;
		TMap<FName, TArray<FVekaElement>> SlotContents;
		TMap<FName, TFunction<void(const FVekaValue&)>> Handlers;
		TSharedPtr<FVekaViewStore> ParentStore;
		int32 Depth = 0;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Asset);
		}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			// Kept in the node across evaluations (the initial value is used the first time only).
			const TSharedPtr<FComponentHandlers> Shared = Ctx.State<TSharedPtr<FComponentHandlers>>(TEXT("Handlers"), MakeShared<FComponentHandlers>()).Get();
			Shared->ByEvent = Handlers;
			Shared->Parent = ParentStore;

			const TSharedPtr<FVekaViewStore> Store = Ctx.State<TSharedPtr<FVekaViewStore>>(TEXT("Store"), MakeShared<FVekaViewStore>()).Get();
			if (!Store->OnEvent)
			{
				for (const FVekaStateDeclaration& State : Document->States)
				{
					Store->Declare(State.Name, State.Initial);
				}
				const TWeakPtr<FComponentHandlers> WeakHandlers = Shared;
				Store->OnEvent = [WeakHandlers](FName Event, const FVekaValue& Payload)
				{
					const TSharedPtr<FComponentHandlers> Pinned = WeakHandlers.Pin();
					if (!Pinned.IsValid())
					{
						return;
					}
					if (const TFunction<void(const FVekaValue&)>* Handler = Pinned->ByEvent.Find(Event))
					{
						(*Handler)(Payload);
					}
					else if (const TSharedPtr<FVekaViewStore> Parent = Pinned->Parent.Pin())
					{
						Parent->Emit(Event, Payload);
					}
				};
			}
			UVekaViewModel* Model = ParentStore.IsValid() ? ParentStore->GetModel() : nullptr;
			if (Store->GetModel() != Model)
			{
				Store->SetModelSilently(Model);
			}
			Store->Attach(Ctx.GetNode());

			FInterpreter Interpreter(Document.ToSharedRef(), Store.ToSharedRef());
			Interpreter.Origin = Origin;
			Interpreter.Params = Params;
			Interpreter.SlotContents = SlotContents;
			Interpreter.ComponentDepth = Depth;
			Interpreter.SetupTimelines(Ctx);
			return Interpreter.Build(Document->Root);
		}
	};

	/** A Canvas block of a view: its draw commands, run by a copy of the interpreter's scope each time it paints. */
	class FInterpretedCanvasPainter : public FVekaCanvasPainter
	{
	public:
		TSharedPtr<const FVekaDocument> Document;
		TSharedPtr<FVekaViewStore> Store;
		TMap<FName, FVekaValue> Params;
		TMap<FName, FVekaValue> Locals;
		TMap<FName, FVekaTimelineHandle> Timelines;
		TArray<int32> Children;
		bool bAnimated = false;

		virtual bool IsAnimated() const override { return bAnimated; }

		virtual void Draw(FVekaCanvasDrawing& Drawing) const override
		{
			if (!Document.IsValid() || !Store.IsValid())
			{
				return;
			}
			FInterpreter Interpreter(Document.ToSharedRef(), Store.ToSharedRef());
			Interpreter.Params = Params;
			Interpreter.Locals = Locals;
			Interpreter.Timelines = Timelines;
			Interpreter.TimeOverride = FPlatformTime::Seconds() - Store->StartSeconds;
			const FVector2D Size = Drawing.GetSize();
			Interpreter.Locals.Add(TEXT("width"), FVekaValue::Float(Size.X));
			Interpreter.Locals.Add(TEXT("height"), FVekaValue::Float(Size.Y));
			TMap<FString, FLinearColor> ColorCache;
			Interpreter.DrawNodes(Children, Drawing, ColorCache);
		}
	};

	FVekaElement FInterpreter::MakeCanvas(const FVekaAstNode& Node)
	{
		static int32 Revision = 0;
		const int32 NodeIndex = static_cast<int32>(&Node - Document.Nodes.GetData());
		TSharedRef<FInterpretedCanvasPainter> Painter = MakeShared<FInterpretedCanvasPainter>();
		Painter->Document = DocumentRef;
		Painter->Store = Store;
		Painter->Params = Params;
		Painter->Locals = Locals;
		Painter->Timelines = Timelines;
		Painter->Children = Node.Children;
		Painter->bAnimated = UsesTime(NodeIndex, true);
		FVekaCanvas Canvas;
		Canvas.bAdditive = HasArg(Node.Arguments, VEKA_FNAME("blend")) && Arg(Node.Arguments, VEKA_FNAME("blend")).AsString() == TEXT("Add");
		Canvas.PainterRevision = ++Revision;
		Canvas.Painter = Painter;
		return FVekaElement::Make(Canvas);
	}

	TFunction<void(const FVekaValue&)> FInterpreter::MakeHandler(const TArray<FVekaAstAction>& Actions) const
	{
		const TSharedRef<const FVekaDocument> Doc = DocumentRef;
		const TSharedRef<FVekaViewStore> InStore = Store;
		const TMap<FName, FVekaValue> CapturedLocals = Locals;
		const TMap<FName, FVekaValue> CapturedParams = Params;
		const TMap<FName, FVekaTimelineHandle> CapturedTimelines = Timelines;
		return [Doc, InStore, CapturedLocals, CapturedParams, CapturedTimelines, Actions](const FVekaValue& Payload)
		{
			FInterpreter Runner(Doc, InStore);
			Runner.Locals = CapturedLocals;
			Runner.Locals.Add(TEXT("payload"), Payload);
			Runner.Params = CapturedParams;
			Runner.Timelines = CapturedTimelines;
			if (TFunction<void()> Run = Runner.MakeActions(Actions))
			{
				Run();
			}
		};
	}

	FVekaElement FInterpreter::MakeComponent(const FVekaAstNode& Node, int32 Depth)
	{
		UVekaView* Asset = Node.Component.Get();
		if (ComponentDepth >= 16)
		{
			UE_LOG(LogVekaUI, Warning, TEXT("VekaUI view: components nested more than 16 deep (%s uses itself?)"), *Asset->GetName());
			return Text(FText::FromString(FString::Printf(TEXT("Recursive '%s'"), *Asset->GetName()))).Foreground(FLinearColor(1.f, 0.25f, 0.25f));
		}
		TSharedRef<FVekaComponentView> View = MakeShared<FVekaComponentView>();
		View->Asset = Asset;
		FVekaDocument Copy = Asset->Document;
		Copy.Upgrade();
		View->Document = MakeShared<const FVekaDocument>(MoveTemp(Copy));
		View->Origin = &Asset->Document;
		for (const FVekaParamDeclaration& Param : View->Document->Params)
		{
			View->Params.Add(Param.Name, Param.Default);
		}
		for (const FVekaAstArgument& Argument : Node.Arguments)
		{
			View->Params.Add(Argument.Name, Eval(Argument.Expr));
		}
		for (const int32 Child : Node.Children)
		{
			if (Document.Nodes.IsValidIndex(Child))
			{
				View->SlotContents.FindOrAdd(Document.Nodes[Child].SlotName).Add(Build(Child, Depth + 1));
			}
		}
		for (const FVekaAstModifier& Modifier : Node.Modifiers)
		{
			if (Modifier.Name == VEKA_FNAME("on"))
			{
				View->Handlers.Add(FName(*Arg(Modifier.Arguments, VEKA_FNAME("event")).AsString()), MakeHandler(Modifier.Actions));
			}
		}
		View->ParentStore = Store;
		View->Depth = ComponentDepth + 1;
		return FVekaElement::MakeView(View);
	}

	/** The interpreted view: evaluated like a C++ view, its state lives in the store. */
	class FVekaInterpretedView : public FVekaView
	{
		VEKA_VIEW(FVekaInterpretedView)

	public:
		FVekaInterpretedView(const TSharedRef<const FVekaDocument>& InDocument, const TSharedRef<FVekaViewStore>& InStore, const void* InOrigin)
			: Document(InDocument), Store(InStore), Origin(InOrigin) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			Store->Attach(Ctx.GetNode());
			FInterpreter Interpreter(Document, Store);
			Interpreter.Origin = Origin;
			Interpreter.SetupTimelines(Ctx);
			return Interpreter.Build(Document->Root);
		}

	private:
		TSharedRef<const FVekaDocument> Document;
		TSharedRef<FVekaViewStore> Store;
		const void* Origin = nullptr;
	};
}

namespace Veka::Interpreter
{
	FVekaElement MakeView(const FVekaDocument& Document, const TSharedRef<FVekaViewStore>& Store)
	{
		FVekaDocument Copy = Document;
		Copy.Upgrade();
		for (const FVekaStateDeclaration& State : Copy.States)
		{
			Store->Declare(State.Name, State.Initial);
		}
		return FVekaElement::MakeView(MakeShared<FVekaInterpretedView>(MakeShared<const FVekaDocument>(MoveTemp(Copy)), Store, &Document));
	}
}

FVekaElement UVekaView::BuildRoot(const TSharedRef<FVekaViewStore>& Store) const
{
	return Veka::Interpreter::MakeView(Document, Store);
}

void UVekaView::PostLoad()
{
	Super::PostLoad();
	Document.Upgrade();
}

FVekaElement UVekaView::BuildRoot() const
{
	return BuildRoot(MakeShared<FVekaViewStore>());
}
