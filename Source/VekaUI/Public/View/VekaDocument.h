// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/VekaValue.h"
#include "VekaDocument.generated.h"

class UVekaView;

/**
 * Asset views are stored as an AST (flat arrays, indices between them), interpreted at runtime. Phase 3's VekaLang
 * parses the DSL into this document and prints it back; until then documents are made in C++ (FVekaDocumentBuilder).
 */

/** Where a node or an expression comes from in the .vk source (1-based; 0 = not from source). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSourceRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Line = 0;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Column = 0;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Length = 0;

	bool IsSet() const { return Line > 0; }
};

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
	/** Value is the pattern with {0} {1}... (a Text pattern is localizable); operands are the arguments. */
	Format,
	/** Parameter of the view (`@Param var title`). */
	Param,
	/** Field Name of operand 0 (`item.name`). */
	Member
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
	Max,
	/** Operand 0 [operand 1]. */
	Index
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

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FVekaSourceRange Source;
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
	/** Expr is a count or an array; Children repeated with LoopVariable = the item (or the index) and IndexVariable = the index. */
	ForEach,
	/** Inside a component: the content the caller gave for slot Type (None = the default slot). */
	Slot
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

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName IndexVariable;

	/** Another view used as a component: arguments set its parameters, children fill its slots, `on` modifiers handle its events. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TObjectPtr<UVekaView> Component;

	/** Child of a component call: the slot it fills (None = the default slot). */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName SlotName;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FVekaSourceRange Source;

	/** Comment lines written above the node in the source (kept by parse / print). */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FString> Comments;
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

/** `@Param var title: Text = "Hello"`: set by the caller when the view is used as a component. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaParamDeclaration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	EVekaValueType Type = EVekaValueType::None;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FVekaValue Default;
};

/** `@Slot accessory`: content the caller injects (the default slot has no name). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSlotDeclaration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;
};

/** `@Event buy(price: Int)`. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaEventDeclaration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	EVekaValueType PayloadType = EVekaValueType::None;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaDocument
{
	GENERATED_BODY()

	/** 1 = Phase 2 (events as names); 2 = params, slots, components, typed events. Upgrade() converts. */
	static constexpr int32 CurrentFormatVersion = 2;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 FormatVersion = 0;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaStateDeclaration> States;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaParamDeclaration> Params;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaSlotDeclaration> Slots;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaEventDeclaration> EventDeclarations;

	/** Format 1 only (event names); moved to EventDeclarations by Upgrade(). */
	UPROPERTY()
	TArray<FName> Events;

	/** Comment lines at the top of the source. */
	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FString> Comments;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstNode> Nodes;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	TArray<FVekaAstExpr> Expressions;

	UPROPERTY(EditAnywhere, Category = "Veka|View")
	int32 Root = INDEX_NONE;

	/** Problems a reader would hit (bad indices, unknown states / params / events / slots); empty when valid. */
	TArray<FString> Validate() const;

	/** Converts an older format to the current one; true if something changed. */
	bool Upgrade();

	bool HasEvent(FName Name) const;
	const FVekaParamDeclaration* FindParam(FName Name) const;
	bool HasSlot(FName Name) const;
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
		/** Component handler: runs Actions when the component emits Event (payload in the local `payload`). */
		FNode& On(FName Event, TArray<FVekaAstAction> Actions) { return Modifier(TEXT("on"), { { TEXT("event"), Builder->Lit(FVekaValue::String(Event.ToString())) } }, MoveTemp(Actions)); }
		/** This node fills slot Name of the component it is a child of. */
		FNode& InSlot(FName Name);
		FNode& Comment(const FString& Line);
		FNode& IndexVariable(FName Name);
		int32 GetIndex() const { return Index; }

	private:
		FVekaAstNode& Get() const;
		FVekaDocumentBuilder* Builder;
		int32 Index;
	};

	void State(FName Name, const FVekaValue& Initial);
	void Event(FName Name, EVekaValueType PayloadType = EVekaValueType::None);
	void Param(FName Name, EVekaValueType Type, const FVekaValue& Default = FVekaValue());
	void Slot(FName Name = NAME_None);

	FNode Node(FName Type);
	FNode If(int32 Condition, TArray<FNode> Then, TArray<FNode> Otherwise = {});
	FNode ForEach(int32 Count, FName LoopVariable, TArray<FNode> Body);
	/** An instance of another view. */
	FNode Component(UVekaView* View);
	/** Where a component shows the content of slot Name. */
	FNode SlotContent(FName Name = NAME_None);
	void SetRoot(const FNode& Node);

	int32 Lit(const FVekaValue& Value);
	int32 Lit(bool Value) { return Lit(FVekaValue::Bool(Value)); }
	int32 Lit(int32 Value) { return Lit(FVekaValue::Int(Value)); }
	int32 Lit(double Value) { return Lit(FVekaValue::Float(Value)); }
	int32 Lit(const TCHAR* Value) { return Lit(FVekaValue::String(Value)); }
	int32 Lit(const FLinearColor& Value) { return Lit(FVekaValue::Color(Value)); }
	/** Localizable text literal. */
	int32 LitText(const FText& Value) { return Lit(FVekaValue::Text(Value)); }
	int32 Var(FName State);
	int32 Model(FName Property);
	int32 Local(FName Name);
	int32 Unary(EVekaExprOp Op, int32 Operand);
	int32 Binary(EVekaExprOp Op, int32 A, int32 B);
	int32 Ternary(int32 Condition, int32 Then, int32 Otherwise);
	int32 Format(const FString& Pattern, TArray<int32> Arguments);
	/** Localizable pattern (FText::Format). */
	int32 FormatText(const FText& Pattern, TArray<int32> Arguments);
	int32 ParamRef(FName Name);
	int32 Member(int32 Object, FName Field);
	int32 Index(int32 Array, int32 Item) { return Binary(EVekaExprOp::Index, Array, Item); }

	static FVekaAstAction Set(FName State, int32 Expr, bool bAnimated = false);
	static FVekaAstAction Toggle(FName State, bool bAnimated = false);
	static FVekaAstAction Increment(FName State, int32 Expr, bool bAnimated = false);
	static FVekaAstAction Emit(FName Event, int32 Payload = INDEX_NONE);

	const FVekaDocument& GetDocument() const { return Document; }
	FVekaDocument&& Release() { Document.FormatVersion = FVekaDocument::CurrentFormatVersion; return MoveTemp(Document); }

private:
	FVekaDocument Document;
};
