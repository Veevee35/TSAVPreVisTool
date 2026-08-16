// Copyright TSAV. All Rights Reserved.

#include "TSAVVideoSwitcher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MediaSource.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StreamMediaSource.h"
#include "TSAVVideoSourceProvider.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVVideoSwitcher)

namespace TSAVVideoSwitcher::Private
{
	FGuid ParseGuid(const FString& Value)
	{
		FGuid Result;
		FGuid::Parse(Value, Result);
		return Result;
	}
}

ATSAVVideoSwitcher::ATSAVVideoSwitcher()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	ConsoleBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Switcher Console"));
	ConsoleBody->SetupAttachment(SceneRoot);
	ConsoleBody->SetMobility(EComponentMobility::Movable);
	ConsoleBody->SetRelativeScale3D(FVector(0.8f, 1.4f, 0.12f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ConsoleBody->SetStaticMesh(CubeMesh.Object);
	}
	Buses = {
		{ TEXT("Program"), FGuid() },
		{ TEXT("Preview"), FGuid() },
		{ TEXT("Aux 1"), FGuid() },
		{ TEXT("Aux 2"), FGuid() },
	};
}

void ATSAVVideoSwitcher::BeginPlay()
{
	Super::BeginPlay();
	NormalizeConfiguration();
	if (bAutoDiscoverSources)
	{
		DiscoverSources();
	}
}

void ATSAVVideoSwitcher::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NormalizeConfiguration();
}

void ATSAVVideoSwitcher::NormalizeConfiguration()
{
	if (!SwitcherId.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		SwitcherId = FGuid::NewGuid();
	}
	for (FTSAVVideoInput& Input : Inputs)
	{
		if (!Input.InputId.IsValid())
		{
			Input.InputId = FGuid::NewGuid();
		}
	}
	const FName RequiredBuses[] = { TEXT("Program"), TEXT("Preview"), TEXT("Aux 1"), TEXT("Aux 2") };
	for (const FName BusName : RequiredBuses)
	{
		if (!FindBus(BusName))
		{
			Buses.Add({ BusName, FGuid() });
		}
	}
}

int32 ATSAVVideoSwitcher::DiscoverSources()
{
	NormalizeConfiguration();
	int32 Added = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			ITSAVVideoSourceProvider* Provider = Cast<ITSAVVideoSourceProvider>(Actor);
			if (!Provider || Actor == this)
			{
				continue;
			}
			const FGuid ProviderId = Provider->GetTSAVVideoSourceId();
			FTSAVVideoInput* Existing = Inputs.FindByPredicate([&](const FTSAVVideoInput& Input)
			{
				return Input.Kind == ETSAVVideoInputKind::CameraFeed && Input.ProviderId == ProviderId;
			});
			if (Existing)
			{
				Existing->ProviderActor = Actor;
				Existing->Label = Provider->GetTSAVVideoSourceName();
				continue;
			}
			FTSAVVideoInput& Input = Inputs.AddDefaulted_GetRef();
			Input.InputId = FGuid::NewGuid();
			Input.Label = Provider->GetTSAVVideoSourceName();
			Input.Kind = ETSAVVideoInputKind::CameraFeed;
			Input.ProviderId = ProviderId;
			Input.ProviderActor = Actor;
			++Added;
		}
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByClass(UMediaSource::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		UMediaSource* Source = Cast<UMediaSource>(Asset.GetAsset());
		if (!Source || Inputs.ContainsByPredicate([Source](const FTSAVVideoInput& Input) { return Input.MediaSource == Source; }))
		{
			continue;
		}
		FTSAVVideoInput& Input = Inputs.AddDefaulted_GetRef();
		Input.InputId = FGuid::NewGuid();
		Input.Label = FText::FromName(Asset.AssetName);
		Input.Kind = ETSAVVideoInputKind::MediaAsset;
		Input.MediaSource = Source;
		++Added;
	}

	if (Added > 0)
	{
		if (FTSAVVideoBus* Program = FindBus(TEXT("Program")); Program && !Program->SelectedInputId.IsValid() && !Inputs.IsEmpty())
		{
			Program->SelectedInputId = Inputs[0].InputId;
		}
		if (FTSAVVideoBus* Preview = FindBus(TEXT("Preview")); Preview && !Preview->SelectedInputId.IsValid() && Inputs.Num() > 1)
		{
			Preview->SelectedInputId = Inputs[1].InputId;
		}
		OnInputsChanged.Broadcast();
		OnBusChanged.Broadcast(TEXT("Program"));
		OnBusChanged.Broadcast(TEXT("Preview"));
	}
	return Added;
}

