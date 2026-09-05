// Copyright TSAV. All Rights Reserved.

#include "TSAVDMXFixture.h"

#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/DMXComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "IO/DMXPortManager.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVDMXFixture)

namespace
{
	bool IsValidSavedMode(const FDMXFixtureMode& Mode)
	{
		if (Mode.ChannelSpan<1 || Mode.ChannelSpan>512) return false;
		TSet<int32> Channels; TSet<FName> Attributes;
		for (const auto& Function : Mode.Functions) {
			if (Function.Attribute.Name.IsNone() || Attributes.Contains(Function.Attribute.Name) || static_cast<uint8>(Function.DataType)>3
				|| Function.Channel<1 || Function.Channel>512 || Function.GetLastChannel()>Mode.ChannelSpan) return false;
			Attributes.Add(Function.Attribute.Name);
			for (int32 C=Function.Channel; C<=Function.GetLastChannel(); ++C) { if (Channels.Contains(C)) return false; Channels.Add(C); }
		}
		if (Mode.bFixtureMatrixEnabled) {
			const auto& Matrix=Mode.FixtureMatrixConfig;
			if (Matrix.XCells<1 || Matrix.XCells>512 || Matrix.YCells<1 || Matrix.YCells>512 || Matrix.XCells*Matrix.YCells>512
				|| Matrix.FirstCellChannel<1 || Matrix.FirstCellChannel>512 || Matrix.CellAttributes.IsEmpty()
				|| !StaticEnum<EDMXPixelMappingDistribution>()->IsValidEnumValue(static_cast<int64>(Matrix.PixelMappingDistribution))) return false;
			int32 CellSpan=0; Attributes.Reset();
			for (const auto& Attribute : Matrix.CellAttributes) {
				if (Attribute.Attribute.Name.IsNone() || Attributes.Contains(Attribute.Attribute.Name) || static_cast<uint8>(Attribute.DataType)>3) return false;
				Attributes.Add(Attribute.Attribute.Name); CellSpan+=Attribute.GetNumChannels();
				if (CellSpan>512) return false;
			}
			const int32 LastChannel=Matrix.FirstCellChannel+Matrix.XCells*Matrix.YCells*CellSpan-1;
			if (LastChannel>Mode.ChannelSpan) return false;
			for (int32 C=Matrix.FirstCellChannel; C<=LastChannel; ++C) if (Channels.Contains(C)) return false;
		}
		return true;
	}
}

ATSAVDMXFixture::ATSAVDMXFixture()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bAllowTickBeforeBeginPlay = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Model Root"));
	ModelRoot->SetupAttachment(SceneRoot);

	BaseVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Mesh"));
	BaseVisual->SetupAttachment(ModelRoot);

	PanPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pan Pivot"));
	PanPivot->SetupAttachment(ModelRoot);

	YokeVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Yoke Mesh"));
	YokeVisual->SetupAttachment(PanPivot);

	TiltPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Tilt Pivot"));
	TiltPivot->SetupAttachment(PanPivot);

	HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head Mesh"));
	HeadVisual->SetupAttachment(TiltPivot);

	LensRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Lens Root"));
	LensRoot->SetupAttachment(TiltPivot);

	LensVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lens Mesh"));
	LensVisual->SetupAttachment(TiltPivot);

	BeamLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam Light"));
	BeamLight->SetupAttachment(LensRoot);
	BeamLight->SetMobility(EComponentMobility::Movable);
	BeamLight->SetCastShadows(false);
	BeamLight->SetUseInverseSquaredFalloff(true);

	DMXComponent = CreateDefaultSubobject<UDMXComponent>(TEXT("DMX"));
	DMXComponent->SetReceiveDMXFromPatch(true);
	DMXComponent->OnFixturePatchReceived.AddDynamic(this, &ATSAVDMXFixture::OnFixturePatchReceived);

	BaseVisual->SetMobility(EComponentMobility::Movable);
	YokeVisual->SetMobility(EComponentMobility::Movable);
	HeadVisual->SetMobility(EComponentMobility::Movable);
	LensVisual->SetMobility(EComponentMobility::Movable);
	// Fixtures need editor/runtime hit testing, but never simulate physics. Query
	// collision avoids Chaos mass generation for intentionally flat GDTF parts.
	BaseVisual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	YokeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LensVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TargetPanDegrees = FMath::Lerp(PanMinDegrees, PanMaxDegrees, 0.5f);
	TargetTiltDegrees = FMath::Lerp(TiltMinDegrees, TiltMaxDegrees, 0.5f);
}

