// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "View/VekaDocument.h"
#include "VekaComponentLibrary.generated.h"

class UVekaView;

/**
 * A named set of modifiers shared by views: `.style(.Card)` applies them, in order, before the view's own modifiers.
 * Written in Veka (`.padding(12).background(.Card, radius: 12)`); the editor compiles the text into Compiled.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSharedStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Style")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Style", meta = (MultiLine = true))
	FText Description;

	/** The modifiers, in Veka: `.padding(12).background(.Card, radius: 12)`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Style", meta = (MultiLine = true))
	FString Modifiers;

	/** Modifiers parsed by the editor (one node holding them), what views apply at runtime. */
	UPROPERTY(VisibleAnywhere, Category = "Veka|Style", AdvancedDisplay)
	FVekaDocument Compiled;

	/** Problems found when compiling Modifiers (empty when it compiles). */
	UPROPERTY(VisibleAnywhere, Category = "Veka|Style", AdvancedDisplay)
	FString CompileErrors;
};

/**
 * Components and styles shared by the views of a project or a team: the palette, completion and the inspector list
 * the library's components under its name, and `.style(.Name)` finds its styles. Loaded libraries register
 * themselves; the editor loads all of them.
 */
UCLASS(BlueprintType)
class VEKAUI_API UVekaComponentLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Shown in the palette (empty: the asset name). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Library")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Library")
	TArray<TObjectPtr<UVekaView>> Components;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Library")
	TArray<FVekaSharedStyle> Styles;

	FText GetLibraryName() const;
	const FVekaSharedStyle* FindStyle(FName Name) const;

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif
};

namespace Veka::Components
{
	VEKAUI_API void RegisterLibrary(UVekaComponentLibrary* Library);
	VEKAUI_API void UnregisterLibrary(UVekaComponentLibrary* Library);
	VEKAUI_API TArray<UVekaComponentLibrary*> GetLibraries();
	/** Style by name in the registered libraries (first library wins), or nullptr. */
	VEKAUI_API const FVekaSharedStyle* FindStyle(FName Name);
	VEKAUI_API TArray<FName> GetAllStyleNames();
	/** The library that lists View, or nullptr. */
	VEKAUI_API const UVekaComponentLibrary* FindLibraryOf(const UVekaView* View);

	/** Set by the editor: compiles a style's Modifiers into Compiled (VekaLang is editor only). */
	VEKAUI_API TFunction<void(FVekaSharedStyle&)>& StyleCompiler();
	/** Compiles every style of Library with StyleCompiler (when set). */
	VEKAUI_API void CompileStyles(UVekaComponentLibrary& Library);

	/** Bumped when a library or one of its styles changes (open views refresh). */
	VEKAUI_API uint32 GetRevision();
	VEKAUI_API void NotifyChanged();
}
