// Copyright TSAV. All Rights Reserved.

#include "TSAVVideoSwitcher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IMediaIOCoreDeviceProvider.h"
#include "IMediaIOCoreModule.h"
#include "MediaSource.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StreamMediaSource.h"
#include "TSAVMediaSurfaceActor.h"
#include "TSAVVideoSourceProvider.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVVideoSwitcher)

namespace TSAVVideoSwitcher::Private
{
	const FName NDIProviderName(TEXT("NDI"));

	FGuid ParseGuid(const FString& Value)
	{
		FGuid Result;
		FGuid::Parse(Value, Result);
		return Result;
	}

	FString NormalizeStreamUrl(const FString& Value)
	{
		FString Result = Value.TrimStartAndEnd();
		if (!Result.IsEmpty() && !Result.Contains(TEXT("://")))
		{
			Result = FString::Printf(TEXT("ndi://%s"), *Result);
		}
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
	if (PruneUnavailableProviderInputs() > 0)
	{
		NormalizeBusSelections();
		OnInputsChanged.Broadcast();
	}
	RefreshOutputs();
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
	bool bInputsChanged = false;
	TSet<FGuid> VisibleProviderIds;
	const bool bCanValidateProviders = GetWorld() != nullptr;
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
			if (!ProviderId.IsValid())
			{
				continue;
			}
			VisibleProviderIds.Add(ProviderId);
			FTSAVVideoInput* Existing = Inputs.FindByPredicate([&](const FTSAVVideoInput& Input)
			{
				return Input.Kind == ETSAVVideoInputKind::CameraFeed && Input.ProviderId == ProviderId;
			});
			if (Existing)
			{
				bInputsChanged |= !Existing->Label.EqualTo(Provider->GetTSAVVideoSourceName());
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
			bInputsChanged = true;
		}
	}

	// Camera inputs refer to live provider actors. Keeping them after an actor is
	// deleted leaves a bus pointing at an input that can never produce a texture.
	const int32 RemovedProviders = bCanValidateProviders
		? Inputs.RemoveAll([&VisibleProviderIds](const FTSAVVideoInput& Input)
		{
			return Input.Kind == ETSAVVideoInputKind::CameraFeed && !VisibleProviderIds.Contains(Input.ProviderId);
		})
		: 0;
	bInputsChanged |= RemovedProviders > 0;

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
		bInputsChanged = true;
	}

	// NDIMedia exposes current network senders through MediaIOCore. Their full
	// NDI names are durable endpoints, so storing ndi://<name> lets saved .tsav
	// projects reconnect automatically when a sender returns.
	FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("NDIMedia"));
	if (IMediaIOCoreModule::IsAvailable())
	{
		if (IMediaIOCoreDeviceProvider* NDIProvider = IMediaIOCoreModule::Get().GetDeviceProvider(TSAVVideoSwitcher::Private::NDIProviderName))
		{
			TArray<FMediaIODevice> NDIDevices = NDIProvider->GetDevices();
			NDIDevices.Sort([](const FMediaIODevice& Left, const FMediaIODevice& Right)
			{
				return Left.DeviceName.LexicalLess(Right.DeviceName);
			});
			for (const FMediaIODevice& Device : NDIDevices)
			{
				const FString SourceName = Device.DeviceName.ToString().TrimStartAndEnd();
				const FString SourceUrl = TSAVVideoSwitcher::Private::NormalizeStreamUrl(SourceName);
				if (SourceName.IsEmpty() || Inputs.ContainsByPredicate([&SourceUrl](const FTSAVVideoInput& Input)
				{
					return Input.Kind == ETSAVVideoInputKind::StreamUrl && Input.StreamUrl.Equals(SourceUrl, ESearchCase::IgnoreCase);
				}))
				{
					continue;
				}

				FTSAVVideoInput& Input = Inputs.AddDefaulted_GetRef();
				Input.InputId = FGuid::NewGuid();
				Input.Label = FText::Format(NSLOCTEXT("TSAVVideo", "VisibleNDIInput", "NDI  |  {0}"), FText::FromString(SourceName));
				Input.Kind = ETSAVVideoInputKind::StreamUrl;
				Input.StreamUrl = SourceUrl;
				ResolveMediaSource(Input);
				++Added;
				bInputsChanged = true;
			}
		}
	}

	NormalizeBusSelections();
	if (bInputsChanged)
	{
		OnInputsChanged.Broadcast();
	}
	// Refresh even when the list did not change. This makes the editor's Refresh
	// Inputs button repair wall bindings after a level reload.
	RefreshOutputs();
	return Added;
}

FGuid ATSAVVideoSwitcher::AddStreamInput(const FText& Label, const FString& StreamUrl)
{
	const FString ResolvedUrl = TSAVVideoSwitcher::Private::NormalizeStreamUrl(StreamUrl);
	if (ResolvedUrl.IsEmpty())
	{
		return FGuid();
	}
	FTSAVVideoInput& Input = Inputs.AddDefaulted_GetRef();
	Input.InputId = FGuid::NewGuid();
	Input.Label = Label.IsEmpty() ? FText::FromString(ResolvedUrl) : Label;
	Input.Kind = ETSAVVideoInputKind::StreamUrl;
	Input.StreamUrl = ResolvedUrl;
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
	NormalizeBusSelections();
	OnInputsChanged.Broadcast();
	RefreshOutputs();
	return true;
}

