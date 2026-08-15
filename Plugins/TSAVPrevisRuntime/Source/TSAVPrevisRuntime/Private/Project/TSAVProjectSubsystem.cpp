// Copyright TSAV. All Rights Reserved.

#include "Project/TSAVProjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVProjectSubsystem)

void UTSAVProjectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NewProject();
}

void UTSAVProjectSubsystem::NewProject(const FString& InProjectName)
{
	ProjectId = FGuid::NewGuid();
	ProjectName = InProjectName.IsEmpty() ? TEXT("Untitled Show") : InProjectName;
	bDirty = false;
	OnProjectChanged.Broadcast();
}

void UTSAVProjectSubsystem::MarkDirty(const bool bInDirty)
{
	if (bDirty == bInDirty)
	{
		return;
	}

	bDirty = bInDirty;
	OnProjectChanged.Broadcast();
}
