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

Filename  : inputVibration.c
Content   : This samples allows 4 inputs on the Quest3 Controllers to vibrate when pressed.
Created   :
Authors   :

*************************************************************************************/

#include <raylib.h>

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
//0 - L trigger
//1 - R trigger
//2 - X button
//3 - A button
//4 - L squeeze
//5 - R squeeze
//6 - L trigger value?
//7 - R trigger value?
//8 - L thumbstick
//9 - R thumbstick
//10 - L thumbstick click
//11 - R thumbstick click
//12 - Y button
//13 - B button
//14 - L haptic output??
//15 - R haptic output??
//16 - L aim pose
//17 - R aim pose
//18 - L grip pose
//19 - R grip pose

void android_main(struct android_app* app) {
    InitApp(app);
    while(!AppShouldClose(app)){
        BeginVRMode();
        SyncControllers();
        inLoop(app);
        if (IsVRButtonPressed(0)) {
            setVRControllerVibration(0, 3000, 0.5, -1);
        }
        if (IsVRButtonPressed(1)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
        }
        if (IsVRButtonPressed(2)) {
            setVRControllerVibration(0, 3000, 0.5, -1);
        }
        if (IsVRButtonPressed(3)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
        }
        EndVRMode();
    }
    CloseApp(app);
}