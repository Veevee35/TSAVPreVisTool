// Copyright TSAV. All Rights Reserved.
// Interchange reference: https://www.ilda.com/resources/StandardsDocs/ILDA_IDTF14_rev011.pdf
#include "Laser/TSAVLaserPreview.h"

namespace TSAVILDA
{
	static uint16 Read16(const uint8* P) { return (static_cast<uint16>(P[0])<<8)|P[1]; }
	static double Coordinate(const uint8* P) { const int16 V=static_cast<int16>(Read16(P)); return static_cast<double>(V)/(V<0?32768.0:32767.0); }
	static void Write16(TArray<uint8>& Bytes,uint16 Value) { Bytes.Add(Value>>8); Bytes.Add(Value&255); }
	static TArray<FColor> DefaultPalette()
	{
		TArray<FColor> Colors; Colors.Init(FColor::Black,256);
		for (int32 I=0; I<16; ++I) Colors[I]=FColor(255,I*16,0);
		Colors[16]=FColor(255,255,0); for (int32 I=17; I<24; ++I) Colors[I]=FColor((24-I)*32,255,0);
		const uint8 GreenBlue[]={0,36,73,109,146,182,219,255}; for (int32 I=24; I<=31; ++I) Colors[I]=FColor(0,255,GreenBlue[I-24]);
		const uint8 CyanGreen[]={227,198,170,142,113,85,56,28,0}; for (int32 I=32; I<=40; ++I) Colors[I]=FColor(0,CyanGreen[I-32],255);
		for (int32 I=41; I<48; ++I) Colors[I]=FColor((I-40)*32,0,255); Colors[48]=FColor(255,0,255);
		for (int32 I=49; I<56; ++I) Colors[I]=FColor(255,(I-48)*32,255); Colors[56]=FColor::White;
		for (int32 I=57; I<64; ++I) Colors[I]=FColor(255,(64-I)*32,(64-I)*32);
		return Colors;
	}
	bool Decode(const TArray<uint8>& Bytes,TArray<FTSAVLaserFrame>& Frames,FString& Error)
	{
		TArray<FTSAVLaserFrame> Candidate; TMap<int32,TArray<FColor>> Palettes;
		int64 Offset=0,TotalPoints=0;
		const auto Fail=[&](const TCHAR* Message) { Error=Message; return false; };
		while (Offset+32<=Bytes.Num()) {
			const uint8* Header=Bytes.GetData()+Offset;
			if (FMemory::Memcmp(Header,"ILDA",4)!=0) return Fail(TEXT("Invalid ILDA section signature."));
			const uint8 Format=Header[7],Projector=Header[30]; const int32 Count=Read16(Header+24); Offset+=32;
			if (Format!=0 && Format!=1 && Format!=2 && Format!=4 && Format!=5) return Fail(TEXT("Unsupported ILDA format code."));
			if (Count==0 && Format!=2) { Frames=MoveTemp(Candidate); Error.Reset(); return true; }
			if (Format==2 && (Count<2 || Count>256)) return Fail(TEXT("Invalid ILDA palette size."));
			const int32 Stride=Format==0?8:Format==1?6:Format==2?3:Format==4?10:8;
			if (Offset+static_cast<int64>(Count)*Stride>Bytes.Num()) return Fail(TEXT("Truncated ILDA section."));
			if (!Palettes.Contains(Projector)) Palettes.Add(Projector,DefaultPalette());
			if (Format==2) {
				auto& Palette=Palettes[Projector]; Palette.Init(FColor::Black,256);
				for (int32 I=0; I<Count; ++I) { const uint8* RGB=Bytes.GetData()+Offset+I*3; Palette[I]=FColor(RGB[0],RGB[1],RGB[2]); }
			} else {
				TotalPoints+=Count; if (TotalPoints>1000000 || Candidate.Num()>=65535) return Fail(TEXT("ILDA preview exceeds one million points or 65535 frames."));
				FTSAVLaserFrame Frame; Frame.Projector=Projector; Frame.b3D=Format==0 || Format==4;
				for (int32 I=8; I<16 && Header[I]; ++I) Frame.Name.AppendChar(Header[I]);
				Frame.Points.Reserve(Count);
				for (int32 I=0; I<Count; ++I) {
					const uint8* P=Bytes.GetData()+Offset+I*Stride; FTSAVLaserPoint Point;
					Point.Position=FVector(Coordinate(P),Coordinate(P+2),Frame.b3D?Coordinate(P+4):0);
					const int32 Status=Frame.b3D?6:4; Point.bBlank=(P[Status]&0x40)!=0;
					Point.Color=Format==0 || Format==1 ? Palettes[Projector][P[Status+1]] : FColor(P[Status+3],P[Status+2],P[Status+1]);
					Frame.Points.Add(Point);
				}
				Candidate.Add(MoveTemp(Frame));
			}
			Offset+=static_cast<int64>(Count)*Stride;
		}
		return Fail(TEXT("ILDA file has no complete end header."));
	}
	static void Header(TArray<uint8>& Bytes,uint8 Format,const FString& Name,uint16 Count,uint16 Number,uint16 Total,uint8 Projector)
	{
		const int32 Start=Bytes.Num(); Bytes.AddZeroed(32); uint8* P=Bytes.GetData()+Start;
		FMemory::Memcpy(P,"ILDA",4); P[7]=Format;
		for (int32 I=0; I<FMath::Min(8,Name.Len()); ++I) P[8+I]=Name[I]<128?Name[I]:'?';
		FMemory::Memcpy(P+16,"TSAV",4); P[24]=Count>>8; P[25]=Count&255; P[26]=Number>>8; P[27]=Number&255; P[28]=Total>>8; P[29]=Total&255; P[30]=Projector;
	}
	bool Encode(const TArray<FTSAVLaserFrame>& Frames,TArray<uint8>& Bytes,FString& Error,bool bIndexed)
	{
		FTSAVLaserData Check; Check.Frames=Frames;
		if (!ATSAVLaserPreview::Validate(Check)) { Error=TEXT("Invalid laser frame data."); return false; }
		TArray<uint8> Candidate;
		for (int32 I=0; I<Frames.Num(); ++I) {
			const auto& Frame=Frames[I]; TArray<FColor> Palette;
			if (bIndexed) {
				for (const auto& Point : Frame.Points) { Palette.AddUnique(Point.Color); if (Palette.Num()>256) break; }
				if (Palette.Num()>256) { Error=TEXT("A frame uses more than 256 colors; export true color instead."); return false; }
				while (Palette.Num()<2) Palette.Add(FColor::Black);
				Header(Candidate,2,TEXT("TSAV RGB"),Palette.Num(),0,0,Frame.Projector);
				for (const FColor Color : Palette) Candidate.Append({Color.R,Color.G,Color.B});
			}
			Header(Candidate,bIndexed?(Frame.b3D?0:1):(Frame.b3D?4:5),Frame.Name,Frame.Points.Num(),I,Frames.Num(),Frame.Projector);
			for (int32 J=0; J<Frame.Points.Num(); ++J) {
				const auto& Point=Frame.Points[J];
				const auto Pack=[](double Value) { return static_cast<uint16>(static_cast<int16>(FMath::RoundToInt(Value*(Value<0?32768:32767)))); };
				Write16(Candidate,Pack(Point.Position.X)); Write16(Candidate,Pack(Point.Position.Y)); if (Frame.b3D) Write16(Candidate,Pack(Point.Position.Z));
				Candidate.Add((Point.bBlank?0x40:0)|(J==Frame.Points.Num()-1?0x80:0));
				if (bIndexed) Candidate.Add(Palette.IndexOfByKey(Point.Color)); else Candidate.Append({Point.Color.B,Point.Color.G,Point.Color.R});
			}
		}
		Header(Candidate,5,TEXT(""),0,0,0,0); Bytes=MoveTemp(Candidate); Error.Reset(); return true;
	}
	TArray<FTSAVLaserFrame> MakePattern(int32 Pattern,int32 FrameCount,int32 PointCount)
	{
		TArray<FTSAVLaserFrame> Frames; FrameCount=FMath::Clamp(FrameCount,1,600); PointCount=FMath::Clamp(PointCount,8,1024);
		for (int32 F=0; F<FrameCount; ++F) {
			FTSAVLaserFrame Frame; Frame.Name=TEXT("TSAV"); const float Phase=F*2*PI/FrameCount;
			for (int32 I=0; I<=PointCount; ++I) {
				const float T=static_cast<float>(I)/PointCount,A=T*2*PI; FTSAVLaserPoint Point;
				Point.Position=Pattern==1?FVector(T*2-1,0.4f*FMath::Sin(Phase),0):Pattern==2?FVector(FMath::Sin(3*A+Phase),FMath::Sin(2*A),0):FVector(FMath::Cos(A)*(0.75f+0.2f*FMath::Sin(Phase)),FMath::Sin(A)*(0.75f+0.2f*FMath::Sin(Phase)),0);
				Point.Color=FLinearColor::MakeFromHSV8(static_cast<uint8>(T*255),255,255).ToFColorSRGB(); Point.bBlank=I==0; Frame.Points.Add(Point);
			}
			Frames.Add(MoveTemp(Frame));
		}
		return Frames;
	}
}
