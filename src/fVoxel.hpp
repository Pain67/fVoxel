#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <mutex>


// ----------------------------------------------------------------------------------------------------
// TypeDefs
// ----------------------------------------------------------------------------------------------------

#define F_LONG_MAX		INT64_MAX
#define F_UINT_MAX		UINT32_MAX

#define F_LOG_SEV_DEBUG			1
#define F_LOG_SEV_INFO			2
#define F_LOG_SEV_WARNING		3
#define F_LOG_SEV_ERROR			4
#define F_LOG_SEV_UNDEFINED		255

typedef int32_t		fInt;
typedef int64_t		fLong;
typedef uint8_t		fUChar;
typedef uint32_t	fUInt;
typedef float		fFloat;
typedef bool		fBool;

typedef void (*fLogCallback)(int,std::string,std::string);
typedef void* (*fMemoryAllocator)(size_t);
typedef void (*fMemoryDeAllocator)(void*);

// ----------------------------------------------------------------------------------------------------
// Utility Structures
// ----------------------------------------------------------------------------------------------------

struct fVector3 { fFloat X = 0.0F; fFloat Y = 0.0F; fFloat Z = 0.0F; };
struct fVector2 { fFloat X = 0.0F; fFloat Y = 0.0F; };
struct fVector2i { fInt X = 0; fInt Y = 0; };
struct fVector2ui { fUInt X = 0; fUInt Y = 0; };
struct fVector3i { fInt X = 0; fInt Y = 0; fInt Z = 0; };
struct fVector3ui { fUInt X = 0; fUInt Y = 0; fUInt Z = 0; };


struct fProcMesh {
	std::vector<fVector3> Vertecies;
	std::vector<fVector3> Normals;
	std::vector<fVector2> UVs;
	
	fProcMesh& operator+=(const fProcMesh& REF_Other);
};


// ----------------------------------------------------------------------------------------------------
// FVoxelDefs
// ----------------------------------------------------------------------------------------------------

struct fVoxelRegionEntry {
	fInt PosX = 0;
	fInt PosZ = 0;
	fLong Offset = 0;
	fLong Size = 0;
};

struct fVoxelRegionData {
	fInt RX = 0;
	fInt RZ = 0;
	fLong EOF_Offset = 0;
	std::vector<fVoxelRegionEntry> EntryList;
};

struct fVoxelBlock {
	fUInt UID = 0;
	std::string Name = "";
	fVector2ui Texture = {0,0};
	fUChar Flags = 0b00000000;
	
	fVoxelBlock(fUInt IN_UID = 0, std::string IN_Name = "", fVector2ui IN_Texture = {0,0}, fUChar IN_Flags = 0b00000000);
};
struct fVoxelLocalPos {
	fInt ChunkX = 0;
	fInt ChunkZ = 0;
	fInt LocalX = 0;
	fInt LocalY = 0;
	fInt LocalZ = 0;
	
	std::string ToString();
};
struct fVoxelGlobalPos {
	fInt GlobalX = 0;
	fInt GlobalY = 0;
	fInt GlobalZ = 0;
};

class fVoxelWorld;

class fVoxelChunk {
private:
	fVoxelWorld* WorldPtr = nullptr;
protected:
	fBool _Internal_GenerateVoxel(fUInt IN_X, fUInt IN_Y, fUInt IN_Z, fProcMesh& OUT_Mesh);
public:
	fVoxelChunk(fVoxelWorld* IN_WorldPtr) : WorldPtr(IN_WorldPtr) {}
	
	fInt PosX = 0;
	fInt PosZ = 0;
	fUInt* BlockList = nullptr;
	fBool isExist = false;
	fBool isModified = false;
	fBool isAllocated = false;
	fBool isVoxelGenerated = false;
	fBool isMeshGenerated = false;
	
	fUInt Debug_VisibleVoxels = 0;
	
	fUInt GetVoxelIndex(fUInt IN_X, fUInt IN_Y, fUInt IN_Z);
};

// ----------------------------------------------------------------------------------------------------
// Main Class
// ----------------------------------------------------------------------------------------------------

class fVoxelWorld {
private:
	// Total number of Chunks exist in memory at any one time
	// Calculated as "WorldSize_X*WorldSize_Z" in "_Internal_Init()"
	fLong ChunksPerWorld = 0;
	
	// Total number of Blocks per chunks
	// Calculated as "ChunkSize_X*ChunkSize_Y*ChunkSize_Z" in "_Internal_Init()"
	fLong BlocksPerChunk = 0;
	
