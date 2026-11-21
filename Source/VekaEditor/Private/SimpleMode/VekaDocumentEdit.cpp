// Copyright TheMiniBoss. All Rights Reserved.

#include "SimpleMode/VekaDocumentEdit.h"
#include "View/VekaCatalog.h"
#include "View/VekaViewAsset.h"
#include "VekaLanguage.h"

namespace
{
	using namespace Veka::Editor;

	TArray<int32>* ListOf(FVekaDocument& Document, int32 Parent, bool bElse)
	{
		if (!Document.Nodes.IsValidIndex(Parent))
		{
			return nullptr;
		}
		return bElse ? &Document.Nodes[Parent].ElseChildren : &Document.Nodes[Parent].Children;
	}

	int32 TextLiteral(FVekaDocument& Document, const FString& Text, const FString& TextNamespace)
	{
		if (!TextNamespace.IsEmpty())
		{
			TArray<FVekaDiagnostic> Ignored;
			const int32 Expr = Veka::Lang::ParseExpression(Document, Veka::Lang::PrintValue(FVekaValue::Text(FText::FromString(Text))), {}, TextNamespace, Ignored);
			if (Expr != INDEX_NONE)
			{
				return Expr;
			}
		}
		return Doc::AddLiteral(Document, FVekaValue::Text(FText::FromString(Text)));
	}

	int32 StateOfType(FVekaDocument& Document, EVekaValueType Type)
	{
		for (const FVekaStateDeclaration& State : Document.States)
		{
			if (State.Initial.Type == Type)
			{
				FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
				Expr.Kind = EVekaExprKind::State;
				Expr.Name = State.Name;
				return Document.Expressions.Num() - 1;
			}
		}
		return INDEX_NONE;
	}

	void SetArg(TArray<FVekaAstArgument>& Arguments, FName Name, int32 Expr)
	{
		Arguments.RemoveAll([Name](const FVekaAstArgument& Argument) { return Argument.Name == Name; });
		if (Expr != INDEX_NONE)
		{
			FVekaAstArgument& Argument = Arguments.AddDefaulted_GetRef();
			Argument.Name = Name;
			Argument.Expr = Expr;
		}
	}

	/** Copies an expression tree (Duplicate). */
	int32 CopyExpr(FVekaDocument& Document, int32 Index)
	{
		if (!Document.Expressions.IsValidIndex(Index))
		{
			return INDEX_NONE;
		}
		FVekaAstExpr Copy = Document.Expressions[Index];
		for (int32& Operand : Copy.Operands)
		{
			Operand = CopyExpr(Document, Operand);
		}
		Document.Expressions.Add(MoveTemp(Copy));
		return Document.Expressions.Num() - 1;
	}

	void CopyArguments(FVekaDocument& Document, TArray<FVekaAstArgument>& Arguments)
	{
		for (FVekaAstArgument& Argument : Arguments)
		{
			Argument.Expr = CopyExpr(Document, Argument.Expr);
		}
	}

	void CopyActions(FVekaDocument& Document, TArray<FVekaAstAction>& InActions)
	{
		for (FVekaAstAction& Action : InActions)
		{
			Action.Expr = CopyExpr(Document, Action.Expr);
		}
	}

	int32 CopyNode(FVekaDocument& Document, int32 Index)
	{
		if (!Document.Nodes.IsValidIndex(Index))
		{
			return INDEX_NONE;
		}
		FVekaAstNode Copy = Document.Nodes[Index];
		Copy.Expr = CopyExpr(Document, Copy.Expr);
		CopyArguments(Document, Copy.Arguments);
		CopyActions(Document, Copy.Actions);
		for (FVekaAstModifier& Modifier : Copy.Modifiers)
		{
			CopyArguments(Document, Modifier.Arguments);
			CopyActions(Document, Modifier.Actions);
		}
		for (int32& Child : Copy.Children)
		{
			Child = CopyNode(Document, Child);
		}
		for (int32& Child : Copy.ElseChildren)
		{
			Child = CopyNode(Document, Child);
		}
		Document.Nodes.Add(MoveTemp(Copy));
		return Document.Nodes.Num() - 1;
	}

