/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/************************************************************************************

Filename  : XrCompositor_NativeActivity.c
Content   : This sample uses the Android NativeActivity class.
Created   :
Authors   :

*************************************************************************************/

#include <raylib.h>
#include <stdlib.h>
#include <android/asset_manager.h>
#include <android/log.h>


/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */

void InitGameplayState(void);
void UpdateGameplayState(void);
void DrawGameplayState(void);
void UnloadGameplayState(void);
bool checkCollisions(int controller);

Sound collisionSound;
bool wasColliding[2] = {false, false};  // Track previous collision state for each controller
AAssetManager* assetManager;

int fps = 60;
typedef struct Note {
    Vector3 position;
    Color color;
    int holdLength;
} Note;

typedef struct Lane {
    Note* notes;
    int nextNote;
    int numNotes;
    Note heldNote;
    bool hasHeldNote;
} Lane;

static int framesCounter = 0;
static int finishScreen = 0;

static const int noteWidth = 50;
static const int noteHeight = 20;

static Vector3 noteSize = { .1f, .1f, .1f };

static Lane* lanes;
static const int numNotes = 50;
static const double noteSpeed = .1f;
static const int chanceHold = 0; // 10;
static const int chanceHalf = 0; // 5;
static const int bpm = 138;
static const int numLanes = 4;

double noteGap;
static bool pause = true;

float speed = 0.1f;
Vector3 selfLoc = (Vector3) {0.0f, 0.0f, 0.0f};
void android_main(struct android_app* app) {
    InitApp(app);
    InitAudioDevice();
    assetManager = app->activity->assetManager;

    AAsset* asset = AAssetManager_open(assetManager, "sound.wav", AASSET_MODE_BUFFER);
    if (asset != NULL) {
        const void* buffer = AAsset_getBuffer(asset);
        int dataSize = AAsset_getLength(asset);
        Wave wave = LoadWaveFromMemory(".wav", (const unsigned char*)buffer, dataSize);
        collisionSound = LoadSoundFromWave(wave);
        if (IsSoundReady(collisionSound)) {
            __android_log_print(ANDROID_LOG_INFO, "VRApp", "Successfully loaded sound.wav");
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "VRApp", "Failed to load sound.wav");
        }
        UnloadWave(wave);
        AAsset_close(asset);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "VRApp", "Failed to open sound.wav");
    }
    InitGameplayState();
    while(!AppShouldClose(app)){
        BeginVRMode();
        UpdateGameplayState();
        DrawGameplayState();
        EndVRMode();
    }
    UnloadSound(collisionSound);
    CloseAudioDevice();
    CloseApp(app);
}

Note note;
// Gameplay Screen Initialization logic
void InitGameplayState(void)
{
//    // TODO: Initialize GAMEPLAY screen variables here!
    framesCounter = 0;
    pause = false;

    // basic calculation on note distance per beat
    noteGap = noteSpeed * fps * 60 / bpm;

    // init lanes
    lanes = malloc(sizeof(Lane) * numLanes);

    for (int i = 0; i < numLanes; i++) {
        lanes[i].notes = malloc(sizeof(Note) * numNotes);
        lanes[i].nextNote = 0;
        lanes[i].numNotes = 0;
        lanes[i].hasHeldNote = false;
    }

    for (int i = 0; i < numNotes; i++) {
        int lane = rand() % numLanes;
        float height = 0.75f + (rand() % 1) / 2.0f;
        if (rand() % chanceHold == 0) {
            lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { lane*0.3-0.86,height,-(noteGap * i + 5.0f)}, BLUE, 1};
        }
        else {
            lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { lane*0.3-0.86,height,-(noteGap * i + 5.0f)}, RED, 0};
            lanes[lane].numNotes++;
            if (rand() % chanceHalf == 0) {
                lane = rand() % numLanes;
                lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { lane*0.3-0.86,height,-(noteGap * (i + .5f) + 5.0f)}, RED, 0};
            }
        }
        lanes[lane].numNotes++;
        // laneD[i] = (Note) {0, (int) (noteGap * (i + 5)), RED};
    }
}

void UpdateGameplayState() {
    SyncControllers();

    if (IsVRButtonPressed(1)) {
        setVRControllerVibration(1, 3000, 0.5, -1);
    }
    if (IsVRButtonPressed(2)) {
        setVRControllerVibration(1, 3000, 0.5, -1);
    }
    if (IsVRButtonPressed(3)) {
        setVRControllerVibration(1, 3000, 0.5, -1);
    }

    for (int i = 0; i < numLanes; i++) {
        while (lanes[i].notes[lanes[i].nextNote].position.z > 10.0f) {
            lanes[i].nextNote++;
            printf("skipped a note at lane %d\n", i);
        }
    }

    if (!pause) {
        for (int i = 0; i < numLanes; i++) {
            for (int j = lanes[i].nextNote; j < lanes[i].numNotes; j++) {
                lanes[i].notes[j].position.z += noteSpeed;
            }
            if (lanes[i].hasHeldNote) {
                lanes[i].heldNote.position.z += noteSpeed;
            }
        }
    }
    for (int controller = 0; controller < 2; controller++) {
        bool isColliding = checkCollisions(controller);
        if (isColliding && !wasColliding[controller]) {
            if (IsSoundReady(collisionSound)) {
                PlaySound(collisionSound);
            }
            setVRControllerVibration(controller, 3000, 0.5, -1);
        }
        wasColliding[controller] = isColliding;
    }
}

