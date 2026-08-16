// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TSAVMediaSurfaceActor.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
template <typename OptionType> class SComboBox;

/** Central editor panel for the basic identity, display, canvas, and transform settings of every TSAV screen. */
class STSAVScreenControlTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVScreenControlTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FScreenOption = TWeakObjectPtr<ATSAVMediaSurfaceActor>;

	void RefreshScreenOptions();
	void SetActiveScreen(ATSAVMediaSurfaceActor* Screen);
	void LoadFormFromScreen();
	ATSAVMediaSurfaceActor* FindSelectedScreen() const;
	TSharedRef<SWidget> GenerateScreenOption(TSharedPtr<FScreenOption> Item) const;
	void ScreenOptionChanged(TSharedPtr<FScreenOption> Item, ESelectInfo::Type SelectionType);
	FText GetActiveScreenText() const;
	FText GetScreenResolutionText() const;
	FReply ApplyChanges();
	FReply RefreshScreens();
	FReply UseSelectedScreen();
	FReply SelectScreenInLevel();
	void SetStatus(const FText& Message, bool bSuccess);
	FSlateColor GetStatusColor() const;

	TWeakObjectPtr<ATSAVMediaSurfaceActor> ActiveScreen;
	TArray<TSharedPtr<FScreenOption>> ScreenOptions;
	TSharedPtr<SComboBox<TSharedPtr<FScreenOption>>> ScreenCombo;
	TSharedPtr<SEditableTextBox> ScreenNameField;
	FString ScreenName;
	float Brightness = 3.0f;
	int32 CanvasStartX = 0;
	int32 CanvasStartY = 0;
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	FText StatusText;
	bool bStatusSuccess = true;
};
