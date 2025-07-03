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
#define F_INT_MIN       INT32_MIN;

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
// fVoxel Internal Defs
// ----------------------------------------------------------------------------------------------------

class fVoxelWorld;

// Stores a single ChunkData Allocation within the region data file
struct fVoxelRegionEntry {
    // Chunk Position X,Z
    fInt PosX = 0;
    fInt PosZ = 0;

    // Offset in Bytes into the data file where to start loading
    fLong Offset = 0;

    // Number of bytes to load
    fLong Size = 0;

    void AppentToBuffer(fUInt* IN_Buffer, fUInt& REF_Pos) {
        IN_Buffer[REF_Pos++] = PosX;
        IN_Buffer[REF_Pos++] = PosZ;
        IN_Buffer[REF_Pos++] = Offset >> 32;
        IN_Buffer[REF_Pos++] = (fUInt)Offset;
        IN_Buffer[REF_Pos++] = Size >> 32;
        IN_Buffer[REF_Pos++] = (fUInt)Size;
    }
    void ReadFromBuffer(fUInt* IN_Buffer, fUInt& REF_Pos) {
        PosX = IN_Buffer[REF_Pos++];
        PosZ = IN_Buffer[REF_Pos++];
        Offset = (fLong)IN_Buffer[REF_Pos++] << 32;
        Offset |= IN_Buffer[REF_Pos++];
        Size = (fLong)IN_Buffer[REF_Pos++] << 32;
        Size |= IN_Buffer[REF_Pos++];
    }
};

// Represents a whole Region
class fVoxelRegionData {
private:
    fVoxelWorld* WorldPtr = nullptr;
public:
    // Region Position X,Z
    fInt RX = 0;
    fInt RZ = 0;

    // Number of Bytes Currently saved into the Data File beloging to this region
    fLong EOF_Offset = 0;

    // List of individual "Chunk Data"s saved
    std::vector<fVoxelRegionEntry> EntryList;

    fVoxelRegionData(fVoxelWorld* IN_WorldPtr);

    fBool LoadHeader();
    fBool SaveHeader();

    // Saves a new Entry into header and Data files
    //      @ REF_Entry - Entry To be added
    //      @ IN_DataPtr - Pointer for data to be saved
    //      @ IN_DataSize - Size in bytes of data
    // return the EntryList for the new entry or F_UINT_MAX on failure
    fUInt SaveNewEntry(fVoxelRegionEntry& REF_Entry, fUChar* IN_DataPtr, fUInt IN_DataSize);

    // Overrides an existing Entry's data
    //      @ IN_EntryIndex - Index of entry to be overriten
    //      @ REF_Entry - New Entry for that Index
    //      @ IN_DataPtr - New Data Pointer
    //      @ IN_DataSize - Size in bytes of new data
    fBool OverrideEntry(fUInt IN_EntryIndex, fVoxelRegionEntry& REF_Entry, fUChar* IN_DataPtr, fUInt IN_DataSize);

    // Loads the data for an Entry
    //      @ IN_EntryIndex - Index of Entry to be loaded
    //      @ OUT_DataPtr - Pointer to an allocations where data to be loaded
    //                      NOTE: there is no check on size, LoadEntry assumes OUT_DataPtr has enough space
    fBool LoadEntry(fUInt IN_EntryIndex, fUChar* OUT_DataPtr);

    // Return the Index into EntryList for a Given Chunk Position X,Z or F_UINT_MAX if no such Entry found
    fUInt GetChunkEntryIndex(fInt IN_PosX, fInt IN_PosZ);
};


// Represents a single Chunk in the World
class fVoxelChunk {
protected:
    // Compress Chunk data (BlockList) into a pair of {Count,ID}
    fBool _Internal_CompressData(std::vector<fVector2ui>& REF_Data);

    // Populates Chunk Data (BlockList) from a list of pairs {Count,ID}
    fBool _Internal_DeCompressData(std::vector<fVector2ui>& REF_Data);

