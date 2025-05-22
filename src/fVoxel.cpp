#include "FVoxel.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <cmath>

// ----------------------------------------------------------------------------------------------------
// FVoxelChunk
// ----------------------------------------------------------------------------------------------------

fUInt fVoxelChunk::GetVoxelIndex(fUInt IN_X, fUInt IN_Y, fUInt IN_Z) {
	fVector3ui ChunkSize = WorldPtr->GetChunkSize();
	// X > Z > Y
	
	if (IN_X >= ChunkSize.X) { return F_UINT_MAX; }
	if (IN_Y >= ChunkSize.Y) { return F_UINT_MAX; }
	if (IN_Z >= ChunkSize.Z) { return F_UINT_MAX; }
	
	return (IN_Y * (ChunkSize.Z * ChunkSize.X)) + (IN_Z * ChunkSize.X) + IN_X;
}

// ----------------------------------------------------------------------------------------------------
// I/O Stuff
// ----------------------------------------------------------------------------------------------------

fBool fVoxelWorld::isFileExist(std::string IN_FileName) {
	return std::filesystem::exists(IN_FileName);
}
fBool fVoxelWorld::CreateEmptyFile(std::string IN_FileName) {
	std::ofstream OutFile(IN_FileName);
	if (!OutFile.is_open()) { return false; }
	OutFile.close();
	return true;
}

fBool fVoxelWorld::SaveBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize) {
	std::scoped_lock Lock(IO_Lock);
	
	// ---------------------------------------------------------------------------------
	// Open Requested File
	std::ofstream OutFile(IN_FileName, std::ios::binary | std::ios::out);
	if (!OutFile.is_open()) {
		Log(
			F_LOG_SEV_ERROR,
			"IO",
			"Unable to open requested file at [" + IN_FileName + "]"
		);
		return false;
	}
	// ---------------------------------------------------------------------------------
	// Write Data
	
	OutFile.write((char*)IN_DataPtr, IN_DataSize);
	
	// ---------------------------------------------------------------------------------
	// All Done :D
	OutFile.close();
	return true;
}
fBool fVoxelWorld::AppendBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset) {
	std::scoped_lock Lock(IO_Lock);
	
	// ---------------------------------------------------------------------------------
	// Open Requested File
	// use Both ios::in and ios::out - if Only ios::out used, file content will be cleared on open
	std::ofstream OutFile(IN_FileName, std::ios::binary | std::ios::in | std::ios::out);
	if (!OutFile.is_open()) {
		Log(
			F_LOG_SEV_ERROR,
			"IO",
			"Unable to open requested file at [" + IN_FileName + "]"
		);
		return false;
	}
	
	OutFile.seekp(0, std::ios::end);
	fLong FileSize = OutFile.tellp();
	
	// ---------------------------------------------------------------------------------
	// Seek to Correct Position
	if (IN_Offset < F_LONG_MAX) {
		if (IN_Offset > FileSize) {
			Log(
				F_LOG_SEV_ERROR,
				"IO",
				"Unable to Save Binary file. Seek to [" + std::to_string(IN_Offset) + "] is out of bounds (FileSize = " + std::to_string(FileSize) + ")"
			);
			return false;
		}
		else {
			OutFile.seekp(0, std::ios::beg);
			OutFile.seekp(IN_Offset);
		}
	}
	// ---------------------------------------------------------------------------------
	// Write Data
	
	OutFile.write((char*)IN_DataPtr, IN_DataSize);
	
	// ---------------------------------------------------------------------------------
	// All Done :D
	OutFile.close();
	return true;
}

fBool fVoxelWorld::LoadBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset) {
	std::scoped_lock Lock(IO_Lock);
	
	// Open Requsted File
	std::ifstream InFile(IN_FileName, std::ios::binary | std::ios::in);
	if (!InFile.is_open()) {
		Log(
			F_LOG_SEV_ERROR,
			"IO",
			"Unable to open requested file at [" + IN_FileName + "]"
		);
		return false;
	}
	InFile.seekg(0, std::ios::end);
	fLong FileSize = InFile.tellg();
	InFile.seekg(0, std::ios::beg);
	
	// Seek to Position
	if (IN_Offset < F_LONG_MAX) {
		if (IN_Offset > FileSize) {
			Log(
				F_LOG_SEV_ERROR,
				"IO",
				"Unable to Load Binary file. Seek to offset [" + std::to_string(IN_Offset) + "] is out of bounds (FileSize = " + std::to_string(FileSize) + ")"
			);
			return false;
		}
		else { InFile.seekg(IN_Offset); }
	}
	
	// See if we have enough data to read
	fLong CurrPos = InFile.tellg();
	if ((FileSize - CurrPos) < IN_DataSize) {
		Log(
			F_LOG_SEV_ERROR,
			"IO",
			"Unable to Read requested number of Bytes. Requested [" + std::to_string(IN_DataSize) + "], Available [" + std::to_string(FileSize) + "]"
		);
		return false;
	}
	
	// Read Data 
	InFile.read((char*)IN_DataPtr, IN_DataSize);
	
	// All Done :D
	InFile.close();
	return true;
}
fBool fVoxelWorld::LoadBinaryData(std::string IN_FileName, fUChar** OUT_DataPtr, fLong& OUT_BufferSize) {
	std::scoped_lock Lock(IO_Lock);
	
	// Open Requsted File
	std::ifstream InFile(IN_FileName, std::ios::binary | std::ios::in);
	if (!InFile.is_open()) {
		Log(
			F_LOG_SEV_ERROR,
			"IO",
			"Unable to open requested file at [" + IN_FileName + "]"
		);
		return false;
	}
	InFile.seekg(0, std::ios::end);
	fLong FileSize = InFile.tellg();
	InFile.seekg(0, std::ios::beg);
	
	// Create Buffer
	fUChar* Buffer = (fUChar*)DefaultMemAllocator(sizeof(fUChar) * FileSize);
	
	// Read File into Buffer
	InFile.read((char*)Buffer, FileSize);
	
	//
	InFile.close();
	(*OUT_DataPtr) = Buffer;
	OUT_BufferSize = FileSize;
	return true;
}
fBool fVoxelWorld::LoadBinaryData(std::string IN_FileName, fLong IN_Offset, fLong IN_Size, fUChar** OUT_Before, fUChar** OUT_After, fLong& OUT_BeforeSize, fLong& OUT_AfterSize) {
	std::scoped_lock Lock(IO_Lock);
	
	// Open Requsted File
	std::ifstream InFile(IN_FileName, std::ios::binary | std::ios::in);
	if (!InFile.is_open()) {
		Log(
			F_LOG_SEV_ERROR,
			"IO",
			"Unable to open requested file at [" + IN_FileName + "]"
		);
		return false;
	}
	InFile.seekg(0, std::ios::end);
	fLong FileSize = InFile.tellg();
	InFile.seekg(0, std::ios::beg);
	
	// Create Buffers
	fLong BeforeS = IN_Offset;
	fLong AfterS = FileSize - (IN_Offset + IN_Size);
	fUChar* Before = (fUChar*)DefaultMemAllocator(sizeof(fUChar) * IN_Offset);
	fUChar* After = (fUChar*)DefaultMemAllocator(sizeof(fUChar) * (FileSize - (IN_Offset + IN_Size)));
	
	// Read Buffers
	InFile.read((char*)Before, BeforeS);
	InFile.seekg((IN_Offset + IN_Size));
	InFile.read((char*)After, AfterS);
	
	// Done :D
	InFile.close();
	(*OUT_Before) = Before;
	(*OUT_After) = After;
	OUT_BeforeSize = BeforeS;
	OUT_AfterSize = AfterS;
	return true;
}



