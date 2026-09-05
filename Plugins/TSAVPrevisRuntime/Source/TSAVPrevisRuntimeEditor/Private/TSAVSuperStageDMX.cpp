// Copyright TSAV. All Rights Reserved.
#include "TSAVSuperStageDMX.h"

#include "Engine/Engine.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/NoExportTypes.h"

// Use the vendor's reflected Blueprint/editor API. No dependency on stripped
// vendor headers, binary layouts, private buffers, or licensing internals.
namespace TSAVSuperStageDMX::Private
{
	bool CallVoid(UObject* Object, const FName Name)
	{
		UFunction* Function = Object ? Object->FindFunction(Name) : nullptr;
		if (!Function || Function->NumParms != 0) return false;
		FEditorScriptExecutionGuard ScriptGuard;
		Object->ProcessEvent(Function, nullptr);
		return true;
	}

	UObject* CallObject(UObject* Object, const FName Name)
	{
		UFunction* Function = Object ? Object->FindFunction(Name) : nullptr;
		auto* Return = Function ? FindFProperty<FObjectPropertyBase>(Function, TEXT("ReturnValue")) : nullptr;
		if (!Return || Function->NumParms != 1) return nullptr;
		FStructOnScope Params(Function);
		Object->ProcessEvent(Function, Params.GetStructMemory());
		return Return->GetObjectPropertyValue_InContainer(Params.GetStructMemory());
	}

	UObject* GetConsolePart(const FName Getter)
	{
		UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/SuperConsole.SuperConsoleProSubsystem"));
		if (!GEngine || !Class || !Class->IsChildOf(UEngineSubsystem::StaticClass())) return nullptr;
		UObject* Console = GEngine->GetEngineSubsystemBase(Class);
		return CallObject(Console, Getter);
	}

	int32 ReadInt(const UStruct* Type, const void* Data, const FName Name)
	{
		const auto* Property = FindFProperty<FIntProperty>(Type, Name);
		return Property ? Property->GetPropertyValue_InContainer(Data) : 0;
	}

	FStructProperty* GetPatchProperty(AActor* Actor)
	{
		UClass* Base = FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperDmxActorBase"));
		return IsValid(Actor) && Base && Actor->IsA(Base)
			? FindFProperty<FStructProperty>(Actor->GetClass(), TEXT("SuperDMXFixture")) : nullptr;
	}
}

bool TSAVSuperStageDMX::ReadFixture(AActor* Actor, FFixture& Out)
{
	FStructProperty* Patch = Private::GetPatchProperty(Actor);
	if (!Patch) return false;
	if (!FindFProperty<FIntProperty>(Patch->Struct, TEXT("Universe"))
		|| !FindFProperty<FIntProperty>(Patch->Struct, TEXT("StartAddress"))) return false;
	Out = {};
	Out.Actor = Actor;
	const void* Data = Patch->ContainerPtrToValuePtr<void>(Actor);
	Out.FixtureId = Private::ReadInt(Patch->Struct, Data, TEXT("FixtureID"));
	Out.Universe = Private::ReadInt(Patch->Struct, Data, TEXT("Universe"));
	Out.Address = Private::ReadInt(Patch->Struct, Data, TEXT("StartAddress"));
	UFunction* GetModules = Actor->FindFunction(TEXT("GetModules"));
	auto* Modules = GetModules ? FindFProperty<FArrayProperty>(GetModules, TEXT("ReturnValue")) : nullptr;
	auto* ModuleType = Modules ? CastField<FStructProperty>(Modules->Inner) : nullptr;
	if (!ModuleType || GetModules->NumParms != 1) return true;
	FStructOnScope Params(GetModules);
	FEditorScriptExecutionGuard ScriptGuard;
	Actor->ProcessEvent(GetModules, Params.GetStructMemory());
	FScriptArrayHelper ModuleArray(Modules, Modules->ContainerPtrToValuePtr<void>(Params.GetStructMemory()));
	auto* Attributes = FindFProperty<FArrayProperty>(ModuleType->Struct, TEXT("AttributeDefs"));
	auto* AttributeType = Attributes ? CastField<FStructProperty>(Attributes->Inner) : nullptr;
	auto* NameProperty = AttributeType ? FindFProperty<FNameProperty>(AttributeType->Struct, TEXT("AttribName")) : nullptr;
	if (!NameProperty) return true;
	for (int32 Index = 0; Index < ModuleArray.Num(); ++Index)
	{
		void* ModuleData = ModuleArray.GetRawPtr(Index);
		const int32 Offset = Private::ReadInt(ModuleType->Struct, ModuleData, TEXT("Patch"));
		FScriptArrayHelper AttributeArray(Attributes, Attributes->ContainerPtrToValuePtr<void>(ModuleData));
		for (int32 AttrIndex = 0; AttrIndex < AttributeArray.Num(); ++AttrIndex)
		{
			void* AttributeData = AttributeArray.GetRawPtr(AttrIndex);
			FAttribute Attribute;
			Attribute.Name = NameProperty->GetPropertyValue_InContainer(AttributeData);
			Attribute.Instance = Index;
			const int32 Coarse = Private::ReadInt(AttributeType->Struct, AttributeData, TEXT("Coarse"));
			const int32 Fine = Private::ReadInt(AttributeType->Struct, AttributeData, TEXT("Fine"));
			const int32 Ultra = Private::ReadInt(AttributeType->Struct, AttributeData, TEXT("Ultra"));
			Attribute.Channel = Coarse > 0 ? Offset + Coarse : 0;
			Attribute.Fine = Fine > 0 ? Offset + Fine : 0;
			Attribute.Ultra = Ultra > 0 ? Offset + Ultra : 0;
			Out.Span = FMath::Max(Out.Span, FMath::Max3(Attribute.Channel, Attribute.Fine, Attribute.Ultra));
			if (!Attribute.Name.IsNone() && Attribute.Channel > 0) Out.Attributes.Add(Attribute);
		}
	}
	return true;
}