    fBool _Internal_Validate(std::string IN_What);
public:
    // World this chuk belongs to
    fVoxelWorld* WorldPtr = nullptr;

    // Region pointer this chunk belongs to
    fVoxelRegionData* RegionPtr = nullptr;

    // Region Entry Index for this Chunk
    // F_UINT_MAX means not yet saved
    fUInt RegionEntryIndex = F_UINT_MAX;

    // Chunk Position in Global Space
    fInt PosX = F_INT_MIN;
    fInt PosZ = F_INT_MIN;

    // Blocklist (Each element is an index / UID of that block)
    fUInt* BlockList = nullptr;

    // Flags
    fBool isExist = false;
    fBool isModified = false;
    fBool isAllocated = false;
    fBool isVoxelGenerated = false;
    fBool isMeshGenerated = false;

    fVoxelChunk(fVoxelWorld* IN_WorldPtr) { WorldPtr = IN_WorldPtr; }

    // Utility info mainly for debuging
    fUInt VisibleVoxels = 0;    // Number of voxel with any mesh generated - Populated from "_Internal_GenerateVoxel()"

    // Return the Index into BlockList for the specified Voxel
    fUInt GetVoxelIndex(fUInt IN_X, fUInt IN_Y, fUInt IN_Z);

    // Save Data into Region Data File
    fBool SaveChunkData();

    // Load Data From Region Data File
    fBool LoadChunkData();
};


// Represents a single "Voxel"
// Most of its members not currently used internally
struct fVoxelBlock {
    fUInt UID = 0;
    std::string Name = "";
    fVector2ui Texture = {0,0};
    fUChar Flags = 0b00000000;

    fVoxelBlock(fUInt IN_UID = 0, std::string IN_Name = "", fVector2ui IN_Texture = {0,0}, fUChar IN_Flags = 0b00000000);
};

// Position of a single Vooxel in Within its chunks
struct fVoxelLocalPos {
    fInt ChunkX = 0;
    fInt ChunkZ = 0;
    fInt LocalX = 0;
    fInt LocalY = 0;
    fInt LocalZ = 0;

    std::string ToString();
};

// Position of a Single Voxel within the World
struct fVoxelGlobalPos {
    fInt GlobalX = 0;
    fInt GlobalY = 0;
    fInt GlobalZ = 0;
};


// ----------------------------------------------------------------------------------------------------
// External Interface Class - Main Class
// ----------------------------------------------------------------------------------------------------

// Represent the Whole Voxel World
// This mean to be the only class interacted with from "Outside"
class fVoxelWorld {
private:
    // Total number of Chunks exist in memory at any one time
    // Calculated as "WorldSize_X*WorldSize_Z" in "_Internal_Init()"
    fLong ChunksPerWorld = 0;

    // Total number of Blocks per chunks
    // Calculated as "ChunkSize_X*ChunkSize_Y*ChunkSize_Z" in "_Internal_Init()"
    fLong BlocksPerChunk = 0;

    // Number of vertecies in each SubMesh
    // To avoid counting them for each voxel when mesh generation
    // Populated from "_Internal_CalculateTempVerts()"
    fUInt TempVertNum_Front = 0;
    fUInt TempVertNum_Back = 0;
    fUInt TempVertNum_Left = 0;
    fUInt TempVertNum_Right = 0;
    fUInt TempVertNum_Top = 0;
    fUInt TempVertNum_Bottom = 0;
    fUInt TempVertNum_Always = 0;
protected:
    // ----------------------------------------------------------------------------
    // World Configuration - Cannot be Change After "Create" or "Load" world
    // Either set by used before "CreateWorld()"
    // Or Loaded during "LoadWorld()"

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

    // name of the folder where all World Data Will be saved
    std::string WorldFolderName = "World";

    // Name of the Subfolder in "WorldFolderName" where region data will be saved
    std::string RegionFolderName = "Regions";

    // Name of the file within "WorldFolderName" where World Properties are saved
    std::string WolrdFileName = "fVoxel";

