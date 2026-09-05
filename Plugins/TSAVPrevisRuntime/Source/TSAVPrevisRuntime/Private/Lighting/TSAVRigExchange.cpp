// Copyright TSAV. All Rights Reserved.
// Interchange specifications: https://gdtf-share.com/help/developers/
#include "Lighting/TSAVRigExchange.h"
#include "Lighting/TSAVLightingShow.h"
#include "TSAVDMXFixture.h"
#include "TSAVDMXPatchPlan.h"
#include "DMXGDTF.h"
#include "DMXZipper.h"
#include "GDTF/DMXGDTFDescription.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "GDTF/DMXModes/DMXGDTFDMXChannel.h"
#include "GDTF/DMXModes/DMXGDTFLogicalChannel.h"
#include "GDTF/DMXModes/DMXGDTFChannelFunction.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "XmlFile.h"

namespace TSAVRigExchange
{
	static bool Fail(FString& Error,const FString& Message) { Error=Message; return false; }
	static FString Escape(FString Text) { return Text.Replace(TEXT("&"),TEXT("&amp;")).Replace(TEXT("<"),TEXT("&lt;")).Replace(TEXT(">"),TEXT("&gt;")).Replace(TEXT("\""),TEXT("&quot;")).Replace(TEXT("'"),TEXT("&apos;")); }
	static TArray64<uint8> Utf8(const FString& Text) { const FString Lines=Text.Replace(TEXT("><"),TEXT(">\n<")); FTCHARToUTF8 Encoded(*Lines); TArray64<uint8> Bytes; Bytes.Append(reinterpret_cast<const uint8*>(Encoded.Get()),Encoded.Length()); return Bytes; }
	static FString FromUtf8(const TArray64<uint8>& Bytes) { FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),static_cast<int32>(Bytes.Num())); return FString(Text.Length(),Text.Get()); }
	static FString ChildText(const FXmlNode* Node,const TCHAR* Tag) { const auto* Child=Node?Node->FindChildNode(Tag):nullptr; return Child?Child->GetContent():FString(); }
	static bool SafeName(const FString& Name) { return !Name.IsEmpty() && Name==FPaths::GetCleanFilename(Name) && !Name.Contains(TEXT(":")) && !Name.Contains(TEXT("..")); }
	static int64 DefaultValue(const FString& Text,int32 TargetBytes) {
		FString ValueText,Resolution; uint64 Value=0; int32 Bytes=0;
		if (!Text.Split(TEXT("/"),&ValueText,&Resolution) || !LexTryParseString(Value,*ValueText)) return 0;
		const bool bShift=Resolution.RemoveFromEnd(TEXT("s"));
		if (!LexTryParseString(Bytes,*Resolution) || Bytes<1 || Bytes>4) return 0;
		const uint64 SourceMax=(1ULL<<(Bytes*8))-1,TargetMax=(1ULL<<(TargetBytes*8))-1; Value=FMath::Min(Value,SourceMax);
		if (!bShift) return static_cast<int64>(Value*TargetMax/SourceMax);
		return static_cast<int64>(Bytes<=TargetBytes?Value<<((TargetBytes-Bytes)*8):Value>>((Bytes-TargetBytes)*8));
	}

	bool ReadGDTF(const TArray64<uint8>& Bytes,const FString& FileName,FTSAVGDTFProfile& Profile,FString& Error)
	{
		if (Bytes.Num()<22 || Bytes.Num()>64*1024*1024) return Fail(Error,TEXT("GDTF files must be no larger than 64 MB."));
		FDMXZipper Zip; TArray64<uint8> Description;
		if (!Zip.LoadFromData(Bytes) || !Zip.GetFileContent(TEXT("description.xml"),Description) || Description.Num()>16*1024*1024) return Fail(Error,TEXT("Missing or oversized GDTF description.xml."));
		// UE's XML preprocessor removes declaration lines, so isolate a minified declaration.
		const auto Xml=MakeShared<FXmlFile>(FromUtf8(Description).Replace(TEXT("?><"),TEXT("?>\n<")),EConstructMethod::ConstructFromBuffer);
		if (!Xml->IsValid() || !Xml->GetRootNode() || Xml->GetRootNode()->GetTag()!=TEXT("GDTF")) return Fail(Error,TEXT("Invalid GDTF XML: ")+Xml->GetLastError());
		const auto Parsed=MakeShared<UE::DMX::GDTF::FDMXGDTFDescription>(); Parsed->InitializeFromDescriptionXml(Xml);
		const auto Type=Parsed->GetFixtureType();
		if (!Type || Type->DMXModes.IsEmpty()) return Fail(Error,TEXT("The GDTF has no fixture type or modes."));
		FTSAVGDTFProfile Candidate; Candidate.Id=Type->FixtureTypeID; Candidate.Name=Type->Name.ToString(); Candidate.Manufacturer=Type->Manufacturer; Candidate.Bytes=Bytes; Candidate.FileName=FPaths::GetCleanFilename(FileName);
		if (!Candidate.Id.IsValid()) Candidate.Id=FGuid::NewGuid();
		for (const auto& SourceMode : Type->DMXModes) {
			if (!SourceMode) continue;
			FTSAVGDTFMode Mode; Mode.DMX.ModeName=SourceMode->Name.ToString(); Mode.DMX.bAutoChannelSpan=false; Mode.DMX.ChannelSpan=0;
			TSet<int32> Occupied; TMap<FName,int32> Counts; bool bSupported=true;
			for (const auto& Channel : SourceMode->DMXChannels) {
				if (!Channel || Channel->Offset.IsEmpty()) continue;
				if (Channel->DMXBreak!=1 || Channel->Offset.Num()>4 || Channel->LogicalChannelArray.IsEmpty() || !Channel->LogicalChannelArray[0] || !Channel->ResolveGeometryReferences().IsEmpty()) { bSupported=false; break; }
				const bool bLSB=Channel->Offset.Num()>1 && Channel->Offset[0]>Channel->Offset[1];
				const int32 First=bLSB?Channel->Offset.Last():Channel->Offset[0];
				for (int32 I=0; I<Channel->Offset.Num(); ++I) {
					const int32 Expected=bLSB?First+Channel->Offset.Num()-1-I:First+I;
					if (Channel->Offset[I]!=Expected || Expected<1 || Expected>512 || Occupied.Contains(Expected)) { bSupported=false; break; }
					Occupied.Add(Expected);
				}
				if (!bSupported) break;
				const FName Attribute=Channel->LogicalChannelArray[0]->Attribute;
				if (Attribute.IsNone()) { bSupported=false; break; }
				int32& Count=Counts.FindOrAdd(Attribute); ++Count;
				FDMXFixtureFunction Function; Function.Attribute=FDMXAttributeName(Count==1?Attribute:FName(*FString::Printf(TEXT("%s_%d"),*Attribute.ToString(),Count)));
				Function.FunctionName=Function.Attribute.Name.ToString(); Function.Channel=First; Function.DataType=static_cast<EDMXFixtureSignalFormat>(Channel->Offset.Num()-1); Function.bUseLSBMode=bLSB;
				if (const auto Initial=Channel->ResolveInitialFunction()) {
					// UE 5.8's Get helper rejects set values; resolve byte mirroring/shifting directly.
					Function.DefaultValue=DefaultValue(Initial->Default.AsString(),Function.GetNumChannels());
					if (FMath::IsFinite(Initial->PhysicalFrom) && FMath::IsFinite(Initial->PhysicalTo)) Mode.PhysicalRanges.Add(ATSAVLightingShow::CanonicalAttribute(Attribute),FVector2D(Initial->PhysicalFrom,Initial->PhysicalTo));
				}
				Mode.DMX.ChannelSpan=FMath::Max(Mode.DMX.ChannelSpan,Function.GetLastChannel()); Mode.DMX.Functions.Add(Function);
			}
			if (bSupported && Mode.DMX.ChannelSpan>0) Candidate.Modes.Add(MoveTemp(Mode));
			else Candidate.Warnings.Add(SourceMode->Name.ToString()+TEXT(": requires geometry references, multiple DMX breaks, split offsets, or has no channels; use the editor fixture builder."));
		}
		if (Candidate.Modes.IsEmpty()) return Fail(Error,TEXT("No modes supported by the runtime profile importer. Use the editor GDTF fixture builder for this profile."));
		Profile=MoveTemp(Candidate); Error.Reset(); return true;
	}
	bool LoadGDTF(const FString& Path,FTSAVGDTFProfile& Profile,FString& Error)
	{
		const int64 Size=IFileManager::Get().FileSize(*Path); TArray64<uint8> Bytes;
		if (Size<22 || Size>64*1024*1024 || !FFileHelper::LoadFileToArray(Bytes,*Path)) return Fail(Error,TEXT("Choose a readable GDTF file up to 64 MB."));
		return ReadGDTF(Bytes,FPaths::GetCleanFilename(Path),Profile,Error);
	}
	bool ApplyProfile(ATSAVDMXFixture* Fixture,const FTSAVGDTFProfile& Profile,const FString& ModeName,int32 Universe,int32 Address,FString& Error)
	{
		const auto* Mode=Profile.Modes.FindByPredicate([&](const auto& Item) { return Item.DMX.ModeName==ModeName; });
		if (!Fixture || !Mode) return Fail(Error,TEXT("Choose an available fixture mode."));
		if (!Fixture->SetStandaloneMode(Mode->DMX,Universe,Address)) return Fail(Error,TEXT("The mode does not fit at this patch address."));
		Fixture->ExchangeGDTFBase64=FBase64::Encode(Profile.Bytes.GetData(),static_cast<uint32>(Profile.Bytes.Num()));
		Fixture->ExchangeGDTFName=Profile.FileName; Fixture->ExchangeProfileId=Profile.Id; Fixture->GDTFModeName=ModeName;
		if (const auto* Range=Mode->PhysicalRanges.Find(TEXT("pan"))) { Fixture->PanMinDegrees=Range->X; Fixture->PanMaxDegrees=Range->Y; }
		if (const auto* Range=Mode->PhysicalRanges.Find(TEXT("tilt"))) { Fixture->TiltMinDegrees=Range->X; Fixture->TiltMaxDegrees=Range->Y; }
		if (const auto* Range=Mode->PhysicalRanges.Find(TEXT("zoom"))) { Fixture->MinimumBeamAngleDegrees=FMath::Clamp(static_cast<float>(Range->X)*0.5f,1.0f,89.0f); Fixture->MaximumBeamAngleDegrees=FMath::Clamp(static_cast<float>(Range->Y)*0.5f,1.0f,89.0f); }
		FDMXNormalizedAttributeValueMap Defaults;
		for (const auto& Function : Mode->DMX.Functions) { const uint64 Max=Function.GetNumChannels()==4?MAX_uint32:(1ULL<<(8*Function.GetNumChannels()))-1; Defaults.Map.Add(Function.Attribute,static_cast<float>(static_cast<double>(Function.DefaultValue)/Max)); }
		Fixture->ApplyAttributeValues(Defaults,true); Fixture->MarkPackageDirty(); Error.Reset(); return true;
	}
	bool GenerateGDTF(ATSAVDMXFixture* Fixture,TArray64<uint8>& Bytes,FString& Error)
	{
		const auto* Patch=IsValid(Fixture)?Fixture->GetFixturePatch():nullptr; const auto* Mode=Patch?Patch->GetActiveMode():nullptr;
		if (!Mode) return Fail(Error,TEXT("Every exported fixture needs a patch and mode."));
		if (!Fixture->ExchangeGDTFBase64.IsEmpty()) {
			TArray<uint8> Original; if (!FBase64::Decode(Fixture->ExchangeGDTFBase64,Original)) return Fail(Error,TEXT("Invalid retained GDTF data."));
			Bytes=Original; return true;
		}
		if (Mode->bFixtureMatrixEnabled) return Fail(Error,TEXT("Matrix export requires an original GDTF profile. Export a non-matrix rig or import its original profile first."));
		if (!Fixture->ExchangeProfileId.IsValid()) { Fixture->Modify(); Fixture->ExchangeProfileId=FGuid::NewGuid(); Fixture->MarkPackageDirty(); }
		FString Definitions,Channels;
		for (const auto& Function : Mode->Functions) {
			const FString A=Escape(Function.Attribute.Name.ToString()); FString Offsets;
			for (int32 I=0; I<Function.GetNumChannels(); ++I) { if (I) Offsets+=TEXT(","); Offsets+=FString::FromInt(Function.Channel+(Function.bUseLSBMode?Function.GetNumChannels()-1-I:I)); }
			Definitions+=FString::Printf(TEXT("<Attribute Name=\"%s\" Pretty=\"%s\" Feature=\"Control.Control\" PhysicalUnit=\"None\"/>"),*A,*A);
			FVector2D Physical(0,1); const FName Key=ATSAVLightingShow::CanonicalAttribute(Function.Attribute.Name);
			if (Key==TEXT("pan")) Physical=FVector2D(Fixture->PanMinDegrees,Fixture->PanMaxDegrees);
			else if (Key==TEXT("tilt")) Physical=FVector2D(Fixture->TiltMinDegrees,Fixture->TiltMaxDegrees);
			else if (Key==TEXT("zoom")) Physical=FVector2D(Fixture->MinimumBeamAngleDegrees*2,Fixture->MaximumBeamAngleDegrees*2);
			Channels+=FString::Printf(TEXT("<DMXChannel DMXBreak=\"1\" Offset=\"%s\" Geometry=\"Root\"><LogicalChannel Attribute=\"%s\"><ChannelFunction Name=\"%s\" Attribute=\"%s\" DMXFrom=\"0/1\" Default=\"%lld/%d\" PhysicalFrom=\"%.9g\" PhysicalTo=\"%.9g\"/></LogicalChannel></DMXChannel>"),*Offsets,*A,*A,*A,Function.DefaultValue,Function.GetNumChannels(),Physical.X,Physical.Y);
		}
		const FString Xml=FString::Printf(TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?><GDTF DataVersion=\"1.2\"><FixtureType Name=\"TSAVFixture\" ShortName=\"TSAV\" LongName=\"TSAV Native Fixture\" Manufacturer=\"TSAV\" Description=\"Native TSAV DMX mode\" FixtureTypeID=\"%s\" CanHaveChildren=\"No\"><AttributeDefinitions><FeatureGroups><FeatureGroup Name=\"Control\" Pretty=\"Control\"><Feature Name=\"Control\"/></FeatureGroup></FeatureGroups><Attributes>%s</Attributes></AttributeDefinitions><Geometries><Geometry Name=\"Root\" Position=\"{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}\"/></Geometries><DMXModes><DMXMode Name=\"%s\" Geometry=\"Root\"><DMXChannels>%s</DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>"),*Fixture->ExchangeProfileId.ToString(EGuidFormats::DigitsWithHyphens),*Definitions,*Escape(Mode->ModeName),*Channels);
		FDMXZipper Zip; Zip.AddFile(TEXT("description.xml"),Utf8(Xml)); return Zip.GetData(Bytes);
	}
	bool ParseMatrix(const FString& Text,FTransform& Transform)
	{
		if (Text.TrimStartAndEnd().IsEmpty()) { Transform=FTransform::Identity; return true; }
		FString Flat=Text.Replace(TEXT("{"),TEXT("")).Replace(TEXT("}"),TEXT(",")); TArray<FString> Parts; Flat.ParseIntoArray(Parts,TEXT(","),true);
		if (Parts.Num()!=12) return false;
		double V[12]; for (int32 I=0; I<12; ++I) if (!LexTryParseString(V[I],*Parts[I].TrimStartAndEnd()) || !FMath::IsFinite(V[I])) return false;
		// Convert right-handed Z-up millimeters to Unreal's left-handed centimeters.
		FMatrix M=FMatrix::Identity;
		for (int32 R=0; R<3; ++R) for (int32 C=0; C<3; ++C) M.M[R][C]=V[R*3+C]*(R==1?-1:1)*(C==1?-1:1);
		M.M[3][0]=V[9]/10; M.M[3][1]=-V[10]/10; M.M[3][2]=V[11]/10;
		Transform=FTransform(M); if (Transform.ContainsNaN() || Transform.GetScale3D().GetAbsMin()<0.00001) return false;
		// FTransform cannot preserve shear. Refuse to silently distort a transferred rig.
		return Transform.ToMatrixWithScale().Equals(M,0.0001);
	}
	FString WriteMatrix(const FTransform& Transform)
	{
		const FMatrix M=Transform.ToMatrixWithScale(); FString Text;
		for (int32 R=0; R<4; ++R) { Text+=TEXT("{"); for (int32 C=0; C<3; ++C) { if (C) Text+=TEXT(","); const double V=M.M[R][C]*(R==1?-1:1)*(C==1?-1:1)*(R==3?10:1); Text+=FString::Printf(TEXT("%.9g"),V); } Text+=TEXT("}"); }
		return Text;
	}
	bool ReadMVR(const FString& Path,FTSAVRigDocument& Document,FString& Error)
	{
		if (IFileManager::Get().FileSize(*Path)>256*1024*1024) return Fail(Error,TEXT("MVR rig import is limited to 256 MB."));
		FDMXZipper Zip; TArray64<uint8> XmlBytes;
		if (!Zip.LoadFromFile(Path) || !Zip.GetFileContent(TEXT("GeneralSceneDescription.xml"),XmlBytes) || XmlBytes.Num()>16*1024*1024) return Fail(Error,TEXT("Missing or oversized MVR GeneralSceneDescription.xml."));
		FXmlFile Xml(FromUtf8(XmlBytes).Replace(TEXT("?><"),TEXT("?>\n<")),EConstructMethod::ConstructFromBuffer); const auto* Root=Xml.GetRootNode();
		if (!Xml.IsValid() || !Root || Root->GetTag()!=TEXT("GeneralSceneDescription") || Root->GetAttribute(TEXT("verMajor"))!=TEXT("1")) return Fail(Error,TEXT("Unsupported MVR document."));
		const auto* Scene=Root->FindChildNode(TEXT("Scene")); const auto* Layers=Scene?Scene->FindChildNode(TEXT("Layers")):nullptr;
		if (!Layers) return Fail(Error,TEXT("MVR contains no scene layers."));
		FTSAVRigDocument Candidate; TSet<FGuid> Ids;
		TFunction<bool(const FXmlNode*,const FTransform&,int32)> Walk;
		Walk=[&](const FXmlNode* Node,const FTransform& Parent,int32 Depth) {
			if (Depth>64 || Candidate.Fixtures.Num()>10000) return Fail(Error,TEXT("MVR hierarchy or fixture count exceeds the import limit."));
			FTransform Local; if (!ParseMatrix(ChildText(Node,TEXT("Matrix")),Local)) return Fail(Error,TEXT("MVR contains an invalid or sheared transform."));
			const FTransform Absolute=Local*Parent;
			if (Node->GetTag()==TEXT("Fixture")) {
				FTSAVRigFixture Fixture; Fixture.Name=Node->GetAttribute(TEXT("name")); Fixture.Transform=Absolute;
				if (!FGuid::Parse(Node->GetAttribute(TEXT("uuid")),Fixture.Id) || !Fixture.Id.IsValid() || Ids.Contains(Fixture.Id)) return Fail(Error,TEXT("MVR fixture UUIDs must be valid and unique."));
				Ids.Add(Fixture.Id); Fixture.Profile=ChildText(Node,TEXT("GDTFSpec")); Fixture.Mode=ChildText(Node,TEXT("GDTFMode"));
				if (!Fixture.Profile.EndsWith(TEXT(".gdtf"))) Fixture.Profile+=TEXT(".gdtf");
				if (!SafeName(Fixture.Profile)) return Fail(Error,TEXT("MVR GDTF references must name files at the archive root."));
				const auto* Addresses=Node->FindChildNode(TEXT("Addresses")); const auto* AddressNode=Addresses?Addresses->FindChildNode(TEXT("Address")):nullptr;
				if (!AddressNode) return Fail(Error,TEXT("Every imported fixture needs a DMX address."));
				FString U,A; const FString Address=AddressNode->GetContent().TrimStartAndEnd();
				if (Address.Split(TEXT("."),&U,&A)) { if (!LexTryParseString(Fixture.Universe,*U) || !LexTryParseString(Fixture.Address,*A)) return Fail(Error,TEXT("Invalid universe.address.")); }
				else { int64 AbsoluteAddress=0; if (!LexTryParseString(AbsoluteAddress,*Address) || AbsoluteAddress<1 || AbsoluteAddress>63999LL*512) return Fail(Error,TEXT("Invalid absolute DMX address.")); Fixture.Universe=(AbsoluteAddress-1)/512+1; Fixture.Address=(AbsoluteAddress-1)%512+1; }
				if (Fixture.Universe<1 || Fixture.Universe>63999 || Fixture.Address<1 || Fixture.Address>512 || Addresses->GetChildrenNodes().Num()!=1) return Fail(Error,TEXT("Runtime rig import requires one valid DMX break per fixture."));
				if (!Candidate.Profiles.Contains(Fixture.Profile)) {
					TArray64<uint8> GDTF; FTSAVGDTFProfile Profile;
					if (!Zip.GetFileContent(Fixture.Profile,GDTF) || !ReadGDTF(GDTF,Fixture.Profile,Profile,Error)) return Fail(Error,Fixture.Profile+TEXT(": ")+Error);
					Candidate.Profiles.Add(Fixture.Profile,MoveTemp(Profile));
				}
				const auto* Mode=Candidate.Profiles[Fixture.Profile].Modes.FindByPredicate([&](const auto& Item) { return Item.DMX.ModeName==Fixture.Mode; });
				if (!Mode || Fixture.Address+Mode->DMX.ChannelSpan-1>512) return Fail(Error,TEXT("MVR requests an unavailable mode or a patch that crosses a universe."));
				Candidate.Fixtures.Add(MoveTemp(Fixture));
			} else if (Node->GetTag()!=TEXT("Layer") && Node->GetTag()!=TEXT("GroupObject")) Candidate.Warnings.AddUnique(Node->GetTag()+TEXT(" objects are outside fixture-rig import and remain in the source MVR."));
			if (const auto* Children=Node->FindChildNode(TEXT("ChildList"))) for (const auto* Child : Children->GetChildrenNodes()) if (!Walk(Child,Absolute,Depth+1)) return false;
			return true;
		};
		for (const auto* Layer : Layers->GetChildrenNodes()) if (!Walk(Layer,FTransform::Identity,0)) return false;
		if (Candidate.Fixtures.IsEmpty()) return Fail(Error,TEXT("MVR contains no importable lighting fixtures."));
		Document=MoveTemp(Candidate); Error.Reset(); return true;
	}
	bool ExportMVR(const FString& Path,const TArray<ATSAVDMXFixture*>& Fixtures,FString& Error)
	{
		if (Path.IsEmpty() || Fixtures.IsEmpty()) return Fail(Error,TEXT("Choose fixtures and an output filename."));
		FDMXZipper Zip; FString Nodes; TSet<FGuid> Seen;
		for (auto* Fixture : Fixtures) {
			if (!IsValid(Fixture)) continue;
			const FGuid Id=ATSAVLightingShow::FixtureId(Fixture); if (Seen.Contains(Id)) continue; Seen.Add(Id);
			TArray64<uint8> GDTF; if (!GenerateGDTF(Fixture,GDTF,Error)) return false;
			const FString Profile=TEXT("TSAV-")+Id.ToString(EGuidFormats::Digits)+TEXT(".gdtf"); Zip.AddFile(Profile,GDTF);
			const auto* Patch=Fixture->GetFixturePatch();
			Nodes+=FString::Printf(TEXT("<Fixture name=\"%s\" uuid=\"%s\"><Matrix>%s</Matrix><GDTFSpec>%s</GDTFSpec><GDTFMode>%s</GDTFMode><FixtureID>%d</FixtureID><Addresses><Address break=\"0\">%lld</Address></Addresses></Fixture>"),*Escape(Fixture->GetActorNameOrLabel()),*Id.ToString(EGuidFormats::DigitsWithHyphens),*WriteMatrix(Fixture->GetActorTransform()),*Profile,*Escape(Patch->GetActiveMode()->ModeName),Seen.Num(),(static_cast<int64>(Patch->GetUniverseID())-1)*512+Patch->GetStartingChannel());
		}
		const FString Xml=FString::Printf(TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?><GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"TSAV PreVis\" providerVersion=\"1\"><Scene><Layers><Layer name=\"TSAV Rig\" uuid=\"%s\"><ChildList>%s</ChildList></Layer></Layers></Scene></GeneralSceneDescription>"),*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),*Nodes);
		Zip.AddFile(TEXT("GeneralSceneDescription.xml"),Utf8(Xml)); IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
		const FString Temp=Path+TEXT(".tmp"); if (!Zip.SaveToFile(Temp) || !IFileManager::Get().Move(*Path,*Temp,true,true)) return Fail(Error,TEXT("Could not save MVR rig."));
		Error.Reset(); return true;
	}
	bool ImportRig(UWorld* World,const FTSAVRigDocument& Document,TArray<ATSAVDMXFixture*>& Created,FString& Error)
	{
		if (!World || Document.Fixtures.IsEmpty()) return Fail(Error,TEXT("Choose a nonempty rig."));
		TSet<FGuid> Existing; for (TActorIterator<ATSAVDMXFixture> It(World); It; ++It) Existing.Add(ATSAVLightingShow::FixtureId(*It,false));
		for (const auto& Item : Document.Fixtures) {
			if (!Item.Id.IsValid() || Item.Transform.ContainsNaN()) return Fail(Error,TEXT("Invalid rig identity or transform."));
			if (Existing.Contains(Item.Id)) return Fail(Error,TEXT("This rig contains fixture IDs already in the scene. Import into a new scene to retain their identities."));
			Existing.Add(Item.Id);
		}
		TArray<ATSAVDMXFixture*> Staged; bool bCommitted=false;
		ON_SCOPE_EXIT { if (!bCommitted) for (auto* Actor : Staged) Actor->Destroy(); };
		for (const auto& Item : Document.Fixtures) {
			const auto* Profile=Document.Profiles.Find(Item.Profile); if (!Profile) return Fail(Error,TEXT("Rig profile missing."));
			FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transactional; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor=World->SpawnActor<ATSAVDMXFixture>(ATSAVDMXFixture::StaticClass(),Item.Transform,Params); if (!Actor) return Fail(Error,TEXT("Could not create fixture.")); Staged.Add(Actor);
			if (!ApplyProfile(Actor,*Profile,Item.Mode,Item.Universe,Item.Address,Error)) return false;
			auto* Identity=UTSAVSceneObjectComponent::EnsureForActor(Actor); Identity->ObjectId=Item.Id; Identity->DisplayName=FText::FromString(Item.Name); Identity->ObjectType=ETSAVObjectType::Fixture;
#if WITH_EDITOR
			Actor->SetActorLabel(Item.Name);
#endif
		}
		Created=MoveTemp(Staged); bCommitted=true; Error.Reset(); return true;
	}
	bool PlaceGrid(UWorld* World,ATSAVDMXFixture* Template,const FTSAVGDTFProfile* Profile,int32 ModeIndex,FIntPoint Count,FVector2D Spacing,const FTransform& Origin,int32 Universe,int32 Address,TArray<ATSAVDMXFixture*>& Created,FString& Error)
	{
		if (!World || Count.X<1 || Count.Y<1 || static_cast<int64>(Count.X)*Count.Y>512 || Spacing.ContainsNaN() || Origin.ContainsNaN()) return Fail(Error,TEXT("Use a grid of 1–512 fixtures and finite dimensions."));
		const auto* Patch=IsValid(Template)?Template->GetFixturePatch():nullptr;
		const FDMXFixtureMode* Mode=Profile && Profile->Modes.IsValidIndex(ModeIndex)?&Profile->Modes[ModeIndex].DMX:Patch?Patch->GetActiveMode():nullptr;
		if (!Mode) return Fail(Error,TEXT("Choose a placed fixture or a loaded GDTF mode."));
		TArray<TSAVDMXPatchPlan::FRange> Selected,Occupied,Plan;
		for (int32 I=0; I<Count.X*Count.Y; ++I) Selected.Add({FName(*FGuid::NewGuid().ToString()),TEXT("New fixture"),Universe,Address,Mode->ChannelSpan});
		for (TActorIterator<ATSAVDMXFixture> It(World); It; ++It) if (const auto* P=It->GetFixturePatch()) Occupied.Add({It->GetFName(),It->GetActorNameOrLabel(),P->GetUniverseID(),P->GetStartingChannel(),P->GetChannelSpan()});
		if (!TSAVDMXPatchPlan::Build(Selected,Occupied,Universe,Address,Plan,Error)) return false;
		const FString Snapshot=Template?Template->CaptureTSAVState():FString(); TArray<ATSAVDMXFixture*> Staged; bool bCommitted=false;
		ON_SCOPE_EXIT { if (!bCommitted) for (auto* Actor : Staged) Actor->Destroy(); };
		for (int32 I=0; I<Plan.Num(); ++I) {
			const FVector Position=Origin.TransformPosition(FVector((I%Count.X)*Spacing.X,(I/Count.X)*Spacing.Y,0));
			FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transactional; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor=World->SpawnActor<ATSAVDMXFixture>(ATSAVDMXFixture::StaticClass(),FTransform(Origin.GetRotation(),Position,Origin.GetScale3D()),Params);
			if (!Actor) return Fail(Error,TEXT("Could not create fixture grid.")); Staged.Add(Actor);
			if (Profile) { if (!ApplyProfile(Actor,*Profile,Mode->ModeName,Plan[I].Universe,Plan[I].Address,Error)) return false; }
			else if (!Actor->RestoreTSAVState(Snapshot) || !Actor->SetIndividualPatchAddress(Plan[I].Universe,Plan[I].Address)) return Fail(Error,TEXT("Could not duplicate the fixture mode."));
			const FString Name=FString::Printf(TEXT("%s %d"),Profile?*Profile->Name:*Template->GetActorNameOrLabel(),I+1);
			auto* Identity=UTSAVSceneObjectComponent::EnsureForActor(Actor); Identity->ObjectType=ETSAVObjectType::Fixture; Identity->DisplayName=FText::FromString(Name);
#if WITH_EDITOR
			Actor->SetActorLabel(Name);
#endif
		}
		Created=MoveTemp(Staged); bCommitted=true; Error.Reset(); return true;
	}
}