void ATSAVDMXFixture::BeginPlay()
{
	Super::BeginPlay();
	ApplyModelSetup();
	DMXComponent->SetReceiveDMXFromPatch(true);
}

void ATSAVDMXFixture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyModelSetup();
	ApplyMotionAndBeam(0.0f, true);
}

void ATSAVDMXFixture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyMotionAndBeam(DeltaSeconds, false);
}

bool ATSAVDMXFixture::ShouldTickIfViewportsOnly() const
{
	return true;
}

#if WITH_EDITOR
void ATSAVDMXFixture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyModelSetup();
}
#endif

void ATSAVDMXFixture::ApplyPreviewValues()
{
	SetTargetsFromNormalized(
		FMath::Clamp(PreviewPan, 0.0f, 1.0f),
		FMath::Clamp(PreviewTilt, 0.0f, 1.0f),
		FMath::Clamp(PreviewDimmer, 0.0f, 1.0f),
		PreviewColor,
		0.0f);
	ApplyMotionAndBeam(0.0f, true);
}

void ATSAVDMXFixture::SetFixturePatch(UDMXEntityFixturePatch* FixturePatch)
{
	if (FixturePatch != IndividualPatch) IndividualPatch = nullptr;
	if (DMXComponent)
	{
		DMXComponent->SetFixturePatch(FixturePatch);
		DMXComponent->SetReceiveDMXFromPatch(true);
	}
}

bool ATSAVDMXFixture::SetIndividualPatchAddress(const int32 Universe, const int32 Address)
{
	UDMXEntityFixturePatch* Patch = GetFixturePatch();
	UDMXLibrary* Library = Patch ? Patch->GetParentLibrary() : nullptr;
	const int32 Span = Patch ? Patch->GetChannelSpan() : 0;
	if (!Library || !Patch->GetActiveMode() || !Patch->GetFixtureType()
		|| Universe < 1 || Universe > 63999 || Address < 1 || Span < 1 || Address + Span - 1 > 512) return false;
	bool bNeedsOwnPatch = Patch != IndividualPatch;
	if (GetWorld())
	{
		for (TActorIterator<ATSAVDMXFixture> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->GetFixturePatch() == Patch) { bNeedsOwnPatch = true; break; }
		}
	}
	Modify();
	DMXComponent->Modify();
	Library->Modify();
	if (bNeedsOwnPatch)
	{
		FDMXEntityFixturePatchConstructionParams Params;
		Params.FixtureTypeRef = FDMXEntityFixtureTypeRef(Patch->GetFixtureType());
		Params.ActiveMode = Patch->GetActiveModeIndex();
		Params.UniverseID = Universe;
		Params.StartingAddress = Address;
		IndividualPatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(Params, TEXT("TSAV Scene Fixture"));
		if (!IndividualPatch) return false;
		SetFixturePatch(IndividualPatch);
	}
	else
	{
		Patch->Modify();
		Patch->SetUniverseID(Universe);
		Patch->SetStartingChannel(Address);
	}
	MarkPackageDirty();
	Library->MarkPackageDirty();
	return GetFixturePatch()->GetUniverseID() == Universe && GetFixturePatch()->GetStartingChannel() == Address;
}

