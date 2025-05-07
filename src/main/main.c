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

Filename  : main.c
Content   : Demo main
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
Sound backgroundSound;
bool wasColliding[2] = {false, false};  // Track previous collision state for each controller
AAssetManager* assetManager;

int fps = 60;
typedef struct Note {
    Vector3 position;
    Color color;
    int holdLength;
    bool hit;
} Note;

typedef struct Sword {
    Vector3 position;
    Vector4 orientation;
    Vector3 size;
    Color color;
} Sword;

typedef struct Lane {
    Note* notes;
    int nextNote;
    int numNotes;
    Note heldNote;
    bool hasHeldNote;
} Lane;

static int framesCounter = 0;
static int finishScreen = 0;

static Vector3 noteSize = { .1f, .1f, .1f };

static Sword swords[2];
static Lane* lanes;
static const int numNotes = 7500;
static const double noteSpeed = 0.15f;
static const int bpm = 178;
static const int numLanes = 5;

double noteGap;
static bool pause = true;

void android_main(struct android_app* app) {
    InitApp(app);
    assetManager = app->activity->assetManager;
    InitAudioDevice();
    InitGameplayState();
    while(!AppShouldClose(app)){
        if (IsSoundReady(backgroundSound) && !IsSoundPlaying(backgroundSound)) {
            PlaySound(backgroundSound);
        }
        BeginVRMode();
        UpdateGameplayState();
        DrawGameplayState();
        EndVRMode();
        if (IsVRButtonPressed(3)) {
            UnloadGameplayState();
            InitGameplayState();
        }
    }
    UnloadGameplayState();
    CloseAudioDevice();
    CloseApp(app);
}

Note note;
// Gameplay Screen Initialization logic
void InitGameplayState()
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

    float centerLane = numLanes / 2.0f;
    for (int i = 0; i < numNotes; i++) {
        int lane = rand() % numLanes;
        float locX = (lane-centerLane)*0.3;
        float height = 0.5f + (rand() % 100) / 250.0f;
        lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { locX,height,-(noteGap * (i+5))}, RED, 0, false};
        lanes[lane].numNotes++;
    }

    for (int i = 0; i < 2; i++) {
        swords[i].size = (Vector3){0.05f, 0.05f, 0.05f};
        swords[i].color = (Color){127,255,255,0};
        swords[i].orientation = (Vector4){0.0f,0.0f,0.0f,1.0f};
    }

    collisionSound = LoadVRSound(assetManager, "sound2.wav");
    backgroundSound = LoadVRSound(assetManager, "backgroundAudio.wav");
    if (IsSoundReady(backgroundSound)) {
        PlaySound(backgroundSound);
    }
}

void UpdateGameplayState() {
    SyncControllers();

//    if (IsVRButtonPressed(1)) {
//        setVRControllerVibration(1, 3000, 0.5, -1);
//    }
//    if (IsVRButtonPressed(2)) {
//        setVRControllerVibration(1, 3000, 0.5, -1);
//    }
//    if (IsVRButtonPressed(3)) {
//        setVRControllerVibration(1, 3000, 0.5, -1);
//    }

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

    for(int i = 0; i < 2; i++) {
        swords[i].position = GetControllerPosition(i);
        swords[i].position.z -= 0.0;
        swords[i].orientation = GetControllerOrientation(i);
    }
}

void DrawGameplayState() {
    Vector4 o0 = {0.0f,0.0f,0.0f,1.0f};
    for (int eye = 0; eye < 2; eye++) {
        BeginVRDraw(eye);

        for (int i = 0; i < 2; i++) {
            DrawVRCuboid(swords[i].position, swords[i].orientation, swords[i].size, swords[i].color);
        }

        for (int i = 0; i < numLanes; i++) {
            for (int j = lanes[i].nextNote; j < lanes[i].numNotes; j++) {
                if(!lanes[i].notes[j].hit) {
                    DrawVRCuboid(lanes[i].notes[j].position, o0, noteSize, lanes[i].notes[j].color);
                }
            }
        }

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
    UnloadSound(backgroundSound);
}

bool checkCollisions(int controller) {
    for (int i = 0; i < numLanes; i++) {
        for (int j = lanes[i].nextNote; j < lanes[i].numNotes; j++) {
            Vector3 center = lanes[i].notes[j].position;
            if (swords[controller].position.z - center.z <= noteSize.z + swords[controller].size.z + 0.05f &&
                center.z - swords[controller].position.z <= noteSize.z + swords[controller].size.z + 0.05f &&
                swords[controller].position.y - center.y <= noteSize.y + swords[controller].size.y + 0.001f &&
                center.y - swords[controller].position.y <= noteSize.y + swords[controller].size.y + 0.001f &&
                swords[controller].position.x - center.x <= noteSize.x + swords[controller].size.x + 0.001f &&
                center.x - swords[controller].position.x <= noteSize.x + swords[controller].size.x + 0.001f) {
                lanes[i].notes[j].hit = true;
                return true;
            }
        }
    }
    return false;
}