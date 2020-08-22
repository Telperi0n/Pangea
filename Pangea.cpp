#include "raylib.h"
#include "rlgl.h"
#include <vector>
#include <cmath>
#include <string>
#include <cstring>
#include "raymath.h"
#include "float.h"




enum class BrushTool
{
    NONE,
    ELEVATION,
    SMOOTH,
    FLATTEN,
    STAMP,
    SELECT
};

enum class Panel
{
    NONE,
    HEIGHTMAP,
    CAMERA,
    TOOLS
};

enum class InputFocus // markers for which the input box is currently in focus
{
    NONE,
    X_MESH,
    Z_MESH,
    X_MESH_SELECT,
    Z_MESH_SELECT,
    STAMP_ANGLE,
    STAMP_HEIGHT,
    SELECT_RADIUS,
    TOOL_STRENGTH, 
    SAVE_MESH,
    LOAD_MESH,
    SAVE_MESH_HEIGHT,
    LOAD_MESH_HEIGHT,
    DIRECTORY
};

// Camera move modes (first person and third person cameras)
typedef enum { 
    MOVE_FRONT = 0, 
    MOVE_BACK, 
    MOVE_RIGHT, 
    MOVE_LEFT, 
    MOVE_UP, 
    MOVE_DOWN 
} CameraMove;

struct ModelVertex;

struct VertexState
{
    Vector2 coords; // coords of the vertex's model
    int vertexIndex; // which in Mesh::vertices cooresponds to the x value
    float y;
    
    friend bool operator== (const VertexState &vs1, const VertexState &vs2);
    friend bool operator!= (const VertexState &vs1, const VertexState &vs2);
    
    friend bool operator== (const VertexState &vs, const ModelVertex &mv);
    friend bool operator!= (const ModelVertex &mv, const VertexState &vs);
    friend bool operator== (const VertexState &vs, const ModelVertex &mv);
    friend bool operator!= (const ModelVertex &mv, const VertexState &vs);
};

struct ModelVertex // saves the index of the vertex in the model's mesh as well as the models coordinates 
{
    Vector2 coords;
    int index;
    
    friend bool operator== (const ModelVertex &mv1, const ModelVertex &mv2);
    friend bool operator!= (const ModelVertex &mv1, const ModelVertex &mv2);
    
    friend bool operator== (const ModelVertex &mv, const VertexState &vs);
    friend bool operator!= (const VertexState &vs, const ModelVertex &mv);
    friend bool operator== (const ModelVertex &mv, const VertexState &vs);
    friend bool operator!= (const VertexState &vs, const ModelVertex &mv);
};

struct ModelSelection
{
    Vector2 topLeft; // coordinates of the top left model in the selection
    Vector2 bottomRight; // coordinates of the bottom right model in the selection
    
    int width; // width of the selection
    int height; // height of the selection
    
    std::vector<Vector2> selection; // a list of the coordinates of the selected models
    std::vector<Vector2> expandedSelection; // selection plus adjacent models
};

float xzDistance(const Vector2 p1, const Vector2 p2); // get the distance between two points on the x and z plane

void HistoryUpdate(std::vector<std::vector<VertexState>>& history, const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, int& stepIndex, const int maxSteps);

Color* GenHeightmapSelection(const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, const int modelVertexWidth, const int modelVertexHeight); // memory should be freed. needs a scale param. generates a heightmap from a selection of models

Color* GenHeightmap(const Model& model, const int modelVertexWidth, const int modelVertexHeight, float& highestY, float& lowestY, bool grayscale); // memory should be freed. generates a heightmap for a single model. used for the model texture, cuts last row and column so pixels and polys are 1:1. will update global highest and lowest Y

Color* GenHeightmap(const std::vector<std::vector<Model>>& models, const int modelVertexWidth, const int modelVertexHeight, float maxHeight, float minHeight); // memory should be freed. generates a heightmap from the whole map. used for export

RayHitInfo GetCollisionRayModel2(Ray ray, const Model *model); // having a copy of GetCollisionRayModel increases performance for some reason

void UpdateHeightmap(const Model& model, const int modelVertexWidth, const int modelVertexHeight, float& highestY, float& lowestY);

void UpdateHeightmap(const std::vector<std::vector<Model>>& models, const int modelVertexWidth, const int modelVertexHeight, float highestY, float lowestY);

std::vector<int> GetVertexIndices(const int x, const int y, const int width); // get the Indices (x value) of all vertices at a particular location in the square mesh. x y start at 0 and read left right, top down

Vector2 GetVertexCoords(const int index, const int width); // get the coordindates of a vertex on a square mesh given its index. x y start at 0

std::vector<Vector2> GetModelCoordsSelection(const std::vector<VertexState>& vsList); // get the coords of each unique model in a list of VertexState

void ModelStitch(std::vector<std::vector<Model>>& models, const ModelSelection &oldModelSelection, const int modelVertexWidth, const int modelVertexHeight); // merges the overlapping vertices on the edges of adjacent models (no longer used)

void SetExSelection(ModelSelection& modelSelection, const int canvasWidth, const int canvasHeight); // populates a model selection's expanded selection, which is selection plus the adjacent models

void FillTerrainCells(ModelSelection& modelSelection, const std::vector<std::vector<Model>>& models); // for model selections being used to track player collision. takes selection[0] and attempts to fill selection with the indices of the surrounding models 

void UpdateCameraCustom(Camera* camera, const std::vector<std::vector<Model>>& models, ModelSelection& terrainCells); // custom update camera function for custom camera



// internal variables from camera.h for custom camera to use
static Vector2 cameraAngle = { 0.0f, 0.0f };          // Camera angle in plane XZ
static float cameraTargetDistance = 0.0f;             // Camera distance from position to target 
static float playerEyesHeight = 0.185f;              // Default player eyes position from ground
static int cameraMoveControl[6]  = { 'W', 'S', 'D', 'A', 'E', 'Q' };

// Camera mouse movement sensitivity
#define CAMERA_MOUSE_MOVE_SENSITIVITY                   0.003f
#define CAMERA_MOUSE_SCROLL_SENSITIVITY                 1.5f
// FIRST_PERSON
#define CAMERA_FIRST_PERSON_FOCUS_DISTANCE              25.0f
#define CAMERA_FIRST_PERSON_MIN_CLAMP                   85.0f
#define CAMERA_FIRST_PERSON_MAX_CLAMP                  -85.0f

// PLAYER (used by camera)
#define PLAYER_MOVEMENT_SENSITIVITY                     65.0f