	/** Default value of a catalog argument, for a new node or modifier. */
	int32 DefaultFor(FVekaDocument& Document, FName Owner, const FVekaCatalogArgument& Argument)
	{
		static const TMap<FName, double> Numbers = {
			{ TEXT("padding"), 16.0 }, { TEXT("frame"), 120.0 }, { TEXT("opacity"), 0.5 }, { TEXT("scale"), 1.2 }, { TEXT("rotation"), 15.0 }, { TEXT("fontSize"), 17.0 },
			{ TEXT("background"), 8.0 }, { TEXT("border"), 1.0 }, { TEXT("offset"), 10.0 },
		};
		switch (Argument.Kind)
		{
		case EVekaArgumentKind::Number:
		{
			const double* Value = Numbers.Find(Owner);
			const double Number = Value ? *Value : 1.0;
			return Doc::AddLiteral(Document, FMath::IsNearlyEqual(Number, FMath::RoundToDouble(Number)) ? FVekaValue::Int(static_cast<int64>(Number)) : FVekaValue::Float(Number));
		}
		case EVekaArgumentKind::Bool: return Doc::AddLiteral(Document, FVekaValue::Bool(true));
		case EVekaArgumentKind::Enum:
			return Argument.Enum ? Doc::AddLiteral(Document, FVekaValue::String(Argument.Enum->GetNameStringByIndex(Owner == TEXT("font") ? 4 : 0))) : INDEX_NONE;
		case EVekaArgumentKind::Paint:
			return Doc::AddLiteral(Document, FVekaValue::String(Owner == TEXT("background") ? TEXT("Fill") : Owner == TEXT("border") ? TEXT("Separator") : TEXT("Tint")));
		case EVekaArgumentKind::Name: return Owner == TEXT("transition") ? Doc::AddLiteral(Document, FVekaValue::String(TEXT("opacity"))) : INDEX_NONE;
		default: return INDEX_NONE;
		}
	}

	void VisitExpressions(const FVekaDocument& Document, int32 Index, TFunctionRef<void(int32)> Visit)
	{
		if (!Document.Expressions.IsValidIndex(Index))
		{
			return;
		}
		Visit(Index);
		for (const int32 Operand : Document.Expressions[Index].Operands)
		{
			VisitExpressions(Document, Operand, Visit);
		}
	}
}

namespace Veka::Editor::Doc
{
	FVekaNodeLocation FindLocation(const FVekaDocument& Document, int32 Node)
	{
		FVekaNodeLocation Location;
		if (Node == Document.Root)
		{
			Location.Index = 0;
			return Location;
		}
		for (int32 Parent = 0; Parent < Document.Nodes.Num(); ++Parent)
		{
			int32 Found = Document.Nodes[Parent].Children.IndexOfByKey(Node);
			if (Found != INDEX_NONE)
			{
				Location.Parent = Parent;
				Location.Index = Found;
				return Location;
			}
			Found = Document.Nodes[Parent].ElseChildren.IndexOfByKey(Node);
			if (Found != INDEX_NONE)
			{
				Location.Parent = Parent;
				Location.bElse = true;
				Location.Index = Found;
				return Location;
			}
		}
		Location.Index = INDEX_NONE;
		return Location;
	}