// ----------------------------------------------------------------------------------------------------
// Log Stuff
// ----------------------------------------------------------------------------------------------------


std::string fVoxelWorld::LogSev_To_String(fUChar IN_Sev) {
	switch(IN_Sev) {
		case F_LOG_SEV_DEBUG: return "DEBUG";
		case F_LOG_SEV_INFO: return "INFO";
		case F_LOG_SEV_WARNING: return "WARNING";
		case F_LOG_SEV_ERROR: return "ERROR";
		default: return "{#NULL}";
	}
}

void fVoxelWorld::SetLogCallbackPtr(fLogCallback IN_Fnc) { LogFunctionPtr = IN_Fnc; }

// ----------------------------------------------------------------------------------------------------
// Stuff
// ----------------------------------------------------------------------------------------------------

void* fVoxelWorld::DefaultMemAllocator(size_t IN_Bytes) { return malloc(IN_Bytes); }
void fVoxelWorld::DefaultMemDeAllocator(void* IN_Ptr) { free(IN_Ptr); }

// ----------------------------------------------------------------------------------------------------
// FVoxel Internal Stuff
// ----------------------------------------------------------------------------------------------------

void fVoxelWorld::_Internal_Init() {
	ChunksPerWorld = WorldSize_X * WorldSize_Z;
	BlocksPerChunk = ChunkSize_X * ChunkSize_Y * ChunkSize_Z;
	
	ChunkList.resize(ChunksPerWorld, fVoxelChunk(this));
	
	// Also Check if Folder for Regions is present
	std::string Path = SavePath + _Internal_GetRegionPath();
	if (!std::filesystem::is_directory(Path)) {
		std::filesystem::create_directories(Path);
	}
	
	// Calculate Mesh Vertecies
	TempVertNum_Front = FoxelMesh[0].Vertecies.size();
	TempVertNum_Back = FoxelMesh[1].Vertecies.size();
	TempVertNum_Left = FoxelMesh[2].Vertecies.size();
	TempVertNum_Right = FoxelMesh[3].Vertecies.size();
	TempVertNum_Top = FoxelMesh[4].Vertecies.size();
	TempVertNum_Bottom = FoxelMesh[5].Vertecies.size();
	TempVertNum_Always = FoxelMesh[6].Vertecies.size();
}

fBool fVoxelWorld::_Internal_SaveWorldProp() {
	fUInt Props[7] = {
		ChunkSize_X, ChunkSize_Y, ChunkSize_Z,
		RegionSize_X, RegionSize_Z,
		WorldSize_X, WorldSize_Z
	};
	std::string FileName = SavePath + _Internal_GetWorldPropName();
	
	fBool Result = SaveBinaryData(
		FileName,
		(fUChar*)&Props[0],
		7 * 4
	);
	
	if (!Result) {
		Log(
			F_LOG_SEV_ERROR,
			"FVoxelWorld",
			"Failed to save World Properties at [" + FileName + "]"
		);
		return false;
	}
	else {
		Log(
			F_LOG_SEV_DEBUG,
			"FVoxelWorld",
			"World Properties saved at [" + FileName + "]"
		);
		return true;
	}
}

fBool fVoxelWorld::_Internal_LoadWorldProp() {
	fUInt Props[7] = {0};
	std::string FileName = SavePath + _Internal_GetWorldPropName();
	fBool Result = LoadBinaryData(
		FileName,
		(fUChar*)&Props[0],
		7 * 4
	);
	
	if (!Result) {
		Log(
			F_LOG_SEV_ERROR,
			"FVoxelWorld",
			"Failed to load World Properties from [" + FileName + "]"
		);
		return false;
	}
	
	ChunkSize_X		= Props[0];
	ChunkSize_Y		= Props[1];
	ChunkSize_Z		= Props[2];
	RegionSize_X	= Props[3];
	RegionSize_Z	= Props[4];
	WorldSize_X		= Props[5];
	WorldSize_Z		= Props[6];
	Log(
		F_LOG_SEV_DEBUG,
		"FVoxelWorld",
		"World Properties loaded from [" + FileName + "]"
	);
	
	std::string C_Size = std::to_string(ChunkSize_X);
	C_Size += ", " + std::to_string(ChunkSize_Y);
	C_Size += ", " + std::to_string(ChunkSize_Z);
	
	std::string R_Size = std::to_string(RegionSize_X);
	R_Size += ", " + std::to_string(RegionSize_Z);
	
	std::string W_Size = std::to_string(WorldSize_X);
	W_Size += ", " + std::to_string(WorldSize_Z);
	
	Log(
		F_LOG_SEV_DEBUG,
		"FVoxelWorld",
		"ChunkSize [" + C_Size + "], RegionSize [" + R_Size + "], WorldSize [" + W_Size + "]"
	);
	return true;
}

