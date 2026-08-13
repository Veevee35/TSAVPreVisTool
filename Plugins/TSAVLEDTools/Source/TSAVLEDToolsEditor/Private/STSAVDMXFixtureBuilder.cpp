// Copyright TSAV. All Rights Reserved.

#include "STSAVDMXFixtureBuilder.h"

#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "DMXGDTF.h"
#include "DMXZipper.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "GDTF/DMXGDTFDescription.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/Geometries/DMXGDTFAxisGeometry.h"
#include "GDTF/Geometries/DMXGDTFBeamGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometryCollect.h"
#include "GDTF/Geometries/DMXGDTFGeometryReference.h"
#include "GDTF/Models/DMXGDTFModel.h"
#include "GDTF/DMXModes/DMXGDTFChannelFunction.h"
#include "GDTF/DMXModes/DMXGDTFDMXChannel.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "GDTF/DMXModes/DMXGDTFLogicalChannel.h"
#include "IDesktopPlatform.h"
#include "LevelEditorViewport.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXGDTFAssetImportData.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "TSAVDMXFixture.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TSAVDMXFixtureBuilder"

namespace TSAVDMXFixtureBuilder::Private
{
	enum class EFixtureMeshRole : uint8
	{
		None,
		Base,
		Yoke,
		Head,
		Lens
	};

	FString Canonicalize(const FString& InString)
	{
		FString Result = InString.ToLower();
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-"), TEXT(""));
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		return Result;
	}

	EFixtureMeshRole GetMeshRole(const UE::DMX::GDTF::FDMXGDTFModel& Model)
	{
		switch (Model.PrimitiveType)
		{
		case EDMXGDTFModelPrimitiveType::Base:
		case EDMXGDTFModelPrimitiveType::Base1_1:
			return EFixtureMeshRole::Base;
		case EDMXGDTFModelPrimitiveType::Yoke:
			return EFixtureMeshRole::Yoke;
		case EDMXGDTFModelPrimitiveType::Head:
			return EFixtureMeshRole::Head;
		default:
			break;
		}

		const FString Name = Canonicalize(Model.Name.ToString() + TEXT("_") + Model.File);
		if (Name.Contains(TEXT("base")) || Name.Contains(TEXT("body")))
		{
			return EFixtureMeshRole::Base;
		}
		if (Name.Contains(TEXT("yoke")) || Name.Contains(TEXT("arm")))
		{
			return EFixtureMeshRole::Yoke;
		}
		if (Name.Contains(TEXT("head")))
		{
			return EFixtureMeshRole::Head;
		}
		if (Name.Contains(TEXT("lens")) || Name.Contains(TEXT("beam")))
		{
			return EFixtureMeshRole::Lens;
		}
		return EFixtureMeshRole::None;
	}

	EFixtureMeshRole GetMeshRole(const UStaticMesh& Mesh)
	{
		const FString Name = Canonicalize(Mesh.GetName());
		if (Name.Contains(TEXT("base")) || Name.Contains(TEXT("body")))
		{
			return EFixtureMeshRole::Base;
		}
		if (Name.Contains(TEXT("yoke")) || Name.Contains(TEXT("arm")))
		{
			return EFixtureMeshRole::Yoke;
		}
		if (Name.Contains(TEXT("head")))
		{
			return EFixtureMeshRole::Head;
		}
		if (Name.Contains(TEXT("lens")) || Name.Contains(TEXT("beam")))
		{
			return EFixtureMeshRole::Lens;
		}
		return EFixtureMeshRole::None;
	}

	int32 GetMeshRolePriority(const UStaticMesh& Mesh, EFixtureMeshRole Role)
	{
		const FString Name = Canonicalize(Mesh.GetName());
		switch (Role)
		{
		case EFixtureMeshRole::Base:
			return Name.Contains(TEXT("base")) ? 0 : Name.Contains(TEXT("body")) ? 1 : 2;
		case EFixtureMeshRole::Yoke:
			return Name.Contains(TEXT("yoke")) ? 0 : Name.Contains(TEXT("arm")) ? 1 : 2;
		case EFixtureMeshRole::Head:
			return Name.Contains(TEXT("head")) ? 0 : 2;
		case EFixtureMeshRole::Lens:
			return Name.Contains(TEXT("lens")) ? 0 : Name.Contains(TEXT("beam")) ? 1 : 2;
		default:
			return MAX_int32;
		}
	}

	FString NormalizeArchivePath(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Path.StartsWith(TEXT("./")))
		{
			Path.RightChopInline(2);
		}
		return Path;
	}

	bool IsSafeArchivePath(const FString& Path)
	{
		if (Path.IsEmpty() || !FPaths::IsRelative(Path) || Path.Contains(TEXT(":")))
		{
			return false;
		}

		TArray<FString> Components;
		Path.ParseIntoArray(Components, TEXT("/"), true);
		return !Components.Contains(TEXT(".."));
	}

	int32 GetEmbeddedModelPriority(const FString& ArchivePath)
	{
		const FString Extension = FPaths::GetExtension(ArchivePath).ToLower();
		if (Extension == TEXT("glb"))
		{
			return 0;
		}
		if (Extension == TEXT("gltf"))
		{
			return 1;
		}
		return MAX_int32;
	}

	TSharedPtr<UE::DMX::GDTF::FDMXGDTFBeamGeometry> FindFirstBeamGeometry(const TSharedPtr<UE::DMX::GDTF::FDMXGDTFGeometry>& Geometry)
	{
		using namespace UE::DMX::GDTF;
		if (!Geometry.IsValid())
		{
			return nullptr;
		}
		for (const TSharedPtr<FDMXGDTFBeamGeometry>& Beam : Geometry->BeamArray)
		{
			if (Beam.IsValid())
			{
				return Beam;
			}
		}
		for (const TSharedPtr<FDMXGDTFGeometry>& Child : Geometry->GeometryArray)
		{
			if (const TSharedPtr<FDMXGDTFBeamGeometry> Beam = FindFirstBeamGeometry(Child))
			{
				return Beam;
			}
		}
		for (const TSharedPtr<FDMXGDTFAxisGeometry>& Axis : Geometry->AxisArray)
		{
			if (const TSharedPtr<FDMXGDTFBeamGeometry> Beam = FindFirstBeamGeometry(StaticCastSharedPtr<FDMXGDTFGeometry>(Axis)))
			{
				return Beam;
			}
		}
		return nullptr;
	}

	void GatherGeometryModelRoles(
		const TSharedPtr<UE::DMX::GDTF::FDMXGDTFGeometry>& Geometry,
		int32 AxisDepth,
		TMap<FName, EFixtureMeshRole>& OutRoleByModelName)
	{
		using namespace UE::DMX::GDTF;
		if (!Geometry.IsValid())
		{
			return;
		}

		if (!Geometry->Model.IsEmpty())
		{
			const FString GeometryName = Canonicalize(Geometry->Name.ToString());
			EFixtureMeshRole Role = EFixtureMeshRole::None;
			if (GeometryName.Contains(TEXT("base")) || GeometryName.Contains(TEXT("body")))
			{
				Role = EFixtureMeshRole::Base;
			}
			else if (GeometryName.Contains(TEXT("yoke")) || GeometryName.Contains(TEXT("arm")) || GeometryName.Contains(TEXT("pan")))
			{
				Role = EFixtureMeshRole::Yoke;
			}
			else if (GeometryName.Contains(TEXT("head")) || GeometryName.Contains(TEXT("tilt")))
			{
				Role = EFixtureMeshRole::Head;
			}
			else if (AxisDepth == 0)
			{
				Role = EFixtureMeshRole::Base;
			}
			else if (AxisDepth == 1)
			{
				Role = EFixtureMeshRole::Yoke;
			}
			else
			{
				Role = EFixtureMeshRole::Head;
			}

			OutRoleByModelName.FindOrAdd(FName(*Geometry->Model)) = Role;
		}

		for (const TSharedPtr<FDMXGDTFBeamGeometry>& Beam : Geometry->BeamArray)
		{
			if (Beam.IsValid() && !Beam->Model.IsEmpty())
			{
				OutRoleByModelName.FindOrAdd(FName(*Beam->Model)) = EFixtureMeshRole::Lens;
			}
		}
		for (const TSharedPtr<FDMXGDTFGeometry>& Child : Geometry->GeometryArray)
		{
			GatherGeometryModelRoles(Child, AxisDepth, OutRoleByModelName);
		}
		for (const TSharedPtr<FDMXGDTFAxisGeometry>& Axis : Geometry->AxisArray)
		{
			GatherGeometryModelRoles(StaticCastSharedPtr<FDMXGDTFGeometry>(Axis), AxisDepth + 1, OutRoleByModelName);
		}
	}

	bool IsAttribute(const FName Attribute, const TCHAR* Prefix)
	{
		return Canonicalize(Attribute.ToString()).StartsWith(Prefix);
	}

	TSharedRef<SWidget> MakeSectionHeader(const FText& Step, const FText& Title, const FText& Help)
	{
		return SNew(SBorder)
			.Padding(FMargin(10.0f, 11.0f))
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Header")))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBorder).Padding(FMargin(7.0f, 3.0f)).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
					[SNew(STextBlock).Text(Step).Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[SNew(STextBlock).Text(Help).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
				]
			];
	}

	TSharedRef<SWidget> MakeFloatField(const FText& Label, float* Value, float Minimum, float Maximum = TNumericLimits<float>::Max())
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(105.0f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([Value]() { return TOptional<float>(*Value); })
					.MinValue(Minimum)
					.MaxValue(Maximum < TNumericLimits<float>::Max() ? TOptional<float>(Maximum) : TOptional<float>())
					.AllowSpin(true)
					.OnValueChanged_Lambda([Value, Minimum, Maximum](float NewValue) { *Value = FMath::Clamp(NewValue, Minimum, Maximum); })
				]
			];
	}

	TSharedRef<SWidget> MakeSignedFloatField(const FText& Label, float* Value)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(105.0f)
				[SNew(SNumericEntryBox<float>).Value_Lambda([Value]() { return TOptional<float>(*Value); }).AllowSpin(true).OnValueChanged_Lambda([Value](float NewValue) { *Value = NewValue; })]
			];
	}

	TSharedRef<SWidget> MakeIntField(const FText& Label, int32* Value, int32 Minimum, int32 Maximum)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(105.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.Value_Lambda([Value]() { return TOptional<int32>(*Value); })
					.MinValue(Minimum).MaxValue(Maximum).MinSliderValue(Minimum).MaxSliderValue(Maximum).AllowSpin(true)
					.OnValueChanged_Lambda([Value, Minimum, Maximum](int32 NewValue) { *Value = FMath::Clamp(NewValue, Minimum, Maximum); })
				]
			];
	}

	TSharedRef<SWidget> MakeVectorField(const FText& Label, FVector* Value)
	{
		auto MakeAxis = [Value](int32 Axis, const FText& AxisLabel) -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f)[SNew(STextBlock).Text(AxisLabel)]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<double>)
					.Value_Lambda([Value, Axis]() { return TOptional<double>((*Value)[Axis]); })
					.AllowSpin(true)
					.OnValueChanged_Lambda([Value, Axis](double NewValue) { (*Value)[Axis] = NewValue; })
				];
		};

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[SNew(STextBlock).Text(Label)]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeAxis(0, LOCTEXT("AxisX", "X"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeAxis(1, LOCTEXT("AxisY", "Y"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeAxis(2, LOCTEXT("AxisZ", "Z"))]
			];
	}

	TSharedRef<SWidget> MakeRotationField(const FText& Label, FRotator* Value)
	{
		auto MakeAngle = [Value](int32 Axis, const FText& AxisLabel) -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f)[SNew(STextBlock).Text(AxisLabel)]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<double>)
					.Value_Lambda([Value, Axis]() { return TOptional<double>(Axis == 0 ? Value->Pitch : Axis == 1 ? Value->Yaw : Value->Roll); })
					.AllowSpin(true)
					.OnValueChanged_Lambda([Value, Axis](double NewValue)
					{
						if (Axis == 0) Value->Pitch = NewValue;
						else if (Axis == 1) Value->Yaw = NewValue;
						else Value->Roll = NewValue;
					})
				];
		};

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[SNew(STextBlock).Text(Label)]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeAngle(0, LOCTEXT("Pitch", "Pitch"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 0.0f)[MakeAngle(1, LOCTEXT("Yaw", "Yaw"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeAngle(2, LOCTEXT("Roll", "Roll"))]
			];
	}
}

