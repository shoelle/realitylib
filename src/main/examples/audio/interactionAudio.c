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

Filename  : interactionAudio.c
Content   : This sample showcases how to run interaction based audio events. These audio events can be overlapping with itself or each other
Created   : 3/29/2025
Authors   :

*************************************************************************************/

#include <raylib.h>
#include <android/asset_manager.h>
#include <android/log.h>

Sound interactionSound1;
Sound interactionSound2;

float speed = 0.1f;
Vector3 selfLoc = (Vector3) {0.0f, 0.0f, 0.0f};

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app) {
    InitAudioDevice();
    AAssetManager* assetManager = app->activity->assetManager;

    asset = AAssetManager_open(assetManager, "testingInteractionSound1.wav", AASSET_MODE_BUFFER);
    if (asset != NULL) {
        const void* buffer = AAsset_getBuffer(asset);
        int dataSize = AAsset_getLength(asset);
        Wave wave = LoadWaveFromMemory(".wav", (const unsigned char*)buffer, dataSize);
        interactionSound1 = LoadSoundFromWave(wave);
        if (IsSoundReady(interactionSound1)) {
            __android_log_print(ANDROID_LOG_INFO, "VRApp", "Successfully loaded testingInteractionSound1.wav");
        }
        UnloadWave(wave);
        AAsset_close(asset);
    }
    asset = AAssetManager_open(assetManager, "testingInteractionSound2.wav", AASSET_MODE_BUFFER);
    if (asset != NULL) {
        const void* buffer = AAsset_getBuffer(asset);
        int dataSize = AAsset_getLength(asset);
        Wave wave = LoadWaveFromMemory(".wav", (const unsigned char*)buffer, dataSize);
        interactionSound2 = LoadSoundFromWave(wave);
        if (IsSoundReady(interactionSound2)) {
            __android_log_print(ANDROID_LOG_INFO, "VRApp", "Successfully loaded testingInteractionSound2.wav");
        }
        UnloadWave(wave);
        AAsset_close(asset);
    }

    InitApp(app);
    static bool wasLeftTriggerPressed = false;
    while(!AppShouldClose(app)){
        BeginVRMode();
        SyncControllers();
        inLoop(app);

        //Left trigger (index 0)
        if (IsVRButtonPressed(0)) {
            if (!wasLeftTriggerPressed && IsSoundReady(interactionSound1)) {
                PlaySound(interactionSound1);
            }
            wasLeftTriggerPressed = true;
        } else {
            wasLeftTriggerPressed = false;
        }
        if (IsVRButtonPressed(1)) {
            setVRControllerVibration(1, 3000, 0.5, -1);
            if (IsSoundReady(interactionSound2)) {
                PlaySound(interactionSound2);
            }
        }
        //Right trigger (index 1)
        if (IsVRButtonPressed(1) && IsSoundReady(interactionSound2)) {
            PlaySound(interactionSound2);
        }

        EndVRMode();
    }
    UnloadSound(backgroundSound);
    UnloadSound(interactionSound1);
    UnloadSound(interactionSound2);
    CloseAudioDevice();
    CloseApp(app);
}