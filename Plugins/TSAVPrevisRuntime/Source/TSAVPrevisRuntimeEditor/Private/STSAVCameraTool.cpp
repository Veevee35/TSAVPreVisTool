// Copyright TSAV. All Rights Reserved.

#include "STSAVCameraTool.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IDetailsView.h"
#include "LevelEditorViewport.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STSAVCameraTool"

namespace TSAVCameraTool::Private
{
	FTransform GetEditorViewTransform()
	{
		if (const FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient)
		{
			return FTransform(ViewportClient->GetViewRotation(), ViewportClient->GetViewLocation());
		}
		return FTransform::Identity;
	}

	FString GetCameraDisplayName(const ATSAVCameraActor* Camera)
	{
		if (!Camera)
		{
			return TEXT("None");
		}
		const FString Label = Camera->CameraLabel.ToString();
		return Label.IsEmpty() ? Camera->GetActorLabel() : Label;
	}
}

void STSAVCameraTool::Construct(const FArguments& InArgs)
{
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bLockable = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsArgs.ViewIdentifier = TEXT("TSAVCameraToolDetails");
	DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "TSAV PRODUCTION CAMERA"))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Description", "Create a render-capable camera from the editor view, then configure lens, output, PTZ, and VISCA settings below."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(CameraCombo, SComboBox<TSharedPtr<FCameraOption>>)
					.OptionsSource(&CameraOptions)
					.OnGenerateWidget(this, &STSAVCameraTool::GenerateCameraOption)
					.OnSelectionChanged(this, &STSAVCameraTool::CameraOptionChanged)
					[
						SNew(STextBlock).Text(this, &STSAVCameraTool::GetActiveCameraText)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.OnClicked(this, &STSAVCameraTool::RefreshCameras)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateFromView", "Create Camera From Current View"))
					.OnClicked(this, &STSAVCameraTool::CreateCameraFromView)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("UseSelected", "Use Selected"))
					.OnClicked(this, &STSAVCameraTool::UseSelectedCamera)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SnapToView", "Snap Camera To Current View"))
					.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
					.OnClicked(this, &STSAVCameraTool::SnapCameraToView)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SelectInLevel", "Select In Level"))
					.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
					.OnClicked(this, &STSAVCameraTool::SelectCameraInLevel)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(this, &STSAVCameraTool::GetStatusText)
				.ColorAndOpacity(this, &STSAVCameraTool::GetStatusColor)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				DetailsView.ToSharedRef()
			]
		]
	];

	RefreshCameraOptions();
	if (ATSAVCameraActor* Selected = FindSelectedCamera())
	{
		SetActiveCamera(Selected);
	}
	else if (!CameraOptions.IsEmpty())
	{
		SetActiveCamera(CameraOptions[0]->Get());
	}
	else
	{
		SetStatus(LOCTEXT("NoCameraStatus", "No TSAV camera is present in the current level."), false);
	}
}

void STSAVCameraTool::RefreshCameraOptions()
{
	CameraOptions.Reset();
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVCameraActor> It(World); It; ++It)
			{
				CameraOptions.Add(MakeShared<FCameraOption>(*It));
			}
		}
	}
	CameraOptions.Sort([](const TSharedPtr<FCameraOption>& Left, const TSharedPtr<FCameraOption>& Right)
	{
		return TSAVCameraTool::Private::GetCameraDisplayName(Left->Get()) < TSAVCameraTool::Private::GetCameraDisplayName(Right->Get());
	});
	if (CameraCombo)
	{
		CameraCombo->RefreshOptions();
	}
}

void STSAVCameraTool::SetActiveCamera(ATSAVCameraActor* Camera)
{
	ActiveCamera = Camera;
	if (DetailsView)
	{
		DetailsView->SetObject(Camera);
	}
	if (CameraCombo && Camera)
	{
		const TSharedPtr<FCameraOption>* Match = CameraOptions.FindByPredicate([Camera](const TSharedPtr<FCameraOption>& Item)
		{
			return Item->Get() == Camera;
		});
		if (Match)
		{
			CameraCombo->SetSelectedItem(*Match);
		}
	}
	if (Camera)
	{
		SetStatus(FText::Format(LOCTEXT("EditingCameraStatus", "Editing {0}. Property changes are applied directly to the level actor."), FText::FromString(TSAVCameraTool::Private::GetCameraDisplayName(Camera))), true);
	}
}

ATSAVCameraActor* STSAVCameraTool::FindSelectedCamera() const
{
	if (!GEditor || !GEditor->GetSelectedActors())
	{
		return nullptr;
	}
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (ATSAVCameraActor* Camera = Cast<ATSAVCameraActor>(*It))
		{
			return Camera;
		}
	}
	return nullptr;
}

