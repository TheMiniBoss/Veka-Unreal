// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaLanguage.h"
#include "View/VekaCatalog.h"
#include "View/VekaViewAsset.h"
#include "Internationalization/Internationalization.h"
#include "Misc/SecureHash.h"

#define LOCTEXT_NAMESPACE "VekaLang"

FString FVekaDiagnostic::ToString() const
{
	return FString::Printf(TEXT("%d:%d: %s: %s"), Range.Line, Range.Column, Severity == EVekaDiagnosticSeverity::Error ? TEXT("error") : TEXT("warning"), *Message.ToString());
}

bool FVekaParseResult::HasErrors() const
{
	return Diagnostics.ContainsByPredicate([](const FVekaDiagnostic& Diagnostic) { return Diagnostic.Severity == EVekaDiagnosticSeverity::Error; });
}

namespace Veka::Lang
{
	FString TypeName(EVekaValueType Type)
	{
		switch (Type)
		{
		case EVekaValueType::Bool: return TEXT("Bool");
		case EVekaValueType::Int: return TEXT("Int");
		case EVekaValueType::Float: return TEXT("Float");
		case EVekaValueType::String: return TEXT("String");
		case EVekaValueType::Color: return TEXT("Color");
		case EVekaValueType::Vector2: return TEXT("Vector2");
		case EVekaValueType::Text: return TEXT("Text");
		case EVekaValueType::Array: return TEXT("Array");
		case EVekaValueType::Struct: return TEXT("Struct");
		default: return TEXT("Any");
		}
	}
}

namespace
{
	// --- Lexer -----------------------------------------------------------------------------------------------------

	enum class ETok : uint8
	{
		End,
		Ident,
		Int,
		Float,
		/** "..." (localizable text) */
		Text,
		/** #"..." (plain string) */
		RawString,
		/** #RRGGBB[AA] */
		Hex,
		Punct
	};

	struct FToken
	{
		ETok Kind = ETok::End;
		FString Text;
		int32 Line = 1;
		int32 Column = 1;
		int32 Length = 0;
		/** Comment lines written since the previous token. */
		TArray<FString> Comments;
		/** Strings: literal parts around the \( ) interpolations. */
		TArray<FString> Parts;
		TArray<FString> Interpolations;
		TArray<FIntPoint> InterpolationPositions;

		bool Is(const TCHAR* Punct) const { return Kind == ETok::Punct && Text == Punct; }
		bool IsWord(const TCHAR* Word) const { return Kind == ETok::Ident && Text == Word; }
		FVekaSourceRange Range() const
		{
			FVekaSourceRange Out;
			Out.Line = Line;
			Out.Column = Column;
			Out.Length = Length;
			return Out;
		}
	};

	class FLexer
	{
	public:
		FLexer(const FString& InSource, int32 InLine, int32 InColumn, TArray<FVekaDiagnostic>& InDiagnostics)
			: Source(InSource), Line(InLine), Column(InColumn), Diagnostics(InDiagnostics)
		{
		}

		TArray<FToken> Run()
		{
			TArray<FToken> Tokens;
			TArray<FString> Comments;
			while (true)
			{
				SkipSpace();
				if (Peek() == TEXT('/') && Peek(1) == TEXT('/'))
				{
					Advance(2);
					FString Comment;
					while (!AtEnd() && Peek() != TEXT('\n'))
					{
						Comment.AppendChar(Peek());
						Advance();
					}
					Comments.Add(Comment.TrimStartAndEnd());
					continue;
				}
				FToken Token;
				Token.Line = Line;
				Token.Column = Column;
				Token.Comments = MoveTemp(Comments);
				Comments.Reset();
				if (AtEnd())
				{
					Token.Kind = ETok::End;
					Tokens.Add(MoveTemp(Token));
					return Tokens;
				}
				const int32 Start = Index;
				const TCHAR C = Peek();
				if (FChar::IsAlpha(C) || C == TEXT('_'))
				{
					while (!AtEnd() && (FChar::IsAlnum(Peek()) || Peek() == TEXT('_')))
					{
						Advance();
					}
					Token.Kind = ETok::Ident;
					Token.Text = Source.Mid(Start, Index - Start);
				}
				else if (FChar::IsDigit(C))
				{
					bool bFloat = false;
					while (!AtEnd() && FChar::IsDigit(Peek()))
					{
						Advance();
					}
					if (Peek() == TEXT('.') && FChar::IsDigit(Peek(1)))
					{
						bFloat = true;
						Advance();
						while (!AtEnd() && FChar::IsDigit(Peek()))
						{
							Advance();
						}
					}
					if ((Peek() == TEXT('e') || Peek() == TEXT('E')) && (FChar::IsDigit(Peek(1)) || ((Peek(1) == TEXT('-') || Peek(1) == TEXT('+')) && FChar::IsDigit(Peek(2)))))
					{
						bFloat = true;
						Advance(2);
						while (!AtEnd() && FChar::IsDigit(Peek()))
						{
							Advance();
						}
					}
					Token.Kind = bFloat ? ETok::Float : ETok::Int;
					Token.Text = Source.Mid(Start, Index - Start);
				}
				else if (C == TEXT('"') || (C == TEXT('#') && Peek(1) == TEXT('"')))
				{
					Token.Kind = C == TEXT('#') ? ETok::RawString : ETok::Text;
					Advance(C == TEXT('#') ? 2 : 1);
					ReadString(Token);
				}
				else if (C == TEXT('#'))
				{
					Advance();
					while (!AtEnd() && FChar::IsHexDigit(Peek()))
					{
						Advance();
					}
					Token.Kind = ETok::Hex;
					Token.Text = Source.Mid(Start, Index - Start);
					if (Token.Text.Len() != 7 && Token.Text.Len() != 9)
					{
						Error(Token, LOCTEXT("BadHex", "A color is written #RRGGBB or #RRGGBBAA"));
					}
				}
				else
				{
					static const TCHAR* Two[] = { TEXT("+="), TEXT("-="), TEXT("=="), TEXT("!="), TEXT("<="), TEXT(">="), TEXT("&&"), TEXT("||") };
					Token.Kind = ETok::Punct;
					const FString Pair = Source.Mid(Index, 2);
					bool bTwo = false;
					for (const TCHAR* Candidate : Two)
					{
						if (Pair == Candidate)
						{
							bTwo = true;
							break;
						}
					}
					if (bTwo)
					{
						Token.Text = Pair;
						Advance(2);
					}
					else if (FCString::Strchr(TEXT("(){}[],:.=<>!+-*/%?;@"), C))
					{
						Token.Text = FString(1, &C);
						Advance();
					}
					else
					{
						Token.Text = FString(1, &C);
						Advance();
						Error(Token, FText::Format(LOCTEXT("BadChar", "Unexpected character '{0}'"), FText::FromString(Token.Text)));
						continue;
					}
				}
				Token.Length = Index - Start;
				Tokens.Add(MoveTemp(Token));
			}
		}