	// Number of vertecies in each SubMesh 
	// To avoid getting to count vectors at each voxel mesh generation
	fUInt TempVertNum_Front = 0;
	fUInt TempVertNum_Back = 0;
	fUInt TempVertNum_Left = 0;
	fUInt TempVertNum_Right = 0;
	fUInt TempVertNum_Top = 0;
	fUInt TempVertNum_Bottom = 0;
	fUInt TempVertNum_Always = 0;
protected:
	// ----------------------------------------------------------------------------
	// World Configuration
	
	// Number of Voxel per Chunk
	fUInt ChunkSize_X = 33;
	fUInt ChunkSize_Y = 256;
	fUInt ChunkSize_Z = 33;
	
	// Number of Chunks Per Region
	fUInt RegionSize_X = 16;
	fUInt RegionSize_Z = 16;
	
	// Number of Chunks That can be loaded at any one time
	fUInt WorldSize_X = 32;
	fUInt WorldSize_Z = 32;
	
	// ----------------------------------------------------------------------------
	// Thread Stuff
	
	std::mutex IO_Lock;
	std::mutex Chunk_Lock;
	std::mutex Region_Lock;
	
	// ----------------------------------------------------------------------------
	// World Properties
	
	// Size of Each voxel
	fFloat VoxelSize_X = 1.0F;
	fFloat VoxelSize_Y = 1.0F;
	fFloat VoxelSize_Z = 1.0F;
	
	fFloat TextureStep_X = 1.0F;
	fFloat TextureStep_Y = 1.0F;
	
	// Indicates if world is initialised or not
	// Once world created, some properties can no longer be changed
	fBool isInit = false;
	
	// Defines the mesh to render for each Voxel
	// 0 - Front	Z-
	// 1 - Back		Z+
	// 2 - Left		X+
	// 3 - Right	X-
	// 4 - Top		Y+
	// 5 - Bottom	Y-
	// 6 - Always Visible
	fProcMesh FoxelMesh[7];
	
	std::vector<fVoxelBlock> VoxelList;
	
	// Root folder for save Data
	//	- Expected to be an absolute path
	std::string SavePath = "";
	
	std::vector<fVoxelChunk> ChunkList;
	std::vector<fVoxelRegionData> RegionList;
	
	// ----------------------------------------------------------------------------
	// I/O Related Stuff
	
	fBool isFileExist(std::string IN_FileName);
	fBool CreateEmptyFile(std::string IN_FileName);
	
	fBool SaveBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize);
	fBool AppendBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset = F_LONG_MAX);
	
	fBool LoadBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset = F_LONG_MAX);
	fBool LoadBinaryData(std::string IN_FileName, fUChar** OUT_DataPtr, fLong& OUT_BufferSize);
	fBool LoadBinaryData(std::string IN_FileName, fLong IN_Offset, fLong IN_Size, fUChar** OUT_Before, fUChar** OUT_After, fLong& OUT_BeforeSize, fLong& OUT_AfterSize);
	
	// ----------------------------------------------------------------------------
	// Log Related Stuff
	
	fLogCallback LogFunctionPtr = nullptr;
	fUChar MinLogLevel = 0;
	std::string LogSev_To_String(fUChar IN_Sev);
	void SetLogCallbackPtr(fLogCallback IN_Fnc);
	
	// ----------------------------------------------------------------------------
	// Stuff
	
	fMemoryAllocator Allocator = &DefaultMemAllocator;
	fMemoryDeAllocator DeAllocator = &DefaultMemDeAllocator;
	static void* DefaultMemAllocator(size_t IN_Bytes);
	static void DefaultMemDeAllocator(void* IN_Ptr);
	
	// ----------------------------------------------------------------------------
	// Internal Functions
	
	// Allocates memory for chunks and BlockLists
	void _Internal_Init();
	
	void _Internal_CalculateTempVerts();

	fBool _Internal_SaveWorldProp();
	fBool _Internal_LoadWorldProp();
	
	fUInt _Internal_LoadRegionHeader(fInt IN_X, fInt IN_Z);
	fBool _Internal_SaveRegionHeader(fVoxelRegionData& REF_Region);
	fBool _Internal_UpdateRegionData(fLong IN_Offset, fUInt IN_RIndex, fUInt IN_SIndex);
	
	fBool _Internal_CreateChunk(fVoxelChunk& REF_Chunk);
	fBool _Internal_LoadChunk(fVoxelChunk& REF_Chunk, fVoxelRegionData& REF_Region, fUInt IN_SavedIndex);
	fBool _Internal_OverrideChunk(std::string& REF_DataFileName, fInt IN_PosX, fInt IN_PosZ, fUInt IN_RIndex, fUInt IN_SIndex, std::vector<fVector2ui>& REF_C_Data, fUInt IN_C_Size);
	
	fBool _Internal_CompressChunk(fVoxelChunk& REF_Chunk, std::vector<fVector2ui>& OUT_Data);
	fBool _Internal_DeCompressChunk(fVoxelChunk& REF_Chunk, std::vector<fVector2ui>& REF_Data);
	
	// ----------------------------------------------------------------------------
	// Mesh Stuff
	
	fBool _Internal_GenerateVoxel(fUInt IN_ChunkIndex, fUInt IN_BlockIndex, fVoxelLocalPos IN_Pos, fProcMesh& OUT_Mesh);
	
	// ----------------------------------------------------------------------------
	// Getters
	
	fVector2i _Internal_GetRegion(fInt IN_X, fInt IN_Z);
	fUInt _Internal_GetChunkIndex(fInt IN_X, fInt IN_Z);
	fUInt _Internal_GetRegionIndex(fInt IN_X, fInt IN_Z);
	fUInt _Internal_GetSavedChunkIndex(fVoxelRegionData& REF_Region, fInt IN_X, fInt IN_Z);
	
	fUInt _Internal_GetEmptyChunk();
	
	std::string _Internal_GetRegionHeaderName(fInt IN_X, fInt IN_Z);
	std::string _Internal_GetRegionDataName(fInt IN_X, fInt IN_Z);
	std::string _Internal_GetWorldPropName();
	std::string _Internal_GetRegionPath();