void STSAVDMXFixtureBuilder::Construct(const FArguments& InArgs)
{
	using namespace TSAVDMXFixtureBuilder::Private;
	StatusMessage = LOCTEXT("Ready", "Choose a GDTF. Its embedded glTF model will be imported automatically when available.");

	ChildSlot
	[
		SNew(SBorder).Padding(14.0f).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Title", "GDTF DMX Fixture Builder")).Font(FAppStyle::GetFontStyle(TEXT("HeadingLarge")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 14.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Subtitle", "Import the fixture definition and model, set articulation and beam behavior, then create a patched DMX fixture in one workflow.")).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
				[
					SNew(SBorder).Padding(9.0f).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Header")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(this, &STSAVDMXFixtureBuilder::GetSelectionStatus).AutoWrapText(true)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("LoadSelected", "Load Selected Fixture")).OnClicked(this, &STSAVDMXFixtureBuilder::LoadSelectedFixture)]
					]
				]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step1", "1"), LOCTEXT("GDTFTitle", "GDTF & Mode"), LOCTEXT("GDTFHelp", "Import a .gdtf file or choose one already in the project. Embedded glTF/GLB models are extracted automatically, and the selected mode becomes a real Unreal DMX fixture type and patch."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SObjectPropertyEntryBox).AllowedClass(UDMXImportGDTF::StaticClass()).ObjectPath_Lambda([this]() { return GDTFSource.IsValid() ? GDTFSource->GetPathName() : FString(); }).OnObjectChanged(this, &STSAVDMXFixtureBuilder::OnGDTFChanged).DisplayUseSelected(true).DisplayBrowse(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("ImportGDTF", "Import GDTF…")).OnClicked(this, &STSAVDMXFixtureBuilder::ImportGDTF)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("Mode", "Fixture mode"))]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(ModeCombo, SComboBox<TSharedPtr<FString>>).OptionsSource(&ModeOptions).OnGenerateWidget(this, &STSAVDMXFixtureBuilder::GenerateModeWidget).OnSelectionChanged(this, &STSAVDMXFixtureBuilder::OnModeSelected)
						[
							SNew(STextBlock).Text_Lambda([this]() { return SelectedMode.IsValid() ? FText::FromString(*SelectedMode) : LOCTEXT("NoMode", "No mode available"); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 14.0f)[SNew(STextBlock).Text(this, &STSAVDMXFixtureBuilder::GetModeSummary).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step2", "2"), LOCTEXT("ModelTitle", "3D Model"), LOCTEXT("ModelHelp", "Embedded GDTF glTF/GLB models are assigned automatically. Use manual import only as a fallback. Separate Base, Yoke, Head, and Lens meshes provide accurate articulation."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f)[SNew(SButton).Text(LOCTEXT("ImportModel", "Import 3D Model…")).HAlign(HAlign_Center).OnClicked(this, &STSAVDMXFixtureBuilder::ImportModel)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("BaseMesh", "Base"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return BaseMesh.IsValid()?BaseMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnBaseMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("YokeMesh", "Yoke"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return YokeMesh.IsValid()?YokeMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnYokeMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("HeadMesh", "Head / full model"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return HeadMesh.IsValid()?HeadMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnHeadMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("LensMesh", "Lens / beam"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return LensMesh.IsValid()?LensMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnLensMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f)[MakeFloatField(LOCTEXT("Scale", "Fixture scale"), &FixtureScale, 0.001f, 100.0f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 14.0f)[MakeRotationField(LOCTEXT("ModelRotation", "Model axis correction"), &ModelRotation)]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step3", "3"), LOCTEXT("MotionTitle", "Pan, Tilt & Pivots"), LOCTEXT("MotionHelp", "Set the real motion limits, home offsets, direction, speed, and the pivot positions used by the imported model."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[SNew(SGridPanel).FillColumn(0,1.0f).FillColumn(1,1.0f)+SGridPanel::Slot(0,0).Padding(0.0f,3.0f,8.0f,3.0f)[MakeSignedFloatField(LOCTEXT("PanMin","Pan minimum °"),&PanMin)]+SGridPanel::Slot(1,0).Padding(8.0f,3.0f,0.0f,3.0f)[MakeSignedFloatField(LOCTEXT("PanMax","Pan maximum °"),&PanMax)]+SGridPanel::Slot(0,1).Padding(0.0f,3.0f,8.0f,3.0f)[MakeSignedFloatField(LOCTEXT("TiltMin","Tilt minimum °"),&TiltMin)]+SGridPanel::Slot(1,1).Padding(8.0f,3.0f,0.0f,3.0f)[MakeSignedFloatField(LOCTEXT("TiltMax","Tilt maximum °"),&TiltMax)]+SGridPanel::Slot(0,2).Padding(0.0f,3.0f,8.0f,3.0f)[MakeSignedFloatField(LOCTEXT("PanOffset","Pan home offset °"),&PanOffset)]+SGridPanel::Slot(1,2).Padding(8.0f,3.0f,0.0f,3.0f)[MakeSignedFloatField(LOCTEXT("TiltOffset","Tilt home offset °"),&TiltOffset)]+SGridPanel::Slot(0,3).Padding(0.0f,3.0f,8.0f,3.0f)[MakeFloatField(LOCTEXT("PanSpeed","Pan speed °/s"),&PanSpeed,0.0f,10000.0f)]+SGridPanel::Slot(1,3).Padding(8.0f,3.0f,0.0f,3.0f)[MakeFloatField(LOCTEXT("TiltSpeed","Tilt speed °/s"),&TiltSpeed,0.0f,10000.0f)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsChecked_Lambda([this](){return bInvertPan?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState State){bInvertPan=State==ECheckBoxState::Checked;})[SNew(STextBlock).Text(LOCTEXT("InvertPan","Invert pan"))]]+SHorizontalBox::Slot().AutoWidth().Padding(24.0f,0.0f)[SNew(SCheckBox).IsChecked_Lambda([this](){return bInvertTilt?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState State){bInvertTilt=State==ECheckBoxState::Checked;})[SNew(STextBlock).Text(LOCTEXT("InvertTilt","Invert tilt"))]]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeVectorField(LOCTEXT("PanPivot", "Pan pivot offset (cm)"), &PanPivotOffset)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeVectorField(LOCTEXT("TiltPivot", "Tilt pivot offset (cm)"), &TiltPivotOffset)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 14.0f)[SNew(SGridPanel).FillColumn(0,1.0f).FillColumn(1,1.0f)+SGridPanel::Slot(0,0).Padding(0.0f,3.0f,8.0f,3.0f)[MakeFloatField(LOCTEXT("PreviewPan","Preview pan (0–1)"),&PreviewPan,0.0f,1.0f)]+SGridPanel::Slot(1,0).Padding(8.0f,3.0f,0.0f,3.0f)[MakeFloatField(LOCTEXT("PreviewTilt","Preview tilt (0–1)"),&PreviewTilt,0.0f,1.0f)]]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step4", "4"), LOCTEXT("BeamTitle", "Beam & DMX Patch"), LOCTEXT("BeamHelp", "Position the lens, set beam output, and choose the local universe and starting address. Dimmer, RGB, Zoom, Pan, and Tilt attributes are detected from GDTF."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[MakeVectorField(LOCTEXT("LensOffset", "Lens offset from tilt pivot (cm)"), &LensOffset)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeRotationField(LOCTEXT("LensMeshRotation", "Lens model rotation"), &LensMeshRotation)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeRotationField(LOCTEXT("BeamRotation", "Beam direction correction"), &BeamRotation)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[SNew(SGridPanel).FillColumn(0,1.0f).FillColumn(1,1.0f)+SGridPanel::Slot(0,0).Padding(0.0f,3.0f,8.0f,3.0f)[MakeFloatField(LOCTEXT("Intensity","Max lumens"),&MaximumIntensity,0.0f,10000000.0f)]+SGridPanel::Slot(1,0).Padding(8.0f,3.0f,0.0f,3.0f)[MakeFloatField(LOCTEXT("Attenuation","Beam distance (cm)"),&AttenuationRadius,1.0f,100000.0f)]+SGridPanel::Slot(0,1).Padding(0.0f,3.0f,8.0f,3.0f)[MakeFloatField(LOCTEXT("MinBeam","Narrow beam °"),&MinimumBeamAngle,1.0f,89.0f)]+SGridPanel::Slot(1,1).Padding(8.0f,3.0f,0.0f,3.0f)[MakeFloatField(LOCTEXT("MaxBeam","Wide beam °"),&MaximumBeamAngle,1.0f,89.0f)]+SGridPanel::Slot(0,2).Padding(0.0f,3.0f,8.0f,3.0f)[MakeIntField(LOCTEXT("Universe","Universe"),&Universe,0,63999)]+SGridPanel::Slot(1,2).Padding(8.0f,3.0f,0.0f,3.0f)[MakeIntField(LOCTEXT("Address","Starting address"),&Address,1,512)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 14.0f)[SNew(STextBlock).Text(this,&STSAVDMXFixtureBuilder::GetValidationText).AutoWrapText(true).ColorAndOpacity_Lambda([this](){return CanCreateFixture()?FLinearColor(0.15f,0.8f,0.35f):FLinearColor(0.9f,0.2f,0.15f);})]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step5", "5"), LOCTEXT("CreateTitle", "Create or Update Fixture"), LOCTEXT("CreateHelp", "Creates the DMX Library, fixture type, patch, and articulated actor. Existing selected fixtures can be updated without rebuilding the level."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f)[SNew(SEditableTextBox).Text_Lambda([this](){return FText::FromString(FixtureName);}).OnTextCommitted_Lambda([this](const FText& Text,ETextCommit::Type){FixtureName=Text.ToString();}).HintText(LOCTEXT("NameHint","Fixture name"))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f,0.0f,4.0f,0.0f)[SNew(SButton).Text(LOCTEXT("CreateFixture","Create Patched Fixture")).HAlign(HAlign_Center).IsEnabled_Lambda([this](){return CanCreateFixture();}).OnClicked(this,&STSAVDMXFixtureBuilder::CreateFixture)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f,0.0f)[SNew(SButton).Text(LOCTEXT("UpdateFixture","Update Selected Fixture")).HAlign(HAlign_Center).IsEnabled_Lambda([this](){return CanCreateFixture()&&(ActiveFixture.IsValid()||FindSelectedFixture()!=nullptr);}).OnClicked(this,&STSAVDMXFixtureBuilder::UpdateFixture)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f,0.0f,0.0f,0.0f)[SNew(SButton).Text(LOCTEXT("PreviewFixture","Preview Position")).OnClicked(this,&STSAVDMXFixtureBuilder::PreviewSelectedFixture)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f,10.0f,0.0f,4.0f)[SNew(STextBlock).Text_Lambda([this](){return StatusMessage;}).ColorAndOpacity_Lambda([this](){return bStatusSuccess?FLinearColor(0.15f,0.8f,0.35f):FLinearColor(0.9f,0.2f,0.15f);}).AutoWrapText(true)]
			]
		]
	];
}