void DrawGameplayState() {
//    DrawVRBackground(selfLoc.x, selfLoc.z); // this draws the 2d wallpaper stretched across a curved rectangle encompassing roughly 120 degrees
//    DrawVRCube((Vector3){1.0f, 2.0f, 3.0f}, 1.0f, (Color){19 ,37, 207, 255});
//    DrawVRCube(note.position,noteSize.x,note.color);
    Vector4 o0 = {0.0f,0.0f,0.0f,1.0f};
    for (int eye = 0; eye < 2; eye++) {
        BeginVRDraw(eye);

        Vector3 lPos = GetControllerPosition(0);
        lPos.z -= 0.2;
        Vector4 lOrientation = GetControllerOrientation(0);
        DrawVRCuboid(lPos, lOrientation, (Vector3){0.02f,0.02f,0.4f}, (Color){127,255,255,0});
        Vector3 rPos = GetControllerPosition(1);
        Vector4 rOrientation = GetControllerOrientation(1);
        rPos.z -= 0.2;
        DrawVRCuboid(rPos, rOrientation, (Vector3){0.02f,0.02f,0.4f}, (Color){127,255,255,0});
//        for (int i = 0; i < 20; i++) {
//            Vector3 pos = {basePos.x, basePos.y+i, basePos.z};
//            DrawVRCuboid(pos, (Vector4){0.0f,0,0,1}, (Vector3){0.03f,0.03f,0.03f}, (Color){127,255,255,0} );
//        }

        for (int i = 0; i < numLanes; i++) {
            for (int j = lanes[i].nextNote; j < lanes[i].numNotes; j++) {
                if (lanes[i].notes[j].holdLength && !lanes[i].hasHeldNote) {
                    int newZ = lanes[i].notes[j].position.z - noteGap * lanes[i].notes[j].holdLength / 2;
                    Vector3 newPos = { lanes[i].notes[j].position.y, lanes[i].notes[j].position.z, newZ};
                    DrawVRCuboid(newPos,o0, (Vector3){noteSize.x, noteSize.y,noteGap * lanes[i].notes[j].holdLength + noteSize.z}, lanes[i].notes[j].color);
                }
                else {
                    DrawVRCuboid(lanes[i].notes[j].position,o0, noteSize, lanes[i].notes[j].color);
                }
            }
            if (lanes[i].hasHeldNote) {
                int newZ = lanes[i].heldNote.position.z - noteGap * lanes[i].heldNote.holdLength / 2;
                Vector3 newPos = {lanes[i].heldNote.position.x, lanes[i].heldNote.position.y, newZ};
                DrawVRCuboid(newPos, o0, (Vector3){noteSize.x, noteSize.y, noteGap * lanes[i].heldNote.holdLength + noteSize.z}, lanes[i].heldNote.color);
            }
        }

        // DrawGrid(40, 1.0f);
//        for (int i = 0; i < 4; i++) DrawVRCuboid((Vector3){0.0f+i, 0.0f, 0.0f},o0,(Vector3){0.05f,0.05f,20.0f}, GRAY);

        EndVRDraw(eye);
    }
}

void UnloadGameplayState(void)
{
    // TODO: Unload GAMEPLAY screen variables here!
    for (int i = 0; i < numLanes; i++) {
        free(lanes[i].notes);
    }
    free(lanes);
    UnloadSound(collisionSound);
}

bool checkCollisions(int controller) {
    Vector3 controllerPos = GetControllerPosition(controller);
    for (int i = 0; i < numLanes; i++) {
        for (int j = lanes[i].nextNote; j < lanes[i].numNotes; j++) {
            Note note = lanes[i].notes[j];
            Vector3 center;
            Vector3 halfSize;
            if (note.holdLength > 0) {
                float offset = noteGap * note.holdLength / 2;
                center = (Vector3){note.position.x, note.position.y, note.position.z - offset};
                halfSize = (Vector3){noteSize.x / 2, noteSize.y / 2, (noteGap * note.holdLength + noteSize.z) / 2};
            } else {
                center = note.position;
                halfSize = (Vector3){noteSize.x / 2, noteSize.y / 2, noteSize.z / 2};
            }
            if (controllerPos.x >= center.x - halfSize.x &&
                controllerPos.x <= center.x + halfSize.x &&
                controllerPos.y >= center.y - halfSize.y &&
                controllerPos.y <= center.y + halfSize.y &&
                controllerPos.z >= center.z - halfSize.z &&
                controllerPos.z <= center.z + halfSize.z) {
                return true;
            }
        }
        if (lanes[i].hasHeldNote) {
            Note heldNote = lanes[i].heldNote;
            Vector3 center;
            Vector3 halfSize;
            if (heldNote.holdLength > 0) {
                float offset = noteGap * heldNote.holdLength / 2;
                center = (Vector3){heldNote.position.x, heldNote.position.y, heldNote.position.z - offset};
                halfSize = (Vector3){noteSize.x / 2, noteSize.y / 2, (noteGap * heldNote.holdLength + noteSize.z) / 2};
            } else {
                center = heldNote.position;
                halfSize = (Vector3){noteSize.x / 2, noteSize.y / 2, noteSize.z / 2};
            }
            if (controllerPos.x >= center.x - halfSize.x &&
                controllerPos.x <= center.x + halfSize.x &&
                controllerPos.y >= center.y - halfSize.y &&
                controllerPos.y <= center.y + halfSize.y &&
                controllerPos.z >= center.z - halfSize.z &&
                controllerPos.z <= center.z + halfSize.z) {
                return true;
            }
        }
    }
    return false;
}