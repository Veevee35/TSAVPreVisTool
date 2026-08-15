// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVCommandSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Interaction/TSAVSceneObjectActor.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Project/TSAVProjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVCommandSubsystem)

namespace TSAVCommands::Private
{
	struct FActorSnapshot
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UWorld> World;
		FString ClassPath;
		FTransform Transform;
		FGuid ObjectId;
		FText DisplayName;
		ETSAVObjectType ObjectType = ETSAVObjectType::Unknown;
		bool bLocked = false;
		bool bVisible = true;

		static FActorSnapshot Capture(AActor* InActor)
		{
			FActorSnapshot Snapshot;
			if (!IsValid(InActor))
			{
				return Snapshot;
			}

			Snapshot.Actor = InActor;
			Snapshot.World = InActor->GetWorld();
			Snapshot.ClassPath = InActor->GetClass()->GetPathName();
			Snapshot.Transform = InActor->GetActorTransform();
			if (const UTSAVSceneObjectComponent* SceneObject = InActor->FindComponentByClass<UTSAVSceneObjectComponent>())
			{
				Snapshot.ObjectId = SceneObject->ObjectId;
				Snapshot.DisplayName = SceneObject->DisplayName;
				Snapshot.ObjectType = SceneObject->ObjectType;
				Snapshot.bLocked = SceneObject->bLocked;
				Snapshot.bVisible = SceneObject->bVisible;
			}
			return Snapshot;
		}

		AActor* Spawn()
		{
			UWorld* TargetWorld = World.Get();
			UClass* ActorClass = LoadObject<UClass>(nullptr, *ClassPath);
			if (!TargetWorld || !ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* SpawnedActor = TargetWorld->SpawnActor<AActor>(ActorClass, Transform, SpawnParameters);
			if (UTSAVSceneObjectComponent* SceneObject = SpawnedActor ? SpawnedActor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr)
			{
				SceneObject->ObjectId = ObjectId.IsValid() ? ObjectId : FGuid::NewGuid();
				SceneObject->DisplayName = DisplayName;
				SceneObject->ObjectType = ObjectType;
				SceneObject->bLocked = bLocked;
				SceneObject->SetObjectVisible(bVisible);
			}
			Actor = SpawnedActor;
			return SpawnedActor;
		}
	};

	AActor* FindActorById(UWorld* World, const FGuid& ObjectId)
	{
		if (!World || !ObjectId.IsValid())
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UTSAVSceneObjectComponent* SceneObject = It->FindComponentByClass<UTSAVSceneObjectComponent>())
			{
				if (SceneObject->ObjectId == ObjectId)
				{
					return *It;
				}
			}
		}
		return nullptr;
	}
}

class UTSAVCommandSubsystem::ITSAVCommand
{
public:
	virtual ~ITSAVCommand() = default;
	virtual void Execute() = 0;
	virtual void Undo() = 0;
	virtual FText GetDescription() const = 0;
	virtual AActor* GetAffectedActor() const = 0;
};

namespace TSAVCommands::Private
{
	class FSpawnCommand final : public UTSAVCommandSubsystem::ITSAVCommand
	{
	public:
		explicit FSpawnCommand(FActorSnapshot InSnapshot) : Snapshot(MoveTemp(InSnapshot)) {}
		virtual void Execute() override { Snapshot.Spawn(); }
		virtual void Undo() override { if (Snapshot.Actor.IsValid()) { Snapshot.Actor->Destroy(); } }
		virtual FText GetDescription() const override { return NSLOCTEXT("TSAVPreVis", "SpawnCommand", "Add Object"); }
		virtual AActor* GetAffectedActor() const override { return Snapshot.Actor.Get(); }
		AActor* GetActor() const { return Snapshot.Actor.Get(); }
	private:
		FActorSnapshot Snapshot;
	};