FReply STSAVDMXFixtureBuilder::ImportGDTF()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		SetStatus(LOCTEXT("NoDesktopPlatform", "The operating-system file dialog is unavailable."), false);
		return FReply::Handled();
	}

	TArray<FString> Files;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (!DesktopPlatform->OpenFileDialog(ParentWindow, TEXT("Import GDTF Fixture"), TEXT(""), TEXT(""), TEXT("GDTF Fixture (*.gdtf)|*.gdtf"), EFileDialogFlags::None, Files) || Files.IsEmpty())
	{
		return FReply::Handled();
	}

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = Files[0];
	Task->DestinationPath = TEXT("/Game/TSAV/Fixtures/GDTF");
	Task->DestinationName = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(Files[0]));
	Task->bAutomated = true;
	Task->bSave = true;
	Task->bAsync = false;
	TArray<UAssetImportTask*> Tasks{ Task };
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks(Tasks);

	for (UObject* Object : Task->GetObjects())
	{
		if (UDMXImportGDTF* ImportedGDTF = Cast<UDMXImportGDTF>(Object))
		{
			GDTFSource = ImportedGDTF;
			RefreshGDTFModes(true);
			FixtureName = FPaths::GetBaseFilename(Files[0]);
			FText EmbeddedModelResult;
			const int32 NumEmbeddedMeshes = ImportEmbeddedGDTFModels(EmbeddedModelResult);
			SetStatus(
				FText::Format(LOCTEXT("GDTFImported", "Imported GDTF: {0}. {1}"), FText::FromString(ImportedGDTF->GetName()), EmbeddedModelResult),
				NumEmbeddedMeshes >= 0);
			return FReply::Handled();
		}
	}

	SetStatus(LOCTEXT("GDTFImportFailed", "The GDTF could not be imported. Check that the file contains a valid description.xml."), false);
	return FReply::Handled();
}

