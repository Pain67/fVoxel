#include "fVoxel.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <cmath>


// ----------------------------------------------------------------------------------------------------
// Utility Structures

fProcMesh& fProcMesh::operator+=(const fProcMesh& REF_Other) {
    fUInt VNum = REF_Other.Vertecies.size();
    if (VNum > 0) {
        for (fUInt X = 0; X < VNum; X++) {
            Vertecies.push_back(REF_Other.Vertecies[X]);
            Normals.push_back(REF_Other.Normals[X]);
        }
    }

    fUInt UNum = REF_Other.UVs.size();
    if (UNum > 0) {
        for(fUInt X = 0; X < UNum; X++) { UVs.push_back(REF_Other.UVs[X]); }
    }

    return *this;
}

// ----------------------------------------------------------------------------------------------------
// fVoxel Internal Defs
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// fVoxelRegionData

fVoxelRegionData::fVoxelRegionData(fVoxelWorld* IN_WorldPtr) { WorldPtr = IN_WorldPtr; }

fBool fVoxelRegionData::LoadHeader() {
    std::string FileName = WorldPtr->GetRegionHeaderFile(RX,RZ);

    fLong BufferSize = 0;
    fUChar* DataPtr = nullptr;

    WorldPtr->IO_LoadBinaryData(FileName, &DataPtr, BufferSize);

    fUInt Num = ((BufferSize / 4) - 4) / 6;
    fUInt* IntDataPtr = (fUInt*)DataPtr;

    EntryList.clear();
    EntryList.resize(Num, fVoxelRegionEntry());

    RX = IntDataPtr[0];
    RZ = IntDataPtr[1];
    EOF_Offset = (fLong)IntDataPtr[2] << 32;
    EOF_Offset |= IntDataPtr[3];

    fUInt Pos = 4;
    for (fUInt X = 0; X < Num; X++) {
        EntryList[X].ReadFromBuffer(IntDataPtr, Pos);
    }

    return true;
}
fBool fVoxelRegionData::SaveHeader() {
    std::string FileName = WorldPtr->GetRegionHeaderFile(RX,RZ);

    fUInt Num = EntryList.size();
    fLong BufferNum = 4;                 // 4 fUInt for the region
    BufferNum += Num * 6;   // 6 fUInt per Entry

    fUInt* Buffer = (fUInt*)WorldPtr->Allocator(sizeof(fUInt) * BufferNum);

    Buffer[0] = RX;
    Buffer[1] = RZ;
    Buffer[2] = EOF_Offset >> 32;
    Buffer[3] = EOF_Offset;

    fUInt Pos = 4;
    for (fUInt X = 0; X < Num; X++) {
        EntryList[X].AppentToBuffer(Buffer, Pos);
    }

    fLong ByteSize = BufferNum * 4;

    WorldPtr->IO_SaveBinaryData(FileName, (fUChar*)Buffer, ByteSize);

    WorldPtr->DeAllocator(Buffer);

    return true;
}

fUInt fVoxelRegionData::SaveNewEntry(fVoxelRegionEntry& REF_Entry, fUChar* IN_DataPtr, fUInt IN_DataSize) {
    EntryList.push_back(REF_Entry);
    EOF_Offset += REF_Entry.Size;

    SaveHeader();
    std::string FileName = WorldPtr->GetRegionDataFile(RX,RZ);

    if (!WorldPtr->IO_isFileExist(FileName)) {
        WorldPtr->IO_CreateEmptyFile(FileName);
    }

    return WorldPtr->IO_AppendBinaryData(FileName, IN_DataPtr, IN_DataSize, F_LONG_MAX);
}
fBool fVoxelRegionData::OverrideEntry(fUInt IN_EntryIndex, fVoxelRegionEntry& REF_Entry, fUChar* IN_DataPtr, fUInt IN_DataSize) {
    // Temporarily save previous Entry
    fVoxelRegionEntry TempEntry = EntryList[IN_EntryIndex];

    // Override Entry
    EntryList[IN_EntryIndex] = REF_Entry;

    // Update All offsets after this entry
    fLong Diff = REF_Entry.Size - TempEntry.Size;
    fUInt Num = EntryList.size();
    fUInt X = IN_EntryIndex + 1;
    if (X < Num - 1) {
        while (X < Num) { EntryList[X].Offset += Diff; X++; }
    }

    //
    SaveHeader();

    std::string FileName = WorldPtr->GetRegionDataFile(RX,RZ);

    fUChar* Before = nullptr;
    fUChar* After = nullptr;
    fLong BeforeSize = 0;
    fLong AfterSize = 0;

    WorldPtr->IO_LoadBinaryData(
        FileName,
        TempEntry.Offset,
        TempEntry.Size,
        &Before,
        &After,
        BeforeSize,
        AfterSize
    );

    fLong FinalSize = BeforeSize + IN_DataSize + AfterSize;
    fUChar* FinalBuffer = (fUChar*)WorldPtr->Allocator(sizeof(fUChar) * FinalSize);
    memcpy(FinalBuffer,Before, BeforeSize);
    memcpy(&FinalBuffer[BeforeSize],IN_DataPtr, IN_DataSize);
    memcpy(&FinalBuffer[BeforeSize + IN_DataSize],After, AfterSize);

    WorldPtr->IO_SaveBinaryData(FileName, FinalBuffer, FinalSize);

    WorldPtr->DeAllocator(Before);
    WorldPtr->DeAllocator(After);
    WorldPtr->DeAllocator(FinalBuffer);

    return true;
}
fBool fVoxelRegionData::LoadEntry(fUInt IN_EntryIndex, fUChar* OUT_DataPtr) {
    std::string FileName = WorldPtr->GetRegionDataFile(RX,RZ);

    WorldPtr->IO_LoadBinaryData(FileName, OUT_DataPtr, EntryList[IN_EntryIndex].Size, EntryList[IN_EntryIndex].Offset);

    return true;
}
fUInt fVoxelRegionData::GetChunkEntryIndex(fInt IN_PosX, fInt IN_PosZ) {
    fUInt Num = EntryList.size();
    if (Num == 0) { return F_UINT_MAX; }

    for (fUInt X = 0; X < Num; X++) {
        if (EntryList[X].PosX == IN_PosX && EntryList[X].PosZ == IN_PosZ) { return X; }
    }

    return F_UINT_MAX;
}
// ----------------------------------------------------------------------------
// Chunk

