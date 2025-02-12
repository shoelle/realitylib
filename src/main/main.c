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

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
float speed = 0.1f;
Vector3 selfLoc = (Vector3) {0.0f, 0.0f, 0.0f};
void android_main(struct android_app* app) {
    InitApp(app);
    while(!AppShouldClose(app)){
        BeginVRMode();
        SyncControllers();
        inLoop(app);
        if (IsVRButtonPressed(1)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
        }
        if (IsVRButtonPressed(2)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
        }
        if (IsVRButtonPressed(3)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
        }

        DrawVRBackground(selfLoc.x, selfLoc.z); // this draws the 2d wallpaper stretched across a curved rectangle encompassing roughly 120 degrees
        DrawVRCylinder((Vector3){0.0f - selfLoc.x, 1.0f - selfLoc.y, 0.0f - selfLoc.z}, (Vector3){0.0f, 0.0f, 0.0f}, 2.0f, 2.0f); // this draws the cylinder objects
        if (IsVRButtonDown(1)) {
            DrawVRQuad((Vector3){0.0f - selfLoc.x, 1.0f - selfLoc.y, 0.0f - selfLoc.z}, (Vector3){-2.0f * (1.0f - 0), 2.0f * (1.0f - 0), -2.0f}, 1.0f, 1.0f);
        }
        struct Vector2 rJoystickVec = GetThumbstickAxisMovement(1);
        selfLoc = (Vector3) {selfLoc.x + rJoystickVec.x * speed, selfLoc.y, selfLoc.z - rJoystickVec.y * speed};

        EndVRMode();
    }
    CloseApp(app);
}