	private:
		void ReadString(FToken& Token)
		{
			FString Part;
			while (true)
			{
				if (AtEnd() || Peek() == TEXT('\n'))
				{
					Error(Token, LOCTEXT("Unterminated", "Unterminated string (missing \")"));
					break;
				}
				const TCHAR C = Peek();
				if (C == TEXT('"'))
				{
					Advance();
					break;
				}
				if (C == TEXT('\\'))
				{
					const TCHAR Next = Peek(1);
					if (Next == TEXT('('))
					{
						// Interpolation: up to the matching parenthesis.
						Advance(2);
						const int32 ExprLine = Line;
						const int32 ExprColumn = Column;
						int32 Depth = 1;
						FString Expr;
						while (!AtEnd() && Peek() != TEXT('\n'))
						{
							const TCHAR E = Peek();
							if (E == TEXT('('))
							{
								++Depth;
							}
							else if (E == TEXT(')') && --Depth == 0)
							{
								break;
							}
							Expr.AppendChar(E);
							Advance();
						}
						if (Depth != 0)
						{
							Error(Token, LOCTEXT("UnterminatedInterpolation", "Unterminated \\( ) in a string"));
							break;
						}
						Advance();
						Token.Parts.Add(Part);
						Part.Reset();
						Token.Interpolations.Add(Expr);
						Token.InterpolationPositions.Add(FIntPoint(ExprLine, ExprColumn));
						continue;
					}
					Advance(2);
					switch (Next)
					{
					case TEXT('n'): Part.AppendChar(TEXT('\n')); break;
					case TEXT('t'): Part.AppendChar(TEXT('\t')); break;
					case TEXT('"'): Part.AppendChar(TEXT('"')); break;
					case TEXT('\\'): Part.AppendChar(TEXT('\\')); break;
					default:
						Part.AppendChar(Next);
						Error(Token, FText::Format(LOCTEXT("BadEscape", "Unknown escape \\{0}"), FText::FromString(FString(1, &Next))));
						break;
					}
					continue;
				}
				Part.AppendChar(C);
				Advance();
			}
			Token.Parts.Add(Part);
			Token.Text = FString::Join(Token.Parts, TEXT(""));
		}

		void Error(const FToken& Token, const FText& Message)
		{
			FVekaDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Severity = EVekaDiagnosticSeverity::Error;
			Diagnostic.Message = Message;
			Diagnostic.Range = Token.Range();
		}

		void SkipSpace()
		{
			while (!AtEnd() && FChar::IsWhitespace(Peek()))
			{
				Advance();
			}
		}

		bool AtEnd() const { return Index >= Source.Len(); }
		TCHAR Peek(int32 Offset = 0) const { return Index + Offset < Source.Len() ? Source[Index + Offset] : TEXT('\0'); }
		void Advance(int32 Count = 1)
		{
			for (int32 Step = 0; Step < Count && !AtEnd(); ++Step)
			{
				if (Source[Index] == TEXT('\n'))
				{
					++Line;
					Column = 1;
				}
				else
				{
					++Column;
				}
				++Index;
			}
		}

		const FString& Source;
		int32 Index = 0;
		int32 Line;
		int32 Column;
		TArray<FVekaDiagnostic>& Diagnostics;
	};

	// --- Parser ----------------------------------------------------------------------------------------------------

	struct FParseFailed
	{
	};

	struct FScope
	{
		TSet<FName> States;
		TSet<FName> Params;
		TSet<FName> Events;
		TArray<TSet<FName>> Locals;

		bool IsLocal(FName Name) const
		{
			for (const TSet<FName>& Set : Locals)
			{
				if (Set.Contains(Name))
				{
					return true;
				}
			}
			return false;
		}
	};

	EVekaValueType ParseTypeName(const FString& Name, bool& bOk)
	{
		bOk = true;
		static const TPair<const TCHAR*, EVekaValueType> Types[] = {
			{ TEXT("Bool"), EVekaValueType::Bool }, { TEXT("Int"), EVekaValueType::Int }, { TEXT("Float"), EVekaValueType::Float },
			{ TEXT("String"), EVekaValueType::String }, { TEXT("Text"), EVekaValueType::Text }, { TEXT("Color"), EVekaValueType::Color },
			{ TEXT("Vector2"), EVekaValueType::Vector2 }, { TEXT("Array"), EVekaValueType::Array }, { TEXT("Struct"), EVekaValueType::Struct },
			{ TEXT("Any"), EVekaValueType::None } };
		for (const TPair<const TCHAR*, EVekaValueType>& Type : Types)
		{
			if (Name == Type.Key)
			{
				return Type.Value;
			}
		}
		bOk = false;
		return EVekaValueType::None;
	}

	FString TextKey(const FString& Source)
	{
		FTCHARToUTF8 Utf8(*Source);
		FMD5 Md5;
		Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		uint8 Digest[16];
		Md5.Final(Digest);
		return BytesToHex(Digest, 16);
	}

	class FParser
	{
	public:
		FParser(TArray<FToken> InTokens, FVekaDocument& InDocument, FScope& InScope, TArray<FVekaDiagnostic>& InDiagnostics, const FVekaParseOptions& InOptions, const FString& InNamespace)
			: Tokens(MoveTemp(InTokens)), Document(InDocument), Scope(InScope), Diagnostics(InDiagnostics), Options(InOptions), Namespace(InNamespace)
		{
		}

