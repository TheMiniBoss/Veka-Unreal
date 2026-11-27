// Copyright TheMiniBoss. All Rights Reserved.

#include "CodeEditor/VekaCodeAssist.h"
#include "View/VekaCatalog.h"
#include "View/VekaDocument.h"
#include "Colora/VekaTheme.h"
#include "Symbols/VekaSymbol.h"
#include "Colora/VekaPaintAsset.h"
#include "View/VekaComponentLibrary.h"
#include "View/VekaViewAsset.h"
#include "VekaLanguage.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
	bool IsIdentStart(TCHAR C) { return FChar::IsAlpha(C) || C == TEXT('_'); }
	bool IsIdentChar(TCHAR C) { return FChar::IsAlnum(C) || C == TEXT('_'); }

	const TSet<FString>& Keywords()
	{
		static const TSet<FString> Set = { TEXT("view"), TEXT("var"), TEXT("if"), TEXT("else"), TEXT("in"), TEXT("true"), TEXT("false"), TEXT("nil"), TEXT("emit"),
			TEXT("withAnimation"), TEXT("action"), TEXT("model"), TEXT("ForEach"), TEXT("Slot"), TEXT("default"), TEXT("format"), TEXT("rgba"), TEXT("vec2"), TEXT("min"),
			TEXT("max"), TEXT("toggle"), TEXT("track") };
		return Set;
	}

	/** Previous non-space character before Index on the line, or 0. */
	TCHAR PreviousNonSpace(const FString& Line, int32 Index)
	{
		for (int32 At = Index - 1; At >= 0; --At)
		{
			if (!FChar::IsWhitespace(Line[At]))
			{
				return Line[At];
			}
		}
		return 0;
	}

	TArray<FString> SplitLines(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), false);
		for (FString& Line : Lines)
		{
			Line.RemoveFromEnd(TEXT("\r"));
		}
		if (Lines.IsEmpty())
		{
			Lines.Add(FString());
		}
		return Lines;
	}

	/** The call whose parentheses contain a position: `Text(` / `.padding(`, with the argument being written. */
	struct FCallContext
	{
		bool bFound = false;
		FName Name;
		bool bModifier = false;
		/** Name of the argument whose value is being written (none: an unnamed argument). */
		FName Argument;
		/** Writing the first, unnamed argument (the primary one). */
		bool bFirstUnnamed = false;
		/** The current argument has no `name:` yet (an argument name may be typed). */
		bool bNoNameYet = false;
		TSet<FName> GivenArguments;
	};

	FCallContext FindCall(const FString& Text, int32 Position)
	{
		FCallContext Out;
		int32 Depth = 0;
		int32 Open = INDEX_NONE;
		for (int32 At = Position - 1; At >= 0; --At)
		{
			const TCHAR C = Text[At];
			if (C == TEXT(')') || C == TEXT(']'))
			{
				++Depth;
			}
			else if (C == TEXT('(') || C == TEXT('['))
			{
				if (Depth == 0)
				{
					if (C == TEXT('('))
					{
						Open = At;
					}
					break;
				}
				--Depth;
			}
			else if ((C == TEXT('{') || C == TEXT('}')) && Depth == 0)
			{
				break;
			}
		}
		if (Open == INDEX_NONE)
		{
			return Out;
		}
		int32 NameEnd = Open;
		while (NameEnd > 0 && FChar::IsWhitespace(Text[NameEnd - 1]) && Text[NameEnd - 1] != TEXT('\n'))
		{
			--NameEnd;
		}
		int32 NameStart = NameEnd;
		while (NameStart > 0 && IsIdentChar(Text[NameStart - 1]))
		{
			--NameStart;
		}
		if (NameStart == NameEnd)
		{
			return Out;
		}
		Out.bFound = true;
		Out.Name = FName(*Text.Mid(NameStart, NameEnd - NameStart));
		Out.bModifier = NameStart > 0 && Text[NameStart - 1] == TEXT('.');

		// Arguments written so far (top level of the call).
		TArray<FString> Segments;
		FString Current;
		int32 Inner = 0;
		bool bInString = false;
		for (int32 At = Open + 1; At < Position; ++At)
		{
			const TCHAR C = Text[At];
			if (C == TEXT('"') && (At == 0 || Text[At - 1] != TEXT('\\')))
			{
				bInString = !bInString;
			}
			if (!bInString)
			{
				if (C == TEXT('(') || C == TEXT('[') || C == TEXT('{'))
				{
					++Inner;
				}
				else if (C == TEXT(')') || C == TEXT(']') || C == TEXT('}'))
				{
					--Inner;
				}
				else if (C == TEXT(',') && Inner == 0)
				{
					Segments.Add(Current);
					Current.Reset();
					continue;
				}
			}
			Current.AppendChar(C);
		}
		auto NameOf = [](const FString& Segment) -> FName
		{
			FString Trimmed = Segment.TrimStart();
			int32 End = 0;
			while (End < Trimmed.Len() && IsIdentChar(Trimmed[End]))
			{
				++End;
			}
			const FString After = Trimmed.Mid(End).TrimStart();
			return End > 0 && After.StartsWith(TEXT(":")) ? FName(*Trimmed.Left(End)) : NAME_None;
		};
		for (const FString& Segment : Segments)
		{
			const FName Given = NameOf(Segment);
			Out.GivenArguments.Add(Given.IsNone() && Segments.Num() > 0 && &Segment == &Segments[0] ? FName(TEXT("__primary")) : Given);
		}
		Out.Argument = NameOf(Current);
		Out.bNoNameYet = Out.Argument.IsNone() && !Current.Contains(TEXT(":"));
		Out.bFirstUnnamed = Out.Argument.IsNone() && Segments.IsEmpty();
		return Out;
	}

	const FVekaCatalogEntry* FindEntry(const FCallContext& Call)
	{
		return Call.bModifier ? Veka::Catalog::FindModifier(Call.Name) : Veka::Catalog::FindComponent(Call.Name);
	}

	const FVekaCatalogArgument* FindArgument(const FCallContext& Call)
	{
		const FVekaCatalogEntry* Entry = FindEntry(Call);
		if (!Entry)
		{
			return nullptr;
		}
		const FName Name = !Call.Argument.IsNone() ? Call.Argument : (Call.bFirstUnnamed ? Entry->PrimaryArgument : NAME_None);
		return Name.IsNone() ? nullptr : Entry->FindArgument(Name);
	}

	void AddItem(TArray<FVekaCompletionItem>& Items, const FString& Label, const FString& Insert, const FString& Detail, EVekaCompletionKind Kind)
	{
		FVekaCompletionItem& Item = Items.AddDefaulted_GetRef();
		Item.Label = Label;
		Item.Insert = Insert;
		Item.Detail = Detail;
		Item.Kind = Kind;
	}

	void AddPaints(TArray<FVekaCompletionItem>& Items)
	{
		for (const FName Token : Veka::Colora::DefaultTokenNames())
		{
			AddItem(Items, Token.ToString(), Token.ToString(), TEXT("Colora token"), EVekaCompletionKind::Value);
		}
		for (const FName Token : Veka::Colora::ControlTokenNames())
		{
			AddItem(Items, Token.ToString(), Token.ToString(), TEXT("control token"), EVekaCompletionKind::Value);
		}
		for (const FName Paint : Veka::Paints::GetAllNames())
		{
			AddItem(Items, Paint.ToString(), Paint.ToString(), TEXT("paint asset"), EVekaCompletionKind::Value);
		}
	}

	void AddVariables(TArray<FVekaCompletionItem>& Items, const FVekaCompletionSources& Sources)
	{
		if (!Sources.Document)
		{
			return;
		}
		for (const FVekaStateDeclaration& State : Sources.Document->States)
		{
			AddItem(Items, State.Name.ToString(), State.Name.ToString(), TEXT("state"), EVekaCompletionKind::Variable);
		}
		for (const FVekaParamDeclaration& Param : Sources.Document->Params)
		{
			AddItem(Items, Param.Name.ToString(), Param.Name.ToString(), TEXT("param"), EVekaCompletionKind::Variable);
		}
		for (const FVekaDocumentTimeline& Timeline : Sources.Document->Timelines)
		{
			AddItem(Items, Timeline.Name.ToString(), Timeline.Name.ToString() + TEXT("."), TEXT("timeline"), EVekaCompletionKind::Variable);
		}
	}

	/** Prefix matches first (in order), then the ones containing it; nothing typed keeps everything. */
	void Filter(TArray<FVekaCompletionItem>& Items, const FString& Prefix)
	{
		if (Prefix.IsEmpty())
		{
			return;
		}
		TArray<FVekaCompletionItem> Starts;
		TArray<FVekaCompletionItem> Contains;
		for (FVekaCompletionItem& Item : Items)
		{
			if (Item.Label.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				if (!Item.Label.Equals(Prefix, ESearchCase::CaseSensitive))
				{
					Starts.Add(MoveTemp(Item));
				}
			}
			else if (Prefix.Len() >= 2 && Item.Label.Contains(Prefix, ESearchCase::IgnoreCase))
			{
				Contains.Add(MoveTemp(Item));
			}
		}
		Starts.Append(MoveTemp(Contains));
		Items = MoveTemp(Starts);
	}
}