bool ATSAVDMXFixture::ApplyFixtureDefinition(const FTSAVDMXFixtureDefinition& Definition, const bool bApplyDefaultPatch)
{
	FixtureDefinitionId = Definition.DefinitionId;
	GDTFSource = Definition.GDTFSource.LoadSynchronous();
	GDTFModeName = Definition.GDTFModeName;
	BaseMesh = Definition.BaseMesh.LoadSynchronous();
	YokeMesh = Definition.YokeMesh.LoadSynchronous();
	HeadMesh = Definition.HeadMesh.LoadSynchronous();
	LensMesh = Definition.LensMesh.LoadSynchronous();
	FixtureScale = FMath::Max(Definition.FixtureScale, 0.001f);
	ModelRotation = Definition.ModelRotation;
	auto SafeModelScale = [](const FVector& Scale)
	{
		return FVector(
			FMath::Clamp(FMath::Abs(Scale.X), 0.001, 1000.0),
			FMath::Clamp(FMath::Abs(Scale.Y), 0.001, 1000.0),
			FMath::Clamp(FMath::Abs(Scale.Z), 0.001, 1000.0));
	};
	BaseMeshScale = SafeModelScale(Definition.BaseMeshScale);
	YokeMeshScale = SafeModelScale(Definition.YokeMeshScale);
	HeadMeshScale = SafeModelScale(Definition.HeadMeshScale);
	LensMeshScale = SafeModelScale(Definition.LensMeshScale);
	BaseMeshOffset = Definition.BaseMeshOffset;
	YokeMeshOffset = Definition.YokeMeshOffset;
	HeadMeshOffset = Definition.HeadMeshOffset;
	PanPivotOffset = Definition.PanPivotOffset;
	TiltPivotOffset = Definition.TiltPivotOffset;
	PanPivotRotation = Definition.PanPivotRotation;
	TiltPivotRotation = Definition.TiltPivotRotation;
	PanMinDegrees = Definition.PanMinDegrees;
	PanMaxDegrees = Definition.PanMaxDegrees;
	TiltMinDegrees = Definition.TiltMinDegrees;
	TiltMaxDegrees = Definition.TiltMaxDegrees;
	PanSpeedDegreesPerSecond = FMath::Max(Definition.PanSpeedDegreesPerSecond, 0.0f);
	TiltSpeedDegreesPerSecond = FMath::Max(Definition.TiltSpeedDegreesPerSecond, 0.0f);
	LensOffset = Definition.LensOffset;
	LensMeshRotation = Definition.LensMeshRotation;
	BeamRotation = Definition.BeamRotation;
	MaximumIntensityLumens = FMath::Max(Definition.MaximumIntensityLumens, 0.0f);
	MinimumBeamAngleDegrees = Definition.MinimumBeamAngleDegrees;
	MaximumBeamAngleDegrees = Definition.MaximumBeamAngleDegrees;
	AttenuationRadiusCm = FMath::Max(Definition.AttenuationRadiusCm, 1.0f);

	if (bApplyDefaultPatch)
	{
		UDMXEntityFixturePatch* DefaultPatch = nullptr;
		if (UDMXLibrary* Library = Definition.DMXLibrary.LoadSynchronous())
		{
			DefaultPatch = Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId));
		}
		SetFixturePatch(DefaultPatch);
	}

	ApplyModelSetup();
	ApplyPreviewValues();
	return GDTFSource != nullptr && (BaseMesh != nullptr || YokeMesh != nullptr || HeadMesh != nullptr || LensMesh != nullptr);
}

void ATSAVDMXFixture::ApplyNormalizedDMX(
	const float Pan,
	const float Tilt,
	const float Dimmer,
	const FLinearColor Color,
	const float Zoom,
	const bool bSnap)
{
	SetTargetsFromNormalized(Pan, Tilt, Dimmer, Color, Zoom);
	ApplyMotionAndBeam(0.0f, bSnap);
}

UDMXEntityFixturePatch* ATSAVDMXFixture::GetFixturePatch() const
{
	return DMXComponent ? DMXComponent->GetFixturePatch() : nullptr;
}

void ATSAVDMXFixture::OnFixturePatchReceived(UDMXEntityFixturePatch* FixturePatch, const FDMXNormalizedAttributeValueMap& ValuePerAttribute)
{
	FDMXNormalizedAttributeValueMap Input=ValuePerAttribute;
	const FIntPoint Dimensions=GetMatrixDimensions();
	if (FixturePatch) for (int32 Y=0; Y<Dimensions.Y; ++Y) for (int32 X=0; X<Dimensions.X; ++X) {
		TMap<FDMXAttributeName,float> CellValues;
		if (FixturePatch->GetNormalizedMatrixCellValues(FIntPoint(X,Y),CellValues))
			for (const auto& Pair : CellValues) Input.Map.Add(FDMXAttributeName(MatrixAttributeKey(FIntPoint(X,Y),Pair.Key.Name)),Pair.Value);
	}
	ApplyAttributeValues(Input);
}