	class FDeleteCommand final : public UTSAVCommandSubsystem::ITSAVCommand
	{
	public:
		explicit FDeleteCommand(AActor* Actor) : Snapshot(FActorSnapshot::Capture(Actor)) {}
		virtual void Execute() override { if (Snapshot.Actor.IsValid()) { Snapshot.Actor->Destroy(); } }
		virtual void Undo() override { Snapshot.Spawn(); }
		virtual FText GetDescription() const override { return NSLOCTEXT("TSAVPreVis", "DeleteCommand", "Delete Object"); }
		virtual AActor* GetAffectedActor() const override { return Snapshot.Actor.Get(); }
	private:
		FActorSnapshot Snapshot;
	};

	class FTransformCommand final : public UTSAVCommandSubsystem::ITSAVCommand
	{
	public:
		FTransformCommand(AActor* InActor, const FTransform& InBefore, const FTransform& InAfter, FText InDescription)
			: Actor(InActor), World(InActor ? InActor->GetWorld() : nullptr), Before(InBefore), After(InAfter), Description(MoveTemp(InDescription))
		{
			if (const UTSAVSceneObjectComponent* SceneObject = InActor ? InActor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr)
			{
				ObjectId = SceneObject->ObjectId;
			}
		}
		virtual void Execute() override { Apply(After); }
		virtual void Undo() override { Apply(Before); }
		virtual FText GetDescription() const override { return Description; }
		virtual AActor* GetAffectedActor() const override { return Resolve(); }
	private:
		AActor* Resolve() const { return Actor.IsValid() ? Actor.Get() : FindActorById(World.Get(), ObjectId); }
		void Apply(const FTransform& Transform) { if (AActor* Target = Resolve()) { Target->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics); } }
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UWorld> World;
		FGuid ObjectId;
		FTransform Before;
		FTransform After;
		FText Description;
	};

	enum class EPropertyKind : uint8 { DisplayName, Locked, Visible };

	class FPropertyCommand final : public UTSAVCommandSubsystem::ITSAVCommand
	{
	public:
		FPropertyCommand(AActor* InActor, EPropertyKind InKind, FString InBefore, FString InAfter, FText InDescription)
			: Actor(InActor), World(InActor ? InActor->GetWorld() : nullptr), Kind(InKind), Before(MoveTemp(InBefore)), After(MoveTemp(InAfter)), Description(MoveTemp(InDescription))
		{
			if (const UTSAVSceneObjectComponent* SceneObject = InActor ? InActor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr)
			{
				ObjectId = SceneObject->ObjectId;
			}
		}
		virtual void Execute() override { Apply(After); }
		virtual void Undo() override { Apply(Before); }
		virtual FText GetDescription() const override { return Description; }
		virtual AActor* GetAffectedActor() const override { return Resolve(); }
	private:
		AActor* Resolve() const { return Actor.IsValid() ? Actor.Get() : FindActorById(World.Get(), ObjectId); }
		void Apply(const FString& Value)
		{
			if (UTSAVSceneObjectComponent* SceneObject = Resolve() ? Resolve()->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr)
			{
				switch (Kind)
				{
				case EPropertyKind::DisplayName: SceneObject->DisplayName = FText::FromString(Value); break;
				case EPropertyKind::Locked: SceneObject->bLocked = Value.ToBool(); break;
				case EPropertyKind::Visible: SceneObject->SetObjectVisible(Value.ToBool()); break;
				}
			}
		}
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UWorld> World;
		FGuid ObjectId;
		EPropertyKind Kind;
		FString Before;
		FString After;
		FText Description;
	};
}

void UTSAVCommandSubsystem::Deinitialize()
{
	ClearHistory();
	Super::Deinitialize();
}

