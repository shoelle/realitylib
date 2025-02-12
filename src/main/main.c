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
Vector3 advance(Vector3 cameraPos, Vector2 joystickVec, float speed) {
    Vector3 out = {cameraPos.x + joystickVec.x * speed, cameraPos.y, cameraPos.z - joystickVec.y * speed};
    if(out.x >= 19) {
        out.x = 19;
    } else if(out.x <= -17) {
        out.x = -17;
    }
    if(out.z >= 19) {
        out.z = 19;
    } else if(out.z <= -99) {
        out.z = -99;
    }
    return out;
}

float speed = 0.1f;
float dir = 0.0f;
void android_main(struct android_app* app) {
    CameraXr camera;

    InitApp(app);
    while(!AppShouldClose(app)){
        BeginVRMode(camera);
        syncControllers();
        inLoop(app);
        if (IsVRButtonPressed(1)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
        }
//        DrawVRBackground(camera.position.x, camera.position.z); // this draws the 2d wallpaper stretched across a curved rectangle encompassing roughly 120 degrees
//        DrawVRCylinder((Vector3){0.0f - camera.position.x, 1.0f - camera.position.y, 0.0f - camera.position.z}, (Vector3){0.0f, 0.0f, 0.0f}, 2.0f, 2.0f); // this draws the cylinder objects
//        if (IsVRButtonDown(1)) {
//            DrawVRQuad((Vector3){0.0f - camera.position.x, 1.0f - camera.position.y, 0.0f - camera.position.z}, (Vector3){-2.0f * (1.0f - 0), 2.0f * (1.0f - 0), -2.0f}, 1.0f, 1.0f);
//        }
        struct Vector2 rJoystickVec = GetThumbstickAxisMovement(0,0);
//        if(rJoystickVec.x <= -0.5) {
//            dir -= 1;
//        } else if(rJoystickVec.x >= 0.5) {
//            dir += 1;
//        }
//        TurnCameraXr((XrVector3f){0.0f,1.0f,0.0f}, dir);
        camera.position = advance(camera.position, rJoystickVec, speed);

//        DrawVRQuad((Vector3){40.0f - camera.position.x, 0.0f - camera.position.y, -100.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 0.0f); // front
//        DrawVRQuad((Vector3){40.0f - camera.position.x, 0.0f - camera.position.y, -60.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 180.0f); // back
//        DrawVRQuad((Vector3){20.0f - camera.position.x, 0.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // left
//        DrawVRQuad((Vector3){60.0f - camera.position.x, 0.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // right
//        DrawVRQuad((Vector3){40.0f - camera.position.x, 20.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // top
//        DrawVRQuad((Vector3){40.0f - camera.position.x, -20.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // bottom

        DrawVRQuad((Vector3){0.0f - camera.position.x, 0.0f - camera.position.y, -100.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 0.0f); // front
        DrawVRQuad((Vector3){-20.0f - camera.position.x, 0.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // left
//        if(!IsVRButtonDown(3)) {
            DrawVRQuad((Vector3){20.0f - camera.position.x, 0.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // right
//        }
        DrawVRQuad((Vector3){0.0f - camera.position.x, 20.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // top
//        DrawVRQuad((Vector3){0.0f - camera.position.x, -20.0f - camera.position.y, -80.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // bottom

        DrawVRQuad((Vector3){-20.0f - camera.position.x, 0.0f - camera.position.y, -40.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // left
        DrawVRQuad((Vector3){20.0f - camera.position.x, 0.0f - camera.position.y, -40.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // right
//        DrawVRQuad((Vector3){0.0f - camera.position.x, -20.0f - camera.position.y, -40.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // bottom
        DrawVRQuad((Vector3){0.0f - camera.position.x, 20.0f - camera.position.y, -40.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // top

        DrawVRQuad((Vector3){0.0f - camera.position.x, 0.0f - camera.position.y, 20.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 180.0f); // back
        DrawVRQuad((Vector3){-20.0f - camera.position.x, 0.0f - camera.position.y, 0.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // left
        DrawVRQuad((Vector3){20.0f - camera.position.x, 0.0f - camera.position.y, 0.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // right
//        DrawVRQuad((Vector3){0.0f - camera.position.x, -20.0f - camera.position.y, 0.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 270.0f); // bottom
        DrawVRQuad((Vector3){0.0f - camera.position.x, 20.0f - camera.position.y, 0.0f - camera.position.z}, (Vector3){1.0f, 0.0f, 0.0f}, 40.0f, 40.0f, 90.0f); // top

        if (GetVRInputFloat(6) < 0.5) {
            DrawVRQuad((Vector3){0.0f - camera.position.x, 0.0f - camera.position.y, -20.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 0.0f); // front
            DrawVRQuad((Vector3){0.0f - camera.position.x, 0.0f - camera.position.y, -20.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 180.0f); // front
        }
        if (GetVRInputFloat(5) < 0.5 && ((camera.position.z <= -20.0f) || (GetVRInputFloat(6) >= 0.5))) {
            DrawVRQuad((Vector3){0.0f - camera.position.x, 0.0f - camera.position.y, -60.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 180.0f); // front
            DrawVRQuad((Vector3){0.0f - camera.position.x, 0.0f - camera.position.y, -60.0f - camera.position.z}, (Vector3){0.0f, 1.0f, 0.0f}, 40.0f, 40.0f, 0.0f); // front
        }
        EndVRMode();
    }
    CloseApp(app);
}
