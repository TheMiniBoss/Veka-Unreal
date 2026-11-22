// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaViewAsset.h"
#include "View/VekaDocument.h"
#include "View/VekaComponentLibrary.h"
#include "Modifiers/VekaEffectModifiers.h"
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

		/**
		 * Players of the document's timelines, kept by the view node: cues become events that run their actions, autoplay
		 * starts once per store, and the store lists them (the editor's Timeline tab).
		 */
		void SetupTimelines(FVekaContext& Ctx)
		{
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

		FVekaValue Eval(int32 Index) const
		{
			if (!Document.Expressions.IsValidIndex(Index))
			{
				return FVekaValue();
			}
			const FVekaAstExpr& Expr = Document.Expressions[Index];
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
			case EVekaExprKind::Member: return Operand(0).GetField(Expr.Name);
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
					Elements.Add(Build(Child, Depth + 1).Id(FName(*FString::Printf(TEXT("%s%d"), bThen ? TEXT("then") : TEXT("else"), Child))));
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
					Elements.Add(FVekaElement::MakeFragment(MoveTemp(Body)).Id(FName(*FString::Printf(TEXT("item%lld"), Item))));
				}
				Locals = SavedLocals;
				return FVekaElement::MakeFragment(MoveTemp(Elements));
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
				if (Modifier.Name == TEXT("style"))
				{
					Element = ApplyStyle(MoveTemp(Element), Modifier);
				}
				else if (Modifier.Name != TEXT("on"))
				{
					Element = ApplyModifier(MoveTemp(Element), Modifier);
				}
			}
			return MoveTemp(Element).Source(Origin ? Origin : &Document, NodeIndex);
		}

		/** Runs Actions with the interpreter's scope plus `payload` (component event handlers). */
		TFunction<void(const FVekaValue&)> MakeHandler(const TArray<FVekaAstAction>& Actions) const;
		FVekaElement MakeComponent(const FVekaAstNode& Node, int32 Depth);

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
			return HasArg(Node.Arguments, TEXT("fill")) ? MoveTemp(Shape).Fill(PaintFrom(Arg(Node.Arguments, TEXT("fill")))) : MoveTemp(Shape);
		}

		FVekaElement MakeElement(const FVekaAstNode& Node, int32 Depth)
		{
			const TArray<FVekaAstArgument>& A = Node.Arguments;
			const FName Type = Node.Type;
			auto Float = [&](const TCHAR* Name, float Default) { return HasArg(A, Name) ? static_cast<float>(Arg(A, Name).AsFloat()) : Default; };
			auto String = [&](const TCHAR* Name) { return Arg(A, Name).AsText(); };

			if (Type == TEXT("VStack"))
			{
				return VStack(Float(TEXT("spacing"), 8.f), EnumFrom(Arg(A, TEXT("alignment")), EVekaHAlign::Center)).Children(BuildChildren(Node, Depth));
			}
			if (Type == TEXT("HStack"))
			{
				return HStack(Float(TEXT("spacing"), 8.f), EnumFrom(Arg(A, TEXT("alignment")), EVekaVAlign::Center)).Children(BuildChildren(Node, Depth));
			}
			if (Type == TEXT("ZStack"))
			{
				return ZStack().Children(BuildChildren(Node, Depth));
			}
			if (Type == TEXT("Text"))
			{
				return Text(String(TEXT("text")));
			}
			if (Type == TEXT("Symbol"))
			{
				return Symbol(FName(*Arg(A, TEXT("name")).AsString()), Float(TEXT("value"), -1.f));
			}
			if (Type == TEXT("Label"))
			{
				return Label(String(TEXT("title")), FName(*Arg(A, TEXT("symbol")).AsString()));
			}
			if (Type == TEXT("Button"))
			{
				TFunction<void()> Action = MakeActions(Node.Actions);
				const EVekaButtonRole Role = EnumFrom(Arg(A, TEXT("role")), EVekaButtonRole::Normal);
				TArray<FVekaElement> Children = BuildChildren(Node, Depth);
				if (!Children.IsEmpty())
				{
					return Button(Children[0], MoveTemp(Action), Role);
				}
				return HasArg(A, TEXT("symbol"))
					? Button(String(TEXT("title")), FName(*Arg(A, TEXT("symbol")).AsString()), MoveTemp(Action), Role)
					: Button(String(TEXT("title")), MoveTemp(Action), Role);
			}
			if (Type == TEXT("Toggle"))
			{
				return Toggle(String(TEXT("title")), Binding<bool>(BoundState(A, TEXT("isOn")), [](const FVekaValue& V) { return V.AsBool(); },
					[](const bool& V) { return FVekaValue::Bool(V); }, Arg(A, TEXT("isOn")).AsBool()));
			}
			if (Type == TEXT("Slider"))
			{
				return Slider(Binding<float>(BoundState(A, TEXT("value")), [](const FVekaValue& V) { return static_cast<float>(V.AsFloat()); },
					[](const float& V) { return FVekaValue::Float(V); }, static_cast<float>(Arg(A, TEXT("value")).AsFloat())),
					Float(TEXT("min"), 0.f), Float(TEXT("max"), 1.f), Float(TEXT("step"), 0.f));
			}
			if (Type == TEXT("Stepper"))
			{
				const FName State = BoundState(A, TEXT("value"));
				const bool bInt = Store->Get(State).Type == EVekaValueType::Int;
				return Stepper(String(TEXT("title")), Binding<float>(State, [](const FVekaValue& V) { return static_cast<float>(V.AsFloat()); },
					[bInt](const float& V) { return bInt ? FVekaValue::Int(FMath::RoundToInt64(V)) : FVekaValue::Float(V); }, static_cast<float>(Arg(A, TEXT("value")).AsFloat())),
					Float(TEXT("min"), 0.f), Float(TEXT("max"), 10.f), Float(TEXT("step"), 1.f), static_cast<int32>(Float(TEXT("decimals"), 0.f)));
			}
			if (Type == TEXT("TextField"))
			{
				return TextField(String(TEXT("placeholder")), Binding<FString>(BoundState(A, TEXT("text")), [](const FVekaValue& V) { return V.AsString(); },
					[](const FString& V) { return FVekaValue::String(V); }, Arg(A, TEXT("text")).AsString()));
			}
			if (Type == TEXT("ProgressView"))
			{
				return ProgressView(Float(TEXT("value"), -1.f), String(TEXT("label")));
			}
			if (Type == TEXT("Gauge"))
			{
				return Gauge(Float(TEXT("value"), 0.f), Float(TEXT("min"), 0.f), Float(TEXT("max"), 1.f), String(TEXT("label")), String(TEXT("current")));
			}
			if (Type == TEXT("Divider"))
			{
				return Divider();
			}
			if (Type == TEXT("Spacer"))
			{
				return Spacer(Float(TEXT("min"), 8.f));
			}
			if (Type == TEXT("Rectangle"))
			{
				return Shape(Rectangle(), Node);
			}
			if (Type == TEXT("RoundedRectangle"))
			{
				return Shape(RoundedRectangle(Float(TEXT("radius"), 8.f)), Node);
			}
			if (Type == TEXT("Capsule"))
			{
				return Shape(Capsule(), Node);
			}
			if (Type == TEXT("Circle"))
			{
				return Shape(Circle(), Node);
			}
			if (Type == TEXT("Ellipse"))
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
			const FName StyleName(*Arg(Modifier.Arguments, TEXT("name")).AsString());
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
				Element = Applied.Name == TEXT("style") ? Inner.ApplyStyle(MoveTemp(Element), Applied) : Inner.ApplyModifier(MoveTemp(Element), Applied);
			}
			return Element;
		}

		FVekaElement ApplyModifier(FVekaElement Element, const FVekaAstModifier& Modifier) const
		{
			const TArray<FVekaAstArgument>& A = Modifier.Arguments;
			const FName Name = Modifier.Name;
			auto Float = [&](const TCHAR* ArgName, double Default) { return HasArg(A, ArgName) ? Arg(A, ArgName).AsFloat() : Default; };
			auto Value = [&](const TCHAR* ArgName) { return Arg(A, ArgName); };

			if (Name == TEXT("onTap"))
			{
				TFunction<void()> Action = MakeActions(Modifier.Actions);
				return Action ? MoveTemp(Element).OnTap(MoveTemp(Action)) : MoveTemp(Element);
			}
			if (Name == TEXT("padding"))
			{
				if (HasArg(A, TEXT("horizontal")) || HasArg(A, TEXT("vertical")))
				{
					return MoveTemp(Element).Padding(FMargin(static_cast<float>(Float(TEXT("horizontal"), 0.0)), static_cast<float>(Float(TEXT("vertical"), 0.0))));
				}
				return MoveTemp(Element).Padding(static_cast<float>(Float(TEXT("value"), 16.0)));
			}
			if (Name == TEXT("frame"))
			{
				if (Value(TEXT("fillWidth")).AsBool())
				{
					return MoveTemp(Element).FillWidth(EnumFrom(Value(TEXT("alignment")), EVekaHAlign::Leading) == EVekaHAlign::Leading ? FVekaAlignment::Leading() : FVekaAlignment());
				}
				return MoveTemp(Element).Frame(Float(TEXT("width"), -1.0), Float(TEXT("height"), -1.0));
			}
			if (Name == TEXT("background"))
			{
				return MoveTemp(Element).Background(PaintFrom(Value(TEXT("paint"))), FVekaCornerRadii(static_cast<float>(Float(TEXT("radius"), 0.0))));
			}
			if (Name == TEXT("foreground"))
			{
				return MoveTemp(Element).Foreground(PaintFrom(Value(TEXT("paint"))));
			}
			if (Name == TEXT("border"))
			{
				return MoveTemp(Element).Border(PaintFrom(Value(TEXT("paint"))), static_cast<float>(Float(TEXT("width"), 1.0)), FVekaCornerRadii(static_cast<float>(Float(TEXT("radius"), 0.0))));
			}
			if (Name == TEXT("backdropBlur") || Name == TEXT("frostedGlass"))
			{
				const bool bFrosted = Name == TEXT("frostedGlass");
				FVekaBackdropModifier Backdrop = bFrosted ? FVekaBackdropModifier::FrostedGlass() : FVekaBackdropModifier();
				Backdrop.Radius = static_cast<float>(Float(bFrosted ? TEXT("blur") : TEXT("radius"), Backdrop.Radius));
				Backdrop.CornerRadii = FVekaCornerRadii(static_cast<float>(Float(TEXT("cornerRadius"), 0.0)));
				if (HasArg(A, TEXT("tint")))
				{
					Backdrop.Tint = PaintFrom(Value(TEXT("tint")));
				}
				Backdrop.TintAmount = static_cast<float>(Float(TEXT("tintAmount"), Backdrop.TintAmount));
				Backdrop.Saturation = static_cast<float>(Float(TEXT("saturation"), Backdrop.Saturation));
				Backdrop.Noise = static_cast<float>(Float(TEXT("noise"), Backdrop.Noise));
				return MoveTemp(Element).AddModifier(FInstancedStruct::Make(Backdrop));
			}
			if (Name == TEXT("opacity"))
			{
				return MoveTemp(Element).Opacity(static_cast<float>(Float(TEXT("value"), 1.0)));
			}
			if (Name == TEXT("clip"))
			{
				return MoveTemp(Element).Clip();
			}
			if (Name == TEXT("offset"))
			{
				return MoveTemp(Element).Offset(FVector2D(Float(TEXT("x"), 0.0), Float(TEXT("y"), 0.0)));
			}
			if (Name == TEXT("scale"))
			{
				return MoveTemp(Element).Scale(static_cast<float>(Float(TEXT("value"), 1.0)));
			}
			if (Name == TEXT("rotation"))
			{
				return MoveTemp(Element).Rotation(static_cast<float>(Float(TEXT("degrees"), 0.0)));
			}
			if (Name == TEXT("font"))
			{
				return MoveTemp(Element).Font(EnumFrom(Value(TEXT("style")), EVekaTextStyle::Body));
			}
			if (Name == TEXT("fontWeight"))
			{
				return MoveTemp(Element).FontWeight(EnumFrom(Value(TEXT("weight")), EVekaFontWeight::Regular));
			}
			if (Name == TEXT("fontSize"))
			{
				return MoveTemp(Element).FontSize(static_cast<float>(Float(TEXT("size"), 14.0)));
			}
			if (Name == TEXT("disabled"))
			{
				return MoveTemp(Element).Disabled(HasArg(A, TEXT("value")) ? Value(TEXT("value")).AsBool() : true);
			}
			if (Name == TEXT("buttonStyle"))
			{
				return MoveTemp(Element).ButtonStyle(EnumFrom(Value(TEXT("style")), EVekaButtonStyleKind::Bordered));
			}
			if (Name == TEXT("toggleStyle"))
			{
				return MoveTemp(Element).ToggleStyle(EnumFrom(Value(TEXT("style")), EVekaToggleStyleKind::Switch));
			}
			if (Name == TEXT("controlSize"))
			{
				return MoveTemp(Element).ControlSize(EnumFrom(Value(TEXT("size")), EVekaControlSize::Regular));
			}
			if (Name == TEXT("symbolRenderingMode"))
			{
				return MoveTemp(Element).SymbolRenderingMode(EnumFrom(Value(TEXT("mode")), EVekaSymbolRenderingMode::Monochrome));
			}
			if (Name == TEXT("animation"))
			{
				return MoveTemp(Element).AnimationKey(FVekaAnimationSpec::Snappy(), static_cast<int64>(GetTypeHash(Value(TEXT("value")))));
			}
			if (Name == TEXT("transition"))
			{
				const FString Kind = Value(TEXT("kind")).AsString();
				const FVekaTransition Transition = Kind == TEXT("scale") ? FVekaTransition::Combined(FVekaTransition::Opacity(), FVekaTransition::Scale(0.6f))
					: Kind == TEXT("slide") ? FVekaTransition::Slide() : FVekaTransition::Opacity();
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
			if (Modifier.Name == TEXT("on"))
			{
				View->Handlers.Add(FName(*Arg(Modifier.Arguments, TEXT("event")).AsString()), MakeHandler(Modifier.Actions));
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