FReply STSAVDMXFixtureBuilder::ImportModel()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		SetStatus(LOCTEXT("NoModelDialog", "The operating-system file dialog is unavailable."), false);
		return FReply::Handled();
	}

	TArray<FString> Files;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const FString Filter = TEXT("3D Models (*.fbx;*.obj;*.gltf;*.glb)|*.fbx;*.obj;*.gltf;*.glb|All files (*.*)|*.*");
	if (!DesktopPlatform->OpenFileDialog(ParentWindow, TEXT("Import Fixture 3D Model"), TEXT(""), TEXT(""), Filter, EFileDialogFlags::None, Files) || Files.IsEmpty())
	{
		return FReply::Handled();
	}

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = Files[0];
	Task->DestinationPath = TEXT("/Game/TSAV/Fixtures/Models/") + ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(Files[0]));
	Task->bAutomated = true;
	Task->bSave = true;
	Task->bAsync = false;
	TArray<UAssetImportTask*> Tasks{ Task };
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks(Tasks);

	TArray<UStaticMesh*> ImportedMeshes;
	for (UObject* Object : Task->GetObjects())
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
		{
			ImportedMeshes.Add(Mesh);
		}
	}

	for (UStaticMesh* Mesh : ImportedMeshes)
	{
		const FString Name = Mesh->GetName().ToLower();
		if (!BaseMesh.IsValid() && (Name.Contains(TEXT("base")) || Name.Contains(TEXT("body")))) BaseMesh = Mesh;
		else if (!YokeMesh.IsValid() && (Name.Contains(TEXT("yoke")) || Name.Contains(TEXT("arm")))) YokeMesh = Mesh;
		else if (!HeadMesh.IsValid() && Name.Contains(TEXT("head"))) HeadMesh = Mesh;
		else if (!LensMesh.IsValid() && (Name.Contains(TEXT("lens")) || Name.Contains(TEXT("beam")))) LensMesh = Mesh;
	}
	if (!ImportedMeshes.IsEmpty() && !BaseMesh.IsValid() && !YokeMesh.IsValid() && !HeadMesh.IsValid() && !LensMesh.IsValid())
	{
		HeadMesh = ImportedMeshes.Last();
	}

	if (ImportedMeshes.IsEmpty())
	{
		SetStatus(LOCTEXT("ModelImportFailed", "No Static Mesh was created. Check the model format and import settings."), false);
	}
	else
	{
		SetStatus(FText::Format(LOCTEXT("ModelImported", "Imported {0} mesh asset(s). Review the Base, Yoke, Head, and Lens assignments below."), FText::AsNumber(ImportedMeshes.Num())), true);
	}
	return FReply::Handled();
}