public:
	// -----------------------------------
	fBool CreateWorld(std::string IN_Path);
	
	fUInt SpawnChunk(fInt IN_PosX, fInt IN_PosZ);
	
	fBool SaveChunk(fInt IN_PosX, fInt IN_PosZ);
	fBool UnloadChunk(fInt IN_PosX, fInt IN_PosZ, fBool IN_isSave = true);
	
	fVoxelChunk* GetChunkPtr(fInt IN_PosX, fInt IN_PosZ);
	
	fBool SaveWorld();
	fBool UnloadWorld();
	
	// ----------------------------------
	void Log(fUChar IN_Sev, std::string IN_SenderName, std::string IN_Msg);
	// ----------------------------------
	fBool SetVoxelMesh(std::vector<fProcMesh> IN_MeshList);
	void UseDefaultVoxelMesh();
	void SetTextureSteps(fFloat IN_StepX, fFloat IN_StepY) { TextureStep_X = IN_StepX; TextureStep_Y = IN_StepY; }
	void SetVoxelList(std::vector<fVoxelBlock> IN_BlockList) { VoxelList = IN_BlockList; }
	void SetVoxelSize(fFloat IN_X, fFloat IN_Y, fFloat IN_Z) { VoxelSize_X = IN_X; VoxelSize_Y = IN_Y; VoxelSize_Z = IN_Z; }
	// ----------------------------------
	fVoxelLocalPos GetVoxelLocalPos(fInt IN_GlobalX, fInt IN_GlobalY, fInt IN_GlobalZ);
	fVoxelGlobalPos GetVoxelGlobalPos(fVoxelLocalPos IN_Pos);
	fUInt GetVoxelIndex(fInt IN_X, fInt IN_Y, fInt IN_Z);
	
	fBool GenerateChunkMesh(fInt IN_PosX, fInt IN_PosZ, fProcMesh& OUT_Mesh);
	
	fBool SetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& REF_Voxel);
	fBool GetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& OUT_Voxel);
	fBool ClearVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z);
	// ----------------------------------
	fBool SetChunkVoxelSize(fInt IN_X, fInt IN_Y, fInt IN_Z);
	fBool SetRegionSize(fInt IN_X, fInt IN_Z);
	fBool SetWorldSize(fInt IN_X, fInt IN_Z);
	// ----------------------------------
	void SetMemoryAllocator(fMemoryAllocator IN_Allocator, fMemoryDeAllocator IN_DeAllocator) { Allocator = IN_Allocator; DeAllocator = IN_DeAllocator; }
	void SetLogCallback(fLogCallback IN_LogCallback) { LogFunctionPtr = IN_LogCallback; }
	void SetMinimumLogLevel(fUChar IN_MinSeverity) { MinLogLevel = IN_MinSeverity; }
	// ----------------------------------
	fBool GetisInited() { return isInit; }
	fVector3ui GetChunkSize() { return {ChunkSize_X, ChunkSize_Y, ChunkSize_Z}; }
	fVector3 GetVoxelSize() { return {VoxelSize_X, VoxelSize_Y, VoxelSize_Z}; }
	fProcMesh* GetFoxelMeshArray() { return &FoxelMesh[0]; }
};