void ATSAVDMXFixture::ApplyAttributeValues(const FDMXNormalizedAttributeValueMap& ValuePerAttribute, const bool bSnap)
{
	for (const auto& Pair : ValuePerAttribute.Map)
		if (FMath::IsFinite(Pair.Value)) LastAttributeValues.Map.Add(FDMXAttributeName(FName(*CanonicalizeAttribute(Pair.Key.Name))), FMath::Clamp(Pair.Value, 0.0f, 1.0f));
	const auto NormalizedAxis = [](float Target, float Offset, float Minimum, float Maximum, bool bInvert)
	{
		const float Value = FMath::IsNearlyEqual(Minimum, Maximum) ? 0.5f : FMath::Clamp((Target - Offset - Minimum) / (Maximum - Minimum), 0.0f, 1.0f);
		return bInvert ? 1.0f - Value : Value;
	};
	float PanValue = NormalizedAxis(TargetPanDegrees, PanOffsetDegrees, PanMinDegrees, PanMaxDegrees, bInvertPan);
	float TiltValue = NormalizedAxis(TargetTiltDegrees, TiltOffsetDegrees, TiltMinDegrees, TiltMaxDegrees, bInvertTilt);
	float DimmerValue = TargetDimmer;
	float ZoomValue = TargetZoom;
	float RedValue = TargetColor.R;
	float GreenValue = TargetColor.G;
	float BlueValue = TargetColor.B;

	FindAttributeValue(ValuePerAttribute, PanAttribute, { TEXT("pan") }, PanValue);
	FindAttributeValue(ValuePerAttribute, TiltAttribute, { TEXT("tilt") }, TiltValue);
	FindAttributeValue(ValuePerAttribute, DimmerAttribute, { TEXT("dimmer"), TEXT("intensity"), TEXT("masterdimmer") }, DimmerValue);
	FindAttributeValue(ValuePerAttribute, ZoomAttribute, { TEXT("zoom"), TEXT("beamangle") }, ZoomValue);

	const bool bHasRed = FindAttributeValue(ValuePerAttribute, RedAttribute, { TEXT("coloraddr"), TEXT("colorrgbred"), TEXT("red") }, RedValue);
	const bool bHasGreen = FindAttributeValue(ValuePerAttribute, GreenAttribute, { TEXT("coloraddg"), TEXT("colorrgbgreen"), TEXT("green") }, GreenValue);
	const bool bHasBlue = FindAttributeValue(ValuePerAttribute, BlueAttribute, { TEXT("coloraddb"), TEXT("colorrgbblue"), TEXT("blue") }, BlueValue);
	const FLinearColor Color = bHasRed || bHasGreen || bHasBlue ? FLinearColor(RedValue, GreenValue, BlueValue) : TargetColor;

	SetTargetsFromNormalized(PanValue, TiltValue, DimmerValue, Color, ZoomValue);
	ApplyMotionAndBeam(0.0f, bSnap);
}