namespace Veka::Editor
{
	FLinearColor TokenColor(EVekaCodeToken Kind)
	{
		switch (Kind)
		{
		case EVekaCodeToken::Keyword: return FLinearColor(0.80f, 0.47f, 0.95f);
		case EVekaCodeToken::Declaration: return FLinearColor(0.98f, 0.62f, 0.30f);
		case EVekaCodeToken::Component: return FLinearColor(0.35f, 0.82f, 0.78f);
		case EVekaCodeToken::Modifier: return FLinearColor(0.45f, 0.65f, 1.f);
		case EVekaCodeToken::EnumValue: return FLinearColor(0.95f, 0.55f, 0.70f);
		case EVekaCodeToken::String: return FLinearColor(0.98f, 0.45f, 0.38f);
		case EVekaCodeToken::Number:
		case EVekaCodeToken::Color: return FLinearColor(0.82f, 0.78f, 0.45f);
		case EVekaCodeToken::Comment: return FLinearColor(0.45f, 0.48f, 0.52f);
		default: return FLinearColor(0.84f, 0.85f, 0.88f);
		}
	}

	TArray<FVekaCodeSpan> TokenizeLine(const FString& Line)
	{
		TArray<FVekaCodeSpan> Spans;
		auto Add = [&Spans](int32 Start, int32 End, EVekaCodeToken Kind)
		{
			if (End > Start && Kind != EVekaCodeToken::Plain)
			{
				Spans.Add({ Start, End - Start, Kind });
			}
		};
		int32 Index = 0;
		const int32 Len = Line.Len();
		while (Index < Len)
		{
			const TCHAR C = Line[Index];
			if (C == TEXT('/') && Index + 1 < Len && Line[Index + 1] == TEXT('/'))
			{
				Add(Index, Len, EVekaCodeToken::Comment);
				break;
			}
			if (C == TEXT('"') || (C == TEXT('#') && Index + 1 < Len && Line[Index + 1] == TEXT('"')))
			{
				const int32 Start = Index;
				Index += C == TEXT('#') ? 2 : 1;
				while (Index < Len && Line[Index] != TEXT('"'))
				{
					Index += Line[Index] == TEXT('\\') ? 2 : 1;
				}
				Index = FMath::Min(Index + 1, Len);
				Add(Start, Index, EVekaCodeToken::String);
				continue;
			}
			if (C == TEXT('#'))
			{
				const int32 Start = Index++;
				while (Index < Len && FChar::IsHexDigit(Line[Index]))
				{
					++Index;
				}
				Add(Start, Index, EVekaCodeToken::Color);
				continue;
			}
			if (FChar::IsDigit(C))
			{
				const int32 Start = Index;
				while (Index < Len && (FChar::IsDigit(Line[Index]) || Line[Index] == TEXT('.') || Line[Index] == TEXT('e')))
				{
					++Index;
				}
				Add(Start, Index, EVekaCodeToken::Number);
				continue;
			}
			if (C == TEXT('@') && Index + 1 < Len && IsIdentStart(Line[Index + 1]))
			{
				const int32 Start = Index++;
				while (Index < Len && IsIdentChar(Line[Index]))
				{
					++Index;
				}
				Add(Start, Index, EVekaCodeToken::Declaration);
				continue;
			}
			if (C == TEXT('.') && Index + 1 < Len && IsIdentStart(Line[Index + 1]))
			{
				const bool bMember = Index > 0 && (IsIdentChar(Line[Index - 1]) || Line[Index - 1] == TEXT(']'));
				const TCHAR Before = PreviousNonSpace(Line, Index);
				const int32 Start = Index++;
				while (Index < Len && IsIdentChar(Line[Index]))
				{
					++Index;
				}
				if (!bMember)
				{
					// `.padding` after a view (line start, `)`, `}`); `.leading` as a value (after `(`, `,`, `:`, an operator).
					const bool bModifier = Before == 0 || Before == TEXT(')') || Before == TEXT('}') || IsIdentChar(Before);
					Add(Start, Index, bModifier ? EVekaCodeToken::Modifier : EVekaCodeToken::EnumValue);
				}
				continue;
			}
			if (IsIdentStart(C))
			{
				const int32 Start = Index;
				while (Index < Len && IsIdentChar(Line[Index]))
				{
					++Index;
				}
				const FString Word = Line.Mid(Start, Index - Start);
				if (Keywords().Contains(Word))
				{
					Add(Start, Index, EVekaCodeToken::Keyword);
				}
				else if (FChar::IsUpper(Word[0]))
				{
					Add(Start, Index, EVekaCodeToken::Component);
				}
				continue;
			}
			++Index;
		}
		return Spans;
	}