fBool fVoxelChunk::_Internal_CompressData(std::vector<fVector2ui>& REF_Data) {
    REF_Data.clear();
    REF_Data.push_back({1,BlockList[0]});
    fUInt Index = 0;

    fLong BlocksPerChunk = WorldPtr->Get_BlocksPerChunk();
    for (fLong X = 1; X < BlocksPerChunk; X++) {
        if (BlockList[X] == REF_Data[Index].Y) {
            REF_Data[Index].X++;
        }
        else {
            REF_Data.push_back({1, BlockList[X]});
            Index++;
        }
    }

    return true;
}
fBool fVoxelChunk::_Internal_DeCompressData(std::vector<fVector2ui>& REF_Data) {
    fUInt Num = REF_Data.size();
    fUInt Index = 0;
    for (fUInt X = 0; X < Num; X++) {
        for (fUInt Y = 0; Y < REF_Data[X].X; Y++) {
            BlockList[Index++] = REF_Data[X].Y;
        }
    }

    return true;
}
fBool fVoxelChunk::_Internal_Validate(std::string IN_What) {
    if (WorldPtr == nullptr) {
        WorldPtr->Log(F_LOG_SEV_ERROR,"FVoxelChunk","Unable to " + IN_What + ". World pointer is NULL.");
        return false;
    }
    if (RegionPtr == nullptr) {
        WorldPtr->Log(F_LOG_SEV_ERROR,"FVoxelChunk","Unable to " + IN_What + ". Region pointer is NULL.");
        return false;
    }
    return true;
}
fUInt fVoxelChunk::GetVoxelIndex(fUInt IN_X, fUInt IN_Y, fUInt IN_Z) {
    fVector3ui ChunkSize = WorldPtr->GetChunkSize();
    // X > Z > Y

    if (IN_X >= ChunkSize.X) { return F_UINT_MAX; }
    if (IN_Y >= ChunkSize.Y) { return F_UINT_MAX; }
    if (IN_Z >= ChunkSize.Z) { return F_UINT_MAX; }

    return (IN_Y * (ChunkSize.Z * ChunkSize.X)) + (IN_Z * ChunkSize.X) + IN_X;
}
fBool fVoxelChunk::SaveChunkData() {
    if (!_Internal_Validate("SaveChunkData")) { return false; }

    std::vector<fVector2ui> C_Data;
    _Internal_CompressData(C_Data);
    fUInt DataSize = C_Data.size() * 8;
    fBool Result = false;

    if (RegionEntryIndex == F_UINT_MAX) {
        fVoxelRegionEntry Entry;
        Entry.PosX = PosX;
        Entry.PosZ = PosZ;
        Entry.Offset = RegionPtr->EOF_Offset;
        Entry.Size = DataSize;

        Result = RegionEntryIndex = RegionPtr->SaveNewEntry(Entry, (fUChar*)C_Data.data(), DataSize);
    }
    else {
        fVoxelRegionEntry Entry = RegionPtr->EntryList[RegionEntryIndex];
        Entry.Size = DataSize;

        Result = RegionPtr->OverrideEntry(RegionEntryIndex, Entry, (fUChar*)C_Data.data(), DataSize);
    }

    if (Result) {isModified = false;}
    return Result;
}
fBool fVoxelChunk::LoadChunkData() {
    if (!_Internal_Validate("LoadChunkData")) { return false; }

    fVoxelRegionEntry& E = RegionPtr->EntryList[RegionEntryIndex];
    fUChar* Buffer = (fUChar*)WorldPtr->Allocator(sizeof(fUChar) * E.Size);
    fBool Result = RegionPtr->LoadEntry(RegionEntryIndex, Buffer);

    std::vector<fVector2ui> C_Data;
    fUInt C_Size = E.Size / 8;
    C_Data.resize(C_Size, {0,0});
    memcpy(C_Data.data(), Buffer, E.Size);
    return _Internal_DeCompressData(C_Data);
}


