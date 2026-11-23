// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaEnvironment.h"
#include "Styling/CoreStyle.h"
#include "Typography/VekaTypography.h"

bool FVekaEnvironment::operator==(const FVekaEnvironment& Other) const
{
	if (Values.Num() != Other.Values.Num())
	{
		return false;
	}
	for (const TPair<FName, TSharedPtr<const FVekaEnvValueBase>>& Pair : Values)
	{
		const TSharedPtr<const FVekaEnvValueBase>* OtherValue = Other.Values.Find(Pair.Key);
		if (!OtherValue)
		{
			return false;
		}
		if (Pair.Value != *OtherValue && !Pair.Value->Equals(**OtherValue))
		{
			return false;
		}
	}
	return true;
}

namespace VekaEnv
{
	FLinearColor GetForegroundColor(const FVekaEnvironment& Environment)
	{
		return Environment.Get<FLinearColor>(ForegroundColor(), FLinearColor::White);
	}

	FSlateFontInfo GetFont(const FVekaEnvironment& Environment)
	{
		return Veka::Typography::Resolve(Environment).Font;
	}

	EVekaColorScheme GetColorScheme(const FVekaEnvironment& Environment)
	{
		return Environment.Get<EVekaColorScheme>(ColorScheme(), EVekaColorScheme::Dark);
	}

	FVekaEnvironment MakeDefault()
	{
		FVekaEnvironment Environment;
		Environment.Set<EVekaColorScheme>(ColorScheme(), EVekaColorScheme::Dark);
		Environment.Set<FLinearColor>(ForegroundColor(), FLinearColor::White);
		return Environment;
	}
}