		void ParseFile(FName& OutViewName)
		{
			Document.Comments.Append(Peek().Comments);
			ExpectWord(TEXT("view"));
			const FToken Name = Expect(ETok::Ident, LOCTEXT("ExpectViewName", "Expected the view name after 'view'"));
			OutViewName = FName(*Name.Text);
			if (Namespace.IsEmpty())
			{
				Namespace = TEXT("Veka.") + Name.Text;
			}
			ExpectPunct(TEXT("{"));
			while (Peek().Is(TEXT("@")))
			{
				Document.Comments.Append(Peek().Comments);
				try
				{
					ParseDeclaration();
				}
				catch (const FParseFailed&)
				{
					SkipToLineEnd();
				}
			}
			if (!Peek().Is(TEXT("}")))
			{
				try
				{
					Document.Root = ParseNode();
				}
				catch (const FParseFailed&)
				{
					SkipNodeRest();
				}
			}
			while (!Peek().Is(TEXT("}")) && Peek().Kind != ETok::End)
			{
				Error(Peek(), LOCTEXT("OneRoot", "A view has one root (put several views in a VStack)"));
				try
				{
					ParseNode();
				}
				catch (const FParseFailed&)
				{
					SkipBlockRest();
				}
			}
			ExpectPunct(TEXT("}"));
			if (Peek().Kind != ETok::End)
			{
				Error(Peek(), LOCTEXT("AfterView", "Nothing can follow the view's closing brace"));
			}
		}

		/** One expression (string interpolations). */
		int32 ParseStandaloneExpression()
		{
			const int32 Expr = ParseExpression();
			if (Peek().Kind != ETok::End)
			{
				Error(Peek(), LOCTEXT("InterpolationEnd", "Unexpected text in \\( )"));
			}
			return Expr;
		}

	private:
		// Tokens -------------------------------------------------------------------------------------------------

		const FToken& Peek(int32 Offset = 0) const { return Tokens[FMath::Min(Position + Offset, Tokens.Num() - 1)]; }
		const FToken& Next()
		{
			const FToken& Token = Peek();
			if (Position < Tokens.Num() - 1)
			{
				++Position;
			}
			return Token;
		}

		void Error(const FToken& Token, const FText& Message, EVekaDiagnosticSeverity Severity = EVekaDiagnosticSeverity::Error)
		{
			FVekaDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Severity = Severity;
			Diagnostic.Message = Message;
			Diagnostic.Range = Token.Range();
		}

		[[noreturn]] void Fail(const FToken& Token, const FText& Message)
		{
			Error(Token, Message);
			FailLine = Token.Line;
			throw FParseFailed();
		}

		const FToken& Expect(ETok Kind, const FText& Message)
		{
			if (Peek().Kind != Kind)
			{
				Fail(Peek(), Message);
			}
			return Next();
		}

		void ExpectPunct(const TCHAR* Punct)
		{
			if (!Peek().Is(Punct))
			{
				Fail(Peek(), FText::Format(LOCTEXT("ExpectPunct", "Expected '{0}'"), FText::FromString(Punct)));
			}
			Next();
		}

		void ExpectWord(const TCHAR* Word)
		{
			if (!Peek().IsWord(Word))
			{
				Fail(Peek(), FText::Format(LOCTEXT("ExpectWord", "Expected '{0}'"), FText::FromString(Word)));
			}
			Next();
		}

		bool Accept(const TCHAR* Punct)
		{
			if (Peek().Is(Punct))
			{
				Next();
				return true;
			}
			return false;
		}

		void SkipToLineEnd()
		{
			const int32 LineOfError = Peek().Line;
			while (Peek().Kind != ETok::End && Peek().Line == LineOfError && !Peek().Is(TEXT("}")))
			{
				Next();
			}
		}

		/** Error recovery: to the end of the current block (balanced braces). */
		void SkipBlockRest()
		{
			int32 Depth = 0;
			while (Peek().Kind != ETok::End)
			{
				if (Peek().Is(TEXT("{")))
				{
					++Depth;
				}
				else if (Peek().Is(TEXT("}")))
				{
					if (Depth == 0)
					{
						return;
					}
					--Depth;
				}
				Next();
			}
		}

		/**
		 * Error recovery inside a block of views: drops the failed view (its argument list, its own block, the modifier
		 * lines under it) and stops at the next view, so the following lines are still checked.
		 */
		void SkipNodeRest()
		{
			int32 Depth = 0;
			while (Peek().Kind != ETok::End)
			{
				const FToken& Token = Peek();
				if (Depth == 0 && Token.Is(TEXT("}")))
				{
					return;
				}
				if (Depth == 0 && Token.Line > FailLine && Token.Kind == ETok::Ident && !Token.IsWord(TEXT("else")))
				{
					return;
				}
				if (Token.Is(TEXT("(")) || Token.Is(TEXT("[")) || Token.Is(TEXT("{")))
				{
					++Depth;
				}
				else if (Token.Is(TEXT(")")) || Token.Is(TEXT("]")) || Token.Is(TEXT("}")))
				{
					Depth = FMath::Max(0, Depth - 1);
				}
				Next();
			}
		}

		// Declarations -------------------------------------------------------------------------------------------

