// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/VekaValue.h"
#include "VekaDocument.generated.h"

/**
 * Asset views are stored as an AST (flat arrays, indices between them), interpreted at runtime. Phase 3's VekaLang
 * parses the DSL into this document and prints it back; until then documents are made in C++ (FVekaDocumentBuilder).
 */

UENUM(BlueprintType)
enum class EVekaExprKind : uint8
{
	Literal,
	/** Value of a state (`count`). */
	State,
	/** Property of the view model (`model.Health`). */
	Model,
	/** Loop variable of a ForEach. */
	Local,
	Unary,
	Binary,
	/** Operands: condition, then, else. */
	Ternary,
	/** Value.StringValue is a format with {0} {1}...; operands are the arguments. */
	Format
};

UENUM(BlueprintType)
enum class EVekaExprOp : uint8
{
	None,
	Not,
	Negate,
	Add,
	Subtract,
	Multiply,
	Divide,
	Modulo,
	Equal,
	NotEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	And,
	Or,
	Min,
	Max
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAstExpr
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	EVekaExprKind Kind = EVekaExprKind::Literal;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FVekaValue Value;

	/** State / model property / local name. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	EVekaExprOp Op = EVekaExprOp::None;

	/** Indices into FVekaDocument::Expressions. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<int32> Operands;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAstArgument
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Expr = INDEX_NONE;
};

UENUM(BlueprintType)
enum class EVekaActionKind : uint8
{
	/** Target = Expr. */
	Set,
	/** Target = !Target. */
	Toggle,
	/** Target += Expr. */
	Increment,
	/** Sends Target (event name) with Expr as payload to the host (OnVekaEvent). */
	Emit
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAstAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	EVekaActionKind Kind = EVekaActionKind::Set;

	/** State name (Set / Toggle / Increment) or event name (Emit). */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Target;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Expr = INDEX_NONE;

	/** Runs inside WithAnimation (spring): the views it changes animate. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	bool bAnimated = false;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAstModifier
{
	GENERATED_BODY()

	/** "padding", "background", "onTap"... or any schema modifier ("opacity"). */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstArgument> Arguments;

	/** Gestures (onTap). */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstAction> Actions;
};

UENUM(BlueprintType)
enum class EVekaAstNodeKind : uint8
{
	Element,
	/** Condition expression; Children if true, ElseChildren otherwise. */
	If,
	/** Count expression; Children repeated, LoopVariable = 0..Count-1. */
	ForEach
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAstNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	EVekaAstNodeKind Kind = EVekaAstNodeKind::Element;

	/** "VStack", "Text", "Button"... or any schema primitive. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Type;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstArgument> Arguments;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstModifier> Modifiers;

	/** Button action. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstAction> Actions;

	/** Indices into FVekaDocument::Nodes. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<int32> Children;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<int32> ElseChildren;

	/** If: condition. ForEach: count. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Expr = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName LoopVariable;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaStateDeclaration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FVekaValue Initial;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaDocument
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaStateDeclaration> States;

	/** Events the view may emit (documentation and validation). */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FName> Events;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstNode> Nodes;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstExpr> Expressions;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Root = INDEX_NONE;

	/** Problems a reader would hit (bad indices, unknown states / events); empty when valid. */
	TArray<FString> Validate() const;
};

/**
 * Builds a document in C++ (Phase 3 builds it from the DSL):
 *
 *   FVekaDocumentBuilder B;
 *   B.State(TEXT("count"), FVekaValue::Int(0));
 *   B.SetRoot(B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(8)).Children({
 *       B.Node(TEXT("Text")).Arg(TEXT("text"), B.Format(TEXT("Count: {0}"), { B.Var(TEXT("count")) })),
 *       B.Node(TEXT("Button")).Arg(TEXT("title"), B.Lit(TEXT("+1"))).Action(B.Increment(TEXT("count"), B.Lit(1))) }));
 */
class VEKAUI_API FVekaDocumentBuilder
{
public:
	/** A node being built (handle into the builder). */
	class VEKAUI_API FNode
	{
	public:
		FNode(FVekaDocumentBuilder& InBuilder, int32 InIndex) : Builder(&InBuilder), Index(InIndex) {}

		FNode& Arg(FName Name, int32 Expr);
		FNode& Modifier(FName Name, TArray<TPair<FName, int32>> Arguments = {}, TArray<FVekaAstAction> Actions = {});
		FNode& OnTap(TArray<FVekaAstAction> Actions) { return Modifier(TEXT("onTap"), {}, MoveTemp(Actions)); }
		FNode& Action(const FVekaAstAction& InAction);
		FNode& Children(TArray<FNode> InChildren);
		FNode& Else(TArray<FNode> InChildren);
		int32 GetIndex() const { return Index; }

	private:
		FVekaAstNode& Get() const;
		FVekaDocumentBuilder* Builder;
		int32 Index;
	};

	void State(FName Name, const FVekaValue& Initial);
	void Event(FName Name);

	FNode Node(FName Type);
	FNode If(int32 Condition, TArray<FNode> Then, TArray<FNode> Otherwise = {});
	FNode ForEach(int32 Count, FName LoopVariable, TArray<FNode> Body);
	void SetRoot(const FNode& Node);

	int32 Lit(const FVekaValue& Value);
	int32 Lit(bool Value) { return Lit(FVekaValue::Bool(Value)); }
	int32 Lit(int32 Value) { return Lit(FVekaValue::Int(Value)); }
	int32 Lit(double Value) { return Lit(FVekaValue::Float(Value)); }
	int32 Lit(const TCHAR* Value) { return Lit(FVekaValue::String(Value)); }
	int32 Lit(const FLinearColor& Value) { return Lit(FVekaValue::Color(Value)); }
	int32 Var(FName State);
	int32 Model(FName Property);
	int32 Local(FName Name);
	int32 Unary(EVekaExprOp Op, int32 Operand);
	int32 Binary(EVekaExprOp Op, int32 A, int32 B);
	int32 Ternary(int32 Condition, int32 Then, int32 Otherwise);
	int32 Format(const FString& Pattern, TArray<int32> Arguments);

	static FVekaAstAction Set(FName State, int32 Expr, bool bAnimated = false);
	static FVekaAstAction Toggle(FName State, bool bAnimated = false);
	static FVekaAstAction Increment(FName State, int32 Expr, bool bAnimated = false);
	static FVekaAstAction Emit(FName Event, int32 Payload = INDEX_NONE);

	const FVekaDocument& GetDocument() const { return Document; }
	FVekaDocument&& Release() { return MoveTemp(Document); }

private:
	FVekaDocument Document;
};