fUInt fVoxelWorld::_Internal_LoadRegionHeader(fInt IN_X, fInt IN_Z) {
	// Check if Region already loaded
	fUInt Index = _Internal_GetRegionIndex(IN_X, IN_Z);
	if (Index < F_UINT_MAX) { return Index; }
	
	// Iff not Add new Region to list
	Index = RegionList.size();
	
	fVoxelRegionData NewRegion;
	NewRegion.RX = IN_X;
	NewRegion.RZ = IN_Z;
	RegionList.push_back(NewRegion);
	
	// See if Region have save data
	std::string TargetName = _Internal_GetRegionHeaderName(IN_X, IN_Z);
	
	// if File exist , also load it in
	if (isFileExist(TargetName)) {
		fUChar* Buffer = nullptr;
		fLong Size = 0;
		if (!LoadBinaryData(TargetName,&Buffer,Size)) {
			Log(
				F_LOG_SEV_ERROR,
				"FoxelWorld",
				"Failed to read Region data for [" + std::to_string(IN_X) + "," + std::to_string(IN_Z) + "]"
			);
			return F_UINT_MAX;
		}
		
		if (Size % 24 != 0) {
			Log(
				F_LOG_SEV_ERROR,
				"FoxelWorld",
				"Invalid Region data for [" + std::to_string(IN_X) + "," + std::to_string(IN_Z) + "].% != 0"
			);
			return F_UINT_MAX;
		}
		
		
		Log(
			F_LOG_SEV_DEBUG,
			"FoxelWorld",
			"Loading Region data for [" + std::to_string(IN_X) + "," + std::to_string(IN_Z) + "]."
		);
		fUInt EntryNum = Size / 24;	// 2*4 - PosX, PosZ, 8 - Offset, 8 - Size => 24
		for (fUInt X = 0; X < EntryNum; X++) {
			fLong Temp[3] = {0};
			memcpy((fUChar*)&Temp[0], &Buffer[X * 24], 24);
			fVoxelRegionEntry Entry;
			Entry.PosX = Temp[0] >> (sizeof(fUInt) * 8);
			Entry.PosZ = Temp[0];
			Entry.Offset = Temp[1];
			Entry.Size = Temp[2];
			
			RegionList[Index].EntryList.push_back(Entry);
		}
		
		RegionList[Index].EOF_Offset = Size;
		
		DeAllocator(Buffer);
		return Index;
	}
	else {
		Log(
			F_LOG_SEV_DEBUG,
			"FoxelWorld",
			"Create Region data for [" + std::to_string(IN_X) + "," + std::to_string(IN_Z) + "]."
		);
	}
	
	return Index;
}
fBool fVoxelWorld::_Internal_SaveRegionHeader(fVoxelRegionData& REF_Region) {
	std::scoped_lock Lock(Region_Lock);
	
	std::string HeaderName = _Internal_GetRegionHeaderName(REF_Region.RX, REF_Region.RZ);
	fUChar* Buffer = nullptr;
	fLong BufferSize = 0;
	fLong BufferPos = 0;
	
	fUInt Num = REF_Region.EntryList.size();
	if (Num == 0) { return true; }
	
	BufferSize = 24 * Num;
	Buffer = (fUChar*)DefaultMemAllocator(sizeof(fUChar) * BufferSize);
	
	for (fUInt X = 0; X < Num; X++) {
		fLong Temp[3] = {
			(fLong)REF_Region.EntryList[X].PosZ,
			REF_Region.EntryList[X].Offset,
			REF_Region.EntryList[X].Size
		};
		Temp[0] |= (fLong)REF_Region.EntryList[X].PosX << (sizeof(fUInt) * 8);
		memcpy(&Buffer[BufferPos], (fUChar*)&Temp[0], 24);
		BufferPos += 24;
	}
	
	fBool Result = SaveBinaryData(HeaderName,Buffer,BufferSize);
	DeAllocator(Buffer);
	
	if (!Result) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Failed to save Region data for [" + std::to_string(REF_Region.RX) + "," + std::to_string(REF_Region.RZ) + "]."
		);
		return false;
	}
	else {
		Log(
			F_LOG_SEV_DEBUG,
			"FoxelWorld",
			"Region data saved for [" + std::to_string(REF_Region.RX) + "," + std::to_string(REF_Region.RZ) + "]."
		);
		return true;
	}
}
fBool fVoxelWorld::_Internal_UpdateRegionData(fLong IN_Offset, fUInt IN_RIndex, fUInt IN_SIndex) {
	std::scoped_lock Lock(Region_Lock);
	
	fUInt Num = RegionList[IN_RIndex].EntryList.size();
	if (IN_SIndex == Num - 1) { return true; }
	
	for (fUInt X = IN_SIndex + 1; X < Num; X++) {
		RegionList[IN_RIndex].EntryList[X].Offset += IN_Offset;
	}
	return true;
}

fBool fVoxelWorld::_Internal_CreateChunk(fVoxelChunk& REF_Chunk) {
	Log(
		F_LOG_SEV_DEBUG,
		"FoxelWorld",
		"Create Chunk [" + std::to_string(REF_Chunk.PosX) + "," + std::to_string(REF_Chunk.PosZ) + "]."
	);
	REF_Chunk.isExist = true;
	if (!REF_Chunk.isAllocated) {
		REF_Chunk.BlockList = (fUInt*)DefaultMemAllocator(sizeof(fUInt) * BlocksPerChunk);
		memset(REF_Chunk.BlockList, F_UINT_MAX, BlocksPerChunk * 4);
		REF_Chunk.isAllocated = true;
	}
	REF_Chunk.isModified = true;
	return false;
}
fBool fVoxelWorld::_Internal_LoadChunk(fVoxelChunk& REF_Chunk, fVoxelRegionData& REF_Region, fUInt IN_SavedIndex) {
	Log(
		F_LOG_SEV_DEBUG,
		"FoxelWorld",
		"Loading Chunk [" + std::to_string(REF_Chunk.PosX) + "," + std::to_string(REF_Chunk.PosZ) + "]."
	);
	
	std::string FileName = _Internal_GetRegionDataName(REF_Region.RX, REF_Region.RZ);
	
	fUInt Num = REF_Region.EntryList[IN_SavedIndex].Size / 4;
	fUInt* Buffer = (fUInt*)DefaultMemAllocator(sizeof(fUInt) * Num);
	
	LoadBinaryData(
		FileName,
		(fUChar*)Buffer,
		REF_Region.EntryList[IN_SavedIndex].Size,
		REF_Region.EntryList[IN_SavedIndex].Offset
	);
	
	fUInt PairNum = Num / 2;
	std::vector<fVector2ui> ChunkData;
	ChunkData.resize(PairNum, fVector2ui());
	for (fUInt X = 0; X < PairNum; X++) {
		ChunkData[X].X = Buffer[(X * 2) + 0];
		ChunkData[X].Y = Buffer[(X * 2) + 1];
	}
	
	if (!REF_Chunk.isAllocated) {
		REF_Chunk.BlockList = (fUInt*)DefaultMemAllocator(sizeof(fUInt) * BlocksPerChunk);
		memset(REF_Chunk.BlockList, -1, BlocksPerChunk * 4);
		REF_Chunk.isAllocated = true;
	}
	
	_Internal_DeCompressChunk(REF_Chunk, ChunkData);
	REF_Chunk.isExist = true;
	REF_Chunk.isModified = false;
	DeAllocator(Buffer);
	return true;
}


fBool fVoxelWorld::_Internal_OverrideChunk(std::string& REF_DataFileName, fInt IN_PosX, fInt IN_PosZ, fUInt IN_RIndex, fUInt IN_SIndex, std::vector<fVector2ui>& REF_C_Data, fUInt IN_C_Size) {
	// Update the Data File
	fUChar* Before = nullptr;
	fUChar* After = nullptr;
	fLong BeforeSize = 0;
	fLong AfterSize = 0;
	fBool Result = LoadBinaryData(
		REF_DataFileName,
		RegionList[IN_RIndex].EntryList[IN_SIndex].Offset,
		RegionList[IN_RIndex].EntryList[IN_SIndex].Size,
		&Before,
		&After,
		BeforeSize,
		AfterSize
	);
	
	if (!Result) {
		if (Before != nullptr) { DeAllocator(Before); }
		if (After != nullptr) { DeAllocator(After); }
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to save chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Failed to read chunk data."
		);
		return false;
	}
	
	fLong FinalBufferSize = sizeof(fUChar) * (BeforeSize + AfterSize + IN_C_Size);
	fUChar* FinalBuffer = (fUChar*)DefaultMemAllocator(FinalBufferSize);
	memcpy(&FinalBuffer[0],Before,0);
	memcpy(&FinalBuffer[BeforeSize],(fUChar*)REF_C_Data.data(),0);
	memcpy(&FinalBuffer[BeforeSize + IN_C_Size],After,0);
	
	Result = SaveBinaryData(REF_DataFileName,FinalBuffer,FinalBufferSize);
	
	DeAllocator(Before);
	DeAllocator(After);
	DeAllocator(FinalBuffer);
	
	if (!Result) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Failed to Update chunk Data");
		return false;
	}
	
	// Also Update the Headers
	fLong RegionOffset = IN_C_Size - RegionList[IN_RIndex].EntryList[IN_SIndex].Size;
	Result = _Internal_UpdateRegionData(RegionOffset, IN_RIndex, IN_SIndex);
	if (!Result) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Failed to Update Region Data");
		return false;
	}
	
	Result = _Internal_SaveRegionHeader(RegionList[IN_RIndex]);
	if (!Result) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Failed to Save Region Data");
		return false;
	}
	
	Log(
		F_LOG_SEV_DEBUG,
		"FoxelWorld",
		"Chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "] Saved"
	);
	
	return true;
}

