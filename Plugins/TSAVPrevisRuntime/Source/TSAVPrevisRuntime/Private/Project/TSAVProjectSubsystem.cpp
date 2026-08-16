// Copyright TSAV. All Rights Reserved.

#include "Project/TSAVProjectSubsystem.h"

#include "TSAVPrevisRuntime.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVProjectSubsystem)

namespace TSAVProject::Private
{
	constexpr int32 CurrentFormatVersion = 1;

	TSharedRef<FJsonObject> VectorToJson(const FVector& Vector)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("x"), Vector.X);
		Json->SetNumberField(TEXT("y"), Vector.Y);
		Json->SetNumberField(TEXT("z"), Vector.Z);
		return Json;
	}

	FVector JsonToVector(const TSharedPtr<FJsonObject>& Json, const FVector& DefaultValue)
	{
		if (!Json)
		{
			return DefaultValue;
		}
		return FVector(
			Json->GetNumberField(TEXT("x")),
			Json->GetNumberField(TEXT("y")),
			Json->GetNumberField(TEXT("z")));
	}

	TSharedRef<FJsonObject> TransformToJson(const FTransform& Transform)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("location"), VectorToJson(Transform.GetLocation()));
		const FRotator Rotation = Transform.Rotator();
		TSharedRef<FJsonObject> RotationJson = MakeShared<FJsonObject>();
		RotationJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
		RotationJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
		RotationJson->SetNumberField(TEXT("roll"), Rotation.Roll);
		Json->SetObjectField(TEXT("rotation"), RotationJson);
		Json->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
		return Json;
	}

	FTransform JsonToTransform(const TSharedPtr<FJsonObject>& Json)
	{
		if (!Json)
		{
			return FTransform::Identity;
		}
		const TSharedPtr<FJsonObject>* LocationJson = nullptr;
		const TSharedPtr<FJsonObject>* RotationJson = nullptr;
		const TSharedPtr<FJsonObject>* ScaleJson = nullptr;
		Json->TryGetObjectField(TEXT("location"), LocationJson);
		Json->TryGetObjectField(TEXT("rotation"), RotationJson);
		Json->TryGetObjectField(TEXT("scale"), ScaleJson);
		FRotator Rotation = FRotator::ZeroRotator;
		if (RotationJson && *RotationJson)
		{
			Rotation.Pitch = (*RotationJson)->GetNumberField(TEXT("pitch"));
			Rotation.Yaw = (*RotationJson)->GetNumberField(TEXT("yaw"));
			Rotation.Roll = (*RotationJson)->GetNumberField(TEXT("roll"));
		}
		return FTransform(
			Rotation,
			JsonToVector(LocationJson ? *LocationJson : nullptr, FVector::ZeroVector),
			JsonToVector(ScaleJson ? *ScaleJson : nullptr, FVector::OneVector));
	}

	FString NormalizeProjectPath(const FString& RequestedPath, const FString& FallbackPath)
	{
		FString Result = RequestedPath.IsEmpty() ? FallbackPath : RequestedPath;
		if (FPaths::IsRelative(Result))
		{
			Result = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TSAV Projects"), Result);
		}
		if (!Result.EndsWith(TEXT(".tsav"), ESearchCase::IgnoreCase))
		{
			Result += TEXT(".tsav");
		}
		Result = FPaths::ConvertRelativePathToFull(Result);
		FPaths::NormalizeFilename(Result);
		return Result;
	}
}

void UTSAVProjectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NewProject();
}

void UTSAVProjectSubsystem::NewProject(const FString& InProjectName)
{
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> ActorsToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->FindComponentByClass<UTSAVSceneObjectComponent>())
			{
				ActorsToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ActorsToDestroy)
		{
			Actor->Destroy();
		}
	}

	ProjectId = FGuid::NewGuid();
	ProjectName = InProjectName.IsEmpty() ? TEXT("Untitled Show") : InProjectName;
	CurrentProjectPath.Reset();
	bDirty = false;
	if (UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr)
	{
		Commands->ClearHistory();
	}
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