int32 STSAVDMXFixtureBuilder::ImportEmbeddedGDTFModels(FText& OutResultMessage)
{
	using namespace TSAVDMXFixtureBuilder::Private;
	using namespace UE::DMX::GDTF;

	if (!GDTFSource.IsValid() || !GDTFSource->GetGDTFAssetImportData())
	{
		OutResultMessage = LOCTEXT("NoEmbeddedModelSource", "The GDTF source archive is unavailable, so its model could not be inspected.");
		return -1;
	}

	const TArray64<uint8>& RawSourceData = GDTFSource->GetGDTFAssetImportData()->GetRawSourceData();
	const TSharedRef<FDMXZipper> Zip = MakeShared<FDMXZipper>();
	if (RawSourceData.IsEmpty() || !Zip->LoadFromData(RawSourceData))
	{
		OutResultMessage = LOCTEXT("InvalidEmbeddedModelArchive", "The stored GDTF archive could not be opened to inspect its model.");
		return -1;
	}

	UDMXGDTF* GDTF = GDTFSource->LoadGDTF();
	const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GDTF && GDTF->GetDescription().IsValid() ? GDTF->GetDescription()->GetFixtureType() : nullptr;
	if (!FixtureType.IsValid())
	{
		OutResultMessage = LOCTEXT("InvalidEmbeddedModelDescription", "The GDTF description is invalid, so its model could not be matched to fixture parts.");
		return -1;
	}

	const FString GltfPrefix = TEXT("models/gltf/");
	TMap<FString, FString> BestArchiveByRelativeStem;
	TArray<FString> GltfResources;
	bool bHasLegacy3DSModel = false;
	for (const FString& File : Zip->GetFiles())
	{
		const FString ArchivePath = NormalizeArchivePath(File);
		if (!IsSafeArchivePath(ArchivePath))
		{
			continue;
		}

		if (ArchivePath.StartsWith(TEXT("models/3ds/"), ESearchCase::IgnoreCase) && FPaths::GetExtension(ArchivePath).Equals(TEXT("3ds"), ESearchCase::IgnoreCase))
		{
			bHasLegacy3DSModel = true;
		}

		if (!ArchivePath.StartsWith(GltfPrefix, ESearchCase::IgnoreCase))
		{
			continue;
		}

		GltfResources.Add(ArchivePath);
		const int32 Priority = GetEmbeddedModelPriority(ArchivePath);
		if (Priority == MAX_int32)
		{
			continue;
		}

		FString RelativeStem = FPaths::ChangeExtension(ArchivePath.Mid(GltfPrefix.Len()), TEXT(""));
		RelativeStem.RemoveFromEnd(TEXT("."));
		RelativeStem.ToLowerInline();
		if (const FString* ExistingPath = BestArchiveByRelativeStem.Find(RelativeStem))
		{
			if (Priority < GetEmbeddedModelPriority(*ExistingPath))
			{
				BestArchiveByRelativeStem[RelativeStem] = ArchivePath;
			}
		}
		else
		{
			BestArchiveByRelativeStem.Add(RelativeStem, ArchivePath);
		}
	}

	if (BestArchiveByRelativeStem.IsEmpty())
	{
		OutResultMessage = bHasLegacy3DSModel
			? LOCTEXT("LegacyEmbeddedModel", "This GDTF only contains a legacy 3DS model, which Unreal cannot import directly. Convert it to glTF/GLB or use manual model import.")
			: LOCTEXT("NoEmbeddedModel", "No embedded glTF/GLB model was found; a model can still be assigned manually.");
		return 0;
	}

	TMap<FString, FString> ArchivePathByKey;
	TMap<FString, TArray<TSharedPtr<FDMXGDTFModel>>> ModelsByArchiveKey;
	TSet<FName> ReferencedModelNames;
	TMap<FName, EFixtureMeshRole> RoleByModelName;
	if (FixtureType->GeometryCollect.IsValid())
	{
		for (const TSharedPtr<FDMXGDTFGeometry>& RootGeometry : FixtureType->GeometryCollect->GeometryArray)
		{
			GatherGeometryModelRoles(RootGeometry, 0, RoleByModelName);
		}
		for (const TSharedPtr<FDMXGDTFAxisGeometry>& RootAxis : FixtureType->GeometryCollect->AxisArray)
		{
			GatherGeometryModelRoles(StaticCastSharedPtr<FDMXGDTFGeometry>(RootAxis), 1, RoleByModelName);
		}

		TArray<TSharedPtr<FDMXGDTFGeometry>> Geometries;
		TArray<TSharedPtr<FDMXGDTFGeometryReference>> GeometryReferences;
		FixtureType->GeometryCollect->GetGeometriesRecursive(Geometries, GeometryReferences);
		for (const TSharedPtr<FDMXGDTFGeometry>& Geometry : Geometries)
		{
			if (Geometry.IsValid() && !Geometry->Model.IsEmpty())
			{
				ReferencedModelNames.Add(FName(*Geometry->Model));
			}
		}
	}

	for (const TSharedPtr<FDMXGDTFModel>& Model : FixtureType->Models)
	{
		if (!Model.IsValid() || Model->File.IsEmpty() || (!ReferencedModelNames.IsEmpty() && !ReferencedModelNames.Contains(Model->Name)))
		{
			continue;
		}

		FString ModelStem = NormalizeArchivePath(Model->File);
		ModelStem = FPaths::ChangeExtension(ModelStem, TEXT(""));
		ModelStem.RemoveFromEnd(TEXT("."));
		ModelStem.ToLowerInline();

		const FString* ArchivePath = BestArchiveByRelativeStem.Find(ModelStem);
		if (!ArchivePath)
		{
			const FString ModelBaseName = FPaths::GetBaseFilename(ModelStem);
			for (const TPair<FString, FString>& Pair : BestArchiveByRelativeStem)
			{
				if (FPaths::GetBaseFilename(Pair.Key).Equals(ModelBaseName, ESearchCase::IgnoreCase))
				{
					ArchivePath = &Pair.Value;
					break;
				}
			}
		}

		if (ArchivePath)
		{
			const FString ArchiveKey = ArchivePath->ToLower();
			ArchivePathByKey.FindOrAdd(ArchiveKey) = *ArchivePath;
			ModelsByArchiveKey.FindOrAdd(ArchiveKey).Add(Model);
		}
	}

	// Some producers omit the Model.File link even though valid model resources are present.
	if (ArchivePathByKey.IsEmpty())
	{
		for (const TPair<FString, FString>& Pair : BestArchiveByRelativeStem)
		{
			ArchivePathByKey.Add(Pair.Value.ToLower(), Pair.Value);
		}
	}

	const FString ExtractionRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("TSAV/GDTFModels"),
		ObjectTools::SanitizeObjectName(GDTFSource->GetName())));
	IFileManager::Get().MakeDirectory(*ExtractionRoot, true);

	TMap<FString, FString> ExtractedPathByArchiveKey;
	for (const FString& ArchivePath : GltfResources)
	{
		TArray64<uint8> FileData;
		if (!Zip->GetFileContent(ArchivePath, FileData))
		{
			continue;
		}

		const FString ExtractedPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ExtractionRoot, ArchivePath));
		if (!ExtractedPath.StartsWith(ExtractionRoot, ESearchCase::IgnoreCase))
		{
			continue;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExtractedPath), true);
		if (FFileHelper::SaveArrayToFile(FileData, *ExtractedPath))
		{
			ExtractedPathByArchiveKey.Add(ArchivePath.ToLower(), ExtractedPath);
		}
	}

	TArray<UAssetImportTask*> ImportTasks;
	TMap<UAssetImportTask*, FString> ArchiveKeyByTask;
	const FString DestinationPath = TEXT("/Game/TSAV/Fixtures/Models/") + ObjectTools::SanitizeObjectName(GDTFSource->GetName());
	for (const TPair<FString, FString>& Pair : ArchivePathByKey)
	{
		const FString* ExtractedPath = ExtractedPathByArchiveKey.Find(Pair.Key);
		if (!ExtractedPath)
		{
			continue;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = *ExtractedPath;
		Task->DestinationPath = DestinationPath;
		Task->bAutomated = true;
		Task->bSave = true;
		Task->bAsync = false;
		Task->bReplaceExisting = true;
		ImportTasks.Add(Task);
		ArchiveKeyByTask.Add(Task, Pair.Key);
	}

	if (ImportTasks.IsEmpty())
	{
		OutResultMessage = LOCTEXT("EmbeddedModelExtractionFailed", "Embedded model resources were found but could not be extracted safely.");
		return -1;
	}

	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks(ImportTasks);

	UStaticMesh* ImportedBase = nullptr;
	UStaticMesh* ImportedYoke = nullptr;
	UStaticMesh* ImportedHead = nullptr;
	UStaticMesh* ImportedLens = nullptr;
	int32 ImportedBasePriority = MAX_int32;
	int32 ImportedYokePriority = MAX_int32;
	int32 ImportedHeadPriority = MAX_int32;
	int32 ImportedLensPriority = MAX_int32;
	TArray<UStaticMesh*> AllImportedMeshes;
	TSet<UStaticMesh*> AssignedMeshes;
	auto GetRoleForModel = [&RoleByModelName](const FDMXGDTFModel& Model)
	{
		const EFixtureMeshRole NamedRole = GetMeshRole(Model);
		if (NamedRole != EFixtureMeshRole::None)
		{
			return NamedRole;
		}
		if (const EFixtureMeshRole* GeometryRole = RoleByModelName.Find(Model.Name))
		{
			return *GeometryRole;
		}
		return EFixtureMeshRole::None;
	};
	auto AssignMesh = [&ImportedBase, &ImportedYoke, &ImportedHead, &ImportedLens, &ImportedBasePriority, &ImportedYokePriority, &ImportedHeadPriority, &ImportedLensPriority, &AssignedMeshes](UStaticMesh* Mesh, EFixtureMeshRole Role)
	{
		if (!Mesh)
		{
			return;
		}

		UStaticMesh** Destination = nullptr;
		int32* DestinationPriority = nullptr;
		switch (Role)
		{
		case EFixtureMeshRole::Base: Destination = &ImportedBase; DestinationPriority = &ImportedBasePriority; break;
		case EFixtureMeshRole::Yoke: Destination = &ImportedYoke; DestinationPriority = &ImportedYokePriority; break;
		case EFixtureMeshRole::Head: Destination = &ImportedHead; DestinationPriority = &ImportedHeadPriority; break;
		case EFixtureMeshRole::Lens: Destination = &ImportedLens; DestinationPriority = &ImportedLensPriority; break;
		default: break;
		}
		const int32 Priority = GetMeshRolePriority(*Mesh, Role);
		if (Destination && DestinationPriority && (!*Destination || Priority < *DestinationPriority))
		{
			AssignedMeshes.Remove(*Destination);
			*Destination = Mesh;
			*DestinationPriority = Priority;
			AssignedMeshes.Add(Mesh);
		}
	};

	for (UAssetImportTask* Task : ImportTasks)
	{
		TArray<UStaticMesh*> TaskMeshes;
		for (UObject* Object : Task->GetObjects())
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
			{
				TaskMeshes.Add(Mesh);
				AllImportedMeshes.AddUnique(Mesh);
			}
		}

		const FString* ArchiveKey = ArchiveKeyByTask.Find(Task);
		const TArray<TSharedPtr<FDMXGDTFModel>>* LinkedModels = ArchiveKey ? ModelsByArchiveKey.Find(*ArchiveKey) : nullptr;
		for (int32 MeshIndex = 0; MeshIndex < TaskMeshes.Num(); ++MeshIndex)
		{
			UStaticMesh* Mesh = TaskMeshes[MeshIndex];
			EFixtureMeshRole Role = GetMeshRole(*Mesh);

			if (Role == EFixtureMeshRole::None && LinkedModels)
			{
				const FString MeshName = Canonicalize(Mesh->GetName());
				for (const TSharedPtr<FDMXGDTFModel>& Model : *LinkedModels)
				{
					if (Model.IsValid() && MeshName.Contains(Canonicalize(Model->Name.ToString())))
					{
						Role = GetRoleForModel(*Model);
						if (Role != EFixtureMeshRole::None)
						{
							break;
						}
					}
				}
			}

			if (Role == EFixtureMeshRole::None && LinkedModels && TaskMeshes.Num() == LinkedModels->Num() && LinkedModels->IsValidIndex(MeshIndex) && (*LinkedModels)[MeshIndex].IsValid())
			{
				Role = GetRoleForModel(*(*LinkedModels)[MeshIndex]);
			}

			if (Role == EFixtureMeshRole::None && LinkedModels && TaskMeshes.Num() == 1)
			{
				EFixtureMeshRole OnlyRole = EFixtureMeshRole::None;
				bool bAmbiguousRole = false;
				for (const TSharedPtr<FDMXGDTFModel>& Model : *LinkedModels)
				{
					const EFixtureMeshRole CandidateRole = Model.IsValid() ? GetRoleForModel(*Model) : EFixtureMeshRole::None;
					if (CandidateRole == EFixtureMeshRole::None)
					{
						continue;
					}
					if (OnlyRole != EFixtureMeshRole::None && CandidateRole != OnlyRole)
					{
						bAmbiguousRole = true;
						break;
					}
					OnlyRole = CandidateRole;
				}
				if (!bAmbiguousRole)
				{
					Role = OnlyRole;
				}
			}

			AssignMesh(Mesh, Role);
		}
	}

	if (AllImportedMeshes.IsEmpty())
	{
		OutResultMessage = LOCTEXT("EmbeddedModelImportFailed", "An embedded glTF/GLB model was extracted, but Unreal did not create a Static Mesh from it.");
		return -1;
	}

	if (!ImportedHead)
	{
		for (int32 Index = AllImportedMeshes.Num() - 1; Index >= 0; --Index)
		{
			if (!AssignedMeshes.Contains(AllImportedMeshes[Index]))
			{
				ImportedHead = AllImportedMeshes[Index];
				break;
			}
		}
	}
	if (!ImportedBase && !ImportedYoke && !ImportedHead && !ImportedLens)
	{
		ImportedHead = AllImportedMeshes.Last();
	}

	BaseMesh = ImportedBase;
	YokeMesh = ImportedYoke;
	HeadMesh = ImportedHead;
	LensMesh = ImportedLens;
	OutResultMessage = FText::Format(
		LOCTEXT("EmbeddedModelImported", "Imported {0} embedded Static Mesh asset(s) and assigned the physical fixture parts automatically."),
		FText::AsNumber(AllImportedMeshes.Num()));
	return AllImportedMeshes.Num();
}

void STSAVDMXFixtureBuilder::OnGDTFChanged(const FAssetData& AssetData)
{
	GDTFSource = Cast<UDMXImportGDTF>(AssetData.GetAsset());
	RefreshGDTFModes(true);
	if (GDTFSource.IsValid())
	{
		FText EmbeddedModelResult;
		const int32 NumEmbeddedMeshes = ImportEmbeddedGDTFModels(EmbeddedModelResult);
		SetStatus(EmbeddedModelResult, NumEmbeddedMeshes >= 0);
	}
}

void STSAVDMXFixtureBuilder::OnBaseMeshChanged(const FAssetData& AssetData) { BaseMesh = Cast<UStaticMesh>(AssetData.GetAsset()); }
void STSAVDMXFixtureBuilder::OnYokeMeshChanged(const FAssetData& AssetData) { YokeMesh = Cast<UStaticMesh>(AssetData.GetAsset()); }
void STSAVDMXFixtureBuilder::OnHeadMeshChanged(const FAssetData& AssetData) { HeadMesh = Cast<UStaticMesh>(AssetData.GetAsset()); }
void STSAVDMXFixtureBuilder::OnLensMeshChanged(const FAssetData& AssetData) { LensMesh = Cast<UStaticMesh>(AssetData.GetAsset()); }