// ----------------------------------------------------------------------------
// Chunk
fVoxelBlock::fVoxelBlock(fUInt IN_UID, std::string IN_Name, fVector2ui IN_Texture, fUChar IN_Flags) {
    UID = IN_UID;
    Name = IN_Name;
    Texture = IN_Texture;
    Flags = IN_Flags;
}




// ----------------------------------------------------------------------------------------------------
// External Interface Class - Main Class
// ----------------------------------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// I/O Related Stuff

fBool fVoxelWorld::IO_isFileExist(std::string IN_FileName) {
    return std::filesystem::exists(IN_FileName);
}
fBool fVoxelWorld::IO_CreateEmptyFile(std::string IN_FileName) {
    std::ofstream OutFile(IN_FileName);
    if (!OutFile.is_open()) { return false; }
    OutFile.close();
    return true;
}
fBool fVoxelWorld::IO_SaveBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize) {
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
fBool fVoxelWorld::IO_AppendBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset) {
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
fBool fVoxelWorld::IO_LoadBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset) {
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
fBool fVoxelWorld::IO_LoadBinaryData(std::string IN_FileName, fUChar** OUT_DataPtr, fLong& OUT_BufferSize) {
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
    fUChar* Buffer = (fUChar*)Allocator(sizeof(fUChar) * FileSize);

    // Read File into Buffer
    InFile.read((char*)Buffer, FileSize);

    //
    InFile.close();
    (*OUT_DataPtr) = Buffer;
    OUT_BufferSize = FileSize;
    return true;
}
fBool fVoxelWorld::IO_LoadBinaryData(std::string IN_FileName, fLong IN_Offset, fLong IN_Size, fUChar** OUT_Before, fUChar** OUT_After, fLong& OUT_BeforeSize, fLong& OUT_AfterSize) {
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
    fUChar* Before = (fUChar*)Allocator(sizeof(fUChar) * IN_Offset);
    fUChar* After = (fUChar*)Allocator(sizeof(fUChar) * (FileSize - (IN_Offset + IN_Size)));

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

// ----------------------------------------------------------------------------
// Log Related Stuff

std::string fVoxelWorld::Log_Sev_To_String(fUChar IN_Sev) {
    switch(IN_Sev) {
        case F_LOG_SEV_DEBUG: return "DEBUG";
        case F_LOG_SEV_INFO: return "INFO";
        case F_LOG_SEV_WARNING: return "WARNING";
        case F_LOG_SEV_ERROR: return "ERROR";
        default: return "{#NULL}";
    }
}
void fVoxelWorld::Log(fUChar IN_Sev, std::string IN_SenderName, std::string IN_Msg) {
    if (IN_Sev < Log_MinLevel) { return; }
    if (Log_FunctionPtr != nullptr) { Log_FunctionPtr(IN_Sev, IN_SenderName, IN_Msg); return; }

    std::string S = Log_Sev_To_String(IN_Sev);
    std::cout << "[" << S << "] " << IN_SenderName << " => " << IN_Msg << std::endl;
}

// ----------------------------------------------------------------------------
// Util Functions

std::string fVoxelWorld::GetWorldFile() {
    return SavePath + WorldFolderName + "/" + WolrdFileName;
}
std::string fVoxelWorld::GetRegionHeaderFile(fInt IN_RX, fInt IN_RZ) {
    return SavePath + WorldFolderName + "/" + RegionFolderName + "/" + RegionHeaderName + "_" + std::to_string(IN_RX) + "_" + std::to_string(IN_RZ);
}
std::string fVoxelWorld::GetRegionDataFile(fInt IN_RX, fInt IN_RZ) {
    return SavePath + WorldFolderName + "/" + RegionFolderName + "/" + RegionDataName + "_" + std::to_string(IN_RX) + "_" + std::to_string(IN_RZ);
}


// ----------------------------------------------------------------------------------------------------
// External Interface Class - Main Class
// ----------------------------------------------------------------------------------------------------


void fVoxelWorld::_Internal_Init() {
    ChunksPerWorld = WorldSize_X * WorldSize_Z;
    BlocksPerChunk = ChunkSize_X * ChunkSize_Y * ChunkSize_Z;

    ChunkList.resize(ChunksPerWorld, fVoxelChunk(this));

    _Internal_CalculateTempVerts();
}
void fVoxelWorld::_Internal_CalculateTempVerts() {
    TempVertNum_Front = VoxelMesh[0].Vertecies.size();
    TempVertNum_Back = VoxelMesh[1].Vertecies.size();
    TempVertNum_Left = VoxelMesh[2].Vertecies.size();
    TempVertNum_Right = VoxelMesh[3].Vertecies.size();
    TempVertNum_Top = VoxelMesh[4].Vertecies.size();
    TempVertNum_Bottom = VoxelMesh[5].Vertecies.size();
    TempVertNum_Always = VoxelMesh[6].Vertecies.size();
}
fBool fVoxelWorld::_Internal_SaveWorldProp() {
    fUInt Properties[7] = {
        ChunkSize_X,
        ChunkSize_Y,
        ChunkSize_Z,
        RegionSize_X,
        RegionSize_Z,
        WorldSize_X,
        WorldSize_Z
    };

    std::string FinalString = WorldFolderName + "#" + RegionFolderName + "#" + WolrdFileName + "#" + RegionHeaderName + "#" + RegionDataName;
    fUInt FinalSize = FinalString.length();

    fLong BufferSize = 7 * sizeof(fUInt);
    BufferSize += FinalSize + 1;            // +1 for the terminateing 0

    fUChar* Buffer = (fUChar*)Allocator(BufferSize);

    memcpy(Buffer, (fUChar*)&Properties[0], 7 * sizeof(fUInt));
    strcpy((char*)&Buffer[7 * sizeof(fUInt)], &FinalString[0]);
    Buffer[BufferSize - 1] = 0;

    std::string FileName = GetWorldFile();
    IO_SaveBinaryData(FileName, Buffer, BufferSize);

    DeAllocator(Buffer);
    return true;
}
fBool fVoxelWorld::_Internal_LoadWorldProp(std::string IN_FileName) {
    fUChar* Buffer = nullptr;
    fLong BufferSize = 0;

    IO_LoadBinaryData(IN_FileName, &Buffer, BufferSize);

    fUInt Properties[7] = {0};
    memcpy(&Properties[0], Buffer, 7 * sizeof(fUInt));

    ChunkSize_X = Properties[0];
    ChunkSize_Y = Properties[1];
    ChunkSize_Z = Properties[2];
    RegionSize_X = Properties[3];
    RegionSize_Z = Properties[4];
    WorldSize_X = Properties[5];
    WorldSize_Z = Properties[6];

    std::string TempString = (char*)&Buffer[7 * sizeof(fUInt)];
    fUInt TempSize= TempString.length();

    std::string StrList[5] = {""};
    fUInt StrIndex = 0;
    std::string Curr = "";
    for (fUInt X = 0; X < TempSize; X++) {
        if (TempString[X] == '#') {
            StrList[StrIndex++] = Curr;
            Curr = "";
        }
        else { Curr += TempString[X]; }
    }
    if (Curr.length() > 0) { StrList[StrIndex++] = Curr; }

    WorldFolderName = StrList[0];
    RegionFolderName = StrList[1];
    WolrdFileName = StrList[2];
    RegionHeaderName = StrList[3];
    RegionDataName = StrList[4];

    DeAllocator(Buffer);

    return false;
}


fVector2i fVoxelWorld::_Internal_GetRegionPos(fInt IN_PosX, fInt IN_PosZ) {
    if (RegionSize_X == 0) { throw 0; }
    if (RegionSize_Z == 0) { throw 0; }

    fInt X = IN_PosX / RegionSize_X;
    fInt Z = IN_PosZ / RegionSize_Z;
    if (IN_PosX < 0) { X--; }
    if (IN_PosZ < 0) { Z--; }

    return {X,Z};
}
fUInt fVoxelWorld::_Internal_GetRegionIndex(fInt IN_PosX, fInt IN_PosZ) {
    fUInt Num = RegionList.size();
    if (Num == 0) { return F_UINT_MAX; }

    for (fUInt X = 0; X < Num; X++) {
        if (RegionList[X].RX != IN_PosX) { continue; }
        if (RegionList[X].RZ != IN_PosZ) { continue; }
        return X;
    }

    return F_UINT_MAX;
}
fUInt fVoxelWorld::_Internal_CreateRegion(fInt IN_PosX, fInt IN_PosZ) {
    fUInt Index = RegionList.size();
    RegionList.push_back(fVoxelRegionData(this));
    RegionList[Index].RX = IN_PosX;
    RegionList[Index].RZ = IN_PosZ;

    // See if this region have a saved data or not
    std::string FileName = GetRegionHeaderFile(IN_PosX, IN_PosZ);
    if (IO_isFileExist(FileName)) {
        Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Loading Region [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]");
        RegionList[Index].LoadHeader();
    }
    else {
        Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Create Region [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]");
        // Dont save header without the Data
        // If only header saved, loading region will assume data exist and will attempt to load it
        //RegionList[Index].SaveHeader();
    }

    return Index;
}

fUInt fVoxelWorld::_Internal_GetChunkIndex(fInt IN_X, fInt IN_Z) {
    for (fLong X = 0; X < ChunksPerWorld; X++) {
        if (ChunkList[X].isExist == false) { continue; }
        if (ChunkList[X].PosX != IN_X) { continue; }
        if (ChunkList[X].PosZ != IN_Z) { continue; }
        return X;
    }
    return F_UINT_MAX;
}
fUInt fVoxelWorld::_Internal_GetEmptyChunk() {
    for (fLong X = 0; X < ChunksPerWorld; X++) {
        if (ChunkList[X].isExist == false) { return X; }
    }
    return F_UINT_MAX;
}
fBool fVoxelWorld::_Internal_GenerateVoxel(fUInt IN_ChunkIndex, fUInt IN_BlockIndex, fVoxelLocalPos IN_Pos, fProcMesh& OUT_Mesh) {
    fProcMesh CurrMesh;
    fVoxelGlobalPos GPos = GetVoxelGlobalPos(IN_Pos);

    // 0 - Front	Z-
    if (TempVertNum_Front > 0) {
        if (IN_Pos.LocalZ > 0) {
            fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY, IN_Pos.LocalZ - 1);
            if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += VoxelMesh[0]; }
        }
        else {
            fVoxelGlobalPos TempPos = GPos;
            TempPos.GlobalZ--;
            fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
            if (Index == F_UINT_MAX) { CurrMesh += VoxelMesh[0]; }
        }
    }

    // 1 - Back		Z+
    if (TempVertNum_Back > 0) {
        if (IN_Pos.LocalZ < (fInt)ChunkSize_Z - 1) {
            fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY, IN_Pos.LocalZ + 1);
            if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += VoxelMesh[1]; }
        }
        else {
            fVoxelGlobalPos TempPos = GPos;
            TempPos.GlobalZ++;
            fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
            if (Index == F_UINT_MAX) { CurrMesh += VoxelMesh[1]; }
        }
    }

    // 2 - Left		X+
    if (TempVertNum_Left > 0) {
        if (IN_Pos.LocalX < (fInt)ChunkSize_X - 1) {
            fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX + 1, IN_Pos.LocalY, IN_Pos.LocalZ);
            if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += VoxelMesh[2]; }
        }
        else {
            fVoxelGlobalPos TempPos = GPos;
            TempPos.GlobalX++;
            fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
            if (Index == F_UINT_MAX) { CurrMesh += VoxelMesh[2]; }
        }
    }

    // 3 - Right	X-
    if (TempVertNum_Right > 0) {
        if (IN_Pos.LocalX > 0) {
            fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX - 1, IN_Pos.LocalY, IN_Pos.LocalZ);
            if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += VoxelMesh[3]; }
        }
        else {
            fVoxelGlobalPos TempPos = GPos;
            TempPos.GlobalX--;
            fUInt Index = GetVoxelIndex(TempPos.GlobalX,TempPos.GlobalY,TempPos.GlobalZ);
            if (Index == F_UINT_MAX) { CurrMesh += VoxelMesh[3]; }
        }
    }

    // 4 - Top		Y+
    if (TempVertNum_Top > 0) {
        if (IN_Pos.LocalY < (fInt)ChunkSize_Y - 1) {
            fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY + 1, IN_Pos.LocalZ);
            if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += VoxelMesh[4]; }
        }
        else { CurrMesh += VoxelMesh[4]; }
    }

    // 5 - Bottom	Y-
    if (TempVertNum_Bottom > 0) {
        if (IN_Pos.LocalY > 0) {
            fUInt I = ChunkList[IN_ChunkIndex].GetVoxelIndex(IN_Pos.LocalX, IN_Pos.LocalY - 1, IN_Pos.LocalZ);
            if (ChunkList[IN_ChunkIndex].BlockList[I] == F_UINT_MAX) { CurrMesh += VoxelMesh[5]; }
        }
        else { CurrMesh += VoxelMesh[5]; }
    }

    // 6 - Always
    if (TempVertNum_Always > 0) { CurrMesh += VoxelMesh[6]; }

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