		void ParseDeclaration()
		{
			ExpectPunct(TEXT("@"));
			const FToken Kind = Expect(ETok::Ident, LOCTEXT("ExpectDecl", "Expected State, Param, Slot or Event after '@'"));
			if (Kind.Text == TEXT("State") || Kind.Text == TEXT("Param"))
			{
				ExpectWord(TEXT("var"));
				const FToken Name = Expect(ETok::Ident, LOCTEXT("ExpectVarName", "Expected a name after 'var'"));
				EVekaValueType Type = EVekaValueType::None;
				if (Accept(TEXT(":")))
				{
					const FToken TypeToken = Expect(ETok::Ident, LOCTEXT("ExpectType", "Expected a type (Bool, Int, Float, String, Text, Color, Vector2, Array, Struct, Any)"));
					bool bOk = false;
					Type = ParseTypeName(TypeToken.Text, bOk);
					if (!bOk)
					{
						Error(TypeToken, FText::Format(LOCTEXT("UnknownType", "Unknown type '{0}'"), FText::FromString(TypeToken.Text)));
					}
				}
				FVekaValue Initial;
				if (Accept(TEXT("=")))
				{
					Initial = ParseConstant();
				}
				const FName VarName(*Name.Text);
				if (Kind.Text == TEXT("State"))
				{
					if (Scope.States.Contains(VarName))
					{
						Error(Name, FText::Format(LOCTEXT("DuplicateState", "State '{0}' declared twice"), FText::FromName(VarName)));
					}
					Scope.States.Add(VarName);
					FVekaStateDeclaration& State = Document.States.AddDefaulted_GetRef();
					State.Name = VarName;
					State.Initial = Initial;
				}
				else
				{
					Scope.Params.Add(VarName);
					FVekaParamDeclaration& Param = Document.Params.AddDefaulted_GetRef();
					Param.Name = VarName;
					Param.Type = Type;
					Param.Default = Initial;
				}
			}
			else if (Kind.Text == TEXT("Slot"))
			{
				const FToken Name = Expect(ETok::Ident, LOCTEXT("ExpectSlotName", "Expected a slot name ('default' for the default slot)"));
				Document.Slots.AddDefaulted_GetRef().Name = Name.Text == TEXT("default") ? NAME_None : FName(*Name.Text);
			}
			else if (Kind.Text == TEXT("Event"))
			{
				const FToken Name = Expect(ETok::Ident, LOCTEXT("ExpectEventName", "Expected an event name"));
				FVekaEventDeclaration& Event = Document.EventDeclarations.AddDefaulted_GetRef();
				Event.Name = FName(*Name.Text);
				if (Accept(TEXT("(")))
				{
					const FToken TypeToken = Expect(ETok::Ident, LOCTEXT("ExpectPayloadType", "Expected the payload type"));
					bool bOk = false;
					Event.PayloadType = ParseTypeName(TypeToken.Text, bOk);
					if (!bOk)
					{
						Error(TypeToken, FText::Format(LOCTEXT("UnknownType", "Unknown type '{0}'"), FText::FromString(TypeToken.Text)));
					}
					ExpectPunct(TEXT(")"));
				}
				Scope.Events.Add(Event.Name);
			}
			else
			{
				Fail(Kind, FText::Format(LOCTEXT("UnknownDecl", "Unknown declaration '@{0}' (State, Param, Slot or Event)"), FText::FromString(Kind.Text)));
			}
		}

		FVekaValue ParseConstant()
		{
			const FToken& Start = Peek();
			const int32 Mark = Document.Expressions.Num();
			const int32 Expr = ParseExpression();
			FVekaValue Value;
			if (Document.Expressions.IsValidIndex(Expr) && Document.Expressions[Expr].Kind == EVekaExprKind::Literal)
			{
				Value = Document.Expressions[Expr].Value;
			}
			else
			{
				Error(Start, LOCTEXT("NotConstant", "An initial value must be a constant"));
			}
			Document.Expressions.SetNum(Mark);
			return Value;
		}

		// Nodes ----------------------------------------------------------------------------------------------------

		int32 NewNode(EVekaAstNodeKind Kind, const FToken& At)
		{
			FVekaAstNode& Node = Document.Nodes.AddDefaulted_GetRef();
			Node.Kind = Kind;
			Node.Source = At.Range();
			Node.Comments = At.Comments;
			return Document.Nodes.Num() - 1;
		}

		int32 ParseNode()
		{
			const FToken& Head = Peek();
			if (Head.Kind != ETok::Ident)
			{
				Fail(Head, LOCTEXT("ExpectNode", "Expected a view (a component name, if, ForEach or Slot)"));
			}
			int32 Index = INDEX_NONE;
			if (Head.Text == TEXT("if"))
			{
				Index = ParseIf();
			}
			else if (Head.Text == TEXT("ForEach"))
			{
				Index = ParseForEach();
			}
			else if (Head.Text == TEXT("Slot"))
			{
				Next();
				Index = NewNode(EVekaAstNodeKind::Slot, Head);
				ExpectPunct(TEXT("("));
				const FToken Name = Expect(ETok::Ident, LOCTEXT("ExpectSlotName", "Expected a slot name ('default' for the default slot)"));
				ExpectPunct(TEXT(")"));
				Document.Nodes[Index].Type = Name.Text == TEXT("default") ? NAME_None : FName(*Name.Text);
			}
			else
			{
				Index = ParseElement();
			}
			ParseModifiers(Index);
			return Index;
		}

		int32 ParseIf()
		{
			const FToken& Head = Next();
			const int32 Index = NewNode(EVekaAstNodeKind::If, Head);
			const int32 Condition = ParseExpression();
			Document.Nodes[Index].Expr = Condition;
			const TArray<int32> Then = ParseNodeBlock();
			Document.Nodes[Index].Children = Then;
			if (Peek().IsWord(TEXT("else")))
			{
				Next();
				if (Peek().IsWord(TEXT("if")))
				{
					const int32 ElseIf = ParseIf();
					Document.Nodes[Index].ElseChildren = { ElseIf };
				}
				else
				{
					const TArray<int32> Else = ParseNodeBlock();
					Document.Nodes[Index].ElseChildren = Else;
				}
			}
			return Index;
		}

		int32 ParseForEach()
		{
			const FToken& Head = Next();
			const int32 Index = NewNode(EVekaAstNodeKind::ForEach, Head);
			ExpectPunct(TEXT("("));
			const int32 Source = ParseExpression();
			ExpectPunct(TEXT(")"));
			Document.Nodes[Index].Expr = Source;
			ExpectPunct(TEXT("{"));
			TSet<FName> Locals;
			if (Peek().Kind == ETok::Ident && (Peek(1).IsWord(TEXT("in")) || (Peek(1).Is(TEXT(",")) && Peek(2).Kind == ETok::Ident && Peek(3).IsWord(TEXT("in")))))
			{
				Document.Nodes[Index].LoopVariable = FName(*Next().Text);
				Locals.Add(Document.Nodes[Index].LoopVariable);
				if (Accept(TEXT(",")))
				{
					Document.Nodes[Index].IndexVariable = FName(*Next().Text);
					Locals.Add(Document.Nodes[Index].IndexVariable);
				}
				ExpectWord(TEXT("in"));
			}
			Scope.Locals.Add(Locals);
			const TArray<int32> Body = ParseNodesUntilBrace();
			Scope.Locals.Pop();
			Document.Nodes[Index].Children = Body;
			return Index;
		}