int32 ATSAVVideoSwitcher::RemoveProviderInputs(const FGuid ProviderId)
{
	if (!ProviderId.IsValid())
	{
		return 0;
	}
	TArray<FGuid> RemovedInputIds;
	for (const FTSAVVideoInput& Input : Inputs)
	{
		if (Input.Kind == ETSAVVideoInputKind::CameraFeed && Input.ProviderId == ProviderId)
		{
			RemovedInputIds.Add(Input.InputId);
		}
	}
	if (RemovedInputIds.IsEmpty())
	{
		return 0;
	}
	const int32 Removed = Inputs.RemoveAll([ProviderId](const FTSAVVideoInput& Input)
	{
		return Input.Kind == ETSAVVideoInputKind::CameraFeed && Input.ProviderId == ProviderId;
	});
	for (const FGuid InputId : RemovedInputIds)
	{
		RuntimeStreamSources.Remove(InputId);
	}
	NormalizeBusSelections();
	OnInputsChanged.Broadcast();
	RefreshOutputs();
	return Removed;
}

bool ATSAVVideoSwitcher::SetBusInput(const FName BusName, const FGuid InputId)
{
	FTSAVVideoBus* Bus = FindBus(BusName);
	FTSAVVideoInput* Input = FindInput(InputId);
	if (!Bus || !Input)
	{
		return false;
	}
	if (Input->Kind == ETSAVVideoInputKind::CameraFeed && !ResolveProvider(*Input))
	{
		// Do not allow an old camera row to become a dead crosspoint. Removing it
		// also selects a valid Program fallback and republishes the wall output.
		RemoveInput(InputId);
		return false;
	}
	Bus->SelectedInputId = InputId;
	// Re-broadcasting an already selected input is intentional: it repairs a
	// stale material/player binding without forcing the user through another bus.
	BroadcastBusChanged(BusName);
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
	BroadcastBusChanged(Program->Name);
	BroadcastBusChanged(Preview->Name);
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

void ATSAVVideoSwitcher::RefreshOutputs()
{
	for (const FTSAVVideoBus& Bus : Buses)
	{
		BroadcastBusChanged(Bus.Name);
	}
}

void ATSAVVideoSwitcher::RefreshOutputsForProvider(const FGuid ProviderId)
{
	if (!ProviderId.IsValid())
	{
		return;
	}
	for (const FTSAVVideoBus& Bus : Buses)
	{
		const FTSAVVideoInput* Input = FindInput(Bus.SelectedInputId);
		if (Input && Input->Kind == ETSAVVideoInputKind::CameraFeed && Input->ProviderId == ProviderId)
		{
			BroadcastBusChanged(Bus.Name);
		}
	}
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

void ATSAVVideoSwitcher::BroadcastBusChanged(const FName BusName)
{
	if (UWorld* World = GetWorld())
	{
		// Editor construction and level loading do not guarantee actor order. Make
		// sure every routed wall is listening before publishing the new crosspoint.
		for (TActorIterator<ATSAVMediaSurfaceActor> It(World); It; ++It)
		{
			It->EnsureVideoRouteBinding(this, BusName);
		}
	}
	OnBusChanged.Broadcast(BusName);
}

void ATSAVVideoSwitcher::NormalizeBusSelections()
{
	for (FTSAVVideoBus& Bus : Buses)
	{
		if (Bus.SelectedInputId.IsValid() && !FindInput(Bus.SelectedInputId))
		{
			Bus.SelectedInputId.Invalidate();
		}
	}
	if (FTSAVVideoBus* Program = FindBus(TEXT("Program")); Program && !Program->SelectedInputId.IsValid() && !Inputs.IsEmpty())
	{
		Program->SelectedInputId = Inputs[0].InputId;
	}
	if (FTSAVVideoBus* Preview = FindBus(TEXT("Preview")); Preview && !Preview->SelectedInputId.IsValid() && Inputs.Num() > 1)
	{
		Preview->SelectedInputId = Inputs[1].InputId;
	}
}

int32 ATSAVVideoSwitcher::PruneUnavailableProviderInputs()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	TSet<FGuid> VisibleProviderIds;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		ITSAVVideoSourceProvider* Provider = Cast<ITSAVVideoSourceProvider>(Actor);
		if (!Provider || Actor == this)
		{
			continue;
		}
		const FGuid ProviderId = Provider->GetTSAVVideoSourceId();
		if (!ProviderId.IsValid())
		{
			continue;
		}
		VisibleProviderIds.Add(ProviderId);
		for (FTSAVVideoInput& Input : Inputs)
		{
			if (Input.Kind == ETSAVVideoInputKind::CameraFeed && Input.ProviderId == ProviderId)
			{
				Input.ProviderActor = Actor;
				Input.Label = Provider->GetTSAVVideoSourceName();
			}
		}
	}
	return Inputs.RemoveAll([&VisibleProviderIds](const FTSAVVideoInput& Input)
	{
		return Input.Kind == ETSAVVideoInputKind::CameraFeed && !VisibleProviderIds.Contains(Input.ProviderId);
	});
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
			Input.StreamUrl = Input.Kind == ETSAVVideoInputKind::StreamUrl
				? TSAVVideoSwitcher::Private::NormalizeStreamUrl(Json->GetStringField(TEXT("streamUrl")))
				: Json->GetStringField(TEXT("streamUrl"));
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
	NormalizeBusSelections();
	OnInputsChanged.Broadcast();
	RefreshOutputs();
	return true;
}
