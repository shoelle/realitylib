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

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */

void InitGameplayState(void);
void UpdateGameplayState(void);
void DrawGameplayState(void);
void UnloadGameplayState(void);
int FinishGameplayState(void);

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

static Vector3 noteSize = { .9f, .9f, .9f };

static Lane* lanes;
static const int numNotes = 20;
static const double noteSpeed = .2f;
static const int chanceHold = 10;
static const int chanceHalf = 5;
static const int bpm = 138;
static const int numLanes = 4;

double noteGap;
static bool pause = true;

float speed = 0.1f;
Vector3 selfLoc = (Vector3) {0.0f, 0.0f, 0.0f};
void android_main(struct android_app* app) {
    InitApp(app);
    InitGameplayState();
    while(!AppShouldClose(app)){
        BeginVRMode();
        UpdateGameplayState();
        DrawGameplayState();
        EndVRMode();
    }
    CloseApp(app);
}

Note note;
// Gameplay Screen Initialization logic
void InitGameplayState(void)
{
//    // TODO: Initialize GAMEPLAY screen variables here!
    framesCounter = 0;
//    pause = true;
//
//    // basic calculation on note distance per beat
//    noteGap = noteSpeed * fps * 60 / bpm;
//
//    // init lanes
//    lanes = malloc(sizeof(Lane) * numLanes);
//
//    for (int i = 0; i < numLanes; i++) {
//        lanes[i].notes = malloc(sizeof(Note) * numNotes);
//        lanes[i].nextNote = 0;
//        lanes[i].numNotes = 0;
//        lanes[i].hasHeldNote = false;
//    }
//
//    for (int i = 0; i < numNotes; i++) {
//        int lane = rand() % numLanes;
//        if (rand() % chanceHold == 0) {
//            lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { -(noteGap * i + 5.0f),0.0f,lane}, BLUE, 1};
//        }
//        else {
//            lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { -(noteGap * i + 5.0f),0.0f,lane}, RED, 0};
//            lanes[lane].numNotes++;
//            if (rand() % chanceHalf == 0) {
//                lane = rand() % numLanes;
//                lanes[lane].notes[lanes[lane].numNotes] = (Note){ (Vector3) { -(noteGap * (i + .5f) + 5.0f),0.0f,lane}, RED, 0};
//            }
//        }
//        lanes[lane].numNotes++;
//        // laneD[i] = (Note) {0, (int) (noteGap * (i + 5)), RED};
//    }

    note = (Note){ (Vector3) { -5.0f,1.0f,-1.0f}, RED, 0};
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
    note.position.x += 0.01;
}

void DrawGameplayState() {
    DrawVRBackground(selfLoc.x, selfLoc.z); // this draws the 2d wallpaper stretched across a curved rectangle encompassing roughly 120 degrees
//    DrawVRCube((Vector3){1.0f, 2.0f, 3.0f}, 1.0f, (Color){19 ,37, 207, 255});
//    DrawVRCube(note.position,noteSize.x,note.color);
    for (int eye = 0; eye < 2; eye++) {
        BeginVRDraw(eye);
        DrawVRCuboid((Vector3){0.2f, 0.0f, -1.0f}, (Vector4){0.0f,0,0,1}, (Vector3){0.1f, 0.1f, 0.1f}, (Color){127,255,255,0});
        DrawVRCuboid((Vector3){1.2f, 0.0f, -1.0f}, (Vector4){0.0f,0,0,1}, (Vector3){0.1f, 0.1f, 0.1f}, (Color){127,255,255,0});
        DrawVRCuboid((Vector3){0.2f, 1.0f, -1.0f}, (Vector4){0.0f,0,0,1}, (Vector3){0.03f, 0.03f, 0.03f}, (Color){127,255,255,0});

        DrawVRCuboid(note.position, (Vector4){0.0f,0,0,1}, (Vector3){0.03f,0.03f,0.03f}, (Color){127,255,255,0});
//        Vector3 basePos = GetControllerPosition(2);
//        Vector4 baseOrientation = GetControllerOrientation(2);
//        for (int i = 0; i < 20; i++) {
//            Vector3 pos = {basePos.x, basePos.y+i, basePos.z};
//            DrawVRCuboid(pos, (Vector4){0.0f,0,0,1}, (Vector3){0.03f,0.03f,0.03f}, (Color){127,255,255,0} );
//        }
        EndVRDraw(eye);
    }
}