FDMXNormalizedAttributeValueMap ATSAVDMXFixture::GetAttributeValues() const
{
	FDMXNormalizedAttributeValueMap Result = LastAttributeValues;
	if (const auto* Patch = GetFixturePatch()) if (const auto* Mode = Patch->GetActiveMode()) for (const auto& Function : Mode->Functions)
	{
		const FDMXAttributeName Name(FName(*CanonicalizeAttribute(Function.Attribute.Name)));
		if (!Name.Name.IsNone() && !Result.Map.Contains(Name)) {
			const uint64 Maximum = Function.GetNumChannels() >= 4 ? MAX_uint32 : (1ULL << (8*Function.GetNumChannels()))-1;
			Result.Map.Add(Name,FMath::Clamp(static_cast<float>(static_cast<double>(Function.DefaultValue)/Maximum),0.0f,1.0f));
		}
	}
	for (FName Name : {TEXT("Shutter"),TEXT("Strobe"),TEXT("Iris"),TEXT("Frost"),TEXT("ColorWheel"),TEXT("Cyan"),TEXT("Magenta"),TEXT("Yellow"),TEXT("CTO"),TEXT("Gobo"),TEXT("GoboRotation"),TEXT("GoboSpin"),TEXT("Prism"),TEXT("PrismRotation"),TEXT("BladeTop"),TEXT("BladeBottom"),TEXT("BladeLeft"),TEXT("BladeRight"),TEXT("ColorAdd_W"),TEXT("ColorAdd_A")})
		Result.Map.Add(FDMXAttributeName(Name),OpticsAttribute(Name,Name==TEXT("Shutter") || Name==TEXT("Iris") ? 1.0f : Name==TEXT("GoboSpin") ? 0.5f : 0.0f));
	const auto Axis = [](float Target, float Offset, float Minimum, float Maximum, bool bInvert)
	{
		const float Value = FMath::IsNearlyEqual(Minimum, Maximum) ? 0.5f : FMath::Clamp((Target - Offset - Minimum) / (Maximum - Minimum), 0.0f, 1.0f);
		return bInvert ? 1.0f - Value : Value;
	};
	Result.Map.Add(FDMXAttributeName(PanAttribute), Axis(TargetPanDegrees, PanOffsetDegrees, PanMinDegrees, PanMaxDegrees, bInvertPan));
	Result.Map.Add(FDMXAttributeName(TiltAttribute), Axis(TargetTiltDegrees, TiltOffsetDegrees, TiltMinDegrees, TiltMaxDegrees, bInvertTilt));
	Result.Map.Add(FDMXAttributeName(DimmerAttribute), TargetDimmer);
	Result.Map.Add(FDMXAttributeName(RedAttribute), TargetColor.R);
	Result.Map.Add(FDMXAttributeName(GreenAttribute), TargetColor.G);
	Result.Map.Add(FDMXAttributeName(BlueAttribute), TargetColor.B);
	Result.Map.Add(FDMXAttributeName(ZoomAttribute), TargetZoom);
	const FIntPoint Dimensions=GetMatrixDimensions();
	if (Dimensions.X>0) for (int32 Y=0; Y<Dimensions.Y; ++Y) for (int32 X=0; X<Dimensions.X; ++X) {
		const FIntPoint Cell(X,Y);
		for (const auto& Entry : GetFixturePatch()->GetActiveMode()->FixtureMatrixConfig.CellAttributes) {
			const FDMXAttributeName Key(MatrixAttributeKey(Cell,Entry.Attribute.Name));
			const uint64 Max=Entry.GetNumChannels()>=4 ? MAX_uint32 : (1ULL<<(8*Entry.GetNumChannels()))-1;
			if (!Result.Map.Contains(Key)) Result.Map.Add(Key,FMath::Clamp(static_cast<float>(static_cast<double>(Entry.DefaultValue)/Max),0.0f,1.0f));
		}
		for (FName Attribute : {TEXT("Dimmer"),TEXT("Red"),TEXT("Green"),TEXT("Blue")}) {
			const FDMXAttributeName Key(MatrixAttributeKey(Cell,Attribute)); if (!Result.Map.Contains(Key)) Result.Map.Add(Key,1.0f);
		}
	}
	// Cached aliases must reflect the latest target, including direct normalized updates.
	for (auto& Pair : Result.Map)
	{
		const FString Key = CanonicalizeAttribute(Pair.Key.Name);
		if (Key == TEXT("pan")) Pair.Value = Result.Map[FDMXAttributeName(PanAttribute)];
		else if (Key == TEXT("tilt")) Pair.Value = Result.Map[FDMXAttributeName(TiltAttribute)];
		else if (Key == TEXT("dimmer") || Key == TEXT("intensity") || Key == TEXT("masterdimmer")) Pair.Value = TargetDimmer;
		else if (Key == TEXT("red") || Key == TEXT("coloraddr") || Key == TEXT("colorrgbred")) Pair.Value = TargetColor.R;
		else if (Key == TEXT("green") || Key == TEXT("coloraddg") || Key == TEXT("colorrgbgreen")) Pair.Value = TargetColor.G;
		else if (Key == TEXT("blue") || Key == TEXT("coloraddb") || Key == TEXT("colorrgbblue")) Pair.Value = TargetColor.B;
		else if (Key == TEXT("zoom") || Key == TEXT("beamangle")) Pair.Value = TargetZoom;
	}
	return Result;
}

