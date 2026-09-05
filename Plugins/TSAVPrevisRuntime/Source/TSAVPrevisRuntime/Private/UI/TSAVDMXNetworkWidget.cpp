// Copyright TSAV. All Rights Reserved.
#include "UI/TSAVDMXNetworkWidget.h"
#include "TSAVDMXFixture.h"
#include "DMXProtocolSettings.h"
#include "Interfaces/IDMXProtocol.h"
#include "IO/DMXPortManager.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "EngineUtils.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "SocketSubsystem.h"
#include "UObject/UnrealType.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace TSAVDMXNetwork
{
	bool Validate(const FTSAVDMXConnection& C,FString& Error)
	{
		const auto Fail=[&](const TCHAR* Text) { Error=Text; return false; }; FIPv4Address IP;
		if (C.Name.TrimStartAndEnd().IsEmpty() || !FIPv4Address::Parse(C.Adapter,IP) || (!C.bInput && C.bUnicast && !FIPv4Address::Parse(C.Destination,IP))) return Fail(TEXT("Enter a port name and valid IPv4 adapter/destination addresses."));
		if (C.LocalUniverse<1 || C.Universes<1 || static_cast<int64>(C.LocalUniverse)+C.Universes-1>63999 || C.Priority<0 || C.Priority>200) return Fail(TEXT("Check the local universe range and priority (0–200)."));
		const int32 Minimum=C.bSACN?1:0,Maximum=C.bSACN?63999:32767;
		if (C.ExternalUniverse<Minimum || static_cast<int64>(C.ExternalUniverse)+C.Universes-1>Maximum) return Fail(TEXT("The external universe range is outside this protocol's limits."));
		Error.Reset(); return true;
	}
	bool Apply(UWorld* World,FTSAVDMXConnection& C,FString& Error)
	{
		if (!Validate(C,Error)) return false;
		const FName ProtocolName=C.bSACN?FDMXProtocolModule::DefaultProtocolSACNName:FDMXProtocolModule::DefaultProtocolArtNetName;
		const auto Protocol=FDMXProtocolName(ProtocolName).GetProtocol();
		if (!Protocol) { Error=TEXT("The selected DMX protocol is not loaded."); return false; }
		auto* Settings=GetMutableDefault<UDMXProtocolSettings>(); const auto BeforeInputs=Settings->InputPortConfigs; const auto BeforeOutputs=Settings->OutputPortConfigs;
		if (!C.Id.IsValid()) C.Id=FGuid::NewGuid();
		if (C.bInput) {
			if (Settings->OutputPortConfigs.ContainsByPredicate([&](const auto& P) { return P.GetPortGuid()==C.Id; })) { Error=TEXT("Use New port to change a port's direction."); return false; }
			FDMXInputPortConfigParams P(FDMXInputPortConfig(C.Id)); P.PortName=C.Name; P.ProtocolName=ProtocolName; P.DeviceAddress=C.Adapter; P.bAutoCompleteDeviceAddressEnabled=false;
			P.LocalUniverseStart=C.LocalUniverse; P.ExternUniverseStart=C.ExternalUniverse; P.NumUniverses=C.Universes; P.Priority=C.Priority; P.PriorityStrategy=EDMXPortPriorityStrategy::Highest;
			const auto Types=Protocol->GetInputPortCommunicationTypes(); P.CommunicationType=Types.IsEmpty()?EDMXCommunicationType::InternalOnly:Types[0];
			const FDMXInputPortConfig Config(C.Id,P); auto* Existing=Settings->InputPortConfigs.FindByPredicate([&](const auto& Port) { return Port.GetPortGuid()==C.Id; }); if (Existing) *Existing=Config; else Settings->InputPortConfigs.Add(Config);
		} else {
			if (Settings->InputPortConfigs.ContainsByPredicate([&](const auto& P) { return P.GetPortGuid()==C.Id; })) { Error=TEXT("Use New port to change a port's direction."); return false; }
			FDMXOutputPortConfigParams P(FDMXOutputPortConfig(C.Id)); P.PortName=C.Name; P.ProtocolName=ProtocolName; P.DeviceAddress=C.Adapter; P.bAutoCompleteDeviceAddressEnabled=false;
			P.LocalUniverseStart=C.LocalUniverse; P.ExternUniverseStart=C.ExternalUniverse; P.NumUniverses=C.Universes; P.Priority=C.Priority; P.bLoopbackToEngine=C.bLoopback;
			P.CommunicationType=C.bUnicast?EDMXCommunicationType::Unicast:C.bSACN?EDMXCommunicationType::Multicast:EDMXCommunicationType::Broadcast;
			P.DestinationAddresses.Reset(); if (C.bUnicast) P.DestinationAddresses.Emplace(C.Destination);
			const FDMXOutputPortConfig Config(C.Id,P); auto* Existing=Settings->OutputPortConfigs.FindByPredicate([&](const auto& Port) { return Port.GetPortGuid()==C.Id; }); if (Existing) *Existing=Config; else Settings->OutputPortConfigs.Add(Config);
		}
		FDMXPortManager::Get().UpdateFromProtocolSettings(); const auto Port=FDMXPortManager::Get().FindPortByGuid(C.Id);
		if (!Port || !Port->IsRegistered()) { Settings->InputPortConfigs=BeforeInputs; Settings->OutputPortConfigs=BeforeOutputs; FDMXPortManager::Get().UpdateFromProtocolSettings(); Error=TEXT("The port could not bind. Choose an adapter address available on this computer."); return false; }
		TSet<UDMXLibrary*> Libraries;
		if (World) for (TActorIterator<ATSAVDMXFixture> It(World); It; ++It) if (auto* Patch=It->GetFixturePatch()) Libraries.Add(Patch->GetParentLibrary());
		const auto* Property=FindFProperty<FStructProperty>(UDMXLibrary::StaticClass(),TEXT("PortReferences"));
		for (auto* Library : Libraries) if (Library && Property) {
			Library->Modify(); auto* References=Property->ContainerPtrToValuePtr<FDMXLibraryPortReferences>(Library);
			if (C.bInput) { References->InputPortReferences.RemoveAll([&](const auto& P) { return P.GetPortGuid()==C.Id; }); References->InputPortReferences.Emplace(C.Id,true); }
			else { References->OutputPortReferences.RemoveAll([&](const auto& P) { return P.GetPortGuid()==C.Id; }); References->OutputPortReferences.Emplace(C.Id,true); }
			Library->UpdatePorts(); Library->MarkPackageDirty();
		}
		Error.Reset(); return true;
	}
	static TSharedRef<SWidget> Label(const FString& Text) { return SNew(STextBlock).Text(FText::FromString(Text)).AutoWrapText(true); }
	class SPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPanel) {} SLATE_ARGUMENT(UWorld*,World) SLATE_EVENT(FSimpleDelegate,OnClose) SLATE_END_ARGS()
		void Construct(const FArguments& Args) {
			World=Args._World;
			ChildSlot[SNew(SBorder).Padding(20)[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Label(TEXT("TSAV DMX CONNECTIONS AND MONITOR"))]+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Close"))).Visibility(Args._OnClose.IsBound()?EVisibility::Visible:EVisibility::Collapsed).OnClicked_Lambda([Close=Args._OnClose] { Close.ExecuteIfBound(); return FReply::Handled(); })]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,10)[Label(TEXT("Choose the network adapter and universe mapping. Apply connects this port to native scene fixtures. Enable show output in the Lighting Show Console when ready to transmit."))]
				+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,20,0)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("New port"))).OnClicked_Lambda([this] { Draft={}; return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight()[Field(TEXT("Port name"),&Draft.Name)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Input port (unchecked = output)"),&Draft.bInput)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("sACN (unchecked = Art-Net)"),&Draft.bSACN)]
						+SVerticalBox::Slot().AutoHeight()[Field(TEXT("Adapter IPv4 address"),&Draft.Adapter)]
						+SVerticalBox::Slot().AutoHeight()[SAssignNew(Adapters,SVerticalBox)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Unicast output (otherwise multicast / broadcast)"),&Draft.bUnicast)]
						+SVerticalBox::Slot().AutoHeight()[Field(TEXT("Unicast destination"),&Draft.Destination)]
						+SVerticalBox::Slot().AutoHeight()[Number(TEXT("Local start universe"),&Draft.LocalUniverse,1,63999)]
						+SVerticalBox::Slot().AutoHeight()[Number(TEXT("External start universe"),&Draft.ExternalUniverse,0,63999)]
						+SVerticalBox::Slot().AutoHeight()[Number(TEXT("Universe count"),&Draft.Universes,1,63999)]
						+SVerticalBox::Slot().AutoHeight()[Number(TEXT("sACN priority"),&Draft.Priority,0,200)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Loop output back into Unreal"),&Draft.bLoopback)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(SButton).Text(FText::FromString(TEXT("Apply port and connect fixtures"))).OnClicked_Lambda([this] { if (Apply(World.Get(),Draft,Message)) Message=TEXT("Port connected. Save port settings to keep this computer's configuration."); Refresh(); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Save port settings on this computer"))).OnClicked_Lambda([this] { GetMutableDefault<UDMXProtocolSettings>()->SaveConfig(CPF_Config,*GEngineIni); Message=TEXT("DMX port settings saved."); return FReply::Handled(); })]
					]]
					+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Refresh ports and adapters"))).OnClicked_Lambda([this] { Refresh(); return FReply::Handled(); })]
						+SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Ports,SVerticalBox)]]
						+SVerticalBox::Slot().AutoHeight()[Number(TEXT("Monitor local universe"),&MonitorUniverse,1,63999)]
						+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Monitor()); }).AutoWrapText(true)]
					]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Message); }).AutoWrapText(true)]
			]]; Refresh();
		}
	private:
		TSharedRef<SWidget> Field(const FString& Name,FString* Value) { return SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().Padding(0,5)[Label(Name)]+SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).Text_Lambda([Value] { return FText::FromString(*Value); }).OnTextChanged_Lambda([Value](const FText& V) { *Value=V.ToString(); })]; }
		TSharedRef<SWidget> Check(const FString& Name,bool* Value) { return SNew(SCheckBox).IsChecked_Lambda([Value] { return *Value?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([Value](ECheckBoxState V) { *Value=V==ECheckBoxState::Checked; })[Label(Name)]; }
		TSharedRef<SWidget> Number(const FString& Name,int32* Value,int32 Min,int32 Max) { return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,5)[Label(Name)]+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130)[SNew(SNumericEntryBox<int32>).MinValue(Min).MaxValue(Max).Value_Lambda([Value] { return *Value; }).OnValueChanged_Lambda([Value](int32 V) { *Value=V; })]]; }
		void Refresh() {
			Ports->ClearChildren(); Adapters->ClearChildren(); TArray<TSharedPtr<FInternetAddr>> Addresses;
			if (auto* Sockets=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)) Sockets->GetLocalAdapterAddresses(Addresses);
			for (const auto& Address : Addresses) { const FString IP=Address->ToString(false); Adapters->AddSlot().AutoHeight()[SNew(SButton).Text(FText::FromString(IP)).OnClicked_Lambda([this,IP] { Draft.Adapter=IP; return FReply::Handled(); })]; }
			const auto* Settings=GetDefault<UDMXProtocolSettings>();
			for (const auto& P : Settings->InputPortConfigs) { FTSAVDMXConnection C; C.Id=P.GetPortGuid(); C.Name=P.GetPortName(); C.bInput=true; C.bSACN=P.GetProtocolName()==FDMXProtocolModule::DefaultProtocolSACNName; C.Adapter=P.GetDeviceAddress(); C.LocalUniverse=P.GetLocalUniverseStart(); C.ExternalUniverse=P.GetExternUniverseStart(); C.Universes=P.GetNumUniverses(); C.Priority=P.GetPriority(); Row(C); }
			for (const auto& P : Settings->OutputPortConfigs) { FTSAVDMXConnection C; C.Id=P.GetPortGuid(); C.Name=P.GetPortName(); C.bSACN=P.GetProtocolName()==FDMXProtocolModule::DefaultProtocolSACNName; C.Adapter=P.GetDeviceAddress(); C.LocalUniverse=P.GetLocalUniverseStart(); C.ExternalUniverse=P.GetExternUniverseStart(); C.Universes=P.GetNumUniverses(); C.Priority=P.GetPriority(); C.bUnicast=P.GetCommunicationType()==EDMXCommunicationType::Unicast; C.bLoopback=P.NeedsLoopbackToEngine(); if (!P.GetDestinationAddresses().IsEmpty()) C.Destination=P.GetDestinationAddresses()[0].DestinationAddressString; Row(C); }
		}
		void Row(const FTSAVDMXConnection& C) {
			Ports->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).Text_Lambda([C] { const auto Port=FDMXPortManager::Get().FindPortByGuid(C.Id); return FText::FromString(FString::Printf(TEXT("%s %s — %s — U%d–%d → %d — %s"),C.bInput?TEXT("IN"):TEXT("OUT"),*C.Name,C.bSACN?TEXT("sACN"):TEXT("Art-Net"),C.LocalUniverse,C.LocalUniverse+C.Universes-1,C.ExternalUniverse,Port && Port->IsRegistered()?TEXT("bound"):TEXT("not bound"))); }).OnClicked_Lambda([this,C] { Draft=C; return FReply::Handled(); })];
		}
		FString Monitor() const {
			FString Result; for (const auto& Port : FDMXPortManager::Get().GetInputPorts()) { FDMXSignalSharedPtr Signal; if (!Port->GameThreadGetDMXSignal(MonitorUniverse,Signal) || !Signal) continue; Result+=Port->GetPortName()+TEXT(" input: "); for (int32 I=0; I<FMath::Min(32,Signal->ChannelData.Num()); ++I) Result+=FString::Printf(TEXT("%03d "),Signal->ChannelData[I]); Result+=TEXT("\n"); }
			for (const auto& Port : FDMXPortManager::Get().GetOutputPorts()) { FDMXSignalSharedPtr Signal; if (!Port->GameThreadGetDMXSignal(MonitorUniverse,Signal,true) || !Signal) continue; Result+=Port->GetPortName()+TEXT(" output: "); for (int32 I=0; I<FMath::Min(32,Signal->ChannelData.Num()); ++I) Result+=FString::Printf(TEXT("%03d "),Signal->ChannelData[I]); Result+=TEXT("\n"); }
			return Result.IsEmpty()?TEXT("No buffered signal for this universe."):TEXT("Latest buffered channels 1–32\n")+Result;
		}
		TWeakObjectPtr<UWorld> World; FTSAVDMXConnection Draft; TSharedPtr<SVerticalBox> Ports,Adapters; FString Message; int32 MonitorUniverse=1;
	};
}
TSharedRef<SWidget> MakeTSAVDMXNetworkPanel(UWorld* World,FSimpleDelegate Close) { return SNew(TSAVDMXNetwork::SPanel).World(World).OnClose(Close); }
TSharedRef<SWidget> UTSAVDMXNetworkWidget::RebuildWidget() { return MakeTSAVDMXNetworkPanel(GetWorld(),FSimpleDelegate::CreateWeakLambda(this,[this] { RemoveFromParent(); })); }
