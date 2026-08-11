// Copyright TSAV. All Rights Reserved.

#include "TSAVPrevisToolset.h"

#include "DMXProtocolSettings.h"
#include "DMXProtocolTypes.h"
#include "HAL/PlatformTime.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXInputPortConfig.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXOutputPortConfig.h"
#include "IO/DMXPortManager.h"
#include "Interfaces/IDMXProtocol.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVPrevisToolset)

namespace TSAVPrevisTools::Private
{
	FString CommunicationTypeToString(const EDMXCommunicationType CommunicationType)
	{
		if (const UEnum* Enum = StaticEnum<EDMXCommunicationType>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(CommunicationType));
		}

		return TEXT("Unknown");
	}

	void SetUniverseRange(
		const TSharedRef<FJsonObject>& JsonObject,
		const int32 LocalStart,
		const int32 LocalEnd,
		const int32 ExternalStart,
		const int32 ExternalEnd)
	{
		JsonObject->SetNumberField(TEXT("localUniverseStart"), LocalStart);
		JsonObject->SetNumberField(TEXT("localUniverseEnd"), LocalEnd);
		JsonObject->SetNumberField(TEXT("externalUniverseStart"), ExternalStart);
		JsonObject->SetNumberField(TEXT("externalUniverseEnd"), ExternalEnd);
	}
}

