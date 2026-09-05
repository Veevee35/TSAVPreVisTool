// Copyright TSAV. All Rights Reserved.
#include "Laser/TSAVLaserPreview.h"
#include "Lighting/TSAVLightingShow.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "JsonObjectConverter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/ConstructorHelpers.h"

ATSAVLaserPreview::ATSAVLaserPreview()
{
	PrimaryActorTick.bCanEverTick=true; bAllowTickBeforeBeginPlay=true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Lines=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Laser paths")); Lines->SetupAttachment(RootComponent); Lines->NumCustomDataFloats=3; Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision); Lines->SetCastShadow(false);
	// Additive line material needs the conventional mesh path even if the source cube uses Nanite.
	Lines->bDisallowNanite=true;
	Housing=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Projector")); Housing->SetupAttachment(RootComponent); Housing->SetRelativeScale3D(FVector(0.3,0.25,0.2)); Housing->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); Lines->SetStaticMesh(Cube.Object); Housing->SetStaticMesh(Cube.Object);
	CreateDefaultSubobject<UTSAVSceneObjectComponent>(TEXT("Scene identity"));
}
bool ATSAVLaserPreview::Validate(const FTSAVLaserData& Candidate)
{
	if (Candidate.Version!=1 || Candidate.Frames.IsEmpty() || Candidate.Frames.Num()>65535 || Candidate.Projector<0 || Candidate.Projector>255
		|| !FMath::IsFinite(Candidate.FramesPerSecond) || Candidate.FramesPerSecond<1 || Candidate.FramesPerSecond>120
		|| !FMath::IsFinite(Candidate.WidthCm) || Candidate.WidthCm<1 || Candidate.WidthCm>100000
		|| !FMath::IsFinite(Candidate.HeightCm) || Candidate.HeightCm<1 || Candidate.HeightCm>100000
		|| !FMath::IsFinite(Candidate.DistanceCm) || Candidate.DistanceCm<1 || Candidate.DistanceCm>100000
		|| !FMath::IsFinite(Candidate.LineWidthCm) || Candidate.LineWidthCm<=0 || Candidate.LineWidthCm>100
		|| !FMath::IsFinite(Candidate.Intensity) || Candidate.Intensity<0 || Candidate.Intensity>1) return false;
	int64 Count=0;
	for (const auto& Frame : Candidate.Frames) {
		Count+=Frame.Points.Num(); if (Frame.Projector<0 || Frame.Projector>255 || Frame.Points.IsEmpty() || Frame.Points.Num()>65535 || Count>1000000) return false;
		for (const auto& Point : Frame.Points) if (Point.Position.ContainsNaN() || Point.Position.GetAbsMax()>1.0 || (!Frame.b3D && !FMath::IsNearlyZero(Point.Position.Z))) return false;
	}
	return true;
}
bool ATSAVLaserPreview::Configure(const FTSAVLaserData& Candidate)
{
	if (!Validate(Candidate)) return false;
	Modify(); Data=Candidate; LastFrame=INDEX_NONE; BeamMaterial=nullptr; RebuildFrame(); MarkPackageDirty(); return true;
}
bool ATSAVLaserPreview::ImportILDA(const FString& Path,FString& Error)
{
	const int64 Size=IFileManager::Get().FileSize(*Path);
	if (Size<32 || Size>64*1024*1024) { Error=TEXT("Choose an ILDA file between 32 bytes and 64 MB."); return false; }
	TArray<uint8> Bytes; TArray<FTSAVLaserFrame> Frames;
	if (!FFileHelper::LoadFileToArray(Bytes,*Path) || !TSAVILDA::Decode(Bytes,Frames,Error)) return false;
	if (Frames.IsEmpty()) { Error=TEXT("The file contains no laser frames."); return false; }
	auto Candidate=Data; Candidate.Frames=MoveTemp(Frames); Candidate.Projector=Candidate.Frames[0].Projector;
	if (!Configure(Candidate)) { Error=TEXT("Invalid laser frame data."); return false; }
	Time=0; bPlaying=false; LastFrame=INDEX_NONE; RebuildFrame(); return true;
}
bool ATSAVLaserPreview::ExportILDA(const FString& Path,FString& Error,bool bIndexed) const
{
	TArray<uint8> Bytes; if (Path.IsEmpty() || !TSAVILDA::Encode(Data.Frames,Bytes,Error,bIndexed)) return false;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true); const FString Temp=Path+TEXT(".tmp");
	if (!FFileHelper::SaveArrayToFile(Bytes,*Temp) || !IFileManager::Get().Move(*Path,*Temp,true,true)) { Error=TEXT("Could not save ILDA file."); return false; }
	return true;
}
void ATSAVLaserPreview::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Data.Frames.IsEmpty()) Data.Frames=TSAVILDA::MakePattern(0,1);
	LastFrame=INDEX_NONE; RebuildFrame();
}
void ATSAVLaserPreview::Seek(float Seconds)
{
	if (!FMath::IsFinite(Seconds)) return;
	Time=FMath::Max(0.0f,Seconds); RebuildFrame();
}
void ATSAVLaserPreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Data.bFollowShowTimeline) { if (const auto* Show=ATSAVLightingShow::Find(GetWorld())) Seek(Show->GetTimelineTime()); }
	else if (bPlaying) Seek(Time+DeltaSeconds);
}
void ATSAVLaserPreview::RebuildFrame()
{
	TArray<int32> FrameIndices; for (int32 I=0; I<Data.Frames.Num(); ++I) if (Data.Frames[I].Projector==Data.Projector) FrameIndices.Add(I);
	if (FrameIndices.IsEmpty()) { Lines->ClearInstances(); LastFrame=INDEX_NONE; return; }
	const double FramePosition=static_cast<double>(Time)*Data.FramesPerSecond;
	int32 Index=static_cast<int32>(FMath::Min(FramePosition,static_cast<double>(MAX_int32)));
	if (Data.bLoop) Index=static_cast<int32>(FMath::Fmod(FramePosition,static_cast<double>(FrameIndices.Num())));
	else if (Index>=FrameIndices.Num()) { Index=FrameIndices.Num()-1; bPlaying=false; Time=static_cast<float>(Index)/Data.FramesPerSecond; }
	const int32 Frame=FrameIndices[Index]; if (Frame==LastFrame) return;
	LastFrame=Frame; Lines->ClearInstances();
	if (!BeamMaterial) if (auto* Source=Data.Material.LoadSynchronous()) { BeamMaterial=UMaterialInstanceDynamic::Create(Source,this); Lines->SetMaterial(0,BeamMaterial); }
	const auto Position=[&](const FVector& P) { return FVector(Data.DistanceCm-P.Z*Data.WidthCm*0.5f,P.X*Data.WidthCm*0.5f,P.Y*Data.HeightCm*0.5f); };
	const auto Line=[&](FVector A,FVector B,FColor Color,float Brightness) {
		const FVector Direction=B-A; if (Direction.IsNearlyZero()) return;
		const int32 Instance=Lines->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector,Direction.GetSafeNormal()),(A+B)*0.5,FVector(Direction.Size(),Data.LineWidthCm,Data.LineWidthCm)/100));
		const FLinearColor RGB=FLinearColor::FromSRGBColor(Color)*Data.Intensity*Brightness;
		Lines->SetCustomDataValue(Instance,0,RGB.R,false); Lines->SetCustomDataValue(Instance,1,RGB.G,false); Lines->SetCustomDataValue(Instance,2,RGB.B,false);
	};
	const auto& Points=Data.Frames[Frame].Points;
	for (int32 I=1; I<Points.Num(); ++I) if (!Points[I].bBlank) {
		Line(Position(Points[I-1].Position),Position(Points[I].Position),Points[I].Color,1);
		if (Data.bShowRays && I%FMath::Max(1,Points.Num()/512)==0) Line(FVector::ZeroVector,Position(Points[I].Position),Points[I].Color,0.15f);
	}
	Lines->MarkRenderStateDirty();
}
int32 ATSAVLaserPreview::GetSegmentCount() const { return Lines->GetInstanceCount(); }
FString ATSAVLaserPreview::CaptureTSAVState() const { FString Text; FJsonObjectConverter::UStructToJsonObjectString(Data,Text); return Text; }
bool ATSAVLaserPreview::RestoreTSAVState(const FString& State) { FTSAVLaserData Candidate; return FJsonObjectConverter::JsonObjectStringToUStruct(State,&Candidate) && Configure(Candidate); }
#if WITH_EDITOR
void ATSAVLaserPreview::PostEditUndo() { Super::PostEditUndo(); LastFrame=INDEX_NONE; RebuildFrame(); }
#endif
