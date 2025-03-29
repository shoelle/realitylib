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
Content   : This is a sample main.c file which plays a background audio looped upon starting
Created   : 3/29/2025
Authors   :

*************************************************************************************/

#include <raylib.h>
#include <android/asset_manager.h>
#include <android/log.h>

Sound backgroundSound;

float speed = 0.1f;

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app) {
    InitAudioDevice();
    AAssetManager* assetManager = app->activity->assetManager;

    AAsset* asset = AAssetManager_open(assetManager, "testingBackgroundMusic.wav", AASSET_MODE_BUFFER);
    if (asset != NULL) {
        const void* buffer = AAsset_getBuffer(asset);
        int dataSize = AAsset_getLength(asset);
        Wave wave = LoadWaveFromMemory(".wav", (const unsigned char*)buffer, dataSize);
        backgroundSound = LoadSoundFromWave(wave);
        if (IsSoundReady(backgroundSound)) {
            __android_log_print(ANDROID_LOG_INFO, "VRApp", "Successfully loaded testingBackgroundMusic.wav");
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "VRApp", "Failed to load testingBackgroundMusic.wav");
        }
        UnloadWave(wave);
        AAsset_close(asset);
    }

    InitApp(app);
    static bool wasLeftTriggerPressed = false;
    while(!AppShouldClose(app)){
        if (IsSoundReady(backgroundSound) && !IsSoundPlaying(backgroundSound)) {
            PlaySound(backgroundSound);
        }
        BeginVRMode();
        SyncControllers();
        inLoop(app);
        EndVRMode();
    }
    UnloadSound(backgroundSound);
    CloseAudioDevice();
    CloseApp(app);
}