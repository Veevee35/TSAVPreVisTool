// Copyright TSAV. All Rights Reserved.

#include "STSAVLEDWallBuilder.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Factories/DataAssetFactory.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditorViewport.h"
#include "MediaSource.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "TSAVLEDPanelDefinition.h"
#include "TSAVLEDWall.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TSAVLEDWallBuilder"

namespace TSAVLEDBuilder::Private
{
	class SCanvasPreview final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCanvasPreview) {}
			SLATE_ATTRIBUTE(int32, CanvasWidth)
			SLATE_ATTRIBUTE(int32, CanvasHeight)
			SLATE_ATTRIBUTE(int32, ScreenX)
			SLATE_ATTRIBUTE(int32, ScreenY)
			SLATE_ATTRIBUTE(int32, ScreenWidth)
			SLATE_ATTRIBUTE(int32, ScreenHeight)
			SLATE_ATTRIBUTE(int32, Columns)
			SLATE_ATTRIBUTE(int32, Rows)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			CanvasWidth = Args._CanvasWidth;
			CanvasHeight = Args._CanvasHeight;
			ScreenX = Args._ScreenX;
			ScreenY = Args._ScreenY;
			ScreenWidth = Args._ScreenWidth;
			ScreenHeight = Args._ScreenHeight;
			Columns = Args._Columns;
			Rows = Args._Rows;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			return FVector2D(520.0f, 260.0f);
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			const int32 SafeCanvasWidth = FMath::Max(CanvasWidth.Get(), 1);
			const int32 SafeCanvasHeight = FMath::Max(CanvasHeight.Get(), 1);
			const FVector2D Available = AllottedGeometry.GetLocalSize() - FVector2D(24.0f, 24.0f);
			const float Scale = FMath::Max(0.001f, FMath::Min(Available.X / SafeCanvasWidth, Available.Y / SafeCanvasHeight));
			const FVector2D CanvasSize(SafeCanvasWidth * Scale, SafeCanvasHeight * Scale);
			const FVector2D CanvasOrigin = (AllottedGeometry.GetLocalSize() - CanvasSize) * 0.5f;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(CanvasSize, FSlateLayoutTransform(CanvasOrigin)),
				FAppStyle::GetBrush(TEXT("WhiteBrush")),
				ESlateDrawEffect::None,
				FLinearColor(0.025f, 0.03f, 0.04f, 1.0f));

			const int32 CurrentScreenX = ScreenX.Get();
			const int32 CurrentScreenY = ScreenY.Get();
			const int32 CurrentScreenWidth = FMath::Max(ScreenWidth.Get(), 1);
			const int32 CurrentScreenHeight = FMath::Max(ScreenHeight.Get(), 1);
			const bool bFits = CurrentScreenX >= 0 && CurrentScreenY >= 0 &&
				CurrentScreenX + CurrentScreenWidth <= SafeCanvasWidth &&
				CurrentScreenY + CurrentScreenHeight <= SafeCanvasHeight;

			const FVector2D ScreenOrigin = CanvasOrigin + FVector2D(CurrentScreenX * Scale, CurrentScreenY * Scale);
			const FVector2D MappedScreenSize(CurrentScreenWidth * Scale, CurrentScreenHeight * Scale);
			const FLinearColor ScreenColor = bFits
				? FLinearColor(0.02f, 0.58f, 0.82f, 0.92f)
				: FLinearColor(0.85f, 0.08f, 0.08f, 0.92f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(MappedScreenSize, FSlateLayoutTransform(ScreenOrigin)),
				FAppStyle::GetBrush(TEXT("WhiteBrush")),
				ESlateDrawEffect::None,
				ScreenColor);

			const int32 SafeColumns = FMath::Clamp(Columns.Get(), 1, 64);
			const int32 SafeRows = FMath::Clamp(Rows.Get(), 1, 64);
			const FLinearColor GridColor(1.0f, 1.0f, 1.0f, 0.35f);
			for (int32 Column = 1; Column < SafeColumns; ++Column)
			{
				const float X = ScreenOrigin.X + MappedScreenSize.X * Column / SafeColumns;
				TArray<FVector2D> Points{FVector2D(X, ScreenOrigin.Y), FVector2D(X, ScreenOrigin.Y + MappedScreenSize.Y)};
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, GridColor, true, 1.0f);
			}
			for (int32 Row = 1; Row < SafeRows; ++Row)
			{
				const float Y = ScreenOrigin.Y + MappedScreenSize.Y * Row / SafeRows;
				TArray<FVector2D> Points{FVector2D(ScreenOrigin.X, Y), FVector2D(ScreenOrigin.X + MappedScreenSize.X, Y)};
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, GridColor, true, 1.0f);
			}

			return LayerId + 2;
		}

	private:
		TAttribute<int32> CanvasWidth;
		TAttribute<int32> CanvasHeight;
		TAttribute<int32> ScreenX;
		TAttribute<int32> ScreenY;
		TAttribute<int32> ScreenWidth;
		TAttribute<int32> ScreenHeight;
		TAttribute<int32> Columns;
		TAttribute<int32> Rows;
	};

	TSharedRef<SWidget> MakeSectionHeader(const FText& Number, const FText& Title, const FText& Help)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.55f, 0.82f, 1.0f))
					.Padding(FMargin(8.0f, 3.0f))
					[SNew(STextBlock).Text(Number).ColorAndOpacity(FLinearColor::White)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock).Text(Help).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}
}