FString ATSAVDMXFixture::CaptureTSAVState() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
	// Persist this class's editable configuration, excluding component/status references.
	for (TFieldIterator<FProperty> It(StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_Edit | CPF_SaveGame) || It->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient)) continue;
		FString Value;
		It->ExportText_InContainer(0, Value, this, this, const_cast<ATSAVDMXFixture*>(this), PPF_None);
		Properties->SetStringField(It->GetName(), Value);
	}
	Root->SetObjectField(TEXT("properties"), Properties);
	TSharedRef<FJsonObject> Attributes = MakeShared<FJsonObject>();
	for (const auto& Pair : GetAttributeValues().Map) Attributes->SetNumberField(Pair.Key.Name.ToString(), Pair.Value);
	Root->SetObjectField(TEXT("attributes"), Attributes);
	if (UDMXEntityFixturePatch* Patch = GetFixturePatch())
	{
		if (const FDMXFixtureMode* Mode = Patch->GetActiveMode())
		{
			FString ModeJson;
			FJsonObjectConverter::UStructToJsonObjectString(*Mode, ModeJson);
			Root->SetStringField(TEXT("mode"), ModeJson);
			Root->SetNumberField(TEXT("universe"), Patch->GetUniverseID());
			Root->SetNumberField(TEXT("address"), Patch->GetStartingChannel());
		}
	}
	FString Result;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

bool ATSAVDMXFixture::SetStandaloneMode(const FDMXFixtureMode& Mode, int32 Universe, int32 Address)
{
	if (!IsValidSavedMode(Mode) || Universe<1 || Universe>63999 || Address<1 || Address>512 || Address+Mode.ChannelSpan-1>512) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(CaptureTSAVState()),Root)) return false;
	FString ModeJson; FJsonObjectConverter::UStructToJsonObjectString(Mode,ModeJson);
	Root->SetStringField(TEXT("mode"),ModeJson); Root->SetNumberField(TEXT("universe"),Universe); Root->SetNumberField(TEXT("address"),Address);
	FString Json; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
	return RestoreTSAVState(Json);
}

bool ATSAVDMXFixture::RestoreTSAVState(const FString& State)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(State), Root) || !Root) return false;
	int32 Version = 0;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1) return false;
	FString ModeJson;
	FDMXFixtureMode Mode;
	int32 Universe = 1, Address = 1;
	const bool bHasMode = Root->TryGetStringField(TEXT("mode"), ModeJson);
	if (bHasMode)
	{
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(ModeJson, &Mode)
			|| !Root->TryGetNumberField(TEXT("universe"), Universe) || !Root->TryGetNumberField(TEXT("address"), Address)
			|| !IsValidSavedMode(Mode) || Universe < 1 || Universe > 63999 || Address < 1 || Address>512 || Address + Mode.ChannelSpan - 1 > 512) return false;
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!Root->TryGetObjectField(TEXT("properties"), Properties)) return false;
	// Validate property text in isolated value buffers before modifying this actor.
	for (const auto& Pair : (*Properties)->Values)
	{
		FProperty* Property = FindFProperty<FProperty>(StaticClass(), *Pair.Key);
		if (!Property || Property->GetOwnerStruct() != StaticClass() || !Property->HasAnyPropertyFlags(CPF_Edit | CPF_SaveGame)
			|| Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient)) return false;
		FString Text;
		if (!Pair.Value->TryGetString(Text)) return false;
		void* Value = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
		Property->InitializeValue(Value);
		const bool bValid = Property->ImportText_Direct(*Text, Value, this, PPF_None) != nullptr;
		Property->DestroyValue(Value);
		FMemory::Free(Value);
		if (!bValid) return false;
	}
	FDMXNormalizedAttributeValueMap Attributes;
	const TSharedPtr<FJsonObject>* AttributeJson = nullptr;
	if (Root->TryGetObjectField(TEXT("attributes"), AttributeJson))
	{
		for (const auto& Pair : (*AttributeJson)->Values)
		{
			double Value;
			if (!Pair.Value->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0.0 || Value > 1.0) return false;
			Attributes.Map.Add(FDMXAttributeName(FName(*Pair.Key)), Value);
		}
	}
	for (const auto& Pair : (*Properties)->Values)
		FindFProperty<FProperty>(StaticClass(), *Pair.Key)->ImportText_InContainer(*Pair.Value->AsString(), this, this, PPF_None);
	if (bHasMode)
	{
		RestoredPatchLibrary = NewObject<UDMXLibrary>(this, NAME_None, RF_Transactional);
		// The property exists at runtime; UE's convenience name accessor is editor-only.
		auto* PortProperty = FindFProperty<FStructProperty>(UDMXLibrary::StaticClass(), TEXT("PortReferences"));
		if (PortProperty)
		{
			auto* Ports = PortProperty->ContainerPtrToValuePtr<FDMXLibraryPortReferences>(RestoredPatchLibrary);
			for (const auto& Port : FDMXPortManager::Get().GetInputPorts()) Ports->InputPortReferences.Emplace(Port->GetPortGuid(), true);
			for (const auto& Port : FDMXPortManager::Get().GetOutputPorts()) Ports->OutputPortReferences.Emplace(Port->GetPortGuid(), true);
			RestoredPatchLibrary->UpdatePorts();
		}
		FDMXEntityFixtureTypeConstructionParams TypeParams;
		TypeParams.ParentDMXLibrary = RestoredPatchLibrary;
		TypeParams.Modes.Add(Mode);
		UDMXEntityFixtureType* Type = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(TypeParams, TEXT("TSAV Saved Mode"), false);
		FDMXEntityFixturePatchConstructionParams PatchParams;
		PatchParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(Type);
		PatchParams.UniverseID = Universe;
		PatchParams.StartingAddress = Address;
		IndividualPatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams, TEXT("TSAV Saved Fixture"), false);
		SetFixturePatch(IndividualPatch);
		if (!IndividualPatch) return false;
	}
	else SetFixturePatch(nullptr);
	LastAttributeValues.Map.Reset();
	ApplyModelSetup();
	ApplyAttributeValues(Attributes, true);
	return true;
}