TArray<TSAVSuperStageDMX::FFixture> TSAVSuperStageDMX::GetSceneFixtures(UWorld* World)
{
	TArray<FFixture> Result;
	UClass* Base = FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperDmxActorBase"));
	if (!World || !Base) return Result;
	for (TActorIterator<AActor> It(World, Base); It; ++It)
	{
		FFixture Fixture;
		if (ReadFixture(*It, Fixture)) Result.Add(MoveTemp(Fixture));
	}
	return Result;
}

bool TSAVSuperStageDMX::SetPatch(AActor* Actor, const int32 Universe, const int32 Address)
{
	FFixture Fixture;
	if (!ReadFixture(Actor, Fixture) || Universe < 1 || Universe > 512 || Address < 1
		|| Fixture.Span < 1 || Address + Fixture.Span - 1 > 512) return false;
	FStructProperty* Patch = Private::GetPatchProperty(Actor);
	Actor->Modify();
	Actor->PreEditChange(Patch);
	void* Data = Patch->ContainerPtrToValuePtr<void>(Actor);
	FindFProperty<FIntProperty>(Patch->Struct, TEXT("Universe"))->SetPropertyValue_InContainer(Data, Universe);
	FindFProperty<FIntProperty>(Patch->Struct, TEXT("StartAddress"))->SetPropertyValue_InContainer(Data, Address);
	FPropertyChangedEvent Event(Patch, EPropertyChangeType::ValueSet);
	Actor->PostEditChangeProperty(Event);
	Actor->MarkPackageDirty();
	Private::CallVoid(Actor, TEXT("ForceRefreshDMX"));
	FFixture Updated;
	return ReadFixture(Actor, Updated) && Updated.Universe == Universe && Updated.Address == Address;
}

bool TSAVSuperStageDMX::SyncConsole(const bool bImportScene)
{
	UObject* Patch = Private::GetConsolePart(TEXT("GetPatchSubsystem"));
	if (!Private::CallVoid(Patch, TEXT("ScanScene"))) return false;
	if (bImportScene && !Private::CallVoid(Patch, TEXT("ImportAll"))) return false;
	return Private::CallVoid(Patch, TEXT("SyncAllFromScene"));
}

bool TSAVSuperStageDMX::IsOutputEnabled()
{
	UObject* DMX = Private::GetConsolePart(TEXT("GetConsoleDMXSubsystem"));
	UFunction* Function = DMX ? DMX->FindFunction(TEXT("IsOutputEnabled")) : nullptr;
	auto* Return = Function ? FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")) : nullptr;
	if (!Return || Function->NumParms != 1) return false;
	FStructOnScope Params(Function);
	DMX->ProcessEvent(Function, Params.GetStructMemory());
	return Return->GetPropertyValue_InContainer(Params.GetStructMemory());
}

bool TSAVSuperStageDMX::SetOutputEnabled(const bool bEnabled)
{
	UObject* DMX = Private::GetConsolePart(TEXT("GetConsoleDMXSubsystem"));
	UFunction* Function = DMX ? DMX->FindFunction(TEXT("SetOutputEnabled")) : nullptr;
	if (!Function || Function->NumParms != 1) return false;
	FStructOnScope Params(Function);
	bool bFoundParameter = false;
	for (TFieldIterator<FBoolProperty> It(Function); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			It->SetPropertyValue_InContainer(Params.GetStructMemory(), bEnabled);
			bFoundParameter = true;
		}
	}
	if (!bFoundParameter) return false;
	DMX->ProcessEvent(Function, Params.GetStructMemory());
	return IsOutputEnabled() == bEnabled;
}