fBool fVoxelWorld::_Internal_CompressChunk(fVoxelChunk& REF_Chunk, std::vector<fVector2ui>& OUT_Data) {
	Log(
		F_LOG_SEV_DEBUG,
		"FoxelWorld",
		"Compressing Chunk data [" + std::to_string(REF_Chunk.PosX) + "," + std::to_string(REF_Chunk.PosZ) + "]."
	);
	
	OUT_Data.clear();
	OUT_Data.push_back({1,REF_Chunk.BlockList[0]});
	fUInt Index = 0;
	
	for (fLong X = 1; X < BlocksPerChunk; X++) {
		if (REF_Chunk.BlockList[X] == OUT_Data[Index].Y) {
			OUT_Data[Index].X++;
		}
		else {
			OUT_Data.push_back({1, REF_Chunk.BlockList[X]});
			Index++;
		}
	}
	
	return true;
}
fBool fVoxelWorld::_Internal_DeCompressChunk(fVoxelChunk& REF_Chunk, std::vector<fVector2ui>& REF_Data) {
	Log(
		F_LOG_SEV_DEBUG,
		"FoxelWorld",
		"De-Compressing Chunk data [" + std::to_string(REF_Chunk.PosX) + "," + std::to_string(REF_Chunk.PosZ) + "]."
	);
	
	fUInt Num = REF_Data.size();
	fUInt Index = 0;
	for (fUInt X = 0; X < Num; X++) {
		for (fUInt Y = 0; Y < REF_Data[X].X; Y++) {
			REF_Chunk.BlockList[Index++] = REF_Data[X].Y;
		}
	}
	
	return true;
}

fBool fVoxelWorld::_Internal_GenerateVoxel(fUInt IN_ChunkIndex, fUInt IN_BlockIndex, fVoxelLocalPos IN_Pos, fProcMesh& OUT_Mesh) {
	fProcMesh CurrMesh;
	fVoxelGlobalPos GPos = GetVoxelGlobalPos(IN_Pos);
	
	// 0 - Front	Z-
	if (TempVertNum_Front > 0) {
		if (IN_Pos.LocalZ > 0) {
			fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY, IN_Pos.LocalZ - 1);
			if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += FoxelMesh[0]; }
		}
		else {
			fVoxelGlobalPos TempPos = GPos;
			TempPos.GlobalZ--;
			fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
			if (Index == F_UINT_MAX) { CurrMesh += FoxelMesh[0]; }
		}
	}
	
	// 1 - Back		Z+
	if (TempVertNum_Back > 0) {
		if (IN_Pos.LocalZ < (fInt)ChunkSize_Z - 1) {
			fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY, IN_Pos.LocalZ + 1);
			if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += FoxelMesh[1]; }
		}
		else {
			fVoxelGlobalPos TempPos = GPos;
			TempPos.GlobalZ++;
			fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
			if (Index == F_UINT_MAX) { CurrMesh += FoxelMesh[1]; }
		}
	}
	
	// 2 - Left		X+
	if (TempVertNum_Left > 0) {
		if (IN_Pos.LocalX < (fInt)ChunkSize_X - 1) {
			fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX + 1, IN_Pos.LocalY, IN_Pos.LocalZ);
			if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += FoxelMesh[2]; }
		}
		else {
			fVoxelGlobalPos TempPos = GPos;
			TempPos.GlobalX++;
			fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
			if (Index == F_UINT_MAX) { CurrMesh += FoxelMesh[2]; }
		}
	}
	
	// 3 - Right	X-
	if (TempVertNum_Right > 0) {
		if (IN_Pos.LocalX > 0) {
			fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX - 1, IN_Pos.LocalY, IN_Pos.LocalZ);
			if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += FoxelMesh[3]; }
		}
		else {
			fVoxelGlobalPos TempPos = GPos;
			TempPos.GlobalX--;
			fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
			if (Index == F_UINT_MAX) { CurrMesh += FoxelMesh[3]; }
		}
	}
	
	// 4 - Top		Y+
	if (TempVertNum_Top > 0) {
		if (IN_Pos.LocalY < (fInt)ChunkSize_Y - 1) {
			fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY + 1, IN_Pos.LocalZ);
			if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += FoxelMesh[4]; }
		}
		else { CurrMesh += FoxelMesh[4]; }
	}
	
	// 5 - Bottom	Y-
	if (TempVertNum_Bottom > 0) {
		if (IN_Pos.LocalY > 0) {
			fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY - 1, IN_Pos.LocalZ);
			if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += FoxelMesh[5]; }
		}
		else { CurrMesh += FoxelMesh[5]; }
	}
	
	// 6 - Always
	if (TempVertNum_Always > 0) { CurrMesh += FoxelMesh[6]; }
	
	// Move Curr Mesh to Corrent Position
	fUInt Num = CurrMesh.Vertecies.size();
	fBool isUVs = CurrMesh.UVs.size() > 0;
	if (Num > 0) {
		fFloat OffsetX = IN_Pos.LocalX * VoxelSize_X;
		fFloat OffsetY = IN_Pos.LocalY * VoxelSize_Y;
		fFloat OffsetZ = IN_Pos.LocalZ * VoxelSize_Z;
		for (fUInt X = 0; X < Num; X++) {
			CurrMesh.Vertecies[X].X += OffsetX;
			CurrMesh.Vertecies[X].Y += OffsetY;
			CurrMesh.Vertecies[X].Z += OffsetZ;
			if (isUVs) {
				fVoxelBlock& BlockRef = VoxelList[ChunkList[IN_ChunkIndex].BlockList[IN_BlockIndex]];
				CurrMesh.UVs[X].X *= TextureStep_X;
				CurrMesh.UVs[X].Y *= TextureStep_Y;
				CurrMesh.UVs[X].X += BlockRef.Texture.X * TextureStep_X;
				CurrMesh.UVs[X].Y += BlockRef.Texture.Y * TextureStep_Y;
			}
		}
	}
	
	// Done :D
	OUT_Mesh += CurrMesh;
	return CurrMesh.Vertecies.size() > 0;
}