void STSAVDMXFixtureBuilder::RefreshGDTFModes(bool bAdoptPhysicalMotion)
{
	ParsedModes = BuildFixtureModes();
	ModeOptions.Reset();
	for (const FDMXFixtureMode& Mode : ParsedModes)
	{
		ModeOptions.Add(MakeShared<FString>(Mode.ModeName));
	}
	SelectedMode = ModeOptions.IsEmpty() ? nullptr : ModeOptions[0];
	if (ModeCombo.IsValid())
	{
		ModeCombo->RefreshOptions();
		ModeCombo->SetSelectedItem(SelectedMode);
	}

	if (bAdoptPhysicalMotion)
	{
		AdoptSelectedModePhysicalProperties();
	}
}

void STSAVDMXFixtureBuilder::AdoptSelectedModePhysicalProperties()
{
	if (!GDTFSource.IsValid())
	{
		return;
	}

	UDMXGDTF* GDTF = GDTFSource->LoadGDTF();
	const TSharedPtr<UE::DMX::GDTF::FDMXGDTFFixtureType> FixtureType = GDTF && GDTF->GetDescription().IsValid() ? GDTF->GetDescription()->GetFixtureType() : nullptr;
	if (!FixtureType.IsValid() || FixtureType->DMXModes.IsEmpty())
	{
		return;
	}

	const TSharedPtr<UE::DMX::GDTF::FDMXGDTFDMXMode> GDTFMode = FixtureType->DMXModes[FMath::Clamp(GetSelectedModeIndex(), 0, FixtureType->DMXModes.Num() - 1)];
	TSharedPtr<UE::DMX::GDTF::FDMXGDTFGeometry> PanGeometry;
	TSharedPtr<UE::DMX::GDTF::FDMXGDTFGeometry> TiltGeometry;
	bool bAdoptedZoomRange = false;
	for (const TSharedPtr<UE::DMX::GDTF::FDMXGDTFDMXChannel>& Channel : GDTFMode->DMXChannels)
	{
		if (!Channel.IsValid() || Channel->LogicalChannelArray.IsEmpty() || !Channel->LogicalChannelArray[0].IsValid()) continue;
		const FName Attribute = Channel->LogicalChannelArray[0]->Attribute;
		const TSharedPtr<UE::DMX::GDTF::FDMXGDTFChannelFunction> Function = Channel->ResolveInitialFunction();
		if (!Function.IsValid()) continue;
		if (TSAVDMXFixtureBuilder::Private::IsAttribute(Attribute, TEXT("pan")))
		{
			PanMin = Function->PhysicalFrom; PanMax = Function->PhysicalTo;
			if (Function->RealFade > 0.01f) PanSpeed = FMath::Abs(PanMax - PanMin) / Function->RealFade;
			PanGeometry = Channel->ResolveGeometry();
		}
		else if (TSAVDMXFixtureBuilder::Private::IsAttribute(Attribute, TEXT("tilt")))
		{
			TiltMin = Function->PhysicalFrom; TiltMax = Function->PhysicalTo;
			if (Function->RealFade > 0.01f) TiltSpeed = FMath::Abs(TiltMax - TiltMin) / Function->RealFade;
			TiltGeometry = Channel->ResolveGeometry();
		}
		else if (TSAVDMXFixtureBuilder::Private::IsAttribute(Attribute, TEXT("zoom")))
		{
			const float HalfAngleFrom = FMath::Abs(Function->PhysicalFrom) * 0.5f;
			const float HalfAngleTo = FMath::Abs(Function->PhysicalTo) * 0.5f;
			MinimumBeamAngle = FMath::Clamp(FMath::Min(HalfAngleFrom, HalfAngleTo), 1.0f, 89.0f);
			MaximumBeamAngle = FMath::Clamp(FMath::Max(HalfAngleFrom, HalfAngleTo), MinimumBeamAngle, 89.0f);
			bAdoptedZoomRange = true;
		}
	}

	// GDTF geometry matrices and model dimensions are expressed in meters, while Unreal uses centimeters.
	constexpr float MetersToCentimeters = 100.0f;
	if (PanGeometry.IsValid())
	{
		PanPivotOffset = PanGeometry->Position.GetTranslation() * MetersToCentimeters;
	}
	if (TiltGeometry.IsValid())
	{
		TiltPivotOffset = TiltGeometry->Position.GetTranslation() * MetersToCentimeters;
	}

	const TSharedPtr<UE::DMX::GDTF::FDMXGDTFBeamGeometry> BeamGeometry = TSAVDMXFixtureBuilder::Private::FindFirstBeamGeometry(GDTFMode->ResolveGeometry());
	if (BeamGeometry.IsValid())
	{
		LensOffset = BeamGeometry->Position.GetTranslation() * MetersToCentimeters;
		LensMeshRotation = BeamGeometry->Position.GetRotation().Rotator();
		const FVector BeamDirection = BeamGeometry->Position.GetRotation().RotateVector(FVector(0.0f, -1.0f, 0.0f));
		if (!BeamDirection.IsNearlyZero())
		{
			BeamRotation = FRotationMatrix::MakeFromX(BeamDirection).Rotator();
		}
		if (BeamGeometry->LuminousFlux > 0.0f)
		{
			MaximumIntensity = BeamGeometry->LuminousFlux;
		}
		if (!bAdoptedZoomRange)
		{
			const float HalfBeamAngle = FMath::Max(BeamGeometry->BeamAngle * 0.5f, 1.0f);
			const float HalfFieldAngle = FMath::Max(BeamGeometry->FieldAngle * 0.5f, HalfBeamAngle);
			MinimumBeamAngle = FMath::Clamp(HalfBeamAngle, 1.0f, 89.0f);
			MaximumBeamAngle = FMath::Clamp(HalfFieldAngle, MinimumBeamAngle, 89.0f);
		}
	}
}

void STSAVDMXFixtureBuilder::OnModeSelected(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo)
{
	SelectedMode = NewMode;
	if (SelectInfo != ESelectInfo::Direct)
	{
		AdoptSelectedModePhysicalProperties();
	}
}

TSharedRef<SWidget> STSAVDMXFixtureBuilder::GenerateModeWidget(TSharedPtr<FString> Mode) const
{
	return SNew(STextBlock).Text(Mode.IsValid() ? FText::FromString(*Mode) : FText::GetEmpty());
}

TArray<FDMXFixtureMode> STSAVDMXFixtureBuilder::BuildFixtureModes() const
{
	TArray<FDMXFixtureMode> Result;
	if (!GDTFSource.IsValid()) return Result;
	UDMXGDTF* GDTF = GDTFSource->LoadGDTF();
	const TSharedPtr<UE::DMX::GDTF::FDMXGDTFFixtureType> FixtureType = GDTF && GDTF->GetDescription().IsValid() ? GDTF->GetDescription()->GetFixtureType() : nullptr;
	if (!FixtureType.IsValid()) return Result;

	for (const TSharedPtr<UE::DMX::GDTF::FDMXGDTFDMXMode>& GDTFMode : FixtureType->DMXModes)
	{
		if (!GDTFMode.IsValid()) continue;
		FDMXFixtureMode Mode;
		Mode.ModeName = GDTFMode->Name.ToString();
		TMap<FName, int32> AttributeCounts;
		for (const TSharedPtr<UE::DMX::GDTF::FDMXGDTFDMXChannel>& Channel : GDTFMode->DMXChannels)
		{
			if (!Channel.IsValid() || Channel->Offset.IsEmpty() || Channel->LogicalChannelArray.IsEmpty() || !Channel->LogicalChannelArray[0].IsValid()) continue;
			const FName SourceAttribute = Channel->LogicalChannelArray[0]->Attribute;
			int32& Count = AttributeCounts.FindOrAdd(SourceAttribute);
			const FString AttributeString = Count == 0 ? SourceAttribute.ToString() : FString::Printf(TEXT("%s_%d"), *SourceAttribute.ToString(), Count + 1);
			++Count;
			const uint32 MinimumOffset = *Algo::MinElement(Channel->Offset);
			const uint32 MaximumOffset = *Algo::MaxElement(Channel->Offset);
			FDMXFixtureFunction Function;
			Function.Attribute = FDMXAttributeName(*AttributeString);
			Function.FunctionName = AttributeString;
			Function.Channel = FMath::Clamp(static_cast<int32>(MinimumOffset), 1, 512);
			Function.DataType = static_cast<EDMXFixtureSignalFormat>(FMath::Clamp(static_cast<int32>(MaximumOffset - MinimumOffset), 0, 3));
			Function.bUseLSBMode = Channel->Offset.Num() > 1 && Channel->Offset[0] > Channel->Offset[1];
			if (const TSharedPtr<UE::DMX::GDTF::FDMXGDTFChannelFunction> ChannelFunction = Channel->ResolveInitialFunction())
			{
				Function.SetPhysicalValueRange(ChannelFunction->PhysicalFrom, ChannelFunction->PhysicalTo);
			}
			Mode.Functions.Add(Function);
		}
		Mode.bAutoChannelSpan = true;
		Result.Add(MoveTemp(Mode));
	}
	return Result;
}

