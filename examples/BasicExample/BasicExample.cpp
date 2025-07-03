#include <iostream>
#include <filesystem>
#include <raylib.h>
#include <FastNoiseLite.h>
#include <ctime>
#include "fVoxel.hpp"


unsigned long long FPS_Sum = 0;
unsigned long long FPS_Count = 0;
float FPS_TimeSum = 0.0F;
fUInt FPS_Actual = 0;

Mesh GenerateRayMesh(fProcMesh& REF_ChunkMesh) {
    Mesh Result = {0};
    fUInt VertNum = REF_ChunkMesh.Vertecies.size();

    Result.vertexCount = VertNum;
    Result.triangleCount = Result.vertexCount / 3;
    Result.vertices = (float *)MemAlloc(Result.vertexCount * 3 * sizeof(float));
    Result.texcoords = (float *)MemAlloc(Result.vertexCount * 2 * sizeof(float));
    Result.normals = (float *)MemAlloc(Result.vertexCount * 3 * sizeof(float));

    for (fUInt X = 0; X < VertNum; X++) {
        Result.vertices[(X * 3) + 0] = REF_ChunkMesh.Vertecies[X].X;
        Result.vertices[(X * 3) + 1] = REF_ChunkMesh.Vertecies[X].Y;
        Result.vertices[(X * 3) + 2] = REF_ChunkMesh.Vertecies[X].Z;

        Result.normals[(X * 3) + 0] = REF_ChunkMesh.Normals[X].X;
        Result.normals[(X * 3) + 1] = REF_ChunkMesh.Normals[X].Y;
        Result.normals[(X * 3) + 2] = REF_ChunkMesh.Normals[X].Z;

        Result.texcoords[(X * 2) + 0] = REF_ChunkMesh.UVs[X].X;
        Result.texcoords[(X * 2) + 1] = REF_ChunkMesh.UVs[X].Y;
    }

    UploadMesh(&Result, false);

    return Result;
}
fFloat Lerp(fFloat IN_From, fFloat IN_To, fFloat IN_Noise) {
    // https://stackoverflow.com/questions/4353525/floating-point-linear-interpolation
    // Thomas O - (Formula in the question)
    return (IN_From * (1.0F - IN_Noise)) + (IN_To * IN_Noise);
}

void DrawInfoPanel(Font& REF_Font, Camera3D& REF_Camera, fVoxelChunk* IN_Chunk) {
    const float RectOffset = 20.0F;
    const float TextOffset = RectOffset;
    const float LineHeight = 32.0F;
    const float FontSize = 32.0F;

    // Rectangle to draw Into
    Rectangle R = {
        RectOffset,
        (float)(GetRenderHeight() - 200 - RectOffset),
        (float)GetRenderWidth() - (RectOffset * 2.0F),
        200
    };

    // Draw Rect
    DrawRectangleRounded(R, 0.25F, 8, {128,128,128,128} );

    // Calculate FPS
    float FTime = GetFrameTime();
    if (FPS_TimeSum < 1.0F) {
        FPS_TimeSum += FTime;
        fUInt Curr_FPS = (1.0F / FTime) * 100.0F;
        Curr_FPS /= 100;
        FPS_Sum += FTime;
        FPS_Sum += Curr_FPS;
        FPS_Count++;
    }
    else {
        FPS_Actual = FPS_Sum / FPS_Count;
        FPS_Count = 0;
        FPS_Sum = 0;
        FPS_TimeSum = 0.0F;
    }

    float CurrH = R.y + TextOffset;
    std::string StrA = "fVoxel - BasicExample ( " + std::to_string(FPS_Actual) + " FPS )";
    DrawTextEx(REF_Font, StrA.c_str(), {R.x + TextOffset, CurrH}, FontSize, 1.0F, WHITE);

    CurrH += LineHeight;
    std::string StrB = "VoxelChunk[0,0] => [" + std::to_string(IN_Chunk->VisibleVoxels) + "] Visible Voxels";
    DrawTextEx(REF_Font, StrB.c_str(), {R.x + TextOffset, CurrH}, FontSize, 1.0F, WHITE);

    CurrH += LineHeight;
    std::string StrC = "CameraPos => [X: " + std::to_string(REF_Camera.position.x) + ", Y: " + std::to_string(REF_Camera.position.y) + ", Z: " + std::to_string(REF_Camera.position.z) + "]";
    DrawTextEx(REF_Font, StrC.c_str(), {R.x + TextOffset, CurrH}, FontSize, 1.0F, WHITE);

    CurrH += LineHeight;

    CurrH += LineHeight;
    DrawTextEx(REF_Font, "Control: Move with W-A-S-D; Left Shift = UP; Space = DOWN; Look around with mouse", {R.x + TextOffset, CurrH}, FontSize, 1.0F, WHITE);
}

