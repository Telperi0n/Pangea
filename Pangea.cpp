#include "raylib.h"
#include "rlgl.h"
#include <vector>
#include <cmath>
#include <string>
#include <cstring>
#include "raymath.h"
#include "float.h"
#include <bitset>
#include <iostream>




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
    HEIGHTMAP, // AKA CANVAS
    CAMERA,
    TOOLS
};

enum class InputFocus // markers for which the input box is currently in focus
{
    NONE,
    X_MESH, // number of models on the x axis
    Z_MESH, // number of models on the z axis
    X_MESH_SELECT, // size of selection in models on the x axis
    Z_MESH_SELECT, // size of selection in models on the z axis
    STAMP_ANGLE, // the angle determining the gradient of the stamp tool
    STAMP_HEIGHT, // the maximum height of the stamp tool
    SELECT_RADIUS, // size of the vertex selection / influence area
    TOOL_STRENGTH, // tool strength
    SAVE_MESH, // what to name your saved file as
    LOAD_MESH, // name of the file to load
    SAVE_MESH_HEIGHT, // the height to use as the scale when saving
    LOAD_MESH_HEIGHT, // the height to use as the scale when loading
    DIRECTORY, // file path of the desired directory
    INNER_RADIUS, // size of the inner radius
    STRETCH_LENGTH, // distance between the twin stamps when stamp stretch is on
    STAMP_ROTATION, // rotation of the stamp tool when stretch is on
    STAMP_SLOPE, // the angle the stamp tool will increment on when dragged
    STAMP_OFFSET // the amount to raise or lower the stamp tool
};

enum class CameraSetting
{
    FREE,
    CHARACTER,
    TOP_DOWN
};

enum class HeightMapMode
{
    GRAYSCALE,
    SLOPE,
    RAINBOW
};

// Camera move modes (first person and third person cameras)
typedef enum 
{ 
    MOVE_FRONT = 0, 
    MOVE_BACK, 
    MOVE_RIGHT, 
    MOVE_LEFT, 
    MOVE_UP, 
    MOVE_DOWN 
} CameraMove;

struct VertexState
{
    Vector2 coords; // coords of the vertex's model
    int index; // which in Mesh::vertices corresponds to the x value
    float y; // y value of the vertex
    
    friend bool operator== (const VertexState& vs1, const VertexState& vs2);
    friend bool operator!= (const VertexState& vs1, const VertexState& vs2);
};

struct ModelSelection
{
    Vector2 topLeft; // coordinates of the top left model in the selection
    Vector2 bottomRight; // coordinates of the bottom right model in the selection
    
    int width; // width of the selection
    int height; // height of the selection
    
    std::vector<Vector2> selection; // a list of the coordinates of the selected models
    std::vector<Vector2> expandedSelection; // selection plus adjacent models
    
    friend bool operator== (const ModelSelection& ms1, const ModelSelection& ms2);
    friend bool operator!= (const ModelSelection& ms1, const ModelSelection& ms2);
};

struct HistoryStep
{
    std::vector<VertexState> startingVertices; // info of the vertices recorded by this step as they were before the edit happened
    std::vector<VertexState> endingVertices; // info of the vertices recorded by this step as they are after the edit happened
    std::vector<Vector2> modelCoords; // list of the model coordinates recorded by this step, sorted left to right, top to bottom
};


float xzDistance(Vector2 p1, Vector2 p2); // get the distance between two points on the x and z plane

void NewHistoryStep(std::vector<HistoryStep>& history, const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, int& stepIndex, int maxSteps); // adds another historyStep to history

Color* GenHeightmapSelection(const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, int modelVertexWidth, int modelVertexHeight); // memory should be freed. needs a scale param. generates a heightmap from a selection of models

Color* GenHeightmap(const Model& model, int modelVertexWidth, int modelVertexHeight, float& highestY, float& lowestY, HeightMapMode mode, float slopeTolerance = 59.0); // memory should be freed. generates a heightmap for a single model. used for the model texture, cuts last row and column so pixels and polys are 1:1. will update global highest and lowest Y

Color* GenHeightmap(const std::vector<std::vector<Model>>& models, int modelVertexWidth, int modelVertexHeight, float maxHeight, float minHeight, bool grayscale); // memory should be freed. generates a heightmap from the whole map. used for export

RayHitInfo GetCollisionRayModel2(Ray ray, const Model *model); // having a copy of GetCollisionRayModel increases performance for some reason

void UpdateHeightmap(const Model& model, int modelVertexWidth, int modelVertexHeight, float& highestY, float& lowestY, HeightMapMode mode);

void UpdateHeightmap(const std::vector<std::vector<Model>>& models, int modelVertexWidth, int modelVertexHeight, float& highestY, float& lowestY, HeightMapMode mode);

std::vector<int> GetVertexIndices(int x, int y, int width); // get the Indices (x value) of all vertices at a particular location in the square mesh. x y start at 0 and read left right, top down

Vector2 GetVertexCoords(int index, int width); // get the coordindates of a vertex on a square mesh given its index. x y start at 0

std::vector<Vector2> GetModelCoordsSelection(const std::vector<VertexState>& vsList); // get the coords of each unique model in a list of VertexState

void ModelStitch(std::vector<std::vector<Model>>& models, const ModelSelection &modelSelection, int modelVertexWidth, int modelVertexHeight); // merges the overlapping vertices on the edges of adjacent models (no longer used)

void SetExSelection(ModelSelection& modelSelection, int canvasWidth, int canvasHeight); // populates a model selection's expanded selection, which is selection plus the adjacent models

void FillTerrainCells(ModelSelection& modelSelection, const std::vector<std::vector<Model>>& models); // for model selections being used to track player collision. takes selection[0] and attempts to fill selection with the indices of the surrounding models 

void UpdateCharacterCamera(Camera* camera, const std::vector<std::vector<Model>>& models, ModelSelection& terrainCells); // custom update camera function for character camera

void UpdateFreeCamera(Camera* camera); // camera update function for perspective mode

void UpdateOrthographicCamera(Camera* camera); // camera update function for orthographic mode

void PrintBoxInfo(Rectangle box, InputFocus currentFocus, InputFocus matchingFocus, const std::string& s, float info); // print into box using default text params

void PrintBoxInfo(Rectangle box, InputFocus currentFocus, InputFocus matchingFocus, const std::string& s, int info); // int overload

void ProcessInput(int key, std::string& s, float& input, InputFocus& inputFocus, int maxSize); // modify string with input and store input as a float. changes input focus as necessary

std::vector<VertexState> FindVertexSelection(const std::vector<std::vector<Model>>& models, const ModelSelection& modelSelection, RayHitInfo hitPosition, float selectRadius); // find the vertices within the selection radius of the ray hit position 

void FindStampPoints(float stampRotationAngle, float stampStretchLength, Vector2& outVec1, Vector2& outVec2, Vector2 stampAnchor); // finds the ends of the stamp tool when stretch is active

float PointSegmentDistance(Vector2 point, Vector2 segmentPoint1, Vector2 segmentPoint2); // shortest distance from a point to a line segment

Mesh CopyMesh(const Mesh& mesh); // do a deep copy of a mesh

void Smooth(std::vector<std::vector<Model>>& models, const std::vector<VertexState>& vertices, int modelVertexWidth, int modelVertexHeight, int canvasWidth, int canvasHeight); // do a smooth operation on the vertices

unsigned long PixelToHeight(Color pixel); // takes the bits from each of the 4 png channels and arranges them into one int

Mesh GenMeshHeightmap32bit(Image heightmap, Vector3 size); // version of GenMeshHeightmap that handles split channel heightmaps

void UpdateTopDownCamera(Camera* camera);

RayHitInfo FindHit2D(const Ray& ray, const std::vector<std::vector<Model>>& models, int modelVertexWidth, int modelVertexHeight);

RayHitInfo FindHit3D(const Ray& ray, const std::vector<std::vector<Model>>& models, Vector2& modelCoords, int length = 0, int direction = 1, int loop = 0, int total = 0);

ModelSelection FindModelSelection(int canvasWidth, int canvasHeight, int modelWidth, Vector2 modelCoords, float selectRadius);

void ExtendHistoryStep(HistoryStep& historyStep, const std::vector<std::vector<Model>>& models, const ModelSelection& modelCoords); // add vertex info to the history step when it's edit range increases mid edit

void FinalizeHistoryStep(HistoryStep& historyStep, const std::vector<std::vector<Model>>& models); // record the ending state of all vertices in this history step's range when the edit is complete

void UpdateNormals(Model& model, int modelVertexWidth, int modelVertexHeight); // update a model's normals

int BinarySearchVec2(Vector2 vec2, const std::vector<Vector2>&v, int &i); // binary search for vector2. returns the index where vec2 was found, -1 if not found. i will be changed to the index where vec2 should be inserted

template<class T, class T2>
int BinarySearch(T var, const std::vector<T2>&v, int &i); // returns the index where var was found, -1 if not found. i will be changed to the index where var should be inserted