AActor* UTSAVCommandSubsystem::SpawnSceneObject(UWorld* World, const FTransform& Transform, const FText& DisplayName)
{
	if (!World)
	{
		return nullptr;
	}
	TSAVCommands::Private::FActorSnapshot Snapshot;
	Snapshot.World = World;
	Snapshot.ClassPath = ATSAVSceneObjectActor::StaticClass()->GetPathName();
	Snapshot.Transform = Transform;
	Snapshot.ObjectId = FGuid::NewGuid();
	Snapshot.DisplayName = DisplayName.IsEmpty() ? NSLOCTEXT("TSAVPreVis", "DefaultCubeName", "Scene Cube") : DisplayName;
	Snapshot.ObjectType = ETSAVObjectType::Scenic;
	const TSharedRef<TSAVCommands::Private::FSpawnCommand> Command = MakeShared<TSAVCommands::Private::FSpawnCommand>(MoveTemp(Snapshot));
	ExecuteAndStore(Command);
	return Command->GetActor();
}

AActor* UTSAVCommandSubsystem::DuplicateActor(AActor* SourceActor, const FVector& WorldOffset)
{
	if (!IsValid(SourceActor))
	{
		return nullptr;
	}
	TSAVCommands::Private::FActorSnapshot Snapshot = TSAVCommands::Private::FActorSnapshot::Capture(SourceActor);
	Snapshot.Actor = nullptr;
	Snapshot.ObjectId = FGuid::NewGuid();
	Snapshot.Transform.AddToTranslation(WorldOffset);
	Snapshot.DisplayName = FText::Format(NSLOCTEXT("TSAVPreVis", "DuplicateName", "{0} Copy"), Snapshot.DisplayName);
	const TSharedRef<TSAVCommands::Private::FSpawnCommand> Command = MakeShared<TSAVCommands::Private::FSpawnCommand>(MoveTemp(Snapshot));
	ExecuteAndStore(Command);
	return Command->GetActor();
}

bool UTSAVCommandSubsystem::DeleteActor(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->FindComponentByClass<UTSAVSceneObjectComponent>())
	{
		return false;
	}
	ExecuteAndStore(MakeShared<TSAVCommands::Private::FDeleteCommand>(Actor));
	return true;
}

bool UTSAVCommandSubsystem::SetActorTransform(AActor* Actor, const FTransform& NewTransform, const FText& Description)
{
	if (!IsValid(Actor) || Actor->GetActorTransform().Equals(NewTransform))
	{
		return false;
	}
	ExecuteAndStore(MakeShared<TSAVCommands::Private::FTransformCommand>(Actor, Actor->GetActorTransform(), NewTransform, Description));
	return true;
}

bool UTSAVCommandSubsystem::SetDisplayName(AActor* Actor, const FText& NewDisplayName)
{
	UTSAVSceneObjectComponent* SceneObject = IsValid(Actor) ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
	if (!SceneObject || NewDisplayName.IsEmpty() || SceneObject->DisplayName.EqualTo(NewDisplayName))
	{
		return false;
	}
	ExecuteAndStore(MakeShared<TSAVCommands::Private::FPropertyCommand>(Actor, TSAVCommands::Private::EPropertyKind::DisplayName,
		SceneObject->DisplayName.ToString(), NewDisplayName.ToString(), NSLOCTEXT("TSAVPreVis", "RenameCommand", "Rename Object")));
	return true;
}

bool UTSAVCommandSubsystem::SetLocked(AActor* Actor, const bool bLocked)
{
	UTSAVSceneObjectComponent* SceneObject = IsValid(Actor) ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
	if (!SceneObject || SceneObject->bLocked == bLocked)
	{
		return false;
	}
	ExecuteAndStore(MakeShared<TSAVCommands::Private::FPropertyCommand>(Actor, TSAVCommands::Private::EPropertyKind::Locked,
		LexToString(SceneObject->bLocked), LexToString(bLocked), NSLOCTEXT("TSAVPreVis", "LockCommand", "Change Lock")));
	return true;
}

bool UTSAVCommandSubsystem::SetVisible(AActor* Actor, const bool bVisible)
{
	UTSAVSceneObjectComponent* SceneObject = IsValid(Actor) ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
	if (!SceneObject || SceneObject->bVisible == bVisible)
	{
		return false;
	}
	ExecuteAndStore(MakeShared<TSAVCommands::Private::FPropertyCommand>(Actor, TSAVCommands::Private::EPropertyKind::Visible,
		LexToString(SceneObject->bVisible), LexToString(bVisible), NSLOCTEXT("TSAVPreVis", "VisibilityCommand", "Change Visibility")));
	return true;
}