		TArray<int32> ParseNodeBlock()
		{
			ExpectPunct(TEXT("{"));
			return ParseNodesUntilBrace();
		}

		/** Nodes up to and including the closing brace. */
		TArray<int32> ParseNodesUntilBrace()
		{
			TArray<int32> Children;
			while (!Peek().Is(TEXT("}")) && Peek().Kind != ETok::End)
			{
				try
				{
					Children.Add(ParseNode());
				}
				catch (const FParseFailed&)
				{
					SkipNodeRest();
				}
			}
			if (!Peek().Comments.IsEmpty())
			{
				Error(Peek(), LOCTEXT("LooseComment", "Comment not attached to a view (it is lost when printed)"), EVekaDiagnosticSeverity::Warning);
			}
			ExpectPunct(TEXT("}"));
			return Children;
		}

		bool IsActionStart() const
		{
			const FToken& Token = Peek();
			if (Token.IsWord(TEXT("emit")) || Token.IsWord(TEXT("withAnimation")))
			{
				return true;
			}
			return Token.Kind == ETok::Ident && (Peek(1).Is(TEXT("=")) || Peek(1).Is(TEXT("+=")) || Peek(1).Is(TEXT("-="))
				|| (Peek(1).Is(TEXT(".")) && Peek(2).IsWord(TEXT("toggle"))));
		}

		int32 ParseElement()
		{
			const FToken& Head = Next();
			const int32 Index = NewNode(EVekaAstNodeKind::Element, Head);
			const FName Type(*Head.Text);
			Document.Nodes[Index].Type = Type;
			const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(Type);
			if (!Entry && Options.ResolveComponent)
			{
				Document.Nodes[Index].Component = Options.ResolveComponent(Type);
			}
			FName Primary = Entry ? Entry->PrimaryArgument : NAME_None;
			if (!Entry && Document.Nodes[Index].Component && Document.Nodes[Index].Component->Document.Params.Num() == 1)
			{
				Primary = Document.Nodes[Index].Component->Document.Params[0].Name;
			}
			if (Peek().Is(TEXT("(")))
			{
				TArray<FVekaAstArgument> Arguments = ParseArguments(Primary, false);
				Document.Nodes[Index].Arguments = MoveTemp(Arguments);
			}
			if (Peek().Is(TEXT("{")))
			{
				Next();
				if (IsActionStart())
				{
					TArray<FVekaAstAction> Actions = ParseActionsUntilBrace(false);
					Document.Nodes[Index].Actions = MoveTemp(Actions);
				}
				else
				{
					const TArray<int32> Children = ParseNodesUntilBrace();
					Document.Nodes[Index].Children = Children;
					if (Peek().IsWord(TEXT("action")))
					{
						Next();
						ExpectPunct(TEXT("{"));
						TArray<FVekaAstAction> Actions = ParseActionsUntilBrace(false);
						Document.Nodes[Index].Actions = MoveTemp(Actions);
					}
				}
			}
			return Index;
		}

		TArray<FVekaAstArgument> ParseArguments(FName Primary, bool bEventName)
		{
			TArray<FVekaAstArgument> Arguments;
			ExpectPunct(TEXT("("));
			bool bFirst = true;
			while (!Peek().Is(TEXT(")")))
			{
				FVekaAstArgument Argument;
				const FToken& At = Peek();
				if (Peek().Kind == ETok::Ident && Peek(1).Is(TEXT(":")))
				{
					Argument.Name = FName(*Next().Text);
					Next();
				}
				else if (bFirst && !Primary.IsNone())
				{
					Argument.Name = Primary;
				}
				else
				{
					// Not fatal: the rest of the call is still read (and checked).
					Error(At, LOCTEXT("UnnamedArgument", "Arguments need a name here (name: value)"));
				}
				if (bEventName && bFirst && Peek().Kind == ETok::Ident && (Peek(1).Is(TEXT(")")) || Peek(1).Is(TEXT(","))))
				{
					// `.on(buy)`: the event name, not a variable.
					const FToken& Event = Next();
					FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
					Expr.Kind = EVekaExprKind::Literal;
					Expr.Value = FVekaValue::String(Event.Text);
					Expr.Source = Event.Range();
					Argument.Expr = Document.Expressions.Num() - 1;
				}
				else
				{
					Argument.Expr = ParseExpression();
				}
				if (Arguments.ContainsByPredicate([&Argument](const FVekaAstArgument& Existing) { return Existing.Name == Argument.Name; }))
				{
					Error(At, FText::Format(LOCTEXT("DuplicateArgument", "Argument '{0}' given twice"), FText::FromName(Argument.Name)));
				}
				Arguments.Add(Argument);
				bFirst = false;
				if (!Accept(TEXT(",")))
				{
					break;
				}
			}
			ExpectPunct(TEXT(")"));
			return Arguments;
		}

		void ParseModifiers(int32 Index)
		{
			while (Peek().Is(TEXT(".")) && Peek(1).Kind == ETok::Ident)
			{
				Next();
				const FToken& Name = Next();
				if (Name.Text == TEXT("inSlot"))
				{
					ExpectPunct(TEXT("("));
					const FToken Slot = Expect(ETok::Ident, LOCTEXT("ExpectSlotName", "Expected a slot name ('default' for the default slot)"));
					ExpectPunct(TEXT(")"));
					Document.Nodes[Index].SlotName = Slot.Text == TEXT("default") ? NAME_None : FName(*Slot.Text);
					continue;
				}
				FVekaAstModifier Modifier;
				Modifier.Name = FName(*Name.Text);
				const FVekaCatalogEntry* Entry = Veka::Catalog::FindModifier(Modifier.Name);
				const bool bOn = Modifier.Name == TEXT("on");
				if (Peek().Is(TEXT("(")))
				{
					Modifier.Arguments = ParseArguments(Entry ? Entry->PrimaryArgument : NAME_None, bOn);
				}
				if (Peek().Is(TEXT("{")))
				{
					Next();
					if (bOn)
					{
						Scope.Locals.Add({ FName(TEXT("payload")) });
					}
					Modifier.Actions = ParseActionsUntilBrace(false);
					if (bOn)
					{
						Scope.Locals.Pop();
					}
				}
				Document.Nodes[Index].Modifiers.Add(MoveTemp(Modifier));
			}
		}

