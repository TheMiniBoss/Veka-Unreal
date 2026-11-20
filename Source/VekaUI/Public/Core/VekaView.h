// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Core/VekaElement.h"
#include "Core/VekaEnvironment.h"
#include "Core/VekaState.h"

class FVekaNode;
class FVekaRuntime;
class FVekaTimelineHandle;
struct FVekaTimelineData;

/**
 * Given to a view's Body: access to @State storage and to the environment.
 */
class VEKAUI_API FVekaContext
{
public:
	FVekaContext(FVekaNode& InNode, const FVekaEnvironment& InEnvironment);

	/**
	 * State stored in the node (SwiftUI's @State). Created with Initial the first time, then kept across
	 * re-evaluations. Use a different name for each state of the view; always the same T for a name.
	 */
	template <typename T>
	TVekaState<T> State(FName Name, const T& Initial)
	{
		TSharedPtr<FVekaStateBoxBase>& Slot = FindOrAddStateSlot(Name);
		if (!Slot.IsValid() || Slot->TypeSize != sizeof(T))
		{
			TSharedRef<TVekaStateBox<T>> NewBox = MakeShared<TVekaStateBox<T>>();
			NewBox->Value = Initial;
			NewBox->TypeSize = sizeof(T);
			NewBox->Owner = GetNodeWeak();
			Slot = NewBox;
		}
		return TVekaState<T>(StaticCastSharedRef<TVekaStateBox<T>>(Slot.ToSharedRef()));
	}

	/**
	 * Timeline stored in the view (playback survives re-evaluations). Read track values in Body; while it plays,
	 * the view is re-evaluated every frame. Include "Anima/VekaTimeline.h".
	 */
	FVekaTimelineHandle Timeline(FName Name, const FVekaTimelineData& Data);

	const FVekaEnvironment& GetEnvironment() const { return Environment; }
	FVekaNode& GetNode() const { return Node; }
	FVekaRuntime& GetRuntime() const;

private:
	TSharedPtr<FVekaStateBoxBase>& FindOrAddStateSlot(FName Name);
	TWeakPtr<FVekaNode> GetNodeWeak() const;

	FVekaNode& Node;
	const FVekaEnvironment& Environment;
};

/**
 * A composite view: a C++ class with a Body, like a SwiftUI View.
 *
 *   class FCounterView : public FVekaView
 *   {
 *       VEKA_VIEW(FCounterView)
 *   public:
 *       FText Title;
 *       explicit FCounterView(FText InTitle) : Title(MoveTemp(InTitle)) {}
 *       virtual FVekaElement Body(FVekaContext& Ctx) const override { ... }
 *   };
 *   ... Veka::View<FCounterView>(INVTEXT("Clicks")) ...
 *
 * Members are the view's parameters (set by the parent). Long-lived values go in Ctx.State(...).
 */
class VEKAUI_API FVekaView : public TSharedFromThis<FVekaView>
{
public:
	virtual ~FVekaView() = default;

	virtual FVekaElement Body(FVekaContext& Context) const = 0;

	/** Unique per class; provided by VEKA_VIEW. Nodes are reused only for the same view type. */
	virtual FName GetViewType() const = 0;

	/** Report UObjects held by the view (called by the runtime's FGCObject). */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
};

#define VEKA_VIEW(ClassName) \
public: \
	static FName StaticViewType() { static const FName TypeName(TEXT(#ClassName)); return TypeName; } \
	virtual FName GetViewType() const override { return StaticViewType(); } \
private:
