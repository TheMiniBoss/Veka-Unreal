// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaLanguage.h"
#include "View/VekaCatalog.h"
#include "View/VekaViewAsset.h"
#include "Core/VekaSchema.h"
#include "Core/VekaPaint.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "VekaLang"

namespace
{
	// --- Values ------------------------------------------------------------------------------------------------

	FString SourceString(const FText& Text)
	{
		const FString* Source = FTextInspector::GetSourceString(Text);
		return Source ? *Source : Text.ToString();
	}

	FString Escape(const FString& Raw)
	{
		FString Out;
		for (const TCHAR C : Raw)
		{
			switch (C)
			{
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default: Out.AppendChar(C); break;
			}
		}
		return Out;
	}

	/** Shortest decimal that reads back as the same double, always with a dot. */
	FString FormatFloat(double Value)
	{
		FString Text;
		for (int32 Precision = 1; Precision <= 17; ++Precision)
		{
			Text = FString::Printf(TEXT("%.*g"), Precision, Value);
			if (FCString::Atod(*Text) == Value)
			{
				break;
			}
		}
		if (!Text.Contains(TEXT(".")) && !Text.Contains(TEXT("e")) && !Text.Contains(TEXT("n")) && !Text.Contains(TEXT("i")))
		{
			Text += TEXT(".0");
		}
		return Text;
	}

	/** Shortest decimal that reads back as the same float (timeline keys and settings are floats). */
	FString FormatFloat32(float Value)
	{
		FString Text;
		for (int32 Precision = 1; Precision <= 9; ++Precision)
		{
			Text = FString::Printf(TEXT("%.*g"), Precision, Value);
			if (static_cast<float>(FCString::Atod(*Text)) == Value)
			{
				break;
			}
		}
		if (!Text.Contains(TEXT(".")) && !Text.Contains(TEXT("e")) && !Text.Contains(TEXT("n")) && !Text.Contains(TEXT("i")))
		{
			Text += TEXT(".0");
		}
		return Text;
	}

