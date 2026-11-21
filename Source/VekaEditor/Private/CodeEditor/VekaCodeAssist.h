// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FVekaDocument;

/** What a piece of a code line is (its color in the Code tab). */
enum class EVekaCodeToken : uint8
{
	Plain,
	Keyword,
	/** @State, @Param, @Slot, @Event. */
	Declaration,
	/** Capitalized name: component, view, type. */
	Component,
	/** `.padding` after a view. */
	Modifier,
	/** `.leading` as a value. */
	EnumValue,
	String,
	Number,
	Color,
	Comment
};

struct FVekaCodeSpan
{
	int32 Start = 0;
	int32 Length = 0;
	EVekaCodeToken Kind = EVekaCodeToken::Plain;
};

enum class EVekaCompletionKind : uint8
{
	Component,
	Modifier,
	Argument,
	Value,
	Variable,
	Keyword,
	Symbol
};

struct FVekaCompletionItem
{
	/** Shown in the list. */
	FString Label;
	/** Replaces the typed prefix. */
	FString Insert;
	/** Short hint shown next to the label ("modifier", "Colora token"...). */
	FString Detail;
	EVekaCompletionKind Kind = EVekaCompletionKind::Keyword;
};

struct FVekaCompletion
{
	/** Columns (in the cursor line) the accepted item replaces: the prefix typed so far. */
	int32 ReplaceStart = 0;
	int32 ReplaceEnd = 0;
	FString Prefix;
	TArray<FVekaCompletionItem> Items;
};

/** What completion can offer beyond the catalog. */
struct FVekaCompletionSources
{
	const FVekaDocument* Document = nullptr;
	/** Views of the project usable as components. */
	TArray<FName> ProjectViews;
	/** Preview model (`model.` offers its properties). */
	const UClass* ModelClass = nullptr;
};

namespace Veka::Editor
{
	/** Colors of one line (strings never span lines in Veka). Spans are sorted and do not overlap; gaps are Plain. */
	TArray<FVekaCodeSpan> TokenizeLine(const FString& Line);

	/**
	 * Suggestions at the cursor, from the context: components / views / keywords at the start of a line, modifiers
	 * after `.`, argument names inside a call, enum values or Colora tokens after `.` in an argument, `@` declarations,
	 * symbol names in a Symbol string, states, params and the model's properties.
	 *
	 * @param bExplicit Ctrl+Space: suggest even with nothing typed.
	 */
	FVekaCompletion ComputeCompletion(const FString& Text, int32 Line, int32 Column, const FVekaCompletionSources& Sources, bool bExplicit);
}