    // Name of the file within "RegionFolderName" for each region where their headers are saved
    // Actual FileName = (RegionHeaderName)_X_Z - where X and Z corresponds to the region Position
    // Default result for region {0,0} => RegionHeader_0_0
    std::string RegionHeaderName = "RegionHeader";

    // Name of the file within "RegionFolderName" for each region where their data are saved
    // Actual FileName = (RegionDataName)_X_Z - where X and Z corresponds to the region Position
    // Default result for region {0,0} => RegionData_0_0
    std::string RegionDataName = "RegionData";

    // ----------------------------------------------------------------------------
    // Thread Stuff

    std::mutex IO_Lock;
    std::mutex Chunk_Lock;
    std::mutex Region_Lock;

    // ----------------------------------------------------------------------------
    // World Properties - Can be changed on the fly

    // Size of Each voxel
    fFloat VoxelSize_X = 1.0F;
    fFloat VoxelSize_Y = 1.0F;
    fFloat VoxelSize_Z = 1.0F;

    // Voxel texture expected to be an atlas (grid of X*Y number of textures)
    // This sets the "Step" on each axis
    fFloat TextureStep_X = 1.0F;
    fFloat TextureStep_Y = 1.0F;

    // Indicates if world is initialised or not
    // Once true "World Configuration" can not be changed
    fBool isInit = false;

    // Defines the mesh to render for each Voxel
    // 0 - Front	Z-
    // 1 - Back		Z+
    // 2 - Left		X+
    // 3 - Right	X-
    // 4 - Top		Y+
    // 5 - Bottom	Y-
    // 6 - Always Visible
    fProcMesh VoxelMesh[7];

    std::vector<fVoxelBlock> VoxelList;

    // Root folder for save Data
    //	- Expected to be an absolute path
    std::string SavePath = "";

    // List of Chunks Currentl "Present" in memory
    std::vector<fVoxelChunk> ChunkList;

    // List of Regions Currentl "Present" in memory
    std::vector<fVoxelRegionData> RegionList;

    // ----------------------------------------------------------------------------
    // I/O Related Stuff

    // Self Explenatory
    fBool IO_isFileExist(std::string IN_FileName);

    // Self Explenatory
    fBool IO_CreateEmptyFile(std::string IN_FileName);