		// Actions --------------------------------------------------------------------------------------------------

		TArray<FVekaAstAction> ParseActionsUntilBrace(bool bAnimated)
		{
			TArray<FVekaAstAction> Actions;
			while (!Peek().Is(TEXT("}")) && Peek().Kind != ETok::End)
			{
				try
				{
					ParseAction(Actions, bAnimated);
				}
				catch (const FParseFailed&)
				{
					SkipToLineEnd();
				}
				Accept(TEXT(";"));
			}
			ExpectPunct(TEXT("}"));
			return Actions;
		}

		FName StateTarget(const FToken& Token)
		{
			const FName Name(*Token.Text);
			if (!Scope.States.Contains(Name))
			{
				Error(Token, FText::Format(LOCTEXT("UnknownTarget", "'{0}' is not a state (declare it with @State var)"), FText::FromName(Name)));
			}
			return Name;
		}

		void ParseAction(TArray<FVekaAstAction>& Actions, bool bAnimated)
		{
			const FToken& Head = Peek();
			if (Head.IsWord(TEXT("withAnimation")))
			{
				Next();
				ExpectPunct(TEXT("{"));
				Actions.Append(ParseActionsUntilBrace(true));
				return;
			}
			FVekaAstAction Action;
			Action.bAnimated = bAnimated;
			if (Head.IsWord(TEXT("emit")))
			{
				Next();
				const FToken Event = Expect(ETok::Ident, LOCTEXT("ExpectEmit", "Expected the event name after 'emit'"));
				Action.Kind = EVekaActionKind::Emit;
				Action.Target = FName(*Event.Text);
				if (!Scope.Events.Contains(Action.Target))
				{
					Error(Event, FText::Format(LOCTEXT("UnknownEvent", "Event '{0}' is not declared (@Event {0})"), FText::FromName(Action.Target)));
				}
				if (Accept(TEXT("(")))
				{
					Action.Expr = ParseExpression();
					ExpectPunct(TEXT(")"));
				}
				Actions.Add(Action);
				return;
			}
			const FToken Target = Expect(ETok::Ident, LOCTEXT("ExpectAction", "Expected an action (state = value, state += value, state.toggle(), emit, withAnimation)"));
			Action.Target = StateTarget(Target);
			if (Accept(TEXT("=")))
			{
				Action.Kind = EVekaActionKind::Set;
				Action.Expr = ParseExpression();
			}
			else if (Accept(TEXT("+=")))
			{
				Action.Kind = EVekaActionKind::Increment;
				Action.Expr = ParseExpression();
			}
			else if (Accept(TEXT("-=")))
			{
				Action.Kind = EVekaActionKind::Increment;
				const int32 Amount = ParseExpression();
				Action.Expr = AddExpr(EVekaExprKind::Unary, Target, EVekaExprOp::Negate, { Amount });
			}
			else if (Accept(TEXT(".")))
			{
				ExpectWord(TEXT("toggle"));
				ExpectPunct(TEXT("("));
				ExpectPunct(TEXT(")"));
				Action.Kind = EVekaActionKind::Toggle;
			}
			else
			{
				Fail(Peek(), LOCTEXT("ExpectAssign", "Expected '=', '+=', '-=' or '.toggle()'"));
			}
			Actions.Add(Action);
		}

		// Expressions ----------------------------------------------------------------------------------------------

		int32 AddExpr(EVekaExprKind Kind, const FToken& At, EVekaExprOp Op = EVekaExprOp::None, TArray<int32> Operands = {})
		{
			FVekaAstExpr& Expr = Document.Expressions.AddDefaulted_GetRef();
			Expr.Kind = Kind;
			Expr.Op = Op;
			Expr.Operands = MoveTemp(Operands);
			Expr.Source = At.Range();
			return Document.Expressions.Num() - 1;
		}

		int32 AddLiteral(const FVekaValue& Value, const FToken& At)
		{
			const int32 Index = AddExpr(EVekaExprKind::Literal, At);
			Document.Expressions[Index].Value = Value;
			return Index;
		}

		bool IsLiteral(int32 Index) const
		{
			return Document.Expressions.IsValidIndex(Index) && Document.Expressions[Index].Kind == EVekaExprKind::Literal;
		}

		int32 ParseExpression()
		{
			const int32 Condition = ParseBinary(1);
			if (Peek().Is(TEXT("?")))
			{
				const FToken& At = Next();
				const int32 Then = ParseExpression();
				ExpectPunct(TEXT(":"));
				const int32 Otherwise = ParseExpression();
				return AddExpr(EVekaExprKind::Ternary, At, EVekaExprOp::None, { Condition, Then, Otherwise });
			}
			return Condition;
		}

		static int32 Precedence(const FToken& Token, EVekaExprOp& OutOp)
		{
			if (Token.Kind != ETok::Punct)
			{
				return 0;
			}
			struct FOp { const TCHAR* Text; int32 Level; EVekaExprOp Op; };
			static const FOp Ops[] = {
				{ TEXT("||"), 1, EVekaExprOp::Or }, { TEXT("&&"), 2, EVekaExprOp::And },
				{ TEXT("=="), 3, EVekaExprOp::Equal }, { TEXT("!="), 3, EVekaExprOp::NotEqual },
				{ TEXT("<"), 4, EVekaExprOp::Less }, { TEXT("<="), 4, EVekaExprOp::LessEqual },
				{ TEXT(">"), 4, EVekaExprOp::Greater }, { TEXT(">="), 4, EVekaExprOp::GreaterEqual },
				{ TEXT("+"), 5, EVekaExprOp::Add }, { TEXT("-"), 5, EVekaExprOp::Subtract },
				{ TEXT("*"), 6, EVekaExprOp::Multiply }, { TEXT("/"), 6, EVekaExprOp::Divide }, { TEXT("%"), 6, EVekaExprOp::Modulo } };
			for (const FOp& Op : Ops)
			{
				if (Token.Text == Op.Text)
				{
					OutOp = Op.Op;
					return Op.Level;
				}
			}
			return 0;
		}

