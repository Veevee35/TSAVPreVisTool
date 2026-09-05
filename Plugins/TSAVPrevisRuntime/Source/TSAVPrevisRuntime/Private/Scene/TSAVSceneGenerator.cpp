// Copyright TSAV. All Rights Reserved.
#include "Scene/TSAVSceneGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "JsonObjectConverter.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATSAVSceneGenerator::ATSAVSceneGenerator()
{
	PrimaryActorTick.bCanEverTick=true; bAllowTickBeforeBeginPlay=true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	MotionRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Motion")); MotionRoot->SetupAttachment(RootComponent);
	Boxes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Boxes"));
	Rods=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rods"));
	Spheres=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spheres"));
	MovingBoxes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Moving boxes"));
	for (auto* Mesh : {Boxes.Get(),Rods.Get(),Spheres.Get(),MovingBoxes.Get()}) {
		Mesh->SetupAttachment(Mesh==MovingBoxes ? MotionRoot.Get() : RootComponent.Get()); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Boxes->SetStaticMesh(Cube.Object); MovingBoxes->SetStaticMesh(Cube.Object); Rods->SetStaticMesh(Cylinder.Object); Spheres->SetStaticMesh(Sphere.Object);
	Cloth=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Pleated cloth")); Cloth->SetupAttachment(RootComponent); Cloth->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CreateDefaultSubobject<UTSAVSceneObjectComponent>(TEXT("Scene identity"));
}

FTSAVScenicSettings ATSAVSceneGenerator::Preset(ETSAVScenicKind Kind)
{
	FTSAVScenicSettings Value; Value.Kind=Kind;
	switch (Kind) {
	case ETSAVScenicKind::Stairs: Value.Width=150; Value.Depth=240; Value.Height=120; Value.Columns=6; break;
	case ETSAVScenicKind::Truss: Value.Width=300; Value.Depth=30; Value.Height=30; Value.Thickness=5; break;
	case ETSAVScenicKind::TrussRing: Value.Width=600; Value.Depth=30; Value.Height=30; Value.Thickness=5; Value.Columns=24; break;
	case ETSAVScenicKind::Scaffold: Value.Width=600; Value.Depth=200; Value.Height=600; Value.Thickness=5; Value.Columns=3; Value.Rows=3; break;
	case ETSAVScenicKind::Drape: Value.Width=600; Value.Depth=20; Value.Height=400; Value.Thickness=3; Value.Columns=16; break;
	case ETSAVScenicKind::Barrier: Value.Width=600; Value.Depth=70; Value.Height=110; Value.Thickness=4; Value.Columns=6; break;
	case ETSAVScenicKind::Crowd: Value.Width=1000; Value.Depth=700; Value.Height=175; Value.Columns=10; Value.Rows=7; break;
	case ETSAVScenicKind::Lift: Value.Width=300; Value.Depth=200; Value.Height=400; break;
	case ETSAVScenicKind::Rail: Value.Width=1000; Value.Depth=100; Value.Height=100; Value.Thickness=5; break;
	default: break;
	}
	return Value;
}
bool ATSAVSceneGenerator::Validate(const FTSAVScenicSettings& Value)
{
	return Value.Version==1 && static_cast<uint8>(Value.Kind)<=static_cast<uint8>(ETSAVScenicKind::Rail)
		&& FMath::IsFinite(Value.Width) && Value.Width>=1 && Value.Width<=100000
		&& FMath::IsFinite(Value.Depth) && Value.Depth>=1 && Value.Depth<=100000
		&& FMath::IsFinite(Value.Height) && Value.Height>=1 && Value.Height<=100000
		&& FMath::IsFinite(Value.Thickness) && Value.Thickness>=0.1f && Value.Thickness<=1000
		&& Value.Columns>=1 && Value.Columns<=100 && Value.Rows>=1 && Value.Rows<=100 && Value.Columns*Value.Rows<=2500
		&& FMath::IsFinite(Value.Position) && Value.Position>=0 && Value.Position<=1
		&& FMath::IsFinite(Value.PeriodSeconds) && Value.PeriodSeconds>=0.1f;
}
bool ATSAVSceneGenerator::Configure(const FTSAVScenicSettings& Value) { if (!Validate(Value)) return false; Modify(); Settings=Value; MotionTime=0; Rebuild(); MarkPackageDirty(); return true; }
void ATSAVSceneGenerator::Box(FVector Center,FVector Size,bool bMoving) { (bMoving ? MovingBoxes : Boxes)->AddInstance(FTransform(FQuat::Identity,Center,Size/100.0)); }
void ATSAVSceneGenerator::Rod(FVector From,FVector To,float Diameter)
{
	const FVector Direction=To-From; if (Direction.IsNearlyZero()) return;
	Rods->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::UpVector,Direction.GetSafeNormal()),(From+To)*0.5,FVector(Diameter/100,Diameter/100,Direction.Size()/100)));
}
void ATSAVSceneGenerator::Rebuild()
{
	if (!Validate(Settings)) return;
	Boxes->ClearInstances(); Rods->ClearInstances(); Spheres->ClearInstances(); MovingBoxes->ClearInstances(); Cloth->ClearAllMeshSections();
	const float W=Settings.Width,D=Settings.Depth,H=Settings.Height,T=FMath::Min(FMath::Min3(Settings.Thickness,W,D),H);
	const int32 NX=Settings.Columns,NY=Settings.Rows;
	auto* Identity=FindComponentByClass<UTSAVSceneObjectComponent>();
	Identity->ObjectType=Settings.Kind==ETSAVScenicKind::Stage || Settings.Kind==ETSAVScenicKind::Stairs ? ETSAVObjectType::Stage : Settings.Kind==ETSAVScenicKind::Truss || Settings.Kind==ETSAVScenicKind::TrussRing ? ETSAVObjectType::Truss : ETSAVObjectType::Scenic;
	switch (Settings.Kind) {
	case ETSAVScenicKind::Stage:
		Box(FVector(0,0,H-T*0.5f),FVector(W,D,T));
		for (int32 X : {-1,1}) for (int32 Y : {-1,1}) Box(FVector(X*(W-T)*0.5f,Y*(D-T)*0.5f,(H-T)*0.5f),FVector(T,T,FMath::Max(0.1f,H-T)));
		break;
	case ETSAVScenicKind::Stairs:
		for (int32 I=0; I<NX; ++I) Box(FVector(0,-D*0.5f+(I+0.5f)*D/NX,(I+1)*H/NX*0.5f),FVector(W,D/NX,(I+1)*H/NX));
		break;
	case ETSAVScenicKind::Truss:
		for (int32 Y : {-1,1}) for (int32 Z : {-1,1}) Rod(FVector(-W*0.5f,Y*D*0.5f,Z*H*0.5f),FVector(W*0.5f,Y*D*0.5f,Z*H*0.5f),T);
		for (int32 I=0; I<NX; ++I) { const float A=-W*0.5f+I*W/NX,B=A+W/NX;
			for (int32 Side : {-1,1}) { Rod(FVector(A,Side*D*0.5f,-H*0.5f),FVector(B,Side*D*0.5f,H*0.5f),T*0.5f); Rod(FVector(A,-D*0.5f,Side*H*0.5f),FVector(B,D*0.5f,Side*H*0.5f),T*0.5f); }
		}
		break;
	case ETSAVScenicKind::TrussRing:
		for (int32 I=0; I<FMath::Max(8,NX); ++I) { const float A=I*2*PI/FMath::Max(8,NX),B=(I+1)*2*PI/FMath::Max(8,NX);
			for (int32 Side : {-1,1}) for (int32 Z : {-1,1}) { const float Radius=FMath::Max(D,(W+Side*D)*0.5f);
				Rod(FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,Z*H*0.5f),FVector(FMath::Cos(B)*Radius,FMath::Sin(B)*Radius,Z*H*0.5f),T);
				if (Z==1) Rod(FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,-H*0.5f),FVector(FMath::Cos(B)*Radius,FMath::Sin(B)*Radius,H*0.5f),T*0.5f);
			}
		}
		break;
	case ETSAVScenicKind::Scaffold:
		for (int32 I=0; I<=NX; ++I) for (int32 Side : {-1,1}) Rod(FVector(-W*0.5f+I*W/NX,Side*D*0.5f,0),FVector(-W*0.5f+I*W/NX,Side*D*0.5f,H),T);
		for (int32 J=1; J<=NY; ++J) { Box(FVector(0,0,J*H/NY-T*0.5f),FVector(W,D,T));
			for (int32 I=0; I<NX; ++I) for (int32 Side : {-1,1}) Rod(FVector(-W*0.5f+I*W/NX,Side*D*0.5f,(J-1)*H/NY),FVector(-W*0.5f+(I+1)*W/NX,Side*D*0.5f,J*H/NY),T*0.6f);
		}
		break;
	case ETSAVScenicKind::Drape:
	{
		TArray<FVector> Vertices,Normals; TArray<int32> Triangles; TArray<FVector2D> UVs; TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
		const int32 Segments=NX*16;
		for (int32 I=0; I<=Segments; ++I) for (int32 Z=0; Z<2; ++Z) {
			const float U=static_cast<float>(I)/Segments,Phase=U*NX*2*PI;
			Vertices.Add(FVector((U-0.5f)*W,FMath::Sin(Phase)*D*0.5f,Z*H)); UVs.Add(FVector2D(U,Z));
			Normals.Add(FVector(-FMath::Cos(Phase)*D*0.5f*NX*2*PI/W,1,0).GetSafeNormal()); Colors.Add(FLinearColor::White); Tangents.Emplace(FVector::UpVector,false);
		}
		for (int32 I=0; I<Segments; ++I) { int32 A=I*2; Triangles.Append({A,A+2,A+1,A+1,A+2,A+3,A+1,A+2,A,A+3,A+2,A+1}); }
		Cloth->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UVs,Colors,Tangents,true); break;
	}
	case ETSAVScenicKind::Barrier:
		for (int32 I=0; I<=NX; ++I) { const float X=-W*0.5f+I*W/NX; Rod(FVector(X,0,0),FVector(X,0,H),T); Box(FVector(X,0,T*0.5f),FVector(T*3,D,T)); }
		for (float Level : {0.4f,1.0f}) Rod(FVector(-W*0.5f,0,H*Level),FVector(W*0.5f,0,H*Level),T);
		break;
	case ETSAVScenicKind::Crowd:
		for (int32 Y=0; Y<NY; ++Y) for (int32 X=0; X<NX; ++X) {
			const FVector P((X+0.5f)*W/NX-W*0.5f,(Y+0.5f)*D/NY-D*0.5f,0);
			Spheres->AddInstance(FTransform(FQuat::Identity,P+FVector(0,0,H*0.9f),FVector(H*0.2f/100)));
			Rod(P+FVector(0,0,H*0.45f),P+FVector(0,0,H*0.8f),H*0.22f);
			for (int32 Side : {-1,1}) { Rod(P+FVector(Side*H*0.08f,0,H*0.45f),P+FVector(Side*H*0.10f,0,0),H*0.07f); Rod(P+FVector(0,0,H*0.73f),P+FVector(Side*H*0.18f,0,H*0.40f),H*0.06f); }
		}
		break;
	case ETSAVScenicKind::Lift:
		Box(FVector(0,0,T*0.5f),FVector(W,D,T),true);
		for (int32 X : {-1,1}) for (int32 Y : {-1,1}) Rod(FVector(X*W*0.5f,Y*D*0.5f,0),FVector(X*W*0.5f,Y*D*0.5f,H),T);
		break;
	case ETSAVScenicKind::Rail:
		for (int32 Side : {-1,1}) Rod(FVector(-W*0.5f,Side*D*0.4f,0),FVector(W*0.5f,Side*D*0.4f,0),T);
		Box(FVector(0,0,H*0.5f),FVector(D,D,H),true); break;
	}
	UpdateMotion();
}
void ATSAVSceneGenerator::UpdateMotion() { MotionRoot->SetRelativeLocation(Settings.Kind==ETSAVScenicKind::Lift ? FVector(0,0,Settings.Height*Settings.Position) : Settings.Kind==ETSAVScenicKind::Rail ? FVector((Settings.Position-0.5f)*Settings.Width,0,0) : FVector::ZeroVector); }
void ATSAVSceneGenerator::SetMotionPosition(float Value) { if (FMath::IsFinite(Value)) { Settings.Position=FMath::Clamp(Value,0.0f,1.0f); UpdateMotion(); } }
void ATSAVSceneGenerator::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); if (Settings.bAnimate && Validate(Settings)) { MotionTime+=DeltaSeconds; SetMotionPosition(0.5f-0.5f*FMath::Cos(MotionTime*2*PI/Settings.PeriodSeconds)); } }
void ATSAVSceneGenerator::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); Rebuild(); }
int32 ATSAVSceneGenerator::GetPartCount() const { return Boxes->GetInstanceCount()+Rods->GetInstanceCount()+Spheres->GetInstanceCount()+MovingBoxes->GetInstanceCount()+Cloth->GetNumSections(); }
FVector ATSAVSceneGenerator::GetMotionOffset() const { return MotionRoot->GetRelativeLocation(); }
FString ATSAVSceneGenerator::CaptureTSAVState() const { FString Result; FJsonObjectConverter::UStructToJsonObjectString(Settings,Result); return Result; }
bool ATSAVSceneGenerator::RestoreTSAVState(const FString& State) { FTSAVScenicSettings Value; return FJsonObjectConverter::JsonObjectStringToUStruct(State,&Value) && Configure(Value); }
#if WITH_EDITOR
void ATSAVSceneGenerator::PostEditUndo() { Super::PostEditUndo(); Rebuild(); }
#endif