int main()
{
    const int windowWidth = 1270;
    const int windowHeight = 720;
    const int maxSteps = 5;   // number of changes to keep track of for the history
    const int modelVertexWidth = 120; // 360x360 aprox max before fps <60 with raycollision
    const int modelVertexHeight = 120;  
    const int modelWidth = 12;
    const int modelHeight = 12;
    int stepIndex = 0;
    int canvasWidth = 0; // in number of models
    int canvasHeight = 0; // in number of models
    float timeCounter = 0;
    float selectRadius = 1.5f;
    float toolStrength = 0.1f; // .02
    float highestY = 0.0f; // highest y value on the mesh
    float lowestY = 0.0f; // lowest y value on the mesh
    float stampAngle = 45.0f;
    float stampHeight = 0;
    bool updateFlag = false;
    bool selectionMask = false;
    bool historyFlag = false; // true if the last entry made to history was not made by an undo operation (so undo can save state without making possible copies)
    bool characterDrag = false; // true when the character camera placement is being held
    bool characterMode = false; // if camera is in character mode
    bool rayCollision2d = true; // if true, ray collision will be tested against the ground plane instead of the actual mesh 
    bool raiseOnly = true; // if true, brush will check if the vertex is higher than what it's attempting to change it to and if so, not alter it
    bool showSaveWindow = false; // whether or not to display the save window with the export options
    bool showLoadWindow = false; // true if the load window should be shown
    bool showDirWindow = false; // if directory change window is shown
    
    std::vector<std::vector<VertexState>> history;
    std::vector<VertexState> vertexSelection;
    std::vector<std::vector<Model>> models;      // 2d vector of all models
    
    std::string xMeshString; // models on the x axis
    std::string zMeshString; // models on the z axis
    std::string xMeshSelectString; // width of the selection in models 
    std::string zMeshSelectString; // height of the selection in models 
    std::string stampAngleString; // angle of the stamp tool 
    std::string stampHeightString; // max height of the stamp tool 
    std::string toolStrengthString; // tool strength  
    std::string selectRadiusString; // selection radius 
    std::string saveMeshString; // file name of the saved project
    std::string saveHeightString; // the intended max height of the mesh. used to scale the heightmap. defaults to the current highest point
    std::string loadMeshString; // file name of the project to load
    std::string loadHeightString; // the height the value 255 represents in the image being loaded. used to scale the heightmap
    std::string directoryString; // the desired directory
    
    ModelSelection modelSelection; // the models currently being worked on by their index in models
    modelSelection.height = 0;
    modelSelection.width = 0;
    
    ModelSelection terrainCells; // the models surrounding the player in character mode
    
    Ray ray = {0};
    
    // ANCHORS
    Vector2 meshSelectAnchor = {0, 66}; // location to which all mesh selection elements are relative
    Vector2 toolButtonAnchor = {0, 245}; // location to which all tool elements are relative
    Vector2 saveWindowAnchor = {windowWidth / 2 - 150, windowHeight / 2 - 75};
    Vector2 loadWindowAnchor = {windowWidth / 2 - 150, windowHeight / 2 - 75};
    Vector2 dirWindowAnchor = {windowWidth / 2 - 250, windowHeight / 2 - 75};
    
    Rectangle UI = {0, 0, 101, 700};
    Rectangle heightmapTab = {0, 0, 101, 20};
    Rectangle panel1 = {0, 20, 101, 3};
    Rectangle cameraTab = {0, 23, 101, 20};
    Rectangle panel2 = {0, 43, 101, 3};
    Rectangle toolsTab = {0, 46, 101, 20};
    Rectangle panel3 = {0, 66, 101, 637};
    
    // SAVE WINDOW
    Rectangle saveWindow = {saveWindowAnchor.x, saveWindowAnchor.y, 300, 185};
    Rectangle saveWindowSaveButton = {saveWindowAnchor.x + 60, saveWindowAnchor.y + 155, 60, 20};
    Rectangle saveWindowCancelButton = {saveWindowAnchor.x + 180, saveWindowAnchor.y + 155, 60, 20};
    Rectangle saveWindowTextBox = {saveWindowAnchor.x + 30, saveWindowAnchor.y + 40, 240, 30};
    Rectangle saveWindowHeightBox = {saveWindowAnchor.x + 30, saveWindowAnchor.y + 115, 240, 30};
    
    // LOAD WINDOW
    Rectangle loadWindow = {loadWindowAnchor.x, loadWindowAnchor.y, 300, 185};
    Rectangle loadWindowLoadButton = {loadWindowAnchor.x + 60, loadWindowAnchor.y + 155, 60, 20};
    Rectangle loadWindowCancelButton = {loadWindowAnchor.x + 180, loadWindowAnchor.y + 155, 60, 20};
    Rectangle loadWindowTextBox = {loadWindowAnchor.x + 30, loadWindowAnchor.y + 40, 240, 30};
    Rectangle loadWindowHeightBox = {loadWindowAnchor.x + 30, loadWindowAnchor.y + 115, 240, 30};
    
    // DIRECTORY WINDOW
    Rectangle dirWindow = {dirWindowAnchor.x, dirWindowAnchor.y, 500, 120};
    Rectangle dirWindowOkayButton = {dirWindowAnchor.x + 160, dirWindowAnchor.y + 85, 60, 20};
    Rectangle dirWindowCancelButton = {dirWindowAnchor.x + 280, dirWindowAnchor.y + 85, 60, 20};
    Rectangle dirWindowTextBox = {dirWindowAnchor.x + 30, dirWindowAnchor.y + 40, 440, 30};
    
    // MESH PANEL
    Rectangle exportButton = {10, 557, 80, 40};
    Rectangle meshGenButton = {10, 80, 80, 40};
    Rectangle xMeshBox = {35, 30, 55, 20};
    Rectangle zMeshBox = {35, 55, 55, 20};
    Rectangle updateTextureButton = {10, 457, 80, 40};
    Rectangle loadButton = {10, 507, 80, 40};
    Rectangle directoryButton = {10, 607, 80, 40};
    
    // CAMERA PANEL
    Rectangle characterButton = {62, 55, 33, 33};
    
    // TOOL PANEL
    Rectangle elevToolButton = {toolButtonAnchor.x + 16, toolButtonAnchor.y + 35, 25, 25};
    Rectangle smoothToolButton = {toolButtonAnchor.x + 59, toolButtonAnchor.y + 35, 25, 25};
    Rectangle flattenToolButton = {toolButtonAnchor.x + 16, toolButtonAnchor.y + 75, 25, 25};
    Rectangle stampToolButton = {toolButtonAnchor.x + 59, toolButtonAnchor.y + 75, 25, 25};
    Rectangle selectToolButton = {toolButtonAnchor.x + 16, toolButtonAnchor.y + 115, 25, 25};
    Rectangle toolText = {toolButtonAnchor.x + 10, toolButtonAnchor.y, 80, 20};
    Rectangle deselectButton = {toolButtonAnchor.x + 10, toolButtonAnchor.y + 160, 80, 20};
    Rectangle trailToolButton = {toolButtonAnchor.x + 10, toolButtonAnchor.y + 186, 80, 20};
    Rectangle selectionMaskButton = {toolButtonAnchor.x + 10, toolButtonAnchor.y + 212, 80, 20};
    Rectangle xMeshSelectBox = {meshSelectAnchor.x + 20, meshSelectAnchor.y + 11, 25, 20};
    Rectangle zMeshSelectBox = {meshSelectAnchor.x + 69, meshSelectAnchor.y + 11, 25, 20};
    Rectangle meshSelectButton = {meshSelectAnchor.x + 10, meshSelectAnchor.y + 37, 80, 20};
    Rectangle meshSelectUpButton = {meshSelectAnchor.x + 40, meshSelectAnchor.y + 64, 21, 21};
    Rectangle meshSelectLeftButton = {meshSelectAnchor.x + 16, meshSelectAnchor.y + 75, 21, 21};
    Rectangle meshSelectRightButton = {meshSelectAnchor.x + 64, meshSelectAnchor.y + 75, 21, 21};
    Rectangle meshSelectDownButton = {meshSelectAnchor.x + 40, meshSelectAnchor.y + 88, 21, 21};
    Rectangle stampAngleBox = {toolButtonAnchor.x + 10, toolButtonAnchor.y + 160, 30, 20};
    Rectangle stampHeightBox = {toolButtonAnchor.x + 56, toolButtonAnchor.y + 160, 30, 20};
    Rectangle selectRadiusBox = {toolButtonAnchor.x + 60, toolButtonAnchor.y - 60, 30, 20};
    Rectangle toolStrengthBox = {toolButtonAnchor.x + 60, toolButtonAnchor.y - 35, 30, 20};
    
    
    BrushTool brush = BrushTool::NONE;
    Panel panel = Panel::NONE;
    InputFocus inputFocus = InputFocus::NONE;
    
    InitWindow(windowWidth, windowHeight, "Pangea");
    
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.type = CAMERA_PERSPECTIVE;                   // Camera mode type
  
    SetCameraMode(camera, CAMERA_FREE); // Set a free camera mode
    
    //ChangeDirectory("C:/Users/msgs4/Desktop/Pangea");
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        if (characterMode)
        {
            UpdateCameraCustom(&camera, models, terrainCells);
            
            if (IsKeyPressed(KEY_TAB)) // exit character mode
            {
                SetCameraMode(camera, CAMERA_FREE);
                camera.fovy = 45.0f;
                characterMode = false;
                terrainCells.selection.clear();
                EnableCursor();
            }
            
            BeginDrawing();
            
                ClearBackground(Color{240, 240, 240, 255});
                
                BeginMode3D(camera);
                
                    if (!models.empty())
                    {
                        for (int i = 0; i < models.size(); i++)
                        {
                            for (int j = 0; j < models[i].size(); j++)
                            {
                                DrawModel(models[i][j], Vector3{0, 0, 0}, 1.0f, WHITE);
                            }
                        }
                    }
                    
                    DrawGrid(100, 1.0f);

                EndMode3D();
                
            EndDrawing();
        }
        else
        {
            UpdateCamera(&camera);
            
            RayHitInfo hitPosition;
            hitPosition.hit = false;
            
            std::vector<Vector3> displayedVertices;   // remove, using just vertexIndices?
            std::vector<ModelVertex> vertexIndices;
            
            Vector2 hitCoords;
            
            Vector2 mousePosition = GetMousePosition();
            bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            
            if (mousePressed)
            {
                // if the mouse is pressed not over an input box, then the input focus should be none. always clear it and allow later code to set back to the correct focus if the cursor was actually over an input box
                inputFocus = InputFocus::NONE;
            }
            
            if (showSaveWindow)
            {
                if (CheckCollisionPointRec(mousePosition, saveWindowTextBox) && mousePressed)
                {
                    inputFocus = InputFocus::SAVE_MESH;
                }
                else if (CheckCollisionPointRec(mousePosition, saveWindowHeightBox) && mousePressed)
                {
                    inputFocus = InputFocus::SAVE_MESH_HEIGHT;
                }
                else if (CheckCollisionPointRec(mousePosition, saveWindowSaveButton) && mousePressed) // save mesh button
                {
                    std::string saveName = saveMeshString + ".png";
                    char text[saveName.size() + 1];
                    strcpy(text, saveName.c_str()); 
                    
                    Color* pixels;
                    
                    if (saveHeightString.empty())
                    {
                        pixels = GenHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY);
                    }
                    else
                    {
                        float maxHeight = std::stof(saveHeightString);
                        
                        pixels = GenHeightmap(models, modelVertexWidth, modelVertexHeight, maxHeight, lowestY);
                    }
                    
                    Image image = LoadImageEx(pixels, canvasWidth * modelVertexWidth - (canvasWidth - 1), canvasHeight * modelVertexHeight - (canvasHeight - 1));
                    ExportImage(image, text);
                    RL_FREE(pixels);
                    UnloadImage(image);
                    
                    if (FileExists(text)) // check if the file saved succesfully, and if so close the save window
                    {
                        saveHeightString.clear();
                        inputFocus = InputFocus::NONE;
                        showSaveWindow = false;
                    }
                }
                else if (CheckCollisionPointRec(mousePosition, saveWindowCancelButton) && mousePressed)
                {
                    saveHeightString.clear();
                    inputFocus = InputFocus::NONE;
                    showSaveWindow = false;
                }
            }
            else if (showLoadWindow)
            {
                if (CheckCollisionPointRec(mousePosition, loadWindowTextBox) && mousePressed)
                {
                    inputFocus = InputFocus::LOAD_MESH;
                }
                else if (CheckCollisionPointRec(mousePosition, loadWindowHeightBox) && mousePressed)
                {
                    inputFocus = InputFocus::LOAD_MESH_HEIGHT;
                }
                else if (CheckCollisionPointRec(mousePosition, loadWindowLoadButton) && mousePressed && !loadHeightString.empty())
                {
                    std::string loadName = loadMeshString + ".png";
                    char text[loadName.size() + 1];
                    strcpy(text, loadName.c_str()); 
                    
                    Image import = LoadImage(text); // the image to turn into a mesh
                    Color* importPixels = GetImageData(import);
                    
                    if (import.width <= modelVertexWidth) // find the new canvas width and height, round up from import width and height
                        canvasWidth = 1;
                    else
                    {
                        canvasWidth = ceil((float)(import.width - modelVertexWidth) / (float)(modelVertexWidth - 1) + 1);
                    }
                    
                    if (import.height <= modelVertexHeight)
                        canvasHeight = 1;
                    else
                    {
                        canvasHeight = ceil((float)(import.height - modelVertexHeight) / (float)(modelVertexHeight - 1) + 1);
                    }
                    
                    models.clear();
                    models.resize(canvasWidth);
                    
                    highestY = FLT_MIN; // reset highest and lowest values
                    lowestY = FLT_MAX;
                    
                    for (int i = 0; i < canvasWidth; i++) // turn the image into a 3d heightmap one model at a time
                    {
                        for (int j = 0; j < canvasHeight; j++)
                        {
                            Color* vertexPixels = (Color*)RL_MALLOC(modelVertexHeight*modelVertexWidth*sizeof(Color)); // pixels corresponding to this model
                            
                            int pixelIndex = i * (modelVertexWidth - 1) + j * (modelVertexHeight - 1) * import.width; // index of the pixel to be copied in importPixels
                            
                            int xLength = pixelIndex - pixelIndex / import.width * import.width; // number of pixels to the left of the current pixelIndex
                            int yLength = pixelIndex / import.width; // number of pixels above the current pixelIndex
                            
                            for (int k = 0; k < modelVertexHeight*modelVertexWidth; k++) // fill vertexPixels with the matching pixels in importPixels
                            {
                                if (k > 0 && k % modelVertexWidth == 0)
                                    pixelIndex += import.width - modelVertexWidth; // iterate pixelIndex each time a row of pixels is completed
                                
                                // since the size of the canvas is in multiples of model width and height, it may have more vertices than the image has pixels. if the vertex is out of the image boundary, make its pixel black
                                if (import.width < xLength + (k - (k / modelVertexWidth * modelVertexWidth) + 1) || import.height < yLength + k / modelVertexWidth + 1)
                                {
                                    vertexPixels[k].r = 0;
                                    vertexPixels[k].g = 0;
                                    vertexPixels[k].b = 0;
                                    vertexPixels[k].a = 255;
                                }
                                else
                                {
                                    vertexPixels[k] = importPixels[pixelIndex + k];
                                }
                            }
                            
                            float heightRef = stof(loadHeightString);
                            
                            Image tempImage = LoadImageEx(vertexPixels, modelVertexWidth, modelVertexHeight);
                            //Mesh mesh = GenMeshHeightmap(tempImage, (Vector3){ modelWidth, heightRef, modelHeight });    // Generate heightmap mesh (RAM and VRAM)
                            Model model = LoadModelFromMesh(GenMeshHeightmap(tempImage, (Vector3){ modelWidth, heightRef, modelHeight }));  
                            UnloadImage(tempImage);
                            RL_FREE(vertexPixels);
                            
                            Color* pixels = GenHeightmap(model, modelVertexWidth, modelVertexHeight, highestY, lowestY, 1);
                            Image image = LoadImageEx(pixels, modelVertexWidth - 1, modelVertexHeight - 1);
                            Texture2D tex = LoadTextureFromImage(image); // create a texture from the heightmap. height and width -1 so that pixels and polys are 1:1
                            RL_FREE(pixels);
                            UnloadImage(image);
                            
                            model.materials[0].maps[MAP_DIFFUSE].texture = tex;
                            
                            float xOffset = (float)i * (modelWidth - (1 / (float)modelVertexWidth) * modelWidth);
                            float zOffset = (float)j * (modelHeight - (1 / (float)modelVertexHeight) * modelHeight);
                            
                            for (int i = 0; i < (model.meshes[0].vertexCount * 3) - 2; i += 3) // adjust vertex locations. (probably more sophisticated to do something with the transform but w/e)
                            {
                                model.meshes[0].vertices[i] += xOffset;
                                model.meshes[0].vertices[i + 2] += zOffset;
                            }
                            
                            models[i].push_back(model);
                        }
                    }
                    
                    UnloadImage(import);
                    RL_FREE(importPixels);
                    
                    history.clear(); // clear history if the canvas is shrunk so that undo operations dont go out of bounds
                    stepIndex = 0;
                    historyFlag = false;
                    
                    modelSelection.selection.clear();
                    modelSelection.selection.push_back(Vector2{0, 0}); // set selection to the first model after loading
                    
                    modelSelection.width = 1;
                    modelSelection.height = 1;
                    modelSelection.topLeft = Vector2{0, 0};
                    modelSelection.bottomRight = Vector2{0, 0};
                    
                    modelSelection.expandedSelection.clear();
                    SetExSelection(modelSelection, canvasWidth, canvasHeight);
                    
                    for (int i = 0; i < models.size(); i++)
                    {
                        for (int j = 0; j < models[i].size(); j++)
                        {
                            rlUpdateBuffer(models[i][j].meshes[0].vboId[0], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                            rlUpdateBuffer(models[i][j].meshes[0].vboId[2], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                        }
                    }
                    
                    loadHeightString.clear();
                    inputFocus = InputFocus::NONE;
                    showLoadWindow = false;
                }
                else if (CheckCollisionPointRec(mousePosition, loadWindowCancelButton) && mousePressed)
                {
                    loadHeightString.clear();
                    inputFocus = InputFocus::NONE;
                    showLoadWindow = false;
                }
                
            }// check if the mouse is over a 2d element
            else if (showDirWindow)
            {
                if (mousePressed && CheckCollisionPointRec(mousePosition, dirWindowOkayButton))
                {
                    char text[directoryString.size() + 1];
                    strcpy(text, directoryString.c_str());
                    
                    if (DirectoryExists(text))
                    {
                       ChangeDirectory(text); 
                       
                       inputFocus = InputFocus::NONE;
                       showDirWindow = false;
                    }
                }
                else if (mousePressed && CheckCollisionPointRec(mousePosition, dirWindowCancelButton))
                {
                    inputFocus = InputFocus::NONE;
                    showDirWindow = false;
                }
                else if (mousePressed && CheckCollisionPointRec(mousePosition, dirWindowTextBox))
                {
                    inputFocus = InputFocus::DIRECTORY;
                }
            }
            else if (CheckCollisionPointRec(mousePosition, UI)) // add mouse click check here?
            {
                switch (panel)
                {
                    case Panel::NONE:
                    {
                        if(CheckCollisionPointRec(mousePosition, heightmapTab) && mousePressed)
                        {
                            panel = Panel::HEIGHTMAP;
                            
                            cameraTab.y = 657;
                            toolsTab.y = 680;
                            panel1.height = 637;
                            panel2.y = 677;
                        }
                        else if (CheckCollisionPointRec(mousePosition, cameraTab) && mousePressed)
                        {
                            panel = Panel::CAMERA;
                            
                            panel2.height = 637;
                            toolsTab.y = 680;
                        }
                        else if (CheckCollisionPointRec(mousePosition, toolsTab) && mousePressed)
                        {
                            panel = Panel::TOOLS;
                        }
                        
                        break;
                    }
                    case Panel::HEIGHTMAP:
                    {
                        if(CheckCollisionPointRec(mousePosition, heightmapTab) && mousePressed)
                        {
                            panel = Panel::NONE;
                            
                            cameraTab.y = 23;
                            toolsTab.y = 46;
                            panel1.height = 3;
                            panel2.y = 43;
                        }
                        else if (CheckCollisionPointRec(mousePosition, cameraTab) && mousePressed)
                        {
                            panel = Panel::CAMERA;
                            
                            panel1.height = 3;
                            panel2.height = 637;
                            panel2.y = 43;
                            cameraTab.y = 23;
                            toolsTab.y = 680;
                        }
                        else if (CheckCollisionPointRec(mousePosition, toolsTab) && mousePressed)
                        {
                            panel = Panel::TOOLS;
                            
                            cameraTab.y = 23;
                            toolsTab.y = 46;
                            panel1.height = 3;
                            panel2.y = 43;
                        }
                        
                        if (CheckCollisionPointRec(mousePosition, exportButton) && mousePressed) // export heightmap button
                        {
                            brush = BrushTool::NONE;
                            inputFocus = InputFocus::NONE;
                            
                            showSaveWindow = true;
                            
                            highestY = FLT_MIN; // reset highest and lowest values
                            lowestY = FLT_MAX;
                            
                            for (int i = 0; i < models.size(); i++) // find the highest and lowest values to display them in the save window
                            {
                                for (int j = 0; j < models[i].size(); j++)
                                {
                                    for (int k = 0; k < (models[i][j].meshes[0].vertexCount * 3) - 2; k += 3) 
                                    {
                                        if (models[i][j].meshes[0].vertices[k+1] > highestY)
                                            highestY = models[i][j].meshes[0].vertices[k+1];
                                            
                                        if (models[i][j].meshes[0].vertices[k+1] < lowestY)
                                            lowestY = models[i][j].meshes[0].vertices[k+1];
                                    }
                                }
                            }
                        }
                        else if (CheckCollisionPointRec(mousePosition, meshGenButton) && mousePressed && (!xMeshString.empty() || canvasWidth > 0) && (!zMeshString.empty() || canvasHeight > 0)) // add or remove models
                        {
                            int xInput;
                            int zInput;
                            
                            if (!xMeshString.empty())
                                xInput = std::stoi(xMeshString);
                            else
                                xInput = canvasWidth;
                            
                            if (!zMeshString.empty())
                                zInput = std::stoi(zMeshString);
                            else
                                zInput = canvasHeight;
                            
                            int xDifference = -(canvasWidth - xInput); // negate the difference so that positive is how many to add, negative to subtract
                            int zDifference = -(canvasHeight - zInput);
                            
                            if (xDifference > 0) 
                            {
                                models.reserve(xInput);
                                models.resize(xInput);
                            }
                            else if (xDifference < 0)
                            {
                                models.erase(models.begin() + (models.size() + xDifference), models.end());
                                
                                history.clear(); // clear history if the canvas is shrunk so that undo operations dont go out of bounds
                                stepIndex = 0;
                                historyFlag = false;
                            }
                            
                            canvasWidth = xInput;
                            
                            if (zDifference < 0)
                            {
                                history.clear(); // clear history if the canvas is shrunk so that undo operations dont go out of bounds
                                stepIndex = 0;
                                historyFlag = false;  
                            }
                            
                            if (canvasWidth)
                            {
                                int newLength = canvasHeight + zDifference;
                                
                                for (int i = 0; i < canvasWidth; i++)
                                {
                                    if (models[i].size() > newLength) // z will need to be cut in existing columns, but may have to be expanded in new ones
                                    {
                                        models[i].erase(models[i].begin() + newLength, models[i].end());
                                    }
                                    else
                                    {
                                        models[i].reserve(newLength);
                                        
                                        while (models[i].size() < newLength)
                                        {
                                            int j = models[i].size();
                                            
                                            Image tempImage = GenImageColor(modelVertexWidth, modelVertexHeight, BLACK);
                                            //Mesh mesh = GenMeshHeightmap(tempImage, (Vector3){ modelWidth, 0, modelHeight });    // Generate heightmap mesh (RAM and VRAM)
                                            Model model = LoadModelFromMesh(GenMeshHeightmap(tempImage, (Vector3){ modelWidth, 0, modelHeight }));  
                                            UnloadImage(tempImage);
                                            
                                            Color* pixels = GenHeightmap(model, modelVertexWidth, modelVertexHeight, highestY, lowestY, 1);
                                            Image image = LoadImageEx(pixels, modelVertexWidth - 1, modelVertexHeight - 1);
                                            Texture2D tex = LoadTextureFromImage(image); // create a texture from the heightmap. height and width -1 so that pixels and polys are 1:1
                                            RL_FREE(pixels);
                                            UnloadImage(image);
                                            
                                            model.materials[0].maps[MAP_DIFFUSE].texture = tex;
                                            
                                            float xOffset = (float)i * (modelWidth - (1 / (float)modelVertexWidth) * modelWidth);
                                            float zOffset = (float)j * (modelHeight - (1 / (float)modelVertexHeight) * modelHeight);
                                            
                                            for (int i = 0; i < (model.meshes[0].vertexCount * 3) - 2; i += 3) // adjust vertex locations. (probably more sophisticated to do something with the transform but w/e)
                                            {
                                                model.meshes[0].vertices[i] += xOffset;
                                                model.meshes[0].vertices[i + 2] += zOffset;
                                            }

                                            models[i].push_back(model);
                                        }                             
                                    }   
                                }
                                
                                canvasHeight = zInput; 
                            }
                            
                            if (modelSelection.selection.empty() && !models.empty()) // if models selection is empty, add the first model as the default selection 
                            {
                                modelSelection.selection.push_back(Vector2{0, 0});
                                
                                modelSelection.width = 1;
                                modelSelection.height = 1;
                                modelSelection.topLeft = Vector2{0, 0};
                                modelSelection.bottomRight = Vector2{0, 0};
                            }
                            else
                            {
                                if (canvasWidth <= modelSelection.topLeft.x || canvasHeight <= modelSelection.topLeft.y) // if the canvas was shrunk past the selection, clear selection
                                {
                                    modelSelection.selection.clear();
                                    modelSelection.width = 0;
                                    modelSelection.height = 0;
                                    modelSelection.topLeft = Vector2{0, 0};
                                    modelSelection.bottomRight = Vector2{0, 0};                              
                                }
                                else
                                {
                                    if (canvasWidth <= modelSelection.bottomRight.x || canvasHeight <= modelSelection.bottomRight.y) // if the canvas was shrunk past the selection boundary, shrink the selection to match
                                    {
                                        for (int i = 0; i < modelSelection.selection.size(); i++) // remove models from the selection that exceed canvas size
                                        {
                                            if (modelSelection.selection[i].x >= canvasWidth || modelSelection.selection[i].y >= canvasHeight)
                                            {
                                                modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                                i--;
                                            }
                                        }
                                        
                                        modelSelection.width = canvasWidth - modelSelection.topLeft.x;
                                        modelSelection.height = canvasHeight - modelSelection.topLeft.y;
                                        modelSelection.bottomRight = Vector2{canvasWidth - 1, canvasHeight - 1};                               
                                    }
                                }
                            }
                            
                            modelSelection.expandedSelection.clear();
                            SetExSelection(modelSelection, canvasWidth, canvasHeight);  
                            
                            for (int i = 0; i < models.size(); i++)
                            {
                                for (int j = 0; j < models[i].size(); j++)
                                {
                                    rlUpdateBuffer(models[i][j].meshes[0].vboId[0], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                                    rlUpdateBuffer(models[i][j].meshes[0].vboId[2], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                                }
                            }
                            
                            xMeshString.clear();
                            zMeshString.clear();
                        }
                        else if (CheckCollisionPointRec(mousePosition, xMeshBox) && mousePressed)
                        {
                            inputFocus = InputFocus::X_MESH;
                        }
                        else if (CheckCollisionPointRec(mousePosition, zMeshBox) && mousePressed)
                        {
                            inputFocus = InputFocus::Z_MESH;
                        }
                        else if (CheckCollisionPointRec(mousePosition, updateTextureButton) && mousePressed && !models.empty()) // find the lowest and highest point on the mesh
                        {
                            float maxY = models[0][0].meshes[0].vertices[1]; // start with the first y value;
                            float minY = models[0][0].meshes[0].vertices[1];
                            
                            for (int i = 0; i < models.size(); i++)
                            {
                                for (int j = 0; j < models[i].size(); j++)
                                {
                                    for (int k = 0; k < (models[i][j].meshes[0].vertexCount * 3) - 2; k += 3) // check this models vertices
                                    {
                                        if (models[i][j].meshes[0].vertices[k + 1] > maxY)
                                            maxY = models[i][j].meshes[0].vertices[k + 1];
                                        
                                        if (models[i][j].meshes[0].vertices[k + 1] < minY)
                                            minY = models[i][j].meshes[0].vertices[k + 1];
                                    }
                                }
                            }
                            
                            highestY = maxY;
                            lowestY = minY;
                            
                            UpdateHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                        }
                        else if (CheckCollisionPointRec(mousePosition, loadButton) && mousePressed)
                        {
                            brush = BrushTool::NONE;
                            inputFocus = InputFocus::NONE;
                            
                            showLoadWindow = true;
                        }
                        else if (mousePressed && CheckCollisionPointRec(mousePosition, directoryButton))
                        {
                            brush = BrushTool::NONE;
                            inputFocus = InputFocus::NONE;
                            
                            showDirWindow = true;
                        }
                    
                        break;
                    }
                    case Panel::CAMERA:
                    {
                        if (CheckCollisionPointRec(mousePosition, heightmapTab) && mousePressed)
                        {
                            panel = Panel::HEIGHTMAP;
                            
                            cameraTab.y = 657;
                            panel1.height = 637;
                            panel2.y = 677;
                            panel2.height = 3;
                        }
                        else if (CheckCollisionPointRec(mousePosition, cameraTab) && mousePressed)
                        {
                            panel = Panel::NONE;
                            
                            toolsTab.y = 46;
                            panel2.y = 43;
                            panel2.height = 3;
                        }
                        else if (CheckCollisionPointRec(mousePosition, toolsTab) && mousePressed)
                        {
                            panel = Panel::TOOLS;
                            
                            toolsTab.y = 46;
                            panel2.y = 43;
                            panel2.height = 3;                       
                        }
                        else if (CheckCollisionPointRec(mousePosition, characterButton) && mousePressed)
                        {
                            brush = BrushTool::NONE;
                            characterDrag = true;
                        }
                        
                        break;
                    }
                    case Panel::TOOLS:
                    {
                        if (CheckCollisionPointRec(mousePosition, heightmapTab) && mousePressed)
                        {
                            panel = Panel::HEIGHTMAP;
                            
                            cameraTab.y = 657;
                            toolsTab.y = 680;
                            panel1.height = 637;
                            panel2.y = 677;
                        }
                        else if (CheckCollisionPointRec(mousePosition, cameraTab) && mousePressed)
                        {
                            panel = Panel::CAMERA;
                            
                            panel2.height = 637;
                            toolsTab.y = 680;
                        }
                        else if (CheckCollisionPointRec(mousePosition, toolsTab) && mousePressed)
                        {
                            panel = Panel::NONE;
                        }
                        
                        if (CheckCollisionPointRec(mousePosition, elevToolButton) && mousePressed)
                        {
                            if (brush == BrushTool::ELEVATION)
                                brush = BrushTool::NONE;
                            else
                                brush = BrushTool::ELEVATION;
                        }
                        else if (CheckCollisionPointRec(mousePosition, smoothToolButton) && mousePressed)
                        {
                            if (brush == BrushTool::SMOOTH)
                                brush = BrushTool::NONE;
                            else                
                                brush = BrushTool::SMOOTH;
                        }
                        else if (CheckCollisionPointRec(mousePosition, flattenToolButton) && mousePressed)
                        {
                            if (brush == BrushTool::FLATTEN)
                                brush = BrushTool::NONE;
                            else                
                                brush = BrushTool::FLATTEN;
                        }
                        else if (CheckCollisionPointRec(mousePosition, stampToolButton) && mousePressed)
                        {
                            if (brush == BrushTool::STAMP)
                                brush = BrushTool::NONE;
                            else                
                                brush = BrushTool::STAMP;
                        }
                        else if (CheckCollisionPointRec(mousePosition, selectToolButton) && mousePressed)
                        {
                            if (brush == BrushTool::SELECT)
                                brush = BrushTool::NONE;
                            else
                                brush = BrushTool::SELECT;
                        }
                        else if (brush == BrushTool::SELECT && mousePressed && CheckCollisionPointRec(mousePosition, deselectButton))
                        {
                            vertexSelection.clear();
                        }
                        else if (brush == BrushTool::SELECT && mousePressed && CheckCollisionPointRec(mousePosition, trailToolButton) && !vertexSelection.empty() && vertexSelection[vertexSelection.size() - 1].y > 1) // use trail tool
                        {
                            HistoryUpdate(history, models, modelSelection.expandedSelection, stepIndex, maxSteps);
                            historyFlag = true;
                            
                            float top = models[vertexSelection[0].coords.x][vertexSelection[0].coords.y].meshes[0].vertices[vertexSelection[0].vertexIndex + 1];
                            float bottom = models[vertexSelection[(int)vertexSelection.size() - 1].coords.x][vertexSelection[(int)vertexSelection.size() - 1].coords.y].meshes[0].vertices[vertexSelection[(int)vertexSelection.size() - 1].vertexIndex + 1];
                            
                            if (top < bottom) // swap values if selection was made bottom to top
                            {
                                float temp = top;
                                top = bottom;
                                bottom = temp;
                            }
                            
                            float increment = (top - bottom) / (vertexSelection[vertexSelection.size() - 1].y); // difference in height between layers on the slope
                            
                            for (int i = 0; i < vertexSelection.size(); i++)
                            {
                                if (vertexSelection[i].y == 1) // flatten out the highest portion with the highest value
                                    models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].vertexIndex + 1] = top;
                                
                                models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].vertexIndex + 1] = top - (increment * (vertexSelection[i].y - 1));
                            } 
                            
                            for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                            {
                                rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[0], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                                rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[2], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                                    
                                UpdateHeightmap(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY);   
                            }
                        }
                        else if (brush == BrushTool::SELECT && mousePressed && CheckCollisionPointRec(mousePosition, selectionMaskButton))
                        {
                            selectionMask = !selectionMask;
                        }
                        else if (CheckCollisionPointRec(mousePosition, xMeshSelectBox) && mousePressed)
                        {
                            inputFocus = InputFocus::X_MESH_SELECT;
                        }
                        else if (CheckCollisionPointRec(mousePosition, zMeshSelectBox) && mousePressed)
                        {
                            inputFocus = InputFocus::Z_MESH_SELECT;
                        }
                        else if (CheckCollisionPointRec(mousePosition, meshSelectButton) && mousePressed && !models.empty()) // adjust model selection
                        {
                            ModelSelection modelSelectionCopy = modelSelection;
                            
                            int xInput;
                            int zInput;
                            
                            if (xMeshSelectString.empty())
                                xInput = modelSelection.width;
                            else
                                xInput = std::stoi(xMeshSelectString);
                            
                            if (zMeshSelectString.empty())
                                zInput = modelSelection.height;
                            else
                                zInput = std::stoi(zMeshSelectString);
                            
                            int newWidth = xInput;
                            int newHeight = zInput;
                            
                            if (newWidth > canvasWidth) // dont allow selection width or height to be larger than the canvas
                                newWidth = canvasWidth;
                            
                            if (newHeight > canvasHeight)
                                newHeight = canvasHeight;
                            
                            if (modelSelection.selection.empty() && xInput != 0 && zInput != 0) // if the selection is empty, fill the new selection from the top left corner of the canvas
                            {
                                for (int i = 0; i < newWidth; i++)
                                {
                                    for (int j = 0; j < newHeight; j++)
                                    {
                                        modelSelection.selection.push_back(Vector2{i, j});
                                    }
                                }
                                
                                modelSelection.bottomRight = Vector2{newWidth - 1, newHeight - 1};
                                modelSelection.topLeft = Vector2{0, 0}; 
                                modelSelection.width = newWidth;
                                modelSelection.height = newHeight;                         
                            } 
                            else if (xInput == 0 || zInput == 0) // if either are 0, clear selection
                            {
                                modelSelection.selection.clear();
                                modelSelection.bottomRight = Vector2{0, 0};
                                modelSelection.topLeft = Vector2{0, 0}; 
                                modelSelection.width = 0;
                                modelSelection.height = 0; 
                            }
                            else 
                            {
                                if (newWidth < modelSelection.width && newHeight < modelSelection.height) // if both columns and rows have to be deleted
                                {
                                    int newX = newWidth + modelSelection.topLeft.x - 1; // find the new x coord that is the rightmost edge
                                    int newY = newHeight + modelSelection.topLeft.y - 1; // find the new y coord that is the bottom edge
                                    
                                    int i = 0;
                                    
                                    while (i < modelSelection.selection.size()) // if a model exceeds new x or y, delete it
                                    {
                                        if (modelSelection.selection[i].x > newX || modelSelection.selection[i].y > newY)
                                        {
                                            modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                            i--;
                                        }
                                        
                                        i++;
                                    }
                                    
                                    modelSelection.bottomRight.x = newX;
                                    modelSelection.bottomRight.y = newY; 
                                    modelSelection.width = newWidth;//newX - modelSelection.topLeft.x;
                                    modelSelection.height = newHeight;//newY - modelSelection.topLeft.y;
                                }
                                
                                if (newWidth < modelSelection.width) // if only columns have to be deleted
                                {
                                    int newX = newWidth + modelSelection.topLeft.x - 1; // find the new x coord that is the rightmost edge
                                    
                                    int i = 0;
                                    
                                    while (i < modelSelection.selection.size()) // if a model exceeds new x, delete it
                                    {
                                        if (modelSelection.selection[i].x > newX)
                                        {
                                            modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                            i--;
                                        }
                                        
                                        i++;
                                    }
                                    
                                    modelSelection.bottomRight.x = newX;
                                    modelSelection.width = newWidth;//newX - modelSelection.topLeft.x;
                                }
                                
                                if (newHeight < modelSelection.height) // if only rows have to be deleted
                                {
                                    int newY = newHeight + modelSelection.topLeft.y - 1; // find the new y coord that is the bottom edge
                                    
                                    int i = 0;
                                    
                                    while (i < modelSelection.selection.size()) // if a model exceeds new y, delete it
                                    {
                                        if (modelSelection.selection[i].y > newY)
                                        {
                                            modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                            i--;
                                        }
                                        
                                        i++;
                                    }
                                    
                                    modelSelection.bottomRight.y = newY; 
                                    modelSelection.height = newHeight; //newY - modelSelection.topLeft.y;                                
                                }
                                
                                if (newHeight > modelSelection.height) // if rows need to be added
                                {
                                    for (int i = 0; i < newHeight - modelSelection.height; i++)
                                    {
                                        if (modelSelection.bottomRight.y == canvasHeight - 1) // if the selection is at the bottom, expand upwards
                                        {
                                            for (int j = 0; j < modelSelection.width; j++)
                                            {
                                                modelSelection.selection.push_back(Vector2{modelSelection.topLeft.x + j, modelSelection.topLeft.y - 1});
                                            }
                                            
                                            modelSelection.topLeft.y--;
                                        }
                                        else // else expand downwards
                                        {
                                            for (int j = 0; j < modelSelection.width; j++)
                                            {
                                                modelSelection.selection.push_back(Vector2{modelSelection.topLeft.x + j, modelSelection.bottomRight.y + 1});
                                            }
                                            
                                            modelSelection.bottomRight.y++;
                                        }
                                    }
                                    
                                    modelSelection.height = newHeight;
                                }
                                
                                if (newWidth > modelSelection.width) // if columns need to be added
                                {
                                    for (int i = 0; i < newWidth - modelSelection.width; i++)
                                    {
                                        if (modelSelection.bottomRight.x == canvasWidth - 1) // if the selection is at the farthest right, expand left
                                        {
                                            for (int j = 0; j < modelSelection.height; j++)
                                            {
                                                modelSelection.selection.push_back(Vector2{modelSelection.topLeft.x - 1, modelSelection.topLeft.y + j});
                                            }
                                            
                                            modelSelection.topLeft.x--;
                                        }
                                        else // else expand right
                                        {
                                            for (int j = 0; j < modelSelection.height; j++)
                                            {
                                                modelSelection.selection.push_back(Vector2{modelSelection.bottomRight.x + 1, modelSelection.topLeft.y + j});
                                            }
                                            
                                            modelSelection.bottomRight.x++;   
                                        }
                                    }
                                    
                                    modelSelection.width = newWidth;
                                }
                            }
                            
                            modelSelection.expandedSelection.clear();
                            SetExSelection(modelSelection, canvasWidth, canvasHeight);
                            
                            xMeshSelectString.clear();
                            zMeshSelectString.clear();
                        }
                        else if (CheckCollisionPointRec(mousePosition, meshSelectUpButton) && mousePressed && !models.empty() && !modelSelection.selection.empty()) // shift model selection up
                        {
                            ModelSelection modelSelectionCopy = modelSelection;
                            
                            if (modelSelection.topLeft.y != 0)
                            {
                                for (int i = 0; i < modelSelection.selection.size(); i++) // remove bottom row from selection
                                {
                                    if (modelSelection.selection[i].y == modelSelection.bottomRight.y)
                                    {
                                        modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                        i--;
                                    }
                                }
                                
                                modelSelection.bottomRight.y--;
                                
                                for (int i = 0; i < modelSelection.width; i++) // add a row above selection
                                {
                                    modelSelection.selection.push_back(Vector2{i + modelSelection.topLeft.x, modelSelection.topLeft.y - 1});
                                }
                                
                                modelSelection.topLeft.y--;
                            }
                            
                            modelSelection.expandedSelection.clear();
                            SetExSelection(modelSelection, canvasWidth, canvasHeight);
                        }
                        else if (CheckCollisionPointRec(mousePosition, meshSelectRightButton) && mousePressed && !models.empty() && !modelSelection.selection.empty()) // shift model selection right
                        {
                            ModelSelection modelSelectionCopy = modelSelection;
                            
                            if (modelSelection.bottomRight.x != canvasWidth - 1)
                            {
                                for (int i = 0; i < modelSelection.selection.size(); i++) // remove left column from selection
                                {
                                    if (modelSelection.selection[i].x == modelSelection.topLeft.x)
                                    {
                                        modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                        i--;
                                    }
                                }
                                
                                modelSelection.topLeft.x++;
                                
                                for (int i = 0; i < modelSelection.height; i++) // add a column to the right of the selection
                                {
                                    modelSelection.selection.push_back(Vector2{modelSelection.bottomRight.x + 1, i + modelSelection.topLeft.y});
                                }
                                
                                modelSelection.bottomRight.x++;
                            }     

                            modelSelection.expandedSelection.clear();
                            SetExSelection(modelSelection, canvasWidth, canvasHeight);
                        }
                        else if (CheckCollisionPointRec(mousePosition, meshSelectLeftButton) && mousePressed && !models.empty() && !modelSelection.selection.empty()) // shift model selection left
                        {
                            ModelSelection modelSelectionCopy = modelSelection;
                            
                            if (modelSelection.topLeft.x != 0)
                            {
                                for (int i = 0; i < modelSelection.selection.size(); i++) // remove rightmost column from selection
                                {
                                    if (modelSelection.selection[i].x == modelSelection.bottomRight.x)
                                    {
                                        modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                        i--;
                                    }
                                }
                                
                                modelSelection.bottomRight.x--;
                                
                                for (int i = 0; i < modelSelection.height; i++) // add a column left of selection
                                {
                                    modelSelection.selection.push_back(Vector2{modelSelection.topLeft.x - 1, i + modelSelection.topLeft.y});
                                }
                                
                                modelSelection.topLeft.x--;
                            }    

                            modelSelection.expandedSelection.clear();
                            SetExSelection(modelSelection, canvasWidth, canvasHeight);
                        }
                        else if (CheckCollisionPointRec(mousePosition, meshSelectDownButton) && mousePressed && !models.empty() && !modelSelection.selection.empty()) // shift model selection down
                        {
                            ModelSelection modelSelectionCopy = modelSelection;
                            
                            if (modelSelection.bottomRight.y != canvasHeight - 1)
                            {
                                for (int i = 0; i < modelSelection.selection.size(); i++) // remove top row from selection
                                {
                                    if (modelSelection.selection[i].y == modelSelection.topLeft.y)
                                    {
                                        modelSelection.selection.erase(modelSelection.selection.begin() + i);
                                        i--;
                                    }
                                }
                                
                                modelSelection.topLeft.y++;
                                
                                for (int i = 0; i < modelSelection.width; i++) // add a row below selection
                                {
                                    modelSelection.selection.push_back(Vector2{i + modelSelection.topLeft.x, modelSelection.bottomRight.y + 1});
                                }
                                
                                modelSelection.bottomRight.y++;
                            }  

                            modelSelection.expandedSelection.clear();
                            SetExSelection(modelSelection, canvasWidth, canvasHeight);
                        }
                        else if (CheckCollisionPointRec(mousePosition, stampAngleBox) && mousePressed)
                        {
                            inputFocus = InputFocus::STAMP_ANGLE;
                        }
                        else if (CheckCollisionPointRec(mousePosition, stampHeightBox) && mousePressed)
                        {
                            inputFocus = InputFocus::STAMP_HEIGHT;
                        }
                        else if (CheckCollisionPointRec(mousePosition, toolStrengthBox) && mousePressed)
                        {
                            inputFocus = InputFocus::TOOL_STRENGTH;
                        }
                        else if (CheckCollisionPointRec(mousePosition, selectRadiusBox) && mousePressed)
                        {
                            inputFocus = InputFocus::SELECT_RADIUS;
                        }
                        
                        break;
                    }
                }
            }
            else if (brush != BrushTool::NONE) // if mouse is not over a 2d element --------------------------------------------------------------------------------------------------
            {
                if (IsKeyDown(KEY_LEFT_BRACKET) && selectRadius > 0) 
                    selectRadius -= 0.04f;
                
                if (IsKeyDown(KEY_RIGHT_BRACKET))
                    selectRadius += 0.04f;
                
                ray = GetMouseRay(GetMousePosition(), camera);
                
                int modelIndex; // the index of the model in modelSelection that collides with ray
                
                if (rayCollision2d && !models.empty()) // check if ray collision should be tested against the x & z plane or the 3d mesh
                {
                    hitPosition = GetCollisionRayGround(ray, 0);
                    
                    int index1 = GetVertexIndices(modelVertexWidth - 1, 0, modelVertexWidth)[0];
                    int index2 = GetVertexIndices(0, modelVertexHeight - 1, modelVertexWidth)[0];
                    
                    float leftX = models[modelSelection.topLeft.x][modelSelection.topLeft.y].meshes[0].vertices[0];
                    float rightX = models[modelSelection.bottomRight.x][modelSelection.bottomRight.y].meshes[0].vertices[index1];
                    float topY = models[modelSelection.topLeft.x][modelSelection.topLeft.y].meshes[0].vertices[2];
                    float bottomY = models[modelSelection.bottomRight.x][modelSelection.bottomRight.y].meshes[0].vertices[index2 + 2];
                    
                    if (hitPosition.position.x < leftX || hitPosition.position.x > rightX || hitPosition.position.z < topY || hitPosition.position.z > bottomY) // if the hit position is outside of the model selection, mark as false
                    {
                        hitPosition.hit = false;
                    }
                }
                else if (!models.empty())
                {
                    for (int i = 0; i < modelSelection.selection.size(); i++) // test ray against selected models
                    {
                        hitPosition = GetCollisionRayModel2(ray, &models[modelSelection.selection[i].x][modelSelection.selection[i].y]); 
                        
                        if (hitPosition.hit) // if collision is found, record which and break the search
                        {
                            modelIndex = i;
                            break;
                        }
                    }
                }
                
                if (hitPosition.hit)
                {
                    hitCoords = {hitPosition.position.x, hitPosition.position.z};
                    
                    for (int i = 0; i < modelSelection.expandedSelection.size(); i++) // check all models recorded by modelSelection for vertices to be selected
                    {
                        for(int j = 0; j < (models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // check this models vertices
                        {
                            Vector2 vertexCoords = {models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices[j], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices[j+2]};
                            
                            float result = xzDistance(hitCoords, vertexCoords);
                            
                            if (result <= selectRadius) // if this vertex is inside the radius
                            {
                                Vector3 vec = {models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices[j], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices[j+1], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices[j+2]};
                                displayedVertices.push_back(vec); // store the location of this vertex
                                
                                ModelVertex mv;
                                mv.coords.x = modelSelection.expandedSelection[i].x;
                                mv.coords.y = modelSelection.expandedSelection[i].y;
                                mv.index = j; 
                                
                                vertexIndices.push_back(mv); // store the vertex's index in the mesh and its models coords
                            }
                        }                   
                    }
                    
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !vertexIndices.empty() && brush == BrushTool::ELEVATION)
                    {
                        if (!updateFlag) // update the history before executing if this is the first tick of the operation
                        {
                            HistoryUpdate(history, models, modelSelection.expandedSelection, stepIndex, maxSteps);
                            historyFlag = true;
                        }
                        
                        if (IsKeyDown(KEY_LEFT_CONTROL)) // do the inverse if left ctrl is held
                        {
                            if (selectionMask) // if selection mask is on, dont modify selected vertices
                            {
                                for (int i = 0; i < vertexIndices.size(); ++i) // TODO change to a better search once vertexSelection is sorted
                                {
                                    bool match = false;
                                    
                                    for (int j = 0; j < vertexSelection.size(); j++)
                                    {
                                        if (vertexIndices[i] == vertexSelection[j])
                                        {
                                            match = true;
                                            break;
                                        }
                                    }
                                    
                                    if (!match)
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index+1] -= toolStrength;
                                }
                            }
                            else
                            {
                                for (int i = 0; i < vertexIndices.size(); ++i)
                                    models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index+1] -= toolStrength;
                            }
                        }
                        else
                        {
                            if (selectionMask) // if selection mask is on, dont modify selected vertices
                            {
                                for (int i = 0; i < vertexIndices.size(); ++i) // TODO change to a better search once vertexSelection is sorted
                                {
                                    bool match = false;
                                    
                                    for (int j = 0; j < vertexSelection.size(); j++)
                                    {
                                        if (vertexIndices[i] == vertexSelection[j])
                                        {
                                            match = true;
                                            break;
                                        }
                                    }
                                    
                                    if (!match)
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index+1] += toolStrength;
                                }
                            }
                            else
                            {
                                for (int i = 0; i < vertexIndices.size(); ++i)
                                    models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index+1] += toolStrength;
                            }                   
                        }
                        
                        for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                        {
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[0], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[2], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                        }
                        
                        timeCounter += GetFrameTime();
                        
                        if (timeCounter >= 0.1f) // update heightmap every tenth of a second
                        {
                            for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                            {
                                UpdateHeightmap(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                            }
                            
                            timeCounter = 0;
                        }
                        
                        updateFlag = true;
                    }
                    
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !vertexIndices.empty() && brush == BrushTool::FLATTEN)
                    {
                        if (!updateFlag) // update the history before executing if this is the first tick of the operation
                        {
                            HistoryUpdate(history, models, modelSelection.expandedSelection, stepIndex, maxSteps);
                            historyFlag = true;
                        }
                        
                        if (selectionMask) // if selection mask is on, dont modify selected vertices
                        {
                            for (int i = 0; i < vertexIndices.size(); ++i) // TODO change to a better search once vertexSelection is sorted 
                            {
                                bool match = false;
                                
                                for (int j = 0; j < vertexSelection.size(); j++)
                                {
                                    if (vertexIndices[i] == vertexSelection[j])
                                    {
                                        match = true;
                                        break;
                                    }
                                }
                                
                                if (!match)
                                    models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index+1] = hitPosition.position.y;
                            }
                        }
                        else
                        {
                            for (int i = 0; i < vertexIndices.size(); ++i)
                                models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index+1] = hitPosition.position.y;
                        }    
                        
                        for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                        {
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[0], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[2], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                        }
                        
                        timeCounter += GetFrameTime();
                        
                        if (timeCounter >= 0.1f)
                        {
                            for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                            {
                                UpdateHeightmap(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                            }
                            
                            timeCounter = 0;
                        }
                        
                        updateFlag = true;
                    }
                    
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !vertexIndices.empty() && brush == BrushTool::SELECT)
                    {
                        if (IsKeyDown(KEY_LEFT_CONTROL)) // if left ctrl is down, do the deselect
                        {
                            for (int i = 0; i < vertexIndices.size(); i++)
                            {
                                for (int j = 0; j < vertexSelection.size(); j++)
                                {
                                    if(vertexIndices[i] == vertexSelection[j])
                                    {
                                        vertexSelection.erase(vertexSelection.begin() + j);
                                        break;
                                    }
                                }
                            }   
                        }
                        else
                        {
                            // add to vertexselection 
                            if (vertexSelection.empty()) 
                            {
                                for (int i = 0; i < vertexIndices.size(); i++) // TODO sort selection, with timer
                                {
                                    VertexState temp;
                                    temp.coords = vertexIndices[i].coords;
                                    temp.vertexIndex = vertexIndices[i].index;
                                    temp.y = 1; // y is used here to represent when this vertex was selected
                                    vertexSelection.push_back(temp);
                                }
                            }
                            else 
                            {
                                int selectionStep = vertexSelection[vertexSelection.size() - 1].y;
                                
                                for (int i = 0; i < vertexIndices.size(); i++)
                                {
                                    bool add = true;
                                    
                                    for (int j = 0; j < vertexSelection.size(); j++)
                                    {
                                        if(vertexIndices[i] == vertexSelection[j])
                                        {
                                            add = false;
                                            break;
                                        }
                                    }
                                    
                                    if (add) // adding to vector seems to have a big performance cost at larger sizes
                                    {
                                        VertexState temp;
                                        temp.coords = vertexIndices[i].coords;
                                        temp.vertexIndex = vertexIndices[i].index;
                                        temp.y = selectionStep + 1; // y is used here to represent when this vertex was selected
                                        vertexSelection.push_back(temp);
                                    }
                                }
                            }                            
                        }
                    }
                    
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !vertexIndices.empty() && brush == BrushTool::SMOOTH) // smooth by moving each vertex closer to the average y of its neighbors
                    {
                        if (!updateFlag) // update the history before executing if this is the first tick of the operation
                        {
                            HistoryUpdate(history, models, modelSelection.expandedSelection, stepIndex, maxSteps);
                            historyFlag = true;
                        }
                        
                        std::vector<VertexState> changes; //  calculate and then make changes all at once rather than one at a time

                        if (selectionMask) // if selection mask is on, dont modify selected vertices
                        {
                            for (int i = 0; i < vertexIndices.size(); ++i) // TODO change to a better search once vertexSelection is sorted
                            {
                                bool match = false;
                                
                                for (int j = 0; j < vertexSelection.size(); j++)
                                {
                                    if (vertexIndices[i] == vertexSelection[j])
                                    {
                                        match = true;
                                        break;
                                    }
                                }
                                
                                if (!match)
                                {
                                    Vector2 coords = GetVertexCoords(vertexIndices[i].index, modelVertexWidth); // the coords of this vertex within the mesh
                            
                                    std::vector<float> yValues; // y values of this vertex's neighbors
                                    
                                    if (coords.x != 0) // dont attempt x = -1
                                    {
                                        int index = GetVertexIndices(coords.x - 1, coords.y, modelVertexWidth)[0];
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);   
                                    }
                                    else if (coords.x == 0 && (vertexIndices[i].coords.x > 0)) // if the vertex x coord is 0, check if there is another model to the left
                                    {
                                        int index = GetVertexIndices(modelVertexWidth - 2, coords.y, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x - 1][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);
                                    }
                                    
                                    if (coords.x != modelVertexWidth - 1) // dont attempt out of bounds x
                                    {
                                        int index = GetVertexIndices(coords.x + 1, coords.y, modelVertexWidth)[0];
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);
                                    }
                                    else if (coords.x == modelVertexWidth - 1 && (vertexIndices[i].coords.x < canvasWidth - 1)) // if the vertex x coord is at max width, check if there is another model to the right
                                    {
                                        int index = GetVertexIndices(1, coords.y, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x + 1][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);                                   
                                    }
                                    
                                    if (coords.y != 0) // dont attempt y = -1
                                    {
                                        int index = GetVertexIndices(coords.x, coords.y - 1, modelVertexWidth)[0];
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);    
                                    }
                                    else if (coords.y == 0 && (vertexIndices[i].coords.y > 0)) // if the vertex y coord is at the top, check if there is a model above
                                    {
                                        int index = GetVertexIndices(coords.x, modelVertexHeight - 2, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y - 1].meshes[0].vertices[index + 1]);
                                    }
                                    
                                    if (coords.y != modelVertexHeight - 1) // dont attempt out of bounds y
                                    {
                                        int index = GetVertexIndices(coords.x, coords.y + 1, modelVertexWidth)[0];
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);
                                    }
                                    else if (coords.y == modelVertexHeight - 1 && (vertexIndices[i].coords.y < canvasHeight - 1)) // if the vertex y coord is at the bottom, check if there is a model below
                                    {
                                        int index = GetVertexIndices(coords.x, 1, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                        
                                        yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y + 1].meshes[0].vertices[index + 1]);
                                    }
                                    
                                    float average = 0;
                                    
                                    for (int i = 0; i < yValues.size(); i++)
                                    {
                                        average += yValues[i];
                                    }
                                    
                                    average = average / (float)yValues.size();
                                    
                                    VertexState temp;
                                    temp.coords = vertexIndices[i].coords;
                                    temp.vertexIndex = vertexIndices[i].index;
                                    temp.y = average;
                                    
                                    changes.push_back(temp);
                                }
                            }
                        }
                        else
                        {
                            for (int i = 0; i < vertexIndices.size(); ++i)
                            {
                                Vector2 coords = GetVertexCoords(vertexIndices[i].index, modelVertexWidth);
                                
                                std::vector<float> yValues;
                                
                                if (coords.x != 0) // dont attempt x = -1
                                {
                                    int index = GetVertexIndices(coords.x - 1, coords.y, modelVertexWidth)[0];
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);   
                                }
                                else if (coords.x == 0 && (vertexIndices[i].coords.x > 0)) // if the vertex x coord is 0, check if there is another model to the left
                                {
                                    int index = GetVertexIndices(modelVertexWidth - 2, coords.y, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x - 1][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);
                                }
                                
                                if (coords.x != modelVertexWidth - 1) // dont attempt out of bounds x
                                {
                                    int index = GetVertexIndices(coords.x + 1, coords.y, modelVertexWidth)[0];
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);
                                }
                                else if (coords.x == modelVertexWidth - 1 && (vertexIndices[i].coords.x < canvasWidth - 1)) // if the vertex x coord is at max width, check if there is another model to the right
                                {
                                    int index = GetVertexIndices(1, coords.y, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x + 1][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);                                   
                                }
                                
                                if (coords.y != 0) // dont attempt y = -1
                                {
                                    int index = GetVertexIndices(coords.x, coords.y - 1, modelVertexWidth)[0];
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);    
                                }
                                else if (coords.y == 0 && (vertexIndices[i].coords.y > 0)) // if the vertex y coord is at the top, check if there is a model above
                                {
                                    int index = GetVertexIndices(coords.x, modelVertexHeight - 2, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y - 1].meshes[0].vertices[index + 1]);
                                }
                                
                                if (coords.y != modelVertexHeight - 1) // dont attempt out of bounds y
                                {
                                    int index = GetVertexIndices(coords.x, coords.y + 1, modelVertexWidth)[0];
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[index + 1]);
                                }
                                else if (coords.y == modelVertexHeight - 1 && (vertexIndices[i].coords.y < canvasHeight - 1)) // if the vertex y coord is at the bottom, check if there is a model below
                                {
                                    int index = GetVertexIndices(coords.x, 1, modelVertexWidth)[0]; // get the index of the vertex in the other model
                                    
                                    yValues.push_back(models[vertexIndices[i].coords.x][vertexIndices[i].coords.y + 1].meshes[0].vertices[index + 1]);
                                }
                                
                                float average = 0;
                                
                                for (int i = 0; i < yValues.size(); i++)
                                {
                                    average += yValues[i];
                                }
                                
                                average = average / (float)yValues.size();
                                
                                VertexState temp;
                                temp.coords = vertexIndices[i].coords;
                                temp.vertexIndex = vertexIndices[i].index;
                                temp.y = average;
                                
                                changes.push_back(temp);
                            }
                        }  
                        
                        for (int i = 0; i < changes.size(); i++) // enact all changes 
                        {
                            models[changes[i].coords.x][changes[i].coords.y].meshes[0].vertices[changes[i].vertexIndex + 1] = changes[i].y;   
                        }

                        for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                        {
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[0], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[2], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                        }
                        
                        timeCounter += GetFrameTime();
                        
                        if (timeCounter >= 0.1f) // update the heightmap at intervals of .1 seconds while the tool is being used
                        {
                            for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                            {
                                UpdateHeightmap(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                            }
                            
                            timeCounter = 0;
                        }
                        
                        updateFlag = true;
                    }
                    
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !vertexIndices.empty() && brush == BrushTool::STAMP)
                    {
                        if (!updateFlag) // update the history before executing if this is the first tick of the operation
                        {
                            HistoryUpdate(history, models, modelSelection.expandedSelection, stepIndex, maxSteps);
                            historyFlag = true;
                        }
                        
                        for (int i = 0; i < vertexIndices.size(); i++)
                        {
                            Vector2 vertexPos = {models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index], models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 2]};
                            
                            float dist = (selectRadius - xzDistance(Vector2{hitPosition.position.x, hitPosition.position.z}, vertexPos)); // distance from the edge of the selection radius
                            float yValue = (dist*sinf(stampAngle*DEG2RAD)/sinf((180 - (90 + stampAngle))*DEG2RAD)); // new y value of this vertex
                            
                            if (raiseOnly)
                            {
                                if (stampHeight)
                                {
                                    if (yValue >= stampHeight && models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] < stampHeight)
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] = stampHeight;   
                                    else if (yValue < stampHeight && models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] < yValue)
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] = yValue;   
                                }
                                else
                                {
                                    if (models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] < yValue)
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] = yValue;   
                                }
                            }
                            else
                            {
                                if (stampHeight)
                                {
                                    if (yValue <= stampHeight)
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] = yValue; 
                                    else
                                        models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] = stampHeight; 
                                }
                                else
                                    models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] = yValue;
                            }
                        }
                        
                        for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                        {
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[0], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                            rlUpdateBuffer(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vboId[2], models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertices, models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                        }
                        
                        timeCounter += GetFrameTime();
                        
                        if (timeCounter >= 0.1f) // update heightmap every tenth of a second
                        {
                            for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                            {
                                UpdateHeightmap(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                            }
                            
                            timeCounter = 0;
                        }
                        
                        updateFlag = true;
                    }
                }
            }
            
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) 
            {
                if (characterDrag)
                {
                    characterDrag = false;
                    
                    if (!CheckCollisionPointRec(mousePosition, UI)) // if it wasnt released over the ui
                    {
                        ray = GetMouseRay(GetMousePosition(), camera);
                        
                        int modelx; 
                        int modely;
                    
                        if (!models.empty())
                        {
                            for (int i = 0; i < models.size(); i++) // test ray against all models
                            {
                                for (int j = 0; j < models[i].size(); j++)
                                {
                                    hitPosition = GetCollisionRayModel2(ray, &models[i][j]); 
                                    
                                    if (hitPosition.hit) // if collision is found, record which and break the search
                                    {
                                        modelx = i;
                                        modely = j;
                                        break;
                                    }
                                }
                                
                                if (hitPosition.hit) break;
                            }
                        }  
                        
                        if (hitPosition.hit)
                        {
                            terrainCells.selection.push_back(Vector2{modelx, modely}); // first cell is whichever one the player begins in
                            FillTerrainCells(terrainCells, models);
                            
                            camera.position = Vector3{hitPosition.position.x, hitPosition.position.y + playerEyesHeight, hitPosition.position.z};
                            SetCameraMode(camera, CAMERA_CUSTOM);
                            camera.fovy = 65.0f;
                            characterMode = true;
                            DisableCursor();
                        }
                    }
                }
                else
                {
                    timeCounter = 0;
                    
                    if (updateFlag)
                    {
                        for (int i = 0; i < modelSelection.expandedSelection.size(); i++)
                        {
                            UpdateHeightmap(models[modelSelection.expandedSelection[i].x][modelSelection.expandedSelection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                        }
                        
                        updateFlag = false;                
                    }
                }
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z) && stepIndex > 0 && !history.empty()) // undo key
            {
                if (stepIndex == (int)history.size() && historyFlag) // if the last edit was not done by an undo operation, create a new entry saving the current state before reversing
                {
                    std::vector<Vector2> modelCoords = GetModelCoordsSelection(history[history.size() - 1]); // get the model selection of the last entry to use for this new entry
                    
                    HistoryUpdate(history, models, modelCoords, stepIndex, maxSteps);
                    historyFlag = false; // since the last edit will now have been done by a history operation, mark historyFlag false
                    
                    stepIndex--;
                }
                
                for (int i = 0; i < history[stepIndex - 1].size(); i++) // reinstate the previous state of the mesh as recorded by the history at stepIndex - 1
                {
                    models[history[stepIndex - 1][i].coords.x][history[stepIndex - 1][i].coords.y].meshes[0].vertices[history[stepIndex - 1][i].vertexIndex + 1] = history[stepIndex - 1][i].y; 
                }
                
                if (stepIndex > 0) 
                    stepIndex--;
                
                for (int i = 0; i < models.size(); i++)
                {
                    for (int j = 0; j < models[i].size(); j++)
                    {
                        rlUpdateBuffer(models[i][j].meshes[0].vboId[0], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                        rlUpdateBuffer(models[i][j].meshes[0].vboId[2], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
               
                        UpdateHeightmap(models[i][j], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                    }
                }           
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_X) && stepIndex < (int)history.size() - 1) //redo key
            {
                for (int i = 0; i < history[stepIndex + 1].size(); i++)
                {
                    models[history[stepIndex + 1][i].coords.x][history[stepIndex + 1][i].coords.y].meshes[0].vertices[history[stepIndex + 1][i].vertexIndex + 1] = history[stepIndex + 1][i].y;
                }           

                stepIndex++;
                
                if (stepIndex == history.size() - 1 && !historyFlag) // if redo moves back to the end of history, remove the last entry, otherwise the next entry made in history will be a copy 
                {
                    history.pop_back();
                    historyFlag = true;
                }
                
                for (int i = 0; i < models.size(); i++)
                {
                    for (int j = 0; j < models[i].size(); j++)
                    {
                        rlUpdateBuffer(models[i][j].meshes[0].vboId[0], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                        rlUpdateBuffer(models[i][j].meshes[0].vboId[2], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
               
                        UpdateHeightmap(models[i][j], modelVertexWidth, modelVertexHeight, highestY, lowestY); 
                    }
                }                 
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D) && !vertexSelection.empty())
            {
                vertexSelection.clear();
            }
            
            switch (inputFocus)
            {
                case InputFocus::NONE:
                {
                    break;
                }
                case InputFocus::X_MESH:
                {
                    int key = GetKeyPressed();
                    
                    if ((key >= 48) && (key <= 57) && ((int)xMeshString.size() < 5))
                    {
                        if (key == 48 && xMeshString.empty())
                        {}
                        else
                            xMeshString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !xMeshString.empty())
                        xMeshString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE; 
                    
                    break;
                }
                case InputFocus::Z_MESH:
                {
                    int key = GetKeyPressed();
                    
                    if ((key >= 48) && (key <= 57) && ((int)zMeshString.size() < 5))
                    {
                        if (key == 48 && zMeshString.empty())
                        {}
                        else
                            zMeshString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !zMeshString.empty())
                        zMeshString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE;
                    
                    break;
                }
                case InputFocus::X_MESH_SELECT:
                {
                    int key = GetKeyPressed();
                
                    if ((key >= 48) && (key <= 57) && ((int)xMeshSelectString.size() < 5))
                    {
                        if (key == 48 && xMeshSelectString.empty())
                        {}
                        else
                            xMeshSelectString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !xMeshSelectString.empty())
                        xMeshSelectString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE;
                    
                    break;
                }
                case InputFocus::Z_MESH_SELECT:
                {
                    int key = GetKeyPressed();
                
                    if ((key >= 48) && (key <= 57) && ((int)zMeshSelectString.size() < 5))
                    {
                        if (key == 48 && zMeshSelectString.empty())
                        {}
                        else
                            zMeshSelectString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !zMeshSelectString.empty())
                        zMeshSelectString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE;

                    break;
                }
                case InputFocus::STAMP_ANGLE:
                {
                    int key = GetKeyPressed();
                
                    if ((key >= 48) && (key <= 57) && ((int)stampAngleString.size() < 5))
                    {
                        if (key == 48 && stampAngleString.empty())
                        {}
                        else
                            stampAngleString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !stampAngleString.empty())
                        stampAngleString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (stampAngleString.empty())
                            inputFocus = InputFocus::NONE;
                        else
                        {
                            stampAngle = stof(stampAngleString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
                case InputFocus::STAMP_HEIGHT:
                {
                    int key = GetKeyPressed();
                    
                    if (key == 46 || ((key >= 48) && (key <= 57)) && ((int)stampHeightString.size() < 5))
                    {
                        stampHeightString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !stampHeightString.empty())
                        stampHeightString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (stampHeightString.empty())
                            inputFocus = InputFocus::NONE; 
                        else
                        {
                            stampHeight = stof(stampHeightString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
                case InputFocus::TOOL_STRENGTH:
                {
                    int key = GetKeyPressed();
                    
                    if (key == 46 || ((key >= 48) && (key <= 57)) && ((int)toolStrengthString.size() < 5))
                    {
                        toolStrengthString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !toolStrengthString.empty())
                        toolStrengthString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (toolStrengthString.empty())
                            inputFocus = InputFocus::NONE;
                        else
                        {
                            toolStrength = stof(toolStrengthString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
                case InputFocus::SELECT_RADIUS:
                {
                    int key = GetKeyPressed();
                    
                    if (key == 46 || ((key >= 48) && (key <= 57)) && ((int)selectRadiusString.size() < 10))
                    {
                        selectRadiusString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !selectRadiusString.empty())
                        selectRadiusString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (selectRadiusString.empty())
                            inputFocus = InputFocus::NONE;
                        else
                        {
                            selectRadius = stof(selectRadiusString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
                case InputFocus::SAVE_MESH:
                {
                    int key = GetKeyPressed();
                
                    if ((key >= 32) && (key <= 126) && ((int)saveMeshString.size() < 30))
                    {
                        saveMeshString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !saveMeshString.empty())
                        saveMeshString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE;

                    break;
                }
                case InputFocus::LOAD_MESH:
                {
                    int key = GetKeyPressed();
                
                    if ((key >= 32) && (key <= 126) && ((int)loadMeshString.size() < 30))
                    {
                        loadMeshString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !loadMeshString.empty())
                        loadMeshString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE;

                    break;
                }
                case InputFocus::SAVE_MESH_HEIGHT:
                {
                    int key = GetKeyPressed();
                    
                    if ((key >= 48 && key <= 57 || key == 46) && ((int)saveHeightString.size() < 10))
                    {
                        if (key == 48 && saveHeightString.empty())
                        {}
                        else
                            saveHeightString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !saveHeightString.empty())
                        saveHeightString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE; 
                    
                    break;
                }
                case InputFocus::LOAD_MESH_HEIGHT:
                {
                    int key = GetKeyPressed();
                    
                    if ((key >= 48 && key <= 57 || key == 46) && ((int)loadHeightString.size() < 10))
                    {
                        if (key == 48 && loadHeightString.empty())
                        {}
                        else
                            loadHeightString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !loadHeightString.empty())
                        loadHeightString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE; 
                    
                    break;
                }
                case InputFocus::DIRECTORY:
                {
                    int key = GetKeyPressed();
                
                    if ((key >= 32) && (key <= 126))
                    {
                        directoryString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !directoryString.empty())
                        directoryString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                        inputFocus = InputFocus::NONE;

                    break;
                }
            }
            
    /**********************************************************************************************************************************************************************
        DRAWING
    **********************************************************************************************************************************************************************/
            
            BeginDrawing();
            
                ClearBackground(Color{240, 240, 240, 255});
                
                BeginMode3D(camera);
                
                    if (!models.empty())
                    {
                        for (int i = 0; i < models.size(); i++)
                        {
                            for (int j = 0; j < models[i].size(); j++)
                            {
                                bool selected = false;
                                
                                for (int k = 0; k < modelSelection.selection.size(); k++) // if the model isn't selected, gray it out
                                {
                                    if (i == modelSelection.selection[k].x && j == modelSelection.selection[k].y)
                                    {
                                        selected = true;
                                        break;
                                    }
                                }
                                
                                if (selected)
                                    DrawModel(models[i][j], Vector3{0, 0, 0}, 1.0f, WHITE);
                                else
                                    DrawModel(models[i][j], Vector3{0, 0, 0}, 1.0f, DARKGRAY);
                            }
                        }
                    }
                    
                    DrawGrid(100, 1.0f);
                    
                    if(hitPosition.hit)
                    {
                        if (brush == BrushTool::SELECT)
                        {
                            int increment = (displayedVertices.size() / 500) + 1; // cull some vertex draw calls with larger selections
                            
                             // draw every highlighted vertex
                            for (int i = 0; i < displayedVertices.size(); i += increment)
                            {
                                DrawCube(displayedVertices[i], 0.03f, 0.03f, 0.03f, YELLOW);
                            }
                        }
                        else
                        {
                            float cylinderHeight = 0;
                            
                            for (int i = 0; i < displayedVertices.size(); i++)
                            {
                                if (displayedVertices[i].y > cylinderHeight)
                                    cylinderHeight = displayedVertices[i].y;
                            }
                            
                            DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius, selectRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 20});
                            DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius, selectRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 40});                       
                        }
                    }
                    
                    if (!vertexSelection.empty()) // draw every selected vertex
                    {
                        Color vertexColor;
                        
                        if (selectionMask)
                            vertexColor = RED;
                        else
                            vertexColor = YELLOW;
                        
                        int increment = ((int)vertexSelection.size() / 500) + 1; // cull some vertex draw calls with larger selections
                        
                        for (size_t i = 0; i < vertexSelection.size(); i += increment)
                        {
                            Vector3 v = {models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].vertexIndex], models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].vertexIndex + 1], models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].vertexIndex + 2]};
                            
                            DrawCube(v, 0.03f, 0.03f, 0.03f, vertexColor);
                        }
                    }
                    
                EndMode3D();
                
                /* debug text
                float temp = canvasHeight;//history.size();
                float temp2 = canvasWidth;//stepIndex;
                
                DrawText(FormatText("%f", temp), 200, 10, 15, BLACK);
                DrawText(FormatText("%f", temp2), 165, 10, 15, BLACK);
                
                
                if (!models.empty())
                {
                int temp = (int)modelSelection.expandedSelection.size();//(modelVertexWidth * (int)models.size() - ((int)models.size() - 1)) * (modelVertexHeight * (int)models[0].size() - ((int)models[0].size() - 1));
                DrawText(FormatText("%i", temp), 200, 10, 15, BLACK);
                DrawFPS(windowWidth - 30, 8);
                }
                */
                DrawFPS(windowWidth - 30, 8);
                DrawRectangleRec(UI, Color{200, 200, 200, 50});
                
                Color panelColor = {200, 200, 200, 150};
                
                switch(panel)
                {
                    case Panel::NONE:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Heightmap", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        break;
                    }
                    case Panel::HEIGHTMAP:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Heightmap", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(exportButton, GRAY);
                        DrawRectangleRec(meshGenButton, GRAY);
                        DrawRectangleRec(loadButton, GRAY);
                        DrawRectangleRec(updateTextureButton, GRAY);
                        DrawRectangleRec(directoryButton, GRAY);
                        DrawTextRec(GetFontDefault(), "Update Texture", Rectangle {updateTextureButton.x + 5, updateTextureButton.y + 1, updateTextureButton.width - 2, updateTextureButton.height - 2}, 15, 0.5f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Generate Mesh", Rectangle {meshGenButton.x + 5, meshGenButton.y + 1, meshGenButton.width - 2, meshGenButton.height - 2}, 15, 0.5f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Export Heightmap", Rectangle {exportButton.x + 5, exportButton.y + 1, exportButton.width - 2, exportButton.height - 2}, 15, 0.5f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Load Heightmap", Rectangle {loadButton.x + 5, loadButton.y + 1, loadButton.width - 2, loadButton.height - 2}, 15, 0.5f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Change Directory", Rectangle {directoryButton.x + 5, directoryButton.y + 1, directoryButton.width - 2, directoryButton.height - 2}, 15, 0.5f, true, BLACK);
                        
                        DrawRectangleRec(xMeshBox, WHITE);
                        DrawRectangleRec(zMeshBox, WHITE);
                        
                        DrawText("X:", 14, 33, 15, BLACK);
                        DrawText("Y:", 14, 58, 15, BLACK);
                        
                        if (inputFocus == InputFocus::X_MESH)
                            DrawRectangleLinesEx(xMeshBox, 1, BLACK);
                        else if (inputFocus == InputFocus::Z_MESH)
                            DrawRectangleLinesEx(zMeshBox, 1, BLACK);
                        
                        if (inputFocus == InputFocus::X_MESH || !xMeshString.empty())     // if the box is selected or the string isnt empty, then display xMeshString
                        {
                            char text[xMeshString.size() + 1];
                            strcpy(text, xMeshString.c_str());                        
                            
                            DrawTextRec(GetFontDefault(), text, Rectangle {xMeshBox.x + 3, xMeshBox.y + 3, xMeshBox.width - 2, xMeshBox.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else        // otherwise display the canvas width
                        {
                            std::string s = std::to_string(canvasWidth);
                            char c[s.size() + 1];
                            strcpy(c, s.c_str());
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {xMeshBox.x + 3, xMeshBox.y + 3, xMeshBox.width - 2, xMeshBox.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        
                        if (inputFocus == InputFocus::Z_MESH || !zMeshString.empty())     // if the box is selected or the string isnt empty, then display zMeshString
                        {
                            char c[zMeshString.size() + 1];
                            strcpy(c, zMeshString.c_str());                        
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {zMeshBox.x + 3, zMeshBox.y + 3, zMeshBox.width - 2, zMeshBox.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else        // otherwise display the canvas height
                        {
                            std::string s = std::to_string(canvasHeight);
                            char c[s.size() + 1];
                            strcpy(c, s.c_str());
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {zMeshBox.x + 3, zMeshBox.y + 3, zMeshBox.width - 2, zMeshBox.height - 2}, 15, 0.5f, false, BLACK);
                        }                    
                        
                        break;
                    }
                    case Panel::CAMERA:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Heightmap", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(characterButton, GRAY);
                        
                        if (!characterDrag) // dont draw the circle in the rectangle if it's being dragged
                            DrawCircle(characterButton.x + (characterButton.width / 2 + 1), characterButton.y + (characterButton.height / 2 + 1), characterButton.width / 2 - 4, ORANGE);
                        
                        break;
                    }
                    case Panel::TOOLS:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Heightmap", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 0.5f, false, BLACK);
                        
                        DrawRectangleRec(panel3, panelColor);
                        
                        switch (brush) // highlight the selected tool
                        {
                            case BrushTool::NONE:
                                break;
                            case BrushTool::ELEVATION:
                                DrawRectangle(elevToolButton.x - 7, elevToolButton.y - 7, elevToolButton.width + 14, elevToolButton.height + 14, WHITE);
                                break;
                            case BrushTool::SMOOTH:
                                DrawRectangle(smoothToolButton.x - 7, smoothToolButton.y - 7, smoothToolButton.width + 14, smoothToolButton.height + 14, WHITE);
                                break;
                            case BrushTool::FLATTEN:
                                DrawRectangle(flattenToolButton.x - 7, flattenToolButton.y - 7, flattenToolButton.width + 14, flattenToolButton.height + 14, WHITE);
                                break;
                            case BrushTool::STAMP:
                                DrawRectangle(stampToolButton.x - 7, stampToolButton.y - 7, stampToolButton.width + 14, stampToolButton.height + 14, WHITE);
                                break;
                            case BrushTool::SELECT:
                                DrawRectangle(selectToolButton.x - 7, selectToolButton.y - 7, selectToolButton.width + 14, smoothToolButton.height + 14, WHITE);
                                break;
                        }
                        
                        DrawRectangleRec(elevToolButton, RED);
                        DrawRectangleRec(smoothToolButton, BLUE);
                        DrawRectangleRec(flattenToolButton, ORANGE);
                        DrawRectangleRec(stampToolButton, VIOLET);
                        DrawRectangleRec(selectToolButton, GREEN);
                        DrawRectangleRec(toolText, WHITE);
                        
                        // check what text should be displayed in the toolText
                        if (CheckCollisionPointRec(mousePosition, elevToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Elevation", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, smoothToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Smooth", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, flattenToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Flatten", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, stampToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Stamp", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, selectToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Select", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (brush == BrushTool::ELEVATION)
                        {
                            DrawTextRec(GetFontDefault(), "Elevation", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (brush == BrushTool::SMOOTH)
                        {
                            DrawTextRec(GetFontDefault(), "Smooth", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (brush == BrushTool::FLATTEN)
                        {
                            DrawTextRec(GetFontDefault(), "Flatten", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (brush == BrushTool::STAMP)
                        {
                            DrawTextRec(GetFontDefault(), "Stamp", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        else if (brush == BrushTool::SELECT)
                        {
                            DrawTextRec(GetFontDefault(), "Select", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        
                        if (brush == BrushTool::SELECT)
                        {
                            if (selectionMask)
                                DrawRectangleRec(selectionMaskButton, RED);
                            else
                                DrawRectangleRec(selectionMaskButton, GRAY);
                            
                            DrawRectangleRec(deselectButton, GRAY);
                            DrawRectangleRec(trailToolButton, GRAY);
                            DrawTextRec(GetFontDefault(), "Deselect", Rectangle {deselectButton.x + 3, deselectButton.y + 3, deselectButton.width - 2, deselectButton.height - 2}, 15, 0.5f, false, BLACK);
                            DrawTextRec(GetFontDefault(), "Trail Tool", Rectangle {trailToolButton.x + 3, trailToolButton.y + 3, trailToolButton.width - 2, trailToolButton.height - 2}, 15, 0.5f, false, BLACK);
                            DrawTextRec(GetFontDefault(), "Selection Mask", Rectangle {selectionMaskButton.x + 3, selectionMaskButton.y + 3, selectionMaskButton.width - 2, selectionMaskButton.height - 2}, 15, 0.5f, false, BLACK);
                        }
                        
                        if (brush == BrushTool::STAMP)
                        {
                            DrawRectangleRec(stampAngleBox, WHITE);
                            DrawRectangleRec(stampHeightBox, WHITE);
                            
                            if (inputFocus == InputFocus::STAMP_ANGLE)
                                DrawRectangleLinesEx(stampAngleBox, 1, BLACK);
                            else if (inputFocus == InputFocus::STAMP_HEIGHT)
                                DrawRectangleLinesEx(stampHeightBox, 1, BLACK);
                            
                            if (inputFocus == InputFocus::STAMP_ANGLE || !stampAngleString.empty())     // if the box is selected or the string isnt empty, then display stampAngleString
                            {
                                char c[stampAngleString.size() + 1];
                                strcpy(c, stampAngleString.c_str());                        
                                
                                DrawTextRec(GetFontDefault(), c, Rectangle {stampAngleBox.x + 3, stampAngleBox.y + 3, stampAngleBox.width - 2, stampAngleBox.height - 2}, 12, 0.5f, false, BLACK);
                            }
                            else       // otherwise display the stamp angle
                            {
                                std::string s = std::to_string(stampAngle);
                                char c[s.size() + 1];
                                strcpy(c, s.c_str());
                                
                                DrawTextRec(GetFontDefault(), c, Rectangle {stampAngleBox.x + 3, stampAngleBox.y + 3, stampAngleBox.width - 2, stampAngleBox.height - 2}, 12, 0.5f, false, BLACK);
                            }
                            
                            if (inputFocus == InputFocus::STAMP_HEIGHT || !stampHeightString.empty())     // if the box is selected or the string isnt empty, then display stampHeightString
                            {
                                char c[stampHeightString.size() + 1];
                                strcpy(c, stampHeightString.c_str());                        
                                
                                DrawTextRec(GetFontDefault(), c, Rectangle {stampHeightBox.x + 3, stampHeightBox.y + 3, stampHeightBox.width - 2, stampHeightBox.height - 2}, 12, 0.5f, false, BLACK);
                            }
                            else       // otherwise display the stamp height
                            {
                                std::string s = std::to_string(stampHeight);
                                char c[s.size() + 1];
                                strcpy(c, s.c_str());
                                
                                DrawTextRec(GetFontDefault(), c, Rectangle {stampHeightBox.x + 3, stampHeightBox.y + 3, stampHeightBox.width - 2, stampHeightBox.height - 2}, 12, 0.5f, false, BLACK);
                            }
                        }
                        
                        DrawLine(6, 180, 95, 180, BLACK);
                        
                        DrawRectangleRec(xMeshSelectBox, WHITE);
                        DrawRectangleRec(zMeshSelectBox, WHITE);
                        DrawRectangleRec(meshSelectButton, GRAY);
                        DrawRectangleRec(meshSelectUpButton, GRAY);
                        DrawRectangleRec(meshSelectLeftButton, GRAY);
                        DrawRectangleRec(meshSelectRightButton, GRAY);
                        DrawRectangleRec(meshSelectDownButton, GRAY);
                        
                        DrawRectangleRec(selectRadiusBox, WHITE);
                        DrawRectangleRec(toolStrengthBox, WHITE);
                        
                        DrawText("X:", meshSelectAnchor.x + 5, meshSelectAnchor.y + 12, 15, BLACK);
                        DrawText("Y:", meshSelectAnchor.x + 53, meshSelectAnchor.y + 12, 15, BLACK);
                        DrawTextRec(GetFontDefault(), "Mesh Selection", Rectangle {meshSelectButton.x + 3, meshSelectButton.y + 3, meshSelectButton.width - 2, meshSelectButton.height - 2}, 15, 0.5f, false, BLACK);
                        DrawText("Radius:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 56, 11, BLACK);
                        DrawText("Strength:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 31, 11, BLACK);
                        
                        if (inputFocus == InputFocus::X_MESH_SELECT)
                            DrawRectangleLinesEx(xMeshSelectBox, 1, BLACK);
                        else if (inputFocus == InputFocus::Z_MESH_SELECT)
                            DrawRectangleLinesEx(zMeshSelectBox, 1, BLACK);
                        else if (inputFocus == InputFocus::TOOL_STRENGTH)
                            DrawRectangleLinesEx(toolStrengthBox, 1, BLACK);
                        else if (inputFocus == InputFocus::SELECT_RADIUS)
                            DrawRectangleLinesEx(selectRadiusBox, 1, BLACK);
                            
                        if (inputFocus == InputFocus::X_MESH_SELECT || !xMeshSelectString.empty())     // if the box is selected or the string isnt empty, then display xMeshSelectString
                        {
                            char c[xMeshSelectString.size() + 1];
                            strcpy(c, xMeshSelectString.c_str());                        
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {xMeshSelectBox.x + 3, xMeshSelectBox.y + 3, xMeshSelectBox.width - 2, xMeshSelectBox.height - 2}, 12, 0.5f, false, BLACK);
                        }
                        else        // otherwise display the selection width
                        {
                            std::string s = std::to_string(modelSelection.width);
                            char c[s.size() + 1];
                            strcpy(c, s.c_str());
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {xMeshSelectBox.x + 3, xMeshSelectBox.y + 3, xMeshSelectBox.width - 2, xMeshSelectBox.height - 2}, 12, 0.5f, false, BLACK);
                        }
                        
                        if (inputFocus == InputFocus::Z_MESH_SELECT || !zMeshSelectString.empty())     // if the box is selected or the string isnt empty, then display zMeshSelectString
                        {
                            char c[zMeshSelectString.size() + 1];
                            strcpy(c, zMeshSelectString.c_str());                        
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {zMeshSelectBox.x + 3, zMeshSelectBox.y + 3, zMeshSelectBox.width - 2, zMeshSelectBox.height - 2}, 12, 0.5f, false, BLACK);
                        }
                        else       // otherwise display the selection height
                        {
                            std::string s = std::to_string(modelSelection.height);
                            char c[s.size() + 1];
                            strcpy(c, s.c_str());
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {zMeshSelectBox.x + 3, zMeshSelectBox.y + 3, zMeshSelectBox.width - 2, zMeshSelectBox.height - 2}, 12, 0.5f, false, BLACK);
                        }
                        
                        if (inputFocus == InputFocus::TOOL_STRENGTH || !toolStrengthString.empty())     // if the box is selected or the string isnt empty, then display toolStrengthString
                        {
                            char c[toolStrengthString.size() + 1];
                            strcpy(c, toolStrengthString.c_str());                        
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {toolStrengthBox.x + 3, toolStrengthBox.y + 4, toolStrengthBox.width - 2, toolStrengthBox.height - 2}, 11, 0.5f, false, BLACK);
                        }
                        else       // otherwise display the selection tool strength
                        {
                            std::string s = std::to_string(toolStrength);
                            char c[s.size() + 1];
                            strcpy(c, s.c_str());
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {toolStrengthBox.x + 3, toolStrengthBox.y + 4, toolStrengthBox.width - 2, toolStrengthBox.height - 2}, 11, 0.5f, false, BLACK);
                        }
                        
                        if (inputFocus == InputFocus::SELECT_RADIUS || !selectRadiusString.empty())     // if the box is selected or the string isnt empty, then display selectRadiusString
                        {
                            char c[selectRadiusString.size() + 1];
                            strcpy(c, selectRadiusString.c_str());                        
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {selectRadiusBox.x + 3, selectRadiusBox.y + 4, selectRadiusBox.width - 2, selectRadiusBox.height - 2}, 11, 0.5f, false, BLACK);
                        }
                        else       // otherwise display the selection select radius
                        {
                            std::string s = std::to_string(selectRadius);
                            char c[s.size() + 1];
                            strcpy(c, s.c_str());
                            
                            DrawTextRec(GetFontDefault(), c, Rectangle {selectRadiusBox.x + 3, selectRadiusBox.y + 4, selectRadiusBox.width - 2, selectRadiusBox.height - 2}, 11, 0.5f, false, BLACK);
                        }
                        
                        DrawTriangle(Vector2{meshSelectAnchor.x + 58, meshSelectAnchor.y + 80}, Vector2{meshSelectAnchor.x + 51, meshSelectAnchor.y + 68}, Vector2{meshSelectAnchor.x + 43, meshSelectAnchor.y + 80}, BLACK);
                        DrawTriangle(Vector2{meshSelectAnchor.x + 33, meshSelectAnchor.y + 92}, Vector2{meshSelectAnchor.x + 33, meshSelectAnchor.y + 79}, Vector2{meshSelectAnchor.x + 18, meshSelectAnchor.y + 85}, BLACK);
                        DrawTriangle(Vector2{meshSelectAnchor.x + 84, meshSelectAnchor.y + 85}, Vector2{meshSelectAnchor.x + 68, meshSelectAnchor.y + 79}, Vector2{meshSelectAnchor.x + 68, meshSelectAnchor.y + 92}, BLACK);
                        DrawTriangle(Vector2{meshSelectAnchor.x + 51, meshSelectAnchor.y + 105}, Vector2{meshSelectAnchor.x + 58, meshSelectAnchor.y + 92}, Vector2{meshSelectAnchor.x + 43, meshSelectAnchor.y + 92}, BLACK);
                        
                        break;
                    }
                }
                
                if (showSaveWindow)
                {
                    DrawRectangleRec(saveWindow, LIGHTGRAY);
                    DrawRectangleRec(saveWindowSaveButton, GRAY);
                    DrawRectangleRec(saveWindowCancelButton, GRAY);
                    DrawRectangleRec(saveWindowTextBox, WHITE);
                    DrawRectangleRec(saveWindowHeightBox, WHITE);
                    
                    DrawTextRec(GetFontDefault(), "Save", Rectangle {saveWindowSaveButton.x + 3, saveWindowSaveButton.y + 3, saveWindowSaveButton.width - 2, saveWindowSaveButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawTextRec(GetFontDefault(), "Cancel", Rectangle {saveWindowCancelButton.x + 3, saveWindowCancelButton.y + 3, saveWindowCancelButton.width - 2, saveWindowCancelButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawText("Save heightmap as .png:", saveWindowAnchor.x + 6, saveWindowAnchor.y + 12, 17, BLACK);
                    DrawText("Reference height (for scale):", saveWindowAnchor.x + 6, saveWindowAnchor.y + 87, 17, BLACK);
                   
                    char text[saveMeshString.size() + 1];
                    strcpy(text, saveMeshString.c_str());  
                    DrawTextRec(GetFontDefault(), text, Rectangle {saveWindowTextBox.x + 3, saveWindowTextBox.y + 3, saveWindowTextBox.width - 2, saveWindowTextBox.height - 2}, 15, 0.5f, false, BLACK);
                    
                    if (saveHeightString.empty() && inputFocus != InputFocus::SAVE_MESH_HEIGHT)
                    {
                        std::string tempString = std::to_string(highestY);
                        char text[tempString.size() + 1];
                        strcpy(text, tempString.c_str());  
                        DrawTextRec(GetFontDefault(), text, Rectangle {saveWindowHeightBox.x + 3, saveWindowHeightBox.y + 3, saveWindowHeightBox.width - 2, saveWindowHeightBox.height - 2}, 15, 0.5f, false, BLACK);
                    }
                    else
                    {
                        char text[saveHeightString.size() + 1];
                        strcpy(text, saveHeightString.c_str());  
                        DrawTextRec(GetFontDefault(), text, Rectangle {saveWindowHeightBox.x + 3, saveWindowHeightBox.y + 3, saveWindowHeightBox.width - 2, saveWindowHeightBox.height - 2}, 15, 0.5f, false, BLACK);
                    }
                    
                    if (inputFocus == InputFocus::SAVE_MESH)
                    {
                        DrawRectangleLinesEx(saveWindowTextBox, 1, BLACK);
                    }
                    else if (inputFocus == InputFocus::SAVE_MESH_HEIGHT)
                    {
                        DrawRectangleLinesEx(saveWindowHeightBox, 1, BLACK);
                    }
                }
                else if (showLoadWindow)
                {
                    DrawRectangleRec(loadWindow, LIGHTGRAY);
                    DrawRectangleRec(loadWindowLoadButton, GRAY);
                    DrawRectangleRec(loadWindowCancelButton, GRAY);
                    DrawRectangleRec(loadWindowTextBox, WHITE);
                    DrawRectangleRec(loadWindowHeightBox, WHITE);
                    
                    DrawTextRec(GetFontDefault(), "Load", Rectangle {loadWindowLoadButton.x + 3, loadWindowLoadButton.y + 3, loadWindowLoadButton.width - 2, loadWindowLoadButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawTextRec(GetFontDefault(), "Cancel", Rectangle {loadWindowCancelButton.x + 3, loadWindowCancelButton.y + 3, loadWindowCancelButton.width - 2, loadWindowCancelButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawText("Load heightmap (png):", loadWindowAnchor.x + 6, loadWindowAnchor.y + 12, 17, BLACK);
                    DrawText("Pure white height value (for scale):", loadWindowAnchor.x + 6, loadWindowAnchor.y + 87, 17, BLACK);
                    
                    char text[loadMeshString.size() + 1];
                    strcpy(text, loadMeshString.c_str());  
                    DrawTextRec(GetFontDefault(), text, Rectangle {loadWindowTextBox.x + 3, loadWindowTextBox.y + 3, loadWindowTextBox.width - 2, loadWindowTextBox.height - 2}, 15, 0.5f, false, BLACK);
                    
                    char text2[loadHeightString.size() + 1];
                    strcpy(text2, loadHeightString.c_str());  
                    DrawTextRec(GetFontDefault(), text2, Rectangle {loadWindowHeightBox.x + 3, loadWindowHeightBox.y + 3, loadWindowHeightBox.width - 2, loadWindowHeightBox.height - 2}, 15, 0.5f, false, BLACK);
                    
                    if (inputFocus == InputFocus::LOAD_MESH)
                    {
                        DrawRectangleLinesEx(loadWindowTextBox, 1, BLACK);
                    }
                    else if (inputFocus == InputFocus::LOAD_MESH_HEIGHT)
                    {
                        DrawRectangleLinesEx(loadWindowHeightBox, 1, BLACK);
                    }
                }
                else if (showDirWindow)
                {
                    DrawRectangleRec(dirWindow, LIGHTGRAY);
                    DrawRectangleRec(dirWindowOkayButton, GRAY);
                    DrawRectangleRec(dirWindowCancelButton, GRAY);
                    DrawRectangleRec(dirWindowTextBox, WHITE);
                    
                    DrawTextRec(GetFontDefault(), "Okay", Rectangle {dirWindowOkayButton.x + 3, dirWindowOkayButton.y + 3, dirWindowOkayButton.width - 2, dirWindowOkayButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawTextRec(GetFontDefault(), "Cancel", Rectangle {dirWindowCancelButton.x + 3, dirWindowCancelButton.y + 3, dirWindowCancelButton.width - 2, dirWindowCancelButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawText("Change Directory (full file path):", dirWindowAnchor.x + 6, dirWindowAnchor.y + 12, 17, BLACK);
                    
                    char text[directoryString.size() + 1];
                    strcpy(text, directoryString.c_str());  
                    DrawTextRec(GetFontDefault(), text, Rectangle {dirWindowTextBox.x + 3, dirWindowTextBox.y + 3, dirWindowTextBox.width - 2, dirWindowTextBox.height - 2}, 15, 0.5f, false, BLACK);
                    
                    if (inputFocus == InputFocus::DIRECTORY)
                    {
                        DrawRectangleLinesEx(dirWindowTextBox, 1, BLACK);
                    }
                }
                
                if (characterDrag) // draw the character camera icon at the mouse cursor if it's being dragged
                {
                    DrawCircle(mousePosition.x, mousePosition.y, characterButton.width / 2 - 4, ORANGE);
                }
            
            EndDrawing();
        }
    }
    //UnloadModel(models[0]);
    
    CloseWindow();
    
    return 0;
}


// upside down cone brush
// selection by angle / normal
// mesh streaming
// incorporate delta time, GetFrameTime
// orthographic camera
// 2d paint mode (or find a way to not use ray trace while in ortho top down)
// expand / contract vertex selection
// selection copy paste
// camera go to functions
// scale selection
// saved selections
// reverse/redo history by x steps
// directory management, save/load
// incremented trail tool - 
// selective history deletion on mesh shrink
// mesh selection move by one time ray collision on desired center
// toggle render non selected models
// trail tool only raise / lower setting
// brush that takes the average of the normals of the first selection and then flattens out perpedicularly to that
// vertical line from hitPosition the height of highest y when on ground collision
// box selection
// make input boxes save current value, empty it to make room for input, but return the saved value if nothing changed. also make unfocusing = enter
// dynamic history capacity
// mesh interpolation
// stamp setting: change max height and tool radius proportionally over distance dragged
// setting that checks mouse hit position distances between frames and interpolates between them by queueing actions  
// set brush collision to current mesh selection, saving a copy of the meshes to test against
// precise export image size
// shading based on angle
// smooth tool angle clamp

// -crash after ~141 history steps (if 3x3+ mesh?)
// -4x4 mesh 4x4 selection, ~55 fps with collision detection. expand mesh to 6x6 and selection to 6x6 (or just more selection), then shrink selection back to 4x4, then ~38 fps with collision detection
// even if mesh shrunk back to 4x4
// -loading 2x3 image 'test' twice in a row crashes















// FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------------------------

float xzDistance(const Vector2 p1, const Vector2 p2)
{
    float side1 = abs(p1.x - p2.x);
    float side2 = abs(p1.y - p2.y);
    
    return sqrt((side1 * side1) + (side2 * side2));
}


void HistoryUpdate(std::vector<std::vector<VertexState>>& history, const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, int& stepIndex, const int maxSteps)
{
    //check for out of bounds models that have been deleted
    
    // if moving forward from a place in history before the last step, clear all subsequent steps
    if (stepIndex < (int)history.size() - 1)
    {
        history.erase(history.begin() + stepIndex, history.end());
    }
    
    // if history reaches max size, remove first element. (have it erase more than one so that reallocation happens less often?)
    if (history.size() == maxSteps)
    {
        history.erase(history.begin());
        stepIndex = history.size() - 1;
    }

    std::vector<VertexState> state;
    
    for (int i = 0; i < modelCoords.size(); i++) // go through each of the models 
    {
        for (int j = 0; j < (models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // save each vertex's data
        {
           VertexState temp;
           temp.vertexIndex = j;
           temp.coords = Vector2{modelCoords[i].x, modelCoords[i].y};
           temp.y = models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[j+1];
           
           state.push_back(temp);
        }
    }
    
    history.push_back(state);
    stepIndex = history.size(); // following an edit, stepIndex is one past the last index
}


Color* GenHeightmapSelection(const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, const int modelVertexWidth, const int modelVertexHeight)
{
    int numOfModels = (int)modelCoords.size(); // find the number of models being used
    
    Color* pixels = (Color*)RL_MALLOC(numOfModels*modelVertexWidth*modelVertexHeight*sizeof(Color));
    
    float highestY = 0; // highest and lowest points on the mesh
    float lowestY = 0;
    
    int eastX = modelCoords[0].x;  // lowest and highest values for the model x and y coordinates. initialize with the first element
    int westX = modelCoords[0].x;
    int northY = modelCoords[0].y;
    int southY = modelCoords[0].y;
    
    for (int i = 0; i < numOfModels; i++)
    {
        // find the most extraneous values
        if (modelCoords[i].x > eastX)
            eastX = modelCoords[i].x;
        
        if (modelCoords[i].x < westX)
            westX = modelCoords[i].x;
        
        if (modelCoords[i].y > northY)
            northY = modelCoords[i].y;
        
        if (modelCoords[i].y < southY)
            southY = modelCoords[i].y;
        
        for (int j = 0; j < (models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // find the the highest and lowest points on the mesh to use as reference for the scale
        {
            if (models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[j+1] > highestY)
                highestY = models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[j+1];
                
            if (models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[j+1] < lowestY)
                lowestY = models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[j+1];
        }
    }
    
    float scale = highestY - lowestY;    
    
    for (int i = 0; i < numOfModels; i++) // go through each model and enter its data into pixels. (doesnt rely on modelCoords being sorted)
    {
        int increment = 0;
        
        for (int j = 0; j < modelVertexWidth*modelVertexHeight; j++) // go through this model's vertices (one per intersection, not every overlapping one)
        {
            if (j%modelVertexWidth == 0) // every time it moves to a new row of vertices, it needs to make a jump where it's writing to in the pixel array
                increment += modelVertexWidth * (eastX - westX);
            
            int x = j - ((j / modelVertexWidth) * modelVertexWidth); // find the x and y of this vertex in the mesh
            int y = j / modelVertexWidth;
            
            int rowPixelCount = (eastX - westX + 1) * modelVertexWidth * modelVertexHeight; // number of pixels in a row of models
            
            int vertexIndex = GetVertexIndices(x, y, modelVertexWidth)[0]; // use x and y to get the actual index in the vertex array (only need one from the returned list)
            int pixelIndex = ((northY - modelCoords[i].y) * rowPixelCount) + ((modelCoords[i].x - westX) * modelVertexWidth) + increment; // index in the pixel array that cooresponds to this vertex
            unsigned char pixelValue = (abs((highestY - models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[vertexIndex+1]) - scale) / scale) * 255;
            
            pixels[pixelIndex].r = pixelValue;
            pixels[pixelIndex].g = pixelValue;
            pixels[pixelIndex].b = pixelValue;
            pixels[pixelIndex].a = 255;
            
            increment++;
        }
    }
    
    return pixels;
}


Color* GenHeightmap(const Model& model, const int modelVertexWidth, const int modelVertexHeight, float& highestY, float& lowestY, bool grayscale)
{
    Color* pixels = (Color*)RL_MALLOC((modelVertexWidth - 1)*(modelVertexHeight - 1)*sizeof(Color)); 
    
    for(int i = 0; i < (model.meshes[0].vertexCount * 3) - 2; i += 3) // find if there is a new highestY and/or lowestY
    {
        if (model.meshes[0].vertices[i+1] > highestY)
            highestY = model.meshes[0].vertices[i+1];
            
        if (model.meshes[0].vertices[i+1] < lowestY)
            lowestY = model.meshes[0].vertices[i+1];
    }
    
    float scale = highestY - lowestY;
    
    if (grayscale)
    {
        for (int i = 0; i < (modelVertexWidth - 1) * (modelVertexHeight - 1); i++)
        {
            unsigned char pixelValue = (abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 255;
            
            pixels[i].r = pixelValue;
            pixels[i].g = pixelValue; 
            pixels[i].b = pixelValue; 
            pixels[i].a = 255;           
        }
    }
    else
    {
        // if not grayscale, assign each pixel one of 1170 colors (cutting out some pink - red range), starting at purple and going up to red
        for (int i = 0; i < (modelVertexWidth - 1) * (modelVertexHeight - 1); i++)
        {
            float rgb = (fabs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 1170;
            
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
            pixels[i].a = 255;
            
            //red 
            if (rgb <= 150)
                pixels[i].r = 150 - rgb; 
            
            if (rgb > 660 && rgb <= 915)
                pixels[i].r = rgb - 660;
            
            if (rgb > 915)
                pixels[i].r = 255;
            //green
            if (rgb > 150 && rgb <= 405)
                pixels[i].g = rgb - 150;
            
            if (rgb > 405 && rgb <= 915)
                pixels[i].g = 255;
            
            if (rgb > 915)
                pixels[i].g = 255 - (rgb - 915);
            //blue
            if (rgb <= 405)
                pixels[i].b = 255;
            
            if (rgb > 405 && rgb <= 660)
                pixels[i].b = 255 - (rgb - 405);
        }
    }
    
    return pixels;
}


Color* GenHeightmap(const std::vector<std::vector<Model>>& models, const int modelVertexWidth, const int modelVertexHeight, float maxHeight, float minHeight)
{
    //(assumes modelVertexWidth and modelVertexHeight are actually the same)
    int modelsSizeX = (int)models.size(); // number of model columns
    int modelsSizeY = (int)models[0].size(); // number of model rows
    int numOfModels = modelsSizeX * modelsSizeY; // find the number of models being used
    int numOfPixels = (modelVertexWidth * modelsSizeX - (modelsSizeX - 1)) * (modelVertexHeight * modelsSizeY - (modelsSizeY - 1)); // number of pixels in the image
    
    Color* pixels = (Color*)RL_MALLOC(numOfPixels*sizeof(Color));
    
    if (minHeight > 0) // min height never above 0
        minHeight = 0;
    
    float scale = maxHeight - minHeight;    
    
    for (int i = 0; i < modelsSizeX; i++) // go through each model and enter its data into pixels. 
    {
        for (int j = 0; j < modelsSizeY; j++)
        {
            int increment = 0; // number of pixels needed to skip to get to the start of the next row of pixels these vertices coorespond to
            
            for (int k = 0; k < modelVertexWidth*modelVertexHeight; k++) // go through this model's vertices (one per intersection, not every overlapping one)
            {
                if (k > 0 && k%modelVertexWidth == 0) // every time it moves to a new row of vertices, it needs to make a jump where it's writing to in the pixel array
                    increment += modelVertexWidth * modelsSizeX - (modelsSizeX - 1) - modelVertexWidth; 
                
                int x = k - ((k / modelVertexWidth) * modelVertexWidth); // find the x and y of this vertex in the mesh
                int y = k / modelVertexWidth;
                
                int rowPixelCount = (modelVertexWidth * modelsSizeX - (modelsSizeX - 1)) * (modelVertexHeight - 1); // number of pixels to adjust pixelIndex by for each row 
                
                int vertexIndex = GetVertexIndices(x, y, modelVertexWidth)[0]; // use x and y to get the actual index in the vertex array (only need one from the returned list)
                int pixelIndex = j * rowPixelCount + (i * modelVertexWidth - i) + increment + k; // index in the pixel array that corresponds to this vertex
                
                unsigned char pixelValue = (abs((maxHeight - models[i][j].meshes[0].vertices[vertexIndex+1]) - scale) / scale) * 255;
                
                pixels[pixelIndex].r = pixelValue;
                pixels[pixelIndex].g = pixelValue;
                pixels[pixelIndex].b = pixelValue;
                pixels[pixelIndex].a = 255;
            }
        }
    }
    
    return pixels;
}


RayHitInfo GetCollisionRayModel2(Ray ray, const Model *model)
{
    RayHitInfo result = { 0 };

    for (int m = 0; m < model->meshCount; m++)
    {
        // Check if meshhas vertex data on CPU for testing
        if (model->meshes[m].vertices != NULL)
        {
            // model->mesh.triangleCount may not be set, vertexCount is more reliable
            int triangleCount = model->meshes[m].vertexCount/3;

            // Test against all triangles in mesh
            for (int i = 0; i < triangleCount; i++)
            {
                Vector3 a, b, c;
                Vector3 *vertdata = (Vector3 *)model->meshes[m].vertices;

                if (model->meshes[m].indices)
                {
                    a = vertdata[model->meshes[m].indices[i*3 + 0]];
                    b = vertdata[model->meshes[m].indices[i*3 + 1]];
                    c = vertdata[model->meshes[m].indices[i*3 + 2]];
                }
                else
                {
                    a = vertdata[i*3 + 0];
                    b = vertdata[i*3 + 1];
                    c = vertdata[i*3 + 2];
                }

                a = Vector3Transform(a, model->transform);
                b = Vector3Transform(b, model->transform);
                c = Vector3Transform(c, model->transform);

                RayHitInfo triHitInfo = GetCollisionRayTriangle(ray, a, b, c);

                if (triHitInfo.hit)
                {
                    // Save the closest hit triangle
                    if ((!result.hit) || (result.distance > triHitInfo.distance)) result = triHitInfo;
                }
            }
        }
    }

    return result;
}


void UpdateHeightmap(const Model& model, const int modelVertexWidth, const int modelVertexHeight, float& highestY, float& lowestY)
{
    Color* pixels = GenHeightmap(model, modelVertexWidth, modelVertexHeight, highestY, lowestY, 1);
        
    UpdateTexture(model.materials[0].maps[MAP_DIFFUSE].texture, pixels);
    
    RL_FREE(pixels);    
}


void UpdateHeightmap(const std::vector<std::vector<Model>>& models, const int modelVertexWidth, const int modelVertexHeight, float highestY, float lowestY)
{
    for (int i = 0; i < models.size(); i++)
    {
        for (int j = 0; j < models[i].size(); j++)
        {
            Color* pixels = GenHeightmap(models[i][j], modelVertexWidth, modelVertexHeight, highestY, lowestY, 1);
                
            UpdateTexture(models[i][j].materials[0].maps[MAP_DIFFUSE].texture, pixels);
            
            RL_FREE(pixels);            
        }
    }
}


std::vector<int> GetVertexIndices(const int x, const int y, const int width) 
{
    std::vector<int> result;
    
    if (x == 0)
    {
        if (y == 0)
        {
            result.push_back(0);
        }
        else if (y == width - 1)
        {
            result.push_back((y - 1) * ((width - 1) * 18) + 3);
            result.push_back((y - 1) * ((width - 1) * 18) + 12);
        }
        else
        {
            result.push_back(((y - 1) * ((width - 1) * 18)) + 3);
            result.push_back(((y - 1) * ((width - 1) * 18)) + 12);
            result.push_back(y * ((width - 1) * 18));   
        }
    }
    else if (x == width - 1)
    {
        if (y == 0)
        {
            result.push_back(((width - 1) * 18) - 9);
            result.push_back(((width - 1) * 18) - 12);
        }
        else if (y == width - 1)
        {
            result.push_back((y * (width - 1) * 18) - 3);          
        }
        else
        {
            result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) - 3);
            result.push_back((x * 18) + ((y * ((width - 1) * 18)) - 9)); 
            result.push_back((x * 18) + ((y * ((width - 1) * 18)) - 12));
        }        
    }
    else if (y == 0)
    {
        result.push_back(x * 18 - 9);
        result.push_back(x * 18 - 12); 
        result.push_back(x * 18);
    }
    else if (y == width - 1)
    {
        result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) - 3);
        result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) + 3);
        result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) + 12);     
    }
    else
    {
        result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) - 3);
        result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) + 3);
        result.push_back((x * 18) + ((y - 1) * ((width - 1) * 18)) + 12);
        result.push_back((x * 18) + ((y * (width - 1) * 18)) - 12);
        result.push_back((x * 18) + ((y * (width - 1) * 18)) - 9);
        result.push_back((x * 18) + (y * (width - 1) * 18));
    }
    
    return result;
}

// meshes are patterns of pairs of tris. divide into and count by low resolution squares, then use piecemeal rules for final precision if there is a remainder
Vector2 GetVertexCoords(const int index, const int width) 
{
    Vector2 result;
    
    int squares = (index + 1) / 18;
    int remainder = (index + 1) - (squares * 18);
    
    result.y = squares / (width - 1);
    result.x = squares - (result.y * (width - 1));
    
    if (result.x == 0 && remainder == 0)  // any time x = 0 without remainder, it needs to be corrected
        result.x = width - 1;
    
    if (result.y == width - 1) // the only time result.y will be at the max is if the coord is the bottom right, so return result
        return result;
    
    // all checks below see if it's necessary to add one to x and/or y because of vertex position in the square (set of two tris)
    if ((remainder > 3 && remainder < 7) || (remainder > 12 && remainder < 16))
    {
        result.y++;
    }
    else if (remainder > 6 && remainder < 13)
    {
        result.x++;
    }
    else if (remainder > 15 && remainder < 19)
    {
        result.y++;
        result.x++;
    }
    
    return result;
}


std::vector<Vector2> GetModelCoordsSelection(const std::vector<VertexState>& vsList)
{
    std::vector<Vector2> coords;
    
    coords.push_back(Vector2{vsList[0].coords.x, vsList[0].coords.y}); // get the first element's info
    
    if (vsList.size() > 1) // if list is only one, we're done
    {
        for (int i = 1; i < vsList.size(); i++) // add info from vsList if it's not already in coords
        {
            if (coords[coords.size() - 1].x != vsList[i].coords.x || coords[coords.size() - 1].y != vsList[i].coords.y) // check the last entry in coords before searching further (last entry will often match)
            {
                bool add = true;
                
                for (int j = 0; j < coords.size(); j++) // check coords if theres already a copy of this info anywhere in the list
                {
                    if (coords[j].x == vsList[i].coords.x && coords[j].y == vsList[i].coords.y) // dont add duplicates
                    {
                        add = false;
                        break;
                    }
                }
                
                if (add)
                    coords.push_back(Vector2{vsList[i].coords.x, vsList[i].coords.y});
            }
        }
    }
    else
        return coords;
    
    return coords;
}


void ModelStitch(std::vector<std::vector<Model>>& models, const ModelSelection &oldModelSelection, const int modelVertexWidth, const int modelVertexHeight)
{
    int canvasWidth = models.size();
    int canvasHeight = models[0].size();
    
    if (oldModelSelection.topLeft.x > 0 && oldModelSelection.topLeft.y > 0) // stitch top left vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = 0; // vertex to be merged to 
        std::vector<int> vertexIndices = GetVertexIndices(modelVertexWidth - 1, modelVertexHeight - 1, modelVertexWidth); // vertices that need to be adjusted
        
        for (int i = 0; i < vertexIndices.size(); i++)
            models[oldModelSelection.topLeft.x - 1][oldModelSelection.topLeft.y - 1].meshes[0].vertices[vertexIndices[i] + 1] = models[oldModelSelection.topLeft.x][oldModelSelection.topLeft.y].meshes[0].vertices[vertexIndex1 + 1];
    }
    
    if (oldModelSelection.bottomRight.x < canvasWidth - 1 && oldModelSelection.topLeft.y > 0) // stitch top right vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = GetVertexIndices(modelVertexWidth - 1, 0, modelVertexWidth)[0]; // vertex to be merged to
        std::vector<int> vertexIndices = GetVertexIndices(0, modelVertexHeight - 1, modelVertexWidth); // vertices that need to be adjusted
        
        for (int i = 0; i < vertexIndices.size(); i++)
            models[oldModelSelection.bottomRight.x + 1][oldModelSelection.topLeft.y - 1].meshes[0].vertices[vertexIndices[i] + 1] = models[oldModelSelection.bottomRight.x][oldModelSelection.topLeft.y].meshes[0].vertices[vertexIndex1 + 1];            
    }
    
    if (oldModelSelection.bottomRight.y < canvasHeight - 1 && oldModelSelection.bottomRight.x < canvasWidth - 1) // stitch bottom right vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = GetVertexIndices(modelVertexWidth - 1, modelVertexHeight - 1, modelVertexWidth)[0]; // vertex to be merged to
        int vertexIndex2 = 0; // vertex that needs to be adjusted
        
        models[oldModelSelection.bottomRight.x + 1][oldModelSelection.bottomRight.y + 1].meshes[0].vertices[vertexIndex2 + 1] = models[oldModelSelection.bottomRight.x][oldModelSelection.bottomRight.y].meshes[0].vertices[vertexIndex1 + 1];
    }
    
    if (oldModelSelection.topLeft.x > 0 && oldModelSelection.bottomRight.y < canvasHeight - 1) // stitch bottom left vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = GetVertexIndices(0, modelVertexHeight - 1, modelVertexWidth)[0]; // vertex that needs to be merged to
        std::vector<int> vertexIndices = GetVertexIndices(modelVertexWidth, 0, modelVertexWidth); // vertices that need to be adjusted
        
        for (int i = 0; i < vertexIndices.size(); i++)
            models[oldModelSelection.topLeft.x - 1][oldModelSelection.bottomRight.y + 1].meshes[0].vertices[vertexIndices[i] + 1] = models[oldModelSelection.topLeft.x][oldModelSelection.bottomRight.y].meshes[0].vertices[vertexIndex1 + 1];            
    } 
    
    if (oldModelSelection.topLeft.y > 0)
    {
        for (int i = 0; i < oldModelSelection.width; i++) // stitch all overlapping vertices on the top edge
        {
            for (int j = 0; j < modelVertexWidth; j++)
            {     
                int vertexIndex1 = GetVertexIndices(j, 0, modelVertexWidth)[0]; // vertex that needs to be merged to
                std::vector<int> vertexIndices = GetVertexIndices(j, modelVertexHeight - 1, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[oldModelSelection.topLeft.x + i][oldModelSelection.topLeft.y - 1].meshes[0].vertices[vertexIndices[ii] + 1] = models[oldModelSelection.topLeft.x + i][oldModelSelection.topLeft.y].meshes[0].vertices[vertexIndex1 + 1];        
            }
        }
    }
    
    if (oldModelSelection.bottomRight.x < canvasWidth - 1) 
    {
        for (int i = 0; i < oldModelSelection.height; i++) // stitch all overlapping vertices on the right edge
        {
            for (int j = 0; j < modelVertexHeight; j++)
            {     
                int vertexIndex1 = GetVertexIndices(modelVertexWidth - 1, j, modelVertexWidth)[0]; // vertex that needs to be merged to
                std::vector<int> vertexIndices = GetVertexIndices(0, j, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[oldModelSelection.bottomRight.x + 1][oldModelSelection.topLeft.y + i].meshes[0].vertices[vertexIndices[ii] + 1] = models[oldModelSelection.bottomRight.x][oldModelSelection.topLeft.y + i].meshes[0].vertices[vertexIndex1 + 1];   
            }
        }        
    }
    
    if (oldModelSelection.bottomRight.y < canvasHeight - 1)
    {
        for (int i = 0; i < oldModelSelection.width; i++) // stitch all overlapping vertices on the bottom edge
        {
            for (int j = 0; j < modelVertexWidth; j++)
            {     
                int vertexIndex1 = GetVertexIndices(j, modelVertexHeight - 1, modelVertexWidth)[0]; // vertex that needs to be merged to
                std::vector<int> vertexIndices = GetVertexIndices(j, 0, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[oldModelSelection.topLeft.x + i][oldModelSelection.bottomRight.y + 1].meshes[0].vertices[vertexIndices[ii] + 1] = models[oldModelSelection.topLeft.x + i][oldModelSelection.bottomRight.y].meshes[0].vertices[vertexIndex1 + 1]; 
            }
        }         
    }
    
    if (oldModelSelection.topLeft.x > 0)
    {
        for (int i = 0; i < oldModelSelection.height; i++) // stitch all overlapping vertices on the left edge
        {
            for (int j = 0; j < modelVertexHeight; j++)
            {     
                int vertexIndex1 = GetVertexIndices(0, j, modelVertexWidth)[0]; // vertex that needs to be merged to 
                std::vector<int> vertexIndices = GetVertexIndices(modelVertexWidth - 1, j, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[oldModelSelection.topLeft.x - 1][oldModelSelection.topLeft.y + i].meshes[0].vertices[vertexIndices[ii] + 1] = models[oldModelSelection.topLeft.x][oldModelSelection.topLeft.y + i].meshes[0].vertices[vertexIndex1 + 1];
            }
        }          
    }
}


void SetExSelection(ModelSelection& modelSelection, const int canvasWidth, const int canvasHeight)
{
    modelSelection.expandedSelection = modelSelection.selection; // fill expanded selection with regular selection
    
    if (modelSelection.topLeft.x > 0 && modelSelection.topLeft.y > 0) // add the diagonally adjacent top left model
    {
        modelSelection.expandedSelection.push_back(Vector2{modelSelection.topLeft.x - 1, modelSelection.topLeft.y - 1});
    }
    
    if (modelSelection.topLeft.y > 0 && modelSelection.bottomRight.x < canvasWidth - 1) // add the diagonally adjacent top right model
    {
        modelSelection.expandedSelection.push_back(Vector2{modelSelection.bottomRight.x + 1, modelSelection.topLeft.y - 1});
    }
    
    if (modelSelection.bottomRight.x < canvasWidth - 1 && modelSelection.bottomRight.y < canvasHeight - 1) // add the diagonally adjacent bottom right model
    {
        modelSelection.expandedSelection.push_back(Vector2{modelSelection.bottomRight.x + 1, modelSelection.bottomRight.y + 1});
    }
    
    if (modelSelection.topLeft.x > 0 && modelSelection.bottomRight.y < canvasHeight - 1) // add the diagonally adjacent bottom left model
    {
        modelSelection.expandedSelection.push_back(Vector2{modelSelection.topLeft.x - 1, modelSelection.bottomRight.y + 1});
    }
    
    if (modelSelection.topLeft.y > 0) // fill top row
    {
        for (int i = 0; i < modelSelection.width; i++)
        {
            modelSelection.expandedSelection.push_back(Vector2{modelSelection.topLeft.x + i, modelSelection.topLeft.y - 1});
        }
    }
    
    if (modelSelection.bottomRight.x < canvasWidth - 1) // fill right column
    {
        for (int i = 0; i < modelSelection.height; i++)
        {
            modelSelection.expandedSelection.push_back(Vector2{modelSelection.bottomRight.x + 1, modelSelection.topLeft.y + i});
        }
    }
    
    if (modelSelection.bottomRight.y < canvasHeight - 1) // fill bottom row
    {
        for (int i = 0; i < modelSelection.width; i++)
        {
            modelSelection.expandedSelection.push_back(Vector2{modelSelection.topLeft.x + i, modelSelection.bottomRight.y + 1});
        }
    }
    
    if (modelSelection.topLeft.x > 0) // fill left column
    {
        for (int i = 0; i < modelSelection.height; i++)
        {
            modelSelection.expandedSelection.push_back(Vector2{modelSelection.topLeft.x - 1, modelSelection.topLeft.y + i});
        }
    }
}


void UpdateCameraCustom(Camera* camera, const std::vector<std::vector<Model>>& models, ModelSelection& terrainCells)
{
    static Vector2 previousMousePosition = { 0.0f, 0.0f };
    
    // Mouse movement detection
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePosition = GetMousePosition();
    
    bool direction[6] = { IsKeyDown(cameraMoveControl[MOVE_FRONT]),
                           IsKeyDown(cameraMoveControl[MOVE_BACK]),
                           IsKeyDown(cameraMoveControl[MOVE_RIGHT]),
                           IsKeyDown(cameraMoveControl[MOVE_LEFT]),
                           IsKeyDown(cameraMoveControl[MOVE_UP]),
                           IsKeyDown(cameraMoveControl[MOVE_DOWN]) };
                           
    mousePositionDelta.x = mousePosition.x - previousMousePosition.x;
    mousePositionDelta.y = mousePosition.y - previousMousePosition.y;

    previousMousePosition = mousePosition;
    
    float newCameraPositionX = camera->position.x;
    float newCameraPositionY = camera->position.y;
    float newCameraPositionZ = camera->position.z;
    
    // Camera orientation calculation
    cameraAngle.x += (mousePositionDelta.x*-CAMERA_MOUSE_MOVE_SENSITIVITY);
    cameraAngle.y += (mousePositionDelta.y*-CAMERA_MOUSE_MOVE_SENSITIVITY);
    
    // Angle clamp
    if (cameraAngle.y > CAMERA_FIRST_PERSON_MIN_CLAMP*DEG2RAD) cameraAngle.y = CAMERA_FIRST_PERSON_MIN_CLAMP*DEG2RAD;
    else if (cameraAngle.y < CAMERA_FIRST_PERSON_MAX_CLAMP*DEG2RAD) cameraAngle.y = CAMERA_FIRST_PERSON_MAX_CLAMP*DEG2RAD;
    
    newCameraPositionX += (sinf(cameraAngle.x)*direction[MOVE_BACK] -
                           sinf(cameraAngle.x)*direction[MOVE_FRONT] -
                           cosf(cameraAngle.x)*direction[MOVE_LEFT] +
                           cosf(cameraAngle.x)*direction[MOVE_RIGHT])/PLAYER_MOVEMENT_SENSITIVITY;
                           
    newCameraPositionZ += (cosf(cameraAngle.x)*direction[MOVE_BACK] -
                           cosf(cameraAngle.x)*direction[MOVE_FRONT] +
                           sinf(cameraAngle.x)*direction[MOVE_LEFT] -
                           sinf(cameraAngle.x)*direction[MOVE_RIGHT])/PLAYER_MOVEMENT_SENSITIVITY;
    
    /* free fly camera
    camera->position.x += (sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_BACK] -
                           sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_FRONT] -
                           cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_LEFT] +
                           cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_RIGHT])/PLAYER_MOVEMENT_SENSITIVITY;
                           
    camera->position.y += (sinf(cameraAngle.y)*direction[MOVE_FRONT] -
                           sinf(cameraAngle.y)*direction[MOVE_BACK] +
                           1.0f*direction[MOVE_UP] - 1.0f*direction[MOVE_DOWN])/PLAYER_MOVEMENT_SENSITIVITY;
                           
    camera->position.z += (cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_BACK] -
                           cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_FRONT] +
                           sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_LEFT] -
                           sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_RIGHT])/PLAYER_MOVEMENT_SENSITIVITY;
    */
    
    // keep the camera at ground level by finding the distance to the ground
    Ray ray = {Vector3{newCameraPositionX, camera->position.y + 10, newCameraPositionZ}, Vector3{0, -1, 0}};
    RayHitInfo hitPosition;
    
    int modelx; // x coord of the model that intersects ray
    int modely; // y coord of the model that intersects ray

    if (!models.empty())
    {
        for (int i = 0; i < terrainCells.selection.size(); i++)
        {
            hitPosition = GetCollisionRayModel2(ray, &models[terrainCells.selection[i].x][terrainCells.selection[i].y]); 
            
            if (hitPosition.hit)
            {
                modelx = terrainCells.selection[i].x;
                modely = terrainCells.selection[i].y;
                break;
            }
        }
    }
    
    if (hitPosition.hit && modelx != terrainCells.selection[0].x || modely != terrainCells.selection[0].y) // if player is found on a cell other than the center, readjust the terrain cells to the new center
    {
        terrainCells.selection.clear();
        terrainCells.selection.push_back(Vector2{modelx, modely});
        FillTerrainCells(terrainCells, models);
    }
    
    camera->target.x = camera->position.x - sinf(cameraAngle.x)*cosf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
    camera->target.y = camera->position.y + sinf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
    camera->target.z = camera->position.z - cosf(cameraAngle.x)*cosf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
        
    if (hitPosition.hit) // if a place to move to was found, update camera
    {
        camera->position.x = newCameraPositionX;
        camera->position.y = hitPosition.position.y + playerEyesHeight;
        camera->position.z = newCameraPositionZ;
    }    
}


void FillTerrainCells(ModelSelection& modelSelection, const std::vector<std::vector<Model>>& models)
{
    // attempt to fill the rest of terrainCells with adjacent models from right, down, left, up, top right, bottom right, bottom left, top left
    // filled in the order of the likelyhood of the player occupying it, so center first, sides second, corners last
    
    if (modelSelection.selection.empty()) // needs to have the first element filled
        return;
    
    int canvasWidth = models.size();
    int canvasHeight = models[0].size();

    Vector2 center = modelSelection.selection[0];

    if (center.x < canvasWidth - 1)
        modelSelection.selection.push_back(Vector2{center.x + 1, center.y});
    
    if (center.y < canvasHeight - 1)
        modelSelection.selection.push_back(Vector2{center.x, center.y + 1});
    
    if (center.x > 0)
        modelSelection.selection.push_back(Vector2{center.x - 1, center.y});
    
    if (center.y > 0)
        modelSelection.selection.push_back(Vector2{center.x, center.y - 1});
    
    if (center.x < canvasWidth - 1 && center.y > 0)
        modelSelection.selection.push_back(Vector2{center.x + 1, center.y - 1});
    
    if (center.x < canvasWidth - 1 && center.y < canvasHeight - 1)
        modelSelection.selection.push_back(Vector2{center.x + 1, center.y + 1}); 

    if (center.x > 0 && center.y < canvasHeight - 1)
        modelSelection.selection.push_back(Vector2{center.x - 1, center.y + 1});    
    
    if (center.x > 0 && center.y > 0)
        modelSelection.selection.push_back(Vector2{center.x - 1, center.y - 1});
}


bool operator== (const ModelVertex &mv1, const ModelVertex &mv2)
{
    if (mv1.coords.x == mv2.coords.x && mv1.coords.y == mv2.coords.y && mv1.index == mv2.index)
        return true;
    else
        return false;
}


bool operator!= (const ModelVertex &mv1, const ModelVertex &mv2)
{
    return !(mv1 == mv2);
}


bool operator== (const VertexState &vs1, const VertexState &vs2)
{
    if (vs1.coords.x == vs2.coords.x && vs1.coords.y == vs2.coords.y && vs1.vertexIndex == vs2.vertexIndex)
        return true;
    else
        return false;
}


bool operator!= (const VertexState &vs1, const VertexState &vs2)
{
    return !(vs1 == vs2);
}


bool operator== (const VertexState &vs, const ModelVertex &mv)
{
    if (vs.coords.x == mv.coords.x && vs.coords.y == mv.coords.y && vs.vertexIndex == mv.index)
        return true;
    else
        return false;
}


bool operator== (const ModelVertex &mv, const VertexState &vs)
{
    return (vs == mv);
}


bool operator!= (const VertexState &vs, const ModelVertex &mv)
{
    return !(vs == mv);
}


bool operator!= (const ModelVertex &mv, const VertexState &vs)
{
    return !(vs == mv);
}
