	bool CanHaveChildren(const FVekaDocument& Document, int32 Node)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return false;
		}
		const FVekaAstNode& N = Document.Nodes[Node];
		if (N.Kind == EVekaAstNodeKind::If || N.Kind == EVekaAstNodeKind::ForEach)
		{
			return true;
		}
		if (N.Kind != EVekaAstNodeKind::Element)
		{
			return false;
		}
		if (N.Component)
		{
			return N.Component->Document.Slots.Num() > 0;
		}
		const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(N.Type);
		return Entry && Entry->bContainer;
	}

	bool IsInside(const FVekaDocument& Document, int32 Node, int32 Ancestor)
	{
		for (int32 At = Node; At != INDEX_NONE;)
		{
			if (At == Ancestor)
			{
				return true;
			}
			At = FindLocation(Document, At).Parent;
		}
		return false;
	}

	TArray<int32> Path(const FVekaDocument& Document, int32 Node)
	{
		TArray<int32> Out;
		for (int32 At = Node; Document.Nodes.IsValidIndex(At) && At != Document.Root;)
		{
			const FVekaNodeLocation Location = FindLocation(Document, At);
			if (Location.Index == INDEX_NONE)
			{
				return {};
			}
			Out.Insert(Location.Index * 2 + (Location.bElse ? 1 : 0), 0);
			At = Location.Parent;
		}
		return Out;
	}

	int32 FromPath(const FVekaDocument& Document, const TArray<int32>& InPath)
	{
		int32 At = Document.Root;
		for (const int32 Step : InPath)
		{
			if (!Document.Nodes.IsValidIndex(At))
			{
				return INDEX_NONE;
			}
			const TArray<int32>& List = (Step & 1) ? Document.Nodes[At].ElseChildren : Document.Nodes[At].Children;
			if (!List.IsValidIndex(Step / 2))
			{
				return INDEX_NONE;
			}
			At = List[Step / 2];
		}
		return Document.Nodes.IsValidIndex(At) ? At : INDEX_NONE;
	}

	int32 NodeAtLine(const FVekaDocument& Document, int32 Line)
	{
		int32 Best = INDEX_NONE;
		int32 BestLine = 0;
		TFunction<void(int32)> Visit = [&](int32 Node)
		{
			if (!Document.Nodes.IsValidIndex(Node))
			{
				return;
			}
			const FVekaAstNode& N = Document.Nodes[Node];
			if (N.Source.Line > 0 && N.Source.Line <= Line && N.Source.Line >= BestLine)
			{
				Best = Node;
				BestLine = N.Source.Line;
			}
			for (const int32 Child : N.Children)
			{
				Visit(Child);
			}
			for (const int32 Child : N.ElseChildren)
			{
				Visit(Child);
			}
		};
		Visit(Document.Root);
		return Best;
	}

	TArray<FName> LocalsAt(const FVekaDocument& Document, int32 Node, int32 Modifier)
	{
		TArray<FName> Out;
		for (int32 At = FindLocation(Document, Node).Parent; At != INDEX_NONE; At = FindLocation(Document, At).Parent)
		{
			const FVekaAstNode& N = Document.Nodes[At];
			if (N.Kind == EVekaAstNodeKind::ForEach)
			{
				if (!N.LoopVariable.IsNone())
				{
					Out.AddUnique(N.LoopVariable);
				}
				if (!N.IndexVariable.IsNone())
				{
					Out.AddUnique(N.IndexVariable);
				}
			}
		}
		if (Document.Nodes.IsValidIndex(Node) && Document.Nodes[Node].Modifiers.IsValidIndex(Modifier) && Document.Nodes[Node].Modifiers[Modifier].Name == TEXT("on"))
		{
			Out.AddUnique(TEXT("payload"));
		}
		return Out;
	}

	FString DisplayName(const FVekaDocument& Document, int32 Node)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return FString();
		}
		const FVekaAstNode& N = Document.Nodes[Node];
		FString Kind;
		switch (N.Kind)
		{
		case EVekaAstNodeKind::If: Kind = TEXT("if ") + Veka::Lang::PrintExpression(Document, N.Expr); break;
		case EVekaAstNodeKind::ForEach: Kind = TEXT("ForEach(") + Veka::Lang::PrintExpression(Document, N.Expr) + TEXT(")"); break;
		case EVekaAstNodeKind::Slot: Kind = TEXT("Slot(") + (N.Type.IsNone() ? FString(TEXT("default")) : N.Type.ToString()) + TEXT(")"); break;
		default: Kind = N.Type.ToString(); break;
		}
		return N.Comments.IsEmpty() ? Kind : FString::Printf(TEXT("%s  (%s)"), *N.Comments[0], *Kind);
	}

	int32 Insert(FVekaDocument& Document, FName Type, int32 Parent, bool bElse, int32 Index, UVekaView* Component, const FString& TextNamespace)
	{
		FVekaAstNode Node;
		if (Type == TEXT("if"))
		{
			Node.Kind = EVekaAstNodeKind::If;
			Node.Expr = AddLiteral(Document, FVekaValue::Bool(true));
		}
		else if (Type == TEXT("ForEach"))
		{
			Node.Kind = EVekaAstNodeKind::ForEach;
			Node.Expr = AddLiteral(Document, FVekaValue::Int(3));
			Node.LoopVariable = TEXT("item");
		}
		else if (Type == TEXT("Slot"))
		{
			Node.Kind = EVekaAstNodeKind::Slot;
			Node.Type = Document.Slots.IsEmpty() ? NAME_None : Document.Slots[0].Name;
		}
		else
		{
			Node.Kind = EVekaAstNodeKind::Element;
			Node.Type = Component ? Component->GetFName() : Type;
			Node.Component = Component;
			const FString Name = Type.ToString();
			auto Text = [&](FName Argument, const TCHAR* Value) { SetArg(Node.Arguments, Argument, TextLiteral(Document, Value, TextNamespace)); };
			auto Literal = [&](FName Argument, const FVekaValue& Value) { SetArg(Node.Arguments, Argument, AddLiteral(Document, Value)); };
			if (Name == TEXT("VStack") || Name == TEXT("HStack"))
			{
				Literal(TEXT("spacing"), FVekaValue::Int(8));
			}
			else if (Name == TEXT("Text"))
			{
				Text(TEXT("text"), TEXT("Text"));
			}
			else if (Name == TEXT("Symbol"))
			{
				Literal(TEXT("name"), FVekaValue::String(TEXT("star")));
			}
			else if (Name == TEXT("Label"))
			{
				Text(TEXT("title"), TEXT("Label"));
				Literal(TEXT("symbol"), FVekaValue::String(TEXT("star")));
			}
			else if (Name == TEXT("Button"))
			{
				Text(TEXT("title"), TEXT("Button"));
			}
			else if (Name == TEXT("Toggle"))
			{
				Text(TEXT("title"), TEXT("Toggle"));
				const int32 State = StateOfType(Document, EVekaValueType::Bool);
				SetArg(Node.Arguments, TEXT("isOn"), State != INDEX_NONE ? State : AddLiteral(Document, FVekaValue::Bool(false)));
			}
			else if (Name == TEXT("Slider") || Name == TEXT("ProgressView") || Name == TEXT("Gauge"))
			{
				const int32 State = Name == TEXT("Slider") ? StateOfType(Document, EVekaValueType::Float) : INDEX_NONE;
				SetArg(Node.Arguments, TEXT("value"), State != INDEX_NONE ? State : AddLiteral(Document, FVekaValue::Float(0.5)));
			}
			else if (Name == TEXT("Stepper"))
			{
				Text(TEXT("title"), TEXT("Stepper"));
				const int32 State = StateOfType(Document, EVekaValueType::Int);
				SetArg(Node.Arguments, TEXT("value"), State != INDEX_NONE ? State : AddLiteral(Document, FVekaValue::Int(0)));
			}
			else if (Name == TEXT("TextField"))
			{
				Text(TEXT("placeholder"), TEXT("Placeholder"));
				const int32 State = StateOfType(Document, EVekaValueType::String);
				if (State != INDEX_NONE)
				{
					SetArg(Node.Arguments, TEXT("text"), State);
				}
			}
			else if (Name == TEXT("RoundedRectangle"))
			{
				Literal(TEXT("radius"), FVekaValue::Int(12));
				Literal(TEXT("fill"), FVekaValue::String(TEXT("Tint")));
			}
			else if (Name == TEXT("Rectangle") || Name == TEXT("Capsule") || Name == TEXT("Circle") || Name == TEXT("Ellipse"))
			{
				Literal(TEXT("fill"), FVekaValue::String(TEXT("Tint")));
			}
		}
		Document.Nodes.Add(MoveTemp(Node));
		const int32 NewIndex = Document.Nodes.Num() - 1;
		if (!Document.Nodes.IsValidIndex(Document.Root))
		{
			Document.Root = NewIndex;
			return NewIndex;
		}
		TArray<int32>* List = ListOf(Document, Parent, bElse);
		if (!List)
		{
			List = &Document.Nodes[Document.Root].Children;
		}
		List->Insert(NewIndex, FMath::Clamp(Index == INDEX_NONE ? List->Num() : Index, 0, List->Num()));
		return NewIndex;
	}

	void Remove(FVekaDocument& Document, const TArray<int32>& Nodes)
	{
		for (const int32 Node : Nodes)
		{
			if (Node == Document.Root)
			{
				Document.Root = INDEX_NONE;
				continue;
			}
			for (FVekaAstNode& Parent : Document.Nodes)
			{
				Parent.Children.Remove(Node);
				Parent.ElseChildren.Remove(Node);
			}
		}
	}

	bool Move(FVekaDocument& Document, int32 Node, int32 NewParent, bool bElse, int32 Index)
	{
		if (!Document.Nodes.IsValidIndex(Node) || Node == Document.Root || IsInside(Document, NewParent, Node))
		{
			return false;
		}
		TArray<int32>* Target = ListOf(Document, NewParent, bElse);
		if (!Target)
		{
			return false;
		}
		const FVekaNodeLocation From = FindLocation(Document, Node);
		if (From.Parent == NewParent && From.bElse == bElse && From.Index < Index)
		{
			--Index;
		}
		Remove(Document, { Node });
		Target = ListOf(Document, NewParent, bElse);
		Target->Insert(Node, FMath::Clamp(Index == INDEX_NONE ? Target->Num() : Index, 0, Target->Num()));
		return true;
	}

	int32 Duplicate(FVekaDocument& Document, int32 Node)
	{
		const FVekaNodeLocation Location = FindLocation(Document, Node);
		if (Location.IsRoot() || Location.Index == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		const int32 Copy = CopyNode(Document, Node);
		ListOf(Document, Location.Parent, Location.bElse)->Insert(Copy, Location.Index + 1);
		return Copy;
	}

	int32 Wrap(FVekaDocument& Document, const TArray<int32>& Nodes, FName Container)
	{
		if (Nodes.IsEmpty())
		{
			return INDEX_NONE;
		}
		const FVekaNodeLocation First = FindLocation(Document, Nodes[0]);
		FVekaAstNode Wrapper;
		Wrapper.Kind = EVekaAstNodeKind::Element;
		Wrapper.Type = Container;
		if (Container != TEXT("ZStack"))
		{
			SetArg(Wrapper.Arguments, TEXT("spacing"), AddLiteral(Document, FVekaValue::Int(8)));
		}
		// Keep the document order of the wrapped nodes.
		TArray<int32> Ordered = Nodes;
		if (!First.IsRoot())
		{
			const TArray<int32>& Siblings = *ListOf(Document, First.Parent, First.bElse);
			Ordered.Sort([&Siblings](int32 A, int32 B) { return Siblings.IndexOfByKey(A) < Siblings.IndexOfByKey(B); });
		}
		Wrapper.Children = Ordered;
		Document.Nodes.Add(MoveTemp(Wrapper));
		const int32 WrapperIndex = Document.Nodes.Num() - 1;
		if (First.IsRoot())
		{
			Document.Root = WrapperIndex;
			return WrapperIndex;
		}
		TArray<int32>& List = *ListOf(Document, First.Parent, First.bElse);
		const int32 At = Ordered.Num() > 0 ? List.IndexOfByKey(Ordered[0]) : First.Index;
		for (const int32 Node : Ordered)
		{
			for (FVekaAstNode& Parent : Document.Nodes)
			{
				if (&Parent != &Document.Nodes[WrapperIndex])
				{
					Parent.Children.Remove(Node);
					Parent.ElseChildren.Remove(Node);
				}
			}
		}
		ListOf(Document, First.Parent, First.bElse)->Insert(WrapperIndex, FMath::Clamp(At, 0, ListOf(Document, First.Parent, First.bElse)->Num()));
		return WrapperIndex;
	}

	void SetLabel(FVekaDocument& Document, int32 Node, const FString& Label)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return;
		}
		TArray<FString>& Comments = Document.Nodes[Node].Comments;
		const FString Trimmed = Label.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			if (!Comments.IsEmpty())
			{
				Comments.RemoveAt(0);
			}
		}
		else if (Comments.IsEmpty())
		{
			Comments.Add(Trimmed);
		}
		else
		{
			Comments[0] = Trimmed;
		}
	}

	void Compact(FVekaDocument& Document)
	{
		FVekaDocument Out = Document;
		Out.Nodes.Reset();
		Out.Expressions.Reset();
		TFunction<int32(int32)> CopyExpression = [&](int32 Index) -> int32
		{
			if (!Document.Expressions.IsValidIndex(Index))
			{
				return INDEX_NONE;
			}
			FVekaAstExpr Expr = Document.Expressions[Index];
			for (int32& Operand : Expr.Operands)
			{
				Operand = CopyExpression(Operand);
			}
			Out.Expressions.Add(MoveTemp(Expr));
			return Out.Expressions.Num() - 1;
		};
		auto CopyArgs = [&](TArray<FVekaAstArgument>& Arguments)
		{
			for (FVekaAstArgument& Argument : Arguments)
			{
				Argument.Expr = CopyExpression(Argument.Expr);
			}
			Arguments.RemoveAll([](const FVekaAstArgument& Argument) { return Argument.Expr == INDEX_NONE; });
		};
		auto CopyActs = [&](TArray<FVekaAstAction>& InActions)
		{
			for (FVekaAstAction& Action : InActions)
			{
				Action.Expr = CopyExpression(Action.Expr);
			}
		};
		TFunction<int32(int32)> CopyNodeTree = [&](int32 Index) -> int32
		{
			if (!Document.Nodes.IsValidIndex(Index))
			{
				return INDEX_NONE;
			}
			const int32 Slot = Out.Nodes.AddDefaulted();
			FVekaAstNode Node = Document.Nodes[Index];
			Node.Expr = CopyExpression(Node.Expr);
			CopyArgs(Node.Arguments);
			CopyActs(Node.Actions);
			for (FVekaAstModifier& Modifier : Node.Modifiers)
			{
				CopyArgs(Modifier.Arguments);
				CopyActs(Modifier.Actions);
			}
			TArray<int32> Children;
			for (const int32 Child : Node.Children)
			{
				const int32 Copied = CopyNodeTree(Child);
				if (Copied != INDEX_NONE)
				{
					Children.Add(Copied);
				}
			}
			TArray<int32> Else;
			for (const int32 Child : Node.ElseChildren)
			{
				const int32 Copied = CopyNodeTree(Child);
				if (Copied != INDEX_NONE)
				{
					Else.Add(Copied);
				}
			}
			Node.Children = MoveTemp(Children);
			Node.ElseChildren = MoveTemp(Else);
			Out.Nodes[Slot] = MoveTemp(Node);
			return Slot;
		};
		Out.Root = CopyNodeTree(Document.Root);
		for (FVekaDocumentTimeline& Timeline : Out.Timelines)
		{
			for (FVekaTimelineCue& Cue : Timeline.Cues)
			{
				CopyActs(Cue.Actions);
			}
		}
		Document = MoveTemp(Out);
	}

	int32 AddLiteral(FVekaDocument& Document, const FVekaValue& Value)
	{
		FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
		Expr.Kind = EVekaExprKind::Literal;
		Expr.Value = Value;
		return Document.Expressions.Num() - 1;
	}

	void SetArgument(FVekaDocument& Document, int32 Node, int32 Modifier, FName Name, int32 Expr)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return;
		}
		FVekaAstNode& N = Document.Nodes[Node];
		if (Modifier == INDEX_NONE)
		{
			SetArg(N.Arguments, Name, Expr);
		}
		else if (N.Modifiers.IsValidIndex(Modifier))
		{
			SetArg(N.Modifiers[Modifier].Arguments, Name, Expr);
		}
	}

	int32 FindArgument(const FVekaDocument& Document, int32 Node, int32 Modifier, FName Name)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return INDEX_NONE;
		}
		const FVekaAstNode& N = Document.Nodes[Node];
		const TArray<FVekaAstArgument>* Arguments = Modifier == INDEX_NONE ? &N.Arguments : (N.Modifiers.IsValidIndex(Modifier) ? &N.Modifiers[Modifier].Arguments : nullptr);
		if (!Arguments)
		{
			return INDEX_NONE;
		}
		const FVekaAstArgument* Found = Arguments->FindByPredicate([Name](const FVekaAstArgument& Argument) { return Argument.Name == Name; });
		return Found ? Found->Expr : INDEX_NONE;
	}

	int32 AddModifier(FVekaDocument& Document, int32 Node, FName Name)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return INDEX_NONE;
		}
		FVekaAstModifier Modifier;
		Modifier.Name = Name;
		if (const FVekaCatalogEntry* Entry = Veka::Catalog::FindModifier(Name))
		{
			if (Name == TEXT("on"))
			{
				const UVekaView* Component = Document.Nodes[Node].Component;
				if (Component && !Component->Document.EventDeclarations.IsEmpty())
				{
					SetArg(Modifier.Arguments, TEXT("event"), AddLiteral(Document, FVekaValue::String(Component->Document.EventDeclarations[0].Name.ToString())));
				}
			}
			else if (Name == TEXT("frame"))
			{
				SetArg(Modifier.Arguments, TEXT("width"), DefaultFor(Document, Name, *Entry->FindArgument(TEXT("width"))));
			}
			else if (Name == TEXT("offset"))
			{
				SetArg(Modifier.Arguments, TEXT("x"), DefaultFor(Document, Name, *Entry->FindArgument(TEXT("x"))));
			}
			else if (const FVekaCatalogArgument* Primary = Entry->FindArgument(Entry->PrimaryArgument))
			{
				SetArg(Modifier.Arguments, Primary->Name, DefaultFor(Document, Name, *Primary));
				if (Name == TEXT("background"))
				{
					SetArg(Modifier.Arguments, TEXT("radius"), AddLiteral(Document, FVekaValue::Int(8)));
				}
			}
		}
		FVekaAstNode& N = Document.Nodes[Node];
		N.Modifiers.Add(MoveTemp(Modifier));
		return N.Modifiers.Num() - 1;
	}

	void RemoveModifier(FVekaDocument& Document, int32 Node, int32 Modifier)
	{
		if (Document.Nodes.IsValidIndex(Node) && Document.Nodes[Node].Modifiers.IsValidIndex(Modifier))
		{
			Document.Nodes[Node].Modifiers.RemoveAt(Modifier);
		}
	}

	void MoveModifier(FVekaDocument& Document, int32 Node, int32 From, int32 To)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return;
		}
		TArray<FVekaAstModifier>& Modifiers = Document.Nodes[Node].Modifiers;
		if (!Modifiers.IsValidIndex(From) || !Modifiers.IsValidIndex(To) || From == To)
		{
			return;
		}
		FVekaAstModifier Moved = MoveTemp(Modifiers[From]);
		Modifiers.RemoveAt(From);
		Modifiers.Insert(MoveTemp(Moved), To);
	}

	TArray<FVekaAstAction>* Actions(FVekaDocument& Document, int32 Node, int32 Modifier)
	{
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return nullptr;
		}
		FVekaAstNode& N = Document.Nodes[Node];
		if (Modifier == INDEX_NONE)
		{
			return &N.Actions;
		}
		return N.Modifiers.IsValidIndex(Modifier) ? &N.Modifiers[Modifier].Actions : nullptr;
	}

	FName UniqueName(const FVekaDocument& Document, const FString& Base)
	{
		auto Used = [&Document](FName Name)
		{
			return Document.States.ContainsByPredicate([Name](const FVekaStateDeclaration& Item) { return Item.Name == Name; })
				|| Document.Params.ContainsByPredicate([Name](const FVekaParamDeclaration& Item) { return Item.Name == Name; })
				|| Document.EventDeclarations.ContainsByPredicate([Name](const FVekaEventDeclaration& Item) { return Item.Name == Name; })
				|| Document.Slots.ContainsByPredicate([Name](const FVekaSlotDeclaration& Item) { return Item.Name == Name; })
				|| Document.Timelines.ContainsByPredicate([Name](const FVekaDocumentTimeline& Item) { return Item.Name == Name; });
		};
		FName Name(*Base);
		for (int32 Suffix = 2; Used(Name); ++Suffix)
		{
			Name = FName(*FString::Printf(TEXT("%s%d"), *Base, Suffix));
		}
		return Name;
	}

	void AddState(FVekaDocument& Document, FName Name, const FVekaValue& Initial)
	{
		FVekaStateDeclaration& State = Document.States.AddDefaulted_GetRef();
		State.Name = Name;
		State.Initial = Initial;
	}

	void AddParam(FVekaDocument& Document, FName Name, EVekaValueType Type, const FVekaValue& Default)
	{
		FVekaParamDeclaration& Param = Document.Params.AddDefaulted_GetRef();
		Param.Name = Name;
		Param.Type = Type;
		Param.Default = Default;
	}

	void AddEvent(FVekaDocument& Document, FName Name, EVekaValueType PayloadType)
	{
		FVekaEventDeclaration& Event = Document.EventDeclarations.AddDefaulted_GetRef();
		Event.Name = Name;
		Event.PayloadType = PayloadType;
	}

	void AddSlot(FVekaDocument& Document, FName Name)
	{
		Document.Slots.AddDefaulted_GetRef().Name = Name;
	}

	void RemoveDeclaration(FVekaDocument& Document, FName Name)
	{
		Document.States.RemoveAll([Name](const FVekaStateDeclaration& Item) { return Item.Name == Name; });
		Document.Params.RemoveAll([Name](const FVekaParamDeclaration& Item) { return Item.Name == Name; });
		Document.EventDeclarations.RemoveAll([Name](const FVekaEventDeclaration& Item) { return Item.Name == Name; });
		Document.Slots.RemoveAll([Name](const FVekaSlotDeclaration& Item) { return Item.Name == Name; });
		Document.Timelines.RemoveAll([Name](const FVekaDocumentTimeline& Item) { return Item.Name == Name; });
	}

	bool RenameDeclaration(FVekaDocument& Document, FName Old, FName New)
	{
		if (Old == New)
		{
			return true;
		}
		if (New.IsNone() || UniqueName(Document, New.ToString()) != New)
		{
			return false;
		}
		bool bState = false;
		bool bParam = false;
		bool bEvent = false;
		bool bSlot = false;
		bool bTimeline = false;
		for (FVekaDocumentTimeline& Item : Document.Timelines)
		{
			if (Item.Name == Old)
			{
				Item.Name = New;
				bTimeline = true;
			}
		}
		for (FVekaStateDeclaration& Item : Document.States)
		{
			if (Item.Name == Old)
			{
				Item.Name = New;
				bState = true;
			}
		}
		for (FVekaParamDeclaration& Item : Document.Params)
		{
			if (Item.Name == Old)
			{
				Item.Name = New;
				bParam = true;
			}
		}
		for (FVekaEventDeclaration& Item : Document.EventDeclarations)
		{
			if (Item.Name == Old)
			{
				Item.Name = New;
				bEvent = true;
			}
		}
		for (FVekaSlotDeclaration& Item : Document.Slots)
		{
			if (Item.Name == Old)
			{
				Item.Name = New;
				bSlot = true;
			}
		}
		for (FVekaAstExpr& Expr : Document.Expressions)
		{
			if ((bState && Expr.Kind == EVekaExprKind::State && Expr.Name == Old) || (bParam && Expr.Kind == EVekaExprKind::Param && Expr.Name == Old)
				|| (bTimeline && Expr.Kind == EVekaExprKind::Timeline && Expr.Name == Old))
			{
				Expr.Name = New;
			}
		}
		auto RenameActions = [&](TArray<FVekaAstAction>& InActions)
		{
			for (FVekaAstAction& Action : InActions)
			{
				const bool bEmit = Action.Kind == EVekaActionKind::Emit;
				const bool bControl = Action.Kind >= EVekaActionKind::Play;
				if (Action.Target == Old && ((bEmit && bEvent) || (bControl && bTimeline) || (!bEmit && !bControl && bState)))
				{
					Action.Target = New;
				}
			}
		};
		for (FVekaDocumentTimeline& Timeline : Document.Timelines)
		{
			for (FVekaTimelineCue& Cue : Timeline.Cues)
			{
				RenameActions(Cue.Actions);
			}
		}
		for (FVekaAstNode& Node : Document.Nodes)
		{
			RenameActions(Node.Actions);
			for (FVekaAstModifier& Modifier : Node.Modifiers)
			{
				RenameActions(Modifier.Actions);
			}
			if (bSlot && Node.Kind == EVekaAstNodeKind::Slot && Node.Type == Old)
			{
				Node.Type = New;
			}
		}
		return true;
	}
}