void STSAVLEDWallBuilder::Construct(const FArguments& InArgs)
{
	using namespace TSAVLEDBuilder::Private;

	auto MakeIntField = [](const FText& Label, int32* Value, int32 Minimum) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(110.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.Value_Lambda([Value]() { return TOptional<int32>(*Value); })
					.MinValue(Minimum)
					.OnValueChanged_Lambda([Value, Minimum](int32 NewValue) { *Value = FMath::Max(NewValue, Minimum); })
				]
			];
	};

	auto MakeFloatField = [](const FText& Label, float* Value, float Minimum, const FText& Unit) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(110.0f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([Value]() { return TOptional<float>(*Value); })
					.MinValue(Minimum)
					.MinSliderValue(Minimum)
					.AllowSpin(true)
					.OnValueChanged_Lambda([Value, Minimum](float NewValue) { *Value = FMath::Max(NewValue, Minimum); })
					.UndeterminedString(Unit)
				]
			];
	};

	ChildSlot
	[
		SNew(SBorder)
		.Padding(14.0f)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "LED Wall Builder"))
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingLarge")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 14.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Subtitle", "Choose a panel, define the wall, position it on the processor canvas, and assign an NDI source."))
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
				[
					SNew(SBorder)
					.Padding(10.0f)
					.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Header")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(this, &STSAVLEDWallBuilder::GetSelectionStatus).AutoWrapText(true)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("LoadSelected", "Load Selected Wall")).OnClicked(this, &STSAVLEDWallBuilder::LoadSelectedWall)
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step1", "1"), LOCTEXT("PanelTitle", "Panel"), LOCTEXT("PanelHelp", "Pick a saved cabinet preset, or enter the physical size and native pixel resolution for a custom panel."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UTSAVLEDPanelDefinition::StaticClass())
					.ObjectPath_Lambda([this]() { return PanelDefinition.IsValid() ? PanelDefinition->GetPathName() : FString(); })
					.OnObjectChanged(this, &STSAVLEDWallBuilder::OnPanelDefinitionChanged)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox)
					.IsEnabled_Lambda([this]() { return !PanelDefinition.IsValid(); })
					[
						SNew(SGridPanel).FillColumn(0, 1.0f).FillColumn(1, 1.0f)
						+ SGridPanel::Slot(0, 0).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeFloatField(LOCTEXT("PanelWidth", "Width"), &PanelWidthCm, 1.0f, LOCTEXT("Cm", "cm"))]
						+ SGridPanel::Slot(1, 0).Padding(8.0f, 3.0f, 0.0f, 3.0f)[MakeFloatField(LOCTEXT("PanelHeight", "Height"), &PanelHeightCm, 1.0f, LOCTEXT("Cm", "cm"))]
						+ SGridPanel::Slot(0, 1).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeFloatField(LOCTEXT("PanelDepth", "Depth"), &PanelDepthCm, 0.1f, LOCTEXT("Cm", "cm"))]
						+ SGridPanel::Slot(0, 2).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeIntField(LOCTEXT("PanelPixelsX", "Pixels X"), &PanelResolutionX, 1)]
						+ SGridPanel::Slot(1, 2).Padding(8.0f, 3.0f, 0.0f, 3.0f)[MakeIntField(LOCTEXT("PanelPixelsY", "Pixels Y"), &PanelResolutionY, 1)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 3.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const FVector2D Pitch = GetPixelPitchMm();
						return FText::Format(LOCTEXT("Pitch", "Calculated pixel pitch: {0} mm × {1} mm"), FText::AsNumber(Pitch.X), FText::AsNumber(Pitch.Y));
					})
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 14.0f)
				[
					SNew(SBox)
					.IsEnabled_Lambda([this]() { return !PanelDefinition.IsValid(); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SEditableTextBox).Text_Lambda([this]() { return FText::FromString(PanelPresetName); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { PanelPresetName = Text.ToString(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("SavePreset", "Save Panel Preset")).OnClicked(this, &STSAVLEDWallBuilder::SavePanelPreset)
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step2", "2"), LOCTEXT("WallTitle", "Wall"), LOCTEXT("WallHelp", "Choose how many cabinets make up the screen. The native screen resolution is calculated automatically."))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SGridPanel).FillColumn(0, 1.0f).FillColumn(1, 1.0f)
					+ SGridPanel::Slot(0, 0).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeIntField(LOCTEXT("Columns", "Columns"), &Columns, 1)]
					+ SGridPanel::Slot(1, 0).Padding(8.0f, 3.0f, 0.0f, 3.0f)[MakeIntField(LOCTEXT("Rows", "Rows"), &Rows, 1)]
					+ SGridPanel::Slot(0, 1).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeFloatField(LOCTEXT("Gap", "Panel gap"), &PanelGapCm, 0.0f, LOCTEXT("Cm", "cm"))]
					+ SGridPanel::Slot(1, 1).Padding(8.0f, 3.0f, 0.0f, 3.0f)[MakeFloatField(LOCTEXT("Border", "Outer border"), &BorderCm, 0.0f, LOCTEXT("Cm", "cm"))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsChecked_Lambda([this]() { return bSerpentine ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bSerpentine = State == ECheckBoxState::Checked; })[SNew(STextBlock).Text(LOCTEXT("Serpentine", "Serpentine cabinet linking"))]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(24.0f, 0.0f, 0.0f, 0.0f)[SNew(SCheckBox).IsChecked_Lambda([this]() { return bShowSeams ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowSeams = State == ECheckBoxState::Checked; })[SNew(STextBlock).Text(LOCTEXT("Seams", "Show panel seams"))]]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 14.0f)
				[
					SNew(SBorder).Padding(9.0f).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Header")))[SNew(STextBlock).Text(this, &STSAVLEDWallBuilder::GetWallSummary).Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))]
				]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step3", "3"), LOCTEXT("CanvasTitle", "Canvas & NDI Source"), LOCTEXT("CanvasHelp", "Set the screen's top-left pixel on the processor canvas, then choose the NDI Media Source carrying that canvas."))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SGridPanel).FillColumn(0, 1.0f).FillColumn(1, 1.0f)
					+ SGridPanel::Slot(0, 0).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeIntField(LOCTEXT("CanvasWidth", "Canvas width"), &CanvasWidth, 1)]
					+ SGridPanel::Slot(1, 0).Padding(8.0f, 3.0f, 0.0f, 3.0f)[MakeIntField(LOCTEXT("CanvasHeight", "Canvas height"), &CanvasHeight, 1)]
					+ SGridPanel::Slot(0, 1).Padding(0.0f, 3.0f, 8.0f, 3.0f)[MakeIntField(LOCTEXT("CanvasX", "Screen X"), &CanvasX, 0)]
					+ SGridPanel::Slot(1, 1).Padding(8.0f, 3.0f, 0.0f, 3.0f)[MakeIntField(LOCTEXT("CanvasY", "Screen Y"), &CanvasY, 0)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
				[
					SNew(SBox).HeightOverride(270.0f)
					[
						SNew(SCanvasPreview)
						.CanvasWidth_Lambda([this]() { return CanvasWidth; })
						.CanvasHeight_Lambda([this]() { return CanvasHeight; })
						.ScreenX_Lambda([this]() { return CanvasX; })
						.ScreenY_Lambda([this]() { return CanvasY; })
						.ScreenWidth_Lambda([this]() { return GetWallResolution().X; })
						.ScreenHeight_Lambda([this]() { return GetWallResolution().Y; })
						.Columns_Lambda([this]() { return Columns; })
						.Rows_Lambda([this]() { return Rows; })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 9.0f)
				[
					SNew(STextBlock).Text(this, &STSAVLEDWallBuilder::GetCanvasStatus).ColorAndOpacity(this, &STSAVLEDWallBuilder::GetCanvasStatusColor).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("SourceLabel", "NDI / Media Source"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMediaSource::StaticClass())
					.ObjectPath_Lambda([this]() { return MediaSource.IsValid() ? MediaSource->GetPathName() : FString(); })
					.OnObjectChanged(this, &STSAVLEDWallBuilder::OnMediaSourceChanged)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 16.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bPreviewInEditor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bPreviewInEditor = State == ECheckBoxState::Checked; })
					[SNew(STextBlock).Text(LOCTEXT("PreviewEditor", "Preview NDI source in the editor"))]
				]

				+ SVerticalBox::Slot().AutoHeight()[MakeSectionHeader(LOCTEXT("Step4", "4"), LOCTEXT("CreateTitle", "Create or Update"), LOCTEXT("CreateHelp", "Create a new wall in front of the active viewport, or apply these settings to the selected LED wall."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SEditableTextBox).Text_Lambda([this]() { return FText::FromString(WallName); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { WallName = Text.ToString(); }).HintText(LOCTEXT("WallNameHint", "Wall name"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("CreateWall", "Create LED Wall")).HAlign(HAlign_Center).IsEnabled_Lambda([this]() { return DoesScreenFitCanvas(); }).OnClicked(this, &STSAVLEDWallBuilder::CreateWall)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("UpdateWall", "Update Selected Wall")).HAlign(HAlign_Center).IsEnabled_Lambda([this]() { return DoesScreenFitCanvas() && (ActiveWall.IsValid() || FindSelectedWall() != nullptr); }).OnClicked(this, &STSAVLEDWallBuilder::UpdateWall)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Text_Lambda([this]() { return StatusMessage; }).ColorAndOpacity_Lambda([this]() { return bStatusSuccess ? FLinearColor(0.15f, 0.8f, 0.35f) : FLinearColor(0.9f, 0.2f, 0.15f); }).AutoWrapText(true)
				]
			]
		]
	];
}