bool UTSAVProjectSubsystem::SaveProject(const FString& FilePath)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString TargetPath = TSAVProject::Private::NormalizeProjectPath(FilePath, CurrentProjectPath.IsEmpty() ? GetDefaultProjectPath() : CurrentProjectPath);
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("formatVersion"), TSAVProject::Private::CurrentFormatVersion);
	TSharedRef<FJsonObject> ProjectJson = MakeShared<FJsonObject>();
	ProjectJson->SetStringField(TEXT("id"), ProjectId.ToString(EGuidFormats::DigitsWithHyphens));
	ProjectJson->SetStringField(TEXT("name"), ProjectName);
	Root->SetObjectField(TEXT("project"), ProjectJson);

	TArray<TSharedPtr<FJsonValue>> Objects;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UTSAVSceneObjectComponent* SceneObject = It->FindComponentByClass<UTSAVSceneObjectComponent>();
		if (!SceneObject)
		{
			continue;
		}

		TSharedRef<FJsonObject> ObjectJson = MakeShared<FJsonObject>();
		ObjectJson->SetStringField(TEXT("id"), SceneObject->ObjectId.ToString(EGuidFormats::DigitsWithHyphens));
		ObjectJson->SetStringField(TEXT("class"), It->GetClass()->GetPathName());
		ObjectJson->SetNumberField(TEXT("type"), static_cast<uint8>(SceneObject->ObjectType));
		ObjectJson->SetObjectField(TEXT("transform"), TSAVProject::Private::TransformToJson(It->GetActorTransform()));
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetStringField(TEXT("displayName"), SceneObject->DisplayName.ToString());
		Properties->SetBoolField(TEXT("locked"), SceneObject->bLocked);
		Properties->SetBoolField(TEXT("visible"), SceneObject->bVisible);
		ObjectJson->SetObjectField(TEXT("properties"), Properties);
		Objects.Add(MakeShared<FJsonValueObject>(ObjectJson));
	}
	Root->SetArrayField(TEXT("objects"), Objects);
	Root->SetArrayField(TEXT("videoRoutes"), TArray<TSharedPtr<FJsonValue>>());
	Root->SetObjectField(TEXT("settings"), MakeShared<FJsonObject>());

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		UE_LOG(LogTSAVPrevisRuntime, Error, TEXT("Could not serialize TSAV project JSON for %s."), *TargetPath);
		return false;
	}
	const FString TargetDirectory = FPaths::GetPath(TargetPath);
	if (!IFileManager::Get().MakeDirectory(*TargetDirectory, true) && !IFileManager::Get().DirectoryExists(*TargetDirectory))
	{
		UE_LOG(LogTSAVPrevisRuntime, Error, TEXT("Could not create TSAV project directory %s."), *TargetDirectory);
		return false;
	}
	if (!FFileHelper::SaveStringToFile(Output, *TargetPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTSAVPrevisRuntime, Error, TEXT("Could not write TSAV project file %s (system error %d)."), *TargetPath, FPlatformMisc::GetLastError());
		return false;
	}

	CurrentProjectPath = TargetPath;
	bDirty = false;
	OnProjectChanged.Broadcast();
	return true;
}

bool UTSAVProjectSubsystem::LoadProject(const FString& FilePath)
{
	UWorld* World = GetWorld();
	const FString TargetPath = TSAVProject::Private::NormalizeProjectPath(FilePath, CurrentProjectPath.IsEmpty() ? GetDefaultProjectPath() : CurrentProjectPath);
	FString Input;
	if (!World || !FFileHelper::LoadFileToString(Input, *TargetPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Input);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root)
	{
		return false;
	}
	const int32 FormatVersion = Root->GetIntegerField(TEXT("formatVersion"));
	if (FormatVersion < 1 || FormatVersion > TSAVProject::Private::CurrentFormatVersion)
	{
		return false;
	}

	TArray<AActor*> ActorsToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UTSAVSceneObjectComponent>())
		{
			ActorsToDestroy.Add(*It);
		}
	}
	for (AActor* Actor : ActorsToDestroy)
	{
		Actor->Destroy();
	}

	const TSharedPtr<FJsonObject>* ProjectJson = nullptr;
	if (Root->TryGetObjectField(TEXT("project"), ProjectJson) && ProjectJson && *ProjectJson)
	{
		FGuid::Parse((*ProjectJson)->GetStringField(TEXT("id")), ProjectId);
		ProjectName = (*ProjectJson)->GetStringField(TEXT("name"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
	if (Root->TryGetArrayField(TEXT("objects"), Objects) && Objects)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Objects)
		{
			const TSharedPtr<FJsonObject> ObjectJson = Value ? Value->AsObject() : nullptr;
			if (!ObjectJson)
			{
				continue;
			}
			UClass* ActorClass = LoadObject<UClass>(nullptr, *ObjectJson->GetStringField(TEXT("class")));
			if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
			{
				continue;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const TSharedPtr<FJsonObject>* TransformJson = nullptr;
			ObjectJson->TryGetObjectField(TEXT("transform"), TransformJson);
			AActor* Actor = World->SpawnActor<AActor>(ActorClass, TSAVProject::Private::JsonToTransform(TransformJson ? *TransformJson : nullptr), SpawnParameters);
			UTSAVSceneObjectComponent* SceneObject = UTSAVSceneObjectComponent::EnsureForActor(Actor);
			if (!SceneObject)
			{
				if (Actor) { Actor->Destroy(); }
				continue;
			}
			FGuid::Parse(ObjectJson->GetStringField(TEXT("id")), SceneObject->ObjectId);
			SceneObject->ObjectType = static_cast<ETSAVObjectType>(ObjectJson->GetIntegerField(TEXT("type")));
			const TSharedPtr<FJsonObject>* Properties = nullptr;
			if (ObjectJson->TryGetObjectField(TEXT("properties"), Properties) && Properties && *Properties)
			{
				SceneObject->DisplayName = FText::FromString((*Properties)->GetStringField(TEXT("displayName")));
				SceneObject->bLocked = (*Properties)->GetBoolField(TEXT("locked"));
				SceneObject->SetObjectVisible((*Properties)->GetBoolField(TEXT("visible")));
			}
		}
	}

	CurrentProjectPath = TargetPath;
	bDirty = false;
	if (UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr)
	{
		Commands->ClearHistory();
	}
	OnProjectChanged.Broadcast();
	return true;
}

FString UTSAVProjectSubsystem::GetDefaultProjectPath() const
{
	const FString SafeProjectName = FPaths::MakeValidFileName(ProjectName.IsEmpty() ? TEXT("Untitled Show") : ProjectName);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TSAV Projects"), SafeProjectName + TEXT(".tsav"));
}
