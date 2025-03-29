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

Filename  : cuboidCreation.c
Content   : This sample constructs 20 cuboids.
Created   :
Authors   :

*************************************************************************************/

#include <raylib.h>
#include <android/log.h>

float speed = 0.1f;
Vector3 selfLoc = (Vector3) {0.0f, 0.0f, 0.0f};

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app) {

    InitApp(app);
    while(!AppShouldClose(app)){
        BeginVRMode();
        SyncControllers();
        inLoop(app);

        for(int i = 0; i < 20; i++){
            DrawVRCuboid((Vector3){i * 0.2f, 0.0f, -1.0f}, (Vector3){0.1f, 0.1f, 0.1f}, (Vector3){1.0f ,.05f*i, .02f * i});
        }
        EndVRMode();
    }
    CloseApp(app);
}