fVector2i fVoxelWorld::_Internal_GetRegion(fInt IN_X, fInt IN_Z) {
	if (RegionSize_X == 0) { throw 0; }
	if (RegionSize_Z == 0) { throw 0; }
	
	fInt X = IN_X / RegionSize_X;
	fInt Z = IN_Z / RegionSize_Z;
	if (IN_X < 0) { X--; }
	if (IN_Z < 0) { Z--; }
	
	return {X,Z};
}
fUInt fVoxelWorld::_Internal_GetChunkIndex(fInt IN_X, fInt IN_Z) {
	std::scoped_lock Lock(Chunk_Lock);
	
	for (fLong X = 0; X < ChunksPerWorld; X++) {
		if (ChunkList[X].isExist == false) { continue; }
		if (ChunkList[X].PosX != IN_X) { continue; }
		if (ChunkList[X].PosZ != IN_Z) { continue; }
		return X;
	}
	return F_UINT_MAX;
}
fUInt fVoxelWorld::_Internal_GetRegionIndex(fInt IN_X, fInt IN_Z) {
	std::scoped_lock Lock(Region_Lock);
	
	fUInt Num = RegionList.size();
	if (Num == 0) { return F_UINT_MAX; }
	
	for (fUInt X = 0; X < Num; X++) {
		if (RegionList[X].RX != IN_X) { continue; }
		if (RegionList[X].RZ != IN_Z) { continue; }
		return X;
	}
	
	return F_UINT_MAX;
}
fUInt fVoxelWorld::_Internal_GetSavedChunkIndex(fVoxelRegionData& REF_Region, fInt IN_X, fInt IN_Z) {
	fUInt Num = REF_Region.EntryList.size();
	if (Num == 0) { return F_UINT_MAX; }
	
	for (fUInt X = 0; X < Num; X++) {
		if (REF_Region.EntryList[X].PosX != IN_X) { continue; }
		if (REF_Region.EntryList[X].PosZ != IN_Z) { continue; }
		return X;
	}
	
	return F_UINT_MAX;
}

fUInt fVoxelWorld::_Internal_GetEmptyChunk() {
	std::scoped_lock Lock(Chunk_Lock);
	
	for (fLong X = 0; X < ChunksPerWorld; X++) {
		if (ChunkList[X].isExist == false) { return X; }
	}
	return F_UINT_MAX;
}

std::string fVoxelWorld::_Internal_GetRegionHeaderName(fInt IN_X, fInt IN_Z) {
	std::string Result = SavePath;
	Result += "Regions/Region_" + std::to_string(IN_X) + "_" + std::to_string(IN_Z) + "_Header";
	return Result;
}
std::string fVoxelWorld::_Internal_GetRegionDataName(fInt IN_X, fInt IN_Z) {
	std::string Result = SavePath;
	Result += "Regions/Region_" + std::to_string(IN_X) + "_" + std::to_string(IN_Z) + "_Data";
	return Result;
}

std::string fVoxelWorld::_Internal_GetWorldPropName() { return "FVoxel"; }
std::string fVoxelWorld::_Internal_GetRegionPath() { return "Regions"; }

// ----------------------------------------------------------------------------------------------------
// FVoxel External Stuff
// ----------------------------------------------------------------------------------------------------


fBool fVoxelWorld::CreateWorld(std::string IN_Path) {
	if (isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Create world. A world is already created.");
		return false;
	}
	
	SavePath = IN_Path;
	if (SavePath[SavePath.length() - 1] != '/') { SavePath += '/'; }
	
	// See if we are loading or createing the world
	std::string FileName = SavePath + _Internal_GetWorldPropName();
	if (isFileExist(FileName)) {
		if (_Internal_LoadWorldProp()) {
			Log(F_LOG_SEV_DEBUG,"FVoxelWorld","World Loaded Successfully");
			_Internal_Init();
			isInit = true;
			return true;
		}
		else {
			Log(F_LOG_SEV_ERROR,"FVoxelWorld","Failed to Create World");
			return false;
		}
	}
	else {
		if (_Internal_SaveWorldProp()) {
			Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Empty World Created Successfully");
			_Internal_Init();
			isInit = true;
			return true;
		}
		else {
			Log(F_LOG_SEV_ERROR,"FVoxelWorld","Failed to Create Empty World");
			return false;
		}
	}
}

fUInt fVoxelWorld::SpawnChunk(fInt IN_PosX, fInt IN_PosZ) {
	if (!isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to spawn chunk. World not yet initialised");
		return F_UINT_MAX;
	}
	
	// See if Chunk Already Loaded
	fUInt ChunkIndex = _Internal_GetChunkIndex(IN_PosX, IN_PosZ);
	if (ChunkIndex != F_UINT_MAX) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to spawn chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Chunk ALready Spawned."
		);
		return F_UINT_MAX;
	}
	
	// Get Empty Chunk Index
	fUInt EmptyIndex = _Internal_GetEmptyChunk();
	if (EmptyIndex == F_UINT_MAX) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to spawn chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. ChunkBuffer is full."
		);
		return F_UINT_MAX;
	}
	
	// See if Target Region is already loaded
	fVector2i RPos = _Internal_GetRegion(IN_PosX, IN_PosZ);
	fUInt RIndex = _Internal_LoadRegionHeader(RPos.X, RPos.Y);
	if (RIndex == F_UINT_MAX) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to spawn chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Failed to load region."
		);
		return F_UINT_MAX;
	}
	
	// Set Chunk Pos
	ChunkList[EmptyIndex].PosX = IN_PosX;
	ChunkList[EmptyIndex].PosZ = IN_PosZ;
	
	// See if Chunk have save data
	fUInt SIndex = _Internal_GetSavedChunkIndex(RegionList[RIndex], IN_PosX, IN_PosZ);
	if (SIndex == F_UINT_MAX) {
		if (!_Internal_CreateChunk(ChunkList[EmptyIndex])) { return F_UINT_MAX; }
		else { return EmptyIndex; }
	}
	else {
		if (!_Internal_LoadChunk(ChunkList[EmptyIndex],RegionList[RIndex], SIndex)) { return F_UINT_MAX; }
		else { return EmptyIndex; }
	}
}
fBool fVoxelWorld::SaveChunk(fInt IN_PosX, fInt IN_PosZ) {
	if (!isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to save chunk. World not yet initialised");
		return false;
	}
	
	
	// See if Chunk is even Loaded
	fUInt ChunkIndex = _Internal_GetChunkIndex(IN_PosX, IN_PosZ);
	if (ChunkIndex == F_UINT_MAX) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to save chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Chunk not yet exist."
		);
		return false;
	}
	
	// See if Chunk is modified
	if (!ChunkList[ChunkIndex].isModified) { return true; }
	
	// Get Compressed chunk data
	std::vector<fVector2ui> C_Data;
	fUInt C_Size = 0;
	_Internal_CompressChunk(ChunkList[ChunkIndex], C_Data);
	C_Size = C_Data.size() * 8;	// 2 * fUInt(4) per entry
	
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld",
		"Saving Chunk [" + 
		std::to_string(IN_PosX) + ", " + 
		std::to_string(IN_PosZ) + "] with [" + 
		std::to_string(C_Data.size() / 2) + "] pairs"
	);
	
	// Get Region
	fVector2i R = _Internal_GetRegion(IN_PosX, IN_PosZ);
	fUInt RIndex = _Internal_LoadRegionHeader(R.X, R.Y);
	if (RIndex == F_UINT_MAX) { return false; }
	
	// Get Data File Name
	std::string DataFileName = _Internal_GetRegionDataName(R.X, R.Y);
	
	// See if chunk has ever been saved
	fUInt SIndex = _Internal_GetSavedChunkIndex(RegionList[RIndex], IN_PosX, IN_PosZ);
	if (SIndex == F_UINT_MAX) {
		// Add new Chunk To Region header
		fVoxelRegionEntry NewEntry;
		NewEntry.PosX = IN_PosX;
		NewEntry.PosZ = IN_PosZ;
		NewEntry.Offset = RegionList[RIndex].EOF_Offset;
		NewEntry.Size = C_Size;
		RegionList[RIndex].EntryList.push_back(NewEntry);
		
		RegionList[RIndex].EOF_Offset += NewEntry.Size;
		
		// Append Chunk Data
		if (RegionList[RIndex].EntryList.size() == 1) { CreateEmptyFile(DataFileName); }
		fBool Result = AppendBinaryData(
			DataFileName,
			(fUChar*)C_Data.data(),
			C_Size
		);
		if (!Result) {
			Log(
				F_LOG_SEV_ERROR,
				"FoxelWorld",
				"Unable to save chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Failed to append chunk data."
			);
			return false;
		}
	}
	else {
		fBool Result = _Internal_OverrideChunk(
			DataFileName,
			IN_PosX,
			IN_PosZ,
			RIndex,
			SIndex,
			C_Data,
			C_Size
		);
		if (!Result) { return false; }
	}
	
	return _Internal_SaveRegionHeader(RegionList[RIndex]);
}