int32 STSAVDMXFixtureBuilder::GetSelectedModeIndex() const
{
	if (!SelectedMode.IsValid()) return 0;
	for (int32 Index = 0; Index < ModeOptions.Num(); ++Index) if (ModeOptions[Index].IsValid() && *ModeOptions[Index] == *SelectedMode) return Index;
	return 0;
}

int32 STSAVDMXFixtureBuilder::GetSelectedModeChannelSpan() const
{
	const int32 Index = GetSelectedModeIndex();
	if (!ParsedModes.IsValidIndex(Index)) return 0;
	int32 Span = 0;
	for (const FDMXFixtureFunction& Function : ParsedModes[Index].Functions) Span = FMath::Max(Span, Function.GetLastChannel());
	return Span;
}

bool STSAVDMXFixtureBuilder::CanCreateFixture() const
{
	const int32 Span = GetSelectedModeChannelSpan();
	return GDTFSource.IsValid() && (BaseMesh.IsValid() || YokeMesh.IsValid() || HeadMesh.IsValid() || LensMesh.IsValid()) && Span > 0 && Address >= 1 && Address + Span - 1 <= 512 && PanMin < PanMax && TiltMin < TiltMax;
}

FText STSAVDMXFixtureBuilder::GetValidationText() const
{
	if (!GDTFSource.IsValid()) return LOCTEXT("NeedGDTF", "Import or select a GDTF fixture definition.");
	if (!BaseMesh.IsValid() && !YokeMesh.IsValid() && !HeadMesh.IsValid() && !LensMesh.IsValid()) return LOCTEXT("NeedMesh", "Import or select at least one Static Mesh.");
	const int32 Span = GetSelectedModeChannelSpan();
	if (Span <= 0) return LOCTEXT("NeedChannels", "The selected GDTF mode has no usable DMX channels.");
	if (Address + Span - 1 > 512) return FText::Format(LOCTEXT("PatchOverflow", "Patch does not fit: address {0} plus {1} channels exceeds 512."), FText::AsNumber(Address), FText::AsNumber(Span));
	if (PanMin >= PanMax || TiltMin >= TiltMax) return LOCTEXT("BadRange", "Motion minimum values must be below maximum values.");
	return FText::Format(LOCTEXT("ValidPatch", "Ready: Universe {0}, addresses {1}–{2}, {3} channels."), FText::AsNumber(Universe), FText::AsNumber(Address), FText::AsNumber(Address + Span - 1), FText::AsNumber(Span));
}

FText STSAVDMXFixtureBuilder::GetModeSummary() const
{
	const int32 Index = GetSelectedModeIndex();
	if (!ParsedModes.IsValidIndex(Index)) return LOCTEXT("NoModeSummary", "Select a valid GDTF to inspect its modes and attributes.");
	TArray<FString> Names;
	for (const FDMXFixtureFunction& Function : ParsedModes[Index].Functions) Names.Add(Function.Attribute.Name.ToString());
	return FText::Format(LOCTEXT("ModeSummary", "{0} channels • Attributes: {1}"), FText::AsNumber(GetSelectedModeChannelSpan()), FText::FromString(FString::Join(Names, TEXT(", "))));
}

UDMXEntityFixturePatch* STSAVDMXFixtureBuilder::CreateDMXLibraryAndPatch(const FString& CleanFixtureName, FString& OutLibraryPath)
{
	ParsedModes = BuildFixtureModes();
	if (ParsedModes.IsEmpty())
	{
		return nullptr;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	FString UniquePackageName;
	FString UniqueAssetName;
	AssetTools.CreateUniqueAssetName(TEXT("/Game/TSAV/Fixtures/DMX/DMX_") + CleanFixtureName, TEXT(""), UniquePackageName, UniqueAssetName);
	UPackage* Package = CreatePackage(*UniquePackageName);
	if (!Package)
	{
		return nullptr;
	}

	UDMXLibrary* Library = NewObject<UDMXLibrary>(Package, *UniqueAssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Library)
	{
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Library);

	FDMXEntityFixtureTypeConstructionParams TypeParams;
	TypeParams.ParentDMXLibrary = Library;
	TypeParams.Modes = ParsedModes;
	UDMXEntityFixtureType* FixtureType = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(TypeParams, CleanFixtureName + TEXT(" Type"), true);
	if (!FixtureType)
	{
		return nullptr;
	}
	FixtureType->GDTFSource = GDTFSource.Get();
	for (int32 ModeIndex = 0; ModeIndex < FixtureType->Modes.Num(); ++ModeIndex)
	{
		FixtureType->UpdateChannelSpan(ModeIndex);
	}

	FDMXEntityFixturePatchConstructionParams PatchParams;
	PatchParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureType);
	PatchParams.ActiveMode = FMath::Clamp(GetSelectedModeIndex(), 0, FixtureType->Modes.Num() - 1);
	PatchParams.UniverseID = Universe;
	PatchParams.StartingAddress = Address;
	UDMXEntityFixturePatch* Patch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams, CleanFixtureName, true);
	if (!Patch)
	{
		return nullptr;
	}
	Patch->bReceiveDMXInEditor = true;
	Patch->RebuildCache();
	Library->MarkPackageDirty();
	FixtureType->MarkPackageDirty();
	Patch->MarkPackageDirty();
	TArray<UPackage*> PackagesToSave{ Package };
	UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
	OutLibraryPath = Library->GetPathName();
	return Patch;
}

void STSAVDMXFixtureBuilder::UpdateExistingPatch(UDMXEntityFixturePatch& Patch)
{
	UDMXEntityFixtureType* FixtureType = Patch.GetFixtureType();
	if (FixtureType)
	{
		FixtureType->Modify();
		FixtureType->Modes = BuildFixtureModes();
		FixtureType->GDTFSource = GDTFSource.Get();
		for (int32 ModeIndex = 0; ModeIndex < FixtureType->Modes.Num(); ++ModeIndex)
		{
			FixtureType->UpdateChannelSpan(ModeIndex);
		}
		UDMXEntityFixtureType::GetOnFixtureTypeChanged().Broadcast(FixtureType);
	}

	Patch.Modify();
	Patch.SetUniverseID(Universe);
	Patch.SetStartingChannel(Address);
	Patch.SetActiveModeIndex(GetSelectedModeIndex());
	Patch.bReceiveDMXInEditor = true;
	Patch.RebuildCache();
	if (UDMXLibrary* Library = Patch.GetParentLibrary())
	{
		Library->MarkPackageDirty();
		TArray<UPackage*> PackagesToSave{ Library->GetPackage() };
		UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
	}
}

void STSAVDMXFixtureBuilder::ApplySettings(ATSAVDMXFixture& Fixture, UDMXEntityFixturePatch* Patch) const
{
	Fixture.GDTFSource = GDTFSource.Get();
	Fixture.GDTFModeName = SelectedMode.IsValid() ? *SelectedMode : FString();
	Fixture.BaseMesh = BaseMesh.Get();
	Fixture.YokeMesh = YokeMesh.Get();
	Fixture.HeadMesh = HeadMesh.Get();
	Fixture.LensMesh = LensMesh.Get();
	Fixture.FixtureScale = FMath::Max(FixtureScale, 0.001f);
	Fixture.ModelRotation = ModelRotation;
	Fixture.PanPivotOffset = PanPivotOffset;
	Fixture.TiltPivotOffset = TiltPivotOffset;
	Fixture.LensOffset = LensOffset;
	Fixture.LensMeshRotation = LensMeshRotation;
	Fixture.BeamRotation = BeamRotation;
	Fixture.PanMinDegrees = PanMin;
	Fixture.PanMaxDegrees = PanMax;
	Fixture.TiltMinDegrees = TiltMin;
	Fixture.TiltMaxDegrees = TiltMax;
	Fixture.PanOffsetDegrees = PanOffset;
	Fixture.TiltOffsetDegrees = TiltOffset;
	Fixture.PanSpeedDegreesPerSecond = FMath::Max(PanSpeed, 0.0f);
	Fixture.TiltSpeedDegreesPerSecond = FMath::Max(TiltSpeed, 0.0f);
	Fixture.bInvertPan = bInvertPan;
	Fixture.bInvertTilt = bInvertTilt;
	Fixture.PreviewPan = PreviewPan;
	Fixture.PreviewTilt = PreviewTilt;
	Fixture.PreviewDimmer = 1.0f;
	Fixture.MaximumIntensityLumens = FMath::Max(MaximumIntensity, 0.0f);
	Fixture.MinimumBeamAngleDegrees = MinimumBeamAngle;
	Fixture.MaximumBeamAngleDegrees = MaximumBeamAngle;
	Fixture.AttenuationRadiusCm = FMath::Max(AttenuationRadius, 1.0f);
	Fixture.SetFixturePatch(Patch);
	Fixture.RerunConstructionScripts();
	Fixture.ApplyPreviewValues();
	Fixture.MarkPackageDirty();
}