    // Saves content of IN_DataPtr into a Binary File
    //      @ IN_FileName - Absolute path to File to be saved
    //      @ IN_DataPtr - Data To be saved
    //      @ IN_DataSize - Number of Bytes to be saved
    fBool IO_SaveBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize);

    // Appends data to a binary file
    //      @ IN_FileName - Absolute path to File to be saved
    //      @ IN_DataPtr - Data To be saved
    //      @ IN_DataSize - Number of Bytes to be saved
    //      @ IN_Offset - Number of Bytes to "Skip" before start writing into File - if == F_LONG_MAX data will be appended at the end of the file
    fBool IO_AppendBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset = F_LONG_MAX);

    // Load Entire file into "IN_DataPtr"
    //      @ IN_FileName - Absolute Path to file to be loaded
    //      @ IN_DataPtr - Pointer to allocated memory where data will be loaded
    //      @ IN_DataSize - Number of bytes to Load
    //      @ IN_Offset - Number of Bytes to "Skip" at the begining of the File - If == F_LONG_MAX Loading start from begining of the file => No Skip
    fBool IO_LoadBinaryData(std::string IN_FileName, fUChar* IN_DataPtr, fLong IN_DataSize, fLong IN_Offset = F_LONG_MAX);

    // Load Binary File Into (*OUT_DataPtr)
    //      @ IN_FileName - Absolute path For file to be loaded
    //      @ OUT_DataPtr - When function return true, Pointer to an allocated memory where data is loaded (Allocated internally by IO_LoadBinaryData), NULLPTR otherwise
    //      @ OUT_BufferSize - When function return true, will holds the number of bytes loaded,  0 otherwise
    fBool IO_LoadBinaryData(std::string IN_FileName, fUChar** OUT_DataPtr, fLong& OUT_BufferSize);

    // Load Binary File Into 2 separate Buffer
    // First Buffer (before) will Hold the content of the File Up To IN_Offset
    // Second Buffer (after) will hold the content of the File After (IN_Offset + IN_Size)
    // The region covered by (from IN_Offset to IN_Offset + IN_Size) will not be loaded
    //      @ IN_FileName - Absolute path to File to be loaded
    //      @ IN_Offset - Offset into File in bytes == Size of Before Buffer
    //      @ IN_Size - Size to between Before and After Buffer in Bytes (Number of butes to "Skip")
    //      @ OUT_Before - Pointer to allocated memory where "Before" Buffer is loaded, or NULLPTR on failure
    //      @ OUT_After - Pointer to allocated memory where "After" Buffer is loaded, or NULLPTR on failure
    //      @ OUT_BeforeSize - Size of Before Buffer in Bytes, or 0 on failure
    //      @ OUT_AfterSize - Size of Before Buffer in Bytes, or 0 on failure
    fBool IO_LoadBinaryData(std::string IN_FileName, fLong IN_Offset, fLong IN_Size, fUChar** OUT_Before, fUChar** OUT_After, fLong& OUT_BeforeSize, fLong& OUT_AfterSize);

    // ----------------------------------------------------------------------------
    // Log Related Stuff

    // Function pointer to be called from "Log()"
    // if this is NULLPTR, "Log()" will execute, otherwise "Log_FunctionPtr()" will be called instead
    fLogCallback Log_FunctionPtr = nullptr;

    // Min level to log
    fUChar Log_MinLevel = 0;

    // Converts IN_Sev into ascii text
    std::string Log_Sev_To_String(fUChar IN_Sev);

    // Sets the function pointer to be called for logging
    void SetLogCallbackPtr(fLogCallback IN_Fnc);

    // Main Log Function
    void Log(fUChar IN_Sev, std::string IN_SenderName, std::string IN_Msg);

    // ----------------------------------------------------------------------------
    // Memory Stuff

    // Function pointer to be called for memory allocation (default - malloc)
    fMemoryAllocator Allocator = &DefaultMemAllocator;

    // Function pointer to be called for free memory (default - free)
    fMemoryDeAllocator DeAllocator = &DefaultMemDeAllocator;

    // Default Allocation / Deallocation function to be used If not set by user
    static void* DefaultMemAllocator(size_t IN_Bytes) { return malloc(IN_Bytes); }
    static void DefaultMemDeAllocator(void* IN_Ptr) { free(IN_Ptr); }

    // ----------------------------------------------------------------------------
    // Util Functions

    // Return an absolute path to the file holding the world properties
    std::string GetWorldFile();
    // Return an absolute path to the file holding the header for the specified region
    std::string GetRegionHeaderFile(fInt IN_RX, fInt IN_RZ);
    // Return an absolute path to the file holding the data for the specified region
    std::string GetRegionDataFile(fInt IN_RX, fInt IN_RZ);

    // ----------------------------------------------------------------------------
    // Internal Init Functions

    // Initialise World based on world properties
    // Allocates memory for chunks and BlockLists
    void _Internal_Init();

    // Calculates the Number of Vertecies in FoxelMesh array
    void _Internal_CalculateTempVerts();

    // Save World Properties - must be called AFTER _Internal_Init
    fBool _Internal_SaveWorldProp();

    // Load World Properties - must be called BEFORE _Internal_Init
    fBool _Internal_LoadWorldProp(std::string IN_FileName);

    // ----------------------------------------------------------------------------
    // Internal Region Related Functions

    // Get the region position based on the X,Z of the chunk
    fVector2i _Internal_GetRegionPos(fInt IN_PosX, fInt IN_PosZ);

    // Get the Index of the Region based on its Position or F_UINT_MAX if no such region loaded yet
    fUInt _Internal_GetRegionIndex(fInt IN_PosX, fInt IN_PosZ);

    // Creates a new region based on X,Z position. Return new Region index of F_UINT_MAX on failure.
    fUInt _Internal_CreateRegion(fInt IN_PosX, fInt IN_PosZ);

    // ----------------------------------------------------------------------------
    // Internal Chunk Related Functions

    // Return the Index for chunk at position X,Z if found, F_UINT_MAX otherwise
    fUInt _Internal_GetChunkIndex(fInt IN_X, fInt IN_Z);

    // Return the index for the first empty chunk, or F_UINT_MAX if no such chunk found
    fUInt _Internal_GetEmptyChunk();

    // ----------------------------------------------------------------------------
    // Mesh Stuff

    fBool _Internal_GenerateVoxel(fUInt IN_ChunkIndex, fUInt IN_BlockIndex, fVoxelLocalPos IN_Pos, fProcMesh& OUT_Mesh);