fBool fVoxelWorld::UnloadChunk(fInt IN_PosX, fInt IN_PosZ, fBool IN_isSave) {
	if (!isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to unload chunk. World not yet initialised");
		return false;
	}
	
	// See if Chunk is even Loaded
	fUInt ChunkIndex = _Internal_GetChunkIndex(IN_PosX, IN_PosZ);
	if (ChunkIndex == F_UINT_MAX) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to uload chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Chunk not yet exist."
		);
		return false;
	}
	else {
		Log(
			F_LOG_SEV_DEBUG,
			"FoxelWorld",
			"Unloading chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]."
		);
	}
	
	// See if Chunk Needs Save
	if (IN_isSave) {
		fBool Result = SaveChunk(IN_PosX, IN_PosZ);
		if (!Result) { return false; }
	}
	
	// Mark Chunk non-existing
	ChunkList[ChunkIndex].isExist = false;
	memset(ChunkList[ChunkIndex].BlockList, F_UINT_MAX, BlocksPerChunk * 4);
	
	return true;
}

fVoxelChunk* fVoxelWorld::GetChunkPtr(fInt IN_PosX, fInt IN_PosZ) {
	if (!isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to get chunk ref. World not yet initialised");
		return nullptr;
	}
	
	// See if Chunk is even Loaded
	fUInt ChunkIndex = _Internal_GetChunkIndex(IN_PosX, IN_PosZ);
	if (ChunkIndex == F_UINT_MAX) {
		Log(
			F_LOG_SEV_ERROR,
			"FoxelWorld",
			"Unable to get chunk ref for [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]. Chunk not yet exist."
		);
		return nullptr;
	}
	
	return &ChunkList[ChunkIndex];
}

fBool fVoxelWorld::SaveWorld() {
	for (fUInt X = 0; X < ChunksPerWorld; X++) {
		if (ChunkList[X].isExist && ChunkList[X].isModified) {
			if (!SaveChunk(ChunkList[X].PosX, ChunkList[X].PosZ)) {
				return false;
			}
		}
	}
	
	return true;
}

