// Copyright TSAV. All Rights Reserved.

#include "STSAVDMXFixtureBuilder.h"

#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"
#include "AssetImportTask.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "DMXGDTF.h"
#include "DMXZipper.h"
#include "Components/SpotLightComponent.h"
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
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeManager.h"
#include "InterchangeProjectSettings.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"
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
#include "PreviewScene.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "TSAVDMXFixture.h"
#include "TSAVDMXFixtureCatalog.h"
#include "UObject/UnrealType.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogTSAVGDTFBatch, Log, All);

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

	bool ConfigureLibraryPorts(UDMXLibrary& Library)
	{
		FStructProperty* PortReferencesProperty = FindFProperty<FStructProperty>(
			UDMXLibrary::StaticClass(), UDMXLibrary::GetPortReferencesPropertyName());
		FDMXLibraryPortReferences* PortReferences = PortReferencesProperty
			? PortReferencesProperty->ContainerPtrToValuePtr<FDMXLibraryPortReferences>(&Library)
			: nullptr;
		if (!PortReferences)
		{
			return false;
		}

		const TArray<FDMXInputPortSharedRef>& InputPorts = FDMXPortManager::Get().GetInputPorts();
		const TArray<FDMXOutputPortSharedRef>& OutputPorts = FDMXPortManager::Get().GetOutputPorts();
		if (InputPorts.IsEmpty() || OutputPorts.IsEmpty())
		{
			return false;
		}

		PortReferences->InputPortReferences.Reset(InputPorts.Num());
		for (const FDMXInputPortSharedRef& Port : InputPorts)
		{
			PortReferences->InputPortReferences.Emplace(Port->GetPortGuid(), true);
		}
		PortReferences->OutputPortReferences.Reset(OutputPorts.Num());
		for (const FDMXOutputPortSharedRef& Port : OutputPorts)
		{
			PortReferences->OutputPortReferences.Emplace(Port->GetPortGuid(), true);
		}
		Library.UpdatePorts();
		return true;
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
		if (Extension == TEXT("3ds"))
		{
			return 2;
		}
		return MAX_int32;
	}

	struct FLegacy3DSObject
	{
		FString Name;
		TArray<FVector3f> Vertices;
		TArray<FIntVector> Faces;
	};

	/** Converts the mesh subset of a legacy 3DS file into an OBJ that Interchange can import as a static mesh. */
	bool ConvertLegacy3DSToOBJ(const TArray64<uint8>& Data, const FString& OutputPath)
	{
		if (Data.Num() < 6)
		{
			return false;
		}

		auto ReadUInt16 = [&Data](const int64 Offset, uint16& OutValue)
		{
			if (Offset < 0 || Offset + static_cast<int64>(sizeof(uint16)) > Data.Num()) return false;
			FMemory::Memcpy(&OutValue, Data.GetData() + Offset, sizeof(uint16));
			return true;
		};
		auto ReadUInt32 = [&Data](const int64 Offset, uint32& OutValue)
		{
			if (Offset < 0 || Offset + static_cast<int64>(sizeof(uint32)) > Data.Num()) return false;
			FMemory::Memcpy(&OutValue, Data.GetData() + Offset, sizeof(uint32));
			return true;
		};
		auto ReadFloat = [&Data](const int64 Offset, float& OutValue)
		{
			if (Offset < 0 || Offset + static_cast<int64>(sizeof(float)) > Data.Num()) return false;
			FMemory::Memcpy(&OutValue, Data.GetData() + Offset, sizeof(float));
			return FMath::IsFinite(OutValue);
		};

		TArray<FLegacy3DSObject> Objects;
		TFunction<void(int64, int64, FLegacy3DSObject*, int32)> ParseChunks;
		ParseChunks = [&](int64 Cursor, const int64 End, FLegacy3DSObject* CurrentObject, const int32 Depth)
		{
			if (Depth > 32) return;
			while (Cursor + 6 <= End && Cursor + 6 <= Data.Num())
			{
				uint16 ChunkId = 0;
				uint32 ChunkLength = 0;
				if (!ReadUInt16(Cursor, ChunkId) || !ReadUInt32(Cursor + 2, ChunkLength) || ChunkLength < 6)
				{
					return;
				}
				const int64 ChunkEnd = FMath::Min3(Cursor + static_cast<int64>(ChunkLength), End, Data.Num());
				if (ChunkEnd <= Cursor + 6)
				{
					return;
				}
				const int64 Payload = Cursor + 6;

				switch (ChunkId)
				{
				case 0x4D4D: // Main file
				case 0x3D3D: // 3D editor
				case 0x4100: // Triangular mesh
					ParseChunks(Payload, ChunkEnd, CurrentObject, Depth + 1);
					break;
				case 0x4000: // Named object
				{
					int64 NameEnd = Payload;
					FString ObjectName;
					while (NameEnd < ChunkEnd && Data[NameEnd] != 0)
					{
						const uint8 Character = Data[NameEnd++];
						ObjectName.AppendChar(Character >= 32 && Character < 127 ? static_cast<TCHAR>(Character) : TEXT('_'));
					}
					if (NameEnd < ChunkEnd) ++NameEnd;
					FLegacy3DSObject& Object = Objects.AddDefaulted_GetRef();
					Object.Name = ObjectName.IsEmpty() ? FString::Printf(TEXT("Object_%d"), Objects.Num()) : ObjectName;
					ParseChunks(NameEnd, ChunkEnd, &Object, Depth + 1);
					break;
				}
				case 0x4110: // Vertex list
					if (CurrentObject)
					{
						uint16 VertexCount = 0;
						if (ReadUInt16(Payload, VertexCount) && Payload + 2 + static_cast<int64>(VertexCount) * 12 <= ChunkEnd)
						{
							CurrentObject->Vertices.Reset(VertexCount);
							for (uint16 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
							{
								const int64 VertexOffset = Payload + 2 + static_cast<int64>(VertexIndex) * 12;
								float X = 0.0f, Y = 0.0f, Z = 0.0f;
								if (!ReadFloat(VertexOffset, X) || !ReadFloat(VertexOffset + 4, Y) || !ReadFloat(VertexOffset + 8, Z)) break;
								CurrentObject->Vertices.Emplace(X, Y, Z);
							}
						}
					}
					break;
				case 0x4120: // Face list
					if (CurrentObject)
					{
						uint16 FaceCount = 0;
						if (ReadUInt16(Payload, FaceCount) && Payload + 2 + static_cast<int64>(FaceCount) * 8 <= ChunkEnd)
						{
							CurrentObject->Faces.Reset(FaceCount);
							for (uint16 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
							{
								const int64 FaceOffset = Payload + 2 + static_cast<int64>(FaceIndex) * 8;
								uint16 A = 0, B = 0, C = 0;
								if (!ReadUInt16(FaceOffset, A) || !ReadUInt16(FaceOffset + 2, B) || !ReadUInt16(FaceOffset + 4, C)) break;
								CurrentObject->Faces.Emplace(A, B, C);
							}
						}
					}
					break;
				default:
					break;
				}
				Cursor += ChunkLength;
			}
		};

		ParseChunks(0, Data.Num(), nullptr, 0);
		FString OBJ;
		// Unreal 5.8's OBJ translator expects a valid UV index whenever it builds
		// vertex instances. Legacy 3DS fixture geometry often has no texture
		// coordinates, so give every face a harmless shared UV instead of leaving
		// the translator with an invalid index.
		OBJ += TEXT("vt 0 0\n");
		int32 GlobalVertexOffset = 0;
		int32 ValidObjectCount = 0;
		for (const FLegacy3DSObject& Object : Objects)
		{
			if (Object.Vertices.IsEmpty() || Object.Faces.IsEmpty()) continue;
			OBJ += FString::Printf(TEXT("o %s\n"), *ObjectTools::SanitizeObjectName(Object.Name));
			for (const FVector3f& Vertex : Object.Vertices)
			{
				OBJ += FString::Printf(TEXT("v %.9g %.9g %.9g\n"), Vertex.X, Vertex.Y, Vertex.Z);
			}
			for (const FIntVector& Face : Object.Faces)
			{
				if (Face.X < 0 || Face.Y < 0 || Face.Z < 0 || Face.X >= Object.Vertices.Num() || Face.Y >= Object.Vertices.Num() || Face.Z >= Object.Vertices.Num()) continue;
				// 3DS faces are clockwise; reverse the last two indices for OBJ's winding.
				OBJ += FString::Printf(TEXT("f %d/1 %d/1 %d/1\n"), GlobalVertexOffset + Face.X + 1, GlobalVertexOffset + Face.Z + 1, GlobalVertexOffset + Face.Y + 1);
			}
			GlobalVertexOffset += Object.Vertices.Num();
			++ValidObjectCount;
		}

		return ValidObjectCount > 0 && FFileHelper::SaveStringToFile(OBJ, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FTransform ConvertParsedGDTFTransformForImportedGLTF(const FTransform& ParsedTransform)
	{
		// Unreal's GDTF parser and glTF Interchange importer each swap Y/Z independently.
		// Undo the parser's swap so geometry transforms use the same XYZ frame as the
		// imported mesh: GDTF X/Y/Z maps to the imported mesh's Unreal X/Y/Z.
		const FMatrix AxisSwap(
			FPlane(1.0, 0.0, 0.0, 0.0),
			FPlane(0.0, 0.0, 1.0, 0.0),
			FPlane(0.0, 1.0, 0.0, 0.0),
			FPlane(0.0, 0.0, 0.0, 1.0));
		return FTransform(AxisSwap * ParsedTransform.ToMatrixWithScale() * AxisSwap);
	}

	FVector GetImportedMeshScale(const UStaticMesh& Mesh, const UE::DMX::GDTF::FDMXGDTFModel& Model)
	{
		// GDTF model dimensions are authoritative even when the embedded GLB was
		// exported in millimeters or with another authoring scale.
		const FVector ImportedSize = Mesh.GetBoundingBox().GetSize();
		const FVector DeclaredSize(Model.Length, Model.Width, Model.Height);
		constexpr float MetersToCentimeters = 100.0f;
		if (ImportedSize.X <= UE_SMALL_NUMBER || ImportedSize.Y <= UE_SMALL_NUMBER || ImportedSize.Z <= UE_SMALL_NUMBER ||
			DeclaredSize.X <= UE_SMALL_NUMBER || DeclaredSize.Y <= UE_SMALL_NUMBER || DeclaredSize.Z <= UE_SMALL_NUMBER)
		{
			return FVector::OneVector;
		}

		const FVector Scale(
			DeclaredSize.X * MetersToCentimeters / ImportedSize.X,
			DeclaredSize.Y * MetersToCentimeters / ImportedSize.Y,
			DeclaredSize.Z * MetersToCentimeters / ImportedSize.Z);
		// Avoid a zero-volume primitive in Chaos when a producer declares a
		// microscopically thin decorative model dimension.
		return FVector(
			FMath::Clamp(Scale.X, 0.001, 1000.0),
			FMath::Clamp(Scale.Y, 0.001, 1000.0),
			FMath::Clamp(Scale.Z, 0.001, 1000.0));
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

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step2", "2"), LOCTEXT("ModelTitle", "3D Model"), LOCTEXT("ModelHelp", "Embedded GDTF glTF/GLB models are assigned and scaled to their declared dimensions automatically. Use manual import only as a fallback. Separate Base, Yoke, Head, and Lens meshes provide accurate articulation."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f)[SNew(SButton).Text(LOCTEXT("ImportModel", "Import 3D Model…")).HAlign(HAlign_Center).OnClicked(this, &STSAVDMXFixtureBuilder::ImportModel)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("BaseMesh", "Base"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return BaseMesh.IsValid()?BaseMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnBaseMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("YokeMesh", "Yoke"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return YokeMesh.IsValid()?YokeMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnYokeMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("HeadMesh", "Head / full model"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return HeadMesh.IsValid()?HeadMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnHeadMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,0.0f,8.0f,0.0f)[SNew(STextBlock).Text(LOCTEXT("LensMesh", "Lens / beam"))]+SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SObjectPropertyEntryBox).AllowedClass(UStaticMesh::StaticClass()).ObjectPath_Lambda([this](){return LensMesh.IsValid()?LensMesh->GetPathName():FString();}).OnObjectChanged(this,&STSAVDMXFixtureBuilder::OnLensMeshChanged).DisplayUseSelected(true).DisplayBrowse(true)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f)[MakeFloatField(LOCTEXT("Scale", "Fixture scale"), &FixtureScale, 0.001f, 100.0f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[MakeRotationField(LOCTEXT("ModelRotation", "Model axis correction"), &ModelRotation)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[MakeVectorField(LOCTEXT("BaseMeshScale", "Base GDTF scale"), &BaseMeshScale)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[MakeVectorField(LOCTEXT("YokeMeshScale", "Yoke GDTF scale"), &YokeMeshScale)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[MakeVectorField(LOCTEXT("HeadMeshScale", "Head GDTF scale"), &HeadMeshScale)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 14.0f)[MakeVectorField(LOCTEXT("LensMeshScale", "Lens GDTF scale"), &LensMeshScale)]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step3", "3"), LOCTEXT("MotionTitle", "Pan, Tilt & Pivots"), LOCTEXT("MotionHelp", "Set the real motion limits, home offsets, direction, speed, and the pivot positions used by the imported model."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[SNew(SGridPanel).FillColumn(0,1.0f).FillColumn(1,1.0f)+SGridPanel::Slot(0,0).Padding(0.0f,3.0f,8.0f,3.0f)[MakeSignedFloatField(LOCTEXT("PanMin","Pan minimum °"),&PanMin)]+SGridPanel::Slot(1,0).Padding(8.0f,3.0f,0.0f,3.0f)[MakeSignedFloatField(LOCTEXT("PanMax","Pan maximum °"),&PanMax)]+SGridPanel::Slot(0,1).Padding(0.0f,3.0f,8.0f,3.0f)[MakeSignedFloatField(LOCTEXT("TiltMin","Tilt minimum °"),&TiltMin)]+SGridPanel::Slot(1,1).Padding(8.0f,3.0f,0.0f,3.0f)[MakeSignedFloatField(LOCTEXT("TiltMax","Tilt maximum °"),&TiltMax)]+SGridPanel::Slot(0,2).Padding(0.0f,3.0f,8.0f,3.0f)[MakeSignedFloatField(LOCTEXT("PanOffset","Pan home offset °"),&PanOffset)]+SGridPanel::Slot(1,2).Padding(8.0f,3.0f,0.0f,3.0f)[MakeSignedFloatField(LOCTEXT("TiltOffset","Tilt home offset °"),&TiltOffset)]+SGridPanel::Slot(0,3).Padding(0.0f,3.0f,8.0f,3.0f)[MakeFloatField(LOCTEXT("PanSpeed","Pan speed °/s"),&PanSpeed,0.0f,10000.0f)]+SGridPanel::Slot(1,3).Padding(8.0f,3.0f,0.0f,3.0f)[MakeFloatField(LOCTEXT("TiltSpeed","Tilt speed °/s"),&TiltSpeed,0.0f,10000.0f)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsChecked_Lambda([this](){return bInvertPan?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState State){bInvertPan=State==ECheckBoxState::Checked;})[SNew(STextBlock).Text(LOCTEXT("InvertPan","Invert pan"))]]+SHorizontalBox::Slot().AutoWidth().Padding(24.0f,0.0f)[SNew(SCheckBox).IsChecked_Lambda([this](){return bInvertTilt?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState State){bInvertTilt=State==ECheckBoxState::Checked;})[SNew(STextBlock).Text(LOCTEXT("InvertTilt","Invert tilt"))]]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeVectorField(LOCTEXT("PanPivot", "Pan pivot offset (cm)"), &PanPivotOffset)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeVectorField(LOCTEXT("TiltPivot", "Tilt pivot offset (cm)"), &TiltPivotOffset)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeRotationField(LOCTEXT("PanPivotRotation", "Pan pivot rest rotation"), &PanPivotRotation)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[MakeRotationField(LOCTEXT("TiltPivotRotation", "Tilt pivot rest rotation"), &TiltPivotRotation)]
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

	// Never carry assignments or per-model scale from the previously selected GDTF.
	BaseMesh.Reset();
	YokeMesh.Reset();
	HeadMesh.Reset();
	LensMesh.Reset();
	BaseMeshScale = FVector::OneVector;
	YokeMeshScale = FVector::OneVector;
	HeadMeshScale = FVector::OneVector;
	LensMeshScale = FVector::OneVector;

	const FString GltfPrefix = TEXT("models/gltf/");
	const FString Legacy3DSPrefix = TEXT("models/3ds/");
	TMap<FString, FString> BestArchiveByRelativeStem;
	TArray<FString> ModelResources;
	for (const FString& File : Zip->GetFiles())
	{
		const FString ArchivePath = NormalizeArchivePath(File);
		if (!IsSafeArchivePath(ArchivePath))
		{
			continue;
		}

		const bool bIsLegacy3DS = ArchivePath.StartsWith(Legacy3DSPrefix, ESearchCase::IgnoreCase) &&
			FPaths::GetExtension(ArchivePath).Equals(TEXT("3ds"), ESearchCase::IgnoreCase);
		const bool bIsGltf = ArchivePath.StartsWith(GltfPrefix, ESearchCase::IgnoreCase) &&
			(FPaths::GetExtension(ArchivePath).Equals(TEXT("glb"), ESearchCase::IgnoreCase) ||
			 FPaths::GetExtension(ArchivePath).Equals(TEXT("gltf"), ESearchCase::IgnoreCase));
		if (!bIsGltf && !bIsLegacy3DS)
		{
			continue;
		}

		ModelResources.Add(ArchivePath);
		const int32 Priority = GetEmbeddedModelPriority(ArchivePath);
		if (Priority == MAX_int32)
		{
			continue;
		}

		const int32 PrefixLength = bIsGltf ? GltfPrefix.Len() : Legacy3DSPrefix.Len();
		FString RelativeStem = FPaths::ChangeExtension(ArchivePath.Mid(PrefixLength), TEXT(""));
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
		OutResultMessage = LOCTEXT("NoEmbeddedModel", "No supported embedded glTF, GLB, or legacy 3DS model was found; a model can still be assigned manually.");
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
	for (const FString& ArchivePath : ModelResources)
	{
		TArray64<uint8> FileData;
		if (!Zip->GetFileContent(ArchivePath, FileData))
		{
			continue;
		}

		const bool bLegacy3DS = FPaths::GetExtension(ArchivePath).Equals(TEXT("3ds"), ESearchCase::IgnoreCase);
		const FString RelativeOutputPath = bLegacy3DS
			? FPaths::ChangeExtension(FString(TEXT("models/converted3ds/")) + ArchivePath.Mid(Legacy3DSPrefix.Len()), TEXT("obj"))
			: ArchivePath;
		const FString ExtractedPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ExtractionRoot, RelativeOutputPath));
		if (!ExtractedPath.StartsWith(ExtractionRoot, ESearchCase::IgnoreCase))
		{
			continue;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExtractedPath), true);
		const bool bSavedModel = bLegacy3DS
			? ConvertLegacy3DSToOBJ(FileData, ExtractedPath)
			: FFileHelper::SaveArrayToFile(FileData, *ExtractedPath);
		if (bSavedModel)
		{
			ExtractedPathByArchiveKey.Add(ArchivePath.ToLower(), ExtractedPath);
		}
	}

	TArray<UAssetImportTask*> ImportTasks;
	TMap<UAssetImportTask*, FString> ArchiveKeyByTask;
	TArray<UInterchangePipelineBase*> RootedImportPipelines;
	// Keep corrected static-only imports isolated from legacy skeletal import assets.
	// This is important for profiles whose old AnimSequence package cannot be loaded.
	const FString DestinationPath = TEXT("/Game/TSAV/Fixtures/Models/") + ObjectTools::SanitizeObjectName(GDTFSource->GetName()) + TEXT("/TSAVStaticGeometry");
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
		Task->bReplaceExistingSettings = true;

		// Fixture geometry is assembled by the fixture actor's base/yoke/head hierarchy.
		// Some otherwise valid GDTF GLBs also contain animated transforms or skin data;
		// force those meshes to remain static and discard their authored animation. This
		// avoids importing unusable skeletal assets and an Unreal 5.8 AnimSequence crash.
		const UInterchangeSourceData* SourceData = UInterchangeManager::CreateSourceData(*ExtractedPath);
		const FName PipelineStackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(false, *SourceData);
		const FInterchangeImportSettings& ImportSettings = FInterchangeProjectSettingsUtils::GetDefaultImportSettings(false);
		const FInterchangePipelineStack* DefaultStack = ImportSettings.PipelineStacks.Find(PipelineStackName);
		bool bConfiguredStaticPipeline = false;
		if (DefaultStack)
		{
			const TArray<FSoftObjectPath>* PipelinePaths = &DefaultStack->Pipelines;
			UE::Interchange::FScopedTranslator ScopedTranslator(SourceData);
			for (const FInterchangeTranslatorPipelines& TranslatorPipelines : DefaultStack->PerTranslatorPipelines)
			{
				const UClass* TranslatorClass = TranslatorPipelines.Translator.LoadSynchronous();
				if (ScopedTranslator.GetTranslator() && ScopedTranslator.GetTranslator()->IsA(TranslatorClass))
				{
					PipelinePaths = &TranslatorPipelines.Pipelines;
					break;
				}
			}

			UInterchangePipelineStackOverride* PipelineOverride = NewObject<UInterchangePipelineStackOverride>(Task);
			for (const FSoftObjectPath& PipelinePath : *PipelinePaths)
			{
				UInterchangePipelineBase* DefaultPipeline = Cast<UInterchangePipelineBase>(PipelinePath.TryLoad());
				UInterchangePipelineBase* GeneratedPipeline = DefaultPipeline ? UE::Interchange::GeneratePipelineInstance(PipelinePath) : nullptr;
				if (!GeneratedPipeline)
				{
					continue;
				}
				GeneratedPipeline->TransferAdjustSettings(DefaultPipeline);
				GeneratedPipeline->AddToRoot();
				RootedImportPipelines.Add(GeneratedPipeline);

				if (UInterchangeGenericAssetsPipeline* AssetsPipeline = Cast<UInterchangeGenericAssetsPipeline>(GeneratedPipeline))
				{
					AssetsPipeline->bAssetTypeSubFolders = true;
					if (AssetsPipeline->CommonMeshesProperties)
					{
						AssetsPipeline->CommonMeshesProperties->ForceAllMeshAsType = EInterchangeForceMeshType::IFMT_StaticMesh;
						AssetsPipeline->CommonMeshesProperties->bConvertStaticsWithAnimatedTransformToSkeletals = false;
						AssetsPipeline->CommonMeshesProperties->bConvertStaticsInBoneHierarchyToSkeletals = false;
					}
					if (AssetsPipeline->MeshPipeline)
					{
						AssetsPipeline->MeshPipeline->bImportStaticMeshes = true;
						AssetsPipeline->MeshPipeline->bImportSkeletalMeshes = false;
						// The catalog can contain more than a thousand small authored parts.
						// Per-part Nanite, Lumen-card, collision, and distance-field builds add
						// no useful detail here, and Unreal's asynchronous Embree distance-field
						// builder can crash while a large batch replaces meshes. Keep these
						// lightweight visual meshes on the conventional static-mesh path.
						AssetsPipeline->MeshPipeline->bBuildNanite = false;
						AssetsPipeline->MeshPipeline->DistanceFieldResolutionScale = 0.0f;
						AssetsPipeline->MeshPipeline->MaxLumenMeshCards = 0;
						AssetsPipeline->MeshPipeline->bCollision = false;
						// A GDTF Model.File is one physical fixture part even when the
						// source file contains hundreds of CAD sub-objects.
						AssetsPipeline->MeshPipeline->CombineStaticMeshesBehavior = EInterchangeCombineStaticMeshesBehavior::All;
					}
					if (AssetsPipeline->AnimationPipeline)
					{
						AssetsPipeline->AnimationPipeline->bImportAnimations = false;
					}
					bConfiguredStaticPipeline = true;
				}
				PipelineOverride->OverridePipelines.Add(GeneratedPipeline);
			}
			Task->Options = PipelineOverride;
		}

		if (!bConfiguredStaticPipeline)
		{
			UE_LOG(LogTSAVGDTFBatch, Error, TEXT("Skipping embedded model '%s': Unreal's static Interchange pipeline could not be configured safely."), **ExtractedPath);
			continue;
		}
		ImportTasks.Add(Task);
		ArchiveKeyByTask.Add(Task, Pair.Key);
	}

	if (ImportTasks.IsEmpty())
	{
		for (UInterchangePipelineBase* Pipeline : RootedImportPipelines)
		{
			if (Pipeline)
			{
				Pipeline->RemoveFromRoot();
			}
		}
		OutResultMessage = LOCTEXT("EmbeddedModelExtractionFailed", "Embedded model resources were found but could not be extracted safely.");
		return -1;
	}

	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks(ImportTasks);
	for (UInterchangePipelineBase* Pipeline : RootedImportPipelines)
	{
		if (Pipeline)
		{
			Pipeline->RemoveFromRoot();
		}
	}

	UStaticMesh* ImportedBase = nullptr;
	UStaticMesh* ImportedYoke = nullptr;
	UStaticMesh* ImportedHead = nullptr;
	UStaticMesh* ImportedLens = nullptr;
	FVector ImportedBaseScale = FVector::OneVector;
	FVector ImportedYokeScale = FVector::OneVector;
	FVector ImportedHeadScale = FVector::OneVector;
	FVector ImportedLensScale = FVector::OneVector;
	int32 ImportedBasePriority = MAX_int32;
	int32 ImportedYokePriority = MAX_int32;
	int32 ImportedHeadPriority = MAX_int32;
	int32 ImportedLensPriority = MAX_int32;
	TArray<UStaticMesh*> AllImportedMeshes;
	TSet<UStaticMesh*> AssignedMeshes;
	auto GetRoleForModel = [&RoleByModelName](const FDMXGDTFModel& Model)
	{
		if (const EFixtureMeshRole* GeometryRole = RoleByModelName.Find(Model.Name))
		{
			return *GeometryRole;
		}
		return GetMeshRole(Model);
	};
	auto AssignMesh = [&ImportedBase, &ImportedYoke, &ImportedHead, &ImportedLens, &ImportedBaseScale, &ImportedYokeScale, &ImportedHeadScale, &ImportedLensScale, &ImportedBasePriority, &ImportedYokePriority, &ImportedHeadPriority, &ImportedLensPriority, &AssignedMeshes](UStaticMesh* Mesh, EFixtureMeshRole Role, const FDMXGDTFModel* Model)
	{
		if (!Mesh)
		{
			return;
		}

		UStaticMesh** Destination = nullptr;
		FVector* DestinationScale = nullptr;
		int32* DestinationPriority = nullptr;
		switch (Role)
		{
		case EFixtureMeshRole::Base: Destination = &ImportedBase; DestinationScale = &ImportedBaseScale; DestinationPriority = &ImportedBasePriority; break;
		case EFixtureMeshRole::Yoke: Destination = &ImportedYoke; DestinationScale = &ImportedYokeScale; DestinationPriority = &ImportedYokePriority; break;
		case EFixtureMeshRole::Head: Destination = &ImportedHead; DestinationScale = &ImportedHeadScale; DestinationPriority = &ImportedHeadPriority; break;
		case EFixtureMeshRole::Lens: Destination = &ImportedLens; DestinationScale = &ImportedLensScale; DestinationPriority = &ImportedLensPriority; break;
		default: break;
		}
		const int32 Priority = GetMeshRolePriority(*Mesh, Role);
		if (Destination && DestinationScale && DestinationPriority && (!*Destination || Priority < *DestinationPriority))
		{
			AssignedMeshes.Remove(*Destination);
			*Destination = Mesh;
			*DestinationScale = Model ? GetImportedMeshScale(*Mesh, *Model) : FVector::OneVector;
			*DestinationPriority = Priority;
			AssignedMeshes.Add(Mesh);
		}
	};

	// Interchange can report transient pre-combine glTF subobjects from
	// UAssetImportTask::GetObjects() even though only the combined mesh package is
	// saved. Resolve the assets from the registry after import so the catalog can
	// never retain a soft path to one of those discarded subobjects.
	TArray<FAssetData> PersistedMeshAssetData;
	FAssetRegistryModule::GetRegistry().GetAssetsByPath(FName(*DestinationPath), PersistedMeshAssetData, true, false);
	TArray<UStaticMesh*> PersistedMeshes;
	for (const FAssetData& MeshAssetData : PersistedMeshAssetData)
	{
		const FString SavedPackageFilename = FPackageName::LongPackageNameToFilename(
			MeshAssetData.PackageName.ToString(), FPackageName::GetAssetPackageExtension());
		const bool bHasSavedPackage = FPaths::FileExists(SavedPackageFilename);
		if (bHasSavedPackage && MeshAssetData.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(MeshAssetData.GetAsset()))
			{
				PersistedMeshes.AddUnique(Mesh);
			}
		}
	}

	for (UAssetImportTask* Task : ImportTasks)
	{
		TArray<UStaticMesh*> TaskMeshes;
		const FString SourceStem = Canonicalize(FPaths::GetBaseFilename(Task->Filename));
		for (UStaticMesh* Mesh : PersistedMeshes)
		{
			const FString MeshName = Canonicalize(Mesh->GetName());
			if (MeshName == SourceStem)
			{
				TaskMeshes.AddUnique(Mesh);
				AllImportedMeshes.AddUnique(Mesh);
			}
		}
		const FString* ArchiveKey = ArchiveKeyByTask.Find(Task);
		const TArray<TSharedPtr<FDMXGDTFModel>>* LinkedModels = ArchiveKey ? ModelsByArchiveKey.Find(*ArchiveKey) : nullptr;
		for (int32 MeshIndex = 0; MeshIndex < TaskMeshes.Num(); ++MeshIndex)
		{
			UStaticMesh* Mesh = TaskMeshes[MeshIndex];
			EFixtureMeshRole Role = EFixtureMeshRole::None;
			const FDMXGDTFModel* MatchedModel = nullptr;

			if (LinkedModels)
			{
				const FString MeshName = Canonicalize(Mesh->GetName());
				for (const TSharedPtr<FDMXGDTFModel>& Model : *LinkedModels)
				{
					if (Model.IsValid() && MeshName.Contains(Canonicalize(Model->Name.ToString())))
					{
						MatchedModel = Model.Get();
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
				MatchedModel = (*LinkedModels)[MeshIndex].Get();
				Role = GetRoleForModel(*(*LinkedModels)[MeshIndex]);
			}

			if (Role == EFixtureMeshRole::None && LinkedModels && TaskMeshes.Num() == 1)
			{
				EFixtureMeshRole OnlyRole = EFixtureMeshRole::None;
				const FDMXGDTFModel* OnlyModel = nullptr;
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
					OnlyModel = Model.Get();
				}
				if (!bAmbiguousRole)
				{
					Role = OnlyRole;
					MatchedModel = OnlyModel;
				}
			}

			if (Role == EFixtureMeshRole::None)
			{
				Role = GetMeshRole(*Mesh);
			}
			if (!MatchedModel && LinkedModels)
			{
				for (const TSharedPtr<FDMXGDTFModel>& Model : *LinkedModels)
				{
					if (Model.IsValid() && GetRoleForModel(*Model) == Role)
					{
						MatchedModel = Model.Get();
						break;
					}
				}
			}

			AssignMesh(Mesh, Role, MatchedModel);
		}
	}

	if (AllImportedMeshes.IsEmpty())
	{
		OutResultMessage = LOCTEXT("EmbeddedModelImportFailed", "An embedded model was extracted, but Unreal did not create a Static Mesh from it.");
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
	BaseMeshScale = ImportedBaseScale;
	YokeMeshScale = ImportedYokeScale;
	HeadMeshScale = ImportedHeadScale;
	LensMeshScale = ImportedLensScale;
	OutResultMessage = FText::Format(
		LOCTEXT("EmbeddedModelImported", "Imported {0} embedded Static Mesh asset(s) and assigned the physical fixture parts automatically."),
		FText::AsNumber(AllImportedMeshes.Num()));
	return AllImportedMeshes.Num();
}

int32 STSAVDMXFixtureBuilder::AssignPrimitiveFallbackModels()
{
	using namespace TSAVDMXFixtureBuilder::Private;
	using namespace UE::DMX::GDTF;

	UStaticMesh* MovingBase = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Base.SM_MovingHead_Base"));
	UStaticMesh* MovingYoke = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Yoke.SM_MovingHead_Yoke"));
	UStaticMesh* MovingHead = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Head.SM_MovingHead_Head"));
	UStaticMesh* MovingLens = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Lens.SM_MovingHead_Lens"));
	UStaticMesh* StaticBody = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_Static_Base.SM_Static_Base"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	auto HasAttribute = [this](const TCHAR* Prefix)
	{
		for (const FDMXFixtureMode& Mode : ParsedModes)
		{
			for (const FDMXFixtureFunction& Function : Mode.Functions)
			{
				if (Canonicalize(Function.Attribute.Name.ToString()).StartsWith(Prefix))
				{
					return true;
				}
			}
		}
		return false;
	};
	const bool bHasPanTilt = HasAttribute(TEXT("pan")) || HasAttribute(TEXT("tilt"));

	auto PrimitiveMesh = [Cube, Cylinder, Sphere](const EDMXGDTFModelPrimitiveType PrimitiveType)
	{
		switch (PrimitiveType)
		{
		case EDMXGDTFModelPrimitiveType::Cylinder:
		case EDMXGDTFModelPrimitiveType::Conventional:
		case EDMXGDTFModelPrimitiveType::Conventional1_1:
		case EDMXGDTFModelPrimitiveType::Pigtail:
			return Cylinder ? Cylinder : Cube;
		case EDMXGDTFModelPrimitiveType::Sphere:
			return Sphere ? Sphere : Cube;
		default:
			return Cube;
		}
	};

	int32 NumAssigned = 0;
	auto AssignRole = [&NumAssigned](
		TWeakObjectPtr<UStaticMesh>& Destination,
		FVector& DestinationScale,
		UStaticMesh* PreferredMesh,
		const FDMXGDTFModel* Model)
	{
		if (Destination.IsValid() || !PreferredMesh)
		{
			return;
		}
		Destination = PreferredMesh;
		DestinationScale = Model ? GetImportedMeshScale(*PreferredMesh, *Model) : FVector::OneVector;
		++NumAssigned;
	};

	UDMXGDTF* GDTF = GDTFSource.IsValid() ? GDTFSource->LoadGDTF() : nullptr;
	const TSharedPtr<FDMXGDTFFixtureType> FixtureType = GDTF && GDTF->GetDescription().IsValid()
		? GDTF->GetDescription()->GetFixtureType()
		: nullptr;
	if (FixtureType.IsValid())
	{
		TMap<FName, EFixtureMeshRole> RoleByModelName;
		if (FixtureType->GeometryCollect.IsValid())
		{
			for (const TSharedPtr<FDMXGDTFGeometry>& Geometry : FixtureType->GeometryCollect->GeometryArray)
			{
				GatherGeometryModelRoles(Geometry, 0, RoleByModelName);
			}
			for (const TSharedPtr<FDMXGDTFAxisGeometry>& Axis : FixtureType->GeometryCollect->AxisArray)
			{
				GatherGeometryModelRoles(StaticCastSharedPtr<FDMXGDTFGeometry>(Axis), 1, RoleByModelName);
			}
		}

		for (const TSharedPtr<FDMXGDTFModel>& Model : FixtureType->Models)
		{
			if (!Model.IsValid())
			{
				continue;
			}
			const EFixtureMeshRole Role = RoleByModelName.Contains(Model->Name)
				? RoleByModelName[Model->Name]
				: GetMeshRole(*Model);
			UStaticMesh* ShapeMesh = PrimitiveMesh(Model->PrimitiveType);
			switch (Role)
			{
			case EFixtureMeshRole::Base:
				AssignRole(BaseMesh, BaseMeshScale, bHasPanTilt ? MovingBase : (StaticBody ? StaticBody : ShapeMesh), Model.Get());
				break;
			case EFixtureMeshRole::Yoke:
				AssignRole(YokeMesh, YokeMeshScale, MovingYoke ? MovingYoke : ShapeMesh, Model.Get());
				break;
			case EFixtureMeshRole::Head:
				AssignRole(HeadMesh, HeadMeshScale, bHasPanTilt ? (MovingHead ? MovingHead : ShapeMesh) : (StaticBody ? StaticBody : ShapeMesh), Model.Get());
				break;
			case EFixtureMeshRole::Lens:
				AssignRole(LensMesh, LensMeshScale, MovingLens ? MovingLens : ShapeMesh, Model.Get());
				break;
			default:
				break;
			}
		}
	}

	if (!BaseMesh.IsValid() && !YokeMesh.IsValid() && !HeadMesh.IsValid() && !LensMesh.IsValid())
	{
		if (bHasPanTilt)
		{
			AssignRole(BaseMesh, BaseMeshScale, MovingBase ? MovingBase : Cube, nullptr);
			AssignRole(YokeMesh, YokeMeshScale, MovingYoke ? MovingYoke : Cube, nullptr);
			AssignRole(HeadMesh, HeadMeshScale, MovingHead ? MovingHead : Cylinder, nullptr);
			AssignRole(LensMesh, LensMeshScale, MovingLens ? MovingLens : Cylinder, nullptr);
		}
		else
		{
			AssignRole(HeadMesh, HeadMeshScale, StaticBody ? StaticBody : Cylinder, nullptr);
		}
	}

	return NumAssigned;
}

void STSAVDMXFixtureBuilder::OnGDTFChanged(const FAssetData& AssetData)
{
	GDTFSource = Cast<UDMXImportGDTF>(AssetData.GetAsset());
	RefreshGDTFModes(true);
	if (GDTFSource.IsValid())
	{
		FText EmbeddedModelResult;
		const int32 NumEmbeddedMeshes = ImportEmbeddedGDTFModels(EmbeddedModelResult);
		const int32 NumFallbackMeshes = AssignPrimitiveFallbackModels();
		SetStatus(
			NumFallbackMeshes > 0
				? FText::Format(LOCTEXT("EmbeddedAndFallbackModelResult", "{0} Completed {1} missing fixture part(s) from GDTF primitive geometry."),
					EmbeddedModelResult, FText::AsNumber(NumFallbackMeshes))
				: EmbeddedModelResult,
			NumEmbeddedMeshes >= 0);
	}
}

void STSAVDMXFixtureBuilder::OnBaseMeshChanged(const FAssetData& AssetData) { BaseMesh = Cast<UStaticMesh>(AssetData.GetAsset()); BaseMeshScale = FVector::OneVector; }
void STSAVDMXFixtureBuilder::OnYokeMeshChanged(const FAssetData& AssetData) { YokeMesh = Cast<UStaticMesh>(AssetData.GetAsset()); YokeMeshScale = FVector::OneVector; }
void STSAVDMXFixtureBuilder::OnHeadMeshChanged(const FAssetData& AssetData) { HeadMesh = Cast<UStaticMesh>(AssetData.GetAsset()); HeadMeshScale = FVector::OneVector; }
void STSAVDMXFixtureBuilder::OnLensMeshChanged(const FAssetData& AssetData) { LensMesh = Cast<UStaticMesh>(AssetData.GetAsset()); LensMeshScale = FVector::OneVector; }

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
		const FTransform PanTransform = TSAVDMXFixtureBuilder::Private::ConvertParsedGDTFTransformForImportedGLTF(PanGeometry->Position);
		PanPivotOffset = PanTransform.GetTranslation() * MetersToCentimeters;
		PanPivotRotation = PanTransform.GetRotation().Rotator();
	}
	if (TiltGeometry.IsValid())
	{
		const FTransform TiltTransform = TSAVDMXFixtureBuilder::Private::ConvertParsedGDTFTransformForImportedGLTF(TiltGeometry->Position);
		TiltPivotOffset = TiltTransform.GetTranslation() * MetersToCentimeters;
		TiltPivotRotation = TiltTransform.GetRotation().Rotator();
	}

	const TSharedPtr<UE::DMX::GDTF::FDMXGDTFBeamGeometry> BeamGeometry = TSAVDMXFixtureBuilder::Private::FindFirstBeamGeometry(GDTFMode->ResolveGeometry());
	if (BeamGeometry.IsValid())
	{
		const FTransform BeamTransform = TSAVDMXFixtureBuilder::Private::ConvertParsedGDTFTransformForImportedGLTF(BeamGeometry->Position);
		LensOffset = BeamTransform.GetTranslation() * MetersToCentimeters;
		LensMeshRotation = BeamTransform.GetRotation().Rotator();
		const FVector BeamDirection = BeamTransform.GetRotation().RotateVector(FVector(0.0f, -1.0f, 0.0f));
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
	Fixture.BaseMeshScale = BaseMeshScale;
	Fixture.YokeMeshScale = YokeMeshScale;
	Fixture.HeadMeshScale = HeadMeshScale;
	Fixture.LensMeshScale = LensMeshScale;
	Fixture.PanPivotOffset = PanPivotOffset;
	Fixture.TiltPivotOffset = TiltPivotOffset;
	Fixture.PanPivotRotation = PanPivotRotation;
	Fixture.TiltPivotRotation = TiltPivotRotation;
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
	BaseMeshScale = Fixture->BaseMeshScale;
	YokeMeshScale = Fixture->YokeMeshScale;
	HeadMeshScale = Fixture->HeadMeshScale;
	LensMeshScale = Fixture->LensMeshScale;
	PanPivotOffset = Fixture->PanPivotOffset;
	TiltPivotOffset = Fixture->TiltPivotOffset;
	PanPivotRotation = Fixture->PanPivotRotation;
	TiltPivotRotation = Fixture->TiltPivotRotation;
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

bool STSAVDMXFixtureBuilder::BuildCompleteFixtureLibrary(FString& OutSummary)
{
	using namespace UE::DMX::GDTF;

	constexpr int32 ExpectedFixtureCount = 607;
	const FString LibraryPackageName = TEXT("/Game/TSAV/Fixtures/DMX/DMX_TSAV_AllFixtures");
	const FString LibraryAssetName = TEXT("DMX_TSAV_AllFixtures");
	const FString CatalogPackageName = TEXT("/Game/TSAV/Fixtures/DMX/DA_TSAVFixtureCatalog");
	const FString CatalogAssetName = TEXT("DA_TSAVFixtureCatalog");

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.ScanPathsSynchronous({ TEXT("/Game/DMX/GDTF_Fixtures") }, true);
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game/DMX/GDTF_Fixtures"));
	Filter.ClassPaths.Add(UDMXImportGDTF::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	TArray<FAssetData> GDTFAssets;
	AssetRegistry.GetAssets(Filter, GDTFAssets);
	GDTFAssets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.AssetName.LexicalLess(B.AssetName);
	});

	if (GDTFAssets.Num() != ExpectedFixtureCount)
	{
		OutSummary = FString::Printf(TEXT("Expected %d imported GDTF assets, but found %d under /Game/DMX/GDTF_Fixtures."),
			ExpectedFixtureCount, GDTFAssets.Num());
		UE_LOG(LogTSAVGDTFBatch, Error, TEXT("%s"), *OutSummary);
		return false;
	}

	UPackage* LibraryPackage = CreatePackage(*LibraryPackageName);
	UDMXLibrary* Library = FindObject<UDMXLibrary>(LibraryPackage, *LibraryAssetName);
	if (!Library)
	{
		Library = NewObject<UDMXLibrary>(LibraryPackage, *LibraryAssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Library);
	}
	else
	{
		Library->Modify();
		const TArray<UDMXEntity*> ExistingEntities = Library->GetEntities();
		for (UDMXEntity* Entity : ExistingEntities)
		{
			if (Entity)
			{
				Entity->Destroy();
			}
		}
	}
	if (!TSAVDMXFixtureBuilder::Private::ConfigureLibraryPorts(*Library))
	{
		OutSummary = TEXT("The master fixture library could not be connected to the project DMX input/output ports.");
		UE_LOG(LogTSAVGDTFBatch, Error, TEXT("%s"), *OutSummary);
		return false;
	}

	UPackage* CatalogPackage = CreatePackage(*CatalogPackageName);
	UTSAVDMXFixtureCatalog* Catalog = FindObject<UTSAVDMXFixtureCatalog>(CatalogPackage, *CatalogAssetName);
	if (!Catalog)
	{
		Catalog = NewObject<UTSAVDMXFixtureCatalog>(CatalogPackage, *CatalogAssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Catalog);
	}
	Catalog->Modify();
	Catalog->Fixtures.Reset(GDTFAssets.Num());

	auto BuildSafeFallbackMode = []()
	{
		FDMXFixtureMode Mode;
		Mode.ModeName = TEXT("TSAV Safe 9 Channel");
		auto AddFunction = [&Mode](const TCHAR* Name, const int32 Channel, const EDMXFixtureSignalFormat Format = EDMXFixtureSignalFormat::E8Bit)
		{
			FDMXFixtureFunction Function;
			Function.Attribute = FDMXAttributeName(Name);
			Function.FunctionName = Name;
			Function.Channel = Channel;
			Function.DataType = Format;
			Mode.Functions.Add(Function);
		};
		AddFunction(TEXT("Dimmer"), 1);
		AddFunction(TEXT("Pan"), 2, EDMXFixtureSignalFormat::E16Bit);
		AddFunction(TEXT("Tilt"), 4, EDMXFixtureSignalFormat::E16Bit);
		AddFunction(TEXT("Zoom"), 6);
		AddFunction(TEXT("ColorAdd_R"), 7);
		AddFunction(TEXT("ColorAdd_G"), 8);
		AddFunction(TEXT("ColorAdd_B"), 9);
		Mode.bAutoChannelSpan = true;
		return Mode;
	};

	auto ResetBuilder = [](STSAVDMXFixtureBuilder& Builder)
	{
		Builder.GDTFSource.Reset();
		Builder.BaseMesh.Reset();
		Builder.YokeMesh.Reset();
		Builder.HeadMesh.Reset();
		Builder.LensMesh.Reset();
		Builder.BaseMeshScale = FVector::OneVector;
		Builder.YokeMeshScale = FVector::OneVector;
		Builder.HeadMeshScale = FVector::OneVector;
		Builder.LensMeshScale = FVector::OneVector;
		Builder.FixtureScale = 1.0f;
		Builder.ModelRotation = FRotator::ZeroRotator;
		Builder.PanPivotOffset = FVector::ZeroVector;
		Builder.TiltPivotOffset = FVector(0.0f, 0.0f, 40.0f);
		Builder.PanPivotRotation = FRotator::ZeroRotator;
		Builder.TiltPivotRotation = FRotator::ZeroRotator;
		Builder.LensOffset = FVector(20.0f, 0.0f, 0.0f);
		Builder.LensMeshRotation = FRotator::ZeroRotator;
		Builder.BeamRotation = FRotator::ZeroRotator;
		Builder.PanMin = -270.0f;
		Builder.PanMax = 270.0f;
		Builder.TiltMin = -135.0f;
		Builder.TiltMax = 135.0f;
		Builder.PanSpeed = 360.0f;
		Builder.TiltSpeed = 360.0f;
		Builder.MaximumIntensity = 50000.0f;
		Builder.MinimumBeamAngle = 5.0f;
		Builder.MaximumBeamAngle = 35.0f;
		Builder.AttenuationRadius = 3000.0f;
		Builder.ParsedModes.Reset();
		Builder.ModeOptions.Reset();
		Builder.SelectedMode.Reset();
	};

	TSharedRef<STSAVDMXFixtureBuilder> Builder = MakeShared<STSAVDMXFixtureBuilder>();
	int32 NextUniverse = 1;
	int32 NextAddress = 1;
	int32 NumEmbeddedModels = 0;
	int32 NumPrimitiveFallbacks = 0;
	int32 NumInvalidProfileFallbacks = 0;
	int32 NumFailures = 0;
	FScopedSlowTask SlowTask(GDTFAssets.Num(), LOCTEXT("BuildCompleteFixtureLibraryProgress", "Building the complete TSAV GDTF fixture library..."));
	SlowTask.MakeDialogDelayed(1.0f, false, false);

	for (int32 AssetIndex = 0; AssetIndex < GDTFAssets.Num(); ++AssetIndex)
	{
		const FAssetData& AssetData = GDTFAssets[AssetIndex];
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BuildFixtureProgress", "Building {0} ({1}/{2})"),
			FText::FromName(AssetData.AssetName), FText::AsNumber(AssetIndex + 1), FText::AsNumber(GDTFAssets.Num())));
		ResetBuilder(*Builder);

		UDMXImportGDTF* GDTFSourceAsset = Cast<UDMXImportGDTF>(AssetData.GetAsset());
		if (!GDTFSourceAsset)
		{
			++NumFailures;
			UE_LOG(LogTSAVGDTFBatch, Error, TEXT("Could not load GDTF asset %s."), *AssetData.GetObjectPathString());
			continue;
		}
		Builder->GDTFSource = GDTFSourceAsset;

		UDMXGDTF* ParsedGDTF = GDTFSourceAsset->LoadGDTF();
		const TSharedPtr<FDMXGDTFFixtureType> ParsedFixtureType = ParsedGDTF && ParsedGDTF->GetDescription().IsValid()
			? ParsedGDTF->GetDescription()->GetFixtureType()
			: nullptr;
		const bool bInvalidProfile = !ParsedFixtureType.IsValid();
		if (bInvalidProfile)
		{
			Builder->ParsedModes = { BuildSafeFallbackMode() };
			Builder->ModeOptions = { MakeShared<FString>(Builder->ParsedModes[0].ModeName) };
			Builder->SelectedMode = Builder->ModeOptions[0];
			++NumInvalidProfileFallbacks;
		}
		else
		{
			Builder->RefreshGDTFModes(true);
			int32 FirstValidModeIndex = INDEX_NONE;
			for (int32 ModeIndex = 0; ModeIndex < Builder->ParsedModes.Num(); ++ModeIndex)
			{
				int32 Span = 0;
				for (const FDMXFixtureFunction& Function : Builder->ParsedModes[ModeIndex].Functions)
				{
					Span = FMath::Max(Span, Function.GetLastChannel());
				}
				if (Span > 0 && Span <= 512)
				{
					FirstValidModeIndex = ModeIndex;
					break;
				}
			}
			if (FirstValidModeIndex == INDEX_NONE)
			{
				Builder->ParsedModes = { BuildSafeFallbackMode() };
				Builder->ModeOptions = { MakeShared<FString>(Builder->ParsedModes[0].ModeName) };
				Builder->SelectedMode = Builder->ModeOptions[0];
				++NumInvalidProfileFallbacks;
			}
			else if (Builder->ModeOptions.IsValidIndex(FirstValidModeIndex))
			{
				Builder->SelectedMode = Builder->ModeOptions[FirstValidModeIndex];
				Builder->AdoptSelectedModePhysicalProperties();
			}
		}

		FText EmbeddedResult;
		const int32 EmbeddedMeshCount = bInvalidProfile ? 0 : Builder->ImportEmbeddedGDTFModels(EmbeddedResult);
		const int32 FallbackMeshCount = Builder->AssignPrimitiveFallbackModels();
		if (EmbeddedMeshCount > 0)
		{
			++NumEmbeddedModels;
		}
		if (FallbackMeshCount > 0)
		{
			++NumPrimitiveFallbacks;
		}
		if (!Builder->BaseMesh.IsValid() && !Builder->YokeMesh.IsValid() && !Builder->HeadMesh.IsValid() && !Builder->LensMesh.IsValid())
		{
			++NumFailures;
			UE_LOG(LogTSAVGDTFBatch, Error, TEXT("No model could be assigned to %s."), *AssetData.AssetName.ToString());
			continue;
		}

		const int32 SelectedModeIndex = Builder->GetSelectedModeIndex();

		const FString CleanName = ObjectTools::SanitizeObjectName(AssetData.AssetName.ToString());
		FDMXEntityFixtureTypeConstructionParams TypeParams;
		TypeParams.ParentDMXLibrary = Library;
		TypeParams.Modes = Builder->ParsedModes;
		UDMXEntityFixtureType* FixtureType = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(
			TypeParams, CleanName + TEXT(" Type"), true);
		if (!FixtureType)
		{
			++NumFailures;
			continue;
		}
		FixtureType->GDTFSource = GDTFSourceAsset;
		for (int32 ModeIndex = 0; ModeIndex < FixtureType->Modes.Num(); ++ModeIndex)
		{
			FixtureType->UpdateChannelSpan(ModeIndex);
		}
		const int32 ActiveModeIndex = FMath::Clamp(SelectedModeIndex, 0, FixtureType->Modes.Num() - 1);
		const FDMXFixtureMode& ActiveMode = FixtureType->Modes[ActiveModeIndex];
		// FDMXEntityFixturePatchCache reports the amount of actual data it stores,
		// which may differ from FDMXFixtureMode::ChannelSpan when a GDTF has gaps,
		// overlapping logical functions, or a matrix. Reserve the larger footprint
		// to guarantee that generated patches never overlap in a universe.
		int32 PatchDataSpan = 0;
		for (const FDMXFixtureFunction& Function : ActiveMode.Functions)
		{
			PatchDataSpan += Function.GetNumChannels();
		}
		if (ActiveMode.bFixtureMatrixEnabled && !ActiveMode.FixtureMatrixConfig.CellAttributes.IsEmpty())
		{
			PatchDataSpan = FMath::Max(PatchDataSpan, ActiveMode.FixtureMatrixConfig.FirstCellChannel - 1);
			PatchDataSpan += ActiveMode.FixtureMatrixConfig.GetNumChannels();
		}
		if (!ActiveMode.bAutoChannelSpan)
		{
			PatchDataSpan = FMath::Max(PatchDataSpan, ActiveMode.ChannelSpan);
		}
		PatchDataSpan = FMath::Clamp(PatchDataSpan, 1, 512);
		const int32 AddressAllocationSpan = FMath::Max(FMath::Clamp(ActiveMode.ChannelSpan, 1, 512), PatchDataSpan);
		if (NextAddress + AddressAllocationSpan - 1 > 512)
		{
			++NextUniverse;
			NextAddress = 1;
		}

		FDMXEntityFixturePatchConstructionParams PatchParams;
		PatchParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureType);
		PatchParams.ActiveMode = ActiveModeIndex;
		PatchParams.UniverseID = NextUniverse;
		PatchParams.StartingAddress = NextAddress;
		// Mark the library once after the batch. Passing true here invokes editor
		// PostEditChange for every patch, which realigns the freshly imported mode
		// channels after their allocation span has already been calculated.
		UDMXEntityFixturePatch* Patch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams, CleanName, false);
		if (!Patch)
		{
			FixtureType->Destroy();
			++NumFailures;
			continue;
		}
		Patch->bReceiveDMXInEditor = true;
		Patch->RebuildCache();
		const int32 ChannelSpan = FMath::Clamp(Patch->GetChannelSpan(), 1, 512);

		FTSAVDMXFixtureDefinition& Definition = Catalog->Fixtures.AddDefaulted_GetRef();
		Definition.DefinitionId = AssetData.AssetName;
		Definition.DisplayName = FText::FromString(ParsedFixtureType.IsValid()
			? (!ParsedFixtureType->LongName.IsEmpty() ? ParsedFixtureType->LongName : ParsedFixtureType->Name.ToString())
			: AssetData.AssetName.ToString().Replace(TEXT("_"), TEXT(" ")));
		Definition.Manufacturer = FText::FromString(ParsedFixtureType.IsValid() ? ParsedFixtureType->Manufacturer : TEXT("Fine Art"));
		Definition.Revision = AssetData.AssetName.ToString();
		Definition.GDTFSource = GDTFSourceAsset;
		Definition.GDTFModeName = Builder->SelectedMode.IsValid() ? *Builder->SelectedMode : Builder->ParsedModes[0].ModeName;
		Definition.DMXLibrary = Library;
		Definition.FixtureTypeId = FixtureType->GetID();
		Definition.FixturePatchId = Patch->GetID();
		Definition.Universe = NextUniverse;
		Definition.Address = NextAddress;
		Definition.ChannelSpan = ChannelSpan;
		Definition.BaseMesh = Builder->BaseMesh.Get();
		Definition.YokeMesh = Builder->YokeMesh.Get();
		Definition.HeadMesh = Builder->HeadMesh.Get();
		Definition.LensMesh = Builder->LensMesh.Get();
		Definition.FixtureScale = Builder->FixtureScale;
		Definition.ModelRotation = Builder->ModelRotation;
		Definition.BaseMeshScale = Builder->BaseMeshScale;
		Definition.YokeMeshScale = Builder->YokeMeshScale;
		Definition.HeadMeshScale = Builder->HeadMeshScale;
		Definition.LensMeshScale = Builder->LensMeshScale;
		Definition.PanPivotOffset = Builder->PanPivotOffset;
		Definition.TiltPivotOffset = Builder->TiltPivotOffset;
		Definition.PanPivotRotation = Builder->PanPivotRotation;
		Definition.TiltPivotRotation = Builder->TiltPivotRotation;
		Definition.PanMinDegrees = Builder->PanMin;
		Definition.PanMaxDegrees = Builder->PanMax;
		Definition.TiltMinDegrees = Builder->TiltMin;
		Definition.TiltMaxDegrees = Builder->TiltMax;
		Definition.PanSpeedDegreesPerSecond = Builder->PanSpeed;
		Definition.TiltSpeedDegreesPerSecond = Builder->TiltSpeed;
		Definition.LensOffset = Builder->LensOffset;
		Definition.LensMeshRotation = Builder->LensMeshRotation;
		Definition.BeamRotation = Builder->BeamRotation;
		Definition.MaximumIntensityLumens = Builder->MaximumIntensity;
		Definition.MinimumBeamAngleDegrees = Builder->MinimumBeamAngle;
		Definition.MaximumBeamAngleDegrees = Builder->MaximumBeamAngle;
		Definition.AttenuationRadiusCm = Builder->AttenuationRadius;
		Definition.bUsesEmbeddedModel = EmbeddedMeshCount > 0;
		Definition.bUsesPrimitiveFallback = FallbackMeshCount > 0;
		Definition.bUsesInvalidProfileFallback = bInvalidProfile;

		NextAddress += AddressAllocationSpan;
	}

	// Interchange may keep temporary glTF subobject packages alive until the end
	// of the full batch and then remove them. Finalize against the physical asset
	// state only after every import has finished, so no catalog soft reference can
	// point at an object that disappears during the same editor session.
	UStaticMesh* RepairBase = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Base.SM_MovingHead_Base"));
	UStaticMesh* RepairYoke = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Yoke.SM_MovingHead_Yoke"));
	UStaticMesh* RepairHead = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Head.SM_MovingHead_Head"));
	UStaticMesh* RepairLens = LoadObject<UStaticMesh>(nullptr,
		TEXT("/DMXFixtures/LightFixtures/Meshes/SM_MovingHead_Lens.SM_MovingHead_Lens"));
	UStaticMesh* RepairCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	int32 NumFinalizedMeshReferences = 0;
	auto FinalizeMeshReference = [&NumFinalizedMeshReferences, RepairCube](TSoftObjectPtr<UStaticMesh>& MeshReference, UStaticMesh* PreferredFallback)
	{
		if (MeshReference.IsNull())
		{
			return false;
		}
		bool bPhysicalPackageExists = true;
		if (bPhysicalPackageExists)
		{
			const FString PackageName = MeshReference.ToSoftObjectPath().GetLongPackageName();
			if (PackageName.StartsWith(TEXT("/Game/")))
			{
				const FString PackageFilename = FPackageName::LongPackageNameToFilename(
					PackageName, FPackageName::GetAssetPackageExtension());
				bPhysicalPackageExists = FPaths::FileExists(PackageFilename);
			}
		}
		if (!bPhysicalPackageExists || !MeshReference.LoadSynchronous())
		{
			MeshReference = PreferredFallback ? PreferredFallback : RepairCube;
			++NumFinalizedMeshReferences;
			return true;
		}
		return false;
	};
	auto NormalizePatchAddresses = [&Catalog, &Library, &NextUniverse]()
	{
		int32 NormalizedUniverse = 1;
		int32 NormalizedAddress = 1;
		for (FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
		{
			if (UDMXEntityFixturePatch* Patch = Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId)))
			{
				Patch->RebuildCache();
				const int32 Span = FMath::Clamp(Patch->GetChannelSpan(), 1, 512);
				if (NormalizedAddress + Span - 1 > 512)
				{
					++NormalizedUniverse;
					NormalizedAddress = 1;
				}
				Patch->SetUniverseID(NormalizedUniverse);
				Patch->SetStartingChannel(NormalizedAddress);
				Definition.Universe = NormalizedUniverse;
				Definition.Address = NormalizedAddress;
				Definition.ChannelSpan = Span;
				NormalizedAddress += Span;
			}
		}
		NextUniverse = NormalizedUniverse;
	};

	for (FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
	{
		const bool bRepairedBase = FinalizeMeshReference(Definition.BaseMesh, RepairBase);
		const bool bRepairedYoke = FinalizeMeshReference(Definition.YokeMesh, RepairYoke);
		const bool bRepairedHead = FinalizeMeshReference(Definition.HeadMesh, RepairHead);
		const bool bRepairedLens = FinalizeMeshReference(Definition.LensMesh, RepairLens);
		if ((bRepairedBase || bRepairedYoke || bRepairedHead || bRepairedLens) && !Definition.bUsesPrimitiveFallback)
		{
			Definition.bUsesPrimitiveFallback = true;
			++NumPrimitiveFallbacks;
		}

		if (UDMXEntityFixturePatch* FinalPatch = Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId)))
		{
			FinalPatch->RebuildCache();
			Definition.ChannelSpan = FMath::Clamp(FinalPatch->GetChannelSpan(), 1, 512);
		}
	}
	NormalizePatchAddresses();

	Library->MarkPackageDirty();
	Catalog->MarkPackageDirty();
	const TArray<UPackage*> PackagesToSave{ LibraryPackage, CatalogPackage };
	bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);

	// Saving is the point where Interchange removes replaced subobject packages
	// and fixture patches settle on their persisted cache state. Run the same
	// normalization once more after that cleanup, then save the corrected catalog.
	for (FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
	{
		const bool bRepairedBase = FinalizeMeshReference(Definition.BaseMesh, RepairBase);
		const bool bRepairedYoke = FinalizeMeshReference(Definition.YokeMesh, RepairYoke);
		const bool bRepairedHead = FinalizeMeshReference(Definition.HeadMesh, RepairHead);
		const bool bRepairedLens = FinalizeMeshReference(Definition.LensMesh, RepairLens);
		if ((bRepairedBase || bRepairedYoke || bRepairedHead || bRepairedLens) && !Definition.bUsesPrimitiveFallback)
		{
			Definition.bUsesPrimitiveFallback = true;
			++NumPrimitiveFallbacks;
		}

		if (UDMXEntityFixturePatch* FinalPatch = Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId)))
		{
			FinalPatch->RebuildCache();
			Definition.ChannelSpan = FMath::Clamp(FinalPatch->GetChannelSpan(), 1, 512);
		}
	}
	NormalizePatchAddresses();
	Library->MarkPackageDirty();
	Catalog->MarkPackageDirty();
	bSaved = UEditorLoadingAndSavingUtils::SavePackages({ LibraryPackage, CatalogPackage }, true) && bSaved;
	const bool bComplete = bSaved && NumFailures == 0 && Catalog->Fixtures.Num() == ExpectedFixtureCount;
	OutSummary = FString::Printf(
		TEXT("Built %d/%d fixtures across %d DMX universes; %d use imported embedded models, %d use GDTF primitive/fallback parts, %d mesh references were finalized, %d use safe-mode recovery, including invalid profiles; failures=%d; saved=%s."),
		Catalog->Fixtures.Num(), ExpectedFixtureCount, NextUniverse, NumEmbeddedModels, NumPrimitiveFallbacks,
		NumFinalizedMeshReferences, NumInvalidProfileFallbacks, NumFailures, bSaved ? TEXT("true") : TEXT("false"));
	if (bComplete)
	{
		UE_LOG(LogTSAVGDTFBatch, Display, TEXT("CODEX_TSAV_GDTF_BATCH_BUILD_SUCCESS %s"), *OutSummary);
	}
	else
	{
		UE_LOG(LogTSAVGDTFBatch, Error, TEXT("CODEX_TSAV_GDTF_BATCH_BUILD_FAILURE %s"), *OutSummary);
	}
	return bComplete;
}