template<class T, class T2>
int BinarySearch(T var, const std::vector<T2>&v); // returns the index where var was found, -1 if not found




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
    const int windowWidth = 1800;
    const int windowHeight = 900;
    const int maxSteps = 10;   // number of changes to keep track of for the history
    const int modelVertexWidth = 120; // 360x360 aprox max before fps <60 with raycollision
    const int modelVertexHeight = 120;  
    const int modelWidth = 12;
    const int modelHeight = 12;
    int stepIndex = 0; // the current location in history
    int canvasWidth = 0; // in number of models
    int canvasHeight = 0; // in number of models
    float timeCounter = 0; // used to track number of frames passed
    float selectRadius = 1.5f;
    float toolStrength = 0.1f; 
    float highestY = 0.0f; // highest y value on the mesh
    float lowestY = 0.0f; // lowest y value on the mesh
    float stampAngle = 60.0f; // how steep the stamp shape is
    float stampHeight = 0; // optional height cap of the stamp tool
    float innerRadius = 0;
    float stampStretchLength = 0.5f;
    float stampRotationAngle = 0.0f;
    float stampSlope = 0.0f;
    float stampOffset = 0.0f;
    bool updateFlag = false; // true when mouse left click has not been released since an edit operation has been done (aka true when painting)
    bool selectionMask = false;
    bool characterDrag = false; // true when the character camera placement is being held
    bool rayCollision2d = true; // if true, ray collision will be tested against the ground plane instead of the actual mesh 
    bool raiseOnly = true; // if true, brush will check if the vertex is higher than what it's attempting to change it to and if so, not alter it
    bool showSaveWindow = false; // whether or not to display the save window with the export options
    bool showLoadWindow = false; // true if the load window should be shown
    bool showDirWindow = false; // if directory change window is shown
    bool lowerOnly = false; // if true, brush will only lower vertices
    bool editQueue = false; // if true, each tick has the ability to produce more than one edit operation. if more than one is produced, theyre pushing into a queue and are completed asap
    bool stampFlip = false; // whether the stamp is upside down or right side up
    bool stampInvert = false; // whether the stamp is normal or mirrored vertically
    bool stampStretch = false; // if true, the stamp will become two connected copies of itself equally spaced from the middle that rotate depending on mouse drag movement
    bool useGhostMesh = false; // when set to true, a copy of the model selection is made which is then tested against for ray collision rather than the current mesh. setting to false clears the mesh
    bool stampDrag = false; // set to true after the first stamp edit with stampStretch on, and off when the left mouse is released
    bool saveGrayscale = true; // true to save the heightmap as grayscale, false to save using all png channels (looks weird, saves more height resolution)
    bool loadGrayscale = true; // should be set to true when loading grayscale image
    
    Vector2 lastRayHitLoc = {0, 0}; // coordinates of the model the mouse ray last hit
    
    std::vector<HistoryStep> history;
    std::vector<VertexState> vertexSelection;
    std::vector<std::vector<Model>> models;      // 2d vector of all models
    
    std::vector<std::vector<Model>> ghostMesh;  // copy of a selection of models used for collision detection
    
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
    std::string innerRadiusString;
    std::string stretchLengthString;
    std::string stampRotationString;
    std::string stampSlopeString;
    std::string stampOffsetString;
    
    ModelSelection modelSelection; // the models currently being worked on by their index in models
    modelSelection.height = 0;
    modelSelection.width = 0;
    
    ModelSelection terrainCells; // the models surrounding the player in character mode
    
    // ANCHORS
    Vector2 meshSelectAnchor = {0, 66}; // location to which all mesh selection elements are relative
    Vector2 toolButtonAnchor = {0, 300}; // location to which all tool elements are relative
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
    Rectangle saveWindow = {saveWindowAnchor.x, saveWindowAnchor.y, 300, 235};
    Rectangle saveWindowSaveButton = {saveWindowAnchor.x + 60, saveWindowAnchor.y + 155, 60, 20};
    Rectangle saveWindowCancelButton = {saveWindowAnchor.x + 180, saveWindowAnchor.y + 155, 60, 20};
    Rectangle saveWindowTextBox = {saveWindowAnchor.x + 30, saveWindowAnchor.y + 40, 240, 30};
    Rectangle saveWindowHeightBox = {saveWindowAnchor.x + 30, saveWindowAnchor.y + 115, 240, 30};
    Rectangle saveWindowGrayscale = {saveWindowAnchor.x + 15, saveWindowAnchor.y + 185, 12, 12};
    Rectangle saveWindow32bit = {saveWindowAnchor.x + 15, saveWindowAnchor.y + 210, 12, 12};
    
    // LOAD WINDOW
    Rectangle loadWindow = {loadWindowAnchor.x, loadWindowAnchor.y, 300, 235};
    Rectangle loadWindowLoadButton = {loadWindowAnchor.x + 60, loadWindowAnchor.y + 155, 60, 20};
    Rectangle loadWindowCancelButton = {loadWindowAnchor.x + 180, loadWindowAnchor.y + 155, 60, 20};
    Rectangle loadWindowTextBox = {loadWindowAnchor.x + 30, loadWindowAnchor.y + 40, 240, 30};
    Rectangle loadWindowHeightBox = {loadWindowAnchor.x + 30, loadWindowAnchor.y + 115, 240, 30};
    Rectangle loadWindowGrayscale = {loadWindowAnchor.x + 15, loadWindowAnchor.y + 185, 12, 12};
    Rectangle loadWindow32bit = {loadWindowAnchor.x + 15, loadWindowAnchor.y + 210, 12, 12};
    
    // DIRECTORY WINDOW
    Rectangle dirWindow = {dirWindowAnchor.x, dirWindowAnchor.y, 500, 120};
    Rectangle dirWindowOkayButton = {dirWindowAnchor.x + 160, dirWindowAnchor.y + 85, 60, 20};
    Rectangle dirWindowCancelButton = {dirWindowAnchor.x + 280, dirWindowAnchor.y + 85, 60, 20};
    Rectangle dirWindowTextBox = {dirWindowAnchor.x + 30, dirWindowAnchor.y + 40, 440, 30};
    
    // CANVAS PANEL
    Rectangle exportButton = {10, 557, 80, 40};
    Rectangle meshGenButton = {10, 80, 80, 40};
    Rectangle xMeshBox = {35, 30, 55, 20};
    Rectangle zMeshBox = {35, 55, 55, 20};
    Rectangle updateTextureButton = {10, 457, 80, 40};
    Rectangle loadButton = {10, 507, 80, 40};
    Rectangle directoryButton = {10, 607, 80, 40};
    Rectangle grayscaleTexBox = {82, 390, 14, 14};
    Rectangle slopeTexBox = {82, 409, 14, 14};
    Rectangle rainbowTexBox = {82, 428, 14, 14};
    
    // CAMERA PANEL
    Rectangle characterButton = {63, 55, 33, 33};
    Rectangle cameraSettingBox = {10, 120, 14, 14};
    
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
    Rectangle selectRadiusBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y - 115, 30, 14};
    Rectangle toolStrengthBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y - 96, 30, 14};
    Rectangle raiseOnlyBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y - 77, 14, 14};
    Rectangle lowerOnlyBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y - 58, 14, 14};
    Rectangle collisionTypeBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y - 39, 14, 14};
    Rectangle editQueueBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y - 20, 14, 14};
    
    Rectangle stampAngleBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 160, 30, 14};
    Rectangle stampHeightBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 179, 30, 14}; // max height cutoff
    Rectangle stampFlipBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y + 198, 14, 14};
        Rectangle innerRadiusBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 217, 30, 14};
    Rectangle stampInvertBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y + 236, 14, 14};
    Rectangle stampStretchBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y + 255, 14, 14};
        Rectangle stretchLengthBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 274, 30, 14};
        Rectangle stampRotationBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 293, 30, 14};
    Rectangle stampSlopeBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 312, 30, 14}; // -90 to 90 degrees
    Rectangle stampOffsetBox = {toolButtonAnchor.x + 66, toolButtonAnchor.y + 331, 30, 14};
    Rectangle ghostMeshBox = {toolButtonAnchor.x + 82, toolButtonAnchor.y + 350, 14, 14};
    
    Rectangle smoothMeshesButton = {toolButtonAnchor.x + 10, toolButtonAnchor.y + 160, 80, 28};
    
    
    BrushTool brush = BrushTool::NONE;
    Panel panel = Panel::NONE;
    InputFocus inputFocus = InputFocus::NONE;
    CameraSetting cameraSetting = CameraSetting::FREE;
    HeightMapMode heightMapMode = HeightMapMode::GRAYSCALE;
    
    InitWindow(windowWidth, windowHeight, "Pangea");
    
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.type = CAMERA_PERSPECTIVE;                   // Camera mode type
  
    SetCameraMode(camera, CAMERA_FREE); // Set a free camera mode
    
    ChangeDirectory("C:/Users/msgs4/Desktop/Pangea");
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        if (cameraSetting == CameraSetting::CHARACTER)
        {
            UpdateCharacterCamera(&camera, models, terrainCells);
            
            if (IsKeyPressed(KEY_TAB)) // exit character mode
            {
                SetCameraMode(camera, CAMERA_FREE);
                camera.fovy = 45.0f;
                cameraSetting = CameraSetting::FREE;
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
            if (inputFocus == InputFocus::NONE) // dont move the camera if typing
            {
                if (cameraSetting == CameraSetting::FREE)
                    UpdateFreeCamera(&camera);
                else if (cameraSetting == CameraSetting::TOP_DOWN)
                    UpdateTopDownCamera(&camera);
            }
            
            RayHitInfo hitPosition;
            hitPosition.hit = false;
            // vectors to hold the locations of the two ends of the stamp tool when stretch is activated
            Vector2 stamp1; 
            Vector2 stamp2;
            
            std::vector<VertexState> vertexIndices;   // information of the vertices within the select radius
            static ModelSelection lastEditSelection; // models that were last edited. if updateFlag is true and lastEditSelection is different from editSelection while making an edit, the current history step will be updated
            ModelSelection editSelection; // models that are being edited this tick
            
            Vector2 mousePosition = GetMousePosition();
            bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            
            if (mouseDown && (IsKeyDown(KEY_R) || IsKeyDown(KEY_V)) || IsKeyDown(KEY_G)) // check if keys are down that preclude mouse click edits from happening
            {
                mouseDown = false;
            }
            
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
                        pixels = GenHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY, saveGrayscale);
                    }
                    else
                    {
                        float maxHeight = std::stof(saveHeightString);
                        
                        pixels = GenHeightmap(models, modelVertexWidth, modelVertexHeight, maxHeight, lowestY, saveGrayscale);
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
                else if (CheckCollisionPointRec(mousePosition, saveWindowGrayscale) && mousePressed)
                {
                    saveGrayscale = true;
                }
                else if (CheckCollisionPointRec(mousePosition, saveWindow32bit) && mousePressed)
                {
                    saveGrayscale = false;
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
                            Model model;
                            
                            if (loadGrayscale)
                                model = LoadModelFromMesh(GenMeshHeightmap(tempImage, (Vector3){ modelWidth, heightRef, modelHeight }));  
                            else
                                model = LoadModelFromMesh(GenMeshHeightmap32bit(tempImage, (Vector3){ modelWidth, heightRef, modelHeight })); 
                            
                            UnloadImage(tempImage);
                            RL_FREE(vertexPixels);
                            
                            Color* pixels = GenHeightmap(model, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
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
                    
                    modelSelection.selection.clear();
                    
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
                else if (CheckCollisionPointRec(mousePosition, loadWindowGrayscale) && mousePressed)
                {
                    loadGrayscale = true;
                }
                else if (CheckCollisionPointRec(mousePosition, loadWindow32bit) && mousePressed)
                {
                    loadGrayscale = false;
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
            else if (CheckCollisionPointRec(mousePosition, UI)) 
            {
                if (mousePressed)
                {
                    switch (panel)
                    {
                        case Panel::NONE:
                        {
                            if (CheckCollisionPointRec(mousePosition, heightmapTab))
                            {
                                panel = Panel::HEIGHTMAP;
                                
                                cameraTab.y = 657;
                                toolsTab.y = 680;
                                panel1.height = 637;
                                panel2.y = 677;
                            }
                            else if (CheckCollisionPointRec(mousePosition, cameraTab))
                            {
                                panel = Panel::CAMERA;
                                
                                panel2.height = 637;
                                toolsTab.y = 680;
                            }
                            else if (CheckCollisionPointRec(mousePosition, toolsTab))
                            {
                                panel = Panel::TOOLS;
                            }
                            
                            break;
                        }
                        case Panel::HEIGHTMAP:
                        {
                            if (CheckCollisionPointRec(mousePosition, heightmapTab))
                            {
                                panel = Panel::NONE;
                                
                                cameraTab.y = 23;
                                toolsTab.y = 46;
                                panel1.height = 3;
                                panel2.y = 43;
                            }
                            else if (CheckCollisionPointRec(mousePosition, cameraTab))
                            {
                                panel = Panel::CAMERA;
                                
                                panel1.height = 3;
                                panel2.height = 637;
                                panel2.y = 43;
                                cameraTab.y = 23;
                                toolsTab.y = 680;
                            }
                            else if (CheckCollisionPointRec(mousePosition, toolsTab))
                            {
                                panel = Panel::TOOLS;
                                
                                cameraTab.y = 23;
                                toolsTab.y = 46;
                                panel1.height = 3;
                                panel2.y = 43;
                            }
                            
                            if (CheckCollisionPointRec(mousePosition, exportButton)) // export heightmap button
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
                            else if (CheckCollisionPointRec(mousePosition, meshGenButton) && (!xMeshString.empty() || canvasWidth > 0) && (!zMeshString.empty() || canvasHeight > 0)) // add or remove models
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
                                    for (int i = models.size() + xDifference; i < models.size(); i++) // unload models from memory
                                    {
                                        for (int j = 0; j < models[i].size(); j++)
                                        {
                                            UnloadModel(models[i][j]);
                                        }
                                    }
                                    
                                    models.erase(models.begin() + (models.size() + xDifference), models.end());
                                    
                                    history.clear(); // clear history if the canvas is shrunk so that undo operations dont go out of bounds
                                    stepIndex = 0;
                                }
                                
                                canvasWidth = xInput;
                                
                                if (zDifference < 0)
                                {
                                    history.clear(); // clear history if the canvas is shrunk so that undo operations dont go out of bounds
                                    stepIndex = 0;
                                }
                                
                                if (canvasWidth)
                                {
                                    int newLength = canvasHeight + zDifference;
                                    
                                    for (int i = 0; i < canvasWidth; i++)
                                    {
                                        if (models[i].size() > newLength) // z will need to be cut in existing columns, but may have to be expanded in new ones
                                        {
                                            for (int j = newLength; j < models[i].size(); j++) // unload models from memory
                                            {
                                                UnloadModel(models[i][j]);
                                            }
                                            
                                            models[i].erase(models[i].begin() + newLength, models[i].end());
                                        }
                                        else
                                        {
                                            models[i].reserve(newLength);
                                            
                                            while (models[i].size() < newLength)
                                            {
                                                int j = models[i].size();
                                                
                                                Image tempImage = GenImageColor(modelVertexWidth, modelVertexHeight, BLACK);
                                                Model model = LoadModelFromMesh(GenMeshHeightmap(tempImage, (Vector3){ modelWidth, 0, modelHeight }));  
                                                UnloadImage(tempImage);
                                                
                                                Color* pixels = GenHeightmap(model, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                                                Image image = LoadImageEx(pixels, modelVertexWidth - 1, modelVertexHeight - 1);
                                                Texture2D tex = LoadTextureFromImage(image); // create a texture from the heightmap. height and width -1 so that pixels and polys are 1:1
                                                RL_FREE(pixels);
                                                UnloadImage(image);
                                                
                                                model.materials[0].maps[MAP_DIFFUSE].texture = tex;
                                                
                                                float xOffset = (float)i * (modelWidth - (1 / (float)modelVertexWidth) * modelWidth); // adjust x and y values by multiples of modelWidth/Height minus the width/height of one poly in the mesh
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
                            else if (CheckCollisionPointRec(mousePosition, xMeshBox))
                            {
                                inputFocus = InputFocus::X_MESH;
                            }
                            else if (CheckCollisionPointRec(mousePosition, zMeshBox))
                            {
                                inputFocus = InputFocus::Z_MESH;
                            }
                            else if (CheckCollisionPointRec(mousePosition, updateTextureButton) && !models.empty()) // find the lowest and highest point on the mesh
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
                                
                                UpdateHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
                            }
                            else if (CheckCollisionPointRec(mousePosition, loadButton))
                            {
                                brush = BrushTool::NONE;
                                inputFocus = InputFocus::NONE;
                                
                                showLoadWindow = true;
                            }
                            else if (CheckCollisionPointRec(mousePosition, directoryButton))
                            {
                                brush = BrushTool::NONE;
                                inputFocus = InputFocus::NONE;
                                
                                showDirWindow = true;
                            }
                            else if (CheckCollisionPointRec(mousePosition, grayscaleTexBox))
                            {
                                heightMapMode = HeightMapMode::GRAYSCALE;
                                UpdateHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                            }
                            else if (CheckCollisionPointRec(mousePosition, slopeTexBox))
                            {
                                heightMapMode = HeightMapMode::SLOPE;
                                UpdateHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                            }
                            else if (CheckCollisionPointRec(mousePosition, rainbowTexBox))
                            {
                                heightMapMode = HeightMapMode::RAINBOW;
                                UpdateHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                            }
                        
                            break;
                        }
                        case Panel::CAMERA:
                        {
                            if (CheckCollisionPointRec(mousePosition, heightmapTab))
                            {
                                panel = Panel::HEIGHTMAP;
                                
                                cameraTab.y = 657;
                                panel1.height = 637;
                                panel2.y = 677;
                                panel2.height = 3;
                            }
                            else if (CheckCollisionPointRec(mousePosition, cameraTab))
                            {
                                panel = Panel::NONE;
                                
                                toolsTab.y = 46;
                                panel2.y = 43;
                                panel2.height = 3;
                            }
                            else if (CheckCollisionPointRec(mousePosition, toolsTab))
                            {
                                panel = Panel::TOOLS;
                                
                                toolsTab.y = 46;
                                panel2.y = 43;
                                panel2.height = 3;                       
                            }
                            else if (CheckCollisionPointRec(mousePosition, characterButton))
                            {
                                brush = BrushTool::NONE;
                                characterDrag = true;
                            }
                            else if (CheckCollisionPointRec(mousePosition, cameraSettingBox))
                            {
                                if (cameraSetting != CameraSetting::TOP_DOWN)
                                {
                                    cameraAngle.x = 0;
                                    cameraAngle.y = -1.5;
                                    cameraSetting = CameraSetting::TOP_DOWN;
                                }
                                else
                                {
                                    cameraSetting = CameraSetting::FREE;
                                }
                            }
                            /*
                            else if (CheckCollisionPointRec(mousePosition, cameraTypeBox))
                            {
                                if (camera.type == CAMERA_PERSPECTIVE)
                                {
                                    cameraAngle.x = 0;
                                    cameraAngle.y = -1.5;
                                    camera.target.x = camera.position.x;
                                    camera.target.y = camera.position.y - 0.1;
                                    camera.target.z = camera.position.z;
                                    
                                    camera.type = CAMERA_ORTHOGRAPHIC;
                                }
                                else
                                    camera.type = CAMERA_PERSPECTIVE;
                            }
                            */
                            break;
                        }
                        case Panel::TOOLS:
                        {
                            if (CheckCollisionPointRec(mousePosition, heightmapTab))
                            {
                                panel = Panel::HEIGHTMAP;
                                
                                cameraTab.y = 657;
                                toolsTab.y = 680;
                                panel1.height = 637;
                                panel2.y = 677;
                            }
                            else if (CheckCollisionPointRec(mousePosition, cameraTab))
                            {
                                panel = Panel::CAMERA;
                                
                                panel2.height = 637;
                                toolsTab.y = 680;
                            }
                            else if (CheckCollisionPointRec(mousePosition, toolsTab))
                            {
                                panel = Panel::NONE;
                            }
                            
                            if (CheckCollisionPointRec(mousePosition, elevToolButton))
                            {
                                if (brush == BrushTool::ELEVATION)
                                    brush = BrushTool::NONE;
                                else
                                    brush = BrushTool::ELEVATION;
                            }
                            else if (CheckCollisionPointRec(mousePosition, smoothToolButton))
                            {
                                if (brush == BrushTool::SMOOTH)
                                    brush = BrushTool::NONE;
                                else                
                                    brush = BrushTool::SMOOTH;
                            }
                            else if (CheckCollisionPointRec(mousePosition, flattenToolButton))
                            {
                                if (brush == BrushTool::FLATTEN)
                                    brush = BrushTool::NONE;
                                else                
                                    brush = BrushTool::FLATTEN;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampToolButton))
                            {
                                if (brush == BrushTool::STAMP)
                                    brush = BrushTool::NONE;
                                else                
                                    brush = BrushTool::STAMP;
                            }
                            else if (CheckCollisionPointRec(mousePosition, selectToolButton))
                            {
                                if (brush == BrushTool::SELECT)
                                    brush = BrushTool::NONE;
                                else
                                    brush = BrushTool::SELECT;
                            }
                            else if (brush == BrushTool::SELECT && CheckCollisionPointRec(mousePosition, deselectButton))
                            {
                                vertexSelection.clear();
                            }
                            else if (brush == BrushTool::SELECT && CheckCollisionPointRec(mousePosition, trailToolButton) && !vertexSelection.empty() && vertexSelection[vertexSelection.size() - 1].y > 1) // use trail tool
                            {
                                ModelSelection ms; // list of the models found in vertexSelection
                                
                                for (int i = 0; i < vertexSelection.size(); i++) // fill ms with all unique model coords
                                {
                                    int index; // insertion index
                                    
                                    if (BinarySearchVec2(vertexSelection[i].coords, ms.selection, index) == -1) // if these coords arent found, add them
                                    {
                                        ms.selection.insert(ms.selection.begin() + index, vertexSelection[i].coords);
                                    }
                                }
                                
                                NewHistoryStep(history, models, ms.selection, stepIndex, maxSteps);
                                
                                float top = models[vertexSelection[0].coords.x][vertexSelection[0].coords.y].meshes[0].vertices[vertexSelection[0].index + 1];
                                float bottom = models[vertexSelection[(int)vertexSelection.size() - 1].coords.x][vertexSelection[(int)vertexSelection.size() - 1].coords.y].meshes[0].vertices[vertexSelection[(int)vertexSelection.size() - 1].index + 1];
                                
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
                                        models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].index + 1] = top;
                                    
                                    models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].index + 1] = top - (increment * (vertexSelection[i].y - 1));
                                } 
                                
                                FinalizeHistoryStep(history[stepIndex - 1], models);
                                
                                for (int i = 0; i < models.size(); i++)
                                {
                                    for (int j = 0; j < models[i].size(); j++)
                                    {
                                        rlUpdateBuffer(models[i][j].meshes[0].vboId[0], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                                        rlUpdateBuffer(models[i][j].meshes[0].vboId[2], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                                            
                                        UpdateHeightmap(models[i][j], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);  
                                    }                                    
                                }
                            }
                            else if (brush == BrushTool::SELECT && CheckCollisionPointRec(mousePosition, selectionMaskButton))
                            {
                                selectionMask = !selectionMask;
                            }
                            else if (CheckCollisionPointRec(mousePosition, xMeshSelectBox))
                            {
                                inputFocus = InputFocus::X_MESH_SELECT;
                            }
                            else if (CheckCollisionPointRec(mousePosition, zMeshSelectBox))
                            {
                                inputFocus = InputFocus::Z_MESH_SELECT;
                            }
                            else if (CheckCollisionPointRec(mousePosition, meshSelectButton) && !models.empty()) // adjust model selection
                            {
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
                            else if (CheckCollisionPointRec(mousePosition, meshSelectUpButton) && !models.empty() && !modelSelection.selection.empty()) // shift model selection up
                            {
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
                            else if (CheckCollisionPointRec(mousePosition, meshSelectRightButton) && !models.empty() && !modelSelection.selection.empty()) // shift model selection right
                            {
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
                            else if (CheckCollisionPointRec(mousePosition, meshSelectLeftButton) && !models.empty() && !modelSelection.selection.empty()) // shift model selection left
                            {
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
                            else if (CheckCollisionPointRec(mousePosition, meshSelectDownButton) && !models.empty() && !modelSelection.selection.empty()) // shift model selection down
                            {
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
                            else if (CheckCollisionPointRec(mousePosition, stampAngleBox))
                            {
                                inputFocus = InputFocus::STAMP_ANGLE;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampHeightBox))
                            {
                                inputFocus = InputFocus::STAMP_HEIGHT;
                            }
                            else if (CheckCollisionPointRec(mousePosition, toolStrengthBox))
                            {
                                inputFocus = InputFocus::TOOL_STRENGTH;
                            }
                            else if (CheckCollisionPointRec(mousePosition, selectRadiusBox))
                            {
                                inputFocus = InputFocus::SELECT_RADIUS;
                            }
                            else if (CheckCollisionPointRec(mousePosition, raiseOnlyBox))
                            {
                                if (lowerOnly)
                                    lowerOnly = false;
                                
                                raiseOnly = !raiseOnly;
                            }
                            else if (CheckCollisionPointRec(mousePosition, lowerOnlyBox))
                            {
                                if (raiseOnly)
                                    raiseOnly = false;
                                
                                lowerOnly = !lowerOnly;
                            }
                            else if (CheckCollisionPointRec(mousePosition, collisionTypeBox))
                            {
                                rayCollision2d = !rayCollision2d;
                            }
                            else if (CheckCollisionPointRec(mousePosition, editQueueBox))
                            {
                                editQueue = !editQueue;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampFlipBox))
                            {
                                stampFlip = !stampFlip;
                            }
                            else if (CheckCollisionPointRec(mousePosition, innerRadiusBox))
                            {
                                inputFocus = InputFocus::INNER_RADIUS;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampInvertBox))
                            {
                                stampInvert = !stampInvert;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampStretchBox))
                            {
                                stampStretch = !stampStretch;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stretchLengthBox) && stampStretch)
                            {
                                inputFocus = InputFocus::STRETCH_LENGTH;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampRotationBox) && stampStretch)
                            {
                                inputFocus = InputFocus::STAMP_ROTATION;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampSlopeBox))
                            {
                                inputFocus = InputFocus::STAMP_SLOPE;
                            }
                            else if (CheckCollisionPointRec(mousePosition, stampOffsetBox))
                            {
                                inputFocus = InputFocus::STAMP_OFFSET;
                            }
                            else if (CheckCollisionPointRec(mousePosition, ghostMeshBox) && !modelSelection.selection.empty())
                            {
                                if (useGhostMesh)
                                {
                                    for (int i = 0; i < ghostMesh.size(); i++)
                                    {
                                        for (int j = 0; j < ghostMesh[i].size(); j++)
                                            UnloadModel(ghostMesh[i][j]);
                                    }
                                    
                                    ghostMesh.clear();
                                    
                                    useGhostMesh = false;
                                }
                                else
                                {
                                    ghostMesh.resize(modelSelection.width);
                                    
                                    for (int i = 0; i < modelSelection.width; i++)
                                    {
                                        for (int j = 0; j < modelSelection.height; j++)
                                        {
                                            Mesh mesh = CopyMesh(models[i + modelSelection.topLeft.x][j + modelSelection.topLeft.y].meshes[0]);
                                            
                                            ghostMesh[i].push_back(LoadModelFromMesh(mesh));
                                        }
                                    }
                                    
                                    useGhostMesh = true;
                                }
                            }
                            else if (brush == BrushTool::SMOOTH && CheckCollisionPointRec(mousePosition, smoothMeshesButton) && !modelSelection.selection.empty()) // smooth all selected models
                            {
                                NewHistoryStep(history, models, modelSelection.expandedSelection, stepIndex, maxSteps);
                                
                                std::vector<VertexState> vertices; // vertices to pass to smooth
                                
                                for (int i = 0; i < modelSelection.selection.size(); i++) // loop through all selected models
                                {
                                    for (int j = 0; j < models[modelSelection.selection[i].x][modelSelection.selection[i].y].meshes[0].vertexCount; j++) // loop through vertices
                                    {
                                        VertexState vs;
                                        
                                        vs.coords = modelSelection.selection[i];
                                        vs.index = j * 3;
                                        
                                        vertices.push_back(vs); // add this vertex's info 
                                    }
                                }
                                
                                Smooth(models, vertices, modelVertexWidth, modelVertexHeight, canvasWidth, canvasHeight);
                                
                                ModelStitch(models, modelSelection, modelVertexWidth, modelVertexHeight); // overlapping vertices of adjacent models need to be changed as well
                                
                                FinalizeHistoryStep(history[stepIndex - 1], models);
                                
                                for (int i = 0; i < models.size(); i++)
                                {
                                    for (int j = 0; j < models[i].size(); j++)
                                    {
                                        rlUpdateBuffer(models[i][j].meshes[0].vboId[0], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                                        rlUpdateBuffer(models[i][j].meshes[0].vboId[2], models[i][j].meshes[0].vertices, models[i][j].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                                            
                                        UpdateHeightmap(models[i][j], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);  
                                    }                                    
                                }
                            }
                            
                            break;
                        }
                    }
                }
            }
            else if (brush != BrushTool::NONE) // if mouse is not over a 2d element --------------------------------------------------------------------------------------------------
            {
                Ray ray = GetMouseRay(GetMousePosition(), camera);
                
                if (IsKeyDown(KEY_LEFT_BRACKET) && selectRadius > 0) 
                    selectRadius -= 0.04f;
                
                if (IsKeyDown(KEY_RIGHT_BRACKET))
                    selectRadius += 0.04f;
                
                if (IsKeyDown(KEY_PERIOD) && inputFocus == InputFocus::NONE && brush == BrushTool::STAMP && stampStretch)
                {
                    stampRotationAngle += 1;
                    
                    if (stampRotationAngle > 360)
                        stampRotationAngle -= 360;
                }
                
                if (IsKeyDown(KEY_COMMA) && inputFocus == InputFocus::NONE && brush == BrushTool::STAMP && stampStretch)
                {
                    stampRotationAngle -= 1;
                    
                    if (stampRotationAngle < 0)
                        stampRotationAngle += 360;
                }
                
                if (IsKeyDown(KEY_R) && mousePressed && !models.empty()) // change selection radius by height eye dropper style
                {
                    for (int i = 0; i < models.size(); i++) // test ray against all models
                    {
                        for (int j = 0; j < models[i].size(); j++)
                        {
                            hitPosition = GetCollisionRayModel2(ray, &models[i][j]); 
                            
                            if (hitPosition.hit) // if collision is found, record which and break the search
                            {
                                selectRadius = (hitPosition.position.y / sinf(stampAngle*DEG2RAD)) * sinf((90 - stampAngle)*DEG2RAD);
                                
                                break;
                            }
                        }
                        
                        if (hitPosition.hit) break;
                    }
                }
                
                if (IsKeyDown(KEY_V) && mousePressed && !models.empty()) // change stamp offset eye dropper style
                {
                    for (int i = 0; i < models.size(); i++) // test ray against all models
                    {
                        for (int j = 0; j < models[i].size(); j++)
                        {
                            hitPosition = GetCollisionRayModel2(ray, &models[i][j]); 
                            
                            if (hitPosition.hit) // if collision is found, record which and break the search
                            {
                                stampOffset = hitPosition.position.y;
                                
                                break;
                            }
                        }
                        
                        if (hitPosition.hit) break;
                    }
                }
                
                if (IsKeyDown(KEY_G) && mousePressed && !models.empty()) // move model selection, centered on the model clicked on
                {
                    for (int i = 0; i < models.size(); i++) // test ray against all models
                    {
                        for (int j = 0; j < models[i].size(); j++)
                        {
                            hitPosition = GetCollisionRayModel2(ray, &models[i][j]); 
                            
                            if (hitPosition.hit) 
                            {
                                modelSelection.selection.clear();
                                modelSelection.expandedSelection.clear();
                                
                                Vector2 topLeft; // the top left model of the new selection, start with i and j as coordinates
                                topLeft.x = i;
                                topLeft.y = j;
                                
                                if (modelSelection.width != 1) // if the width is 1, topLeft.x is already known
                                {
                                    if (!(modelSelection.width % 2)) // if the width is even, shift x by half with selection width - 1
                                    {
                                        topLeft.x -= (modelSelection.width / 2 - 1);
                                    }
                                    else // if the width is odd, shift x by half the selection width, remainder truncated
                                    {
                                        topLeft.x -= modelSelection.width / 2;
                                    }
                                }
                                
                                if (topLeft.x < 0) // if x was corrected past 0, set to 0
                                    topLeft.x = 0;
                                    
                                if (modelSelection.height != 1) // if the height is 1, topLeft.y is already known
                                {
                                    if (!(modelSelection.height % 2)) // if the height is even, shift y by half with selection height - 1
                                    {
                                        topLeft.y -= (modelSelection.height / 2 - 1);
                                    }
                                    else // if the height is odd, shift x by half the selection height, remainder truncated
                                    {
                                        topLeft.y -= modelSelection.height / 2;
                                    }
                                }
                                
                                if (topLeft.y < 0) // if x was corrected past 0, set to 0
                                    topLeft.y = 0;
                                
                                if (topLeft.x + modelSelection.width > canvasWidth) // make sure it wont extend out of bounds
                                    topLeft.x = canvasWidth - modelSelection.width;
                                
                                if (topLeft.y + modelSelection.height > canvasHeight)
                                    topLeft.y = canvasHeight - modelSelection.height;
                                
                                modelSelection.topLeft = topLeft;
                                modelSelection.bottomRight = Vector2{topLeft.x + modelSelection.width - 1, topLeft.y + modelSelection.height - 1};
                                
                                for (int i2 = 0; i2 < modelSelection.width; i2++)
                                {
                                    for (int j2 = 0; j2 < modelSelection.height; j2++)
                                    {
                                        modelSelection.selection.push_back(Vector2{i2 + topLeft.x, j2 + topLeft.y});
                                    }
                                }
                                
                                SetExSelection(modelSelection, canvasWidth, canvasHeight);
                                
                                break;
                            }
                        }
                        
                        if (hitPosition.hit) break;
                    }
                }
                
                if (!models.empty()) // find the ray hit position
                {
                    Vector2 prevRayHitLoc = lastRayHitLoc; // save in case lastRayHitLoc has to be reverted
                    
                    if (!IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && !rayCollision2d) // dont try to find the hit position if the camera angle is being adjusted
                    {
                        if (useGhostMesh && !ghostMesh.empty()) // if ghost mesh is active, test collision against that rather than models
                        {
                            hitPosition = FindHit3D(ray, ghostMesh, lastRayHitLoc);
                        }
                        else
                        {
                            hitPosition = FindHit3D(ray, models, lastRayHitLoc);
                        }
                    }
                    else if (!IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && rayCollision2d)
                    {
                        hitPosition = FindHit2D(ray, models, modelVertexWidth, modelVertexHeight);
                        
                        if (hitPosition.hit)
                        {
                            float realModelWidth = modelWidth - (1 / 120.f) * modelWidth; // modelWidth minus the width of one polygon. ASSUMES MODELVERTEXWIDTH = 120
                            lastRayHitLoc.x = (int)(hitPosition.position.x / realModelWidth); // convert to int to truncate decimal
                            lastRayHitLoc.y = (int)(hitPosition.position.z / realModelWidth); 
                        }
                    }
                    
                    if (hitPosition.hit)
                    {
                        if (stampStretch && brush == BrushTool::STAMP) // if stampStretch is on, this affects the range FindModelSelection uses
                        {
                            editSelection = FindModelSelection(canvasWidth, canvasHeight, modelWidth, lastRayHitLoc, selectRadius + stampStretchLength/2);
                        }
                        else
                        {
                            editSelection = FindModelSelection(canvasWidth, canvasHeight, modelWidth, lastRayHitLoc, selectRadius);
                        }
                    }
                    else
                    {
                        lastRayHitLoc = prevRayHitLoc; // if the cursor isnt over a model, reverse changes made to lastRayHitLoc
                    }
                    
                    if (hitPosition.hit && stampStretch && brush == BrushTool::STAMP)
                    {
                        vertexIndices = FindVertexSelection(models, editSelection, hitPosition, stampStretchLength/2+selectRadius+innerRadius); // this info will be used to find the height of the cylinders drawn around hit position
                        
                        FindStampPoints(stampRotationAngle, stampStretchLength, stamp1, stamp2, Vector2{hitPosition.position.x, hitPosition.position.z});
                    }
                    else if (hitPosition.hit && innerRadius > 0 && brush == BrushTool::STAMP)
                    {
                        vertexIndices = FindVertexSelection(models, editSelection, hitPosition, selectRadius+innerRadius);
                    }
                    else if (hitPosition.hit)
                    {
                        vertexIndices = FindVertexSelection(models, editSelection, hitPosition, selectRadius);
                    }
                }
                
                if (mouseDown && hitPosition.hit && brush == BrushTool::ELEVATION)
                {
                    if (!updateFlag) // update the history before executing if this is the first tick of the operation
                    {
                        NewHistoryStep(history, models, editSelection.selection, stepIndex, maxSteps);
                    }
                    else if (editSelection != lastEditSelection)
                    {
                        ExtendHistoryStep(history[stepIndex - 1], models, editSelection);
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
                    
                    for (int i = 0; i < editSelection.selection.size(); i++)
                    {
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[0], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[2], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                    }
                    
                    timeCounter += GetFrameTime();
                    
                    if (timeCounter >= 0.1f) // update heightmap every tenth of a second
                    {
                        for (int i = 0; i < editSelection.selection.size(); i++)
                        {
                            UpdateHeightmap(models[editSelection.selection[i].x][editSelection.selection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
                        }
                        
                        timeCounter = 0;
                    }
                    
                    updateFlag = true;
                }
                
                if (mouseDown && hitPosition.hit && brush == BrushTool::FLATTEN)
                {
                    if (!updateFlag) // update the history before executing if this is the first tick of the operation
                    {
                        NewHistoryStep(history, models, editSelection.selection, stepIndex, maxSteps);
                    }
                    else if (editSelection != lastEditSelection)
                    {
                        ExtendHistoryStep(history[stepIndex - 1], models, editSelection);
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
                    
                    for (int i = 0; i < editSelection.selection.size(); i++)
                    {
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[0], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[2], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                    }
                    
                    timeCounter += GetFrameTime();
                    
                    if (timeCounter >= 0.1f)
                    {
                        for (int i = 0; i < editSelection.selection.size(); i++)
                        {
                            UpdateHeightmap(models[editSelection.selection[i].x][editSelection.selection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
                        }
                        
                        timeCounter = 0;
                    }
                    
                    updateFlag = true;
                }
                
                if (mouseDown && hitPosition.hit && brush == BrushTool::SELECT)
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
                                temp.index = vertexIndices[i].index;
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
                                    temp.index = vertexIndices[i].index;
                                    temp.y = selectionStep + 1; // y is used here to represent when this vertex was selected
                                    vertexSelection.push_back(temp);
                                }
                            }
                        }                            
                    }
                }
                
                if (mouseDown && hitPosition.hit && brush == BrushTool::SMOOTH) // smooth by moving each vertex closer to the average y of its neighbors
                {
                    if (!updateFlag) // update the history before executing if this is the first tick of the operation
                    {
                        NewHistoryStep(history, models, editSelection.selection, stepIndex, maxSteps);
                    }
                    else if (editSelection != lastEditSelection)
                    {
                        ExtendHistoryStep(history[stepIndex - 1], models, editSelection);
                    }
                    
                    if (selectionMask) // if selection mask is on, dont modify selected vertices
                    {
                        std::vector<VertexState> vertices;
                        
                        for (int i = 0; i < vertexIndices.size(); ++i) // TODO change to a better search once vertexSelection is sorted
                        {
                            bool selected = false;
                            
                            for (int j = 0; j < vertexSelection.size(); j++)
                            {
                                if (vertexIndices[i] == vertexSelection[j])
                                {
                                    selected = true;
                                    break;
                                }
                            }
                            
                            if (!selected)
                            {
                                vertices.push_back(vertexIndices[i]);
                            }
                        }
                        
                        Smooth(models, vertices, modelVertexWidth, modelVertexHeight, canvasWidth, canvasHeight);
                    }
                    else
                    {
                        Smooth(models, vertexIndices, modelVertexWidth, modelVertexHeight, canvasWidth, canvasHeight);
                    }  

                    for (int i = 0; i < editSelection.selection.size(); i++)
                    {
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[0], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[2], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                    }
                    
                    timeCounter += GetFrameTime();
                    
                    if (timeCounter >= 0.1f) // update the heightmap at intervals of .1 seconds while the tool is being used
                    {
                        for (int i = 0; i < editSelection.selection.size(); i++)
                        {
                            UpdateHeightmap(models[editSelection.selection[i].x][editSelection.selection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                        }
                        
                        timeCounter = 0;
                    }
                    
                    updateFlag = true;
                }
                
                if (mouseDown && hitPosition.hit && brush == BrushTool::STAMP)
                {
                    if (!updateFlag) // update the history before executing if this is the first tick of the operation
                    {
                        NewHistoryStep(history, models, editSelection.selection, stepIndex, maxSteps);
                    }
                    else if (editSelection != lastEditSelection)
                    {
                        ExtendHistoryStep(history[stepIndex - 1], models, editSelection);
                    }
                    
                    static Vector2 previousLocation; // location of the last hit position
                    
                    if (stampSlope != 0 && stampDrag && !stampStretch && !IsKeyDown(KEY_F)) // adjust the select radius if there is a slope and the stamp is being dragged. if stamp stretch is true, this is done later. holding F prevents
                    {
                        float distance = xzDistance(Vector2{hitPosition.position.x, hitPosition.position.z}, previousLocation); // distance between the current and previous hit position
                        float angle = 90 - fabs(stampSlope); // right triangle with 90 degrees and stampSlope on top, angle is on bottom
                        float heightDifference = distance/sinf(angle*DEG2RAD)*sinf(fabs(stampSlope)*DEG2RAD);
                        
                        float ratio = sinf((90 - stampAngle)*DEG2RAD) / sinf(stampAngle*DEG2RAD); // rise to run ratio, used to determine how much height difference translates into select radius difference
                        
                        if (stampSlope > 0)
                            selectRadius = selectRadius + ratio * heightDifference;
                        else
                        {
                            selectRadius = selectRadius - ratio * heightDifference;
                            
                            if (selectRadius < 0)
                                selectRadius = 0;
                        }
                    }
                    
                    float influenceRadius = selectRadius + innerRadius; // select radius is expanded by inner radius
                    float heightCap = selectRadius*sinf(stampAngle*DEG2RAD)/sinf((180 - (90 + stampAngle))*DEG2RAD); // max height that can be produced by this select radius and stamp angle
                    
                    if (stampStretch) // find the vertex selection if stamp stretch is on, which has to be done differently
                    {       
                        static Vector2 stampAnchor; // compared to hit position to determine new stamp rotation
                        float direction; // angle from the last position to the new one
                        float adjustedDirection; // final angle to use to determine stamp rotation
                        float distX = fabs(hitPosition.position.x - stampAnchor.x); // stamp anchor to hit position in x
                        float distY = fabs(hitPosition.position.z - stampAnchor.y); // stamp anchor to hit position in y
                        float dist = xzDistance(Vector2{hitPosition.position.x, hitPosition.position.z}, stampAnchor); // distance from stamp anchor to hit position
                        
                        float tolerance = stampStretchLength; // distance required between the stamp anchor and hit position to update the stamp rotation
                        
                        if (stampDrag && dist > tolerance) // update the rotation angle based on the new position
                        {
                            if (hitPosition.position.x == stampAnchor.x && hitPosition.position.z > stampAnchor.y)
                            {
                                direction = 0;
                                adjustedDirection = 0;
                            }
                            else if (hitPosition.position.x > stampAnchor.x && hitPosition.position.z == stampAnchor.y)
                            {
                                direction = 90;
                                adjustedDirection = 90;
                            }
                            else if (hitPosition.position.x == stampAnchor.x && hitPosition.position.z < stampAnchor.y)
                            {
                                direction = 180;
                                adjustedDirection = 180;
                            }
                            else if (hitPosition.position.x < stampAnchor.x && hitPosition.position.z == stampAnchor.y)
                            {
                                direction = 270;
                                adjustedDirection = 270;
                            }
                            else
                            {
                                if (hitPosition.position.x > stampAnchor.x && hitPosition.position.z < stampAnchor.y)
                                {
                                    direction = asinf(distY / dist)*RAD2DEG;
                                    adjustedDirection = direction + 90;
                                }
                                else if (hitPosition.position.x < stampAnchor.x && hitPosition.position.z < stampAnchor.y)
                                {
                                    direction = asinf(distX / dist)*RAD2DEG;
                                    adjustedDirection = direction + 180;
                                }
                                else if (hitPosition.position.x < stampAnchor.x && hitPosition.position.z > stampAnchor.y)
                                {
                                    direction = asinf(distY / dist)*RAD2DEG;
                                    adjustedDirection = direction + 270;
                                }
                                else
                                {
                                    direction = asinf(distX / dist)*RAD2DEG;
                                    adjustedDirection = direction;  
                                }
                            }
                            
                            stampRotationAngle = adjustedDirection + 90.f;
                            
                            if (stampRotationAngle >= 360)
                                stampRotationAngle -= 360;
                            
                            if (stampRotationAngle < 0)
                                stampRotationAngle += 360;
                            
                            float ratio = (dist - tolerance) / dist; // used to multiply to the x and y difference in the anchor and hit position to determine new location of anchor
                            
                            stampAnchor = Vector2{(hitPosition.position.x - stampAnchor.x) * ratio + stampAnchor.x, (hitPosition.position.z - stampAnchor.y) * ratio + stampAnchor.y};
                            
                            if (stampSlope != 0 && !IsKeyDown(KEY_F)) // adjust the select radius if there is a slope and the stamp is being dragged
                            {
                                float distance = xzDistance(Vector2{hitPosition.position.x, hitPosition.position.z}, previousLocation); // distance between the current and previous hit position
                                float angle = 90 - fabs(stampSlope); // right triangle with 90 degrees and stampSlope on top, angle is on bottom
                                float heightDifference = distance/sinf(angle*DEG2RAD)*sinf(fabs(stampSlope)*DEG2RAD);
                                
                                float ratio = sinf((90 - stampAngle)*DEG2RAD) / sinf(stampAngle*DEG2RAD); // rise to run ratio, used to detemine how much height difference translates into select radius difference
                                
                                if (stampSlope > 0)
                                    selectRadius = selectRadius + ratio * heightDifference;
                                else
                                {
                                    selectRadius = selectRadius - ratio * heightDifference;
                                    
                                    if (selectRadius < 0)
                                        selectRadius = 0;
                                }
                            }
                        }
                        
                        if(!stampDrag) // set stamp anchor if this is the first edit while the mouse has been held down
                            stampAnchor = Vector2{hitPosition.position.x, hitPosition.position.z};
                        
                        FindStampPoints(stampRotationAngle, stampStretchLength, stamp1, stamp2, stampAnchor);
                        
                        RayHitInfo hp; // hit position on the mesh at the stamp anchor
                        hp.hit = false;
                        
                        if (!rayCollision2d)
                        {
                            // if stamp stretch is on, the height to add to vertexY has to be found at stamp anchor, not cursor hit position
                            Ray ray = {Vector3{stampAnchor.x, highestY, stampAnchor.y}, Vector3{0, -1, 0}}; 
                            
                            if (useGhostMesh && !ghostMesh.empty())
                            {
                                hp = FindHit3D(ray, ghostMesh, lastRayHitLoc);
                            }
                            else
                            {
                                for (int i = 0; i < editSelection.selection.size(); i++)
                                {
                                    hp = GetCollisionRayModel2(ray, &models[editSelection.selection[i].x][editSelection.selection[i].y]);
                                    
                                    if (hp.hit) 
                                        break;
                                }
                            }
                        }
                        
                        for (int i = 0; i < editSelection.selection.size(); i++) // check all models recorded by modelSelection for vertices to be modified
                        {
                            for (int j = 0; j < (models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // check this models vertices
                            {
                                float& vertexY = models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices[j + 1]; // make an alias for this montrosity
                                
                                Vector2 vertexCoords = {models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices[j], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices[j+2]};
                            
                                float dist = PointSegmentDistance(vertexCoords, stamp1, stamp2);
                                
                                if (dist <= influenceRadius)
                                {
                                    float yValue = vertexY; // old y value of this vertex
                                    
                                    if (stampFlip) // if stamp is upside down
                                    {
                                        dist = dist - innerRadius; // distance from the middle of the selection
                                        
                                        if (dist < 0) // the vertices inside inner radius will be at y = 0
                                            vertexY = 0;
                                        else
                                            vertexY = dist*sinf(stampAngle*DEG2RAD)/sinf((180 - (90 + stampAngle))*DEG2RAD); 
                                    }
                                    else
                                        vertexY = (influenceRadius - dist)*sinf(stampAngle*DEG2RAD)/sinf((180 - (90 + stampAngle))*DEG2RAD);
                                    
                                    if (vertexY > heightCap) // if the vertex is within the inner radius, limit its height extention
                                        vertexY = heightCap;
                                    
                                    if (stampInvert) // invert vertexY
                                        vertexY = -vertexY;
                                    
                                    if (stampOffset != 0) // add stamp offset
                                        vertexY += stampOffset;
                                    
                                    if (!rayCollision2d) // if the mouse cursor mode is 3d, add the hit position y to vertexY
                                    {
                                        vertexY += hp.position.y;
                                    }
                                    
                                    if (stampHeight && vertexY > stampHeight) // dont allow vertexY to go higher than what stampHeight (cut off) is set to
                                        vertexY = stampHeight;
                                        
                                    if (raiseOnly && vertexY < yValue) // if raise only is on and vertex has been lowered, reverse it
                                        vertexY = yValue;
                                        
                                    if (lowerOnly && vertexY > yValue) // if lower only is on and vertex has been raised, reverse it
                                        vertexY = yValue;
                                }
                            }
                        }
                    }
                    else
                    {
                        for (int i = 0; i < vertexIndices.size(); i++)
                        {
                            float& vertexY = models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1]; // make an alias for this montrosity
                            
                            Vector2 vertexPos = {models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index], models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 2]};
                            
                            float dist = influenceRadius - xzDistance(Vector2{hitPosition.position.x, hitPosition.position.z}, vertexPos); // distance from the edge of the selection radius
                            float yValue = vertexY;// old y value of this vertex

                            if (stampFlip) // if stamp is upside down
                            {
                                dist = xzDistance(Vector2{hitPosition.position.x, hitPosition.position.z}, vertexPos) - innerRadius; // distance from the middle of the selection
                                
                                if (dist < 0) // the vertices inside inner radius will be at y = 0
                                    vertexY = 0;
                                else
                                    vertexY = dist*sinf(stampAngle*DEG2RAD)/sinf((180 - (90 + stampAngle))*DEG2RAD); 
                            }
                            else
                                vertexY = dist*sinf(stampAngle*DEG2RAD)/sinf((180 - (90 + stampAngle))*DEG2RAD);
                            
                            if (vertexY > heightCap) // if the vertex is within the inner radius, limit its height extention
                                vertexY = heightCap;
                            
                            if (stampInvert) // invert vertexY
                                vertexY = -vertexY;
                            
                            if (stampOffset != 0) // add stamp offset
                                vertexY += stampOffset;
                            
                            if (!rayCollision2d) // if the mouse cursor mode is 3d, add the hit position y to vertexY
                                vertexY += hitPosition.position.y;
                                
                            if (stampHeight && vertexY > stampHeight) // dont allow vertexY to go higher than what stampHeight (cut off) is set to
                                vertexY = stampHeight;
                                
                            if (raiseOnly && vertexY < yValue) // if raise only is on and vertex has been lowered, reverse it
                                vertexY = yValue;
                                
                            if (lowerOnly && vertexY > yValue) // if lower only is on and vertex has been raised, reverse it
                                vertexY = yValue;
                        }
                    }
                    
                    stampDrag = true; // set to true so subsequent edits will act accordingly. reset to false on mouse click release
                    previousLocation = {hitPosition.position.x, hitPosition.position.z}; // change previous location to current location so it's ready for the next tick
                    
                    for (int i = 0; i < editSelection.selection.size(); i++)
                    {
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[0], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                        rlUpdateBuffer(models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vboId[2], models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertices, models[editSelection.selection[i].x][editSelection.selection[i].y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
                    }
                    
                    timeCounter += GetFrameTime();
                    
                    if (timeCounter >= 0.1f) // update heightmap every tenth of a second
                    {
                        for (int i = 0; i < editSelection.selection.size(); i++)
                        {
                            UpdateHeightmap(models[editSelection.selection[i].x][editSelection.selection[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
                        }
                        
                        timeCounter = 0;
                    }
                    
                    updateFlag = true;
                }
            
                if (hitPosition.hit)
                {
                    lastEditSelection = editSelection; // update what the lastEditSelection was 
                }
            }
            else
            {
                Ray ray = GetMouseRay(GetMousePosition(), camera);
                
                if (IsKeyDown(KEY_G) && mousePressed && !models.empty()) // move model selection, centered on the model clicked on
                {
                    for (int i = 0; i < models.size(); i++) // test ray against all models
                    {
                        for (int j = 0; j < models[i].size(); j++)
                        {
                            hitPosition = GetCollisionRayModel2(ray, &models[i][j]); 
                            
                            if (hitPosition.hit) 
                            {
                                modelSelection.selection.clear();
                                modelSelection.expandedSelection.clear();
                                
                                Vector2 topLeft; // the top left model of the new selection, start with i and j as coordinates
                                topLeft.x = i;
                                topLeft.y = j;
                                
                                if (modelSelection.width != 1) // if the width is 1, topLeft.x is already known
                                {
                                    if (!(modelSelection.width % 2)) // if the width is even, shift x by half with selection width - 1
                                    {
                                        topLeft.x -= (modelSelection.width / 2 - 1);
                                    }
                                    else // if the width is odd, shift x by half the selection width, remainder truncated
                                    {
                                        topLeft.x -= modelSelection.width / 2;
                                    }
                                }
                                
                                if (topLeft.x < 0) // if x was corrected past 0, set to 0
                                    topLeft.x = 0;
                                    
                                if (modelSelection.height != 1) // if the height is 1, topLeft.y is already known
                                {
                                    if (!(modelSelection.height % 2)) // if the height is even, shift y by half with selection height - 1
                                    {
                                        topLeft.y -= (modelSelection.height / 2 - 1);
                                    }
                                    else // if the height is odd, shift x by half the selection height, remainder truncated
                                    {
                                        topLeft.y -= modelSelection.height / 2;
                                    }
                                }
                                
                                if (topLeft.y < 0) // if x was corrected past 0, set to 0
                                    topLeft.y = 0;
                                
                                if (topLeft.x + modelSelection.width > canvasWidth) // make sure it wont extend out of bounds
                                    topLeft.x = canvasWidth - modelSelection.width;
                                
                                if (topLeft.y + modelSelection.height > canvasHeight)
                                    topLeft.y = canvasHeight - modelSelection.height;
                                
                                modelSelection.topLeft = topLeft;
                                modelSelection.bottomRight = Vector2{topLeft.x + modelSelection.width - 1, topLeft.y + modelSelection.height - 1};
                                
                                for (int i2 = 0; i2 < modelSelection.width; i2++)
                                {
                                    for (int j2 = 0; j2 < modelSelection.height; j2++)
                                    {
                                        modelSelection.selection.push_back(Vector2{i2 + topLeft.x, j2 + topLeft.y});
                                    }
                                }
                                
                                SetExSelection(modelSelection, canvasWidth, canvasHeight);
                                
                                break;
                            }
                        }
                        
                        if (hitPosition.hit) break;
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
                        Ray ray = GetMouseRay(GetMousePosition(), camera);
                        
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
                            cameraSetting = CameraSetting::CHARACTER;
                            DisableCursor();
                        }
                    }
                }
                else
                {
                    timeCounter = 0;
                    
                    if (stampDrag)
                    {
                        stampDrag = false;
                    }
                    
                    if (updateFlag) // if an edit was just completed
                    {
                        FinalizeHistoryStep(history[stepIndex - 1], models);
                        
                        for (int i = 0; i < history[stepIndex - 1].modelCoords.size(); i++)
                        {
                            UpdateNormals(models[history[stepIndex - 1].modelCoords[i].x][history[stepIndex - 1].modelCoords[i].y], modelVertexWidth, modelVertexHeight);
                            UpdateHeightmap(models[history[stepIndex - 1].modelCoords[i].x][history[stepIndex - 1].modelCoords[i].y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                        }
                        
                        updateFlag = false;                
                    }
                }
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z) && stepIndex > 0 && !history.empty()) // undo key
            {
                for (int i = 0; i < history[stepIndex - 1].startingVertices.size(); i++) // reinstate the previous state of the mesh as recorded by the startingVertices at stepIndex - 1
                {
                    int x = history[stepIndex - 1].startingVertices[i].coords.x;
                    int y = history[stepIndex - 1].startingVertices[i].coords.y;
                    
                    models[x][y].meshes[0].vertices[history[stepIndex - 1].startingVertices[i].index + 1] = history[stepIndex - 1].startingVertices[i].y; 
                }
                
                for (int i = 0; i < history[stepIndex - 1].modelCoords.size(); i++)
                {
                    int x = history[stepIndex - 1].modelCoords[i].x;
                    int y = history[stepIndex - 1].modelCoords[i].y;
                    
                    UpdateNormals(models[x][y], modelVertexWidth, modelVertexHeight);
                    
                    rlUpdateBuffer(models[x][y].meshes[0].vboId[0], models[x][y].meshes[0].vertices, models[x][y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                    rlUpdateBuffer(models[x][y].meshes[0].vboId[2], models[x][y].meshes[0].vertices, models[x][y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
           
                    UpdateHeightmap(models[x][y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
                }         

                stepIndex--;
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_X) && !history.empty() && stepIndex < (int)history.size()) // redo key
            {
                for (int i = 0; i < history[stepIndex].endingVertices.size(); i++) // reinstate the previous state of the mesh as recorded by the endingVertices at stepIndex
                {
                    int x = history[stepIndex].endingVertices[i].coords.x;
                    int y = history[stepIndex].endingVertices[i].coords.y;
                    
                    models[x][y].meshes[0].vertices[history[stepIndex].endingVertices[i].index + 1] = history[stepIndex].endingVertices[i].y;
                }           

                for (int i = 0; i < history[stepIndex].modelCoords.size(); i++)
                {
                    int x = history[stepIndex].modelCoords[i].x;
                    int y = history[stepIndex].modelCoords[i].y;
                    
                    UpdateNormals(models[x][y], modelVertexWidth, modelVertexHeight);
                    
                    rlUpdateBuffer(models[x][y].meshes[0].vboId[0], models[x][y].meshes[0].vertices, models[x][y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex position 
                    rlUpdateBuffer(models[x][y].meshes[0].vboId[2], models[x][y].meshes[0].vertices, models[x][y].meshes[0].vertexCount*3*sizeof(float));    // Update vertex normals 
           
                    UpdateHeightmap(models[x][y], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
                }   

                stepIndex++;               
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D) && !vertexSelection.empty()) // deselect
            {
                vertexSelection.clear();
            }
            
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) // hotkey for updating the texture
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
                
                for (int i = 0; i < models.size(); i++) // update normals
                {
                    for (int j = 0; j < models[i].size(); j++)
                    {
                        UpdateNormals(models[i][j], modelVertexWidth, modelVertexHeight);
                    }
                }
                
                UpdateHeightmap(models, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode); 
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
                    ProcessInput(GetKeyPressed(), stampAngleString, stampAngle, inputFocus, 5);
                    break;
                }
                case InputFocus::STAMP_HEIGHT:
                {
                    int key = GetKeyPressed();
                    
                    if (((key >= 48 && key <= 57) || key == 46) && ((int)stampHeightString.size() < 5))
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
                    ProcessInput(GetKeyPressed(), toolStrengthString, toolStrength, inputFocus, 5);
                    break;
                }
                case InputFocus::SELECT_RADIUS:
                {
                    ProcessInput(GetKeyPressed(), selectRadiusString, selectRadius, inputFocus, 5);
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
                case InputFocus::INNER_RADIUS:
                {
                    int key = GetKeyPressed();
                    
                    if (((key >= 48 && key <= 57) || key == 46) && ((int)innerRadiusString.size() < 5))
                    {
                        innerRadiusString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !innerRadiusString.empty())
                        innerRadiusString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (innerRadiusString.empty())
                            inputFocus = InputFocus::NONE;
                        else
                        {
                            innerRadius = stof(innerRadiusString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
                case InputFocus::STRETCH_LENGTH:
                {
                    ProcessInput(GetKeyPressed(), stretchLengthString, stampStretchLength, inputFocus, 5);
                    break;
                }
                case InputFocus::STAMP_ROTATION:
                {
                    ProcessInput(GetKeyPressed(), stampRotationString, stampRotationAngle, inputFocus, 5);
                    break;
                }
                case InputFocus::STAMP_SLOPE:
                {
                    int key = GetKeyPressed();
                    
                    if (((key >= 48 && key <= 57) || key == 46 || key == 45) && ((int)stampSlopeString.size() < 5))
                    {
                        stampSlopeString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !stampSlopeString.empty())
                        stampSlopeString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (stampSlopeString.empty())
                            inputFocus = InputFocus::NONE;
                        else
                        {
                            stampSlope = stof(stampSlopeString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
                case InputFocus::STAMP_OFFSET:
                {
                    int key = GetKeyPressed();
                    
                    if (((key >= 48 && key <= 57) || key == 46) && ((int)stampOffsetString.size() < 5))
                    {
                        stampOffsetString.push_back((char)key);
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && !stampOffsetString.empty())
                        stampOffsetString.pop_back();
                    
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                    {
                        if (stampOffsetString.empty())
                            inputFocus = InputFocus::NONE;
                        else
                        {
                            stampOffset = stof(stampOffsetString);
                            inputFocus = InputFocus::NONE; 
                        }
                    }
                    
                    break;
                }
            }
            
    /**********************************************************************************************************************************************************************
        DRAWING
    **********************************************************************************************************************************************************************/
            
            BeginDrawing();
            
                ClearBackground(Color{240, 240, 240, 255});
                
                BeginMode3D(camera);
                
                    if (!models.empty()) // draw models
                    {
                        for (int i = 0; i < models.size(); i++)
                        {
                            for (int j = 0; j < models[i].size(); j++)
                            {
                                DrawModel(models[i][j], Vector3{0, 0, 0}, 1.0f, WHITE);
                            }
                        }
                    }
                    
                    if (models.empty()) DrawGrid(100, 1.0f);
                    
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
                            Vector3 v = {models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].index], models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].index + 1], models[vertexSelection[i].coords.x][vertexSelection[i].coords.y].meshes[0].vertices[vertexSelection[i].index + 2]};
                            
                            DrawCube(v, 0.03f, 0.03f, 0.03f, vertexColor);
                        }
                    }
                    
                    if (hitPosition.hit) // draw brush influence cylinder
                    {
                        if (brush == BrushTool::SELECT)
                        {
                            int increment = (vertexIndices.size() / 500) + 1; // cull some vertex draw calls with larger selections
                            
                             // draw every highlighted vertex
                            for (int i = 0; i < vertexIndices.size(); i += increment)
                            {
                                Vector3 v = {models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index], models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1], models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 2]};
                                DrawCube(v, 0.03f, 0.03f, 0.03f, YELLOW);
                            }
                        }
                        else
                        {
                            float cylinderHeight = 0;
                            
                            for (int i = 0; i < vertexIndices.size(); i++) // find highest vertex and adjust cylinder height accordingly
                            {
                                if (models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1] > cylinderHeight)
                                    cylinderHeight = models[vertexIndices[i].coords.x][vertexIndices[i].coords.y].meshes[0].vertices[vertexIndices[i].index + 1];
                            }
                            
                            if (stampStretch && brush == BrushTool::STAMP)
                            {
                                DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {255, 0, 0, 20});
                                DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {255, 0, 0, 40});  
                                
                                DrawCylinderWires(Vector3 {stamp1.x, 0, stamp1.y}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {126, 0, 255, 20});
                                DrawCylinder(Vector3 {stamp1.x, 0, stamp1.y}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {126, 0, 255, 40});  
                           
                                DrawCylinderWires(Vector3 {stamp2.x, 0, stamp2.y}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {126, 0, 255, 20});
                                DrawCylinder(Vector3 {stamp2.x, 0, stamp2.y}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {126, 0, 255, 40});  
                                
                                DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius + (stampStretchLength/2) + innerRadius, selectRadius + (stampStretchLength/2) + innerRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 20});
                                DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius + (stampStretchLength/2) + innerRadius, selectRadius + (stampStretchLength/2) + innerRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 40});
                            }
                            else if (brush == BrushTool::STAMP)
                            {
                                DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {255, 0, 0, 20});
                                DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {255, 0, 0, 40}); 
                                
                                DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius + innerRadius, selectRadius + innerRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 20});
                                DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius + innerRadius, selectRadius + innerRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 40});   
                            }                  
                            else
                            {
                                DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {255, 0, 0, 20});
                                DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, 0.1, 0.1, cylinderHeight + 0.09, 19, Color {255, 0, 0, 40}); 
                                
                                DrawCylinderWires(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius, selectRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 20});
                                DrawCylinder(Vector3 {hitPosition.position.x, 0, hitPosition.position.z}, selectRadius, selectRadius, cylinderHeight + 0.1, 19, Color {255, 255, 255, 40});  
                            }
                        }
                    }
                    
                    if (!modelSelection.selection.empty()) // draw model selection frame
                    {
                        float realModelWidth = modelWidth - (1 / 120.f) * modelWidth; // modelWidth minus the width of one polygon. ASSUMES MODELVERTEXWIDTH = 120
                        float cubeY;
                        
                        if (highestY > realModelWidth)
                            cubeY = highestY;
                        else
                            cubeY = realModelWidth;
                        
                        Vector3 position = {(realModelWidth * modelSelection.width) / 2.f + (modelSelection.topLeft.x * realModelWidth), cubeY / 2.f, (realModelWidth * modelSelection.height) / 2.f + (modelSelection.topLeft.y * realModelWidth)};
                        
                        DrawCubeWires(position, realModelWidth * modelSelection.width, cubeY, realModelWidth * modelSelection.height, SKYBLUE); 
                    }
                    
                EndMode3D();
                DrawText(FormatText("%03.02f", highestY), 1700, 30, 15, BLACK);
                /* debug text
                if (editSelection.selection.size())
                {
                    for (int i = 0; i < editSelection.selection.size(); i++)
                    {
                        DrawText(FormatText("%02.01f", editSelection.selection[i].x), 1700, 30 + i * 30, 15, BLACK);
                        DrawText(FormatText("%02.01f", editSelection.selection[i].y), 1730, 30 + i * 30, 15, BLACK);
                    }
                }
                
                DrawText(FormatText("%f", lastRayHitLoc.x), 165, 30, 15, BLACK);
                DrawText(FormatText("%f", lastRayHitLoc.y), 165, 50, 15, BLACK);
                
                if (hitPosition.hit)
                    DrawText(FormatText("%f", hitPosition.position.x), 400, 30, 15, BLACK);
                
                if (history.size())
                {
                    for (int i = 0; i < history[stepIndex - 1].modelCoords.size(); i++)
                    {
                        DrawText(FormatText("%02.01f", history[stepIndex - 1].modelCoords[i].x), 1700, 30 + i * 30, 15, BLACK);
                        DrawText(FormatText("%02.01f", history[stepIndex - 1].modelCoords[i].y), 1730, 30 + i * 30, 15, BLACK);
                    }
                }
                
                float temp = canvasHeight;//history.size();
                float temp2 = canvasWidth;//stepIndex;
                
                
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
                        DrawTextRec(GetFontDefault(), "Canvas", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        break;
                    }
                    case Panel::HEIGHTMAP:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Canvas", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(exportButton, GRAY);
                        DrawRectangleRec(meshGenButton, GRAY);
                        DrawRectangleRec(loadButton, GRAY);
                        DrawRectangleRec(updateTextureButton, GRAY);
                        DrawRectangleRec(directoryButton, GRAY);
                        DrawTextRec(GetFontDefault(), "Update Texture", Rectangle {updateTextureButton.x + 5, updateTextureButton.y + 1, updateTextureButton.width - 2, updateTextureButton.height - 2}, 15, 1.f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Generate Mesh", Rectangle {meshGenButton.x + 5, meshGenButton.y + 1, meshGenButton.width - 2, meshGenButton.height - 2}, 15, 1.f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Export Heightmap", Rectangle {exportButton.x + 5, exportButton.y + 1, exportButton.width - 2, exportButton.height - 2}, 15, 1.f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Load Heightmap", Rectangle {loadButton.x + 5, loadButton.y + 1, loadButton.width - 2, loadButton.height - 2}, 15, 1.f, true, BLACK);
                        DrawTextRec(GetFontDefault(), "Change Directory", Rectangle {directoryButton.x + 5, directoryButton.y + 1, directoryButton.width - 2, directoryButton.height - 2}, 15, 1.f, true, BLACK);
                        
                        DrawRectangleRec(xMeshBox, WHITE);
                        DrawRectangleRec(zMeshBox, WHITE);
                        DrawText("X:", 14, 33, 15, BLACK);
                        DrawText("Y:", 14, 58, 15, BLACK);
                        
                        DrawRectangleRec(grayscaleTexBox, WHITE);
                        DrawRectangleRec(slopeTexBox, WHITE);
                        DrawRectangleRec(rainbowTexBox, WHITE);
                        DrawText("Grayscale:", 5, 391, 11, BLACK);
                        DrawText("Slope:", 5, 410, 11, BLACK);
                        DrawText("Rainbow:", 5, 429, 11, BLACK);
                        
                        switch (heightMapMode)
                        {
                            case HeightMapMode::GRAYSCALE:
                            {
                                DrawText("+", grayscaleTexBox.x + 2, grayscaleTexBox.y - 2, 20, BLACK);
                                break;
                            }
                            case HeightMapMode::SLOPE:
                            {
                                DrawText("+", slopeTexBox.x + 2, slopeTexBox.y - 2, 20, BLACK);
                                break;
                            }
                            case HeightMapMode::RAINBOW:
                            {
                                DrawText("+", rainbowTexBox.x + 2, rainbowTexBox.y - 2, 20, BLACK);
                                break;
                            }
                        }
                        
                        if (inputFocus == InputFocus::X_MESH)
                            DrawRectangleLinesEx(xMeshBox, 1, BLACK);
                        else if (inputFocus == InputFocus::Z_MESH)
                            DrawRectangleLinesEx(zMeshBox, 1, BLACK);
                        
                        PrintBoxInfo(xMeshBox, inputFocus, InputFocus::X_MESH, xMeshString, canvasWidth);
                        PrintBoxInfo(zMeshBox, inputFocus, InputFocus::Z_MESH, zMeshString, canvasHeight);
                        
                        break;
                    }
                    case Panel::CAMERA:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Canvas", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(characterButton, GRAY);
                        DrawText("Character", 4, 60, 11, BLACK);
                        DrawText("Camera", 4, 72, 11, BLACK);
                        DrawText("Drag & Drop", 15, 92, 11, BLACK);
                        
                        DrawRectangleRec(cameraSettingBox, WHITE);
                        DrawText("Top Down", 30, 122, 11, BLACK);
                        
                        //DrawText("Camera Sensitivity:", 5, 110, 11, BLACK);
                        
                        if (cameraSetting == CameraSetting::TOP_DOWN)
                            DrawText("+", cameraSettingBox.x + 2, cameraSettingBox.y - 2, 20, BLACK);
                        
                        if (!characterDrag) // dont draw the circle in the rectangle if it's being dragged
                            DrawCircle(characterButton.x + (characterButton.width / 2 + 1), characterButton.y + (characterButton.height / 2 + 1), characterButton.width / 2 - 4, ORANGE);
                        
                        break;
                    }
                    case Panel::TOOLS:
                    {
                        DrawRectangleRec(heightmapTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Canvas", Rectangle {heightmapTab.x + 3, heightmapTab.y + 3, heightmapTab.width - 2, heightmapTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel1, panelColor);
                        
                        DrawRectangleRec(cameraTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Camera", Rectangle {cameraTab.x + 3, cameraTab.y + 3, cameraTab.width - 2, cameraTab.height - 2}, 15, 1.f, false, BLACK);
                        
                        DrawRectangleRec(panel2, panelColor);
                        
                        DrawRectangleRec(toolsTab, GRAY);
                        DrawTextRec(GetFontDefault(), "Tools", Rectangle {toolsTab.x + 3, toolsTab.y + 3, toolsTab.width - 2, toolsTab.height - 2}, 15, 1.f, false, BLACK);
                        
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
                            DrawTextRec(GetFontDefault(), "Elevation", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, smoothToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Smooth", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, flattenToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Flatten", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, stampToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Stamp", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (CheckCollisionPointRec(mousePosition, selectToolButton))
                        {
                            DrawTextRec(GetFontDefault(), "Select", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (brush == BrushTool::ELEVATION)
                        {
                            DrawTextRec(GetFontDefault(), "Elevation", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (brush == BrushTool::SMOOTH)
                        {
                            DrawTextRec(GetFontDefault(), "Smooth", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (brush == BrushTool::FLATTEN)
                        {
                            DrawTextRec(GetFontDefault(), "Flatten", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (brush == BrushTool::STAMP)
                        {
                            DrawTextRec(GetFontDefault(), "Stamp", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
                        }
                        else if (brush == BrushTool::SELECT)
                        {
                            DrawTextRec(GetFontDefault(), "Select", Rectangle {toolText.x + 3, toolText.y + 3, toolText.width - 2, toolText.height - 2}, 15, 1.0f, false, BLACK);
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
                            DrawRectangleRec(stampFlipBox, WHITE);
                            DrawRectangleRec(innerRadiusBox, WHITE);
                            DrawRectangleRec(stampInvertBox, WHITE);
                            DrawRectangleRec(stampStretchBox, WHITE);
                            DrawRectangleRec(stretchLengthBox, WHITE);
                            DrawRectangleRec(stampRotationBox, WHITE);
                            DrawRectangleRec(stampSlopeBox, WHITE);
                            DrawRectangleRec(stampOffsetBox, WHITE);
                            DrawRectangleRec(ghostMeshBox, WHITE);
                            
                            DrawText("Angle:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 162, 11, BLACK);
                            DrawText("Cut Off:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 181, 11, BLACK);
                            DrawText("Flip:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 200, 11, BLACK);
                            DrawText("Radius:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 219, 11, BLACK);
                            DrawText("Invert:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 238, 11, BLACK);
                            DrawText("Stretch:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 257, 11, BLACK);
                            
                            if (stampStretch)
                            {
                                DrawText("Length:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 276, 11, BLACK);
                                DrawText("Rotation:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 295, 11, BLACK);
                            }
                            else
                            {
                                DrawText("Length:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 276, 11, GRAY);
                                DrawText("Rotation:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 295, 11, GRAY);
                            }
                            
                            DrawText("Slope:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 314, 11, BLACK);
                            DrawText("Offset:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 333, 11, BLACK);
                            DrawText("Ghost Mesh:", toolButtonAnchor.x + 5, toolButtonAnchor.y + 352, 11, BLACK);
                            
                            if (stampFlip)
                                DrawText("+", stampFlipBox.x + 2, stampFlipBox.y - 2, 20, BLACK);
                            
                            if (stampInvert)
                                DrawText("+", stampInvertBox.x + 2, stampInvertBox.y - 2, 20, BLACK);
                            
                            if (stampStretch)
                                DrawText("+", stampStretchBox.x + 2, stampStretchBox.y - 2, 20, BLACK);
                            
                            if (useGhostMesh)
                                DrawText("+", ghostMeshBox.x + 2, ghostMeshBox.y - 2, 20, BLACK);
                            
                            if (inputFocus == InputFocus::STAMP_ANGLE)
                                DrawRectangleLinesEx(stampAngleBox, 1, BLACK);
                            else if (inputFocus == InputFocus::STAMP_HEIGHT)
                                DrawRectangleLinesEx(stampHeightBox, 1, BLACK);
                            else if (inputFocus == InputFocus::INNER_RADIUS)
                                DrawRectangleLinesEx(innerRadiusBox, 1, BLACK);
                            else if (inputFocus == InputFocus::STRETCH_LENGTH)
                                DrawRectangleLinesEx(stretchLengthBox, 1, BLACK);
                            else if (inputFocus == InputFocus::STAMP_ROTATION)
                                DrawRectangleLinesEx(stampRotationBox, 1, BLACK);
                            else if (inputFocus == InputFocus::STAMP_SLOPE)
                                DrawRectangleLinesEx(stampSlopeBox, 1, BLACK);
                            else if (inputFocus == InputFocus::STAMP_OFFSET)
                                DrawRectangleLinesEx(stampOffsetBox, 1, BLACK);
                            
                            PrintBoxInfo(stampAngleBox, inputFocus, InputFocus::STAMP_ANGLE, stampAngleString, stampAngle);
                            PrintBoxInfo(stampHeightBox, inputFocus, InputFocus::STAMP_HEIGHT, stampHeightString, stampHeight);
                            PrintBoxInfo(innerRadiusBox, inputFocus, InputFocus::INNER_RADIUS, innerRadiusString, innerRadius);
                            PrintBoxInfo(stretchLengthBox, inputFocus, InputFocus::STRETCH_LENGTH, stretchLengthString, stampStretchLength);
                            PrintBoxInfo(stampRotationBox, inputFocus, InputFocus::STAMP_ROTATION, stampRotationString, stampRotationAngle);
                            PrintBoxInfo(stampSlopeBox, inputFocus, InputFocus::STAMP_SLOPE, stampSlopeString, stampSlope);
                            PrintBoxInfo(stampOffsetBox, inputFocus, InputFocus::STAMP_OFFSET, stampOffsetString, stampOffset);
                        }
                        
                        if (brush == BrushTool::SMOOTH)
                        {
                            DrawRectangleRec(smoothMeshesButton, GRAY);
                            
                            DrawTextRec(GetFontDefault(), "Smooth Mesh", Rectangle {smoothMeshesButton.x + 3, smoothMeshesButton.y + 3, smoothMeshesButton.width - 2, smoothMeshesButton.height - 2}, 13, 0.5f, false, BLACK);
                            DrawTextRec(GetFontDefault(), "Selection", Rectangle {smoothMeshesButton.x + 3, smoothMeshesButton.y + 14, smoothMeshesButton.width - 2, smoothMeshesButton.height - 2}, 13, 0.5f, false, BLACK);
                        }
                        
                        DrawLine(6, 180, 95, 180, BLACK);
                        
                        DrawRectangleRec(xMeshSelectBox, WHITE);
                        DrawRectangleRec(zMeshSelectBox, WHITE);
                        DrawRectangleRec(meshSelectButton, GRAY);
                        DrawRectangleRec(meshSelectUpButton, GRAY);
                        DrawRectangleRec(meshSelectLeftButton, GRAY);
                        DrawRectangleRec(meshSelectRightButton, GRAY);
                        DrawRectangleRec(meshSelectDownButton, GRAY);
                        
                        DrawText("X:", meshSelectAnchor.x + 5, meshSelectAnchor.y + 12, 15, BLACK);
                        DrawText("Y:", meshSelectAnchor.x + 53, meshSelectAnchor.y + 12, 15, BLACK);
                        DrawTextRec(GetFontDefault(), "Mesh Select", Rectangle {meshSelectButton.x + 3, meshSelectButton.y + 3, meshSelectButton.width - 2, meshSelectButton.height - 2}, 13, 1.0f, false, BLACK);
                        
                        DrawRectangleRec(selectRadiusBox, WHITE);
                        DrawRectangleRec(toolStrengthBox, WHITE);
                        DrawRectangleRec(raiseOnlyBox, WHITE);
                        DrawRectangleRec(lowerOnlyBox, WHITE);
                        DrawRectangleRec(collisionTypeBox, WHITE);
                        DrawRectangleRec(editQueueBox, WHITE);
                        DrawText("Radius:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 113, 11, BLACK);
                        DrawText("Strength:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 94, 11, BLACK);
                        DrawText("Raise Only:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 75, 11, BLACK);
                        DrawText("Lower Only:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 56, 11, BLACK);
                        DrawText("Cursor Ray:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 37, 11, BLACK);
                        DrawText("Edit Queue:", toolButtonAnchor.x + 5, toolButtonAnchor.y - 18, 11, BLACK);
                            
                        if (rayCollision2d)
                            DrawText("2D", collisionTypeBox.x + 1, collisionTypeBox.y + 2, 10, BLACK);
                        else
                            DrawText("3D", collisionTypeBox.x + 1, collisionTypeBox.y + 2, 10, BLACK);
                            
                        if (raiseOnly)
                            DrawText("+", raiseOnlyBox.x + 2, raiseOnlyBox.y - 2, 20, BLACK);
                        
                        if (lowerOnly)
                            DrawText("+", lowerOnlyBox.x + 2, lowerOnlyBox.y - 2, 20, BLACK);
                        
                        if (raiseOnly)
                            DrawText("+", raiseOnlyBox.x + 2, raiseOnlyBox.y - 2, 20, BLACK);
                        
                        if (editQueue)
                            DrawText("+", editQueueBox.x + 2, editQueueBox.y - 2, 20, RED);
                        
                        if (inputFocus == InputFocus::X_MESH_SELECT)
                            DrawRectangleLinesEx(xMeshSelectBox, 1, BLACK);
                        else if (inputFocus == InputFocus::Z_MESH_SELECT)
                            DrawRectangleLinesEx(zMeshSelectBox, 1, BLACK);
                        else if (inputFocus == InputFocus::TOOL_STRENGTH)
                            DrawRectangleLinesEx(toolStrengthBox, 1, BLACK);
                        else if (inputFocus == InputFocus::SELECT_RADIUS)
                            DrawRectangleLinesEx(selectRadiusBox, 1, BLACK);    
                        
                        PrintBoxInfo(xMeshSelectBox, inputFocus, InputFocus::X_MESH_SELECT, xMeshSelectString, modelSelection.width);
                        PrintBoxInfo(zMeshSelectBox, inputFocus, InputFocus::Z_MESH_SELECT, zMeshSelectString, modelSelection.height);
                        PrintBoxInfo(toolStrengthBox, inputFocus, InputFocus::TOOL_STRENGTH, toolStrengthString, toolStrength);
                        PrintBoxInfo(selectRadiusBox, inputFocus, InputFocus::SELECT_RADIUS, selectRadiusString, selectRadius);
                        
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
                    DrawRectangleRec(saveWindowGrayscale, WHITE);
                    DrawRectangleRec(saveWindow32bit, WHITE);
                    
                    DrawTextRec(GetFontDefault(), "Save", Rectangle {saveWindowSaveButton.x + 3, saveWindowSaveButton.y + 3, saveWindowSaveButton.width - 2, saveWindowSaveButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawTextRec(GetFontDefault(), "Cancel", Rectangle {saveWindowCancelButton.x + 3, saveWindowCancelButton.y + 3, saveWindowCancelButton.width - 2, saveWindowCancelButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawText("Save heightmap as .png:", saveWindowAnchor.x + 6, saveWindowAnchor.y + 12, 17, BLACK);
                    DrawText("Reference height (for scale):", saveWindowAnchor.x + 6, saveWindowAnchor.y + 87, 17, BLACK);
                    DrawText("Save as grayscale (8 bits)", saveWindowGrayscale.x + 16, saveWindowGrayscale.y - 1, 15, BLACK);
                    DrawText("Save using 4 channels (32 bits)", saveWindow32bit.x + 16, saveWindow32bit.y - 1, 15, BLACK);
                    
                    if (saveGrayscale)
                        DrawText("+", saveWindowGrayscale.x + 1, saveWindowGrayscale.y - 3, 20, BLACK);
                    else
                        DrawText("+", saveWindow32bit.x + 1, saveWindow32bit.y - 3, 20, BLACK);
                    
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
                    DrawRectangleRec(loadWindowGrayscale, WHITE);
                    DrawRectangleRec(loadWindow32bit, WHITE);
                    
                    DrawTextRec(GetFontDefault(), "Load", Rectangle {loadWindowLoadButton.x + 3, loadWindowLoadButton.y + 3, loadWindowLoadButton.width - 2, loadWindowLoadButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawTextRec(GetFontDefault(), "Cancel", Rectangle {loadWindowCancelButton.x + 3, loadWindowCancelButton.y + 3, loadWindowCancelButton.width - 2, loadWindowCancelButton.height - 2}, 15, 0.5f, false, BLACK);
                    DrawText("Load heightmap (png):", loadWindowAnchor.x + 6, loadWindowAnchor.y + 12, 17, BLACK);
                    DrawText("Pure white height value (for scale):", loadWindowAnchor.x + 6, loadWindowAnchor.y + 87, 17, BLACK);
                    DrawText("Image is grayscale", saveWindowGrayscale.x + 16, saveWindowGrayscale.y - 1, 15, BLACK);
                    DrawText("Image is split png", saveWindow32bit.x + 16, saveWindow32bit.y - 1, 15, BLACK);
                    
                    if (loadGrayscale)
                        DrawText("+", loadWindowGrayscale.x + 1, loadWindowGrayscale.y - 3, 20, BLACK);
                    else
                        DrawText("+", loadWindow32bit.x + 1, loadWindow32bit.y - 3, 20, BLACK);
                    
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
    
    CloseWindow();
    
    return 0;
}


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
// incremented trail tool - 
// selective history deletion on mesh shrink
// toggle render non selected models
// trail tool only raise / lower setting
// brush that takes the average of the normals of the first selection and then flattens out perpedicularly to that
// vertical line from hitPosition the height of highest y when on ground collision
// box selection
// dynamic history capacity 
// mesh interpolation
// setting that checks mouse hit position distances between frames and interpolates between them by queueing actions  
// precise export image size
// shading based on angle
// smooth tool angle clamp
// slope finder: find the slope between two points
// camera lock onto hitposition at certain angle and distance
// brush height cap according to adjustable plane / mesh
// make it so pressing x doesnt overlap with shift or ctrl x
// insert a size reference dummy model (human, tree, etc)

// -crash after ~141 history steps (if 3x3+ mesh?)
// -loading 2x3 image 'test' twice in a row crashes
// free fly camera moves slowly when strafing looking down
// hit detection passing through mesh when camera is at certain relative angles / distances















// FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------------------------

float xzDistance(Vector2 p1, Vector2 p2)
{
    float side1 = abs(p1.x - p2.x);
    float side2 = abs(p1.y - p2.y);
    
    return sqrt((side1 * side1) + (side2 * side2));
}


void NewHistoryStep(std::vector<HistoryStep>& history, const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, int& stepIndex, int maxSteps)
{
    //check for out of bounds models that have been deleted
    
    // if moving forward from a place in history before the last step, clear all subsequent steps
    if (stepIndex < (int)history.size())
    {
        history.erase(history.begin() + stepIndex, history.end());
    }
    
    // if history reaches max size, remove first element. (have it erase more than one so that reallocation happens less often?)
    if (history.size() == maxSteps)
    {
        history.erase(history.begin());
        stepIndex = history.size();
    }

    std::vector<VertexState> state; // info of the vertices recorded by this history step
    HistoryStep step; // history step to be added to history
    
    for (int i = 0; i < modelCoords.size(); i++) // go through each of the models 
    {
        int index;
        
        if (BinarySearchVec2(modelCoords[i], step.modelCoords, index) == -1) // sort .selection and add it to step.modelCoords
        {
            step.modelCoords.insert(step.modelCoords.begin() + index, modelCoords[i]);
        }
        
        for (int j = 0; j < (models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // save each vertex's data
        {
           VertexState temp;
           temp.index = j;
           temp.coords = modelCoords[i];
           temp.y = models[modelCoords[i].x][modelCoords[i].y].meshes[0].vertices[j+1];
           
           state.push_back(temp);
        }
    }
    
    step.startingVertices = state;
    
    history.push_back(step);
    stepIndex = history.size(); // history step is iterated here rather than in FinalizeHistoryStep() because the history size may have just been changed
}


Color* GenHeightmapSelection(const std::vector<std::vector<Model>>& models, const std::vector<Vector2>& modelCoords, int modelVertexWidth, int modelVertexHeight)
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


Color* GenHeightmap(const Model& model, int modelVertexWidth, int modelVertexHeight, float& highestY, float& lowestY, HeightMapMode mode, float slopeTolerance)
{
    // this version of GenHeightMap is used only for texturing the models in the editor, not exporting. it matches pixels 1:1 with polys rather than vertices
    
    Color* pixels = (Color*)RL_MALLOC((modelVertexWidth - 1)*(modelVertexHeight - 1)*sizeof(Color)); 
    
    for (int i = 0; i < (model.meshes[0].vertexCount * 3) - 2; i += 3) // find if there is a new highestY and/or lowestY
    {
        if (model.meshes[0].vertices[i+1] > highestY)
            highestY = model.meshes[0].vertices[i+1];
            
        if (model.meshes[0].vertices[i+1] < lowestY)
            lowestY = model.meshes[0].vertices[i+1];
    }
    
    float scale = highestY - lowestY;
    
    switch (mode)
    {
        case HeightMapMode::GRAYSCALE:
        {
            for (int i = 0; i < (modelVertexWidth - 1) * (modelVertexHeight - 1); i++)
            {
                unsigned char pixelValue = (abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 255;
                
                pixels[i].r = pixelValue;
                pixels[i].g = pixelValue; 
                pixels[i].b = pixelValue; 
                pixels[i].a = 255;           
            }
            
            break;
        }
        case HeightMapMode::SLOPE:
        {
            // generates a texture that colors the pixels based on their vertex normals as well as height. below slopeTolerance being tinted one color, above being another
            for (int i = 0; i < (modelVertexWidth - 1) * (modelVertexHeight - 1); i++)
            {
                float adj = sqrt(model.meshes[0].normals[i * 18] * model.meshes[0].normals[i * 18] + model.meshes[0].normals[i * 18 + 2] * model.meshes[0].normals[i * 18 + 2]); // distance from vertex to end of normal in x and z
                float opp = model.meshes[0].normals[i * 18 + 1]; // distance from vertex to end of normal in y
                float hyp = sqrt(adj * adj + opp * opp); // distance from vertex to end of normal
                float vertexAngle = asinf(opp / hyp)*RAD2DEG; // the angle of the normal
                
                if (180 - (vertexAngle + 90) >= slopeTolerance)
                {
                    // if the incline is greater than angle, the color appears brown
                    pixels[i].r = 110 + ((abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 145);
                    pixels[i].g = 66 + (abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 147; 
                    pixels[i].b = (abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 150; 
                    pixels[i].a = 255;  
                }
                else
                {
                    // if the incline is less than angle, the color appears green
                    pixels[i].r = (abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 150; 
                    pixels[i].g = 110 + ((abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 145);
                    pixels[i].b = (abs((highestY - model.meshes[0].vertices[i * 18 + 1]) - scale) / scale) * 150; 
                    pixels[i].a = 255;    
                }      
            }
            
            break;
        }
        case HeightMapMode::RAINBOW:
        {
            // if rainbow, assign each pixel one of 1170 colors (cutting out some pink - red range), starting at purple and going up to red
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
            
            break;
        }
    }
    
    return pixels;
}


Color* GenHeightmap(const std::vector<std::vector<Model>>& models, int modelVertexWidth, int modelVertexHeight, float maxHeight, float minHeight, bool grayscale)
{
    // this version of GenHeightMap is for exporting the heightmap only. it matches pixels 1:1 with vertices rather than polys
    //(assumes modelVertexWidth and modelVertexHeight are actually the same)
    int modelsSizeX = (int)models.size(); // number of model columns
    int modelsSizeY = (int)models[0].size(); // number of model rows
    int numOfModels = modelsSizeX * modelsSizeY; // find the number of models being used
    int numOfPixels = (modelVertexWidth * modelsSizeX - (modelsSizeX - 1)) * (modelVertexHeight * modelsSizeY - (modelsSizeY - 1)); // number of pixels in the image. - (modelsSize - 1) so that overlapping pixels are only counted once
    
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
                
                if (grayscale)
                {
                    unsigned char pixelValue = (abs((maxHeight - models[i][j].meshes[0].vertices[vertexIndex+1]) - scale) / scale) * 255;
                    
                    pixels[pixelIndex].r = pixelValue;
                    pixels[pixelIndex].g = pixelValue;
                    pixels[pixelIndex].b = pixelValue;
                    pixels[pixelIndex].a = 255;
                }
                else
                {
                    // use all channels of the png to save height data at much greater resolution
                    unsigned int pixelValue = (abs((maxHeight - models[i][j].meshes[0].vertices[vertexIndex+1]) - scale) / scale) * 2147483647;
                    
                    std::bitset<32> pixelBits(pixelValue); // pixelValue in binary
                    std::bitset<8> redBits; // red channel in binary
                    std::bitset<8> greenBits; // green channel in binary
                    std::bitset<8> blueBits; // blue channel in binary
                    std::bitset<8> alphaBits; // alpha channel in binary
                    
                    int pixelBitsIndex = 0; // which bit in pixelBits needs to be copied next
                    
                    for (int i = 0; i < 8; i++) // copy the first 8 bits (right to left) of pixelBits into the red channel
                    {
                        redBits[i] = pixelBits[pixelBitsIndex];
                        pixelBitsIndex++;  
                    }
                    
                    for (int i = 0; i < 8; i++) // copy the second set of 8 bits into the green channel, and so on
                    {
                        greenBits[i] = pixelBits[pixelBitsIndex];
                        pixelBitsIndex++;  
                    }
                    
                    for (int i = 0; i < 8; i++)
                    {
                        blueBits[i] = pixelBits[pixelBitsIndex];
                        pixelBitsIndex++;  
                    }
                    
                    for (int i = 0; i < 8; i++)
                    {
                        alphaBits[i] = pixelBits[pixelBitsIndex];
                        pixelBitsIndex++;  
                    }
                    
                    // convert from binary to unsigned char and assign the channels their values
                    pixels[pixelIndex].r = (unsigned char)redBits.to_ulong();
                    pixels[pixelIndex].g = (unsigned char)greenBits.to_ulong();
                    pixels[pixelIndex].b = (unsigned char)blueBits.to_ulong();
                    pixels[pixelIndex].a = (unsigned char)alphaBits.to_ulong();
                }
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


void UpdateHeightmap(const Model& model, int modelVertexWidth, int modelVertexHeight, float& highestY, float& lowestY, HeightMapMode heightMapMode)
{
    Color* pixels = GenHeightmap(model, modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
        
    UpdateTexture(model.materials[0].maps[MAP_DIFFUSE].texture, pixels);
    
    RL_FREE(pixels);    
}


void UpdateHeightmap(const std::vector<std::vector<Model>>& models, int modelVertexWidth, int modelVertexHeight, float& highestY, float& lowestY, HeightMapMode heightMapMode)
{
    for (int i = 0; i < models.size(); i++)
    {
        for (int j = 0; j < models[i].size(); j++)
        {
            Color* pixels = GenHeightmap(models[i][j], modelVertexWidth, modelVertexHeight, highestY, lowestY, heightMapMode);
                
            UpdateTexture(models[i][j].materials[0].maps[MAP_DIFFUSE].texture, pixels);
            
            RL_FREE(pixels);            
        }
    }
}


std::vector<int> GetVertexIndices(int x, int y, int width) 
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


Vector2 GetVertexCoords(int index, int width) 
{
    // meshes are patterns of pairs of tris. divide into and count by low resolution squares, then use piecemeal rules for final precision if there is a remainder
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


void ModelStitch(std::vector<std::vector<Model>>& models, const ModelSelection &modelSelection, int modelVertexWidth, int modelVertexHeight)
{
    int canvasWidth = models.size();
    int canvasHeight = models[0].size();
    
    if (modelSelection.topLeft.x > 0 && modelSelection.topLeft.y > 0) // stitch top left vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = 0; // vertex to be merged to 
        std::vector<int> vertexIndices = GetVertexIndices(modelVertexWidth - 1, modelVertexHeight - 1, modelVertexWidth); // vertices that need to be adjusted
        
        for (int i = 0; i < vertexIndices.size(); i++)
            models[modelSelection.topLeft.x - 1][modelSelection.topLeft.y - 1].meshes[0].vertices[vertexIndices[i] + 1] = models[modelSelection.topLeft.x][modelSelection.topLeft.y].meshes[0].vertices[vertexIndex1 + 1];
    }
    
    if (modelSelection.bottomRight.x < canvasWidth - 1 && modelSelection.topLeft.y > 0) // stitch top right vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = GetVertexIndices(modelVertexWidth - 1, 0, modelVertexWidth)[0]; // vertex to be merged to
        std::vector<int> vertexIndices = GetVertexIndices(0, modelVertexHeight - 1, modelVertexWidth); // vertices that need to be adjusted
        
        for (int i = 0; i < vertexIndices.size(); i++)
            models[modelSelection.bottomRight.x + 1][modelSelection.topLeft.y - 1].meshes[0].vertices[vertexIndices[i] + 1] = models[modelSelection.bottomRight.x][modelSelection.topLeft.y].meshes[0].vertices[vertexIndex1 + 1];            
    }
    
    if (modelSelection.bottomRight.y < canvasHeight - 1 && modelSelection.bottomRight.x < canvasWidth - 1) // stitch bottom right vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = GetVertexIndices(modelVertexWidth - 1, modelVertexHeight - 1, modelVertexWidth)[0]; // vertex to be merged to
        int vertexIndex2 = 0; // vertex that needs to be adjusted
        
        models[modelSelection.bottomRight.x + 1][modelSelection.bottomRight.y + 1].meshes[0].vertices[vertexIndex2 + 1] = models[modelSelection.bottomRight.x][modelSelection.bottomRight.y].meshes[0].vertices[vertexIndex1 + 1];
    }
    
    if (modelSelection.topLeft.x > 0 && modelSelection.bottomRight.y < canvasHeight - 1) // stitch bottom left vertex if there is a diagonally adjacent model
    {
        int vertexIndex1 = GetVertexIndices(0, modelVertexHeight - 1, modelVertexWidth)[0]; // vertex that needs to be merged to
        std::vector<int> vertexIndices = GetVertexIndices(modelVertexWidth, 0, modelVertexWidth); // vertices that need to be adjusted
        
        for (int i = 0; i < vertexIndices.size(); i++)
            models[modelSelection.topLeft.x - 1][modelSelection.bottomRight.y + 1].meshes[0].vertices[vertexIndices[i] + 1] = models[modelSelection.topLeft.x][modelSelection.bottomRight.y].meshes[0].vertices[vertexIndex1 + 1];            
    } 
    
    if (modelSelection.topLeft.y > 0)
    {
        for (int i = 0; i < modelSelection.width; i++) // stitch all overlapping vertices on the top edge
        {
            for (int j = 0; j < modelVertexWidth; j++)
            {     
                int vertexIndex1 = GetVertexIndices(j, 0, modelVertexWidth)[0]; // vertex that needs to be merged to
                std::vector<int> vertexIndices = GetVertexIndices(j, modelVertexHeight - 1, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[modelSelection.topLeft.x + i][modelSelection.topLeft.y - 1].meshes[0].vertices[vertexIndices[ii] + 1] = models[modelSelection.topLeft.x + i][modelSelection.topLeft.y].meshes[0].vertices[vertexIndex1 + 1];        
            }
        }
    }
    
    if (modelSelection.bottomRight.x < canvasWidth - 1) 
    {
        for (int i = 0; i < modelSelection.height; i++) // stitch all overlapping vertices on the right edge
        {
            for (int j = 0; j < modelVertexHeight; j++)
            {     
                int vertexIndex1 = GetVertexIndices(modelVertexWidth - 1, j, modelVertexWidth)[0]; // vertex that needs to be merged to
                std::vector<int> vertexIndices = GetVertexIndices(0, j, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[modelSelection.bottomRight.x + 1][modelSelection.topLeft.y + i].meshes[0].vertices[vertexIndices[ii] + 1] = models[modelSelection.bottomRight.x][modelSelection.topLeft.y + i].meshes[0].vertices[vertexIndex1 + 1];   
            }
        }        
    }
    
    if (modelSelection.bottomRight.y < canvasHeight - 1)
    {
        for (int i = 0; i < modelSelection.width; i++) // stitch all overlapping vertices on the bottom edge
        {
            for (int j = 0; j < modelVertexWidth; j++)
            {     
                int vertexIndex1 = GetVertexIndices(j, modelVertexHeight - 1, modelVertexWidth)[0]; // vertex that needs to be merged to
                std::vector<int> vertexIndices = GetVertexIndices(j, 0, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[modelSelection.topLeft.x + i][modelSelection.bottomRight.y + 1].meshes[0].vertices[vertexIndices[ii] + 1] = models[modelSelection.topLeft.x + i][modelSelection.bottomRight.y].meshes[0].vertices[vertexIndex1 + 1]; 
            }
        }         
    }
    
    if (modelSelection.topLeft.x > 0)
    {
        for (int i = 0; i < modelSelection.height; i++) // stitch all overlapping vertices on the left edge
        {
            for (int j = 0; j < modelVertexHeight; j++)
            {     
                int vertexIndex1 = GetVertexIndices(0, j, modelVertexWidth)[0]; // vertex that needs to be merged to 
                std::vector<int> vertexIndices = GetVertexIndices(modelVertexWidth - 1, j, modelVertexWidth); // vertices that need to be adjusted
                
                for (int ii = 0; ii < vertexIndices.size(); ii++)
                    models[modelSelection.topLeft.x - 1][modelSelection.topLeft.y + i].meshes[0].vertices[vertexIndices[ii] + 1] = models[modelSelection.topLeft.x][modelSelection.topLeft.y + i].meshes[0].vertices[vertexIndex1 + 1];
            }
        }          
    }
}


void SetExSelection(ModelSelection& modelSelection, int canvasWidth, int canvasHeight)
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


void UpdateCharacterCamera(Camera* camera, const std::vector<std::vector<Model>>& models, ModelSelection& terrainCells)
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


void UpdateFreeCamera(Camera* camera)
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
    
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
    {
        // Camera orientation calculation
        cameraAngle.x += (mousePositionDelta.x*-CAMERA_MOUSE_MOVE_SENSITIVITY);
        cameraAngle.y += (mousePositionDelta.y*-CAMERA_MOUSE_MOVE_SENSITIVITY);
    }
    
    // Angle clamp
    if (cameraAngle.y > CAMERA_FIRST_PERSON_MIN_CLAMP*DEG2RAD) cameraAngle.y = CAMERA_FIRST_PERSON_MIN_CLAMP*DEG2RAD;
    else if (cameraAngle.y < CAMERA_FIRST_PERSON_MAX_CLAMP*DEG2RAD) cameraAngle.y = CAMERA_FIRST_PERSON_MAX_CLAMP*DEG2RAD;
    
    // free fly camera
    camera->position.x += (sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_BACK] -
                           sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_FRONT] -
                           cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_LEFT] +
                           cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_RIGHT]);
                           
    camera->position.y += (sinf(cameraAngle.y)*direction[MOVE_FRONT] -
                           sinf(cameraAngle.y)*direction[MOVE_BACK] +
                           1.0f*direction[MOVE_UP] - 1.0f*direction[MOVE_DOWN]);
                           
    camera->position.z += (cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_BACK] -
                           cosf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_FRONT] +
                           sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_LEFT] -
                           sinf(cameraAngle.x)*cosf(cameraAngle.y)*direction[MOVE_RIGHT]);
    
    camera->target.x = camera->position.x - sinf(cameraAngle.x)*cosf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
    camera->target.y = camera->position.y + sinf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
    camera->target.z = camera->position.z - cosf(cameraAngle.x)*cosf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
}


void UpdateOrthographicCamera(Camera* camera)
{
    bool direction[6] = { IsKeyDown(cameraMoveControl[MOVE_FRONT]),
                           IsKeyDown(cameraMoveControl[MOVE_BACK]),
                           IsKeyDown(cameraMoveControl[MOVE_RIGHT]),
                           IsKeyDown(cameraMoveControl[MOVE_LEFT]),
                           IsKeyDown(cameraMoveControl[MOVE_UP]),
                           IsKeyDown(cameraMoveControl[MOVE_DOWN]) };
    
    // free fly camera
    camera->position.x += direction[MOVE_RIGHT] - direction[MOVE_LEFT];
                           
    camera->position.y += direction[MOVE_UP] - direction[MOVE_DOWN];
                           
    camera->position.z += direction[MOVE_BACK] - direction[MOVE_FRONT];
    
    camera->target.x = camera->position.x;
    camera->target.y = camera->position.y - 0.1;
    camera->target.z = camera->position.z;
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


void PrintBoxInfo(Rectangle box, InputFocus currentFocus, InputFocus matchingFocus, const std::string& s, float info)
{
    if (currentFocus == matchingFocus || !s.empty())     
    {
        char c[s.size() + 1];
        strcpy(c, s.c_str());                        
        
        DrawTextRec(GetFontDefault(), c, Rectangle {box.x + 2, box.y + 2, box.width - 2, box.height - 2}, 11, 1.f, false, BLACK);
    }
    else     
    {
        std::string temp = std::to_string(info);
        char c[temp.size() + 1];
        strcpy(c, temp.c_str());
        
        DrawTextRec(GetFontDefault(), c, Rectangle {box.x + 2, box.y + 2, box.width - 2, box.height - 2}, 11, 1.f, false, BLACK);
    }
}


void PrintBoxInfo(Rectangle box, InputFocus currentFocus, InputFocus matchingFocus, const std::string& s, int info)
{
    if (currentFocus == matchingFocus || !s.empty())     
    {
        char c[s.size() + 1];
        strcpy(c, s.c_str());                        
        
        DrawTextRec(GetFontDefault(), c, Rectangle {box.x + 2, box.y + 2, box.width - 2, box.height - 2}, 11, 1.f, false, BLACK);
    }
    else     
    {
        std::string temp = std::to_string(info);
        char c[temp.size() + 1];
        strcpy(c, temp.c_str());
        
        DrawTextRec(GetFontDefault(), c, Rectangle {box.x + 2, box.y + 2, box.width - 2, box.height - 2}, 11, 1.f, false, BLACK);
    }
}


void ProcessInput(int key, std::string& s, float& input, InputFocus& inputFocus, int maxSize)
{
    if (((key >= 48 && key <= 57) || key == 46) && ((int)s.size() < maxSize))
    {
        if (key == 48 && s.empty())
        {}
        else
            s.push_back((char)key);
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !s.empty())
        s.pop_back();
    
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    {
        if (s.empty())
            inputFocus = InputFocus::NONE;
        else
        {
            input = stof(s);
            inputFocus = InputFocus::NONE; 
        }
    }
}


std::vector<VertexState> FindVertexSelection(const std::vector<std::vector<Model>>& models, const ModelSelection& modelSelection, RayHitInfo hitPosition, float selectRadius)
{
    std::vector<VertexState>vertexIndices; // vector to return
    
    if (hitPosition.hit)
    {
        Vector2 hitCoords = {hitPosition.position.x, hitPosition.position.z};
    
        for (int i = 0; i < modelSelection.selection.size(); i++) // check models for vertices to be selected
        {
            for (int j = 0; j < (models[modelSelection.selection[i].x][modelSelection.selection[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // check this models vertices
            {
                Vector2 vertexCoords = {models[modelSelection.selection[i].x][modelSelection.selection[i].y].meshes[0].vertices[j], models[modelSelection.selection[i].x][modelSelection.selection[i].y].meshes[0].vertices[j+2]};
                
                float result = xzDistance(hitCoords, vertexCoords);
                
                if (result <= selectRadius) // if this vertex is inside the radius
                {
                    VertexState vs;
                    vs.coords.x = modelSelection.selection[i].x;
                    vs.coords.y = modelSelection.selection[i].y;
                    vs.index = j; 
                    
                    vertexIndices.push_back(vs); // store the vertex's index in the mesh and its models coords
                }
            }          
        }
    }
    
    return vertexIndices;
}


void FindStampPoints(float stampRotationAngle, float stampStretchLength, Vector2& outVec1, Vector2& outVec2, Vector2 stampAnchor)
{
    float offsetX;
    float offsetY;
    // find the locations of stamp1 and stamp2 given the stretch length and rotation angle
    if (stampRotationAngle == 0)
    {
        offsetX = 0;
        offsetY = stampStretchLength/2;
    }
    else if (stampRotationAngle > 0 && stampRotationAngle < 90)
    {
        float a = 90 - stampRotationAngle;
        
        offsetX = ((stampStretchLength / 2) * sinf(stampRotationAngle*DEG2RAD));
        offsetY = ((stampStretchLength / 2) * sinf(a*DEG2RAD));
    }
    else if (stampRotationAngle == 90)
    {
        offsetX = stampStretchLength/2;
        offsetY = 0;
    }
    else if (stampRotationAngle > 90 && stampRotationAngle < 180)
    {
        float b = stampRotationAngle - 90;
        float a = 90 - b;
        
        offsetX = ((stampStretchLength / 2) * sinf(a*DEG2RAD));
        offsetY = -((stampStretchLength / 2) * sinf(b*DEG2RAD));
    }
    else if (stampRotationAngle == 180)
    {
        offsetX = 0;
        offsetY = -stampStretchLength/2;
    }
    else if (stampRotationAngle > 180 && stampRotationAngle < 270)
    {
        float b = stampRotationAngle - 180;
        float a = 90 - b;
        
        offsetX = -((stampStretchLength / 2) * sinf(b*DEG2RAD));
        offsetY = -((stampStretchLength / 2) * sinf(a*DEG2RAD));
    }
    else if (stampRotationAngle == 270)
    {
        offsetX = -stampStretchLength/2;
        offsetY = 0;
    }
    else if (stampRotationAngle > 270)
    {
        float b = stampRotationAngle - 270;
        float a = 90 - b;
        
        offsetX = -((stampStretchLength / 2) * sinf(a*DEG2RAD));
        offsetY = ((stampStretchLength / 2) * sinf(b*DEG2RAD));
    }
    
    outVec1 = Vector2{stampAnchor.x + offsetX, stampAnchor.y + offsetY};
    outVec2 = Vector2{stampAnchor.x - offsetX, stampAnchor.y - offsetY};
    
    return;
}


float PointSegmentDistance(Vector2 point, Vector2 segmentPoint1, Vector2 segmentPoint2)
{
    float a = point.x - segmentPoint1.x;
    float b = point.y - segmentPoint1.y;
    float c = segmentPoint2.x - segmentPoint1.x;
    float d = segmentPoint2.y - segmentPoint1.y;

    float dot = a * c + b * d;
    float len_sq = c * c + d * d;
    float p = -1;
    
    if (len_sq != 0) 
      p = dot / len_sq;

    float xx;
    float yy;

    if (p < 0)
    {
        xx = segmentPoint1.x;
        yy = segmentPoint1.y;
    }
    else if (p > 1) 
    {
        xx = segmentPoint2.x;
        yy = segmentPoint2.y;
    }
    else 
    {
        xx = segmentPoint1.x + p * c;
        yy = segmentPoint1.y + p * d;
    }

    float dx = point.x - xx;
    float dy = point.y - yy;
    
    return sqrt(dx * dx + dy * dy);    
}


Mesh CopyMesh(const Mesh& mesh)
{
    // (doesnt copy animation data)
    Mesh newMesh = { 0 };
    newMesh.vboId = (unsigned int *)RL_CALLOC(7, sizeof(unsigned int)); // (MAX_MESH_VBO = 7)
    
    newMesh.vertexCount = mesh.vertexCount;
    newMesh.triangleCount = mesh.triangleCount;
    
    newMesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    newMesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    newMesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    newMesh.colors = NULL;
    
    for (int i = 0; i < mesh.vertexCount*3; i++)
    {
        newMesh.vertices[i] = mesh.vertices[i];
    }
    
    for (int i = 0; i < mesh.vertexCount*3; i++)
    {
        newMesh.normals[i] = mesh.normals[i];
    }
    
    for (int i = 0; i < mesh.vertexCount*2; i++)
    {
        newMesh.texcoords[i] = mesh.texcoords[i];
    }
    
    rlLoadMesh(&newMesh, false);
    
    return newMesh;
}


void Smooth(std::vector<std::vector<Model>>& models, const std::vector<VertexState>& vertices, int modelVertexWidth, int modelVertexHeight, int canvasWidth, int canvasHeight)
{
    std::vector<VertexState> changes; //  calculate and then make changes all at once rather than one at a time
    
    for (int i = 0; i < vertices.size(); ++i)
    {
        Vector2 coords = GetVertexCoords(vertices[i].index, modelVertexWidth);
        
        std::vector<float> yValues;
        
        if (coords.x != 0) // dont attempt x = -1
        {
            int index = GetVertexIndices(coords.x - 1, coords.y, modelVertexWidth)[0];
            
            yValues.push_back(models[vertices[i].coords.x][vertices[i].coords.y].meshes[0].vertices[index + 1]);   
        }
        else if (coords.x == 0 && (vertices[i].coords.x > 0)) // if the vertex x coord is 0, check if there is another model to the left
        {
            int index = GetVertexIndices(modelVertexWidth - 2, coords.y, modelVertexWidth)[0]; // get the index of the vertex in the other model
            
            yValues.push_back(models[vertices[i].coords.x - 1][vertices[i].coords.y].meshes[0].vertices[index + 1]);
        }
        
        if (coords.x != modelVertexWidth - 1) // dont attempt out of bounds x
        {
            int index = GetVertexIndices(coords.x + 1, coords.y, modelVertexWidth)[0];
            
            yValues.push_back(models[vertices[i].coords.x][vertices[i].coords.y].meshes[0].vertices[index + 1]);
        }
        else if (coords.x == modelVertexWidth - 1 && (vertices[i].coords.x < canvasWidth - 1)) // if the vertex x coord is at max width, check if there is another model to the right
        {
            int index = GetVertexIndices(1, coords.y, modelVertexWidth)[0]; // get the index of the vertex in the other model
            
            yValues.push_back(models[vertices[i].coords.x + 1][vertices[i].coords.y].meshes[0].vertices[index + 1]);                                   
        }
        
        if (coords.y != 0) // dont attempt y = -1
        {
            int index = GetVertexIndices(coords.x, coords.y - 1, modelVertexWidth)[0];
            
            yValues.push_back(models[vertices[i].coords.x][vertices[i].coords.y].meshes[0].vertices[index + 1]);    
        }
        else if (coords.y == 0 && (vertices[i].coords.y > 0)) // if the vertex y coord is at the top, check if there is a model above
        {
            int index = GetVertexIndices(coords.x, modelVertexHeight - 2, modelVertexWidth)[0]; // get the index of the vertex in the other model
            
            yValues.push_back(models[vertices[i].coords.x][vertices[i].coords.y - 1].meshes[0].vertices[index + 1]);
        }
        
        if (coords.y != modelVertexHeight - 1) // dont attempt out of bounds y
        {
            int index = GetVertexIndices(coords.x, coords.y + 1, modelVertexWidth)[0];
            
            yValues.push_back(models[vertices[i].coords.x][vertices[i].coords.y].meshes[0].vertices[index + 1]);
        }
        else if (coords.y == modelVertexHeight - 1 && (vertices[i].coords.y < canvasHeight - 1)) // if the vertex y coord is at the bottom, check if there is a model below
        {
            int index = GetVertexIndices(coords.x, 1, modelVertexWidth)[0]; // get the index of the vertex in the other model
            
            yValues.push_back(models[vertices[i].coords.x][vertices[i].coords.y + 1].meshes[0].vertices[index + 1]);
        }
        
        float average = 0;
        
        for (int i = 0; i < yValues.size(); i++)
        {
            average += yValues[i];
        }
        
        average = average / (float)yValues.size();
        
        VertexState temp;
        temp.coords = vertices[i].coords;
        temp.index = vertices[i].index;
        temp.y = average;
        
        changes.push_back(temp);
    }

    for (int i = 0; i < changes.size(); i++) // enact all changes 
    {
        models[changes[i].coords.x][changes[i].coords.y].meshes[0].vertices[changes[i].index + 1] = changes[i].y;   
    }
    
    return;
}


unsigned long PixelToHeight(Color pixel)
{
    std::bitset<32> heightValueBits;
    int heightValueIndex = 0;
    
    std::bitset<8> redPixels(pixel.r);
    
    for (int i = 0; i < 8; i++)
    {
        heightValueBits[heightValueIndex] = redPixels[i];
        heightValueIndex++;
    }
    
    std::bitset<8> greenPixels(pixel.g);
    
    for (int i = 0; i < 8; i++)
    {
        heightValueBits[heightValueIndex] = greenPixels[i];
        heightValueIndex++;
    }
    
    std::bitset<8> bluePixels(pixel.b);
    
    for (int i = 0; i < 8; i++)
    {
        heightValueBits[heightValueIndex] = bluePixels[i];
        heightValueIndex++;
    }
    
    std::bitset<8> alphaPixels(pixel.a);
    
    for (int i = 0; i < 8; i++)
    {
        heightValueBits[heightValueIndex] = alphaPixels[i];
        heightValueIndex++;
    }
    
    return heightValueBits.to_ulong();
}


Mesh GenMeshHeightmap32bit(Image heightmap, Vector3 size)
{
    Mesh mesh = { 0 };
    mesh.vboId = (unsigned int *)RL_CALLOC(7, sizeof(unsigned int));

    int mapX = heightmap.width;
    int mapZ = heightmap.height;

    Color *pixels = GetImageData(heightmap);

    // NOTE: One vertex per pixel
    mesh.triangleCount = (mapX-1)*(mapZ-1)*2;    // One quad every four pixels

    mesh.vertexCount = mesh.triangleCount*3;

    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.colors = NULL;

    int vCounter = 0;       // Used to count vertices float by float
    int tcCounter = 0;      // Used to count texcoords float by float
    int nCounter = 0;       // Used to count normals float by float

    int trisCounter = 0;

    Vector3 scaleFactor = { size.x/mapX, size.y/2147483647.f, size.z/mapZ };

    Vector3 vA;
    Vector3 vB;
    Vector3 vC;
    Vector3 vN;

    for (int z = 0; z < mapZ-1; z++)
    {
        for (int x = 0; x < mapX-1; x++)
        {
            // Fill vertices array with data
            //----------------------------------------------------------

            // one triangle - 3 vertex
            
            unsigned long heightValue = PixelToHeight(pixels[x + z*mapX]);
            
            mesh.vertices[vCounter] = (float)x*scaleFactor.x;
            mesh.vertices[vCounter + 1] = (float)heightValue*scaleFactor.y;
            mesh.vertices[vCounter + 2] = (float)z*scaleFactor.z;
            
            unsigned long heightValue2 = PixelToHeight(pixels[x + (z + 1)*mapX]);

            mesh.vertices[vCounter + 3] = (float)x*scaleFactor.x;
            mesh.vertices[vCounter + 4] = (float)heightValue2*scaleFactor.y;
            mesh.vertices[vCounter + 5] = (float)(z + 1)*scaleFactor.z;
            
            unsigned long heightValue3 = PixelToHeight(pixels[(x + 1) + z*mapX]);

            mesh.vertices[vCounter + 6] = (float)(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 7] = (float)heightValue3*scaleFactor.y;
            mesh.vertices[vCounter + 8] = (float)z*scaleFactor.z;

            // another triangle - 3 vertex
            mesh.vertices[vCounter + 9] = mesh.vertices[vCounter + 6];
            mesh.vertices[vCounter + 10] = mesh.vertices[vCounter + 7];
            mesh.vertices[vCounter + 11] = mesh.vertices[vCounter + 8];

            mesh.vertices[vCounter + 12] = mesh.vertices[vCounter + 3];
            mesh.vertices[vCounter + 13] = mesh.vertices[vCounter + 4];
            mesh.vertices[vCounter + 14] = mesh.vertices[vCounter + 5];
            
            unsigned long heightValue4 = PixelToHeight(pixels[(x + 1) + (z + 1)*mapX]);

            mesh.vertices[vCounter + 15] = (float)(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 16] = (float)heightValue4*scaleFactor.y;
            mesh.vertices[vCounter + 17] = (float)(z + 1)*scaleFactor.z;
            vCounter += 18;     // 6 vertex, 18 floats

            // Fill texcoords array with data
            //--------------------------------------------------------------
            mesh.texcoords[tcCounter] = (float)x/(mapX - 1);
            mesh.texcoords[tcCounter + 1] = (float)z/(mapZ - 1);

            mesh.texcoords[tcCounter + 2] = (float)x/(mapX - 1);
            mesh.texcoords[tcCounter + 3] = (float)(z + 1)/(mapZ - 1);

            mesh.texcoords[tcCounter + 4] = (float)(x + 1)/(mapX - 1);
            mesh.texcoords[tcCounter + 5] = (float)z/(mapZ - 1);

            mesh.texcoords[tcCounter + 6] = mesh.texcoords[tcCounter + 4];
            mesh.texcoords[tcCounter + 7] = mesh.texcoords[tcCounter + 5];

            mesh.texcoords[tcCounter + 8] = mesh.texcoords[tcCounter + 2];
            mesh.texcoords[tcCounter + 9] = mesh.texcoords[tcCounter + 3];

            mesh.texcoords[tcCounter + 10] = (float)(x + 1)/(mapX - 1);
            mesh.texcoords[tcCounter + 11] = (float)(z + 1)/(mapZ - 1);
            tcCounter += 12;    // 6 texcoords, 12 floats

            // Fill normals array with data
            //--------------------------------------------------------------
            for (int i = 0; i < 18; i += 9)
            {
                vA.x = mesh.vertices[nCounter + i];
                vA.y = mesh.vertices[nCounter + i + 1];
                vA.z = mesh.vertices[nCounter + i + 2];

                vB.x = mesh.vertices[nCounter + i + 3];
                vB.y = mesh.vertices[nCounter + i + 4];
                vB.z = mesh.vertices[nCounter + i + 5];

                vC.x = mesh.vertices[nCounter + i + 6];
                vC.y = mesh.vertices[nCounter + i + 7];
                vC.z = mesh.vertices[nCounter + i + 8];

                vN = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(vB, vA), Vector3Subtract(vC, vA)));

                mesh.normals[nCounter + i] = vN.x;
                mesh.normals[nCounter + i + 1] = vN.y;
                mesh.normals[nCounter + i + 2] = vN.z;

                mesh.normals[nCounter + i + 3] = vN.x;
                mesh.normals[nCounter + i + 4] = vN.y;
                mesh.normals[nCounter + i + 5] = vN.z;

                mesh.normals[nCounter + i + 6] = vN.x;
                mesh.normals[nCounter + i + 7] = vN.y;
                mesh.normals[nCounter + i + 8] = vN.z;
            }

            nCounter += 18;     // 6 vertex, 18 floats
            trisCounter += 2;
        }
    }

    RL_FREE(pixels);

    // Upload vertex data to GPU (static mesh)
    rlLoadMesh(&mesh, false);

    return mesh;
}


void UpdateTopDownCamera(Camera* camera)
{
    bool direction[6] = { IsKeyDown(cameraMoveControl[MOVE_FRONT]),
                           IsKeyDown(cameraMoveControl[MOVE_BACK]),
                           IsKeyDown(cameraMoveControl[MOVE_RIGHT]),
                           IsKeyDown(cameraMoveControl[MOVE_LEFT]),
                           IsKeyDown(cameraMoveControl[MOVE_UP]),
                           IsKeyDown(cameraMoveControl[MOVE_DOWN]) };
    
    // free fly camera
    camera->position.x += direction[MOVE_RIGHT] - direction[MOVE_LEFT];
    camera->position.y += direction[MOVE_UP] - direction[MOVE_DOWN];
    camera->position.z += direction[MOVE_BACK] - direction[MOVE_FRONT];
    
    camera->target.x = camera->position.x - sinf(cameraAngle.x)*cosf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
    camera->target.y = camera->position.y + sinf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
    camera->target.z = camera->position.z - cosf(cameraAngle.x)*cosf(cameraAngle.y)*CAMERA_FIRST_PERSON_FOCUS_DISTANCE;
}


RayHitInfo FindHit2D(const Ray& ray, const std::vector<std::vector<Model>>& models, int modelVertexWidth, int modelVertexHeight)
{
    RayHitInfo hitPosition = GetCollisionRayGround(ray, 0);
    
    int index1 = GetVertexIndices(modelVertexWidth - 1, 0, modelVertexWidth)[0]; // index of the top right vertex
    int index2 = GetVertexIndices(0, modelVertexHeight - 1, modelVertexWidth)[0]; // index of the bottom left vertex
    
    float leftX = models[0][0].meshes[0].vertices[0];
    float rightX = models[models.size() - 1][0].meshes[0].vertices[index1];
    float topY = models[0][0].meshes[0].vertices[2];
    float bottomY = models[0][models[0].size() - 1].meshes[0].vertices[index2 + 2];
    
    if (hitPosition.position.x < leftX || hitPosition.position.x > rightX || hitPosition.position.z < topY || hitPosition.position.z > bottomY) // if the hit position is outside of the model boundary, mark as false
    {
        hitPosition.hit = false;
    }
    
    return hitPosition;
}

// modelCoords: coordinates of the model to check. length: how many models to check in this direction before changing directions. direction: 1 right, 2 up, 3 left 4 down. loop: iteration along the current direction. total: total number of models checked 
RayHitInfo FindHit3D(const Ray& ray, const std::vector<std::vector<Model>>& models, Vector2& modelCoords, int length, int direction, int loop, int total)
{
    // search for ray model collision starting from modelCoords and spiral out
    if (modelCoords.x < models.size() && modelCoords.x >= 0 && modelCoords.y < models[0].size() && modelCoords.y >= 0) // skip if attempting to check a model that doesnt exist
    {
        RayHitInfo hitPosition;
        
        hitPosition = GetCollisionRayModel2(ray, &models[modelCoords.x][modelCoords.y]); 
        
        if (hitPosition.hit)
        {
            return hitPosition;
        }
        else
        {
            total++;
            loop++;
            
            if (total == models.size() * models[0].size()) // if all models have been checked, return with .hit false
            {
                hitPosition.hit = false;
                return hitPosition;
            }
            
            switch (direction) // call the next FindHit3D, with different arguments depending on the direction and whether or not it's reached the end of the length of one line of search
            {
                case 1:
                {
                    if (loop >= length)
                    {
                        modelCoords.x += 1;
                        return FindHit3D(ray, models, modelCoords, length + 2, direction + 1, 0, total); // when loop count reaches length, change direction. when direction is one, also increase length
                    }
                    else
                    {
                        modelCoords.x += 1;
                        return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move right
                    }
                }
                case 2:
                {
                    if (loop >= length)
                    {
                        modelCoords.x -= 1;
                        return FindHit3D(ray, models, modelCoords, length, direction + 1, 0, total);
                    }
                    else
                    {
                        modelCoords.y -= 1;
                        return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move up
                    }
                }
                case 3:
                {
                    if (loop >= length)
                    {
                        modelCoords.y += 1;
                        return FindHit3D(ray, models, modelCoords, length, direction + 1, 0, total);
                    }
                    else
                    {
                        modelCoords.x -= 1;
                        return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move left
                    }
                }
                case 4:
                {
                    if (loop >= length)
                    {
                        modelCoords.x += 1;
                        return FindHit3D(ray, models, modelCoords, length, 1, 0, total);
                    }
                    else
                    {
                        modelCoords.y += 1;
                        return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move down
                    }
                }
            }
        }
    }
    else
    {
        //TraceLog(LOG_ERROR, "###################");
        loop++;
       
        switch (direction) // call the next FindHit3D, with different arguments depending on the direction and whether or not it's reached the end of the length of one line of search
        {
            case 1:
            {
                if (loop >= length)
                {
                    modelCoords.x += 1;
                    return FindHit3D(ray, models, modelCoords, length + 2, direction + 1, 0, total); // when loop count reaches length, change direction. when direction is one, also increase length
                }
                else
                {
                    modelCoords.x += 1;
                    return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move right
                }
            }
            case 2:
            {
                if (loop >= length)
                {
                    modelCoords.x -= 1;
                    return FindHit3D(ray, models, modelCoords, length, direction + 1, 0, total);
                }
                else
                {
                    modelCoords.y -= 1;
                    return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move up
                }
            }
            case 3:
            {
                if (loop >= length)
                {
                    modelCoords.y += 1;
                    return FindHit3D(ray, models, modelCoords, length, direction + 1, 0, total);
                }
                else
                {
                    modelCoords.x -= 1;
                    return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move left
                }
            }
            case 4:
            {
                if (loop >= length)
                {
                    modelCoords.x += 1;
                    return FindHit3D(ray, models, modelCoords, length, 1, 0, total);
                }
                else
                {
                    modelCoords.y += 1;
                    return FindHit3D(ray, models, modelCoords, length, direction, loop, total); // move down
                }
            }
        }
    }
}


ModelSelection FindModelSelection(int canvasWidth, int canvasHeight, int modelWidth, Vector2 modelCoords, float selectRadius)
{
    ModelSelection modelSelection;
    
    float realModelWidth = modelWidth - (1 / 120.f) * modelWidth; // modelWidth minus the width of one polygon. ASSUMES MODELVERTEXWIDTH = 120
    int modelCheckRadius = ceil((selectRadius * 1.05) / realModelWidth); // how many models out from the model at modelCoords to check for vertices based on selectRadius plus a 5% margin. ASSUMES MODEL WIDTH = HEIGHT
    
    modelSelection.topLeft.y = modelCoords.y - modelCheckRadius; // top most model coordinate to be checked
    if (modelSelection.topLeft.y < 0) modelSelection.topLeft.y = 0;
    
    modelSelection.bottomRight.x = modelCoords.x + modelCheckRadius; // right most model coordinate to be checked
    if (modelSelection.bottomRight.x >= canvasWidth) modelSelection.bottomRight.x = canvasWidth - 1;
    
    modelSelection.bottomRight.y = modelCoords.y + modelCheckRadius; // bottom most model coordinate to be checked
    if (modelSelection.bottomRight.y >= canvasHeight) modelSelection.bottomRight.y = canvasHeight - 1;
    
    modelSelection.topLeft.x = modelCoords.x - modelCheckRadius; // left most model coordinate to be checked
    if (modelSelection.topLeft.x < 0) modelSelection.topLeft.x = 0;
    
    modelSelection.width = modelSelection.bottomRight.x - modelSelection.topLeft.x + 1;
    modelSelection.height = modelSelection.bottomRight.y - modelSelection.topLeft.y + 1;
    
    for (int i = 0; i < modelSelection.width; i++)
    {
        for (int j = 0; j < modelSelection.height; j++)
        {
            modelSelection.selection.push_back(Vector2{i + modelSelection.topLeft.x, j + modelSelection.topLeft.y});
        }
    }
    
    return modelSelection;
}


void ExtendHistoryStep(HistoryStep& historyStep, const std::vector<std::vector<Model>>& models, const ModelSelection& modelCoords)
{
    for (int i = 0; i < modelCoords.selection.size(); i++) // go through each of the models 
    {
        int insert; // index to insert new model coordinates if they arent already in the historyStep
        
        if (BinarySearchVec2(modelCoords.selection[i], historyStep.modelCoords, insert) == -1) // if this model hasnt been recorded yet, add it
        {
            historyStep.modelCoords.insert(historyStep.modelCoords.begin() + insert, modelCoords.selection[i]);
        
            for (int j = 0; j < (models[modelCoords.selection[i].x][modelCoords.selection[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // save each vertex's data
            {
               VertexState temp;
               temp.index = j;
               temp.coords = modelCoords.selection[i];
               temp.y = models[modelCoords.selection[i].x][modelCoords.selection[i].y].meshes[0].vertices[j+1];
               
               historyStep.startingVertices.push_back(temp);
            }
        }
    }
}


void FinalizeHistoryStep(HistoryStep& historyStep, const std::vector<std::vector<Model>>& models)
{
    for (int i = 0; i < historyStep.modelCoords.size(); i++) // go through each of the models 
    {
        for (int j = 0; j < (models[historyStep.modelCoords[i].x][historyStep.modelCoords[i].y].meshes[0].vertexCount * 3) - 2; j += 3) // save each vertex's data
        {
           VertexState temp;
           temp.index = j;
           temp.coords = historyStep.modelCoords[i];
           temp.y = models[historyStep.modelCoords[i].x][historyStep.modelCoords[i].y].meshes[0].vertices[j+1];
           
           historyStep.endingVertices.push_back(temp);
        }
    }
}


void UpdateNormals(Model& model, int modelVertexWidth, int modelVertexHeight)
{
    int nCounter = 0;       // Used to count normals float by float

    Vector3 vA;
    Vector3 vB;
    Vector3 vC;
    Vector3 vN;
    
    for (int z = 0; z < modelVertexHeight-1; z++)
    {
        for (int x = 0; x < modelVertexWidth-1; x++)
        {
            for (int i = 0; i < 18; i += 9)
            {
                vA.x = model.meshes[0].vertices[nCounter + i];
                vA.y = model.meshes[0].vertices[nCounter + i + 1];
                vA.z = model.meshes[0].vertices[nCounter + i + 2];

                vB.x = model.meshes[0].vertices[nCounter + i + 3];
                vB.y = model.meshes[0].vertices[nCounter + i + 4];
                vB.z = model.meshes[0].vertices[nCounter + i + 5];

                vC.x = model.meshes[0].vertices[nCounter + i + 6];
                vC.y = model.meshes[0].vertices[nCounter + i + 7];
                vC.z = model.meshes[0].vertices[nCounter + i + 8];

                vN = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(vB, vA), Vector3Subtract(vC, vA)));

                model.meshes[0].normals[nCounter + i] = vN.x;
                model.meshes[0].normals[nCounter + i + 1] = vN.y;
                model.meshes[0].normals[nCounter + i + 2] = vN.z;

                model.meshes[0].normals[nCounter + i + 3] = vN.x;
                model.meshes[0].normals[nCounter + i + 4] = vN.y;
                model.meshes[0].normals[nCounter + i + 5] = vN.z;

                model.meshes[0].normals[nCounter + i + 6] = vN.x;
                model.meshes[0].normals[nCounter + i + 7] = vN.y;
                model.meshes[0].normals[nCounter + i + 8] = vN.z;
            }

            nCounter += 18;     // 6 vertex, 18 floats
            
        }
    }
}


int BinarySearchVec2(Vector2 vec2, const std::vector<Vector2>&v, int &i)
{
	if (!v.size())
    {
        i = 0;
		return -1;
    }

	int min = 0;
	int max = v.size() - 1;

	while (1)
	{
		int index = ((max - min) / 2) + min;

		if (min == max && (v[index].x != vec2.x || v[index].y != vec2.y))
		{
			if (vec2.y > v[index].y || (vec2.y == v[index].y && vec2.x > v[index].x))
			{
				i = index + 1;
			}
			else
				i = index;

			return -1;
		}

		if (v[index].x == vec2.x && v[index].y == vec2.y)
			return index;
		else if (v[index].y > vec2.y || (v[index].y == vec2.y && v[index].x > vec2.x))
		{
			max = index - 1;
			if (max < min)
				max = min;
		}
		else if (v[index].y < vec2.y || (v[index].y == vec2.y && v[index].x < vec2.x))
		{
			min = index + 1;
			if (min > max)
				min = max;
		}

		if (max < 0)
			max = 0;
	}
}


template<class T, class T2>
int BinarySearch(T var, const std::vector<T2>&v, int &i)
{
	if (!v.size())
    {
        i = 0;
		return -1;
    }

	int min = 0;
	int max = v.size() - 1;

	while (1)
	{
		int index = ((max - min) / 2) + min;

		if (min == max && v[index] != var)
		{
			if (var > v[index])
			{
				i = index + 1;
			}
			else
				i = index;

			return -1;
		}

		if (v[index] == var)
			return index;
		else if (v[index] > var)
		{
			max = index - 1;
			if (max < min)
				max = min;
		}
		else if (v[index] < var)
		{
			min = index + 1;
			if (min > max)
				min = max;
		}

		if (max < 0)
			max = 0;
	}
}


template<class T, class T2>
int BinarySearch(T var, const std::vector<T2>&v)
{
	if (!v.size())
		return -1;

	int min = 0;
	int max = v.size() - 1;

	while (1)
	{
		int index = ((max - min) / 2) + min;

		if (min == max && v[index] != var)
			return -1;

		if (v[index] == var)
			return index;
		else if (v[index] > var)
		{
			max = index - 1;
			if (max < min)
				max = min;
		}
		else if (v[index] < var)
		{
			min = index + 1;
			if (min > max)
				min = max;
		}

		if (max < 0)
			max = 0;
	}
}


bool operator== (const VertexState &vs1, const VertexState &vs2)
{
    if (vs1.coords.x == vs2.coords.x && vs1.coords.y == vs2.coords.y && vs1.index == vs2.index)
        return true;
    else
        return false;
}


bool operator!= (const VertexState &vs1, const VertexState &vs2)
{
    return !(vs1 == vs2);
}


bool operator== (const ModelSelection& ms1, const ModelSelection& ms2)
{
    if (ms1.topLeft.x == ms2.topLeft.x && ms1.topLeft.y == ms2.topLeft.y && ms1.bottomRight.x == ms2.bottomRight.x && ms1.bottomRight.y == ms2.bottomRight.y)
        return true;
    else
        return false;
}


bool operator!= (const ModelSelection& ms1, const ModelSelection& ms2)
{
    return !(ms1 == ms2);
}




