int main() {
    // Get the absolute path of the current working directory
    std::string RootPath = std::filesystem::current_path();


    // Create an instance of the world
    fVoxelWorld World;

    // Set the world properties
    // These properties needs to be set before world init
    // They cannot be changed afterwards.
    // NOTE: if world is loading, these will be overriten by saved values
    World.SetChunkVoxelSize(256,128,256); // Number of Voxels per chunk
    World.SetRegionSize(33,33); // Number of chunks per region
    World.SetWorldSize(32,32); // Number of Chunks that can simultaneously exist in the world

    // Since in this example, there is no way to modify the world
    // We just re-create it every time with ForceCreate
    if (!World.CreateWorld(RootPath, true)) {
        std::cout << "Failed to Create Voxel World" << std::endl;
        return -1;
    }

    // There are some properties of the world that can be change "on-the-fly"
    World.SetVoxelSize(1.0F,1.0F,1.0F); // Sets the size of a single voxel in world space - used when determining where each voxel mesh needs to be
    World.SetTextureSteps(0.5F, 0.5F); // Sets the texture coordinates offset. Texturing the World works based on texture atlases

    // This will make the world use a simple cube as the voxel mesh
    // This needs to be called after Voxel size has been set as this function
    // Will generate the "cube" using the current voxel size
    // Note: a custom mesh can be used with "fBool SetVoxelMesh(std::vector<fProcMesh> IN_MeshList)"
    World.UseDefaultVoxelMesh();

    // Now that we have a world that is initialised and ready,
    // We still needs to give it some Voxeldefinitions
    // Create 4 example voxel
    std::vector<fVoxelBlock> VList;
    VList.push_back(fVoxelBlock(0,"Block0",{0,0},0));
    VList.push_back(fVoxelBlock(1,"Block1",{1,0},0));
    VList.push_back(fVoxelBlock(2,"Block2",{0,1},0));
    VList.push_back(fVoxelBlock(3,"Block3",{1,1},0));
    World.SetVoxelList(VList);

    // At this point we can start spawning chunks to work with
    // This will create a chunk at position 0,0 in world space
    //  Note: the returned chunk index will be needed for any
    //        further interaction with this chunk
    fUInt ChunkIndex = World.SpawnChunk(0,0);

    // To generate the chunk data, a pointer to the chunk can be obtained:
    // The resulting value can be a "nullptr" indicating that the requested chunk not found
    fVoxelChunk* ChunkPtr = World.GetChunkPtr(ChunkIndex);
    if (ChunkPtr == nullptr) {
        // TODO: handle unable to get chunk data pointer.
        std::cout << "Failed to get ChunkPtr (NULLPTR)" << std::endl;
        return -1;
    }

    // Also get the chunk size for the iteration
    fVector3ui ChunkSize = World.GetChunkSize();

    FastNoiseLite NoiseGen = FastNoiseLite(time(0));
    NoiseGen.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2S);
    float Magic = 0.98798F;
    float HeightSmooth = 0.275F;
    float BlocktSmooth = 0.5F;

    // Now we have access to the chunk data, just iterate each voxel and set it to a random value
    for (fUInt Z = 0; Z < ChunkSize.Z; Z++) {
        for (fUInt X = 0; X < ChunkSize.X; X++) {
            float HNoise = NoiseGen.GetNoise(
                X * HeightSmooth * Magic,
                Z * HeightSmooth * Magic
            );
            HNoise = (HNoise + 1.0F) / 2.0F;
            // Generate a random heigh at this X,Z column
            fUInt CurrH = ChunkSize.Y * HNoise;

            // Use this height to only "spawn" voxels up to
            // Any voxel above this will keeps its default value of F_UINT_MAX indicating it is "non-existing"
            for (fUInt Y = 0; Y < CurrH; Y++) {
                // Get an index for the Voxel we aim to set
                // This can be calculated manually but this function
                // for easy of use and to avoid issues in axis orders
                fUInt Index = ChunkPtr->GetVoxelIndex(X,Y,Z);

                // Set the value of the voxels to a random entry in the FVoxelList (Currently contains 4 elements).
                float BNoise = NoiseGen.GetNoise(
                    X * BlocktSmooth * Magic,
                    Y * BlocktSmooth * Magic,
                    Z * BlocktSmooth * Magic
                );
                BNoise = (BNoise + 1.0F) / 2.0F;
                float ID = Lerp(0.0F, (float)VList.size() - 1 , BNoise);
                ChunkPtr->BlockList[Index] = (fUInt)ID;
            }
        }
    }

    // We generated the voxel for this chunk, we can mark it as such
    // NOTE: at this point this variable is not internally used
    ChunkPtr->isVoxelGenerated = true;

    // Also need to make sure that the updated chunk will be saved
    // Since we get a pointer to the chunk, the engine has no way of knowing
    // if we made any changes so we have to manually mark it as changed
    ChunkPtr->isModified = true;

    // After Voxels generated, we can create a mesh for this chunk
    // mesh generated from the World perspective to enable chunk borders to only
    // render faces if needed based on the surroundings chunks
    fProcMesh ChunkMesh;
    if (!World.GenerateChunkMesh(ChunkIndex, ChunkMesh)) {
        // TODO: Handle unable to generate chunk mesh.
    }

    // -------------------------------------------------------------------------------------
    // RayLib Stuff
    // -------------------------------------------------------------------------------------

    SetTraceLogLevel(LOG_ERROR);
    InitWindow(800,600,"fVoxel - BasicExample");
    ToggleFullscreen();
    DisableCursor();

    Font Fnt = LoadFont("LieraSans-Regular.ttf");

    Texture RayTexture = LoadTexture("Blocks.png");
    SetTextureFilter(RayTexture, TEXTURE_FILTER_TRILINEAR);
    GenTextureMipmaps(&RayTexture);

    Mesh RayMesh = GenerateRayMesh(ChunkMesh);
    Model RayModel = LoadModelFromMesh(RayMesh);
    RayModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = RayTexture;

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 175.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float CmaeraMoveSpeed = 25.0F;

    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
            DrawModel(RayModel, {0.0F,0.0F,0.0F}, 1.0F, WHITE);
            //DrawModelWires(RayModel, {0.0F,0.0F,0.0F}, 1.0F, BLACK);
            DrawGrid(10, 1.0f);
        EndMode3D();

        DrawInfoPanel(Fnt, camera, ChunkPtr);

        EndDrawing();

        float CSpeed = CmaeraMoveSpeed * GetFrameTime();

        if (IsKeyDown(KEY_SPACE)) {
            camera.position.y += CSpeed;
            camera.target.y += CSpeed;
        }
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            camera.position.y -= CSpeed;
            camera.target.y -= CSpeed;
        }
    }

    UnloadFont(Fnt);
    UnloadTexture(RayTexture);
    UnloadModel(RayModel);

    CloseWindow();

    // -------------------------------------------------------------------------------------
    // /RayLib Stuff
    // -------------------------------------------------------------------------------------

    // Clean up world and deallocate any memory currently in use
    // Since no modification possible, we dont neet to save the world
    World.UnloadWorld();
}