bool STSAVDMXFixtureBuilder::ValidateCompleteFixtureLibrary(FString& OutSummary)
{
	constexpr int32 ExpectedFixtureCount = 607;
	UTSAVDMXFixtureCatalog* Catalog = Cast<UTSAVDMXFixtureCatalog>(UTSAVDMXFixtureCatalog::DefaultCatalogPath.TryLoad());
	UDMXLibrary* Library = LoadObject<UDMXLibrary>(nullptr,
		TEXT("/Game/TSAV/Fixtures/DMX/DMX_TSAV_AllFixtures.DMX_TSAV_AllFixtures"));
	if (!Catalog || !Library)
	{
		OutSummary = TEXT("The generated fixture catalog or DMX library could not be loaded.");
		return false;
	}

	const TArray<UDMXEntityFixtureType*> FixtureTypes = Library->GetEntitiesTypeCast<UDMXEntityFixtureType>();
	const TArray<UDMXEntityFixturePatch*> FixturePatches = Library->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	int32 NumFailures = 0;
	int32 NumFallbackModels = 0;
	int32 NumInvalidProfiles = 0;
	int32 NumRenderableModelAssets = 0;
	FPreviewScene PreviewScene;
	ATSAVDMXFixture* ValidationActor = PreviewScene.GetWorld()->SpawnActor<ATSAVDMXFixture>();
	if (!ValidationActor)
	{
		OutSummary = TEXT("A transient fixture validation actor could not be spawned.");
		return false;
	}

	for (const FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
	{
		UDMXEntityFixtureType* FixtureType = Cast<UDMXEntityFixtureType>(Library->FindEntity(Definition.FixtureTypeId));
		UDMXEntityFixturePatch* Patch = Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId));
		const bool bHasModelReference = !Definition.BaseMesh.IsNull() || !Definition.YokeMesh.IsNull() ||
			!Definition.HeadMesh.IsNull() || !Definition.LensMesh.IsNull();
		const bool bApplied = ValidationActor->ApplyFixtureDefinition(Definition, true);
		ValidationActor->ApplyNormalizedDMX(0.25f, 0.75f, 0.5f, FLinearColor(0.2f, 0.4f, 0.8f), 0.6f, true);

		auto IsRenderableMesh = [&NumRenderableModelAssets, &Definition](const TSoftObjectPtr<UStaticMesh>& MeshReference, const FVector& PartScale)
		{
			if (MeshReference.IsNull())
			{
				return true;
			}
			UStaticMesh* Mesh = MeshReference.LoadSynchronous();
			if (!Mesh || Mesh->GetNumVertices(0) <= 0)
			{
				return false;
			}
			const FVector Size = Mesh->GetBoundingBox().GetSize() * PartScale.GetAbs() * FMath::Max(Definition.FixtureScale, 0.001f);
			const bool bFiniteBounds = !Size.ContainsNaN() && FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y) && FMath::IsFinite(Size.Z);
			const bool bSaneBounds = Size.GetMax() > 0.001 && Size.GetMax() < 1000000.0 && Size.GetMin() >= 0.0;
			if (bFiniteBounds && bSaneBounds)
			{
				++NumRenderableModelAssets;
				return true;
			}
			return false;
		};
		const bool bModelAssetsValid = IsRenderableMesh(Definition.BaseMesh, Definition.BaseMeshScale) &&
			IsRenderableMesh(Definition.YokeMesh, Definition.YokeMeshScale) &&
			IsRenderableMesh(Definition.HeadMesh, Definition.HeadMeshScale) &&
			IsRenderableMesh(Definition.LensMesh, Definition.LensMeshScale);
		const float ExpectedPan = FMath::Lerp(Definition.PanMinDegrees, Definition.PanMaxDegrees, 0.25f);
		const float ExpectedTilt = FMath::Lerp(Definition.TiltMinDegrees, Definition.TiltMaxDegrees, 0.75f);
		const float NarrowAngle = FMath::Clamp(Definition.MinimumBeamAngleDegrees, 1.0f, 89.0f);
		const float WideAngle = FMath::Clamp(FMath::Max(Definition.MaximumBeamAngleDegrees, NarrowAngle), 1.0f, 89.0f);
		const float ExpectedConeAngle = FMath::Lerp(WideAngle, NarrowAngle, 0.6f);
		const USpotLightComponent* Beam = ValidationActor->GetBeamLightComponent();
		const bool bBeamValid = Beam &&
			FMath::IsNearlyEqual(Beam->Intensity, Definition.MaximumIntensityLumens * 0.5f, 0.1f) &&
			FMath::IsNearlyEqual(Beam->OuterConeAngle, ExpectedConeAngle, 0.01f) &&
			Beam->GetLightColor().Equals(FLinearColor(0.2f, 0.4f, 0.8f), 0.02f);
		const bool bBehaviorValid = FMath::IsNearlyEqual(ValidationActor->CurrentPanDegrees, ExpectedPan) &&
			FMath::IsNearlyEqual(ValidationActor->CurrentTiltDegrees, ExpectedTilt) &&
			FMath::IsNearlyEqual(ValidationActor->LastDimmerValue, 0.5f) &&
			bBeamValid;
		const bool bPatchValid = FixtureType && Patch && Patch->GetFixtureType() == FixtureType &&
			Patch->GetUniverseID() == Definition.Universe && Patch->GetStartingChannel() == Definition.Address &&
			Patch->GetChannelSpan() == Definition.ChannelSpan && Definition.ChannelSpan > 0 &&
			Definition.Address + Definition.ChannelSpan - 1 <= 512;
		const bool bPhysicalValuesValid = FMath::IsFinite(Definition.PanMinDegrees) && FMath::IsFinite(Definition.PanMaxDegrees) &&
			FMath::IsFinite(Definition.TiltMinDegrees) && FMath::IsFinite(Definition.TiltMaxDegrees) &&
			Definition.MinimumBeamAngleDegrees > 0.0f && Definition.MaximumBeamAngleDegrees >= Definition.MinimumBeamAngleDegrees;
		if (Definition.GDTFSource.IsNull() || !bHasModelReference || !bModelAssetsValid || !bApplied || !bBehaviorValid || !bPatchValid || !bPhysicalValuesValid)
		{
			++NumFailures;
			UE_LOG(LogTSAVGDTFBatch, Error,
				TEXT("Fixture validation failed for %s: source=%s model=%s renderable=%s applied=%s behavior=%s patch=%s physical=%s"),
				*Definition.DefinitionId.ToString(), Definition.GDTFSource.IsNull() ? TEXT("false") : TEXT("true"),
				bHasModelReference ? TEXT("true") : TEXT("false"), bModelAssetsValid ? TEXT("true") : TEXT("false"),
				bApplied ? TEXT("true") : TEXT("false"), bBehaviorValid ? TEXT("true") : TEXT("false"),
				bPatchValid ? TEXT("true") : TEXT("false"), bPhysicalValuesValid ? TEXT("true") : TEXT("false"));
			if (!bPatchValid)
			{
				UE_LOG(LogTSAVGDTFBatch, Error,
					TEXT("Patch details for %s: expected universe=%d address=%d span=%d; actual universe=%d address=%d span=%d; typeMatch=%s."),
					*Definition.DefinitionId.ToString(), Definition.Universe, Definition.Address, Definition.ChannelSpan,
					Patch ? Patch->GetUniverseID() : -1, Patch ? Patch->GetStartingChannel() : -1,
					Patch ? Patch->GetChannelSpan() : -1,
					(Patch && FixtureType && Patch->GetFixtureType() == FixtureType) ? TEXT("true") : TEXT("false"));
			}
		}
		NumFallbackModels += Definition.bUsesPrimitiveFallback ? 1 : 0;
		NumInvalidProfiles += Definition.bUsesInvalidProfileFallback ? 1 : 0;
	}

	const bool bCountsValid = Catalog->Fixtures.Num() == ExpectedFixtureCount &&
		FixtureTypes.Num() == ExpectedFixtureCount && FixturePatches.Num() == ExpectedFixtureCount;
	const bool bSuccess = bCountsValid && NumFailures == 0;
	OutSummary = FString::Printf(
		TEXT("Validated catalog=%d, fixture types=%d, patches=%d, normalized behavior and renderable geometry=%d/%d, renderable model assets=%d, fallback models=%d, invalid-profile recoveries=%d."),
		Catalog->Fixtures.Num(), FixtureTypes.Num(), FixturePatches.Num(), Catalog->Fixtures.Num() - NumFailures,
		Catalog->Fixtures.Num(), NumRenderableModelAssets, NumFallbackModels, NumInvalidProfiles);
	if (bSuccess)
	{
		UE_LOG(LogTSAVGDTFBatch, Display, TEXT("CODEX_TSAV_GDTF_BATCH_VALIDATION_SUCCESS %s"), *OutSummary);
	}
	else
	{
		UE_LOG(LogTSAVGDTFBatch, Error, TEXT("CODEX_TSAV_GDTF_BATCH_VALIDATION_FAILURE %s"), *OutSummary);
	}
	return bSuccess;
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