FReply STSAVDMXFixtureBuilder::CreateFixture()
{
	if (!CanCreateFixture())
	{
		SetStatus(GetValidationText(), false);
		return FReply::Handled();
	}

	const FString CleanName = ObjectTools::SanitizeObjectName(FixtureName.IsEmpty() ? TEXT("DMX_Fixture") : FixtureName);
	FString LibraryPath;
	UDMXEntityFixturePatch* Patch = CreateDMXLibraryAndPatch(CleanName, LibraryPath);
	if (!Patch)
	{
		SetStatus(LOCTEXT("PatchCreateFailed", "Unreal could not create the DMX Library fixture type and patch."), false);
		return FReply::Handled();
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		SetStatus(LOCTEXT("NoWorld", "Open a level before creating the fixture actor."), false);
		return FReply::Handled();
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (GCurrentLevelEditingViewportClient)
	{
		SpawnLocation = GCurrentLevelEditingViewportClient->GetViewLocation() + GCurrentLevelEditingViewportClient->GetViewRotation().Vector() * 500.0f;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateFixtureTransaction", "Create TSAV GDTF DMX Fixture"));
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = World->GetCurrentLevel();
	ATSAVDMXFixture* Fixture = World->SpawnActor<ATSAVDMXFixture>(ATSAVDMXFixture::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	if (!Fixture)
	{
		SetStatus(LOCTEXT("ActorSpawnFailed", "The DMX assets were created, but the fixture actor could not be spawned."), false);
		return FReply::Handled();
	}

	Fixture->SetActorLabel(FixtureName.IsEmpty() ? CleanName : FixtureName);
	Fixture->SetFolderPath(FName(TEXT("TSAV DMX Fixtures")));
	ApplySettings(*Fixture, Patch);
	ActiveFixture = Fixture;
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Fixture, true, true);
	GEditor->MoveViewportCamerasToActor(*Fixture, true);
	SetStatus(FText::Format(LOCTEXT("FixtureCreated", "Created {0}, patched to Universe {1} Address {2}. DMX Library: {3}"), FText::FromString(Fixture->GetActorLabel()), FText::AsNumber(Universe), FText::AsNumber(Address), FText::FromString(LibraryPath)), true);
	return FReply::Handled();
}

FReply STSAVDMXFixtureBuilder::UpdateFixture()
{
	ATSAVDMXFixture* Fixture = ActiveFixture.Get();
	if (!Fixture) Fixture = FindSelectedFixture();
	if (!Fixture)
	{
		SetStatus(LOCTEXT("NoFixtureToUpdate", "Select or load a TSAV GDTF DMX Fixture first."), false);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("UpdateFixtureTransaction", "Update TSAV GDTF DMX Fixture"));
	Fixture->Modify();
	UDMXEntityFixturePatch* Patch = Fixture->GetFixturePatch();
	FString NewLibraryPath;
	if (Patch)
	{
		UpdateExistingPatch(*Patch);
	}
	else
	{
		Patch = CreateDMXLibraryAndPatch(ObjectTools::SanitizeObjectName(FixtureName), NewLibraryPath);
	}
	if (!Patch)
	{
		SetStatus(LOCTEXT("UpdatePatchFailed", "The fixture's DMX patch could not be updated or recreated."), false);
		return FReply::Handled();
	}

	Fixture->SetActorLabel(FixtureName.IsEmpty() ? Fixture->GetActorLabel() : FixtureName);
	ApplySettings(*Fixture, Patch);
	ActiveFixture = Fixture;
	SetStatus(FText::Format(LOCTEXT("FixtureUpdated", "Updated {0} and its DMX patch."), FText::FromString(Fixture->GetActorLabel())), true);
	return FReply::Handled();
}

FReply STSAVDMXFixtureBuilder::LoadSelectedFixture()
{
	ATSAVDMXFixture* Fixture = FindSelectedFixture();
	if (!Fixture)
	{
		SetStatus(LOCTEXT("NoFixtureSelected", "Select a TSAV GDTF DMX Fixture actor in the level first."), false);
		return FReply::Handled();
	}

	ActiveFixture = Fixture;
	FixtureName = Fixture->GetActorLabel();
	GDTFSource = Fixture->GDTFSource;
	BaseMesh = Fixture->BaseMesh;
	YokeMesh = Fixture->YokeMesh;
	HeadMesh = Fixture->HeadMesh;
	LensMesh = Fixture->LensMesh;
	FixtureScale = Fixture->FixtureScale;
	ModelRotation = Fixture->ModelRotation;
	PanPivotOffset = Fixture->PanPivotOffset;
	TiltPivotOffset = Fixture->TiltPivotOffset;
	LensOffset = Fixture->LensOffset;
	LensMeshRotation = Fixture->LensMeshRotation;
	BeamRotation = Fixture->BeamRotation;
	PanMin = Fixture->PanMinDegrees;
	PanMax = Fixture->PanMaxDegrees;
	TiltMin = Fixture->TiltMinDegrees;
	TiltMax = Fixture->TiltMaxDegrees;
	PanOffset = Fixture->PanOffsetDegrees;
	TiltOffset = Fixture->TiltOffsetDegrees;
	PanSpeed = Fixture->PanSpeedDegreesPerSecond;
	TiltSpeed = Fixture->TiltSpeedDegreesPerSecond;
	bInvertPan = Fixture->bInvertPan;
	bInvertTilt = Fixture->bInvertTilt;
	PreviewPan = Fixture->PreviewPan;
	PreviewTilt = Fixture->PreviewTilt;
	MaximumIntensity = Fixture->MaximumIntensityLumens;
	MinimumBeamAngle = Fixture->MinimumBeamAngleDegrees;
	MaximumBeamAngle = Fixture->MaximumBeamAngleDegrees;
	AttenuationRadius = Fixture->AttenuationRadiusCm;

	RefreshGDTFModes(false);
	if (UDMXEntityFixturePatch* Patch = Fixture->GetFixturePatch())
	{
		Universe = Patch->GetUniverseID();
		Address = Patch->GetStartingChannel();
		if (!ModeOptions.IsEmpty())
		{
			const int32 ModeIndex = FMath::Clamp(Patch->GetActiveModeIndex(), 0, ModeOptions.Num() - 1);
			SelectedMode = ModeOptions[ModeIndex];
			if (ModeCombo.IsValid()) ModeCombo->SetSelectedItem(SelectedMode);
		}
	}
	SetStatus(FText::Format(LOCTEXT("FixtureLoaded", "Loaded {0}. Change values, preview, or update the fixture."), FText::FromString(FixtureName)), true);
	return FReply::Handled();
}

FReply STSAVDMXFixtureBuilder::PreviewSelectedFixture()
{
	ATSAVDMXFixture* Fixture = ActiveFixture.Get();
	if (!Fixture) Fixture = FindSelectedFixture();
	if (!Fixture)
	{
		SetStatus(LOCTEXT("NoPreviewFixture", "Create or select a TSAV GDTF DMX Fixture to preview."), false);
		return FReply::Handled();
	}
	Fixture->Modify();
	ApplySettings(*Fixture, Fixture->GetFixturePatch());
	SetStatus(LOCTEXT("PreviewApplied", "Applied the configured preview pan, tilt, and full-intensity beam. Incoming DMX will take control when received."), true);
	return FReply::Handled();
}

ATSAVDMXFixture* STSAVDMXFixtureBuilder::FindSelectedFixture() const
{
	if (!GEditor || !GEditor->GetSelectedActors()) return nullptr;
	for (FSelectionIterator Iterator(*GEditor->GetSelectedActors()); Iterator; ++Iterator)
	{
		if (ATSAVDMXFixture* Fixture = Cast<ATSAVDMXFixture>(*Iterator)) return Fixture;
	}
	return nullptr;
}

FText STSAVDMXFixtureBuilder::GetSelectionStatus() const
{
	if (ActiveFixture.IsValid()) return FText::Format(LOCTEXT("EditingFixture", "Editing: {0}"), FText::FromString(ActiveFixture->GetActorLabel()));
	if (const ATSAVDMXFixture* Fixture = FindSelectedFixture()) return FText::Format(LOCTEXT("FixtureSelected", "Selected fixture: {0}. Click Load Selected Fixture to edit it here."), FText::FromString(Fixture->GetActorLabel()));
	return LOCTEXT("NoFixtureSelection", "No fixture loaded. Follow the five sections below to create one.");
}

void STSAVDMXFixtureBuilder::SetStatus(const FText& Message, bool bSuccess)
{
	StatusMessage = Message;
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