FGuid ATSAVVideoSwitcher::AddStreamInput(const FText& Label, const FString& StreamUrl)
{
	if (StreamUrl.IsEmpty())
	{
		return FGuid();
	}
	FTSAVVideoInput& Input = Inputs.AddDefaulted_GetRef();
	Input.InputId = FGuid::NewGuid();
	Input.Label = Label.IsEmpty() ? FText::FromString(StreamUrl) : Label;
	Input.Kind = ETSAVVideoInputKind::StreamUrl;
	Input.StreamUrl = StreamUrl;
	ResolveMediaSource(Input);
	OnInputsChanged.Broadcast();
	return Input.InputId;
}

bool ATSAVVideoSwitcher::RemoveInput(const FGuid InputId)
{
	const int32 Removed = Inputs.RemoveAll([InputId](const FTSAVVideoInput& Input) { return Input.InputId == InputId; });
	if (Removed == 0)
	{
		return false;
	}
	RuntimeStreamSources.Remove(InputId);
	for (FTSAVVideoBus& Bus : Buses)
	{
		if (Bus.SelectedInputId == InputId)
		{
			Bus.SelectedInputId.Invalidate();
			OnBusChanged.Broadcast(Bus.Name);
		}
	}
	OnInputsChanged.Broadcast();
	return true;
}

bool ATSAVVideoSwitcher::SetBusInput(const FName BusName, const FGuid InputId)
{
	FTSAVVideoBus* Bus = FindBus(BusName);
	if (!Bus || !FindInput(InputId) || Bus->SelectedInputId == InputId)
	{
		return false;
	}
	Bus->SelectedInputId = InputId;
	OnBusChanged.Broadcast(BusName);
	return true;
}

void ATSAVVideoSwitcher::Cut()
{
	FTSAVVideoBus* Program = FindBus(TEXT("Program"));
	FTSAVVideoBus* Preview = FindBus(TEXT("Preview"));
	if (!Program || !Preview)
	{
		return;
	}
	Swap(Program->SelectedInputId, Preview->SelectedInputId);
	OnBusChanged.Broadcast(Program->Name);
	OnBusChanged.Broadcast(Preview->Name);
}

void ATSAVVideoSwitcher::AutoTransition()
{
	// The routing model is transition-ready; Phase 1 performs a clean cut.
	Cut();
}

FGuid ATSAVVideoSwitcher::GetBusInputId(const FName BusName) const
{
	const FTSAVVideoBus* Bus = FindBus(BusName);
	return Bus ? Bus->SelectedInputId : FGuid();
}

FText ATSAVVideoSwitcher::GetBusInputLabel(const FName BusName) const
{
	const FTSAVVideoInput* Input = FindInput(GetBusInputId(BusName));
	return Input ? Input->Label : NSLOCTEXT("TSAVVideo", "NoVideoInput", "None");
}

UMediaSource* ATSAVVideoSwitcher::GetOutputMediaSource(const FName BusName)
{
	FTSAVVideoInput* Input = FindInput(GetBusInputId(BusName));
	return Input ? ResolveMediaSource(*Input) : nullptr;
}

UTexture* ATSAVVideoSwitcher::GetOutputTexture(const FName BusName)
{
	FTSAVVideoInput* Input = FindInput(GetBusInputId(BusName));
	if (!Input || Input->Kind != ETSAVVideoInputKind::CameraFeed)
	{
		return nullptr;
	}
	AActor* ProviderActor = ResolveProvider(*Input);
	ITSAVVideoSourceProvider* Provider = Cast<ITSAVVideoSourceProvider>(ProviderActor);
	return Provider ? Provider->GetTSAVVideoTexture() : nullptr;
}

FTSAVVideoInput* ATSAVVideoSwitcher::FindInput(const FGuid InputId)
{
	return Inputs.FindByPredicate([InputId](const FTSAVVideoInput& Input) { return Input.InputId == InputId; });
}

const FTSAVVideoInput* ATSAVVideoSwitcher::FindInput(const FGuid InputId) const
{
	return Inputs.FindByPredicate([InputId](const FTSAVVideoInput& Input) { return Input.InputId == InputId; });
}

FTSAVVideoBus* ATSAVVideoSwitcher::FindBus(const FName BusName)
{
	return Buses.FindByPredicate([BusName](const FTSAVVideoBus& Bus) { return Bus.Name.IsEqual(BusName); });
}

const FTSAVVideoBus* ATSAVVideoSwitcher::FindBus(const FName BusName) const
{
	return Buses.FindByPredicate([BusName](const FTSAVVideoBus& Bus) { return Bus.Name.IsEqual(BusName); });
}

AActor* ATSAVVideoSwitcher::ResolveProvider(FTSAVVideoInput& Input)
{
	if (IsValid(Input.ProviderActor))
	{
		return Input.ProviderActor;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (ITSAVVideoSourceProvider* Provider = Cast<ITSAVVideoSourceProvider>(*It))
			{
				if (Provider->GetTSAVVideoSourceId() == Input.ProviderId)
				{
					Input.ProviderActor = *It;
					return *It;
				}
			}
		}
	}
	return nullptr;
}