fBool fVoxelWorld::CreateWorld(std::string IN_FolderPath) {
    if (isInit) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Create world. A world is already initialised.");
        return false;
    }

    SavePath = IN_FolderPath;
    if (SavePath[SavePath.length() - 1] != '/') { SavePath += '/'; }

    std::string FileName = GetWorldFile();

    if (IO_isFileExist(FileName)) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Create world. A world is already exist in the specified location.");
        return false;
    }

    Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Creating World at [" + FileName + "]");

    // Create Folder Structure
    std::filesystem::create_directory(SavePath + WorldFolderName);
    std::filesystem::create_directory(SavePath + WorldFolderName + "/" + RegionFolderName);

    _Internal_SaveWorldProp();
    _Internal_Init();
    isInit = true;

    return false;
}
fBool fVoxelWorld::LoadWorld(std::string IN_FilePath) {
    if (isInit) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Load world. A world is already initialised.");
        return false;
    }

    if (!IO_isFileExist(IN_FilePath)) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Load world. World file at [" + IN_FilePath + "] does not exist.");
        return false;
    }
    _Internal_LoadWorldProp(IN_FilePath);

    std::filesystem::path Temp(IN_FilePath);
    Temp = Temp.parent_path();
    SavePath = Temp.parent_path();
    SavePath += "/";

    if (IN_FilePath != GetWorldFile()) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Load world. World file at [" + IN_FilePath + "] in incorrect.");
        return false;
    }

    Log(F_LOG_SEV_DEBUG,"FVoxelWorld","World loaded from [" + IN_FilePath + "].");

    _Internal_Init();
    isInit = true;

    return false;
}
fBool fVoxelWorld::isWorldExist(std::string IN_FilePath) {
    return IO_isFileExist(IN_FilePath);
}
fUInt fVoxelWorld::SpawnChunk(fInt IN_PosX, fInt IN_PosZ) {
    if (!isInit) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Spawn Chunk. The world is not yet initialised.");
        return F_UINT_MAX;
    }

    // See if chunk already loaded
    fUInt ChunkIndex = _Internal_GetChunkIndex(IN_PosX, IN_PosZ);
    if (ChunkIndex < F_UINT_MAX) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Spawn Chunk. Chunk Already Exist.");
        return F_UINT_MAX;
    }
    else {
        ChunkIndex = _Internal_GetEmptyChunk();
        if (ChunkIndex == F_UINT_MAX) {
            Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to Spawn Chunk. No empty chunk found.");
            return F_UINT_MAX;
        }
    }

    // Get Target Region
    fVector2i RPos = _Internal_GetRegionPos(IN_PosX, IN_PosZ);
    fUInt RIndex = _Internal_GetRegionIndex(RPos.X, RPos.Y);
    if (RIndex == F_UINT_MAX) {
        RIndex = _Internal_CreateRegion(RPos.X, RPos.Y);
    }

    // Configure Chunk
    ChunkList[ChunkIndex].isExist = true;
    ChunkList[ChunkIndex].PosX = IN_PosX;
    ChunkList[ChunkIndex].PosZ = IN_PosZ;
    ChunkList[ChunkIndex].RegionPtr = &RegionList[RIndex];
    ChunkList[ChunkIndex].RegionEntryIndex = RegionList[RIndex].GetChunkEntryIndex(IN_PosX, IN_PosZ);

    if (!ChunkList[ChunkIndex].isAllocated) {
        fLong AllocSize = sizeof(fUInt) * BlocksPerChunk;
        ChunkList[ChunkIndex].BlockList = (fUInt*)Allocator(AllocSize);
        memset(ChunkList[ChunkIndex].BlockList, 0xFF , AllocSize);
        ChunkList[ChunkIndex].isAllocated = true;
    }

    if (ChunkList[ChunkIndex].RegionEntryIndex < F_UINT_MAX) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Loading Chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]");
        ChunkList[ChunkIndex].LoadChunkData();
    }
    else {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Creating Chunk [" + std::to_string(IN_PosX) + "," + std::to_string(IN_PosZ) + "]");
    }

    return RIndex;
}
fBool fVoxelWorld::SaveChunk(fUInt IN_ChunkIndex) {
    if (!isInit) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to save chunk. World not yet initialised");
        return false;
    }
    if (IN_ChunkIndex >= ChunksPerWorld) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to save chunk. Invalid Chunk Index");
        return false;
    }
    Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Saving Chunk [" + std::to_string(ChunkList[IN_ChunkIndex].PosX) + "," + std::to_string(ChunkList[IN_ChunkIndex].PosZ) + "]");

    if (!ChunkList[IN_ChunkIndex].isExist) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to save chunk. Chunk not yet loaded");
        return false;
    }
    if (ChunkList[IN_ChunkIndex].isModified) {
        Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Save Chunk data for Chunk[" + std::to_string(IN_ChunkIndex) + "]");
        return ChunkList[IN_ChunkIndex].SaveChunkData();
    }

    return true;
}
fBool fVoxelWorld::UnloadChunk(fUInt IN_ChunkIndex, fBool IN_isSave) {
    if (!isInit) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable unload chunk. World not yet initialised");
        return false;
    }

    if (IN_ChunkIndex >= ChunksPerWorld) {
        Log(
            F_LOG_SEV_ERROR,
            "FoxelWorld",
            "Unable to unload chunk with Index [" + std::to_string(IN_ChunkIndex) + "]. Index out of bounds."
        );
        return false;
    }

    Log(F_LOG_SEV_DEBUG,"FVoxelWorld","Unload Chunk[" + std::to_string(IN_ChunkIndex) + "]");

    if (IN_isSave) {
        if (ChunkList[IN_ChunkIndex].isModified) {
            ChunkList[IN_ChunkIndex].SaveChunkData();
        }
    }
    ChunkList[IN_ChunkIndex].isExist = false;
    return true;
}
fVoxelChunk* fVoxelWorld::GetChunkPtr(fUInt IN_ChunkIndex) {
    if (!isInit) {
        Log(F_LOG_SEV_ERROR,"FVoxelWorld","Unable to get chunk ref. World not yet initialised");
        return nullptr;
    }

    if (IN_ChunkIndex >= ChunksPerWorld) {
        Log(
            F_LOG_SEV_ERROR,
            "FoxelWorld",
            "Unable to get chunk ref for Index [" + std::to_string(IN_ChunkIndex) + "]. Index out of bounds."
        );
        return nullptr;
    }

    return &ChunkList[IN_ChunkIndex];
}
fBool fVoxelWorld::SaveWorld() {
    for (fUInt X = 0; X < ChunksPerWorld; X++) {
        if (ChunkList[X].isExist && ChunkList[X].isModified) {
            if (!ChunkList[X].SaveChunkData()) {
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
fBool fVoxelWorld::SetVoxelMesh(std::vector<fProcMesh> IN_MeshList) {
    if (IN_MeshList.size() != 7) {
        Log(
            F_LOG_SEV_ERROR,
            "FVoxelWorld",
            "Unable to set VoxelMeshList. Expected [7] Elements, provided [" + std::to_string(IN_MeshList.size()) + "]."
        );
        return false;
    }

    VoxelMesh[0] = IN_MeshList[0];
    VoxelMesh[1] = IN_MeshList[1];
    VoxelMesh[2] = IN_MeshList[2];
    VoxelMesh[3] = IN_MeshList[3];
    VoxelMesh[4] = IN_MeshList[4];
    VoxelMesh[5] = IN_MeshList[5];
    VoxelMesh[6] = IN_MeshList[6];

    _Internal_CalculateTempVerts();

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
    VoxelMesh[0].Vertecies.push_back(CubeVertecies[0]);
    VoxelMesh[0].Vertecies.push_back(CubeVertecies[4]);
    VoxelMesh[0].Vertecies.push_back(CubeVertecies[7]);

    VoxelMesh[0].Vertecies.push_back(CubeVertecies[3]);
    VoxelMesh[0].Vertecies.push_back(CubeVertecies[0]);
    VoxelMesh[0].Vertecies.push_back(CubeVertecies[7]);

    VoxelMesh[0].Normals.push_back(CubeNormals[1]);
    VoxelMesh[0].Normals.push_back(CubeNormals[1]);
    VoxelMesh[0].Normals.push_back(CubeNormals[1]);

    VoxelMesh[0].Normals.push_back(CubeNormals[1]);
    VoxelMesh[0].Normals.push_back(CubeNormals[1]);
    VoxelMesh[0].Normals.push_back(CubeNormals[1]);

    VoxelMesh[0].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[0].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[0].UVs.push_back({1.0F, 0.0F});

    VoxelMesh[0].UVs.push_back({1.0F, 1.0F});
    VoxelMesh[0].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[0].UVs.push_back({1.0F, 0.0F});


    // 1 - Back		Z+
    VoxelMesh[1].Vertecies.push_back(CubeVertecies[2]);
    VoxelMesh[1].Vertecies.push_back(CubeVertecies[6]);
    VoxelMesh[1].Vertecies.push_back(CubeVertecies[5]);

    VoxelMesh[1].Vertecies.push_back(CubeVertecies[1]);
    VoxelMesh[1].Vertecies.push_back(CubeVertecies[2]);
    VoxelMesh[1].Vertecies.push_back(CubeVertecies[5]);

    VoxelMesh[1].Normals.push_back(CubeNormals[0]);
    VoxelMesh[1].Normals.push_back(CubeNormals[0]);
    VoxelMesh[1].Normals.push_back(CubeNormals[0]);

    VoxelMesh[1].Normals.push_back(CubeNormals[0]);
    VoxelMesh[1].Normals.push_back(CubeNormals[0]);
    VoxelMesh[1].Normals.push_back(CubeNormals[0]);

    VoxelMesh[1].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[1].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[1].UVs.push_back({1.0F, 0.0F});

    VoxelMesh[1].UVs.push_back({1.0F, 1.0F});
    VoxelMesh[1].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[1].UVs.push_back({1.0F, 0.0F});


    // 2 - Left		X+
    VoxelMesh[2].Vertecies.push_back(CubeVertecies[3]);
    VoxelMesh[2].Vertecies.push_back(CubeVertecies[7]);
    VoxelMesh[2].Vertecies.push_back(CubeVertecies[6]);

    VoxelMesh[2].Vertecies.push_back(CubeVertecies[2]);
    VoxelMesh[2].Vertecies.push_back(CubeVertecies[3]);
    VoxelMesh[2].Vertecies.push_back(CubeVertecies[6]);

    VoxelMesh[2].Normals.push_back(CubeNormals[2]);
    VoxelMesh[2].Normals.push_back(CubeNormals[2]);
    VoxelMesh[2].Normals.push_back(CubeNormals[2]);

    VoxelMesh[2].Normals.push_back(CubeNormals[2]);
    VoxelMesh[2].Normals.push_back(CubeNormals[2]);
    VoxelMesh[2].Normals.push_back(CubeNormals[2]);

    VoxelMesh[2].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[2].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[2].UVs.push_back({1.0F, 0.0F});

    VoxelMesh[2].UVs.push_back({1.0F, 1.0F});
    VoxelMesh[2].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[2].UVs.push_back({1.0F, 0.0F});

    // 3 - Right	X-
    VoxelMesh[3].Vertecies.push_back(CubeVertecies[1]);
    VoxelMesh[3].Vertecies.push_back(CubeVertecies[5]);
    VoxelMesh[3].Vertecies.push_back(CubeVertecies[4]);

    VoxelMesh[3].Vertecies.push_back(CubeVertecies[0]);
    VoxelMesh[3].Vertecies.push_back(CubeVertecies[1]);
    VoxelMesh[3].Vertecies.push_back(CubeVertecies[4]);

    VoxelMesh[3].Normals.push_back(CubeNormals[3]);
    VoxelMesh[3].Normals.push_back(CubeNormals[3]);
    VoxelMesh[3].Normals.push_back(CubeNormals[3]);

    VoxelMesh[3].Normals.push_back(CubeNormals[3]);
    VoxelMesh[3].Normals.push_back(CubeNormals[3]);
    VoxelMesh[3].Normals.push_back(CubeNormals[3]);

    VoxelMesh[3].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[3].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[3].UVs.push_back({1.0F, 0.0F});

    VoxelMesh[3].UVs.push_back({1.0F, 1.0F});
    VoxelMesh[3].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[3].UVs.push_back({1.0F, 0.0F});


    // 4 - Top		Y+
    VoxelMesh[4].Vertecies.push_back(CubeVertecies[4]);
    VoxelMesh[4].Vertecies.push_back(CubeVertecies[5]);
    VoxelMesh[4].Vertecies.push_back(CubeVertecies[6]);

    VoxelMesh[4].Vertecies.push_back(CubeVertecies[7]);
    VoxelMesh[4].Vertecies.push_back(CubeVertecies[4]);
    VoxelMesh[4].Vertecies.push_back(CubeVertecies[6]);

    VoxelMesh[4].Normals.push_back(CubeNormals[4]);
    VoxelMesh[4].Normals.push_back(CubeNormals[4]);
    VoxelMesh[4].Normals.push_back(CubeNormals[4]);

    VoxelMesh[4].Normals.push_back(CubeNormals[4]);
    VoxelMesh[4].Normals.push_back(CubeNormals[4]);
    VoxelMesh[4].Normals.push_back(CubeNormals[4]);

    VoxelMesh[4].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[4].UVs.push_back({0.0F, 1.0F});
    VoxelMesh[4].UVs.push_back({1.0F, 1.0F});

    VoxelMesh[4].UVs.push_back({1.0F, 0.0F});
    VoxelMesh[4].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[4].UVs.push_back({1.0F, 1.0F});

    // 5 - Bottom	Y-
    VoxelMesh[5].Vertecies.push_back(CubeVertecies[0]);
    VoxelMesh[5].Vertecies.push_back(CubeVertecies[2]);
    VoxelMesh[5].Vertecies.push_back(CubeVertecies[1]);

    VoxelMesh[5].Vertecies.push_back(CubeVertecies[3]);
    VoxelMesh[5].Vertecies.push_back(CubeVertecies[2]);
    VoxelMesh[5].Vertecies.push_back(CubeVertecies[0]);

    VoxelMesh[5].Normals.push_back(CubeNormals[5]);
    VoxelMesh[5].Normals.push_back(CubeNormals[5]);
    VoxelMesh[5].Normals.push_back(CubeNormals[5]);

    VoxelMesh[5].Normals.push_back(CubeNormals[5]);
    VoxelMesh[5].Normals.push_back(CubeNormals[5]);
    VoxelMesh[5].Normals.push_back(CubeNormals[5]);

    VoxelMesh[5].UVs.push_back({0.0F, 0.0F});
    VoxelMesh[5].UVs.push_back({1.0F, 1.0F});
    VoxelMesh[5].UVs.push_back({0.0F, 1.0F});

    VoxelMesh[5].UVs.push_back({1.0F, 0.0F});
    VoxelMesh[5].UVs.push_back({1.0F, 1.0F});
    VoxelMesh[5].UVs.push_back({0.0F, 0.0F});

    _Internal_CalculateTempVerts();
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
fBool fVoxelWorld::GenerateChunkMesh(fUInt IN_ChunkIndex, fProcMesh& OUT_Mesh) {
    if (!isInit) { return false; }

    if (IN_ChunkIndex >= ChunksPerWorld) { return false; }

    fVoxelLocalPos LPos;
    LPos.ChunkX = ChunkList[IN_ChunkIndex].PosX;
    LPos.ChunkZ = ChunkList[IN_ChunkIndex].PosZ;

    for (fUInt Y = 0; Y < ChunkSize_Y; Y++) {
        LPos.LocalY = Y;

        for (fUInt Z = 0; Z < ChunkSize_Z; Z++) {
            LPos.LocalZ = Z;

            for (fUInt X = 0; X < ChunkSize_X; X++) {
                LPos.LocalX = X;

                fUInt Index = ChunkList[IN_ChunkIndex].GetVoxelIndex(X,Y,Z);
                if (ChunkList[IN_ChunkIndex].BlockList[Index] < F_UINT_MAX) {
                    if (_Internal_GenerateVoxel(IN_ChunkIndex, Index, LPos, OUT_Mesh)) {
                        ChunkList[IN_ChunkIndex].VisibleVoxels++;
                    }
                }
            }
        }
    }

    return true;
}
fBool fVoxelWorld::SetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& REF_Voxel) { return false; }
fBool fVoxelWorld::GetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& OUT_Voxel) { return false; }
fBool fVoxelWorld::ClearVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z) { return false; }
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



