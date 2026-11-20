// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaView.h"
#include "Core/VekaNode.h"
#include "Core/VekaRuntime.h"

FVekaContext::FVekaContext(FVekaNode& InNode, const FVekaEnvironment& InEnvironment)
	: Node(InNode)
	, Environment(InEnvironment)
{
}

FVekaRuntime& FVekaContext::GetRuntime() const
{
	return Node.GetRuntime();
}

TSharedPtr<FVekaStateBoxBase>& FVekaContext::FindOrAddStateSlot(FName Name)
{
	return Node.States.FindOrAdd(Name);
}

TWeakPtr<FVekaNode> FVekaContext::GetNodeWeak() const
{
	return Node.AsShared();
}