fBool fVoxelWorld::UnloadWorld() {
	if (!isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to UnloadWorld. World net yet created.");
		return false;
	}
	
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Unloading world...");
	
	// Deallocate All blocklists
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Deallocate Block Lists");
	for (fUInt X = 0; X < ChunksPerWorld; X++) {
		if (ChunkList[X].isAllocated) {
			DeAllocator(ChunkList[X].BlockList);
			ChunkList[X].isAllocated = false;
		}
	}
	
	// Empty Chunks
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Clear Chunks and Regions");
	ChunkList.clear();
	RegionList.clear();
	
	// Reset Consts
	ChunksPerWorld = 0;
	BlocksPerChunk = 0;
	
	isInit = false;
	
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","World Unloaded");
	return true;
}
void fVoxelWorld::Log(fUChar IN_Sev, std::string IN_SenderName, std::string IN_Msg) {
	if (IN_Sev < MinLogLevel) { return; }
	if (LogFunctionPtr != nullptr) { LogFunctionPtr(IN_Sev, IN_SenderName, IN_Msg); return; }
	
	std::string S = LogSev_To_String(IN_Sev);
	std::cout << "[" << S << "] " << IN_SenderName << " => " << IN_Msg << std::endl;
}
fBool fVoxelWorld::SetVoxelMesh(std::vector<fProcMesh> IN_MeshList) {
	if (IN_MeshList.size() != 7) {
		Log(
			F_LOG_SEV_ERROR,
			"FVoxelWorld",
			"Unable to set VoxelMeshList. Expected [7] Elements, provided [" + std::to_string(IN_MeshList.size()) + "]."
		);
		return false;
	}
	
	FoxelMesh[0] = IN_MeshList[0];
	FoxelMesh[1] = IN_MeshList[1];
	FoxelMesh[2] = IN_MeshList[2];
	FoxelMesh[3] = IN_MeshList[3];
	FoxelMesh[4] = IN_MeshList[4];
	FoxelMesh[5] = IN_MeshList[5];
	FoxelMesh[6] = IN_MeshList[6];
	
	return true;
}
void fVoxelWorld::UseDefaultVoxelMesh() {
	const fVector3 CubeVertecies[8] = {
		/*0*/{0.0F,0.0F,0.0F},
		/*1*/{0.0F,0.0F,VoxelSize_Z},
		/*2*/{VoxelSize_X,0.0F,VoxelSize_Z},
		/*3*/{VoxelSize_X,0.0F,0.0F},
		
		/*4*/{0.0F,VoxelSize_Y,0.0F},
		/*5*/{0.0F,VoxelSize_Y,VoxelSize_Z},
		/*6*/{VoxelSize_X,VoxelSize_Y,VoxelSize_Z},
		/*7*/{VoxelSize_X,VoxelSize_Y,0.0F}
	};
	
	const fVector3 CubeNormals[6] = {
		/*Front*/ {0.0F,0.0F,1.0F},
		/*Back*/ {0.0F,0.0F,-1.0F},
		/*Left*/ {-1.0F,0.0F,0.0F},
		/*Right*/ {1.0F,0.0F,0.0F},
		/*Top*/ {0.0F,1.0F,0.0F},
		/*Bottom*/ {0.0F,-1.0F,0.0F}
	};
	
	
	// 0 - Front	Z-
	FoxelMesh[0].Vertecies.push_back(CubeVertecies[0]);
	FoxelMesh[0].Vertecies.push_back(CubeVertecies[4]);
	FoxelMesh[0].Vertecies.push_back(CubeVertecies[7]);
	
	FoxelMesh[0].Vertecies.push_back(CubeVertecies[3]);
	FoxelMesh[0].Vertecies.push_back(CubeVertecies[0]);
	FoxelMesh[0].Vertecies.push_back(CubeVertecies[7]);
	
	FoxelMesh[0].Normals.push_back(CubeNormals[1]);
	FoxelMesh[0].Normals.push_back(CubeNormals[1]);
	FoxelMesh[0].Normals.push_back(CubeNormals[1]);
	
	FoxelMesh[0].Normals.push_back(CubeNormals[1]);
	FoxelMesh[0].Normals.push_back(CubeNormals[1]);
	FoxelMesh[0].Normals.push_back(CubeNormals[1]);
	
	FoxelMesh[0].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[0].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[0].UVs.push_back({1.0F, 0.0F});
	
	FoxelMesh[0].UVs.push_back({1.0F, 1.0F});
	FoxelMesh[0].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[0].UVs.push_back({1.0F, 0.0F});
	
	
	// 1 - Back		Z+
	FoxelMesh[1].Vertecies.push_back(CubeVertecies[2]);
	FoxelMesh[1].Vertecies.push_back(CubeVertecies[6]);
	FoxelMesh[1].Vertecies.push_back(CubeVertecies[5]);
	
	FoxelMesh[1].Vertecies.push_back(CubeVertecies[1]);
	FoxelMesh[1].Vertecies.push_back(CubeVertecies[2]);
	FoxelMesh[1].Vertecies.push_back(CubeVertecies[5]);
	
	FoxelMesh[1].Normals.push_back(CubeNormals[0]);
	FoxelMesh[1].Normals.push_back(CubeNormals[0]);
	FoxelMesh[1].Normals.push_back(CubeNormals[0]);
	
	FoxelMesh[1].Normals.push_back(CubeNormals[0]);
	FoxelMesh[1].Normals.push_back(CubeNormals[0]);
	FoxelMesh[1].Normals.push_back(CubeNormals[0]);
	
	FoxelMesh[1].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[1].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[1].UVs.push_back({1.0F, 0.0F});
	
	FoxelMesh[1].UVs.push_back({1.0F, 1.0F});
	FoxelMesh[1].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[1].UVs.push_back({1.0F, 0.0F});
	
	
	// 2 - Left		X+
	FoxelMesh[2].Vertecies.push_back(CubeVertecies[3]);
	FoxelMesh[2].Vertecies.push_back(CubeVertecies[7]);
	FoxelMesh[2].Vertecies.push_back(CubeVertecies[6]);
	
	FoxelMesh[2].Vertecies.push_back(CubeVertecies[2]);
	FoxelMesh[2].Vertecies.push_back(CubeVertecies[3]);
	FoxelMesh[2].Vertecies.push_back(CubeVertecies[6]);
	
	FoxelMesh[2].Normals.push_back(CubeNormals[2]);
	FoxelMesh[2].Normals.push_back(CubeNormals[2]);
	FoxelMesh[2].Normals.push_back(CubeNormals[2]);
	
	FoxelMesh[2].Normals.push_back(CubeNormals[2]);
	FoxelMesh[2].Normals.push_back(CubeNormals[2]);
	FoxelMesh[2].Normals.push_back(CubeNormals[2]);
	
	FoxelMesh[2].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[2].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[2].UVs.push_back({1.0F, 0.0F});
	
	FoxelMesh[2].UVs.push_back({1.0F, 1.0F});
	FoxelMesh[2].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[2].UVs.push_back({1.0F, 0.0F});
	
	// 3 - Right	X-
	FoxelMesh[3].Vertecies.push_back(CubeVertecies[1]);
	FoxelMesh[3].Vertecies.push_back(CubeVertecies[5]);
	FoxelMesh[3].Vertecies.push_back(CubeVertecies[4]);
	
	FoxelMesh[3].Vertecies.push_back(CubeVertecies[0]);
	FoxelMesh[3].Vertecies.push_back(CubeVertecies[1]);
	FoxelMesh[3].Vertecies.push_back(CubeVertecies[4]);
	
	FoxelMesh[3].Normals.push_back(CubeNormals[3]);
	FoxelMesh[3].Normals.push_back(CubeNormals[3]);
	FoxelMesh[3].Normals.push_back(CubeNormals[3]);
	
	FoxelMesh[3].Normals.push_back(CubeNormals[3]);
	FoxelMesh[3].Normals.push_back(CubeNormals[3]);
	FoxelMesh[3].Normals.push_back(CubeNormals[3]);
	
	FoxelMesh[3].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[3].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[3].UVs.push_back({1.0F, 0.0F});
	
	FoxelMesh[3].UVs.push_back({1.0F, 1.0F});
	FoxelMesh[3].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[3].UVs.push_back({1.0F, 0.0F});
	
	
	// 4 - Top		Y+
	FoxelMesh[4].Vertecies.push_back(CubeVertecies[4]);
	FoxelMesh[4].Vertecies.push_back(CubeVertecies[5]);
	FoxelMesh[4].Vertecies.push_back(CubeVertecies[6]);
	
	FoxelMesh[4].Vertecies.push_back(CubeVertecies[7]);
	FoxelMesh[4].Vertecies.push_back(CubeVertecies[4]);
	FoxelMesh[4].Vertecies.push_back(CubeVertecies[6]);
	
	FoxelMesh[4].Normals.push_back(CubeNormals[4]);
	FoxelMesh[4].Normals.push_back(CubeNormals[4]);
	FoxelMesh[4].Normals.push_back(CubeNormals[4]);
	
	FoxelMesh[4].Normals.push_back(CubeNormals[4]);
	FoxelMesh[4].Normals.push_back(CubeNormals[4]);
	FoxelMesh[4].Normals.push_back(CubeNormals[4]);
	
	FoxelMesh[4].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[4].UVs.push_back({0.0F, 1.0F});
	FoxelMesh[4].UVs.push_back({1.0F, 1.0F});
	
	FoxelMesh[4].UVs.push_back({1.0F, 0.0F});
	FoxelMesh[4].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[4].UVs.push_back({1.0F, 1.0F});
	
	// 5 - Bottom	Y-
	FoxelMesh[5].Vertecies.push_back(CubeVertecies[0]);
	FoxelMesh[5].Vertecies.push_back(CubeVertecies[2]);
	FoxelMesh[5].Vertecies.push_back(CubeVertecies[1]);
	
	FoxelMesh[5].Vertecies.push_back(CubeVertecies[3]);
	FoxelMesh[5].Vertecies.push_back(CubeVertecies[2]);
	FoxelMesh[5].Vertecies.push_back(CubeVertecies[0]);
	
	FoxelMesh[5].Normals.push_back(CubeNormals[5]);
	FoxelMesh[5].Normals.push_back(CubeNormals[5]);
	FoxelMesh[5].Normals.push_back(CubeNormals[5]);
	
	FoxelMesh[5].Normals.push_back(CubeNormals[5]);
	FoxelMesh[5].Normals.push_back(CubeNormals[5]);
	FoxelMesh[5].Normals.push_back(CubeNormals[5]);
	
	FoxelMesh[5].UVs.push_back({0.0F, 0.0F});
	FoxelMesh[5].UVs.push_back({1.0F, 1.0F});
	FoxelMesh[5].UVs.push_back({0.0F, 1.0F});
	
	FoxelMesh[5].UVs.push_back({1.0F, 0.0F});
	FoxelMesh[5].UVs.push_back({1.0F, 1.0F});
	FoxelMesh[5].UVs.push_back({0.0F, 0.0F});
}
fVoxelLocalPos fVoxelWorld::GetVoxelLocalPos(fInt IN_GlobalX, fInt IN_GlobalY, fInt IN_GlobalZ) {
	fVoxelLocalPos Result;
	
	Result.ChunkX = std::floor((fFloat)IN_GlobalX/(fFloat)ChunkSize_X);
	if (IN_GlobalX >= 0) {
		Result.LocalX = IN_GlobalX - (Result.ChunkX * (fInt)ChunkSize_X);
	}
	else {
		Result.LocalX = IN_GlobalX * -1;
		fInt TempX = ((Result.ChunkX * -1) - 1) * (fInt)ChunkSize_X;
		Result.LocalX = (Result.LocalX  - TempX) - (fInt)ChunkSize_X;
		Result.LocalX *= -1;
	}
	
	Result.ChunkZ = std::floor((fFloat)IN_GlobalZ/(fFloat)ChunkSize_Z);
	if (IN_GlobalZ >= 0) {
		Result.LocalZ = IN_GlobalZ - (Result.ChunkZ * (fInt)ChunkSize_Z);
	}
	else {
		Result.LocalZ = IN_GlobalZ * -1;
		fInt TempZ = ((Result.ChunkZ * -1) - 1) * (fInt)ChunkSize_Z;
		Result.LocalZ = (Result.LocalZ  - TempZ) - (fInt)ChunkSize_Z;
		Result.LocalZ *= -1;
	}
	
	Result.LocalY = IN_GlobalY;
	
	return Result;
}
fVoxelGlobalPos fVoxelWorld::GetVoxelGlobalPos(fVoxelLocalPos IN_Pos) {
	fVoxelGlobalPos Result;
	
	Result.GlobalX = IN_Pos.ChunkX * (fInt)ChunkSize_X;
	Result.GlobalX += IN_Pos.LocalX;
	
	Result.GlobalY = IN_Pos.LocalY;
	
	Result.GlobalZ = IN_Pos.ChunkZ * (fInt)ChunkSize_Z;
	Result.GlobalZ += IN_Pos.LocalZ;
	
	return Result;
}
fUInt fVoxelWorld::GetVoxelIndex(fInt IN_X, fInt IN_Y, fInt IN_Z) {
	// Calculate The Chunk This Voxel Belongs To
	fVoxelLocalPos Pos = GetVoxelLocalPos(IN_X, IN_Y, IN_Z);
	
	// Get Chunk Index
	fUInt CIndex = _Internal_GetChunkIndex(Pos.ChunkX, Pos.ChunkZ);
	if (CIndex == F_UINT_MAX) {
		return F_UINT_MAX;
	}
	
	fUInt VIndex = ChunkList[CIndex].GetVoxelIndex(Pos.LocalX, Pos.LocalY, Pos.LocalZ);
	if (VIndex == F_UINT_MAX) {
		return F_UINT_MAX;
	}
	
	return ChunkList[CIndex].BlockList[VIndex];
}
fBool fVoxelWorld::GenerateChunkMesh(fInt IN_PosX, fInt IN_PosZ, fProcMesh& OUT_Mesh) {
	if (!isInit) { return false; }
	
	fUInt CIndex = _Internal_GetChunkIndex(IN_PosX, IN_PosZ);
	if (CIndex == F_UINT_MAX) { return false; }
	
	fVoxelLocalPos LPos;
	LPos.ChunkX = IN_PosX;
	LPos.ChunkZ = IN_PosZ;
	
	for (fUInt Y = 0; Y < ChunkSize_Y; Y++) {
		LPos.LocalY = Y;
		
		for (fUInt Z = 0; Z < ChunkSize_Z; Z++) {
			LPos.LocalZ = Z;
			
			for (fUInt X = 0; X < ChunkSize_X; X++) {
				LPos.LocalX = X;
				
				fUInt Index = ChunkList[CIndex].GetVoxelIndex(X,Y,Z);
				if (ChunkList[CIndex].BlockList[Index] < F_UINT_MAX) {
					if (_Internal_GenerateVoxel(CIndex, Index, LPos, OUT_Mesh)) {
						ChunkList[CIndex].Debug_VisibleVoxels++;
					}
				}
			}
		}
	}
	
	return true;
}
fBool fVoxelWorld::SetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& REF_Voxel) {
	return false;
}
fBool fVoxelWorld::GetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& OUT_Voxel) {
	return false;
}
fBool fVoxelWorld::ClearVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z) {
	return false;
}