public:
    // -----------------------------------

    // Creates a new Voxel World
    //      @ IN_FolderPath - Absolute path to the folder where the world data will be saved
    //      @ IN_isForceCreate - If set to true, will check and remove the target folder if already exist
    fBool CreateWorld(std::string IN_FolderPath, fBool IN_isForceCreate = false);

    // Loads an existing Voxel Wolrd
    //      @ IN_FilePath - Absolute path to the Voxel World file to load from
    fBool LoadWorld(std::string IN_FilePath);

    // Check whether the given world is exist or not
    fBool isWorldExist(std::string IN_FilePath);

    // Creates or load a chunk at the specified location
    //      @ IN_PosX - Global Chunk Position X
    //      @ IN_PosZ - Global Chunk Position Z
    fUInt SpawnChunk(fInt IN_PosX, fInt IN_PosZ);

    // See if Specified Chunk has been modified or not and saves change if it has been
    fBool SaveChunk(fUInt IN_ChunkIndex);

    // Unloads the chunk data from memory and marks chunk as non existing
    fBool UnloadChunk(fUInt IN_ChunkIndex, fBool IN_isSave = true);

    // Return a pointer to the specified chunk or NULLPTR if invalid Index
    fVoxelChunk* GetChunkPtr(fUInt IN_ChunkIndex);

    // Check all existing Chunks and saved them if modified
    fBool SaveWorld();

    // Unloads all chunks and Wolrd
    fBool UnloadWorld();


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

    fBool GenerateChunkMesh(fUInt IN_ChunkIndex, fProcMesh& OUT_Mesh);

    fBool SetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& REF_Voxel);
    fBool GetVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z, fVoxelBlock& OUT_Voxel);
    fBool ClearVoxel(fInt IN_X, fInt IN_Y, fInt IN_Z);
    // ----------------------------------
    fBool SetChunkVoxelSize(fInt IN_X, fInt IN_Y, fInt IN_Z);
    fBool SetRegionSize(fInt IN_X, fInt IN_Z);
    fBool SetWorldSize(fInt IN_X, fInt IN_Z);
    // ----------------------------------
    void SetMemoryAllocator(fMemoryAllocator IN_Allocator, fMemoryDeAllocator IN_DeAllocator) { Allocator = IN_Allocator; DeAllocator = IN_DeAllocator; }
    void SetLogCallback(fLogCallback IN_LogCallback) { Log_FunctionPtr = IN_LogCallback; }
    void SetMinimumLogLevel(fUChar IN_MinSeverity) { Log_MinLevel = IN_MinSeverity; }
    // ----------------------------------
    fBool GetisInited() { return isInit; }
    fVector3ui GetChunkSize() { return {ChunkSize_X, ChunkSize_Y, ChunkSize_Z}; }
    fVector3 GetVoxelSize() { return {VoxelSize_X, VoxelSize_Y, VoxelSize_Z}; }
    fProcMesh* GetFoxelMeshArray() { return &VoxelMesh[0]; }

    // ----------------------------------
    // Getters
    fLong Get_ChunksPerWorld() { return ChunksPerWorld; }
    fLong Get_BlocksPerChunk() { return BlocksPerChunk; }

    // ----------------------------------
    // Give access to IO / Log funtions
    friend class fVoxelChunk;
    friend class fVoxelRegionData;
};

