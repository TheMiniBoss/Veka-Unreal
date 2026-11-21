// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaDocument.h"

TArray<FString> FVekaDocument::Validate() const
{
	TArray<FString> Problems;
	TSet<FName> StateNames;
	for (const FVekaStateDeclaration& State : States)
	{
		if (StateNames.Contains(State.Name))
		{
			Problems.Add(FString::Printf(TEXT("State '%s' declared twice"), *State.Name.ToString()));
		}
		StateNames.Add(State.Name);
	}
	const TSet<FName> EventNames(Events);
	auto CheckExpr = [&](int32 Index, const TCHAR* Where)
	{
		if (Index != INDEX_NONE && !Expressions.IsValidIndex(Index))
		{
			Problems.Add(FString::Printf(TEXT("%s: expression %d out of range"), Where, Index));
		}
	};
	for (int32 Index = 0; Index < Expressions.Num(); ++Index)
	{
		const FVekaAstExpr& Expr = Expressions[Index];
		for (const int32 Operand : Expr.Operands)
		{
			if (!Expressions.IsValidIndex(Operand) || Operand == Index)
			{
				Problems.Add(FString::Printf(TEXT("Expression %d: operand %d out of range"), Index, Operand));
			}
		}
		if (Expr.Kind == EVekaExprKind::State && !StateNames.Contains(Expr.Name))
		{
			Problems.Add(FString::Printf(TEXT("Expression %d reads unknown state '%s'"), Index, *Expr.Name.ToString()));
		}
	}
	auto CheckActions = [&](const TArray<FVekaAstAction>& Actions, int32 Node)
	{
		for (const FVekaAstAction& Action : Actions)
		{
			CheckExpr(Action.Expr, TEXT("Action"));
			if (Action.Kind == EVekaActionKind::Emit)
			{
				if (!EventNames.Contains(Action.Target))
				{
					Problems.Add(FString::Printf(TEXT("Node %d emits undeclared event '%s'"), Node, *Action.Target.ToString()));
				}
			}
			else if (!StateNames.Contains(Action.Target))
			{
				Problems.Add(FString::Printf(TEXT("Node %d changes unknown state '%s'"), Node, *Action.Target.ToString()));
			}
		}
	};
	if (!Nodes.IsValidIndex(Root))
	{
		Problems.Add(TEXT("No root node"));
	}
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FVekaAstNode& Node = Nodes[Index];
		for (const int32 Child : Node.Children)
		{
			if (!Nodes.IsValidIndex(Child) || Child == Index)
			{
				Problems.Add(FString::Printf(TEXT("Node %d: child %d out of range"), Index, Child));
			}
		}
		for (const int32 Child : Node.ElseChildren)
		{
			if (!Nodes.IsValidIndex(Child) || Child == Index)
			{
				Problems.Add(FString::Printf(TEXT("Node %d: else child %d out of range"), Index, Child));
			}
		}
		for (const FVekaAstArgument& Argument : Node.Arguments)
		{
			CheckExpr(Argument.Expr, TEXT("Argument"));
		}
		for (const FVekaAstModifier& Modifier : Node.Modifiers)
		{
			for (const FVekaAstArgument& Argument : Modifier.Arguments)
			{
				CheckExpr(Argument.Expr, TEXT("Modifier argument"));
			}
			CheckActions(Modifier.Actions, Index);
		}
		CheckActions(Node.Actions, Index);
		if (Node.Kind != EVekaAstNodeKind::Element)
		{
			if (!Expressions.IsValidIndex(Node.Expr))
			{
				Problems.Add(FString::Printf(TEXT("Node %d: missing condition / count"), Index));
			}
		}
	}
	return Problems;
}

// --- Builder ---------------------------------------------------------------------------------------------------

FVekaAstNode& FVekaDocumentBuilder::FNode::Get() const
{
	return Builder->Document.Nodes[Index];
}

FVekaDocumentBuilder::FNode& FVekaDocumentBuilder::FNode::Arg(FName Name, int32 Expr)
{
	FVekaAstArgument& Argument = Get().Arguments.AddDefaulted_GetRef();
	Argument.Name = Name;
	Argument.Expr = Expr;
	return *this;
}

FVekaDocumentBuilder::FNode& FVekaDocumentBuilder::FNode::Modifier(FName Name, TArray<TPair<FName, int32>> Arguments, TArray<FVekaAstAction> Actions)
{
	FVekaAstModifier& Modifier = Get().Modifiers.AddDefaulted_GetRef();
	Modifier.Name = Name;
	for (const TPair<FName, int32>& Pair : Arguments)
	{
		FVekaAstArgument& Argument = Modifier.Arguments.AddDefaulted_GetRef();
		Argument.Name = Pair.Key;
		Argument.Expr = Pair.Value;
	}
	Modifier.Actions = MoveTemp(Actions);
	return *this;
}