fBool fVoxelWorld::SetChunkVoxelSize(fInt IN_X, fInt IN_Y, fInt IN_Z) {
	if (isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","SetChunkVoxelSize() => World properties cannot be changed after initialization");
		return false;
	}
	
	if (IN_X <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set Chunk VoxelSize. Invalid [X] value. Value must be >= 0" );
		return false;
	}
	if (IN_Y <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set Chunk VoxelSize. Invalid [Y] value. Value must be >= 0" );
		return false;
	}
	if (IN_Z <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set Chunk VoxelSize. Invalid [Z] value. Value must be >= 0" );
		return false;
	}
	
	ChunkSize_X = IN_X;
	ChunkSize_Y = IN_Y;
	ChunkSize_Z = IN_Z;
	
	std::string C = "[" + std::to_string(IN_X) + "," + std::to_string(IN_Y) + "," + std::to_string(IN_Z) + "]";
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","World Chunk Size set to " + C);
	
	return true;
}
fBool fVoxelWorld::SetRegionSize(fInt IN_X, fInt IN_Z) {
	if (isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","SetChunkVoxelSize() => World properties cannot be changed after initialization");
		return false;
	}
	
	if (IN_X <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set World Size. Invalid [X] value. Value must be >= 0" );
		return false;
	}
	if (IN_Z <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set World Size. Invalid [Z] value. Value must be >= 0" );
		return false;
	}
	
	RegionSize_X = IN_X;
	RegionSize_Z = IN_Z;
	
	std::string R = "[" + std::to_string(IN_X) + "," + std::to_string(IN_Z) + "]";
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","World Region Size set to " + R);
	
	return true;
}
fBool fVoxelWorld::SetWorldSize(fInt IN_X, fInt IN_Z) {
	if (isInit) {
		Log(F_LOG_SEV_ERROR,"FVoxelWorld","SetChunkVoxelSize() => World properties cannot be changed after initialization");
		return false;
	}
	
	if (IN_X <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set World Size. Invalid [X] value. Value must be >= 0" );
		return false;
	}
	if (IN_Z <= 0) {
		Log( F_LOG_SEV_ERROR, "FVoxelWorld", "Failed to Set World Size. Invalid [Z] value. Value must be >= 0" );
		return false;
	}
	
	WorldSize_X = IN_X;
	WorldSize_Z = IN_Z;
	
	std::string W = "[" + std::to_string(IN_X) + "," + std::to_string(IN_Z) + "]";
	Log(F_LOG_SEV_DEBUG,"FVoxelWorld","World Size set to " + W);
	
	return true;
}