TSharedRef<SWidget> STSAVCameraTool::GenerateCameraOption(const TSharedPtr<FCameraOption> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(TSAVCameraTool::Private::GetCameraDisplayName(Item.IsValid() ? Item->Get() : nullptr)));
}

void STSAVCameraTool::CameraOptionChanged(const TSharedPtr<FCameraOption> Item, ESelectInfo::Type SelectionType)
{
	if (Item.IsValid())
	{
		SetActiveCamera(Item->Get());
	}
}

FText STSAVCameraTool::GetActiveCameraText() const
{
	return FText::FromString(TSAVCameraTool::Private::GetCameraDisplayName(ActiveCamera.Get()));
}

FSlateColor STSAVCameraTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.35f, 0.85f, 0.45f) : FLinearColor(1.0f, 0.55f, 0.20f);
}

FReply STSAVCameraTool::CreateCameraFromView()
{
	if (!GEditor)
	{
		return FReply::Handled();
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		SetStatus(LOCTEXT("NoWorld", "No editable level is currently open."), false);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateCameraTransaction", "Create TSAV Production Camera"));
	World->Modify();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transactional;
	ATSAVCameraActor* Camera = World->SpawnActor<ATSAVCameraActor>(ATSAVCameraActor::StaticClass(), TSAVCameraTool::Private::GetEditorViewTransform(), SpawnParameters);
	if (!Camera)
	{
		SetStatus(LOCTEXT("CreateFailed", "The camera could not be created."), false);
		return FReply::Handled();
	}

	int32 CameraNumber = 1;
	for (TActorIterator<ATSAVCameraActor> It(World); It; ++It)
	{
		if (*It != Camera)
		{
			++CameraNumber;
		}
	}
	const FText CameraName = FText::Format(LOCTEXT("CameraNameFormat", "CAM {0}"), FText::AsNumber(CameraNumber));
	Camera->Modify();
	Camera->CameraLabel = CameraName;
	Camera->SetActorLabel(CameraName.ToString());
	Camera->PostEditChange();
	Camera->MarkPackageDirty();

	RefreshCameraOptions();
	SetActiveCamera(Camera);
	SelectCameraInLevel();
	SetStatus(FText::Format(LOCTEXT("CameraCreated", "Created {0} from the current editor view."), CameraName), true);
	return FReply::Handled();
}

FReply STSAVCameraTool::UseSelectedCamera()
{
	if (ATSAVCameraActor* Camera = FindSelectedCamera())
	{
		RefreshCameraOptions();
		SetActiveCamera(Camera);
	}
	else
	{
		SetStatus(LOCTEXT("SelectionNotCamera", "Select a TSAV Production Camera actor in the level first."), false);
	}
	return FReply::Handled();
}

FReply STSAVCameraTool::RefreshCameras()
{
	ATSAVCameraActor* Previous = ActiveCamera.Get();
	RefreshCameraOptions();
	if (Previous && CameraOptions.ContainsByPredicate([Previous](const TSharedPtr<FCameraOption>& Item) { return Item->Get() == Previous; }))
	{
		SetActiveCamera(Previous);
	}
	else if (!CameraOptions.IsEmpty())
	{
		SetActiveCamera(CameraOptions[0]->Get());
	}
	else
	{
		ActiveCamera.Reset();
		DetailsView->SetObject(nullptr);
		SetStatus(LOCTEXT("NoCamerasAfterRefresh", "No TSAV cameras were found in the current level."), false);
	}
	return FReply::Handled();
}

FReply STSAVCameraTool::SnapCameraToView()
{
	ATSAVCameraActor* Camera = ActiveCamera.Get();
	if (!Camera)
	{
		return FReply::Handled();
	}
	const FScopedTransaction Transaction(LOCTEXT("SnapCameraTransaction", "Snap TSAV Camera To Editor View"));
	Camera->Modify();
	Camera->SetActorTransform(TSAVCameraTool::Private::GetEditorViewTransform());
	Camera->PostEditMove(true);
	Camera->MarkPackageDirty();
	SetStatus(FText::Format(LOCTEXT("CameraSnapped", "Snapped {0} to the current editor view."), GetActiveCameraText()), true);
	return FReply::Handled();
}

FReply STSAVCameraTool::SelectCameraInLevel()
{
	if (ATSAVCameraActor* Camera = ActiveCamera.Get(); GEditor && Camera)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Camera, true, true, true);
	}
	return FReply::Handled();
}

void STSAVCameraTool::SetStatus(const FText& Message, const bool bSuccess)
{
	StatusText = Message;
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