	bool IsIdentifier(const FString& Text)
	{
		if (Text.IsEmpty() || !(FChar::IsAlpha(Text[0]) || Text[0] == TEXT('_')))
		{
			return false;
		}
		for (const TCHAR C : Text)
		{
			if (!FChar::IsAlnum(C) && C != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}

	bool IsHexColor(const FString& Text)
	{
		if (!Text.StartsWith(TEXT("#")) || (Text.Len() != 7 && Text.Len() != 9))
		{
			return false;
		}
		for (int32 Index = 1; Index < Text.Len(); ++Index)
		{
			if (!FChar::IsHexDigit(Text[Index]))
			{
				return false;
			}
		}
		return true;
	}

	FString PrintValue(const FVekaValue& Value)
	{
		switch (Value.Type)
		{
		case EVekaValueType::Bool: return Value.BoolValue ? TEXT("true") : TEXT("false");
		case EVekaValueType::Int: return FString::Printf(TEXT("%lld"), Value.IntValue);
		case EVekaValueType::Float: return FormatFloat(Value.FloatValue);
		case EVekaValueType::String:
			if (IsIdentifier(Value.StringValue))
			{
				return TEXT(".") + Value.StringValue;
			}
			if (IsHexColor(Value.StringValue))
			{
				return Value.StringValue;
			}
			return TEXT("#\"") + Escape(Value.StringValue) + TEXT("\"");
		case EVekaValueType::Text: return TEXT("\"") + Escape(SourceString(Value.TextValue)) + TEXT("\"");
		case EVekaValueType::Color:
			return FString::Printf(TEXT("rgba(%s, %s, %s, %s)"), *FormatFloat(Value.ColorValue.R), *FormatFloat(Value.ColorValue.G), *FormatFloat(Value.ColorValue.B), *FormatFloat(Value.ColorValue.A));
		case EVekaValueType::Vector2: return FString::Printf(TEXT("vec2(%s, %s)"), *FormatFloat(Value.VectorValue.X), *FormatFloat(Value.VectorValue.Y));
		case EVekaValueType::Array:
		{
			TArray<FString> Items;
			for (int32 Index = 0; Index < Value.Num(); ++Index)
			{
				Items.Add(PrintValue(Value.GetItem(Index)));
			}
			return TEXT("[") + FString::Join(Items, TEXT(", ")) + TEXT("]");
		}
		case EVekaValueType::Struct:
		{
			TArray<FName> Keys;
			Value.StructValue.GetKeys(Keys);
			Keys.Sort(FNameLexicalLess());
			TArray<FString> Fields;
			for (const FName Key : Keys)
			{
				Fields.Add(Key.ToString() + TEXT(": ") + PrintValue(Value.GetField(Key)));
			}
			return TEXT("{ ") + FString::Join(Fields, TEXT(", ")) + TEXT(" }");
		}
		default: return TEXT("nil");
		}
	}

	// --- Printer -----------------------------------------------------------------------------------------------

	class FPrinter
	{
	public:
		explicit FPrinter(const FVekaDocument& InDocument) : Document(InDocument) {}

		/** One expression (Veka::Lang::PrintExpression). */
		FString PrintOne(int32 Index) const { return Expression(Index); }

		FString Run(FName ViewName)
		{
			for (const FString& Comment : Document.Comments)
			{
				Line(0, TEXT("// ") + Comment);
			}
			Line(0, FString::Printf(TEXT("view %s {"), *ViewName.ToString()));
			bool bDeclarations = false;
			for (const FVekaStateDeclaration& State : Document.States)
			{
				Line(1, Declaration(TEXT("State"), State.Name, State.Initial.Type, State.Initial));
				bDeclarations = true;
			}
			for (const FVekaParamDeclaration& Param : Document.Params)
			{
				Line(1, Declaration(TEXT("Param"), Param.Name, Param.Type, Param.Default));
				bDeclarations = true;
			}
			for (const FVekaSlotDeclaration& Slot : Document.Slots)
			{
				Line(1, TEXT("@Slot ") + SlotName(Slot.Name));
				bDeclarations = true;
			}
			for (const FVekaEventDeclaration& Event : Document.EventDeclarations)
			{
				Line(1, Event.PayloadType == EVekaValueType::None ? TEXT("@Event ") + Event.Name.ToString()
					: FString::Printf(TEXT("@Event %s(%s)"), *Event.Name.ToString(), *Veka::Lang::TypeName(Event.PayloadType)));
				bDeclarations = true;
			}
			for (const FVekaDocumentTimeline& Timeline : Document.Timelines)
			{
				PrintTimeline(Timeline);
				bDeclarations = true;
			}
			if (Document.Nodes.IsValidIndex(Document.Root))
			{
				if (bDeclarations)
				{
					Out += TEXT("\n");
				}
				Node(Document.Root, 1);
			}
			Line(0, TEXT("}"));
			return Out;
		}

	private:
		static FString SlotName(FName Name) { return Name.IsNone() ? FString(TEXT("default")) : Name.ToString(); }

		FString Declaration(const TCHAR* Kind, FName Name, EVekaValueType Type, const FVekaValue& Value) const
		{
			// A state's type is the type of its initial value; a param keeps its declared type.
			FString Text = FString::Printf(TEXT("@%s var %s"), Kind, *Name.ToString());
			if (Type != EVekaValueType::None)
			{
				Text += TEXT(": ") + Veka::Lang::TypeName(Type);
			}
			if (!Value.IsNone())
			{
				Text += TEXT(" = ") + PrintValue(Value);
			}
			return Text;
		}

		void PrintTimeline(const FVekaDocumentTimeline& Timeline)
		{
			TArray<FString> Settings;
			if (Timeline.Data.Length > 0.f)
			{
				Settings.Add(TEXT("length: ") + FormatFloat32(Timeline.Data.Length));
			}
			if (Timeline.Data.bLoop)
			{
				Settings.Add(TEXT("loop: true"));
			}
			if (Timeline.Data.Speed != 1.f)
			{
				Settings.Add(TEXT("speed: ") + FormatFloat32(Timeline.Data.Speed));
			}
			if (Timeline.bAutoPlay)
			{
				Settings.Add(TEXT("autoplay: true"));
			}
			Line(1, TEXT("@Timeline ") + Timeline.Name.ToString() + (Settings.IsEmpty() ? FString() : TEXT("(") + FString::Join(Settings, TEXT(", ")) + TEXT(")")) + TEXT(" {"));
			const UEnum* Curves = StaticEnum<EVekaAnimationCurve>();
			for (const FVekaTimelineTrack& Track : Timeline.Data.Tracks)
			{
				TArray<FString> Keys;
				for (const FVekaTimelineKey& Key : Track.Keys)
				{
					FString Text = FormatFloat32(Key.Time) + TEXT(": ") + FormatFloat32(Key.Value);
					if (Key.Curve != EVekaAnimationCurve::EaseInOut)
					{
						Text += TEXT(" .") + Curves->GetNameStringByValue(static_cast<int64>(Key.Curve));
					}
					Keys.Add(Text);
				}
				Line(2, TEXT("track ") + Track.Name.ToString() + TEXT(" { ") + FString::Join(Keys, TEXT(", ")) + (Keys.IsEmpty() ? TEXT("}") : TEXT(" }")));
			}
			for (const FVekaTimelineCue& Cue : Timeline.Cues)
			{
				Line(2, TEXT("at ") + FormatFloat32(Cue.Time) + TEXT(" {"));
				Actions(Cue.Actions, 3);
				Line(2, TEXT("}"));
			}
			Line(1, TEXT("}"));
		}

		void Line(int32 Indent, const FString& Text)
		{
			Out += FString::ChrN(Indent * 4, TEXT(' ')) + Text + TEXT("\n");
		}

		FString Arguments(const TArray<FVekaAstArgument>& Arguments, FName Primary, bool bEventName, bool bAlwaysParens) const
		{
			if (Arguments.IsEmpty())
			{
				return bAlwaysParens ? FString(TEXT("()")) : FString();
			}
			TArray<FString> Parts;
			const FVekaAstArgument* PrimaryArgument = Primary.IsNone() ? nullptr : Arguments.FindByPredicate([Primary](const FVekaAstArgument& Argument) { return Argument.Name == Primary; });
			if (PrimaryArgument)
			{
				const FVekaAstExpr* Expr = Document.Expressions.IsValidIndex(PrimaryArgument->Expr) ? &Document.Expressions[PrimaryArgument->Expr] : nullptr;
				if (bEventName && Expr && Expr->Kind == EVekaExprKind::Literal && IsIdentifier(Expr->Value.AsString()))
				{
					Parts.Add(Expr->Value.AsString());
				}
				else
				{
					Parts.Add(Expression(PrimaryArgument->Expr));
				}
			}
			for (const FVekaAstArgument& Argument : Arguments)
			{
				if (&Argument != PrimaryArgument)
				{
					Parts.Add(Argument.Name.ToString() + TEXT(": ") + Expression(Argument.Expr));
				}
			}
			return TEXT("(") + FString::Join(Parts, TEXT(", ")) + TEXT(")");
		}

		void Actions(const TArray<FVekaAstAction>& InActions, int32 Indent)
		{
			for (int32 Index = 0; Index < InActions.Num();)
			{
				if (InActions[Index].bAnimated)
				{
					Line(Indent, TEXT("withAnimation {"));
					while (Index < InActions.Num() && InActions[Index].bAnimated)
					{
						Line(Indent + 1, Action(InActions[Index]));
						++Index;
					}
					Line(Indent, TEXT("}"));
					continue;
				}
				Line(Indent, Action(InActions[Index]));
				++Index;
			}
		}

		FString Action(const FVekaAstAction& InAction) const
		{
			const FString Target = InAction.Target.ToString();
			switch (InAction.Kind)
			{
			case EVekaActionKind::Set: return Target + TEXT(" = ") + Expression(InAction.Expr);
			case EVekaActionKind::Toggle: return Target + TEXT(".toggle()");
			case EVekaActionKind::Increment:
				// The parser stores x -= e as an increment by -(e).
				if (Document.Expressions.IsValidIndex(InAction.Expr) && Document.Expressions[InAction.Expr].Kind == EVekaExprKind::Unary
					&& Document.Expressions[InAction.Expr].Op == EVekaExprOp::Negate && Document.Expressions[InAction.Expr].Operands.Num() == 1)
				{
					return Target + TEXT(" -= ") + Expression(Document.Expressions[InAction.Expr].Operands[0]);
				}
				return Target + TEXT(" += ") + (InAction.Expr == INDEX_NONE ? FString(TEXT("1")) : Expression(InAction.Expr));
			case EVekaActionKind::Emit: return TEXT("emit ") + Target + (InAction.Expr == INDEX_NONE ? FString() : TEXT("(") + Expression(InAction.Expr) + TEXT(")"));
			case EVekaActionKind::Play: return Target + TEXT(".play()");
			case EVekaActionKind::Pause: return Target + TEXT(".pause()");
			case EVekaActionKind::Stop: return Target + TEXT(".stop()");
			case EVekaActionKind::Reverse: return Target + TEXT(".reverse()");
			case EVekaActionKind::Seek: return Target + TEXT(".seek(") + Expression(InAction.Expr) + TEXT(")");
			}
			return FString();
		}

		void Children(const TArray<int32>& InChildren, int32 Indent)
		{
			for (const int32 Child : InChildren)
			{
				Node(Child, Indent);
			}
		}

		void Node(int32 Index, int32 Indent)
		{
			if (!Document.Nodes.IsValidIndex(Index))
			{
				return;
			}
			const FVekaAstNode& N = Document.Nodes[Index];
			for (const FString& Comment : N.Comments)
			{
				Line(Indent, TEXT("// ") + Comment);
			}
			switch (N.Kind)
			{
			case EVekaAstNodeKind::If:
			{
				Line(Indent, TEXT("if ") + Expression(N.Expr) + TEXT(" {"));
				Children(N.Children, Indent + 1);
				const FVekaAstNode* Branch = &N;
				// "else { if ... }" with nothing else in it prints as "else if" (what the parser builds for it).
				while (Branch->ElseChildren.Num() == 1 && Document.Nodes.IsValidIndex(Branch->ElseChildren[0]))
				{
					const FVekaAstNode& Next = Document.Nodes[Branch->ElseChildren[0]];
					if (Next.Kind != EVekaAstNodeKind::If || !Next.Comments.IsEmpty() || !Next.Modifiers.IsEmpty() || !Next.SlotName.IsNone())
					{
						break;
					}
					Line(Indent, TEXT("} else if ") + Expression(Next.Expr) + TEXT(" {"));
					Children(Next.Children, Indent + 1);
					Branch = &Next;
				}
				if (!Branch->ElseChildren.IsEmpty())
				{
					Line(Indent, TEXT("} else {"));
					Children(Branch->ElseChildren, Indent + 1);
				}
				Line(Indent, TEXT("}"));
				break;
			}
			case EVekaAstNodeKind::ForEach:
			{
				FString Head = TEXT("ForEach(") + Expression(N.Expr) + TEXT(") {");
				if (!N.LoopVariable.IsNone())
				{
					Head += TEXT(" ") + N.LoopVariable.ToString() + (N.IndexVariable.IsNone() ? FString() : TEXT(", ") + N.IndexVariable.ToString()) + TEXT(" in");
				}
				Line(Indent, Head);
				Children(N.Children, Indent + 1);
				Line(Indent, TEXT("}"));
				break;
			}
			case EVekaAstNodeKind::Slot:
				Line(Indent, TEXT("Slot(") + SlotName(N.Type) + TEXT(")"));
				break;
			default:
			{
				FName Primary = NAME_None;
				if (const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(N.Type))
				{
					Primary = Entry->PrimaryArgument;
				}
				else if (N.Component && N.Component->Document.Params.Num() == 1)
				{
					Primary = N.Component->Document.Params[0].Name;
				}
				const FString Head = N.Type.ToString() + Arguments(N.Arguments, Primary, false, false);
				if (!N.Children.IsEmpty())
				{
					Line(Indent, Head + TEXT(" {"));
					Children(N.Children, Indent + 1);
					if (N.Actions.IsEmpty())
					{
						Line(Indent, TEXT("}"));
					}
					else
					{
						Line(Indent, TEXT("} action {"));
						Actions(N.Actions, Indent + 1);
						Line(Indent, TEXT("}"));
					}
				}
				else if (!N.Actions.IsEmpty())
				{
					Line(Indent, Head + TEXT(" {"));
					Actions(N.Actions, Indent + 1);
					Line(Indent, TEXT("}"));
				}
				else
				{
					Line(Indent, Head);
				}
				break;
			}
			}

			// After a closing brace modifiers line up with it; under a one-line view they are indented.
			const bool bBlock = N.Kind == EVekaAstNodeKind::If || N.Kind == EVekaAstNodeKind::ForEach
				|| (N.Kind == EVekaAstNodeKind::Element && (!N.Children.IsEmpty() || !N.Actions.IsEmpty()));
			const int32 ModifierIndent = bBlock ? Indent : Indent + 1;
			if (!N.SlotName.IsNone())
			{
				Line(ModifierIndent, TEXT(".inSlot(") + N.SlotName.ToString() + TEXT(")"));
			}
			for (const FVekaAstModifier& Modifier : N.Modifiers)
			{
				const FVekaCatalogEntry* Entry = Veka::Catalog::FindModifier(Modifier.Name);
				const FString Head = TEXT(".") + Modifier.Name.ToString() + Arguments(Modifier.Arguments, Entry ? Entry->PrimaryArgument : NAME_None, Modifier.Name == TEXT("on"), true);
				if (Modifier.Actions.IsEmpty())
				{
					Line(ModifierIndent, Head);
				}
				else
				{
					Line(ModifierIndent, Head + TEXT(" {"));
					Actions(Modifier.Actions, ModifierIndent + 1);
					Line(ModifierIndent, TEXT("}"));
				}
			}
		}

		// Expressions ----------------------------------------------------------------------------------------------

		static int32 Precedence(EVekaExprOp Op)
		{
			switch (Op)
			{
			case EVekaExprOp::Or: return 1;
			case EVekaExprOp::And: return 2;
			case EVekaExprOp::Equal:
			case EVekaExprOp::NotEqual: return 3;
			case EVekaExprOp::Less:
			case EVekaExprOp::LessEqual:
			case EVekaExprOp::Greater:
			case EVekaExprOp::GreaterEqual: return 4;
			case EVekaExprOp::Add:
			case EVekaExprOp::Subtract: return 5;
			case EVekaExprOp::Multiply:
			case EVekaExprOp::Divide:
			case EVekaExprOp::Modulo: return 6;
			default: return 0;
			}
		}

		static const TCHAR* OpText(EVekaExprOp Op)
		{
			switch (Op)
			{
			case EVekaExprOp::Or: return TEXT("||");
			case EVekaExprOp::And: return TEXT("&&");
			case EVekaExprOp::Equal: return TEXT("==");
			case EVekaExprOp::NotEqual: return TEXT("!=");
			case EVekaExprOp::Less: return TEXT("<");
			case EVekaExprOp::LessEqual: return TEXT("<=");
			case EVekaExprOp::Greater: return TEXT(">");
			case EVekaExprOp::GreaterEqual: return TEXT(">=");
			case EVekaExprOp::Add: return TEXT("+");
			case EVekaExprOp::Subtract: return TEXT("-");
			case EVekaExprOp::Multiply: return TEXT("*");
			case EVekaExprOp::Divide: return TEXT("/");
			case EVekaExprOp::Modulo: return TEXT("%");
			default: return TEXT("?");
			}
		}

		/** Precedence of an expression as printed (10 = atom). */
		int32 Level(int32 Index) const
		{
			if (!Document.Expressions.IsValidIndex(Index))
			{
				return 10;
			}
			const FVekaAstExpr& E = Document.Expressions[Index];
			if (E.Kind == EVekaExprKind::Ternary)
			{
				return 0;
			}
			if (E.Kind == EVekaExprKind::Binary && Precedence(E.Op) > 0)
			{
				return Precedence(E.Op);
			}
			if (E.Kind == EVekaExprKind::Unary)
			{
				return 7;
			}
			if (E.Kind == EVekaExprKind::Literal && E.Value.IsNumber() && E.Value.AsFloat() < 0.0)
			{
				return 7;
			}
			return 10;
		}

		FString Wrapped(int32 Index, int32 MinLevel) const
		{
			const FString Text = Expression(Index);
			return Level(Index) < MinLevel ? TEXT("(") + Text + TEXT(")") : Text;
		}

		/** "{0} gold" with operand 0 used once and in order: printable as an interpolation. */
		static bool IsSimplePattern(const FString& Pattern, int32 Count)
		{
			int32 Expected = 0;
			for (int32 Index = 0; Index < Pattern.Len(); ++Index)
			{
				if (Pattern[Index] == TEXT('{'))
				{
					int32 End = Index + 1;
					while (End < Pattern.Len() && FChar::IsDigit(Pattern[End]))
					{
						++End;
					}
					if (End < Pattern.Len() && Pattern[End] == TEXT('}') && End > Index + 1)
					{
						if (FCString::Atoi(*Pattern.Mid(Index + 1, End - Index - 1)) != Expected)
						{
							return false;
						}
						++Expected;
						Index = End;
					}
				}
			}
			return Expected == Count;
		}

		FString Expression(int32 Index) const
		{
			if (!Document.Expressions.IsValidIndex(Index))
			{
				return TEXT("nil");
			}
			const FVekaAstExpr& E = Document.Expressions[Index];
			auto Operand = [&E](int32 Position) { return E.Operands.IsValidIndex(Position) ? E.Operands[Position] : INDEX_NONE; };
			switch (E.Kind)
			{
			case EVekaExprKind::Literal: return PrintValue(E.Value);
			case EVekaExprKind::State:
			case EVekaExprKind::Param:
			case EVekaExprKind::Local: return E.Name.ToString();
			case EVekaExprKind::Model: return TEXT("model.") + E.Name.ToString();
			case EVekaExprKind::Timeline: return E.Name.ToString();
			case EVekaExprKind::Member: return Wrapped(Operand(0), 10) + TEXT(".") + E.Name.ToString();
			case EVekaExprKind::Unary:
			{
				const int32 Inner = Operand(0);
				// A negated literal stays a Unary node: -(3), not the literal -3.
				const bool bLiteral = Document.Expressions.IsValidIndex(Inner) && Document.Expressions[Inner].Kind == EVekaExprKind::Literal;
				const FString Text = bLiteral ? TEXT("(") + Expression(Inner) + TEXT(")") : Wrapped(Inner, 7);
				return (E.Op == EVekaExprOp::Negate ? TEXT("-") : TEXT("!")) + Text;
			}
			case EVekaExprKind::Binary:
				if (E.Op == EVekaExprOp::Index)
				{
					return Wrapped(Operand(0), 10) + TEXT("[") + Expression(Operand(1)) + TEXT("]");
				}
				if (E.Op == EVekaExprOp::Min || E.Op == EVekaExprOp::Max)
				{
					return FString::Printf(TEXT("%s(%s, %s)"), E.Op == EVekaExprOp::Min ? TEXT("min") : TEXT("max"), *Expression(Operand(0)), *Expression(Operand(1)));
				}
				// Left-associative: the right operand needs parentheses at the same level.
				return Wrapped(Operand(0), Precedence(E.Op)) + TEXT(" ") + OpText(E.Op) + TEXT(" ") + Wrapped(Operand(1), Precedence(E.Op) + 1);
			case EVekaExprKind::Ternary: return Wrapped(Operand(0), 1) + TEXT(" ? ") + Wrapped(Operand(1), 1) + TEXT(" : ") + Expression(Operand(2));
			case EVekaExprKind::Format:
			{
				const bool bText = E.Value.Type == EVekaValueType::Text;
				const FString Pattern = bText ? SourceString(E.Value.TextValue) : E.Value.StringValue;
				if (!IsSimplePattern(Pattern, E.Operands.Num()))
				{
					TArray<FString> Parts = { (bText ? TEXT("\"") : TEXT("#\"")) + Escape(Pattern) + TEXT("\"") };
					for (const int32 Item : E.Operands)
					{
						Parts.Add(Expression(Item));
					}
					return TEXT("format(") + FString::Join(Parts, TEXT(", ")) + TEXT(")");
				}
				FString Escaped = Escape(Pattern);
				for (int32 Position = 0; Position < E.Operands.Num(); ++Position)
				{
					Escaped.ReplaceInline(*FString::Printf(TEXT("{%d}"), Position), *(TEXT("\\(") + Expression(E.Operands[Position]) + TEXT(")")));
				}
				return (bText ? TEXT("\"") : TEXT("#\"")) + Escaped + TEXT("\"");
			}
			}
			return TEXT("nil");
		}

		const FVekaDocument& Document;
		FString Out;
	};

	// --- Structural comparison ---------------------------------------------------------------------------------

	class FComparer
	{
	public:
		FComparer(const FVekaDocument& InA, const FVekaDocument& InB) : A(InA), B(InB) {}

		FString Difference;

		bool Run()
		{
			if (A.States.Num() != B.States.Num())
			{
				return Fail(TEXT("state count"));
			}
			for (int32 Index = 0; Index < A.States.Num(); ++Index)
			{
				if (A.States[Index].Name != B.States[Index].Name || !SameValue(A.States[Index].Initial, B.States[Index].Initial))
				{
					return Fail(FString::Printf(TEXT("state %s"), *A.States[Index].Name.ToString()));
				}
			}
			if (A.Params.Num() != B.Params.Num())
			{
				return Fail(TEXT("param count"));
			}
			for (int32 Index = 0; Index < A.Params.Num(); ++Index)
			{
				const FVekaParamDeclaration& PA = A.Params[Index];
				const FVekaParamDeclaration& PB = B.Params[Index];
				if (PA.Name != PB.Name || PA.Type != PB.Type || !SameValue(PA.Default, PB.Default))
				{
					return Fail(FString::Printf(TEXT("param %s"), *PA.Name.ToString()));
				}
			}
			if (A.Slots.Num() != B.Slots.Num())
			{
				return Fail(TEXT("slot count"));
			}
			for (int32 Index = 0; Index < A.Slots.Num(); ++Index)
			{
				if (A.Slots[Index].Name != B.Slots[Index].Name)
				{
					return Fail(TEXT("slot name"));
				}
			}
			if (A.EventDeclarations.Num() != B.EventDeclarations.Num())
			{
				return Fail(TEXT("event count"));
			}
			for (int32 Index = 0; Index < A.EventDeclarations.Num(); ++Index)
			{
				if (A.EventDeclarations[Index].Name != B.EventDeclarations[Index].Name || A.EventDeclarations[Index].PayloadType != B.EventDeclarations[Index].PayloadType)
				{
					return Fail(FString::Printf(TEXT("event %s"), *A.EventDeclarations[Index].Name.ToString()));
				}
			}
			if (A.Comments != B.Comments)
			{
				return Fail(TEXT("document comments"));
			}
			if (A.Timelines.Num() != B.Timelines.Num())
			{
				return Fail(TEXT("timeline count"));
			}
			for (int32 Index = 0; Index < A.Timelines.Num(); ++Index)
			{
				const FVekaDocumentTimeline& TA = A.Timelines[Index];
				const FVekaDocumentTimeline& TB = B.Timelines[Index];
				const FString Where = TEXT("timeline ") + TA.Name.ToString();
				if (TA.Name != TB.Name || TA.bAutoPlay != TB.bAutoPlay || TA.Data.Length != TB.Data.Length || TA.Data.bLoop != TB.Data.bLoop || TA.Data.Speed != TB.Data.Speed
					|| TA.Data.Tracks.Num() != TB.Data.Tracks.Num() || TA.Cues.Num() != TB.Cues.Num())
				{
					return Fail(Where);
				}
				for (int32 Track = 0; Track < TA.Data.Tracks.Num(); ++Track)
				{
					const FVekaTimelineTrack& KA = TA.Data.Tracks[Track];
					const FVekaTimelineTrack& KB = TB.Data.Tracks[Track];
					if (KA.Name != KB.Name || KA.Keys.Num() != KB.Keys.Num())
					{
						return Fail(Where + TEXT(" track ") + KA.Name.ToString());
					}
					for (int32 Key = 0; Key < KA.Keys.Num(); ++Key)
					{
						if (KA.Keys[Key].Time != KB.Keys[Key].Time || KA.Keys[Key].Value != KB.Keys[Key].Value || KA.Keys[Key].Curve != KB.Keys[Key].Curve)
						{
							return Fail(Where + TEXT(" track ") + KA.Name.ToString() + TEXT(" key"));
						}
					}
				}
				for (int32 Cue = 0; Cue < TA.Cues.Num(); ++Cue)
				{
					if (TA.Cues[Cue].Time != TB.Cues[Cue].Time || !SameActions(TA.Cues[Cue].Actions, TB.Cues[Cue].Actions, Where + TEXT(" cue")))
					{
						return Fail(Where + TEXT(" cue"));
					}
				}
			}
			return SameNode(A.Root, B.Root, TEXT("root"));
		}

	private:
		bool Fail(const FString& What)
		{
			if (Difference.IsEmpty())
			{
				Difference = What;
			}
			return false;
		}

		static bool SameValue(const FVekaValue& X, const FVekaValue& Y)
		{
			if (X.Type != Y.Type)
			{
				return false;
			}
			switch (X.Type)
			{
			case EVekaValueType::Text: return SourceString(X.TextValue) == SourceString(Y.TextValue);
			case EVekaValueType::Array:
				if (X.Num() != Y.Num())
				{
					return false;
				}
				for (int32 Index = 0; Index < X.Num(); ++Index)
				{
					if (!SameValue(X.GetItem(Index), Y.GetItem(Index)))
					{
						return false;
					}
				}
				return true;
			case EVekaValueType::Struct:
				if (X.StructValue.Num() != Y.StructValue.Num())
				{
					return false;
				}
				for (const TPair<FName, FInstancedStruct>& Pair : X.StructValue)
				{
					if (!Y.StructValue.Contains(Pair.Key) || !SameValue(X.GetField(Pair.Key), Y.GetField(Pair.Key)))
					{
						return false;
					}
				}
				return true;
			case EVekaValueType::Float: return X.FloatValue == Y.FloatValue;
			default: return X == Y;
			}
		}

		bool SameExpr(int32 IA, int32 IB, const FString& Where)
		{
			const bool bA = A.Expressions.IsValidIndex(IA);
			const bool bB = B.Expressions.IsValidIndex(IB);
			if (bA != bB)
			{
				return Fail(Where + TEXT(": one expression is missing"));
			}
			if (!bA)
			{
				return true;
			}
			const FVekaAstExpr& EA = A.Expressions[IA];
			const FVekaAstExpr& EB = B.Expressions[IB];
			if (EA.Kind != EB.Kind || EA.Op != EB.Op || EA.Name != EB.Name || EA.Operands.Num() != EB.Operands.Num() || !SameValue(EA.Value, EB.Value))
			{
				return Fail(Where + TEXT(": expressions differ"));
			}
			for (int32 Index = 0; Index < EA.Operands.Num(); ++Index)
			{
				if (!SameExpr(EA.Operands[Index], EB.Operands[Index], Where))
				{
					return false;
				}
			}
			return true;
		}

		bool SameArguments(const TArray<FVekaAstArgument>& XA, const TArray<FVekaAstArgument>& XB, const FString& Where)
		{
			if (XA.Num() != XB.Num())
			{
				return Fail(Where + TEXT(": argument count"));
			}
			for (const FVekaAstArgument& Argument : XA)
			{
				const FVekaAstArgument* Other = XB.FindByPredicate([&Argument](const FVekaAstArgument& Item) { return Item.Name == Argument.Name; });
				if (!Other)
				{
					return Fail(Where + TEXT(": argument ") + Argument.Name.ToString());
				}
				if (!SameExpr(Argument.Expr, Other->Expr, Where + TEXT(".") + Argument.Name.ToString()))
				{
					return false;
				}
			}
			return true;
		}

		/** Increment with no expression means += 1. */
		bool SameIncrement(const FVekaAstAction& XA, const FVekaAstAction& XB)
		{
			auto IsOne = [](const FVekaDocument& D, int32 Index)
			{
				return Index == INDEX_NONE || (D.Expressions.IsValidIndex(Index) && D.Expressions[Index].Kind == EVekaExprKind::Literal && D.Expressions[Index].Value == FVekaValue::Int(1)
					&& D.Expressions[Index].Value.Type == EVekaValueType::Int);
			};
			return (XA.Expr == INDEX_NONE || XB.Expr == INDEX_NONE) && IsOne(A, XA.Expr) && IsOne(B, XB.Expr);
		}

		bool SameActions(const TArray<FVekaAstAction>& XA, const TArray<FVekaAstAction>& XB, const FString& Where)
		{
			if (XA.Num() != XB.Num())
			{
				return Fail(Where + TEXT(": action count"));
			}
			for (int32 Index = 0; Index < XA.Num(); ++Index)
			{
				const FVekaAstAction& AA = XA[Index];
				const FVekaAstAction& AB = XB[Index];
				if (AA.Kind != AB.Kind || AA.Target != AB.Target || AA.bAnimated != AB.bAnimated)
				{
					return Fail(Where + TEXT(": action ") + AA.Target.ToString());
				}
				if (AA.Kind == EVekaActionKind::Increment && SameIncrement(AA, AB))
				{
					continue;
				}
				if (!SameExpr(AA.Expr, AB.Expr, Where + TEXT(": action ") + AA.Target.ToString()))
				{
					return false;
				}
			}
			return true;
		}

		bool SameNodes(const TArray<int32>& XA, const TArray<int32>& XB, const FString& Where)
		{
			if (XA.Num() != XB.Num())
			{
				return Fail(Where + TEXT(": child count"));
			}
			for (int32 Index = 0; Index < XA.Num(); ++Index)
			{
				if (!SameNode(XA[Index], XB[Index], FString::Printf(TEXT("%s/%d"), *Where, Index)))
				{
					return false;
				}
			}
			return true;
		}

		bool SameNode(int32 IA, int32 IB, const FString& Where)
		{
			const bool bA = A.Nodes.IsValidIndex(IA);
			const bool bB = B.Nodes.IsValidIndex(IB);
			if (bA != bB)
			{
				return Fail(Where + TEXT(": one node is missing"));
			}
			if (!bA)
			{
				return true;
			}
			const FVekaAstNode& NA = A.Nodes[IA];
			const FVekaAstNode& NB = B.Nodes[IB];
			const FString Here = Where + TEXT(" ") + NA.Type.ToString();
			if (NA.Kind != NB.Kind || NA.Type != NB.Type || NA.Component != NB.Component || NA.SlotName != NB.SlotName || NA.LoopVariable != NB.LoopVariable
				|| NA.IndexVariable != NB.IndexVariable)
			{
				return Fail(Here + TEXT(": kind, type, component, slot or loop variables differ"));
			}
			if (NA.Comments != NB.Comments)
			{
				return Fail(Here + TEXT(": comments"));
			}
			if (!SameExpr(NA.Expr, NB.Expr, Here + TEXT(" condition")) || !SameArguments(NA.Arguments, NB.Arguments, Here) || !SameActions(NA.Actions, NB.Actions, Here))
			{
				return false;
			}
			if (NA.Modifiers.Num() != NB.Modifiers.Num())
			{
				return Fail(Here + TEXT(": modifier count"));
			}
			for (int32 Index = 0; Index < NA.Modifiers.Num(); ++Index)
			{
				const FVekaAstModifier& MA = NA.Modifiers[Index];
				const FVekaAstModifier& MB = NB.Modifiers[Index];
				const FString ModifierWhere = Here + TEXT(".") + MA.Name.ToString();
				if (MA.Name != MB.Name)
				{
					return Fail(ModifierWhere + TEXT(": modifier name"));
				}
				if (!SameArguments(MA.Arguments, MB.Arguments, ModifierWhere) || !SameActions(MA.Actions, MB.Actions, ModifierWhere))
				{
					return false;
				}
			}
			return SameNodes(NA.Children, NB.Children, Here) && SameNodes(NA.ElseChildren, NB.ElseChildren, Here + TEXT(" else"));
		}

		const FVekaDocument& A;
		const FVekaDocument& B;
	};

	// --- Analysis ------------------------------------------------------------------------------------------------

	class FAnalyzer
	{
	public:
		explicit FAnalyzer(const FVekaDocument& InDocument) : Document(InDocument) {}

		TArray<FVekaDiagnostic> Diagnostics;

		void Run()
		{
			for (const FString& Problem : Document.Validate())
			{
				Add(EVekaDiagnosticSeverity::Error, FText::FromString(Problem), FVekaSourceRange());
			}
			for (const FVekaAstExpr& Expr : Document.Expressions)
			{
				if (Expr.Kind != EVekaExprKind::Member || !Document.Expressions.IsValidIndex(Expr.Operands.IsEmpty() ? INDEX_NONE : Expr.Operands[0]))
				{
					continue;
				}
				const FVekaAstExpr& Owner = Document.Expressions[Expr.Operands[0]];
				const FVekaDocumentTimeline* Timeline = Owner.Kind == EVekaExprKind::Timeline ? Document.FindTimeline(Owner.Name) : nullptr;
				if (Timeline && !Timeline->Data.Tracks.ContainsByPredicate([&Expr](const FVekaTimelineTrack& Track) { return Track.Name == Expr.Name; }))
				{
					Add(EVekaDiagnosticSeverity::Warning, FText::Format(LOCTEXT("UnknownTrack", "Timeline {0} has no track '{1}' (reads 0)"), FText::FromName(Owner.Name),
						FText::FromName(Expr.Name)), Expr.Source);
				}
			}
			for (const FVekaParamDeclaration& Param : Document.Params)
			{
				if (Document.States.ContainsByPredicate([&Param](const FVekaStateDeclaration& State) { return State.Name == Param.Name; }))
				{
					Add(EVekaDiagnosticSeverity::Warning, FText::Format(LOCTEXT("ParamShadowsState", "'{0}' is both a param and a state (the param wins)"), FText::FromName(Param.Name)), FVekaSourceRange());
				}
			}
			for (const FVekaAstNode& Node : Document.Nodes)
			{
				if (Node.Kind != EVekaAstNodeKind::Element)
				{
					continue;
				}
				if (!Node.Component)
				{
					if (const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(Node.Type))
					{
						CheckArguments(*Entry, Node.Arguments, Node.Type, Node.Source);
					}
					else if (const FVekaSchemaEntry* Schema = FVekaSchema::Get().Find(Node.Type, EVekaSchemaKind::Primitive))
					{
						CheckProperties(Schema->Struct, Node.Arguments, Node.Type, Node.Source);
					}
					else
					{
						Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("UnknownComponent", "Unknown component '{0}' (not a Veka component, a schema primitive or a view of the project)"),
							FText::FromName(Node.Type)), Node.Source);
					}
				}
				for (const FVekaAstModifier& Modifier : Node.Modifiers)
				{
					if (const FVekaCatalogEntry* Entry = Veka::Catalog::FindModifier(Modifier.Name))
					{
						if (Modifier.Name != TEXT("on"))
						{
							CheckArguments(*Entry, Modifier.Arguments, Modifier.Name, Node.Source);
						}
						if (!Entry->bActions && !Modifier.Actions.IsEmpty())
						{
							Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("ModifierNoActions", ".{0} takes no actions"), FText::FromName(Modifier.Name)), Node.Source);
						}
					}
					else if (const FVekaSchemaEntry* Schema = FVekaSchema::Get().Find(Modifier.Name, EVekaSchemaKind::Modifier))
					{
						CheckProperties(Schema->Struct, Modifier.Arguments, Modifier.Name, Node.Source);
					}
					else
					{
						Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("UnknownModifier", "Unknown modifier '.{0}'"), FText::FromName(Modifier.Name)), Node.Source);
					}
				}
				const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(Node.Type);
				if (!Node.Actions.IsEmpty() && Entry && !Entry->bActions)
				{
					Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("ComponentNoActions", "{0} takes no actions"), FText::FromName(Node.Type)), Node.Source);
				}
				if (!Node.Children.IsEmpty() && Entry && !Entry->bContainer)
				{
					Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("ComponentNoChildren", "{0} has no content"), FText::FromName(Node.Type)), Node.Source);
				}
			}
		}

	private:
		void Add(EVekaDiagnosticSeverity Severity, const FText& Message, const FVekaSourceRange& Range)
		{
			FVekaDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Severity = Severity;
			Diagnostic.Message = Message;
			Diagnostic.Range = Range;
		}

		const FVekaAstExpr* Expr(int32 Index) const
		{
			return Document.Expressions.IsValidIndex(Index) ? &Document.Expressions[Index] : nullptr;
		}

		void CheckArguments(const FVekaCatalogEntry& Entry, const TArray<FVekaAstArgument>& Arguments, FName Owner, const FVekaSourceRange& Range)
		{
			for (const FVekaAstArgument& Argument : Arguments)
			{
				const FVekaCatalogArgument* Spec = Entry.FindArgument(Argument.Name);
				const FVekaAstExpr* Value = Expr(Argument.Expr);
				const FVekaSourceRange& At = Value && Value->Source.IsSet() ? Value->Source : Range;
				if (!Spec)
				{
					Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("UnknownArgument", "{0} has no argument '{1}'"), FText::FromName(Owner), FText::FromName(Argument.Name)), At);
					continue;
				}
				if (!Value)
				{
					continue;
				}
				const bool bLiteral = Value->Kind == EVekaExprKind::Literal;
				switch (Spec->Kind)
				{
				case EVekaArgumentKind::Binding:
					if (Value->Kind != EVekaExprKind::State)
					{
						Add(EVekaDiagnosticSeverity::Warning, FText::Format(LOCTEXT("NotBinding", "'{0}' should be a state (the control writes it back); a constant is read-only"),
							FText::FromName(Argument.Name)), At);
					}
					break;
				case EVekaArgumentKind::Enum:
					if (bLiteral && Value->Value.Type == EVekaValueType::String && Spec->Enum && Spec->Enum->GetValueByNameString(Value->Value.StringValue) == INDEX_NONE)
					{
						TArray<FString> Names;
						for (int32 Item = 0; Item < Spec->Enum->NumEnums() - 1; ++Item)
						{
							Names.Add(TEXT(".") + Spec->Enum->GetNameStringByIndex(Item));
						}
						Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("BadEnum", "'.{0}' is not a value of {1} ({2})"), FText::FromString(Value->Value.StringValue),
							FText::FromName(Argument.Name), FText::FromString(FString::Join(Names, TEXT(", ")))), At);
					}
					break;
				case EVekaArgumentKind::Paint:
					if (bLiteral && Value->Value.Type == EVekaValueType::String && !Value->Value.StringValue.StartsWith(TEXT("#")))
					{
						FLinearColor Ignored;
						if (!Veka::Colora::GetDefaultColor(FName(*Value->Value.StringValue), EVekaColorScheme::Dark, Ignored) && !Veka::Paints::Find(FName(*Value->Value.StringValue)))
						{
							Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("BadToken", "'.{0}' is not a Colora token nor a paint asset"), FText::FromString(Value->Value.StringValue)), At);
						}
					}
					break;
				case EVekaArgumentKind::Number:
					if (bLiteral && !Value->Value.IsNumber())
					{
						Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("NotNumber", "'{0}' expects a number"), FText::FromName(Argument.Name)), At);
					}
					break;
				default:
					break;
				}
			}
		}

		void CheckProperties(const UScriptStruct* Struct, const TArray<FVekaAstArgument>& Arguments, FName Owner, const FVekaSourceRange& Range)
		{
			for (const FVekaAstArgument& Argument : Arguments)
			{
				bool bFound = Struct->FindPropertyByName(Argument.Name) != nullptr;
				for (TFieldIterator<FProperty> It(Struct); It && !bFound; ++It)
				{
					bFound = It->GetName().Equals(Argument.Name.ToString(), ESearchCase::IgnoreCase);
				}
				if (!bFound)
				{
					Add(EVekaDiagnosticSeverity::Error, FText::Format(LOCTEXT("UnknownProperty", "{0} has no property '{1}'"), FText::FromName(Owner), FText::FromName(Argument.Name)), Range);
				}
			}
		}

		const FVekaDocument& Document;
	};
}

