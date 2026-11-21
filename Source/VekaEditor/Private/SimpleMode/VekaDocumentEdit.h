// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/VekaDocument.h"

class UVekaView;

/** Where a node sits: its parent and the list it is in (the root has no parent). */
struct FVekaNodeLocation
{
	int32 Parent = INDEX_NONE;
	/** In the parent's else branch (If). */
	bool bElse = false;
	int32 Index = INDEX_NONE;

	bool IsRoot() const { return Parent == INDEX_NONE; }
};

/**
 * Edits of a view document, used by the Simple mode (outliner, palette, inspector, State tab). They change a copy that
 * the caller hands to Veka::Editor::ApplyDocument (one undo step), then Compact keeps the flat arrays tidy. Nodes are
 * addressed by index; Path / FromPath give an address that survives edits and re-parses (selection).
 */
namespace Veka::Editor::Doc
{
	// --- Structure ---------------------------------------------------------------------------------------------

	FVekaNodeLocation FindLocation(const FVekaDocument& Document, int32 Node);
	/** Containers (VStack...), If, ForEach, and components that have slots. */
	bool CanHaveChildren(const FVekaDocument& Document, int32 Node);
	/** True if Node is Ancestor or inside it. */
	bool IsInside(const FVekaDocument& Document, int32 Node, int32 Ancestor);

	/** Address from the root: one entry per level (child index * 2, + 1 in an else branch). Empty for the root. */
	TArray<int32> Path(const FVekaDocument& Document, int32 Node);
	int32 FromPath(const FVekaDocument& Document, const TArray<int32>& Path);
	/** Node at a source line (the deepest whose range starts on or before it) in a parsed document. */
	int32 NodeAtLine(const FVekaDocument& Document, int32 Line);

	/** Names in scope at a node: loop variables of its ForEach ancestors, `payload` inside an `.on` modifier. */
	TArray<FName> LocalsAt(const FVekaDocument& Document, int32 Node, int32 Modifier = INDEX_NONE);

	/** Short name of a node for lists: its label (first comment), else its type ("if", "ForEach", "Slot(badge)"). */
	FString DisplayName(const FVekaDocument& Document, int32 Node);

	// --- Nodes -------------------------------------------------------------------------------------------------

	/**
	 * New node with useful default arguments: a catalog component, "if", "ForEach", "Slot", or a view used as a
	 * component (Component set). With no root it becomes the root. Returns its index.
	 */
	int32 Insert(FVekaDocument& Document, FName Type, int32 Parent, bool bElse, int32 Index, UVekaView* Component = nullptr, const FString& TextNamespace = FString());
	void Remove(FVekaDocument& Document, const TArray<int32>& Nodes);
	/** False if the move would put a node inside itself. */
	bool Move(FVekaDocument& Document, int32 Node, int32 NewParent, bool bElse, int32 Index);
	/** Deep copy placed right after the node. */
	int32 Duplicate(FVekaDocument& Document, int32 Node);
	/** Puts sibling nodes in a new container (VStack, HStack, ZStack) at the place of the first. */
	int32 Wrap(FVekaDocument& Document, const TArray<int32>& Nodes, FName Container);
	/** Label shown in the outliner (stored as the node's first comment, `// Label` in code). */
	void SetLabel(FVekaDocument& Document, int32 Node, const FString& Label);
	/** Drops unreachable nodes and expressions. Nodes keep their order of appearance. */
	void Compact(FVekaDocument& Document);

	// --- Arguments and modifiers -------------------------------------------------------------------------------

	int32 AddLiteral(FVekaDocument& Document, const FVekaValue& Value);
	/** Argument of the node (Modifier = INDEX_NONE) or of one of its modifiers; Expr = INDEX_NONE removes it. */
	void SetArgument(FVekaDocument& Document, int32 Node, int32 Modifier, FName Name, int32 Expr);
	int32 FindArgument(const FVekaDocument& Document, int32 Node, int32 Modifier, FName Name);
	/** Appends a modifier with default arguments; returns its index in the node's stack. */
	int32 AddModifier(FVekaDocument& Document, int32 Node, FName Name);
	void RemoveModifier(FVekaDocument& Document, int32 Node, int32 Modifier);
	void MoveModifier(FVekaDocument& Document, int32 Node, int32 From, int32 To);
	/** Actions of the node (Modifier = INDEX_NONE: Button content) or of a modifier (onTap, on). */
	TArray<FVekaAstAction>* Actions(FVekaDocument& Document, int32 Node, int32 Modifier);

	// --- Declarations ------------------------------------------------------------------------------------------

	/** Name not used yet by a state, param, event or slot ("count", "count2"...). */
	FName UniqueName(const FVekaDocument& Document, const FString& Base);
	void AddState(FVekaDocument& Document, FName Name, const FVekaValue& Initial);
	void AddParam(FVekaDocument& Document, FName Name, EVekaValueType Type, const FVekaValue& Default);
	void AddEvent(FVekaDocument& Document, FName Name, EVekaValueType PayloadType);
	void AddSlot(FVekaDocument& Document, FName Name);
	/** Removes a state / param / event / slot by name. */
	void RemoveDeclaration(FVekaDocument& Document, FName Name);
	/** Renames a declaration and every use of it (reads, action targets, emits, slot nodes). False if the new name is taken. */
	bool RenameDeclaration(FVekaDocument& Document, FName Old, FName New);
}