		int32 ParseBinary(int32 MinPrecedence)
		{
			int32 Left = ParseUnary();
			while (true)
			{
				EVekaExprOp Op = EVekaExprOp::None;
				const int32 Level = Precedence(Peek(), Op);
				if (Level == 0 || Level < MinPrecedence)
				{
					return Left;
				}
				const FToken& At = Next();
				const int32 Right = ParseBinary(Level + 1);
				Left = AddExpr(EVekaExprKind::Binary, At, Op, { Left, Right });
			}
		}

		int32 ParseUnary()
		{
			if (Peek().Is(TEXT("!")))
			{
				const FToken& At = Next();
				const int32 Operand = ParseUnary();
				return AddExpr(EVekaExprKind::Unary, At, EVekaExprOp::Not, { Operand });
			}
			if (Peek().Is(TEXT("-")))
			{
				const FToken& At = Next();
				if (Peek().Kind == ETok::Int || Peek().Kind == ETok::Float)
				{
					// -3 is a literal (printed back as -3).
					const FToken& Number = Next();
					const FVekaValue Value = Number.Kind == ETok::Int ? FVekaValue::Int(-FCString::Atoi64(*Number.Text)) : FVekaValue::Float(-FCString::Atod(*Number.Text));
					return ParsePostfix(AddLiteral(Value, At));
				}
				const int32 Operand = ParseUnary();
				return AddExpr(EVekaExprKind::Unary, At, EVekaExprOp::Negate, { Operand });
			}
			return ParsePostfix(ParsePrimary());
		}

		int32 ParsePostfix(int32 Expr)
		{
			while (true)
			{
				if (Peek().Is(TEXT(".")) && Peek(1).Kind == ETok::Ident)
				{
					const FToken& At = Next();
					const FToken& Field = Next();
					const int32 Member = AddExpr(EVekaExprKind::Member, At, EVekaExprOp::None, { Expr });
					Document.Expressions[Member].Name = FName(*Field.Text);
					Expr = Member;
				}
				else if (Peek().Is(TEXT("[")))
				{
					const FToken& At = Next();
					const int32 Index = ParseExpression();
					ExpectPunct(TEXT("]"));
					Expr = AddExpr(EVekaExprKind::Binary, At, EVekaExprOp::Index, { Expr, Index });
				}
				else
				{
					return Expr;
				}
			}
		}

		TArray<int32> ParseCallArguments()
		{
			TArray<int32> Arguments;
			ExpectPunct(TEXT("("));
			while (!Peek().Is(TEXT(")")))
			{
				Arguments.Add(ParseExpression());
				if (!Accept(TEXT(",")))
				{
					break;
				}
			}
			ExpectPunct(TEXT(")"));
			return Arguments;
		}

		double ConstantNumber(int32 Index, const FToken& At)
		{
			if (!IsLiteral(Index) || !Document.Expressions[Index].Value.IsNumber())
			{
				Error(At, LOCTEXT("ConstantNumber", "Expected a number"));
				return 0.0;
			}
			return Document.Expressions[Index].Value.AsFloat();
		}

		FText MakeText(const FString& SourceString) const
		{
			return FText::AsLocalizable_Advanced(FTextKey(Namespace), FTextKey(TextKey(SourceString)), SourceString);
		}

		int32 ParseString(const FToken& Token)
		{
			const bool bText = Token.Kind == ETok::Text;
			if (Token.Interpolations.IsEmpty())
			{
				return AddLiteral(bText ? FVekaValue::Text(MakeText(Token.Text)) : FVekaValue::String(Token.Text), Token);
			}
			FString Pattern;
			TArray<int32> Operands;
			for (int32 Part = 0; Part < Token.Parts.Num(); ++Part)
			{
				Pattern += Token.Parts[Part];
				if (Token.Interpolations.IsValidIndex(Part))
				{
					Pattern += FString::Printf(TEXT("{%d}"), Part);
					TArray<FToken> SubTokens = FLexer(Token.Interpolations[Part], Token.InterpolationPositions[Part].X, Token.InterpolationPositions[Part].Y, Diagnostics).Run();
					FParser Sub(MoveTemp(SubTokens), Document, Scope, Diagnostics, Options, Namespace);
					try
					{
						Operands.Add(Sub.ParseStandaloneExpression());
					}
					catch (const FParseFailed&)
					{
						Operands.Add(AddLiteral(FVekaValue(), Token));
					}
				}
			}
			const int32 Format = AddExpr(EVekaExprKind::Format, Token, EVekaExprOp::None, MoveTemp(Operands));
			Document.Expressions[Format].Value = bText ? FVekaValue::Text(MakeText(Pattern)) : FVekaValue::String(Pattern);
			return Format;
		}

