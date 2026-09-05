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
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TSAVMediaSurfaceActor.h"
#include "TSAVStateSerializable.h"

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

	bool ValidTransformJson(const TSharedPtr<FJsonObject>& Json)
	{
		if (!Json) return true;
		for (const TCHAR* Group : {TEXT("location"),TEXT("rotation"),TEXT("scale")}) {
			const TSharedPtr<FJsonObject>* Object=nullptr;
			if (!Json->TryGetObjectField(Group,Object)) return false;
			const TArray<FString> Keys=FString(Group)==TEXT("rotation")?TArray<FString>{TEXT("pitch"),TEXT("yaw"),TEXT("roll")}:TArray<FString>{TEXT("x"),TEXT("y"),TEXT("z")};
			for (const auto& Key : Keys) { double Value=0; if (!(*Object)->TryGetNumberField(Key,Value) || !FMath::IsFinite(Value)) return false; }
		}
		return true;
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
		if (const ITSAVStateSerializable* Serializable = Cast<ITSAVStateSerializable>(*It))
		{
			ObjectJson->SetStringField(TEXT("customState"), Serializable->CaptureTSAVState());
		}
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
	const FString TempPath = TargetPath + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Output, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !IFileManager::Get().Move(*TargetPath, *TempPath, true, true))
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
	int32 FormatVersion = 0;
	if (!Root->TryGetNumberField(TEXT("formatVersion"), FormatVersion) || FormatVersion < 1 || FormatVersion > TSAVProject::Private::CurrentFormatVersion)
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
	const TSharedPtr<FJsonObject>* ProjectJson = nullptr;
	FGuid LoadedProjectId; FString LoadedIdText, LoadedProjectName;
	if (!Root->TryGetObjectField(TEXT("project"), ProjectJson)
		|| !(*ProjectJson)->TryGetStringField(TEXT("id"), LoadedIdText) || !FGuid::Parse(LoadedIdText, LoadedProjectId) || !LoadedProjectId.IsValid()
		|| !(*ProjectJson)->TryGetStringField(TEXT("name"), LoadedProjectName)) return false;
	const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
	if (!Root->TryGetArrayField(TEXT("objects"), Objects)) return false;
	// Stage and validate every replacement before destroying the current scene.
	TArray<AActor*> StagedActors;
	TSet<FGuid> SeenIds;
	bool bCommitted = false;
	const bool bWasDirty = bDirty;
	ON_SCOPE_EXIT { if (!bCommitted) { for (AActor* Actor : StagedActors) Actor->Destroy(); bDirty = bWasDirty; } };
	if (Root->TryGetArrayField(TEXT("objects"), Objects) && Objects)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Objects)
		{
			const TSharedPtr<FJsonObject> ObjectJson = Value && Value->Type==EJson::Object ? Value->AsObject() : nullptr;
			if (!ObjectJson)
			{
				return false;
			}
			FString ClassPath, ObjectIdText; FGuid ObjectId;
			if (!ObjectJson->TryGetStringField(TEXT("class"), ClassPath) || !ObjectJson->TryGetStringField(TEXT("id"), ObjectIdText)
				|| !FGuid::Parse(ObjectIdText, ObjectId) || !ObjectId.IsValid() || SeenIds.Contains(ObjectId)) return false;
			SeenIds.Add(ObjectId);
			UClass* ActorClass = LoadObject<UClass>(nullptr, *ClassPath);
			if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
			{
				return false;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const TSharedPtr<FJsonObject>* TransformJson = nullptr;
			ObjectJson->TryGetObjectField(TEXT("transform"), TransformJson);
			if (!TSAVProject::Private::ValidTransformJson(TransformJson?*TransformJson:nullptr)) return false;
			int32 ObjectType=0;
			if (!ObjectJson->TryGetNumberField(TEXT("type"),ObjectType) || !StaticEnum<ETSAVObjectType>()->IsValidEnumValue(ObjectType)) return false;
			const TSharedPtr<FJsonObject>* Properties = nullptr; FString DisplayName; bool bLocked=false,bVisible=true;
			if (ObjectJson->TryGetObjectField(TEXT("properties"),Properties)
				&& (!(*Properties)->TryGetStringField(TEXT("displayName"),DisplayName) || !(*Properties)->TryGetBoolField(TEXT("locked"),bLocked) || !(*Properties)->TryGetBoolField(TEXT("visible"),bVisible))) return false;
			AActor* Actor = World->SpawnActor<AActor>(ActorClass, TSAVProject::Private::JsonToTransform(TransformJson ? *TransformJson : nullptr), SpawnParameters);
			if (Actor) StagedActors.Add(Actor);
			UTSAVSceneObjectComponent* SceneObject = UTSAVSceneObjectComponent::EnsureForActor(Actor);
			if (!SceneObject)
			{
				return false;
			}
			SceneObject->ObjectId = ObjectId;
			SceneObject->ObjectType = static_cast<ETSAVObjectType>(ObjectType);
			if (Properties && *Properties)
			{
				SceneObject->DisplayName = FText::FromString(DisplayName);
				SceneObject->bLocked = bLocked;
				SceneObject->SetObjectVisible(bVisible);
			}
			FString CustomState;
			if (ObjectJson->TryGetStringField(TEXT("customState"), CustomState) && !CustomState.IsEmpty())
			{
				if (ITSAVStateSerializable* Serializable = Cast<ITSAVStateSerializable>(Actor))
				{
					if (!Serializable->RestoreTSAVState(CustomState)) return false;
				}
				else return false;
			}
		}
	}

	for (AActor* Actor : ActorsToDestroy) Actor->Destroy();
	bCommitted = true;
	ProjectId = LoadedProjectId;
	ProjectName = LoadedProjectName;
	// Resolve cross-actor video routes after every switcher and camera has been restored.
	for (TActorIterator<ATSAVMediaSurfaceActor> It(World); It; ++It)
	{
		It->RefreshMedia();
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
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TSAV Projects"), SafeProjectName + TEXT(".tsav")));
}