	FVekaCompletion ComputeCompletion(const FString& Text, int32 LineIndex, int32 Column, const FVekaCompletionSources& Sources, bool bExplicit)
	{
		FVekaCompletion Out;
		const TArray<FString> Lines = SplitLines(Text);
		if (!Lines.IsValidIndex(LineIndex))
		{
			return Out;
		}
		const FString& Line = Lines[LineIndex];
		Column = FMath::Clamp(Column, 0, Line.Len());
		// Absolute position of the cursor in Text (lines joined with \n).
		int32 Position = Column;
		for (int32 Index = 0; Index < LineIndex; ++Index)
		{
			Position += Lines[Index].Len() + 1;
		}
		const FString Joined = FString::Join(Lines, TEXT("\n"));

		// Inside a comment or a string?
		bool bInString = false;
		int32 StringStart = INDEX_NONE;
		for (int32 At = 0; At < Column; ++At)
		{
			if (!bInString && Line[At] == TEXT('/') && At + 1 < Line.Len() && Line[At + 1] == TEXT('/'))
			{
				return Out;
			}
			if (Line[At] == TEXT('"') && (At == 0 || Line[At - 1] != TEXT('\\')))
			{
				bInString = !bInString;
				StringStart = At;
			}
		}
		if (bInString)
		{
			// Symbol names: Symbol(#"..."), Label(..., symbol: #"...").
			const FCallContext Call = FindCall(Joined, Position - (Column - StringStart));
			const FVekaCatalogArgument* Argument = FindArgument(Call);
			if (!Argument || Argument->Kind != EVekaArgumentKind::Name || Call.Name == TEXT("on"))
			{
				return Out;
			}
			Out.ReplaceStart = StringStart + 1;
			Out.ReplaceEnd = Column;
			Out.Prefix = Line.Mid(Out.ReplaceStart, Column - Out.ReplaceStart);
			for (const FName Name : Veka::Symbols::GetAllNames())
			{
				AddItem(Out.Items, Name.ToString(), Name.ToString(), TEXT("symbol"), EVekaCompletionKind::Symbol);
			}
			Filter(Out.Items, Out.Prefix);
			return Out;
		}

		int32 Start = Column;
		while (Start > 0 && IsIdentChar(Line[Start - 1]))
		{
			--Start;
		}
		Out.ReplaceStart = Start;
		Out.ReplaceEnd = Column;
		Out.Prefix = Line.Mid(Start, Column - Start);
		const TCHAR Before = Start > 0 ? Line[Start - 1] : 0;
		const int32 PrefixPosition = Position - Out.Prefix.Len();

		if (Before == TEXT('@'))
		{
			AddItem(Out.Items, TEXT("State"), TEXT("State var "), TEXT("state of the view"), EVekaCompletionKind::Keyword);
			AddItem(Out.Items, TEXT("Param"), TEXT("Param var "), TEXT("given by the caller"), EVekaCompletionKind::Keyword);
			AddItem(Out.Items, TEXT("Slot"), TEXT("Slot "), TEXT("content given by the caller"), EVekaCompletionKind::Keyword);
			AddItem(Out.Items, TEXT("Event"), TEXT("Event "), TEXT("sent to the caller"), EVekaCompletionKind::Keyword);
			AddItem(Out.Items, TEXT("Timeline"), TEXT("Timeline "), TEXT("keyframe animation"), EVekaCompletionKind::Keyword);
			Filter(Out.Items, Out.Prefix);
			return Out;
		}

		if (Before == TEXT('.'))
		{
			const bool bMember = Start >= 2 && (IsIdentChar(Line[Start - 2]) || Line[Start - 2] == TEXT(']'));
			if (bMember)
			{
				int32 WordStart = Start - 1;
				while (WordStart > 0 && IsIdentChar(Line[WordStart - 1]))
				{
					--WordStart;
				}
				const FString Owner = Line.Mid(WordStart, Start - 1 - WordStart);
				if (Owner == TEXT("model") && Sources.ModelClass)
				{
					for (TFieldIterator<FProperty> It(Sources.ModelClass); It; ++It)
					{
						if (It->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit) && It->GetOwnerClass() != UObject::StaticClass())
						{
							AddItem(Out.Items, It->GetName(), It->GetName(), It->GetCPPType(), EVekaCompletionKind::Variable);
						}
					}
				}
				else if (const FVekaDocumentTimeline* Timeline = Sources.Document ? Sources.Document->FindTimeline(FName(*Owner)) : nullptr)
				{
					for (const FVekaTimelineTrack& Track : Timeline->Data.Tracks)
					{
						AddItem(Out.Items, Track.Name.ToString(), Track.Name.ToString(), TEXT("track"), EVekaCompletionKind::Variable);
					}
					for (const TCHAR* Control : { TEXT("play()"), TEXT("pause()"), TEXT("stop()"), TEXT("reverse()"), TEXT("seek(") })
					{
						AddItem(Out.Items, Control, Control, TEXT("timeline control"), EVekaCompletionKind::Keyword);
					}
				}
				else if (Sources.Document && Sources.Document->States.ContainsByPredicate([&Owner](const FVekaStateDeclaration& State) { return State.Name.ToString() == Owner; }))
				{
					AddItem(Out.Items, TEXT("toggle()"), TEXT("toggle()"), TEXT("flips a Bool state"), EVekaCompletionKind::Keyword);
				}
				Filter(Out.Items, Out.Prefix);
				return Out;
			}

			const FCallContext Call = FindCall(Joined, PrefixPosition - 1);
			if (Call.bFound)
			{
				// A value: enum values or Colora tokens of that argument.
				if (const FVekaCatalogArgument* Argument = FindArgument(Call))
				{
					if (Argument->Kind == EVekaArgumentKind::Enum && Argument->Enum)
					{
						for (int32 Index = 0; Index < Argument->Enum->NumEnums() - 1; ++Index)
						{
							if (!Argument->Enum->HasMetaData(TEXT("Hidden"), Index))
							{
								const FString Name = Argument->Enum->GetNameStringByIndex(Index);
								AddItem(Out.Items, Name, Name, Argument->Enum->GetName(), EVekaCompletionKind::Value);
							}
						}
					}
					else if (Argument->Kind == EVekaArgumentKind::Paint)
					{
						AddPaints(Out.Items);
					}
					else if (Argument->Kind == EVekaArgumentKind::Style)
					{
						for (const FName Style : Veka::Components::GetAllStyleNames())
						{
							AddItem(Out.Items, Style.ToString(), Style.ToString(), TEXT("shared style"), EVekaCompletionKind::Value);
						}
					}
				}
				else if (const UVekaView* View = !Call.bModifier && Sources.ResolveView ? Sources.ResolveView(Call.Name) : nullptr)
				{
					// A Color parameter of a component.
					const FName ParamName = !Call.Argument.IsNone() ? Call.Argument : (Call.bFirstUnnamed && View->Document.Params.Num() == 1 ? View->Document.Params[0].Name : NAME_None);
					const FVekaParamDeclaration* Param = View->Document.FindParam(ParamName);
					if (Param && Param->Type == EVekaValueType::Color)
					{
						AddPaints(Out.Items);
					}
				}
				Filter(Out.Items, Out.Prefix);
				return Out;
			}

			// A modifier of the view above.
			for (const FVekaCatalogEntry& Entry : Veka::Catalog::Modifiers())
			{
				const bool bCall = Entry.Arguments.Num() > 0 || Entry.Name == TEXT("clip");
				AddItem(Out.Items, Entry.Name.ToString(), Entry.Name.ToString() + (bCall ? TEXT("(") : TEXT("")), TEXT("modifier"), EVekaCompletionKind::Modifier);
			}
			AddItem(Out.Items, TEXT("inSlot"), TEXT("inSlot("), TEXT("fills a slot of the component"), EVekaCompletionKind::Modifier);
			Filter(Out.Items, Out.Prefix);
			return Out;
		}