		int32 ParsePrimary()
		{
			const FToken& Token = Peek();
			switch (Token.Kind)
			{
			case ETok::Int: Next(); return AddLiteral(FVekaValue::Int(FCString::Atoi64(*Token.Text)), Token);
			case ETok::Float: Next(); return AddLiteral(FVekaValue::Float(FCString::Atod(*Token.Text)), Token);
			case ETok::Text:
			case ETok::RawString: Next(); return ParseString(Token);
			case ETok::Hex: Next(); return AddLiteral(FVekaValue::String(Token.Text), Token);
			default: break;
			}
			if (Token.Is(TEXT("(")))
			{
				Next();
				const int32 Inner = ParseExpression();
				ExpectPunct(TEXT(")"));
				return Inner;
			}
			if (Token.Is(TEXT(".")) && Peek(1).Kind == ETok::Ident)
			{
				// `.headline`: enum value or token name.
				Next();
				const FToken& Name = Next();
				return AddLiteral(FVekaValue::String(Name.Text), Token);
			}
			if (Token.Is(TEXT("[")))
			{
				Next();
				TArray<FVekaValue> Items;
				while (!Peek().Is(TEXT("]")))
				{
					const FToken& At = Peek();
					const int32 Item = ParseExpression();
					if (IsLiteral(Item))
					{
						Items.Add(Document.Expressions[Item].Value);
					}
					else
					{
						Error(At, LOCTEXT("ConstantArray", "Array literals hold constants"));
					}
					if (!Accept(TEXT(",")))
					{
						break;
					}
				}
				ExpectPunct(TEXT("]"));
				return AddLiteral(FVekaValue::Array(MoveTemp(Items)), Token);
			}
			if (Token.Is(TEXT("{")))
			{
				Next();
				TMap<FName, FVekaValue> Fields;
				while (!Peek().Is(TEXT("}")))
				{
					const FToken Field = Expect(ETok::Ident, LOCTEXT("ExpectField", "Expected a field name"));
					ExpectPunct(TEXT(":"));
					const FToken& At = Peek();
					const int32 Value = ParseExpression();
					if (IsLiteral(Value))
					{
						Fields.Add(FName(*Field.Text), Document.Expressions[Value].Value);
					}
					else
					{
						Error(At, LOCTEXT("ConstantStruct", "Struct literals hold constants"));
					}
					if (!Accept(TEXT(",")))
					{
						break;
					}
				}
				ExpectPunct(TEXT("}"));
				return AddLiteral(FVekaValue::Struct(MoveTemp(Fields)), Token);
			}
			if (Token.Kind != ETok::Ident)
			{
				Fail(Token, LOCTEXT("ExpectExpression", "Expected a value"));
			}
			Next();
			const FString& Word = Token.Text;
			if (Word == TEXT("true") || Word == TEXT("false"))
			{
				return AddLiteral(FVekaValue::Bool(Word == TEXT("true")), Token);
			}
			if (Word == TEXT("nil"))
			{
				return AddLiteral(FVekaValue(), Token);
			}
			if (Word == TEXT("model"))
			{
				ExpectPunct(TEXT("."));
				const FToken Property = Expect(ETok::Ident, LOCTEXT("ExpectModelProperty", "Expected a model property (model.Name)"));
				const int32 Expr = AddExpr(EVekaExprKind::Model, Token);
				Document.Expressions[Expr].Name = FName(*Property.Text);
				return Expr;
			}
			if (Word == TEXT("rgba") || Word == TEXT("vec2"))
			{
				const TArray<int32> Arguments = ParseCallArguments();
				const int32 Wanted = Word == TEXT("rgba") ? 4 : 2;
				if (Arguments.Num() != Wanted)
				{
					Error(Token, FText::Format(LOCTEXT("CallArity", "'{0}' takes {1} numbers"), FText::FromString(Word), FText::AsNumber(Wanted)));
					return AddLiteral(FVekaValue(), Token);
				}
				TArray<double> N;
				for (const int32 Argument : Arguments)
				{
					N.Add(ConstantNumber(Argument, Token));
				}
				const FVekaValue Value = Wanted == 4
					? FVekaValue::Color(FLinearColor(static_cast<float>(N[0]), static_cast<float>(N[1]), static_cast<float>(N[2]), static_cast<float>(N[3])))
					: FVekaValue::Vector2(FVector2D(N[0], N[1]));
				return AddLiteral(Value, Token);
			}
			if (Word == TEXT("min") || Word == TEXT("max"))
			{
				const TArray<int32> Arguments = ParseCallArguments();
				if (Arguments.Num() != 2)
				{
					Fail(Token, FText::Format(LOCTEXT("MinMaxArity", "'{0}' takes two values"), FText::FromString(Word)));
				}
				return AddExpr(EVekaExprKind::Binary, Token, Word == TEXT("min") ? EVekaExprOp::Min : EVekaExprOp::Max, Arguments);
			}
			if (Word == TEXT("format"))
			{
				ExpectPunct(TEXT("("));
				const FToken& Pattern = Peek();
				if ((Pattern.Kind != ETok::Text && Pattern.Kind != ETok::RawString) || !Pattern.Interpolations.IsEmpty())
				{
					Fail(Pattern, LOCTEXT("FormatPattern", "format() starts with a string pattern with {0} {1}..."));
				}
				Next();
				TArray<int32> Operands;
				while (Accept(TEXT(",")))
				{
					Operands.Add(ParseExpression());
				}
				ExpectPunct(TEXT(")"));
				const int32 Expr = AddExpr(EVekaExprKind::Format, Token, EVekaExprOp::None, MoveTemp(Operands));
				Document.Expressions[Expr].Value = Pattern.Kind == ETok::Text ? FVekaValue::Text(MakeText(Pattern.Text)) : FVekaValue::String(Pattern.Text);
				return Expr;
			}

			const FName Name(*Word);
			EVekaExprKind Kind;
			if (Scope.IsLocal(Name))
			{
				Kind = EVekaExprKind::Local;
			}
			else if (Scope.Params.Contains(Name))
			{
				Kind = EVekaExprKind::Param;
			}
			else if (Scope.States.Contains(Name))
			{
				Kind = EVekaExprKind::State;
			}
			else
			{
				Fail(Token, FText::Format(LOCTEXT("UnknownName", "Unknown name '{0}' (not a state, param or loop variable)"), FText::FromName(Name)));
			}
			const int32 Expr = AddExpr(Kind, Token);
			Document.Expressions[Expr].Name = Name;
			return Expr;
		}

		TArray<FToken> Tokens;
		int32 Position = 0;
		/** Line of the last fatal error (recovery resumes on a later line). */
		int32 FailLine = 0;
		FVekaDocument& Document;
		FScope& Scope;
		TArray<FVekaDiagnostic>& Diagnostics;
		const FVekaParseOptions& Options;
		FString Namespace;
	};
}

namespace Veka::Lang
{
	FVekaParseResult Parse(const FString& Source, const FVekaParseOptions& Options)
	{
		FVekaParseResult Result;
		Result.Document.FormatVersion = FVekaDocument::CurrentFormatVersion;
		TArray<FToken> Tokens = FLexer(Source, 1, 1, Result.Diagnostics).Run();
		FScope Scope;
		FParser Parser(MoveTemp(Tokens), Result.Document, Scope, Result.Diagnostics, Options, Options.TextNamespace);
		try
		{
			Parser.ParseFile(Result.ViewName);
		}
		catch (const FParseFailed&)
		{
		}
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
