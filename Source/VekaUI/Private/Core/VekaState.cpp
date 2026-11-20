// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaState.h"
#include "Core/VekaNode.h"
#include "Core/VekaRuntime.h"

void FVekaStateBoxBase::NotifyChanged() const
{
	if (TSharedPtr<FVekaNode> Node = Owner.Pin())
	{
		Node->MarkDirty();
	}
}

void FVekaStateBoxBase::AnimateScalar(double From, double To, const FVekaAnimationSpec& Spec, TFunction<void(double)> Apply) const
{
	TSharedPtr<FVekaNode> Node = Owner.Pin();
	if (!Node.IsValid())
	{
		if (Apply)
		{
			Apply(To);
		}
		return;
	}

	FVekaRuntime& Runtime = Node->GetRuntime();
	Runtime.GetScheduler().Animate(Spec, From, To, MoveTemp(Apply), nullptr, this);
	Runtime.RequestUpdate();
}