void ATSAVDMXFixture::ApplyModelSetup()
{
	if (!ModelRoot)
	{
		return;
	}

	ModelRoot->SetRelativeScale3D(FVector(FMath::Max(FixtureScale, 0.001f)));
	ModelRoot->SetRelativeRotation(ModelRotation);
	BaseVisual->SetStaticMesh(BaseMesh);
	YokeVisual->SetStaticMesh(YokeMesh);
	HeadVisual->SetStaticMesh(HeadMesh);
	LensVisual->SetStaticMesh(LensMesh);
	BaseVisual->SetRelativeScale3D(BaseMeshScale);
	YokeVisual->SetRelativeScale3D(YokeMeshScale);
	HeadVisual->SetRelativeScale3D(HeadMeshScale);
	LensVisual->SetRelativeScale3D(LensMeshScale);
	BaseVisual->SetRelativeLocation(BaseMeshOffset);
	YokeVisual->SetRelativeLocation(YokeMeshOffset);
	HeadVisual->SetRelativeLocation(HeadMeshOffset);
	PanPivot->SetRelativeLocation(PanPivotOffset);
	TiltPivot->SetRelativeLocation(TiltPivotOffset);
	LensVisual->SetRelativeLocation(LensOffset);
	LensVisual->SetRelativeRotation(LensMeshRotation);
	LensRoot->SetRelativeLocation(LensOffset);
	LensRoot->SetRelativeRotation(BeamRotation);

	BeamLight->SetAttenuationRadius(FMath::Max(AttenuationRadiusCm, 1.0f));
	BeamLight->SetInnerConeAngle(FMath::Max(1.0f, MinimumBeamAngleDegrees * 0.7f));
	BeamLight->SetOuterConeAngle(FMath::Clamp(MaximumBeamAngleDegrees, 1.0f, 89.0f));
}

void ATSAVDMXFixture::ApplyMotionAndBeam(float DeltaSeconds, bool bSnap)
{
	const bool bSnapPan = bSnap || PanSpeedDegreesPerSecond <= 0.0f;
	const bool bSnapTilt = bSnap || TiltSpeedDegreesPerSecond <= 0.0f;
	CurrentPanDegrees = bSnapPan
		? TargetPanDegrees
		: FMath::FInterpConstantTo(CurrentPanDegrees, TargetPanDegrees, DeltaSeconds, PanSpeedDegreesPerSecond);
	CurrentTiltDegrees = bSnapTilt
		? TargetTiltDegrees
		: FMath::FInterpConstantTo(CurrentTiltDegrees, TargetTiltDegrees, DeltaSeconds, TiltSpeedDegreesPerSecond);

	const FQuat PanMotion(FVector::UpVector, FMath::DegreesToRadians(CurrentPanDegrees));
	const FQuat TiltMotion(FVector::ForwardVector, FMath::DegreesToRadians(CurrentTiltDegrees));
	PanPivot->SetRelativeRotation(PanPivotRotation.Quaternion() * PanMotion);
	TiltPivot->SetRelativeRotation(TiltPivotRotation.Quaternion() * TiltMotion);

	const float NarrowAngle = FMath::Clamp(MinimumBeamAngleDegrees, 1.0f, 89.0f);
	const float WideAngle = FMath::Clamp(FMath::Max(MaximumBeamAngleDegrees, NarrowAngle), 1.0f, 89.0f);
	const float OuterAngle = FMath::Lerp(WideAngle, NarrowAngle, FMath::Clamp(TargetZoom, 0.0f, 1.0f));
	BeamLight->SetOuterConeAngle(OuterAngle);
	BeamLight->SetInnerConeAngle(FMath::Clamp(OuterAngle * 0.72f, 0.0f, OuterAngle));
	BeamLight->SetIntensity(FMath::Max(MaximumIntensityLumens, 0.0f) * FMath::Clamp(TargetDimmer, 0.0f, 1.0f));
	BeamLight->SetLightColor(TargetColor.GetClamped());
	LastDimmerValue = TargetDimmer;
	ApplyOptics(DeltaSeconds, OuterAngle);
}