int32 TSAVSuperStageDMX::GetConsoleFixtureId(AActor* Actor)
{
	FFixture Fixture;
	if (!ReadFixture(Actor, Fixture)) return INDEX_NONE;
	UObject* Patch = Private::GetConsolePart(TEXT("GetPatchSubsystem"));
	UFunction* Function = Patch ? Patch->FindFunction(TEXT("GetImportedFixtures")) : nullptr;
	auto* Rows = Function ? FindFProperty<FArrayProperty>(Function, TEXT("ReturnValue")) : nullptr;
	auto* RowType = Rows ? CastField<FStructProperty>(Rows->Inner) : nullptr;
	if (!RowType || Function->NumParms != 1) return INDEX_NONE;
	if (!FindFProperty<FIntProperty>(RowType->Struct, TEXT("Id"))) return INDEX_NONE;
	auto* Guid = FindFProperty<FStructProperty>(RowType->Struct, TEXT("ActorGuid"));
	if (!Guid || Guid->Struct != TBaseStructure<FGuid>::Get()) return INDEX_NONE;
	FStructOnScope Params(Function);
	Patch->ProcessEvent(Function, Params.GetStructMemory());
	FScriptArrayHelper Array(Rows, Rows->ContainerPtrToValuePtr<void>(Params.GetStructMemory()));
	for (int32 I = 0; I < Array.Num(); ++I)
	{
		const void* Data = Array.GetRawPtr(I);
		if (*Guid->ContainerPtrToValuePtr<FGuid>(Data) != Actor->GetActorGuid()) continue;
		// A stale console patch could send to another light. Require an exact address match.
		if (Private::ReadInt(RowType->Struct, Data, TEXT("DMXUniverse")) != Fixture.Universe
			|| Private::ReadInt(RowType->Struct, Data, TEXT("DMXAddress")) != Fixture.Address) return INDEX_NONE;
		return Private::ReadInt(RowType->Struct, Data, TEXT("Id"));
	}
	return INDEX_NONE;
}

bool TSAVSuperStageDMX::SendAttributes(AActor* Actor, const TArray<FValue>& Values)
{
	FFixture Fixture;
	if (!ReadFixture(Actor, Fixture) || !IsOutputEnabled()) return false;
	const int32 ConsoleId = GetConsoleFixtureId(Actor);
	if (ConsoleId == INDEX_NONE) return false;
	UObject* DMX = Private::GetConsolePart(TEXT("GetConsoleDMXSubsystem"));
	UFunction* Function = DMX ? DMX->FindFunction(TEXT("SetAttributeValue")) : nullptr;
	if (!Function || Function->NumParms != 5) return false;
	auto* Id = FindFProperty<FIntProperty>(Function, TEXT("FixtureID"));
	auto* Index = FindFProperty<FIntProperty>(Function, TEXT("InstanceIndex"));
	auto* Name = FindFProperty<FNameProperty>(Function, TEXT("AttributeName"));
	auto* Normalized = FindFProperty<FFloatProperty>(Function, TEXT("Value"));
	auto* Source = FindFProperty<FEnumProperty>(Function, TEXT("Source"));
	if (!Id || !Index || !Name || !Normalized || !Source) return false;
	const int64 Programmer = Source->GetEnum()->GetValueByNameString(TEXT("Programmer"));
	if (Programmer == INDEX_NONE) return false;
	bool bSent = false;
	for (const FValue& Value : Values)
	{
		for (const FAttribute& Attribute : Fixture.Attributes)
		{
			if (Attribute.Name != Value.Attribute || (Value.Instance != INDEX_NONE && Attribute.Instance != Value.Instance)) continue;
			FStructOnScope Params(Function);
			void* Data = Params.GetStructMemory();
			Id->SetPropertyValue_InContainer(Data, ConsoleId);
			Index->SetPropertyValue_InContainer(Data, Attribute.Instance);
			Name->SetPropertyValue_InContainer(Data, Value.Attribute);
			Normalized->SetPropertyValue_InContainer(Data, FMath::Clamp(Value.Value, 0.0f, 1.0f));
			Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Data), Programmer);
			DMX->ProcessEvent(Function, Data);
			bSent = true;
		}
	}
	if (bSent)
	{
		Private::CallVoid(DMX, TEXT("FlushOutput"));
		Private::CallVoid(Actor, TEXT("ForceRefreshDMX"));
	}
	return bSent;
}

bool TSAVSuperStageDMX::SendAttribute(AActor* Actor, FName Attribute, float Value, int32 Instance)
{
	return SendAttributes(Actor, {{Attribute, Value, Instance}});
}
