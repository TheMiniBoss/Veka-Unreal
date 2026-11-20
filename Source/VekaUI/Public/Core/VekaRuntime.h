// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Core/VekaElement.h"
#include "Core/VekaEnvironment.h"
#include "Anima/VekaAnimaScheduler.h"

class FVekaNode;
class FVekaImplicitAnimator;
class FVekaTimelinePlayer;

/**
 * Owns a node tree and keeps it in sync with an element tree (the reconciler).
 *
 *  - SetRoot(Element): diff against the current tree; nodes are reused when kind + type + key match.
 *  - State changes mark view nodes dirty; Flush() re-evaluates only those views (coalesced).
 *  - Holds the Anima scheduler. Works without Slate (tests, headless); SVekaHost adds the widgets.
 */
class VEKAUI_API FVekaRuntime : public FGCObject, public TSharedFromThis<FVekaRuntime>
{
public:
	FVekaRuntime();
	virtual ~FVekaRuntime() override;

	void SetRoot(const FVekaElement& Root);
	void SetEnvironment(const FVekaEnvironment& Environment);
	const FVekaEnvironment& GetEnvironment() const { return RootEnvironment; }

	TSharedPtr<FVekaNode> GetRootNode() const { return Root; }

	/** Re-evaluates dirty views. Returns true if anything was re-evaluated. */
	bool Flush();

	/** Flush + advance animations + flush. Returns true while work remains (dirty views or animations). */
	bool Tick(double DeltaSeconds);

	bool HasPendingWork() const;

	FVekaAnimaScheduler& GetScheduler() { return Scheduler; }
	FVekaImplicitAnimator& GetImplicit() { return *Implicit; }

	/** Spec of the animation transaction whose layout changes are being placed (valid for the frame after the flush). */
	const FVekaAnimationSpec* GetLayoutTransaction() const;
	void BeginLayoutTransaction(const FVekaAnimationSpec& Spec);

	/** Timelines tick with the runtime while they play. */
	void RegisterTimeline(const TSharedRef<FVekaTimelinePlayer>& Player);

	/** Called when the runtime needs a tick (state changed, animation started). Set by the host. */
	TFunction<void()> OnUpdateRequested;

	/** Broadcast when the root node instance changed (the host swaps its root widget). */
	FSimpleMulticastDelegate OnRootChanged;

	void RequestUpdate();
	void MarkNodeDirty(FVekaNode& Node);

	/** Last frame (root space) of the views with a matched-geometry id (`.MatchedGeometry`). */
	struct FMatchedGeometry
	{
		TWeakPtr<FVekaNode> Node;
		FVekaRect RootRect;
	};
	void RecordMatchedGeometry(FName Key, FVekaNode& Node, const FVekaRect& RootRect);
	const FMatchedGeometry* FindMatchedGeometry(FName Key) const;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FVekaRuntime"); }

private:
	TSharedPtr<FVekaNode> Reconcile(FVekaNode* Parent, const TSharedPtr<FVekaNode>& Existing, const FVekaElement& Element,
		const FVekaEnvironment& InheritedEnvironment, FName Key);
	void EvaluateNode(FVekaNode& Node, bool bDescriptionChanged);
	bool ReconcileChildren(FVekaNode& Node, const TArray<FVekaElement>& Elements);

	TSharedPtr<FVekaNode> Root;
	FVekaElement RootElement;
	FVekaEnvironment RootEnvironment;
	TArray<TWeakPtr<FVekaNode>> DirtyNodes;
	FVekaAnimaScheduler Scheduler;
	TUniquePtr<FVekaImplicitAnimator> Implicit;
	TArray<TWeakPtr<FVekaTimelinePlayer>> Timelines;
	/** Transaction of the reconcile in progress (WithAnimation / `.Animation(spec, value)`). */
	TOptional<FVekaAnimationSpec> CurrentTransaction;
	TOptional<FVekaAnimationSpec> LayoutTransaction;
	uint64 LayoutTransactionFrame = 0;
	bool bFlushing = false;
	TMap<FName, FMatchedGeometry> MatchedGeometry;

	void TickTimelines(double DeltaSeconds);
};