namespace Veka::Lang
{
	FString Print(const FVekaDocument& Document, FName ViewName)
	{
		return FPrinter(Document).Run(ViewName);
	}

	FString PrintExpression(const FVekaDocument& Document, int32 Expression)
	{
		return FPrinter(Document).PrintOne(Expression);
	}

	FString PrintValue(const FVekaValue& Value)
	{
		return ::PrintValue(Value);
	}

	bool StructurallyEqual(const FVekaDocument& A, const FVekaDocument& B, FString* OutDifference)
	{
		FComparer Comparer(A, B);
		const bool bSame = Comparer.Run();
		if (OutDifference)
		{
			*OutDifference = Comparer.Difference;
		}
		return bSame;
	}

	TArray<FVekaDiagnostic> Analyze(const FVekaDocument& Document)
	{
		FAnalyzer Analyzer(Document);
		Analyzer.Run();
		return Analyzer.Diagnostics;
	}

	TFunction<UVekaView*(FName)> MakeAssetResolver()
	{
		return [](FName Name) -> UVekaView*
		{
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			TArray<FAssetData> Assets;
			Registry.GetAssetsByClass(UVekaView::StaticClass()->GetClassPathName(), Assets, true);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.AssetName == Name)
				{
					return Cast<UVekaView>(Asset.GetAsset());
				}
			}
			return nullptr;
		};
	}
}

#undef LOCTEXT_NAMESPACE
