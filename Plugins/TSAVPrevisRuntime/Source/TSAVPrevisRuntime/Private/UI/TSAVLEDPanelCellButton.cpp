// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVLEDPanelCellButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVLEDPanelCellButton)

void UTSAVLEDPanelCellButton::InitializeCell(const int32 InColumn, const int32 InRow)
{
	Column = InColumn;
	Row = InRow;
	OnClicked.AddUniqueDynamic(this, &UTSAVLEDPanelCellButton::HandleClicked);
}

void UTSAVLEDPanelCellButton::HandleClicked()
{
	OnCellClicked.Broadcast(Column, Row);
}