void UTSAVCommandSubsystem::BeginTransformTransaction(AActor* Actor)
{
	CancelTransformTransaction();
	if (IsValid(Actor))
	{
		TransactionActor = Actor;
		TransactionStartTransform = Actor->GetActorTransform();
		bTransformTransactionActive = true;
	}
}

void UTSAVCommandSubsystem::UpdateTransformTransaction(AActor* Actor, const FTransform& NewTransform)
{
	if (bTransformTransactionActive && Actor == TransactionActor.Get())
	{
		Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		OnObjectChanged.Broadcast(Actor);
	}
}

void UTSAVCommandSubsystem::EndTransformTransaction(AActor* Actor, const FText& Description)
{
	if (!bTransformTransactionActive || Actor != TransactionActor.Get())
	{
		return;
	}
	const FTransform EndTransform = Actor->GetActorTransform();
	bTransformTransactionActive = false;
	TransactionActor = nullptr;
	if (!TransactionStartTransform.Equals(EndTransform))
	{
		StoreApplied(MakeShared<TSAVCommands::Private::FTransformCommand>(Actor, TransactionStartTransform, EndTransform, Description));
	}
}

void UTSAVCommandSubsystem::CancelTransformTransaction()
{
	if (bTransformTransactionActive && TransactionActor.IsValid())
	{
		TransactionActor->SetActorTransform(TransactionStartTransform, false, nullptr, ETeleportType::TeleportPhysics);
		OnObjectChanged.Broadcast(TransactionActor.Get());
	}
	bTransformTransactionActive = false;
	TransactionActor = nullptr;
}

bool UTSAVCommandSubsystem::Undo()
{
	CancelTransformTransaction();
	if (UndoStack.IsEmpty())
	{
		return false;
	}
	TSharedPtr<ITSAVCommand> Command = UndoStack.Pop();
	Command->Undo();
	RedoStack.Add(Command);
	NotifyMutation(Command->GetAffectedActor());
	return true;
}

bool UTSAVCommandSubsystem::Redo()
{
	CancelTransformTransaction();
	if (RedoStack.IsEmpty())
	{
		return false;
	}
	TSharedPtr<ITSAVCommand> Command = RedoStack.Pop();
	Command->Execute();
	UndoStack.Add(Command);
	NotifyMutation(Command->GetAffectedActor());
	return true;
}

void UTSAVCommandSubsystem::ClearHistory()
{
	UndoStack.Reset();
	RedoStack.Reset();
	CancelTransformTransaction();
	OnHistoryChanged.Broadcast();
}

FText UTSAVCommandSubsystem::GetUndoDescription() const
{
	return UndoStack.IsEmpty() ? FText::GetEmpty() : UndoStack.Last()->GetDescription();
}

FText UTSAVCommandSubsystem::GetRedoDescription() const
{
	return RedoStack.IsEmpty() ? FText::GetEmpty() : RedoStack.Last()->GetDescription();
}

void UTSAVCommandSubsystem::ExecuteAndStore(const TSharedRef<ITSAVCommand>& Command)
{
	Command->Execute();
	StoreApplied(Command);
}

void UTSAVCommandSubsystem::StoreApplied(const TSharedRef<ITSAVCommand>& Command)
{
	UndoStack.Add(Command);
	RedoStack.Reset();
	NotifyMutation(Command->GetAffectedActor());
}

void UTSAVCommandSubsystem::NotifyMutation(AActor* Actor)
{
	if (UTSAVProjectSubsystem* ProjectSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>() : nullptr)
	{
		ProjectSubsystem->MarkDirty();
	}
	OnObjectChanged.Broadcast(Actor);
	OnHistoryChanged.Broadcast();
}