FVekaDocumentBuilder::FNode& FVekaDocumentBuilder::FNode::Action(const FVekaAstAction& InAction)
{
	Get().Actions.Add(InAction);
	return *this;
}

FVekaDocumentBuilder::FNode& FVekaDocumentBuilder::FNode::Children(TArray<FNode> InChildren)
{
	for (const FNode& Child : InChildren)
	{
		Get().Children.Add(Child.Index);
	}
	return *this;
}

FVekaDocumentBuilder::FNode& FVekaDocumentBuilder::FNode::Else(TArray<FNode> InChildren)
{
	for (const FNode& Child : InChildren)
	{
		Get().ElseChildren.Add(Child.Index);
	}
	return *this;
}

void FVekaDocumentBuilder::State(FName Name, const FVekaValue& Initial)
{
	FVekaStateDeclaration& State = Document.States.AddDefaulted_GetRef();
	State.Name = Name;
	State.Initial = Initial;
}

void FVekaDocumentBuilder::Event(FName Name)
{
	Document.Events.AddUnique(Name);
}

FVekaDocumentBuilder::FNode FVekaDocumentBuilder::Node(FName Type)
{
	FVekaAstNode& Node = Document.Nodes.AddDefaulted_GetRef();
	Node.Kind = EVekaAstNodeKind::Element;
	Node.Type = Type;
	return FNode(*this, Document.Nodes.Num() - 1);
}

FVekaDocumentBuilder::FNode FVekaDocumentBuilder::If(int32 Condition, TArray<FNode> Then, TArray<FNode> Otherwise)
{
	FVekaAstNode& Node = Document.Nodes.AddDefaulted_GetRef();
	Node.Kind = EVekaAstNodeKind::If;
	Node.Expr = Condition;
	FNode Handle(*this, Document.Nodes.Num() - 1);
	Handle.Children(MoveTemp(Then));
	Handle.Else(MoveTemp(Otherwise));
	return Handle;
}

FVekaDocumentBuilder::FNode FVekaDocumentBuilder::ForEach(int32 Count, FName LoopVariable, TArray<FNode> Body)
{
	FVekaAstNode& Node = Document.Nodes.AddDefaulted_GetRef();
	Node.Kind = EVekaAstNodeKind::ForEach;
	Node.Expr = Count;
	Node.LoopVariable = LoopVariable;
	FNode Handle(*this, Document.Nodes.Num() - 1);
	Handle.Children(MoveTemp(Body));
	return Handle;
}

void FVekaDocumentBuilder::SetRoot(const FNode& Node)
{
	Document.Root = Node.GetIndex();
}

int32 FVekaDocumentBuilder::Lit(const FVekaValue& Value)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Literal;
	Expr.Value = Value;
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Var(FName State)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::State;
	Expr.Name = State;
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Model(FName Property)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Model;
	Expr.Name = Property;
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Local(FName Name)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Local;
	Expr.Name = Name;
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Unary(EVekaExprOp Op, int32 Operand)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Unary;
	Expr.Op = Op;
	Expr.Operands = { Operand };
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Binary(EVekaExprOp Op, int32 A, int32 B)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Binary;
	Expr.Op = Op;
	Expr.Operands = { A, B };
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Ternary(int32 Condition, int32 Then, int32 Otherwise)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Ternary;
	Expr.Operands = { Condition, Then, Otherwise };
	return Document.Expressions.Num() - 1;
}

int32 FVekaDocumentBuilder::Format(const FString& Pattern, TArray<int32> Arguments)
{
	FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
	Expr.Kind = EVekaExprKind::Format;
	Expr.Value = FVekaValue::String(Pattern);
	Expr.Operands = MoveTemp(Arguments);
	return Document.Expressions.Num() - 1;
}

FVekaAstAction FVekaDocumentBuilder::Set(FName State, int32 Expr, bool bAnimated)
{
	FVekaAstAction Action;
	Action.Kind = EVekaActionKind::Set;
	Action.Target = State;
	Action.Expr = Expr;
	Action.bAnimated = bAnimated;
	return Action;
}

FVekaAstAction FVekaDocumentBuilder::Toggle(FName State, bool bAnimated)
{
	FVekaAstAction Action;
	Action.Kind = EVekaActionKind::Toggle;
	Action.Target = State;
	Action.bAnimated = bAnimated;
	return Action;
}

FVekaAstAction FVekaDocumentBuilder::Increment(FName State, int32 Expr, bool bAnimated)
{
	FVekaAstAction Action;
	Action.Kind = EVekaActionKind::Increment;
	Action.Target = State;
	Action.Expr = Expr;
	Action.bAnimated = bAnimated;
	return Action;
}

FVekaAstAction FVekaDocumentBuilder::Emit(FName Event, int32 Payload)
{
	FVekaAstAction Action;
	Action.Kind = EVekaActionKind::Emit;
	Action.Target = Event;
	Action.Expr = Payload;
	return Action;
}
