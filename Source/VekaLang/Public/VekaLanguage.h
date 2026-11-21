// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/VekaDocument.h"

class UVekaView;

/**
 * The Veka language (.vk): text form of a view document. Parse gives an FVekaDocument (the source of truth, stored in
 * UVekaView assets), Print gives back canonical text. Editor-only module: games interpret the document, never the text.
 *
 *   // A counter
 *   view Counter {
 *       @State var count: Int = 0
 *
 *       VStack(spacing: 12) {
 *           Text("Tapped \(count) times")
 *               .font(.title2)
 *           Button("Add") {
 *               withAnimation { count += 1 }
 *           }
 *       }
 *       .padding(24)
 *   }
 */

enum class EVekaDiagnosticSeverity : uint8
{
	Error,
	Warning
};

struct VEKALANG_API FVekaDiagnostic
{
	EVekaDiagnosticSeverity Severity = EVekaDiagnosticSeverity::Error;
	/** Localizable (follows the editor language). */
	FText Message;
	FVekaSourceRange Range;

	/** "12:5: error: Unknown name 'cnt'". */
	FString ToString() const;
};

struct VEKALANG_API FVekaParseOptions
{
	/** Finds a view used as a component by its name (Veka::Lang::MakeAssetResolver: the project's UVekaView assets). */
	TFunction<UVekaView*(FName)> ResolveComponent;
	/** Localization namespace of the texts; default "Veka.<view name>". */
	FString TextNamespace;
};

struct VEKALANG_API FVekaParseResult
{
	/** Best effort even with errors (the editor keeps the last valid document). */
	FVekaDocument Document;
	FName ViewName;
	TArray<FVekaDiagnostic> Diagnostics;

	bool HasErrors() const;
};

namespace Veka::Lang
{
	VEKALANG_API FVekaParseResult Parse(const FString& Source, const FVekaParseOptions& Options = FVekaParseOptions());

	/** Canonical text of a document (4-space indent, primary arguments unnamed, one modifier per line). */
	VEKALANG_API FString Print(const FVekaDocument& Document, FName ViewName);

	/**
	 * Checks a document against the catalog and the schema: unknown components / modifiers / arguments, enum values,
	 * Colora tokens, bindings that are not states; plus FVekaDocument::Validate.
	 */
	VEKALANG_API TArray<FVekaDiagnostic> Analyze(const FVekaDocument& Document);

	/**
	 * Same document, ignoring how it is laid out in memory (expression / node indices, argument order, source ranges).
	 * The round-trip criterion: StructurallyEqual(Parse(Print(Doc)), Doc).
	 */
	VEKALANG_API bool StructurallyEqual(const FVekaDocument& A, const FVekaDocument& B, FString* OutDifference = nullptr);

	/** Resolver over the project's UVekaView assets (asset registry), by asset name. */
	VEKALANG_API TFunction<UVekaView*(FName)> MakeAssetResolver();

	/** Name of a value type in the language ("Int", "Text"...). */
	VEKALANG_API FString TypeName(EVekaValueType Type);
}