FString UTSAVPrevisToolset::GetDMXStatus(float ActiveWindowSeconds)
{
	using namespace TSAVPrevisTools::Private;

	if (!IsInGameThread())
	{
		return TEXT("{\"error\":\"GetDMXStatus must run on the Unreal game thread.\"}");
	}

	ActiveWindowSeconds = FMath::Clamp(ActiveWindowSeconds, 0.01f, 60.0f);

	const UDMXProtocolSettings* Settings = GetDefault<UDMXProtocolSettings>();
	FDMXPortManager& PortManager = FDMXPortManager::Get();
	const double NowSeconds = FPlatformTime::Seconds();

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("receiveEnabled"), Settings->IsReceiveDMXEnabled());
	Root->SetBoolField(TEXT("sendEnabled"), Settings->IsSendDMXEnabled());
	Root->SetBoolField(TEXT("protocolsSuspended"), PortManager.AreProtocolsSuspended());
	Root->SetNumberField(TEXT("sendingRefreshRateHz"), Settings->SendingRefreshRate);
	Root->SetNumberField(TEXT("activeWindowSeconds"), ActiveWindowSeconds);

	const TArray<FDMXInputPortSharedRef>& InputPorts = PortManager.GetInputPorts();
	Root->SetNumberField(TEXT("inputPortCount"), InputPorts.Num());

	int32 TotalActiveUniverses = 0;
	TArray<TSharedPtr<FJsonValue>> InputPortValues;
	InputPortValues.Reserve(InputPorts.Num());

	for (const FDMXInputPortSharedRef& InputPort : InputPorts)
	{
		const FDMXInputPortConfig Config = InputPort->MakeInputPortConfig();
		TSharedRef<FJsonObject> PortObject = MakeShared<FJsonObject>();
		PortObject->SetStringField(TEXT("name"), Config.GetPortName());
		PortObject->SetStringField(TEXT("protocol"), Config.GetProtocolName().ToString());
		PortObject->SetStringField(TEXT("guid"), Config.GetPortGuid().ToString(EGuidFormats::DigitsWithHyphens));
		PortObject->SetStringField(TEXT("communicationType"), CommunicationTypeToString(Config.GetCommunicationType()));
		PortObject->SetStringField(TEXT("deviceAddress"), Config.GetDeviceAddress());
		PortObject->SetBoolField(TEXT("registered"), InputPort->IsRegistered());
		PortObject->SetBoolField(TEXT("receiveEnabled"), InputPort->IsReceiveDMXEnabled());
		SetUniverseRange(
			PortObject,
			Config.GetLocalUniverseStart(),
			Config.GetLocalUniverseStart() + Config.GetNumUniverses() - 1,
			Config.GetExternUniverseStart(),
			Config.GetExternUniverseStart() + Config.GetNumUniverses() - 1);

		const TMap<int32, FDMXSignalSharedPtr>& Signals = InputPort->GameThreadGetAllDMXSignals();
		PortObject->SetNumberField(TEXT("bufferedUniverseCount"), Signals.Num());

		int32 ActiveUniverseCount = 0;
		TArray<TSharedPtr<FJsonValue>> UniverseValues;
		UniverseValues.Reserve(Signals.Num());

		for (const TPair<int32, FDMXSignalSharedPtr>& SignalPair : Signals)
		{
			if (!SignalPair.Value.IsValid())
			{
				continue;
			}

			const FDMXSignal& Signal = *SignalPair.Value;
			const double PacketAgeSeconds = FMath::Max(0.0, NowSeconds - Signal.Timestamp);
			const bool bIsActive = PacketAgeSeconds <= ActiveWindowSeconds;
			ActiveUniverseCount += bIsActive ? 1 : 0;

			int32 NonZeroChannelCount = 0;
			for (const uint8 ChannelValue : Signal.ChannelData)
			{
				NonZeroChannelCount += ChannelValue != 0 ? 1 : 0;
			}

			TSharedRef<FJsonObject> UniverseObject = MakeShared<FJsonObject>();
			UniverseObject->SetNumberField(TEXT("localUniverse"), SignalPair.Key);
			UniverseObject->SetNumberField(TEXT("externalUniverse"), Signal.ExternUniverseID);
			UniverseObject->SetBoolField(TEXT("active"), bIsActive);
			UniverseObject->SetNumberField(TEXT("packetAgeSeconds"), PacketAgeSeconds);
			UniverseObject->SetNumberField(TEXT("priority"), Signal.Priority);
			UniverseObject->SetNumberField(TEXT("channelCount"), Signal.ChannelData.Num());
			UniverseObject->SetNumberField(TEXT("nonZeroChannelCount"), NonZeroChannelCount);
			UniverseValues.Add(MakeShared<FJsonValueObject>(UniverseObject));
		}

		UniverseValues.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
		{
			return Left->AsObject()->GetNumberField(TEXT("localUniverse")) <
				Right->AsObject()->GetNumberField(TEXT("localUniverse"));
		});

		PortObject->SetNumberField(TEXT("activeUniverseCount"), ActiveUniverseCount);
		PortObject->SetArrayField(TEXT("universes"), UniverseValues);
		TotalActiveUniverses += ActiveUniverseCount;
		InputPortValues.Add(MakeShared<FJsonValueObject>(PortObject));
	}

	Root->SetNumberField(TEXT("activeUniverseCount"), TotalActiveUniverses);
	Root->SetArrayField(TEXT("inputPorts"), InputPortValues);

	const TArray<FDMXOutputPortSharedRef>& OutputPorts = PortManager.GetOutputPorts();
	Root->SetNumberField(TEXT("outputPortCount"), OutputPorts.Num());
	TArray<TSharedPtr<FJsonValue>> OutputPortValues;
	OutputPortValues.Reserve(OutputPorts.Num());

	for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
	{
		const FDMXOutputPortConfig Config = OutputPort->MakeOutputPortConfig();
		TSharedRef<FJsonObject> PortObject = MakeShared<FJsonObject>();
		PortObject->SetStringField(TEXT("name"), Config.GetPortName());
		PortObject->SetStringField(TEXT("protocol"), Config.GetProtocolName().ToString());
		PortObject->SetStringField(TEXT("guid"), Config.GetPortGuid().ToString(EGuidFormats::DigitsWithHyphens));
		PortObject->SetStringField(TEXT("communicationType"), CommunicationTypeToString(Config.GetCommunicationType()));
		PortObject->SetStringField(TEXT("deviceAddress"), Config.GetDeviceAddress());
		PortObject->SetBoolField(TEXT("registered"), OutputPort->IsRegistered());
		SetUniverseRange(
			PortObject,
			Config.GetLocalUniverseStart(),
			Config.GetLocalUniverseStart() + Config.GetNumUniverses() - 1,
			Config.GetExternUniverseStart(),
			Config.GetExternUniverseStart() + Config.GetNumUniverses() - 1);
		OutputPortValues.Add(MakeShared<FJsonValueObject>(PortObject));
	}

	Root->SetArrayField(TEXT("outputPorts"), OutputPortValues);

	FString Result;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
	FJsonSerializer::Serialize(Root, Writer);
	return Result;
}