void STSAVLEDWallBuilder::OnPanelDefinitionChanged(const FAssetData& AssetData)
{
	PanelDefinition = Cast<UTSAVLEDPanelDefinition>(AssetData.GetAsset());
	if (PanelDefinition.IsValid())
	{
		PanelWidthCm = PanelDefinition->WidthCm;
		PanelHeightCm = PanelDefinition->HeightCm;
		PanelDepthCm = PanelDefinition->DepthCm;
		PanelResolutionX = PanelDefinition->ResolutionX;
		PanelResolutionY = PanelDefinition->ResolutionY;
	}
}

void STSAVLEDWallBuilder::OnMediaSourceChanged(const FAssetData& AssetData)
{
	MediaSource = Cast<UMediaSource>(AssetData.GetAsset());
}

FReply STSAVLEDWallBuilder::SavePanelPreset()
{
	const FString SanitizedName = ObjectTools::SanitizeObjectName(PanelPresetName);
	if (SanitizedName.IsEmpty())
	{
		SetStatus(LOCTEXT("InvalidPresetName", "Enter a name for the panel preset."), false);
		return FReply::Handled();
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	FString UniquePackageName;
	FString UniqueAssetName;
	AssetTools.CreateUniqueAssetName(TEXT("/Game/TSAV/PanelDefinitions/") + SanitizedName, TEXT(""), UniquePackageName, UniqueAssetName);

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = UTSAVLEDPanelDefinition::StaticClass();
	UTSAVLEDPanelDefinition* Definition = Cast<UTSAVLEDPanelDefinition>(AssetTools.CreateAsset(
		UniqueAssetName,
		FPackageName::GetLongPackagePath(UniquePackageName),
		UTSAVLEDPanelDefinition::StaticClass(),
		Factory));

	if (!Definition)
	{
		SetStatus(LOCTEXT("PresetCreateFailed", "Unreal could not create the panel preset."), false);
		return FReply::Handled();
	}

	Definition->ModelName = PanelPresetName;
	Definition->WidthCm = PanelWidthCm;
	Definition->HeightCm = PanelHeightCm;
	Definition->DepthCm = PanelDepthCm;
	Definition->ResolutionX = PanelResolutionX;
	Definition->ResolutionY = PanelResolutionY;
	Definition->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({Definition->GetPackage()}, true);
	PanelDefinition = Definition;
	SetStatus(FText::Format(LOCTEXT("PresetCreated", "Created panel preset: {0}"), FText::FromString(Definition->GetName())), true);
	return FReply::Handled();
}

FReply STSAVLEDWallBuilder::LoadSelectedWall()
{
	ATSAVLEDWall* Wall = FindSelectedWall();
	if (!Wall)
	{
		SetStatus(LOCTEXT("NoWallSelected", "Select a TSAV LED Wall actor in the level first."), false);
		return FReply::Handled();
	}

	ActiveWall = Wall;
	WallName = Wall->GetActorLabel();
	PanelDefinition = Wall->bUsePanelDefinition ? Wall->PanelDefinition.Get() : nullptr;
	PanelWidthCm = Wall->PanelWidthCm;
	PanelHeightCm = Wall->PanelHeightCm;
	PanelDepthCm = Wall->WallDepthCm;
	PanelResolutionX = Wall->PanelResolutionX;
	PanelResolutionY = Wall->PanelResolutionY;
	if (PanelDefinition.IsValid())
	{
		PanelWidthCm = PanelDefinition->WidthCm;
		PanelHeightCm = PanelDefinition->HeightCm;
		PanelDepthCm = PanelDefinition->DepthCm;
		PanelResolutionX = PanelDefinition->ResolutionX;
		PanelResolutionY = PanelDefinition->ResolutionY;
	}
	Columns = Wall->Columns;
	Rows = Wall->Rows;
	PanelGapCm = Wall->PanelGapCm;
	BorderCm = Wall->BorderCm;
	CanvasWidth = Wall->CanvasResolution.X;
	CanvasHeight = Wall->CanvasResolution.Y;
	CanvasX = Wall->CanvasPosition.X;
	CanvasY = Wall->CanvasPosition.Y;
	bSerpentine = Wall->LinkPattern == ETSAVLEDLinkPattern::RowsSerpentine;
	bShowSeams = Wall->bShowPanelSeams;
	bPreviewInEditor = Wall->bPlayInEditor;
	MediaSource = Wall->MediaSource.Get();
	SetStatus(FText::Format(LOCTEXT("WallLoaded", "Loaded {0}. Change values and click Update Selected Wall."), FText::FromString(WallName)), true);
	return FReply::Handled();
}

FReply STSAVLEDWallBuilder::CreateWall()
{
	if (!DoesScreenFitCanvas())
	{
		SetStatus(GetCanvasStatus(), false);
		return FReply::Handled();
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		SetStatus(LOCTEXT("NoEditorWorld", "Open a level before creating the LED wall."), false);
		return FReply::Handled();
	}

	FVector SpawnLocation(0.0f, 0.0f, PanelHeightCm * Rows * 0.5f);
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (GCurrentLevelEditingViewportClient)
	{
		const FVector ViewLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
		const FVector ViewDirection = GCurrentLevelEditingViewportClient->GetViewRotation().Vector();
		SpawnLocation = ViewLocation + ViewDirection * 800.0f;
		SpawnRotation = (ViewLocation - SpawnLocation).Rotation();
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateWallTransaction", "Create TSAV LED Wall"));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.OverrideLevel = World->GetCurrentLevel();
	ATSAVLEDWall* Wall = World->SpawnActor<ATSAVLEDWall>(ATSAVLEDWall::StaticClass(), SpawnLocation, SpawnRotation, SpawnParameters);
	if (!Wall)
	{
		SetStatus(LOCTEXT("WallSpawnFailed", "Unreal could not spawn the LED wall."), false);
		return FReply::Handled();
	}

	Wall->SetActorLabel(WallName.IsEmpty() ? TEXT("LED Wall") : WallName);
	Wall->SetFolderPath(TEXT("TSAV LED Walls"));
	ApplySettings(*Wall);
	ActiveWall = Wall;

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Wall, true, true);
	GEditor->MoveViewportCamerasToActor(*Wall, true);
	SetStatus(FText::Format(LOCTEXT("WallCreated", "Created {0} and linked the selected Media/NDI source."), FText::FromString(Wall->GetActorLabel())), true);
	return FReply::Handled();
}

FReply STSAVLEDWallBuilder::UpdateWall()
{
	ATSAVLEDWall* Wall = ActiveWall.Get();
	if (!Wall)
	{
		Wall = FindSelectedWall();
	}
	if (!Wall)
	{
		SetStatus(LOCTEXT("NoWallToUpdate", "Select or load a TSAV LED Wall before updating."), false);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("UpdateWallTransaction", "Update TSAV LED Wall"));
	Wall->Modify();
	Wall->SetActorLabel(WallName.IsEmpty() ? Wall->GetActorLabel() : WallName);
	ApplySettings(*Wall);
	ActiveWall = Wall;
	SetStatus(FText::Format(LOCTEXT("WallUpdated", "Updated {0}."), FText::FromString(Wall->GetActorLabel())), true);
	return FReply::Handled();
}

void STSAVLEDWallBuilder::ApplySettings(ATSAVLEDWall& Wall) const
{
	Wall.PanelDefinition = PanelDefinition.Get();
	Wall.bUsePanelDefinition = PanelDefinition.IsValid();
	Wall.PanelWidthCm = PanelWidthCm;
	Wall.PanelHeightCm = PanelHeightCm;
	Wall.WallDepthCm = PanelDepthCm;
	Wall.PanelResolutionX = PanelResolutionX;
	Wall.PanelResolutionY = PanelResolutionY;
	Wall.Columns = FMath::Clamp(Columns, 1, 64);
	Wall.Rows = FMath::Clamp(Rows, 1, 64);
	Wall.PanelGapCm = FMath::Max(PanelGapCm, 0.0f);
	Wall.BorderCm = FMath::Max(BorderCm, 0.0f);
	Wall.bShowPanelSeams = bShowSeams;
	Wall.LinkPattern = bSerpentine ? ETSAVLEDLinkPattern::RowsSerpentine : ETSAVLEDLinkPattern::RowsLeftToRight;
	Wall.CanvasResolution = FIntPoint(FMath::Max(CanvasWidth, 1), FMath::Max(CanvasHeight, 1));
	Wall.CanvasPosition = FIntPoint(FMath::Max(CanvasX, 0), FMath::Max(CanvasY, 0));
	Wall.bUseCanvasMapping = true;
	Wall.MediaSource = MediaSource.Get();
	Wall.bPlayInEditor = bPreviewInEditor;
	Wall.bAutoPlay = true;
	Wall.RerunConstructionScripts();
	Wall.RefreshMedia();
	Wall.MarkPackageDirty();
}

ATSAVLEDWall* STSAVLEDWallBuilder::FindSelectedWall() const
{
	if (!GEditor || !GEditor->GetSelectedActors())
	{
		return nullptr;
	}

	for (FSelectionIterator Iterator(*GEditor->GetSelectedActors()); Iterator; ++Iterator)
	{
		if (ATSAVLEDWall* Wall = Cast<ATSAVLEDWall>(*Iterator))
		{
			return Wall;
		}
	}

	return nullptr;
}

bool STSAVLEDWallBuilder::DoesScreenFitCanvas() const
{
	const FIntPoint WallResolution = GetWallResolution();
	return CanvasWidth > 0 && CanvasHeight > 0 && CanvasX >= 0 && CanvasY >= 0 &&
		CanvasX + WallResolution.X <= CanvasWidth && CanvasY + WallResolution.Y <= CanvasHeight;
}

FIntPoint STSAVLEDWallBuilder::GetWallResolution() const
{
	return FIntPoint(FMath::Max(Columns, 1) * FMath::Max(PanelResolutionX, 1), FMath::Max(Rows, 1) * FMath::Max(PanelResolutionY, 1));
}

FVector2D STSAVLEDWallBuilder::GetPixelPitchMm() const
{
	return FVector2D(PanelWidthCm * 10.0f / FMath::Max(PanelResolutionX, 1), PanelHeightCm * 10.0f / FMath::Max(PanelResolutionY, 1));
}

FText STSAVLEDWallBuilder::GetWallSummary() const
{
	const FIntPoint Resolution = GetWallResolution();
	const float PhysicalWidth = FMath::Max(Columns, 1) * PanelWidthCm + FMath::Max(Columns - 1, 0) * PanelGapCm;
	const float PhysicalHeight = FMath::Max(Rows, 1) * PanelHeightCm + FMath::Max(Rows - 1, 0) * PanelGapCm;
	return FText::Format(
		LOCTEXT("WallSummary", "{0} panels  •  {1} × {2} px  •  {3} × {4} cm"),
		FText::AsNumber(FMath::Max(Columns, 1) * FMath::Max(Rows, 1)),
		FText::AsNumber(Resolution.X),
		FText::AsNumber(Resolution.Y),
		FText::AsNumber(PhysicalWidth),
		FText::AsNumber(PhysicalHeight));
}

FText STSAVLEDWallBuilder::GetCanvasStatus() const
{
	const FIntPoint Resolution = GetWallResolution();
	if (DoesScreenFitCanvas())
	{
		return FText::Format(
			LOCTEXT("CanvasFits", "Screen fits. Uses X {0}–{1}, Y {2}–{3} on the {4} × {5} canvas."),
			FText::AsNumber(CanvasX), FText::AsNumber(CanvasX + Resolution.X - 1),
			FText::AsNumber(CanvasY), FText::AsNumber(CanvasY + Resolution.Y - 1),
			FText::AsNumber(CanvasWidth), FText::AsNumber(CanvasHeight));
	}

	return FText::Format(
		LOCTEXT("CanvasOverflow", "Screen does not fit: {0} × {1} px at X={2}, Y={3} exceeds the {4} × {5} canvas."),
		FText::AsNumber(Resolution.X), FText::AsNumber(Resolution.Y), FText::AsNumber(CanvasX), FText::AsNumber(CanvasY), FText::AsNumber(CanvasWidth), FText::AsNumber(CanvasHeight));
}

FSlateColor STSAVLEDWallBuilder::GetCanvasStatusColor() const
{
	return DoesScreenFitCanvas() ? FLinearColor(0.15f, 0.8f, 0.35f) : FLinearColor(0.9f, 0.2f, 0.15f);
}

FText STSAVLEDWallBuilder::GetSelectionStatus() const
{
	if (ActiveWall.IsValid())
	{
		return FText::Format(LOCTEXT("EditingWall", "Editing: {0}"), FText::FromString(ActiveWall->GetActorLabel()));
	}
	if (const ATSAVLEDWall* SelectedWall = FindSelectedWall())
	{
		return FText::Format(LOCTEXT("WallIsSelected", "Selected LED wall: {0}. Click Load Selected Wall to edit it here."), FText::FromString(SelectedWall->GetActorLabel()));
	}
	return LOCTEXT("NoSelection", "No LED wall loaded. Configure the fields below and click Create LED Wall.");
}

void STSAVLEDWallBuilder::SetStatus(const FText& Message, bool bSuccess)
{
	StatusMessage = Message;
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
