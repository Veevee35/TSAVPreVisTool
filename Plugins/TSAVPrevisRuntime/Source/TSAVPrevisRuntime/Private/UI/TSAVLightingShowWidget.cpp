// Copyright TSAV. All Rights Reserved.
#include "UI/TSAVLightingShowWidget.h"
#include "UI/STSAVLightingShowPanel.h"

TSharedRef<SWidget> UTSAVLightingShowWidget::RebuildWidget()
{
	return SNew(STSAVLightingShowPanel).World(GetWorld()).OnClose(FSimpleDelegate::CreateWeakLambda(this, [this] { RemoveFromParent(); }));
}