		if (Out.Prefix.IsEmpty() && !bExplicit)
		{
			return Out;
		}

		const FCallContext Call = FindCall(Joined, PrefixPosition);
		if (Call.bFound)
		{
			if (Call.bNoNameYet)
			{
				const UVekaView* View = !Call.bModifier && !FindEntry(Call) && Sources.ResolveView ? Sources.ResolveView(Call.Name) : nullptr;
				if (View)
				{
					// A view used as a component: its @Param declarations.
					for (const FVekaParamDeclaration& Param : View->Document.Params)
					{
						if (!Call.GivenArguments.Contains(Param.Name) && !(View->Document.Params.Num() == 1 && Call.GivenArguments.Contains(FName(TEXT("__primary")))))
						{
							FString Detail = Veka::Lang::TypeName(Param.Type);
							if (!Param.Default.IsNone())
							{
								Detail += TEXT(" = ") + Veka::Lang::PrintValue(Param.Default);
							}
							AddItem(Out.Items, Param.Name.ToString() + TEXT(":"), Param.Name.ToString() + TEXT(": "), Detail, EVekaCompletionKind::Argument);
						}
					}
				}
				else if (const FVekaCatalogEntry* Entry = FindEntry(Call))
				{
					for (const FVekaCatalogArgument& Argument : Entry->Arguments)
					{
						const bool bGiven = Call.GivenArguments.Contains(Argument.Name) || (Argument.Name == Entry->PrimaryArgument && Call.GivenArguments.Contains(FName(TEXT("__primary"))));
						if (!bGiven)
						{
							AddItem(Out.Items, Argument.Name.ToString() + TEXT(":"), Argument.Name.ToString() + TEXT(": "), TEXT("argument"), EVekaCompletionKind::Argument);
						}
					}
				}
			}
			AddVariables(Out.Items, Sources);
			AddItem(Out.Items, TEXT("model"), TEXT("model."), TEXT("view model"), EVekaCompletionKind::Keyword);
			Filter(Out.Items, Out.Prefix);
			return Out;
		}

		// Start of a statement: a view, a keyword, or an action on a state.
		for (const FVekaCatalogEntry& Entry : Veka::Catalog::Components())
		{
			const bool bCall = Entry.Arguments.Num() > 0;
			AddItem(Out.Items, Entry.Name.ToString(), Entry.Name.ToString() + (bCall ? TEXT("(") : TEXT("")), Entry.bContainer ? TEXT("container") : TEXT("component"),
				EVekaCompletionKind::Component);
		}
		for (const FName View : Sources.ProjectViews)
		{
			AddItem(Out.Items, View.ToString(), View.ToString() + TEXT("("), TEXT("view of the project"), EVekaCompletionKind::Component);
		}
		const TCHAR* Statements[] = { TEXT("if"), TEXT("else"), TEXT("ForEach"), TEXT("Slot"), TEXT("emit"), TEXT("withAnimation") };
		for (const TCHAR* Keyword : Statements)
		{
			AddItem(Out.Items, Keyword, Keyword, TEXT("keyword"), EVekaCompletionKind::Keyword);
		}
		AddVariables(Out.Items, Sources);
		Filter(Out.Items, Out.Prefix);
		return Out;
	}
}