void ATSAVDMXFixture::SetTargetsFromNormalized(float Pan, float Tilt, float Dimmer, const FLinearColor& Color, float Zoom)
{
	const float SafePan = bInvertPan ? 1.0f - FMath::Clamp(Pan, 0.0f, 1.0f) : FMath::Clamp(Pan, 0.0f, 1.0f);
	const float SafeTilt = bInvertTilt ? 1.0f - FMath::Clamp(Tilt, 0.0f, 1.0f) : FMath::Clamp(Tilt, 0.0f, 1.0f);
	TargetPanDegrees = FMath::Lerp(PanMinDegrees, PanMaxDegrees, SafePan) + PanOffsetDegrees;
	TargetTiltDegrees = FMath::Lerp(TiltMinDegrees, TiltMaxDegrees, SafeTilt) + TiltOffsetDegrees;
	TargetDimmer = FMath::Clamp(Dimmer, 0.0f, 1.0f);
	TargetColor = Color.GetClamped();
	TargetZoom = FMath::Clamp(Zoom, 0.0f, 1.0f);
}

bool ATSAVDMXFixture::FindAttributeValue(const FDMXNormalizedAttributeValueMap& Values, FName PreferredName, const TArray<FString>& Aliases, float& OutValue)
{
	if (!PreferredName.IsNone())
	{
		if (const float* ExactValue = Values.Map.Find(FDMXAttributeName(PreferredName)))
		{
			OutValue = *ExactValue;
			return true;
		}
	}

	for (const TPair<FDMXAttributeName, float>& Pair : Values.Map)
	{
		const FString Candidate = CanonicalizeAttribute(Pair.Key.Name);
		for (const FString& Alias : Aliases)
		{
			if (Candidate == Alias || Candidate.StartsWith(Alias))
			{
				OutValue = Pair.Value;
				return true;
			}
		}
	}

	return false;
}

FString ATSAVDMXFixture::CanonicalizeAttribute(FName AttributeName)
{
	FString Result = AttributeName.ToString().ToLower();
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	const int32 CellSeparator=Result.Find(TEXT("]."));
	if (Result.StartsWith(TEXT("cell[")) && CellSeparator!=INDEX_NONE) return Result.Left(CellSeparator+2)+CanonicalizeAttribute(FName(*Result.Mid(CellSeparator+2)));
	if (Result==TEXT("intensity") || Result==TEXT("masterdimmer")) return TEXT("dimmer");
	if (Result==TEXT("coloraddr") || Result==TEXT("colorrgbred")) return TEXT("red");
	if (Result==TEXT("coloraddg") || Result==TEXT("colorrgbgreen")) return TEXT("green");
	if (Result==TEXT("coloraddb") || Result==TEXT("colorrgbblue")) return TEXT("blue");
	if (Result==TEXT("beamangle")) return TEXT("zoom");
	if (Result==TEXT("color1")) return TEXT("colorwheel");
	if (Result==TEXT("gobo1")) return TEXT("gobo");
	if (Result==TEXT("gobo1pos")) return TEXT("goborotation");
	if (Result==TEXT("shutter1")) return TEXT("shutter");
	if (Result==TEXT("frost1")) return TEXT("frost");
	if (Result==TEXT("colorsubc")) return TEXT("cyan");
	if (Result==TEXT("colorsubm")) return TEXT("magenta");
	if (Result==TEXT("colorsuby")) return TEXT("yellow");
	return Result;
}