UMediaSource* ATSAVVideoSwitcher::ResolveMediaSource(FTSAVVideoInput& Input)
{
	if (Input.Kind == ETSAVVideoInputKind::MediaAsset)
	{
		return Input.MediaSource;
	}
	if (Input.Kind != ETSAVVideoInputKind::StreamUrl || Input.StreamUrl.IsEmpty())
	{
		return nullptr;
	}
	if (TObjectPtr<UStreamMediaSource>* Existing = RuntimeStreamSources.Find(Input.InputId))
	{
		(*Existing)->StreamUrl = Input.StreamUrl;
		return *Existing;
	}
	UStreamMediaSource* Source = NewObject<UStreamMediaSource>(this);
	Source->StreamUrl = Input.StreamUrl;
	RuntimeStreamSources.Add(Input.InputId, Source);
	return Source;
}

FString ATSAVVideoSwitcher::CaptureTSAVState() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("switcherId"), SwitcherId.ToString(EGuidFormats::DigitsWithHyphens));
	Root->SetBoolField(TEXT("autoDiscover"), bAutoDiscoverSources);
	TArray<TSharedPtr<FJsonValue>> InputValues;
	for (const FTSAVVideoInput& Input : Inputs)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("id"), Input.InputId.ToString(EGuidFormats::DigitsWithHyphens));
		Json->SetStringField(TEXT("label"), Input.Label.ToString());
		Json->SetNumberField(TEXT("kind"), static_cast<uint8>(Input.Kind));
		Json->SetStringField(TEXT("mediaSource"), Input.MediaSource ? Input.MediaSource->GetPathName() : FString());
		Json->SetStringField(TEXT("streamUrl"), Input.StreamUrl);
		Json->SetStringField(TEXT("providerId"), Input.ProviderId.ToString(EGuidFormats::DigitsWithHyphens));
		InputValues.Add(MakeShared<FJsonValueObject>(Json));
	}
	Root->SetArrayField(TEXT("inputs"), InputValues);
	TArray<TSharedPtr<FJsonValue>> BusValues;
	for (const FTSAVVideoBus& Bus : Buses)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Bus.Name.ToString());
		Json->SetStringField(TEXT("inputId"), Bus.SelectedInputId.ToString(EGuidFormats::DigitsWithHyphens));
		BusValues.Add(MakeShared<FJsonValueObject>(Json));
	}
	Root->SetArrayField(TEXT("buses"), BusValues);
	FString Result;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

bool ATSAVVideoSwitcher::RestoreTSAVState(const FString& State)
{
	TSharedPtr<FJsonObject> Root;
	if (State.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(State), Root) || !Root)
	{
		return false;
	}
	SwitcherId = TSAVVideoSwitcher::Private::ParseGuid(Root->GetStringField(TEXT("switcherId")));
	bAutoDiscoverSources = Root->GetBoolField(TEXT("autoDiscover"));
	Inputs.Reset();
	RuntimeStreamSources.Reset();
	const TArray<TSharedPtr<FJsonValue>>* InputValues = nullptr;
	if (Root->TryGetArrayField(TEXT("inputs"), InputValues) && InputValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *InputValues)
		{
			const TSharedPtr<FJsonObject> Json = Value ? Value->AsObject() : nullptr;
			if (!Json) { continue; }
			FTSAVVideoInput& Input = Inputs.AddDefaulted_GetRef();
			Input.InputId = TSAVVideoSwitcher::Private::ParseGuid(Json->GetStringField(TEXT("id")));
			Input.Label = FText::FromString(Json->GetStringField(TEXT("label")));
			Input.Kind = static_cast<ETSAVVideoInputKind>(Json->GetIntegerField(TEXT("kind")));
			const FString SourcePath = Json->GetStringField(TEXT("mediaSource"));
			Input.MediaSource = SourcePath.IsEmpty() ? nullptr : LoadObject<UMediaSource>(nullptr, *SourcePath);
			Input.StreamUrl = Json->GetStringField(TEXT("streamUrl"));
			Input.ProviderId = TSAVVideoSwitcher::Private::ParseGuid(Json->GetStringField(TEXT("providerId")));
		}
	}
	Buses.Reset();
	const TArray<TSharedPtr<FJsonValue>>* BusValues = nullptr;
	if (Root->TryGetArrayField(TEXT("buses"), BusValues) && BusValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *BusValues)
		{
			const TSharedPtr<FJsonObject> Json = Value ? Value->AsObject() : nullptr;
			if (!Json) { continue; }
			FTSAVVideoBus& Bus = Buses.AddDefaulted_GetRef();
			Bus.Name = FName(*Json->GetStringField(TEXT("name")));
			Bus.SelectedInputId = TSAVVideoSwitcher::Private::ParseGuid(Json->GetStringField(TEXT("inputId")));
		}
	}
	NormalizeConfiguration();
	OnInputsChanged.Broadcast();
	for (const FTSAVVideoBus& Bus : Buses)
	{
		OnBusChanged.Broadcast(Bus.Name);
	}
	return true;
}
