//
// Created by lowej2 on 12/18/2024.
//

#ifndef RAYMOB_XR_SUPPORT_H
#define RAYMOB_XR_SUPPORT_H

#endif //RAYMOB_XR_SUPPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

/*
This example demonstrates the use of the supported compositor layer types: projection,
cylinder, quad, cubemap, and equirect.

Compositor layers provide advantages such as increased resolution and better sampling
qualities over rendering to the eye buffers. Layers are composited strictly in order,
with no depth interaction, so it is the applications responsibility to not place the
layers such that they would be occluded by any world geometry, which can result in
eye straining depth paradoxes.

Layer rendering may break down at extreme grazing angles, and as such, they should be
faded out or converted to normal surfaces which render to the eye buffers if the viewer
can get too close.

All texture levels must have a 0 alpha border to avoid edge smear.
*/

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#if !defined(GL_EXT_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
        GLenum target,
        GLsizei samples,
        GLenum internalformat,
        GLsizei width,
        GLsizei height);
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
        GLenum target,
        GLenum attachment,
        GLenum textarget,
        GLuint texture,
        GLint level,
        GLsizei samples);
#endif

// GL_EXT_texture_cube_map_array
#if !defined(GL_TEXTURE_CUBE_MAP_ARRAY)
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#endif

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <meta_openxr_preview/openxr_oculus_helpers.h>
#include "../../Samples/3rdParty/khronos/openxr/OpenXR-SDK/src/common/xr_linear.h"

#define MATH_PI 3.14159265358979323846f

#define DEBUG 1
#define OVR_LOG_TAG "XrCompositor_NativeActivity"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, OVR_LOG_TAG, __VA_ARGS__)

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_MULTI_SAMPLES = 4;

typedef union {
    XrCompositionLayerProjection Projection;
    XrCompositionLayerQuad Quad;
    XrCompositionLayerCylinderKHR Cylinder;
    XrCompositionLayerCubeKHR Cube;
    XrCompositionLayerEquirect2KHR Equirect2;
} ovrCompositorLayer_Union;

enum { ovrMaxLayerCount = 16 };
enum { ovrMaxNumEyes = 2 };

// Forward declarations
XrInstance ovrApp_GetInstance();

/*
================================================================================

OpenXR Utility Functions

================================================================================
*/

#if defined(DEBUG)
static void
OXR_CheckErrors(XrInstance instance, XrResult result, const char* function, bool failOnError) {
    if (XR_FAILED(result)) {
        char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, errorBuffer);
        if (failOnError) {
            ALOGE("OpenXR error: %s: %s\n", function, errorBuffer);
        } else {
            ALOGV("OpenXR error: %s: %s\n", function, errorBuffer);
        }
    }
}
#endif

#if defined(DEBUG)
#define OXR(func) OXR_CheckErrors(ovrApp_GetInstance(), func, #func, true);
#else
#define OXR(func) OXR_CheckErrors(ovrApp_GetInstance(), func, #func, false);
#endif

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

static const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

static const char* GlFrameBufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default:
            return "unknown";
    }
}

#ifdef CHECK_GL_ERRORS

static const char* GlErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "unknown";
    }
}

static void GLCheckErrors(int line) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        ALOGE("GL error on line %d: %s", line, GlErrorString(error));
    }
}

#define GL(func) \
    func;        \
    GLCheckErrors(__LINE__);

#else // CHECK_GL_ERRORS

#define GL(func) func;

#endif // CHECK_GL_ERRORS

/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct {
    EGLint MajorVersion;
    EGLint MinorVersion;
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
} ovrEgl;

static void ovrEgl_Clear(ovrEgl* egl) {
    egl->MajorVersion = 0;
    egl->MinorVersion = 0;
    egl->Display = 0;
    egl->Config = 0;
    egl->TinySurface = EGL_NO_SURFACE;
    egl->MainSurface = EGL_NO_SURFACE;
    egl->Context = EGL_NO_CONTEXT;
}

/**
 * Checks if a given flag @flag is present in the list of flags @flags, and
 * logs an error if it isn't.
 * @humanName is the human-readable name of the list of flags being checked,
 * used for logging.
 */
static bool checkFlagAndLog(EGLint flags, EGLint flag, const char* humanName) {
    const bool present = (flags & flag) == flag;
    if (!present)
    {
        ALOGD("        Skipping EGL config because %s %d doesn't have %d flag", humanName, flags, flag);
    }
    return present;
}

static void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) {
    if (egl->Display != 0) {
        return;
    }

    egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
    eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    enum { MAX_CONFIGS = 1024 };
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }

    const EGLint defaultConfigAttribs[] = {
            EGL_RED_SIZE,
            8,
            EGL_GREEN_SIZE,
            8,
            EGL_BLUE_SIZE,
            8,
            EGL_ALPHA_SIZE,
            8, // need alpha for the multi-pass timewarp compositor
            EGL_DEPTH_SIZE,
            0,
            EGL_STENCIL_SIZE,
            0,
            EGL_SAMPLES,
            0,
            EGL_NONE
    };

    const EGLint fallbackConfigAttribs[] = {
            EGL_RED_SIZE,
            8,
            EGL_GREEN_SIZE,
            8,
            EGL_BLUE_SIZE,
            8,
            EGL_ALPHA_SIZE,
            8, // need alpha for the multi-pass timewarp compositor
            EGL_NONE
    };

    const EGLint* configAttribs[2] = {
            defaultConfigAttribs,
            fallbackConfigAttribs,
    };

    egl->Config = 0;
    ALOGD("        Queried %d EGL configs, evaluating ...", numConfigs);
    for (int attempt = 0 ; attempt < 2; attempt++) {
        for (int i = 0; i < numConfigs; i++) {
            EGLint value = 0;

            ALOGD("        Evaluating EGL config %d.", i);
            eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
            if (!checkFlagAndLog(value, EGL_OPENGL_ES3_BIT_KHR, "renderable type"))
            {
                continue;
            }

            // The pbuffer config also needs to be compatible with normal window rendering
            // so it can share textures with the window context.
            eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
            if (!checkFlagAndLog(value, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, "surface type"))
            {
                continue;
            }

            int j = 0;
            ALOGD("        Checking EGL config attributes to make sure they match what we want...");
            for (; configAttribs[attempt][j] != EGL_NONE; j += 2) {
                eglGetConfigAttrib(egl->Display, configs[i], configAttribs[attempt][j], &value);
                if (value != configAttribs[attempt][j + 1]) {
                    ALOGD("        Skipping EGL config due to mismatch in config attribute %d: expected %d, got %d", j / 2, configAttribs[attempt][j + 1], value);
                    break;
                }
            }
            if (configAttribs[attempt][j] == EGL_NONE) {
                ALOGD("        Successfully picked EGL config %d!", i);
                egl->Config = configs[i];
                break;
            }
        }
        if (egl->Config != 0) {
            ALOGW("        Failed to pick EGL config! Fallback to simpler color config.");
            break;
        }
    }
    if (egl->Config == 0) {
        ALOGE("        Failed to pick EGL config!");
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
    egl->Context = eglCreateContext(
            egl->Display,
            egl->Config,
            (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT,
            contextAttribs);
    if (egl->Context == EGL_NO_CONTEXT) {
        ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
    egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
    if (egl->TinySurface == EGL_NO_SURFACE) {
        ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
    ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
    if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) ==
        EGL_FALSE) {
        ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(egl->Display, egl->TinySurface);
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
}

static void ovrEgl_DestroyContext(ovrEgl* egl) {
    if (egl->Display != 0) {
        ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ==
            EGL_FALSE) {
            ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (egl->Context != EGL_NO_CONTEXT) {
        ALOGE("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE) {
            ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Context = EGL_NO_CONTEXT;
    }
    if (egl->TinySurface != EGL_NO_SURFACE) {
        ALOGE("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE) {
            ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        egl->TinySurface = EGL_NO_SURFACE;
    }
    if (egl->Display != 0) {
        ALOGE("        eglTerminate( Display )");
        if (eglTerminate(egl->Display) == EGL_FALSE) {
            ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Display = 0;
    }
}

/*
================================================================================

ovrGeometry

================================================================================
*/

typedef struct {
    GLint Index;
    GLint Size;
    GLenum Type;
    GLboolean Normalized;
    GLsizei Stride;
    const GLvoid* Pointer;
} ovrVertexAttribPointer;

#define MAX_VERTEX_ATTRIB_POINTERS 3

typedef struct {
    GLuint VertexBuffer;
    GLuint IndexBuffer;
    GLuint VertexArrayObject;
    int VertexCount;
    int IndexCount;
    ovrVertexAttribPointer VertexAttribs[MAX_VERTEX_ATTRIB_POINTERS];
} ovrGeometry;

enum VertexAttributeLocation {
    VERTEX_ATTRIBUTE_LOCATION_POSITION,
    VERTEX_ATTRIBUTE_LOCATION_COLOR,
    VERTEX_ATTRIBUTE_LOCATION_UV,
    VERTEX_ATTRIBUTE_LOCATION_TRANSFORM
};

typedef struct {
    enum VertexAttributeLocation location;
    const char* name;
} ovrVertexAttribute;

static ovrVertexAttribute ProgramVertexAttributes[] = {
        {VERTEX_ATTRIBUTE_LOCATION_POSITION, "vertexPosition"},
        {VERTEX_ATTRIBUTE_LOCATION_COLOR, "vertexColor"},
        {VERTEX_ATTRIBUTE_LOCATION_UV, "vertexUv"},
};

static void ovrGeometry_Clear(ovrGeometry* geometry) {
    geometry->VertexBuffer = 0;
    geometry->IndexBuffer = 0;
    geometry->VertexArrayObject = 0;
    geometry->VertexCount = 0;
    geometry->IndexCount = 0;
    for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++) {
        memset(&geometry->VertexAttribs[i], 0, sizeof(geometry->VertexAttribs[i]));
        geometry->VertexAttribs[i].Index = -1;
    }
}

static void ovrGeometry_CreateGroundPlane(ovrGeometry* geometry) {
    typedef struct {
        float positions[12][4];
        unsigned char colors[12][4];
    } ovrCubeVertices;

    static const ovrCubeVertices cubeVertices = {
            // positions
            {{4.5f, 0.0f, 4.5f, 1.0f},
                    {4.5f, 0.0f, -4.5f, 1.0f},
                    {-4.5f, 0.0f, -4.5f, 1.0f},
                    {-4.5f, 0.0f, 4.5f, 1.0f},

                    {4.5f, -10.0f, 4.5f, 1.0f},
                    {4.5f, -10.0f, -4.5f, 1.0f},
                    {-4.5f, -10.0f, -4.5f, 1.0f},
                    {-4.5f, -10.0f, 4.5f, 1.0f},

                    {4.5f, 10.0f, 4.5f, 1.0f},
                    {4.5f, 10.0f, -4.5f, 1.0f},
                    {-4.5f, 10.0f, -4.5f, 1.0f},
                    {-4.5f, 10.0f, 4.5f, 1.0f}},
            // colors
            {{255, 0, 0, 255},
                    {0, 255, 0, 255},
                    {0, 0, 255, 255},
                    {255, 255, 0, 255},

                    {255, 128, 0, 255},
                    {0, 255, 255, 255},
                    {0, 0, 255, 255},
                    {255, 0, 255, 255},

                    {255, 128, 128, 255},
                    {128, 255, 128, 255},
                    {128, 128, 255, 255},
                    {255, 255, 128, 255}},
    };

    static const unsigned short cubeIndices[18] = {
            0,
            1,
            2,
            0,
            2,
            3,

            4,
            5,
            6,
            4,
            6,
            7,

            8,
            9,
            10,
            8,
            10,
            11,
    };

    geometry->VertexCount = 12;
    geometry->IndexCount = 18;

    geometry->VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    geometry->VertexAttribs[0].Size = 4;
    geometry->VertexAttribs[0].Type = GL_FLOAT;
    geometry->VertexAttribs[0].Normalized = false;
    geometry->VertexAttribs[0].Stride = sizeof(cubeVertices.positions[0]);
    geometry->VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, positions);

    geometry->VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
    geometry->VertexAttribs[1].Size = 4;
    geometry->VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    geometry->VertexAttribs[1].Normalized = true;
    geometry->VertexAttribs[1].Stride = sizeof(cubeVertices.colors[0]);
    geometry->VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, colors);

    GL(glGenBuffers(1, &geometry->VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &geometry->IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

static void ovrGeometry_CreateStagePlane(
        ovrGeometry* geometry,
        float minx,
        float minz,
        float maxx,
        float maxz) {
    typedef struct {
        float positions[12][4];
        unsigned char colors[12][4];
    } ovrCubeVertices;

    const ovrCubeVertices cubeVertices = {
            // positions
            {{maxx, 0.0f, maxz, 1.0f},
                    {maxx, 0.0f, minz, 1.0f},
                    {minx, 0.0f, minz, 1.0f},
                    {minx, 0.0f, maxz, 1.0f},

                    {maxx, -3.0f, maxz, 1.0f},
                    {maxx, -3.0f, minz, 1.0f},
                    {minx, -3.0f, minz, 1.0f},
                    {minx, -3.0f, maxz, 1.0f},

                    {maxx, 3.0f, maxz, 1.0f},
                    {maxx, 3.0f, minz, 1.0f},
                    {minx, 3.0f, minz, 1.0f},
                    {minx, 3.0f, maxz, 1.0f}},
            // colors
            {{128, 0, 0, 255},
                    {0, 128, 0, 255},
                    {0, 0, 128, 255},
                    {128, 128, 0, 255},

                    {128, 64, 0, 255},
                    {0, 128, 128, 255},
                    {0, 0, 128, 255},
                    {128, 0, 128, 255},

                    {128, 64, 64, 255},
                    {64, 128, 64, 255},
                    {64, 64, 128, 255},
                    {128, 128, 64, 255}},
    };

    static const unsigned short cubeIndices[18] = {
            0,
            1,
            2,
            0,
            2,
            3,

            4,
            5,
            6,
            4,
            6,
            7,

            8,
            9,
            10,
            8,
            10,
            11,
    };

    geometry->VertexCount = 12;
    geometry->IndexCount = 18;

    geometry->VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    geometry->VertexAttribs[0].Size = 4;
    geometry->VertexAttribs[0].Type = GL_FLOAT;
    geometry->VertexAttribs[0].Normalized = false;
    geometry->VertexAttribs[0].Stride = sizeof(cubeVertices.positions[0]);
    geometry->VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, positions);

    geometry->VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
    geometry->VertexAttribs[1].Size = 4;
    geometry->VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    geometry->VertexAttribs[1].Normalized = true;
    geometry->VertexAttribs[1].Stride = sizeof(cubeVertices.colors[0]);
    geometry->VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, colors);

    GL(glGenBuffers(1, &geometry->VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &geometry->IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

static void ovrGeometry_CreateBox(ovrGeometry* geometry) {
    typedef struct {
        float positions[8][4];
        unsigned char colors[8][4];
    } ovrCubeVertices;

    static const ovrCubeVertices cubeVertices = {
            // positions
            {{-1.0f, -1.0f, -1.0f, 1.0f},
                    {1.0f, -1.0f, -1.0f, 1.0f},
                    {-1.0f, 1.0f, -1.0f, 1.0f},
                    {1.0f, 1.0f, -1.0f, 1.0f},

                    {-1.0f, -1.0f, 1.0f, 1.0f},
                    {1.0f, -1.0f, 1.0f, 1.0f},
                    {-1.0f, 1.0f, 1.0f, 1.0f},
                    {1.0f, 1.0f, 1.0f, 1.0f}},
            // colors
            {
             {255, 0, 0, 255},
                    {250, 255, 0, 255},
                    {250, 0, 255, 255},
                    {255, 255, 0, 255},
                    {255, 0, 0, 255},
                    {250, 255, 0, 255},
                    {250, 0, 255, 255},
                    {255, 255, 0, 255},
            },
    };

    //     6------7
    //    /|     /|
    //   2-+----3 |
    //   | |    | |
    //   | 4----+-5
    //   |/     |/
    //   0------1

    static const unsigned short cubeIndices[36] = {0, 1, 3, 0, 3, 2,

                                                   5, 4, 6, 5, 6, 7,

                                                   4, 0, 2, 4, 2, 6,

                                                   1, 5, 7, 1, 7, 3,

                                                   4, 5, 1, 4, 1, 0,

                                                   2, 3, 7, 2, 7, 6};

    geometry->VertexCount = 8;
    geometry->IndexCount = 36;

    geometry->VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    geometry->VertexAttribs[0].Size = 4;
    geometry->VertexAttribs[0].Type = GL_FLOAT;
    geometry->VertexAttribs[0].Normalized = false;
    geometry->VertexAttribs[0].Stride = sizeof(cubeVertices.positions[0]);
    geometry->VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, positions);

    geometry->VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
    geometry->VertexAttribs[1].Size = 4;
    geometry->VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    geometry->VertexAttribs[1].Normalized = true;
    geometry->VertexAttribs[1].Stride = sizeof(cubeVertices.colors[0]);
    geometry->VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, colors);

    GL(glGenBuffers(1, &geometry->VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &geometry->IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

static void ovrGeometry_Destroy(ovrGeometry* geometry) {
    GL(glDeleteBuffers(1, &geometry->IndexBuffer));
    GL(glDeleteBuffers(1, &geometry->VertexBuffer));

    ovrGeometry_Clear(geometry);
}

static void ovrGeometry_CreateVAO(ovrGeometry* geometry) {
    GL(glGenVertexArrays(1, &geometry->VertexArrayObject));
    GL(glBindVertexArray(geometry->VertexArrayObject));

    GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));

    for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++) {
        if (geometry->VertexAttribs[i].Index != -1) {
            GL(glEnableVertexAttribArray(geometry->VertexAttribs[i].Index));
            GL(glVertexAttribPointer(
                    geometry->VertexAttribs[i].Index,
                    geometry->VertexAttribs[i].Size,
                    geometry->VertexAttribs[i].Type,
                    geometry->VertexAttribs[i].Normalized,
                    geometry->VertexAttribs[i].Stride,
                    geometry->VertexAttribs[i].Pointer));
        }
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));

    GL(glBindVertexArray(0));
}

static void ovrGeometry_DestroyVAO(ovrGeometry* geometry) {
    GL(glDeleteVertexArrays(1, &geometry->VertexArrayObject));
}

/*
================================================================================

ovrProgram

================================================================================
*/

#define MAX_PROGRAM_UNIFORMS 8
#define MAX_PROGRAM_TEXTURES 8

typedef struct {
    GLuint Program;
    GLuint VertexShader;
    GLuint FragmentShader;
    // These will be -1 if not used by the program.
    GLint UniformLocation[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint UniformBinding[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint Textures[MAX_PROGRAM_TEXTURES]; // Texture%i
} ovrProgram;

typedef enum ovrUniformIndex {
    MODEL_MATRIX,
    VIEW_PROJ_MATRIX,
    SCENE_MATRICES,
} ovrUniformIndex;

typedef enum ovrUniformType {
    VECTOR4,
    MATRIX4X4,
    INTEGER,
    BUFFER,
} ovrUniformType;

typedef struct {
    ovrUniformIndex index;
    ovrUniformType type;
    const char* name;
} ovrUniform;

static ovrUniform ProgramUniforms[] = {
        {MODEL_MATRIX, MATRIX4X4, "modelMatrix"},
        {VIEW_PROJ_MATRIX, MATRIX4X4, "viewProjectionMatrix"},
};

static void ovrProgram_Clear(ovrProgram* program) {
    program->Program = 0;
    program->VertexShader = 0;
    program->FragmentShader = 0;
    memset(program->UniformLocation, 0, sizeof(program->UniformLocation));
    memset(program->UniformBinding, 0, sizeof(program->UniformBinding));
    memset(program->Textures, 0, sizeof(program->Textures));
}

static bool
ovrProgram_Create(ovrProgram* program, const char* vertexSource, const char* fragmentSource) {
    GLint r;

    GL(program->VertexShader = glCreateShader(GL_VERTEX_SHADER));

    GL(glShaderSource(program->VertexShader, 1, &vertexSource, 0));
    GL(glCompileShader(program->VertexShader));
    GL(glGetShaderiv(program->VertexShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(program->VertexShader, sizeof(msg), 0, msg));
        ALOGE("%s\n%s\n", vertexSource, msg);
        return false;
    }

    GL(program->FragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
    GL(glShaderSource(program->FragmentShader, 1, &fragmentSource, 0));
    GL(glCompileShader(program->FragmentShader));
    GL(glGetShaderiv(program->FragmentShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(program->FragmentShader, sizeof(msg), 0, msg));
        ALOGE("%s\n%s\n", fragmentSource, msg);
        return false;
    }

    GL(program->Program = glCreateProgram());
    GL(glAttachShader(program->Program, program->VertexShader));
    GL(glAttachShader(program->Program, program->FragmentShader));

    // Bind the vertex attribute locations.
    for (size_t i = 0; i < sizeof(ProgramVertexAttributes) / sizeof(ProgramVertexAttributes[0]);
         i++) {
        GL(glBindAttribLocation(
                program->Program,
                ProgramVertexAttributes[i].location,
                ProgramVertexAttributes[i].name));
    }

    GL(glLinkProgram(program->Program));
    GL(glGetProgramiv(program->Program, GL_LINK_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetProgramInfoLog(program->Program, sizeof(msg), 0, msg));
        ALOGE("Linking program failed: %s\n", msg);
        return false;
    }

    int numBufferBindings = 0;

    // Get the uniform locations.
    memset(program->UniformLocation, -1, sizeof(program->UniformLocation));
    for (size_t i = 0; i < sizeof(ProgramUniforms) / sizeof(ProgramUniforms[0]); i++) {
        const int uniformIndex = ProgramUniforms[i].index;
        if (ProgramUniforms[i].type == BUFFER) {
            GL(program->UniformLocation[uniformIndex] =
                       glGetUniformBlockIndex(program->Program, ProgramUniforms[i].name));
            program->UniformBinding[uniformIndex] = numBufferBindings++;
            GL(glUniformBlockBinding(
                    program->Program,
                    program->UniformLocation[uniformIndex],
                    program->UniformBinding[uniformIndex]));
        } else {
            GL(program->UniformLocation[uniformIndex] =
                       glGetUniformLocation(program->Program, ProgramUniforms[i].name));
            program->UniformBinding[uniformIndex] = program->UniformLocation[uniformIndex];
        }
    }

    GL(glUseProgram(program->Program));

    // Get the texture locations.
    for (int i = 0; i < MAX_PROGRAM_TEXTURES; i++) {
        char name[32];
        sprintf(name, "Texture%i", i);
        program->Textures[i] = glGetUniformLocation(program->Program, name);
        if (program->Textures[i] != -1) {
            GL(glUniform1i(program->Textures[i], i));
        }
    }

    GL(glUseProgram(0));

    return true;
}

static void ovrProgram_Destroy(ovrProgram* program) {
    if (program->Program != 0) {
        GL(glDeleteProgram(program->Program));
        program->Program = 0;
    }
    if (program->VertexShader != 0) {
        GL(glDeleteShader(program->VertexShader));
        program->VertexShader = 0;
    }
    if (program->FragmentShader != 0) {
        GL(glDeleteShader(program->FragmentShader));
        program->FragmentShader = 0;
    }
}

static const char VERTEX_SHADER[] =
        "#version 300 es\n"
        "in vec3 vertexPosition;\n"
        "in vec4 vertexColor;\n"
        "uniform mat4 viewProjectionMatrix;\n"
        "uniform mat4 modelMatrix;\n"
        "out vec4 fragmentColor;\n"
        "void main()\n"
        "{\n"
        " gl_Position = viewProjectionMatrix * ( modelMatrix * vec4( vertexPosition, 1.0 ) );\n"
        " fragmentColor = vertexColor;\n"
        "}\n";

static const char FRAGMENT_SHADER[] =
        "#version 300 es\n"
        "in lowp vec4 fragmentColor;\n"
        "out lowp vec4 outColor;\n"
        "void main()\n"
        "{\n"
        " outColor = fragmentColor;\n"
        "}\n";

typedef struct {
    XrSwapchain Handle;
    uint32_t Width;
    uint32_t Height;
} ovrSwapChain;

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int Width;
    int Height;
    int Multisamples;
    uint32_t TextureSwapChainLength;
    uint32_t TextureSwapChainIndex;
    ovrSwapChain ColorSwapChain;
    XrSwapchainImageOpenGLESKHR* ColorSwapChainImage;
    GLuint* DepthBuffers;
    GLuint* FrameBuffers;
} ovrFramebuffer;

static void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->Multisamples = 0;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->ColorSwapChain.Handle = XR_NULL_HANDLE;
    frameBuffer->ColorSwapChain.Width = 0;
    frameBuffer->ColorSwapChain.Height = 0;
    frameBuffer->ColorSwapChainImage = NULL;
    frameBuffer->DepthBuffers = NULL;
    frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create(
        XrSession session,
        ovrFramebuffer* frameBuffer,
        const GLenum colorFormat,
        const int width,
        const int height,
        const int multisamples) {
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
            (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
                    "glRenderbufferStorageMultisampleEXT");
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
            (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
                    "glFramebufferTexture2DMultisampleEXT");

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->Multisamples = multisamples;

    GLenum requestedGLFormat = colorFormat;

    // Get the number of supported formats.
    uint32_t numInputFormats = 0;
    uint32_t numOutputFormats = 0;
    OXR(xrEnumerateSwapchainFormats(session, numInputFormats, &numOutputFormats, NULL));

    // Allocate an array large enough to contain the supported formats.
    numInputFormats = numOutputFormats;
    int64_t* supportedFormats = (int64_t*)malloc(numOutputFormats * sizeof(int64_t));
    if (supportedFormats != NULL) {
        OXR(xrEnumerateSwapchainFormats(
                session, numInputFormats, &numOutputFormats, supportedFormats));
    }

    // Verify the requested format is supported.
    uint64_t selectedFormat = 0;
    for (uint32_t i = 0; i < numOutputFormats; i++) {
        if (supportedFormats[i] == requestedGLFormat) {
            selectedFormat = supportedFormats[i];
            break;
        }
    }

    free(supportedFormats);

    if (selectedFormat == 0) {
        ALOGE("Format not supported");
    }

    XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapChainCreateInfo.usageFlags =
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.format = selectedFormat;
    swapChainCreateInfo.sampleCount = 1;
    swapChainCreateInfo.width = width;
    swapChainCreateInfo.height = height;
    swapChainCreateInfo.faceCount = 1;
    swapChainCreateInfo.arraySize = 1;
    swapChainCreateInfo.mipCount = 1;

    // Enable Foveation on this swapchain
    XrSwapchainCreateInfoFoveationFB swapChainFoveationCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB};
    swapChainCreateInfo.next = &swapChainFoveationCreateInfo;

    frameBuffer->ColorSwapChain.Width = swapChainCreateInfo.width;
    frameBuffer->ColorSwapChain.Height = swapChainCreateInfo.height;

    // Create the swapchain.
    OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &frameBuffer->ColorSwapChain.Handle));
    // Get the number of swapchain images.
    OXR(xrEnumerateSwapchainImages(
            frameBuffer->ColorSwapChain.Handle, 0, &frameBuffer->TextureSwapChainLength, NULL));
    // Allocate the swapchain images array.
    frameBuffer->ColorSwapChainImage = (XrSwapchainImageOpenGLESKHR*)malloc(
            frameBuffer->TextureSwapChainLength * sizeof(XrSwapchainImageOpenGLESKHR));

    // Populate the swapchain image array.
    for (uint32_t i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        frameBuffer->ColorSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
        frameBuffer->ColorSwapChainImage[i].next = NULL;
    }
    OXR(xrEnumerateSwapchainImages(
            frameBuffer->ColorSwapChain.Handle,
            frameBuffer->TextureSwapChainLength,
            &frameBuffer->TextureSwapChainLength,
            (XrSwapchainImageBaseHeader*)frameBuffer->ColorSwapChainImage));

    frameBuffer->DepthBuffers =
            (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
    frameBuffer->FrameBuffers =
            (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

    for (uint32_t i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the color buffer texture.
        const GLuint colorTexture = frameBuffer->ColorSwapChainImage[i].image;

        GLenum colorTextureTarget = GL_TEXTURE_2D;
        GL(glBindTexture(colorTextureTarget, colorTexture));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glBindTexture(colorTextureTarget, 0));

        if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL &&
            glFramebufferTexture2DMultisampleEXT != NULL) {
            // Create multisampled depth buffer.
            GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
            GL(glRenderbufferStorageMultisampleEXT(
                    GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

            // Create the frame buffer.
            // NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
            GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
            GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
            GL(glFramebufferTexture2DMultisampleEXT(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D,
                    colorTexture,
                    0,
                    multisamples));
            GL(glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,
                    frameBuffer->DepthBuffers[i]));
            GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
            GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                        "Incomplete frame buffer object: %s",
                        GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        } else {
            // Create depth buffer.
            GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
            GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

            // Create the frame buffer.
            GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
            GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
            GL(glFramebufferRenderbuffer(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,
                    frameBuffer->DepthBuffers[i]));
            GL(glFramebufferTexture2D(
                    GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
            GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
            GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                        "Incomplete frame buffer object: %s",
                        GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        }
    }

    return true;
}

static void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) {
    GL(glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers));
    GL(glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
    OXR(xrDestroySwapchain(frameBuffer->ColorSwapChain.Handle));
    free(frameBuffer->ColorSwapChainImage);

    free(frameBuffer->DepthBuffers);
    free(frameBuffer->FrameBuffers);

    ovrFramebuffer_Clear(frameBuffer);
}

static void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) {
    GL(glBindFramebuffer(
            GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]));
}

static void ovrFramebuffer_SetNone() {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

static void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) {
    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

    // We now let the resolve happen implicitly.
}

static void ovrFramebuffer_Acquire(ovrFramebuffer* frameBuffer) {
    // Acquire the swapchain image
    XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    OXR(xrAcquireSwapchainImage(
            frameBuffer->ColorSwapChain.Handle, &acquireInfo, &frameBuffer->TextureSwapChainIndex));

    XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = 1000000000; /* timeout in nanoseconds */
    XrResult res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
    int i = 0;
    while (res == XR_TIMEOUT_EXPIRED) {
        res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
        i++;
        ALOGV(
                " Retry xrWaitSwapchainImage %d times due to XR_TIMEOUT_EXPIRED (duration %f seconds)",
                i,
                waitInfo.timeout * (1E-9));
    }
}

static void ovrFramebuffer_Release(ovrFramebuffer* frameBuffer) {
    XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    OXR(xrReleaseSwapchainImage(frameBuffer->ColorSwapChain.Handle, &releaseInfo));
}

/*
================================================================================

KTX Loading

================================================================================
*/

typedef struct {
    int width;
    int height;
    int depth;
    GLenum internalFormat;
    GLenum target;
    int numberOfArrayElements;
    int numberOfFaces;
    int numberOfMipmapLevels;
    bool mipSizeStored;

    const void* data; // caller responsible for freeing memory
    int dataOffset;
    int dataSize;
} ktxImageInfo_t;

bool LoadImageDataFromKTXFile(AAssetManager* amgr, const char* assetName, ktxImageInfo_t* outInfo) {
#pragma pack(1)
    typedef struct {
        unsigned char identifier[12];
        unsigned int endianness;
        unsigned int glType;
        unsigned int glTypeSize;
        unsigned int glFormat;
        unsigned int glInternalFormat;
        unsigned int glBaseInternalFormat;
        unsigned int pixelWidth;
        unsigned int pixelHeight;
        unsigned int pixelDepth;
        unsigned int numberOfArrayElements;
        unsigned int numberOfFaces;
        unsigned int numberOfMipmapLevels;
        unsigned int bytesOfKeyValueData;
    } GlHeaderKTX_t;
#pragma pack()

    AAsset* asset = AAssetManager_open(amgr, assetName, AASSET_MODE_BUFFER);
    if (asset == NULL) {
        ALOGE("Failed to open %s", assetName);
        return false;
    }
    const void* buffer = AAsset_getBuffer(asset);
    if (buffer == NULL) {
        ALOGE("Failed to read %s", assetName);
        return false;
    }
    size_t bufferSize = AAsset_getLength(asset);

    if (bufferSize < sizeof(GlHeaderKTX_t)) {
        ALOGE("%s: Invalid KTX file", assetName);
        return false;
    }

    const unsigned char fileIdentifier[12] = {
            (unsigned char)'\xAB',
            'K',
            'T',
            'X',
            ' ',
            '1',
            '1',
            (unsigned char)'\xBB',
            '\r',
            '\n',
            '\x1A',
            '\n'};

    const GlHeaderKTX_t* header = (GlHeaderKTX_t*)buffer;
    if (memcmp(header->identifier, fileIdentifier, sizeof(fileIdentifier)) != 0) {
        ALOGE("%s: Invalid KTX file", assetName);
        return false;
    }
    // only support little endian
    if (header->endianness != 0x04030201) {
        ALOGE("%s: KTX file has wrong endianness", assetName);
        return false;
    }
    // only support unsigned byte
    if (header->glType != GL_UNSIGNED_BYTE) {
        ALOGE("%s: KTX file has unsupported glType %d", assetName, header->glType);
        return false;
    }
    // skip the key value data
    const size_t startTex = sizeof(GlHeaderKTX_t) + header->bytesOfKeyValueData;
    if ((startTex < sizeof(GlHeaderKTX_t)) || (startTex >= bufferSize)) {
        ALOGE("%s: Invalid KTX header sizes", assetName);
        return false;
    }

    outInfo->width = header->pixelWidth;
    outInfo->height = header->pixelHeight;
    outInfo->depth = header->pixelDepth;
    outInfo->internalFormat = header->glInternalFormat;
    outInfo->numberOfArrayElements =
            (header->numberOfArrayElements >= 1) ? header->numberOfArrayElements : 1;
    outInfo->numberOfFaces = (header->numberOfFaces >= 1) ? header->numberOfFaces : 1;
    outInfo->numberOfMipmapLevels = header->numberOfMipmapLevels;
    outInfo->mipSizeStored = true;
    outInfo->data = buffer;
    outInfo->dataOffset = startTex;
    outInfo->dataSize = bufferSize;

    outInfo->target =
            ((outInfo->depth > 1)
             ? GL_TEXTURE_3D
             : ((outInfo->numberOfFaces > 1)
                ? ((outInfo->numberOfArrayElements > 1) ? GL_TEXTURE_CUBE_MAP_ARRAY
                                                        : GL_TEXTURE_CUBE_MAP)
                : ((outInfo->numberOfArrayElements > 1) ? GL_TEXTURE_2D_ARRAY
                                                        : GL_TEXTURE_2D)));

    return true;
}

bool LoadTextureFromKTXImageMemory(const ktxImageInfo_t* info, const int textureId) {
    if (info == NULL) {
        return false;
    }

    if (info->data != NULL) {
        GL(glBindTexture(info->target, textureId));

        const int numDataLevels = info->numberOfMipmapLevels;
        const unsigned char* levelData = (const unsigned char*)(info->data) + info->dataOffset;
        const unsigned char* endOfBuffer = levelData + (info->dataSize - info->dataOffset);

        for (int i = 0; i < numDataLevels; i++) {
            const int w = (info->width >> i) >= 1 ? (info->width >> i) : 1;
            const int h = (info->height >> i) >= 1 ? (info->height >> i) : 1;
            const int d = (info->depth >> i) >= 1 ? (info->depth >> i) : 1;

            size_t mipSize = 0;
            bool compressed = false;
            GLenum glFormat = GL_RGBA;
            GLenum glDataType = GL_UNSIGNED_BYTE;
            switch (info->internalFormat) {
                case GL_R8: {
                    mipSize = w * h * d * 1 * sizeof(unsigned char);
                    glFormat = GL_RED;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
                case GL_RG8: {
                    mipSize = w * h * d * 2 * sizeof(unsigned char);
                    glFormat = GL_RG;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
                case GL_RGB8: {
                    mipSize = w * h * d * 3 * sizeof(unsigned char);
                    glFormat = GL_RGB;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
                case GL_RGBA8: {
                    mipSize = w * h * d * 4 * sizeof(unsigned char);
                    glFormat = GL_RGBA;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
            }
            if (mipSize == 0) {
                ALOGE("Unsupported image format %d", info->internalFormat);
                GL(glBindTexture(info->target, 0));
                return false;
            }

            if (info->numberOfArrayElements > 1) {
                mipSize = mipSize * info->numberOfArrayElements * info->numberOfFaces;
            }

            // ALOGV( "mipSize%d = %zu (%d)", i, mipSize, info->numberOfMipmapLevels );

            if (info->mipSizeStored) {
                if (levelData + 4 > endOfBuffer) {
                    ALOGE("Image data exceeds buffer size");
                    GL(glBindTexture(info->target, 0));
                    return false;
                }
                const size_t storedMipSize = (size_t) * (const unsigned int*)levelData;
                // ALOGV( "storedMipSize = %zu", storedMipSize );
                mipSize = storedMipSize;
                levelData += 4;
            }

            if (info->depth <= 1 && info->numberOfArrayElements <= 1) {
                for (int face = 0; face < info->numberOfFaces; face++) {
                    if (mipSize <= 0 || mipSize > (size_t)(endOfBuffer - levelData)) {
                        ALOGE(
                                "Mip %d data exceeds buffer size (%zu > %zu)",
                                i,
                                mipSize,
                                (endOfBuffer - levelData));
                        GL(glBindTexture(info->target, 0));
                        return false;
                    }

                    const GLenum uploadTarget = (info->target == GL_TEXTURE_CUBE_MAP)
                                                ? GL_TEXTURE_CUBE_MAP_POSITIVE_X
                                                : GL_TEXTURE_2D;
                    if (compressed) {
                        GL(glCompressedTexSubImage2D(
                                uploadTarget + face,
                                i,
                                0,
                                0,
                                w,
                                h,
                                info->internalFormat,
                                (GLsizei)mipSize,
                                levelData));
                    } else {
                        GL(glTexSubImage2D(
                                uploadTarget + face, i, 0, 0, w, h, glFormat, glDataType, levelData));
                    }

                    levelData += mipSize;

                    if (info->mipSizeStored) {
                        levelData += 3 - ((mipSize + 3) % 4);
                        if (levelData > endOfBuffer) {
                            ALOGE("Image data exceeds buffer size");
                            GL(glBindTexture(info->target, 0));
                            return false;
                        }
                    }
                }
            } else {
                if (mipSize <= 0 || mipSize > (size_t)(endOfBuffer - levelData)) {
                    ALOGE(
                            "Mip %d data exceeds buffer size (%zu > %zu)",
                            i,
                            mipSize,
                            (endOfBuffer - levelData));
                    GL(glBindTexture(info->target, 0));
                    return false;
                }

                if (compressed) {
                    GL(glCompressedTexSubImage3D(
                            info->target,
                            i,
                            0,
                            0,
                            0,
                            w,
                            h,
                            d * info->numberOfArrayElements,
                            info->internalFormat,
                            (GLsizei)mipSize,
                            levelData));
                } else {
                    GL(glTexSubImage3D(
                            info->target,
                            i,
                            0,
                            0,
                            0,
                            w,
                            h,
                            d * info->numberOfArrayElements,
                            glFormat,
                            glDataType,
                            levelData));
                }

                levelData += mipSize;

                if (info->mipSizeStored) {
                    levelData += 3 - ((mipSize + 3) % 4);
                    if (levelData > endOfBuffer) {
                        ALOGE("Image data exceeds buffer size");
                        GL(glBindTexture(info->target, 0));
                        return false;
                    }
                }
            }
        }

        GL(glTexParameteri(
                info->target,
                GL_TEXTURE_MIN_FILTER,
                (info->numberOfMipmapLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
        GL(glTexParameteri(info->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

        GL(glBindTexture(info->target, 0));
    }

    return true;
}

/*
================================================================================

ovrTrackedController

================================================================================
*/

typedef struct {
    bool Active;
    XrPosef Pose;
} ovrTrackedController;

static void ovrTrackedController_Clear(ovrTrackedController* controller) {
    controller->Active = false;
    XrPosef_CreateIdentity(&controller->Pose);
}

/*
================================================================================

ovrScene

================================================================================
*/

enum ovrBackgroundType {
    BACKGROUND_NONE,
    BACKGROUND_CUBEMAP,
    BACKGROUND_EQUIRECT,
    MAX_BACKGROUND_TYPES
};

typedef struct {
    bool CreatedScene;
    bool CreatedVAOs;
    ovrProgram Program;
    ovrGeometry GroundPlane;
    ovrGeometry Box;
    ovrTrackedController TrackedController[4]; // left aim, left grip, right aim, right grip

    ovrSwapChain CubeMapSwapChain;
    XrSwapchainImageOpenGLESKHR* CubeMapSwapChainImage;
    ovrSwapChain EquirectSwapChain;
    XrSwapchainImageOpenGLESKHR* EquirectSwapChainImage;
    ovrSwapChain CylinderSwapChain;
    XrSwapchainImageOpenGLESKHR* CylinderSwapChainImage;
    ovrSwapChain QuadSwapChain;
    XrSwapchainImageOpenGLESKHR* QuadSwapChainImage;

    enum ovrBackgroundType BackGroundType;
} ovrScene;

static void ovrScene_Clear(ovrScene* scene) {
    scene->CreatedScene = false;
    scene->CreatedVAOs = false;
    ovrProgram_Clear(&scene->Program);
    ovrGeometry_Clear(&scene->GroundPlane);
    ovrGeometry_Clear(&scene->Box);
    for (int i = 0; i < 4; i++) {
        ovrTrackedController_Clear(&scene->TrackedController[i]);
    }

    scene->CubeMapSwapChain.Handle = XR_NULL_HANDLE;
    scene->CubeMapSwapChain.Width = 0;
    scene->CubeMapSwapChain.Height = 0;
    scene->CubeMapSwapChainImage = NULL;
    scene->EquirectSwapChain.Handle = XR_NULL_HANDLE;
    scene->EquirectSwapChain.Width = 0;
    scene->EquirectSwapChain.Height = 0;
    scene->EquirectSwapChainImage = NULL;
    scene->CylinderSwapChain.Handle = XR_NULL_HANDLE;
    scene->CylinderSwapChain.Width = 0;
    scene->CylinderSwapChain.Height = 0;
    scene->CylinderSwapChainImage = NULL;
    scene->QuadSwapChain.Handle = XR_NULL_HANDLE;
    scene->QuadSwapChain.Width = 0;
    scene->QuadSwapChain.Height = 0;
    scene->QuadSwapChainImage = NULL;

    scene->BackGroundType = BACKGROUND_EQUIRECT;
}

static bool ovrScene_IsCreated(ovrScene* scene) {
    return scene->CreatedScene;
}

static void ovrScene_CreateVAOs(ovrScene* scene) {
    if (!scene->CreatedVAOs) {
        ovrGeometry_CreateVAO(&scene->GroundPlane);
        ovrGeometry_CreateVAO(&scene->Box);
        scene->CreatedVAOs = true;
    }
}

static void ovrScene_DestroyVAOs(ovrScene* scene) {
    if (scene->CreatedVAOs) {
        ovrGeometry_DestroyVAO(&scene->GroundPlane);
        ovrGeometry_DestroyVAO(&scene->Box);
        scene->CreatedVAOs = false;
    }
}

static void
ovrScene_Create(AAssetManager* amgr, XrInstance instance, XrSession session, ovrScene* scene) {
    // Simple ground plane and box geometry.
    {
        ovrProgram_Create(&scene->Program, VERTEX_SHADER, FRAGMENT_SHADER);
        ovrGeometry_CreateGroundPlane(&scene->GroundPlane);
        ovrGeometry_CreateBox(&scene->Box);

        ovrScene_CreateVAOs(scene);
    }

    // Simple cubemap loaded from ktx file on the sdcard. NOTE: Currently only
    // handles texture2d or cubemap types.
    {
        ktxImageInfo_t ktxImageInfo;
        memset(&ktxImageInfo, 0, sizeof(ktxImageInfo_t));
        if (LoadImageDataFromKTXFile(amgr, "cubemap256.ktx", &ktxImageInfo) == true) {
            int swapChainTexId = 0;
            XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swapChainCreateInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
            swapChainCreateInfo.usageFlags =
                    XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            swapChainCreateInfo.format = ktxImageInfo.internalFormat;
            swapChainCreateInfo.sampleCount = 1;
            swapChainCreateInfo.width = ktxImageInfo.width;
            swapChainCreateInfo.height = ktxImageInfo.height;
            swapChainCreateInfo.faceCount = ktxImageInfo.numberOfFaces;
            swapChainCreateInfo.arraySize = ktxImageInfo.numberOfArrayElements;
            swapChainCreateInfo.mipCount = ktxImageInfo.numberOfMipmapLevels;

            scene->CubeMapSwapChain.Width = swapChainCreateInfo.width;
            scene->CubeMapSwapChain.Height = swapChainCreateInfo.height;

            // Create the swapchain.
            OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &scene->CubeMapSwapChain.Handle));
            // Get the number of swapchain images.
            uint32_t length;
            OXR(xrEnumerateSwapchainImages(scene->CubeMapSwapChain.Handle, 0, &length, NULL));
            scene->CubeMapSwapChainImage =
                    (XrSwapchainImageOpenGLESKHR*)malloc(length * sizeof(XrSwapchainImageOpenGLESKHR));
            for (uint32_t i = 0; i < length; i++) {
                scene->CubeMapSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
                scene->CubeMapSwapChainImage[i].next = NULL;
            }
            OXR(xrEnumerateSwapchainImages(
                    scene->CubeMapSwapChain.Handle,
                    length,
                    &length,
                    (XrSwapchainImageBaseHeader*)scene->CubeMapSwapChainImage));

            swapChainTexId = scene->CubeMapSwapChainImage[0].image;

            if (LoadTextureFromKTXImageMemory(&ktxImageInfo, swapChainTexId) == false) {
                ALOGE("Failed to load texture from KTX image");
            }

            uint32_t index = 0;
            XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            OXR(xrAcquireSwapchainImage(scene->CubeMapSwapChain.Handle, &acquireInfo, &index));

            XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            OXR(xrWaitSwapchainImage(scene->CubeMapSwapChain.Handle, &waitInfo));

            XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            OXR(xrReleaseSwapchainImage(scene->CubeMapSwapChain.Handle, &releaseInfo));
        } else {
            ALOGE("Failed to load KTX image - generating procedural cubemap");
            XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swapChainCreateInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
            swapChainCreateInfo.usageFlags =
                    XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            swapChainCreateInfo.format = GL_RGBA8;
            swapChainCreateInfo.sampleCount = 1;
            int w = 512;
            swapChainCreateInfo.width = w;
            swapChainCreateInfo.height = w;
            swapChainCreateInfo.faceCount = 6;
            swapChainCreateInfo.arraySize = 1;
            swapChainCreateInfo.mipCount = 1;

            scene->CubeMapSwapChain.Width = w;
            scene->CubeMapSwapChain.Height = w;

            // Create the swapchain.
            OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &scene->CubeMapSwapChain.Handle));
            // Get the number of swapchain images.
            uint32_t length;
            OXR(xrEnumerateSwapchainImages(scene->CubeMapSwapChain.Handle, 0, &length, NULL));
            scene->CubeMapSwapChainImage =
                    (XrSwapchainImageOpenGLESKHR*)malloc(length * sizeof(XrSwapchainImageOpenGLESKHR));
            for (uint32_t i = 0; i < length; i++) {
                scene->CubeMapSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
                scene->CubeMapSwapChainImage[i].next = NULL;
            }
            OXR(xrEnumerateSwapchainImages(
                    scene->CubeMapSwapChain.Handle,
                    length,
                    &length,
                    (XrSwapchainImageBaseHeader*)scene->CubeMapSwapChainImage));

            uint32_t* img = (uint32_t*)malloc(w * w * sizeof(uint32_t));
            for (int j = 0; j < w; j++) {
                for (int i = 0; i < w; i++) {
                    img[j * w + i] = 0xff000000 + (((j * 256) / w) << 8) + (((i * 256) / w) << 0);
                }
            }
            GL(glBindTexture(GL_TEXTURE_CUBE_MAP, scene->CubeMapSwapChainImage[0].image));
            const int start = 3 * w / 8;
            const int stop = 5 * w / 8;
            for (int j = start; j < stop; j++) {
                for (int i = start; i < stop; i++) {
                    img[j * w + i] = 0xff0000ff;
                }
            }
            GL(glTexSubImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, w, w, GL_RGBA, GL_UNSIGNED_BYTE, img));
            for (int j = start; j < stop; j++) {
                for (int i = start; i < stop; i++) {
                    img[j * w + i] = 0xffffff00;
                }
            }
            GL(glTexSubImage2D(
                    GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, w, w, GL_RGBA, GL_UNSIGNED_BYTE, img));
            for (int j = start; j < stop; j++) {
                for (int i = start; i < stop; i++) {
                    img[j * w + i] = 0xff00ff00;
                }
            }
            GL(glTexSubImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, 0, 0, w, w, GL_RGBA, GL_UNSIGNED_BYTE, img));
            for (int j = start; j < stop; j++) {
                for (int i = start; i < stop; i++) {
                    img[j * w + i] = 0xffff00ff;
                }
            }
            GL(glTexSubImage2D(
                    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 0, 0, w, w, GL_RGBA, GL_UNSIGNED_BYTE, img));
            for (int j = start; j < stop; j++) {
                for (int i = start; i < stop; i++) {
                    img[j * w + i] = 0xffff0000;
                }
            }
            GL(glTexSubImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, w, w, GL_RGBA, GL_UNSIGNED_BYTE, img));
            for (int j = start; j < stop; j++) {
                for (int i = start; i < stop; i++) {
                    img[j * w + i] = 0xff00ffff;
                }
            }
            GL(glTexSubImage2D(
                    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, 0, 0, w, w, GL_RGBA, GL_UNSIGNED_BYTE, img));

            free(img);

            uint32_t index = 0;
            XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            OXR(xrAcquireSwapchainImage(scene->CubeMapSwapChain.Handle, &acquireInfo, &index));

            XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            OXR(xrWaitSwapchainImage(scene->CubeMapSwapChain.Handle, &waitInfo));

            XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            OXR(xrReleaseSwapchainImage(scene->CubeMapSwapChain.Handle, &releaseInfo));
        }
    }

    // Simple checkerboard pattern.
    {
        const int width = 512;
        const int height = 256;
        XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapChainCreateInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
        swapChainCreateInfo.usageFlags =
                XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainCreateInfo.format = GL_SRGB8_ALPHA8;
        swapChainCreateInfo.sampleCount = 1;
        swapChainCreateInfo.width = width;
        swapChainCreateInfo.height = height;
        swapChainCreateInfo.faceCount = 1;
        swapChainCreateInfo.arraySize = 1;
        swapChainCreateInfo.mipCount = 1;

        scene->EquirectSwapChain.Width = swapChainCreateInfo.width;
        scene->EquirectSwapChain.Height = swapChainCreateInfo.height;

        // Create the swapchain.
        OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &scene->EquirectSwapChain.Handle));
        // Get the number of swapchain images.
        uint32_t length;
        OXR(xrEnumerateSwapchainImages(scene->EquirectSwapChain.Handle, 0, &length, NULL));
        scene->EquirectSwapChainImage =
                (XrSwapchainImageOpenGLESKHR*)malloc(length * sizeof(XrSwapchainImageOpenGLESKHR));

        for (uint32_t i = 0; i < length; i++) {
            scene->EquirectSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
            scene->EquirectSwapChainImage[i].next = NULL;
        }
        OXR(xrEnumerateSwapchainImages(
                scene->EquirectSwapChain.Handle,
                length,
                &length,
                (XrSwapchainImageBaseHeader*)scene->EquirectSwapChainImage));

        uint32_t* texData = (uint32_t*)malloc(width * height * sizeof(uint32_t));

        for (int y = 0; y < height; y++) {
            float greenfrac = y / (height - 1.0f);
            uint32_t green = ((uint32_t)(255 * greenfrac)) << 8;
            for (int x = 0; x < width; x++) {
                float redfrac = x / (width - 1.0f);
                uint32_t red = ((uint32_t)(255 * redfrac));
                texData[y * width + x] = (x ^ y) & 16 ? 0xFFFFFFFF : (0xFF000000 | green | red);
            }
        }

        const int texId = scene->EquirectSwapChainImage[0].image;
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        glBindTexture(GL_TEXTURE_2D, 0);
        free(texData);

        uint32_t index = 0;
        XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        OXR(xrAcquireSwapchainImage(scene->EquirectSwapChain.Handle, &acquireInfo, &index));

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        OXR(xrWaitSwapchainImage(scene->EquirectSwapChain.Handle, &waitInfo));

        XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        OXR(xrReleaseSwapchainImage(scene->EquirectSwapChain.Handle, &releaseInfo));
    }

    // Simple checkerboard pattern.
    {
        static const int CYLINDER_WIDTH = 512;
        static const int CYLINDER_HEIGHT = 128;

        XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapChainCreateInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
        swapChainCreateInfo.usageFlags =
                XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainCreateInfo.format = GL_SRGB8_ALPHA8;
        swapChainCreateInfo.sampleCount = 1;
        swapChainCreateInfo.width = CYLINDER_WIDTH;
        swapChainCreateInfo.height = CYLINDER_HEIGHT;
        swapChainCreateInfo.faceCount = 1;
        swapChainCreateInfo.arraySize = 1;
        swapChainCreateInfo.mipCount = 1;

        scene->CylinderSwapChain.Width = swapChainCreateInfo.width;
        scene->CylinderSwapChain.Height = swapChainCreateInfo.height;

        // Create the swapchain.
        OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &scene->CylinderSwapChain.Handle));
        // Get the number of swapchain images.
        uint32_t length;
        OXR(xrEnumerateSwapchainImages(scene->CylinderSwapChain.Handle, 0, &length, NULL));
        scene->CylinderSwapChainImage =
                (XrSwapchainImageOpenGLESKHR*)malloc(length * sizeof(XrSwapchainImageOpenGLESKHR));
        for (uint32_t i = 0; i < length; i++) {
            scene->CylinderSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
            scene->CylinderSwapChainImage[i].next = NULL;
        }
        OXR(xrEnumerateSwapchainImages(
                scene->CylinderSwapChain.Handle,
                length,
                &length,
                (XrSwapchainImageBaseHeader*)scene->CylinderSwapChainImage));

        uint32_t* texData = (uint32_t*)malloc(CYLINDER_WIDTH * CYLINDER_HEIGHT * sizeof(uint32_t));

        for (int y = 0; y < CYLINDER_HEIGHT; y++) {
            for (int x = 0; x < CYLINDER_WIDTH; x++) {
                texData[y * CYLINDER_WIDTH + x] = (x ^ y) & 64 ? 0xFF6464F0 : 0xFFF06464;
            }
        }
        for (int y = 0; y < CYLINDER_HEIGHT; y++) {
            int g = 255.0f * (y / (CYLINDER_HEIGHT - 1.0f));
            texData[y * CYLINDER_WIDTH] = 0xff000000 | (g << 8);
        }
        for (int x = 0; x < CYLINDER_WIDTH; x++) {
            int r = 255.0f * (x / (CYLINDER_WIDTH - 1.0f));
            texData[x] = 0xff000000 | r;
        }

        const int texId = scene->CylinderSwapChainImage[0].image;

        glBindTexture(GL_TEXTURE_2D, texId);
        glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                CYLINDER_WIDTH,
                CYLINDER_HEIGHT,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                texData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindTexture(GL_TEXTURE_2D, 0);

        free(texData);

        uint32_t index = 0;
        XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        OXR(xrAcquireSwapchainImage(scene->CylinderSwapChain.Handle, &acquireInfo, &index));

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        OXR(xrWaitSwapchainImage(scene->CylinderSwapChain.Handle, &waitInfo));

        XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        OXR(xrReleaseSwapchainImage(scene->CylinderSwapChain.Handle, &releaseInfo));
    }

    // Simple checkerboard pattern.
    {
        static const int QUAD_WIDTH = 256;
        static const int QUAD_HEIGHT = 256;

        XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapChainCreateInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
        swapChainCreateInfo.usageFlags =
                XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainCreateInfo.format = GL_SRGB8_ALPHA8;
        swapChainCreateInfo.sampleCount = 1;
        swapChainCreateInfo.width = QUAD_WIDTH;
        swapChainCreateInfo.height = QUAD_HEIGHT;
        swapChainCreateInfo.faceCount = 1;
        swapChainCreateInfo.arraySize = 1;
        swapChainCreateInfo.mipCount = 1;

        scene->QuadSwapChain.Width = swapChainCreateInfo.width;
        scene->QuadSwapChain.Height = swapChainCreateInfo.height;

        // Create the swapchain.
        OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &scene->QuadSwapChain.Handle));
        // Get the number of swapchain images.
        uint32_t length;
        OXR(xrEnumerateSwapchainImages(scene->QuadSwapChain.Handle, 0, &length, NULL));
        scene->QuadSwapChainImage =
                (XrSwapchainImageOpenGLESKHR*)malloc(length * sizeof(XrSwapchainImageOpenGLESKHR));
        for (uint32_t i = 0; i < length; i++) {
            scene->QuadSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
            scene->QuadSwapChainImage[i].next = NULL;
        }
        OXR(xrEnumerateSwapchainImages(
                scene->QuadSwapChain.Handle,
                length,
                &length,
                (XrSwapchainImageBaseHeader*)scene->QuadSwapChainImage));

        uint32_t* texData = (uint32_t*)malloc(QUAD_WIDTH * QUAD_HEIGHT * sizeof(uint32_t));

        for (int y = 0; y < QUAD_HEIGHT; y++) {
            for (int x = 0; x < QUAD_WIDTH; x++) {
                uint32_t gray = ((x ^ (x >> 1)) ^ (y ^ (y >> 1))) & 0xff;
                uint32_t r = gray + 255.0f * ((1.0f - (gray / 255.0f)) * (x / (QUAD_WIDTH - 1.0f)));
                uint32_t g = gray + 255.0f * ((1.0f - (gray / 255.0f)) * (y / (QUAD_HEIGHT - 1.0f)));
                uint32_t rgba = (0xffu << 24) | (gray << 16) | (g << 8) | r;
                texData[y * QUAD_WIDTH + x] = rgba;
            }
        }

        const int texId = scene->QuadSwapChainImage[0].image;

        glBindTexture(GL_TEXTURE_2D, texId);
        glTexSubImage2D(
                GL_TEXTURE_2D, 0, 0, 0, QUAD_WIDTH, QUAD_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, texData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindTexture(GL_TEXTURE_2D, 0);

        free(texData);

        uint32_t index = 0;
        XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        OXR(xrAcquireSwapchainImage(scene->QuadSwapChain.Handle, &acquireInfo, &index));

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        OXR(xrWaitSwapchainImage(scene->QuadSwapChain.Handle, &waitInfo));

        XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        OXR(xrReleaseSwapchainImage(scene->QuadSwapChain.Handle, &releaseInfo));
    }

    scene->CreatedScene = true;
}

static void ovrScene_Destroy(ovrScene* scene) {
    ovrScene_DestroyVAOs(scene);

    ovrProgram_Destroy(&scene->Program);
    ovrGeometry_Destroy(&scene->GroundPlane);
    ovrGeometry_Destroy(&scene->Box);

    // Cubemap is optional
    if (scene->CubeMapSwapChain.Handle != XR_NULL_HANDLE) {
        OXR(xrDestroySwapchain(scene->CubeMapSwapChain.Handle));
    }
    if (scene->CubeMapSwapChainImage != NULL) {
        free(scene->CubeMapSwapChainImage);
    }
    OXR(xrDestroySwapchain(scene->EquirectSwapChain.Handle));
    free(scene->EquirectSwapChainImage);
    OXR(xrDestroySwapchain(scene->CylinderSwapChain.Handle));
    free(scene->CylinderSwapChainImage);
    OXR(xrDestroySwapchain(scene->QuadSwapChain.Handle));
    free(scene->QuadSwapChainImage);

    scene->CreatedScene = false;
}

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct {
    ovrFramebuffer FrameBuffer[ovrMaxNumEyes];
} ovrRenderer;

static void ovrRenderer_Clear(ovrRenderer* renderer) {
    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
    }
}

static void ovrRenderer_Create(
        XrSession session,
        ovrRenderer* renderer,
        int suggestedEyeTextureWidth,
        int suggestedEyeTextureHeight) {
    // Create the frame buffers.
    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        ovrFramebuffer_Create(
                session,
                &renderer->FrameBuffer[eye],
                GL_SRGB8_ALPHA8,
                suggestedEyeTextureWidth,
                suggestedEyeTextureHeight,
                NUM_MULTI_SAMPLES);
    }
}

static void ovrRenderer_Destroy(ovrRenderer* renderer) {
    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
    }
}

typedef struct {
    XrMatrix4x4f ViewMatrix[ovrMaxNumEyes];
    XrMatrix4x4f ProjectionMatrix[ovrMaxNumEyes];
} ovrSceneMatrices;

static void ovrRenderer_RenderFrame(
        ovrRenderer* renderer,
        const ovrScene* scene,
        const ovrSceneMatrices* sceneMatrices) {
    // Let the background layer show through if one is present.
    float clearAlpha = 1.0f;
    if (scene->BackGroundType != BACKGROUND_NONE) {
        clearAlpha = 0.0f;
    }

    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];

        ovrFramebuffer_Acquire(frameBuffer);

        // Set the current framebuffer.
        ovrFramebuffer_SetCurrent(frameBuffer);

        GL(glUseProgram(scene->Program.Program));

        XrMatrix4x4f modelMatrix;
        XrMatrix4x4f_CreateIdentity(&modelMatrix);
        glUniformMatrix4fv(
                scene->Program.UniformLocation[MODEL_MATRIX], 1, GL_FALSE, &modelMatrix.m[0]);
        XrMatrix4x4f viewProjMatrix;
        XrMatrix4x4f_Multiply(
                &viewProjMatrix,
                &sceneMatrices->ProjectionMatrix[eye],
                &sceneMatrices->ViewMatrix[eye]);
        glUniformMatrix4fv(
                scene->Program.UniformLocation[VIEW_PROJ_MATRIX], 1, GL_FALSE, &viewProjMatrix.m[0]);

        GL(glEnable(GL_SCISSOR_TEST));
        GL(glDepthMask(GL_TRUE));
        GL(glEnable(GL_DEPTH_TEST));
        GL(glDepthFunc(GL_LEQUAL));
        GL(glDisable(GL_CULL_FACE));
        // GL( glCullFace( GL_BACK ) );
        GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
        GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));
        GL(glClearColor(0.0f, 0.0f, 0.0f, clearAlpha));
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        GL(glBindVertexArray(scene->GroundPlane.VertexArrayObject));
        GL(glDrawElements(GL_TRIANGLES, scene->GroundPlane.IndexCount, GL_UNSIGNED_SHORT, NULL));

        for (int i = 0; i < 4; i++) {
            if (scene->TrackedController[i].Active == false) {
                continue;
            }
            XrMatrix4x4f pose;
            XrMatrix4x4f_CreateFromRigidTransform(&pose, &scene->TrackedController[i].Pose);
            XrMatrix4x4f scale;
            if (i & 1) {
                XrMatrix4x4f_CreateScale(&scale, 0.03f, 0.03f, 0.03f);
            } else {
                XrMatrix4x4f_CreateScale(&scale, 0.02f, 0.02f, 0.06f);
            }
            XrMatrix4x4f model;
            XrMatrix4x4f_Multiply(&model, &pose, &scale);
            glUniformMatrix4fv(
                    scene->Program.UniformLocation[MODEL_MATRIX], 1, GL_FALSE, &model.m[0]);
            GL(glBindVertexArray(scene->Box.VertexArrayObject));
            GL(glDrawElements(GL_TRIANGLES, scene->Box.IndexCount, GL_UNSIGNED_SHORT, NULL));
        }
        glUniformMatrix4fv(
                scene->Program.UniformLocation[MODEL_MATRIX], 1, GL_FALSE, &modelMatrix.m[0]);

        GL(glBindVertexArray(0));
        GL(glUseProgram(0));

        ovrFramebuffer_Resolve(frameBuffer);

        ovrFramebuffer_Release(frameBuffer);
    }

    ovrFramebuffer_SetNone();
}

static void ovrRenderer_SetFoveation(
        XrInstance* instance,
        XrSession* session,
        ovrRenderer* renderer,
        XrFoveationLevelFB level,
        float verticalOffset,
        XrFoveationDynamicFB dynamic) {
    PFN_xrCreateFoveationProfileFB pfnCreateFoveationProfileFB;
    OXR(xrGetInstanceProcAddr(
            *instance,
            "xrCreateFoveationProfileFB",
            (PFN_xrVoidFunction*)(&pfnCreateFoveationProfileFB)));

    PFN_xrDestroyFoveationProfileFB pfnDestroyFoveationProfileFB;
    OXR(xrGetInstanceProcAddr(
            *instance,
            "xrDestroyFoveationProfileFB",
            (PFN_xrVoidFunction*)(&pfnDestroyFoveationProfileFB)));

    PFN_xrUpdateSwapchainFB pfnUpdateSwapchainFB;
    OXR(xrGetInstanceProcAddr(
            *instance, "xrUpdateSwapchainFB", (PFN_xrVoidFunction*)(&pfnUpdateSwapchainFB)));

    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        XrFoveationLevelProfileCreateInfoFB levelProfileCreateInfo = {XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB};
        levelProfileCreateInfo.level = level;
        levelProfileCreateInfo.verticalOffset = verticalOffset;
        levelProfileCreateInfo.dynamic = dynamic;

        XrFoveationProfileCreateInfoFB profileCreateInfo = {XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB};
        profileCreateInfo.next = &levelProfileCreateInfo;

        XrFoveationProfileFB foveationProfile;

        pfnCreateFoveationProfileFB(*session, &profileCreateInfo, &foveationProfile);

        XrSwapchainStateFoveationFB foveationUpdateState = {XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB};
        foveationUpdateState.profile = foveationProfile;

        pfnUpdateSwapchainFB(
                renderer->FrameBuffer[eye].ColorSwapChain.Handle,
                (XrSwapchainStateBaseHeaderFB*)(&foveationUpdateState));

        pfnDestroyFoveationProfileFB(foveationProfile);
    }
}

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct {
    ovrEgl Egl;
    bool Resumed;
    bool Focused;

    XrInstance Instance;
    XrSession Session;
    XrViewConfigurationProperties ViewportConfig;
    XrViewConfigurationView ViewConfigurationView[ovrMaxNumEyes];
    XrSystemId SystemId;
    XrSpace HeadSpace;
    XrSpace LocalSpace;
    XrSpace StageSpace;
    XrSpace FakeStageSpace;
    XrSpace CurrentSpace;
    bool SessionActive;

    ovrScene Scene;

    float* SupportedDisplayRefreshRates;
    uint32_t RequestedDisplayRefreshRateIndex;
    uint32_t NumSupportedDisplayRefreshRates;
    PFN_xrGetDisplayRefreshRateFB pfnGetDisplayRefreshRate;
    PFN_xrRequestDisplayRefreshRateFB pfnRequestDisplayRefreshRate;

    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    // These threads will be marked as performance threads.
    int MainThreadTid;
    int RenderThreadTid;
    ovrCompositorLayer_Union Layers[ovrMaxLayerCount];
    int LayerCount;

    bool TouchPadDownLastFrame;
    ovrRenderer Renderer;
} ovrApp;

static void ovrApp_Clear(ovrApp* app) {
    app->Resumed = false;
    app->Focused = false;
    app->Instance = XR_NULL_HANDLE;
    app->Session = XR_NULL_HANDLE;
    memset(&app->ViewportConfig, 0, sizeof(XrViewConfigurationProperties));
    memset(&app->ViewConfigurationView, 0, ovrMaxNumEyes * sizeof(XrViewConfigurationView));
    app->SystemId = XR_NULL_SYSTEM_ID;
    app->HeadSpace = XR_NULL_HANDLE;
    app->LocalSpace = XR_NULL_HANDLE;
    app->StageSpace = XR_NULL_HANDLE;
    app->FakeStageSpace = XR_NULL_HANDLE;
    app->CurrentSpace = XR_NULL_HANDLE;
    app->SessionActive = false;
    app->SupportedDisplayRefreshRates = NULL;
    app->RequestedDisplayRefreshRateIndex = 0;
    app->NumSupportedDisplayRefreshRates = 0;
    app->pfnGetDisplayRefreshRate = NULL;
    app->pfnRequestDisplayRefreshRate = NULL;
    app->SwapInterval = 1;
    memset(app->Layers, 0, sizeof(ovrCompositorLayer_Union) * ovrMaxLayerCount);
    app->LayerCount = 0;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;
    app->TouchPadDownLastFrame = false;

    ovrEgl_Clear(&app->Egl);
    ovrScene_Clear(&app->Scene);
    ovrRenderer_Clear(&app->Renderer);
}

static void ovrApp_Destroy(ovrApp* app) {
    if (app->SupportedDisplayRefreshRates != NULL) {
        free(app->SupportedDisplayRefreshRates);
    }

    ovrApp_Clear(app);
}

static void ovrApp_HandleSessionStateChanges(ovrApp* app, XrSessionState state) {
    if (state == XR_SESSION_STATE_READY) {
        assert(app->Resumed);
        assert(app->SessionActive == false);

        XrSessionBeginInfo sessionBeginInfo = {XR_TYPE_SESSION_BEGIN_INFO};
        sessionBeginInfo.primaryViewConfigurationType = app->ViewportConfig.viewConfigurationType;

        XrResult result;
        OXR(result = xrBeginSession(app->Session, &sessionBeginInfo));

        app->SessionActive = (result == XR_SUCCESS);

        // Set session state once we have entered VR mode and have a valid session object.
        if (app->SessionActive) {
            XrPerfSettingsLevelEXT cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
            switch (app->CpuLevel) {
                case 0:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
                    break;
                case 1:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
                    break;
                case 2:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
                    break;
                case 3:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
                    break;
                default:
                    ALOGE("Invalid CPU level %d", app->CpuLevel);
                    break;
            }

            XrPerfSettingsLevelEXT gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
            switch (app->GpuLevel) {
                case 0:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
                    break;
                case 1:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
                    break;
                case 2:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
                    break;
                case 3:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
                    break;
                default:
                    ALOGE("Invalid GPU level %d", app->GpuLevel);
                    break;
            }

            PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
            OXR(xrGetInstanceProcAddr(
                    app->Instance,
                    "xrPerfSettingsSetPerformanceLevelEXT",
                    (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

            OXR(pfnPerfSettingsSetPerformanceLevelEXT(
                    app->Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, cpuPerfLevel));
            OXR(pfnPerfSettingsSetPerformanceLevelEXT(
                    app->Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, gpuPerfLevel));

            PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
            OXR(xrGetInstanceProcAddr(
                    app->Instance,
                    "xrSetAndroidApplicationThreadKHR",
                    (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

            OXR(pfnSetAndroidApplicationThreadKHR(
                    app->Session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, app->MainThreadTid));
            OXR(pfnSetAndroidApplicationThreadKHR(
                    app->Session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, app->RenderThreadTid));
        }
    } else if (state == XR_SESSION_STATE_STOPPING) {
        assert(app->Resumed == false);
        assert(app->SessionActive);

        OXR(xrEndSession(app->Session));
        app->SessionActive = false;
    }
}

static void ovrApp_HandleXrEvents(ovrApp* app) {
    XrEventDataBuffer eventDataBuffer = {XR_TYPE_EVENT_DATA_BUFFER};

    // Poll for events
    for (;;) {
        XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
        baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
        baseEventHeader->next = NULL;
        XrResult r;
        OXR(r = xrPollEvent(app->Instance, &eventDataBuffer));
        if (r != XR_SUCCESS) {
            break;
        }

        switch (baseEventHeader->type) {
            case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
                break;
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                const XrEventDataInstanceLossPending* instance_loss_pending_event =
                        (XrEventDataInstanceLossPending*)(baseEventHeader);
                ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event: time %f",
                        FromXrTime(instance_loss_pending_event->lossTime));
            } break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
                break;
            case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
                const XrEventDataPerfSettingsEXT* perf_settings_event =
                        (XrEventDataPerfSettingsEXT*)(baseEventHeader);
                ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type %d subdomain %d : level %d -> level %d",
                        perf_settings_event->type,
                        perf_settings_event->subDomain,
                        perf_settings_event->fromLevel,
                        perf_settings_event->toLevel);
            } break;
            case XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB: {
                const XrEventDataDisplayRefreshRateChangedFB* refresh_rate_changed_event =
                        (XrEventDataDisplayRefreshRateChangedFB*)(baseEventHeader);
                ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB event: fromRate %f -> toRate %f",
                        refresh_rate_changed_event->fromDisplayRefreshRate,
                        refresh_rate_changed_event->toDisplayRefreshRate);
            } break;
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                XrEventDataReferenceSpaceChangePending* ref_space_change_event =
                        (XrEventDataReferenceSpaceChangePending*)(baseEventHeader);
                ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING event: changed space: %d for session %p at time %f",
                        ref_space_change_event->referenceSpaceType,
                        (void*)ref_space_change_event->session,
                        FromXrTime(ref_space_change_event->changeTime));
            } break;
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                const XrEventDataSessionStateChanged* session_state_changed_event =
                        (XrEventDataSessionStateChanged*)(baseEventHeader);
                ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: %d for session %p at time %f",
                        session_state_changed_event->state,
                        (void*)session_state_changed_event->session,
                        FromXrTime(session_state_changed_event->time));

                switch (session_state_changed_event->state) {
                    case XR_SESSION_STATE_FOCUSED:
                        app->Focused = true;
                        break;
                    case XR_SESSION_STATE_VISIBLE:
                        app->Focused = false;
                        break;
                    case XR_SESSION_STATE_READY:
                    case XR_SESSION_STATE_STOPPING:
                        ovrApp_HandleSessionStateChanges(app, session_state_changed_event->state);
                        break;
                    default:
                        break;
                }
            } break;
            default:
                ALOGV("xrPollEvent: Unknown event");
                break;
        }
    }
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    ovrApp* appState = (ovrApp*)app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            ovrApp_Clear(appState);
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            break;
        }
    }
}

void UpdateStageBounds(ovrApp* pappState) {
    XrExtent2Df stageBounds = {0};

    XrResult result;
    OXR(result = xrGetReferenceSpaceBoundsRect(
            pappState->Session, XR_REFERENCE_SPACE_TYPE_STAGE, &stageBounds));
    if (result != XR_SUCCESS) {
        ALOGV("Stage bounds query failed: using small defaults");
        stageBounds.width = 1.0f;
        stageBounds.height = 1.0f;

        pappState->CurrentSpace = pappState->FakeStageSpace;
    }

    ALOGV("Stage bounds: width = %f, depth %f", stageBounds.width, stageBounds.height);

    const float halfWidth = stageBounds.width * 0.5f;
    const float halfDepth = stageBounds.height * 0.5f;

    ovrGeometry_Destroy(&pappState->Scene.GroundPlane);
    ovrGeometry_DestroyVAO(&pappState->Scene.GroundPlane);
    ovrGeometry_CreateStagePlane(
            &pappState->Scene.GroundPlane, -halfWidth, -halfDepth, halfWidth, halfDepth);
    ovrGeometry_CreateVAO(&pappState->Scene.GroundPlane);
}

ovrApp appState;

XrInstance ovrApp_GetInstance() {
    return appState.Instance;
}

static XrActionSet CreateActionSet(int priority, const char* name, const char* localizedName) {
    XrActionSetCreateInfo asci = {XR_TYPE_ACTION_SET_CREATE_INFO};
    asci.priority = priority;
    strcpy(asci.actionSetName, name);
    strcpy(asci.localizedActionSetName, localizedName);
    XrActionSet actionSet = XR_NULL_HANDLE;
    OXR(xrCreateActionSet(appState.Instance, &asci, &actionSet));
    return actionSet;
}

static XrAction CreateAction(
        XrActionSet actionSet,
        XrActionType type,
        const char* actionName,
        const char* localizedName,
        int countSubactionPaths,
        XrPath* subactionPaths) {
    ALOGV("CreateAction %s, %" PRIi32, actionName, countSubactionPaths);

    XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
    aci.actionType = type;
    if (countSubactionPaths > 0) {
        aci.countSubactionPaths = countSubactionPaths;
        aci.subactionPaths = subactionPaths;
    }
    strcpy(aci.actionName, actionName);
    strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
    XrAction action = XR_NULL_HANDLE;
    OXR(xrCreateAction(actionSet, &aci, &action));
    return action;
}

static XrActionSuggestedBinding ActionSuggestedBinding(XrAction action, const char* bindingString) {
    XrActionSuggestedBinding asb;
    asb.action = action;
    XrPath bindingPath;
    OXR(xrStringToPath(appState.Instance, bindingString, &bindingPath));
    asb.binding = bindingPath;
    return asb;
}

static XrSpace CreateActionSpace(XrAction poseAction, XrPath subactionPath) {
    XrActionSpaceCreateInfo asci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
    asci.action = poseAction;
    asci.poseInActionSpace.orientation.w = 1.0f;
    asci.subactionPath = subactionPath;
    XrSpace actionSpace = XR_NULL_HANDLE;
    OXR(xrCreateActionSpace(appState.Session, &asci, &actionSpace));
    return actionSpace;
}

static XrActionStateBoolean GetActionStateBoolean(XrAction action) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};

    OXR(xrGetActionStateBoolean(appState.Session, &getInfo, &state));
    return state;
}

static XrActionStateFloat GetActionStateFloat(XrAction action) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};

    OXR(xrGetActionStateFloat(appState.Session, &getInfo, &state));
    return state;
}

static XrActionStateVector2f GetActionStateVector2(XrAction action) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};

    OXR(xrGetActionStateVector2f(appState.Session, &getInfo, &state));
    return state;
}

static bool ActionPoseIsActive(XrAction action, XrPath subactionPath) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    getInfo.subactionPath = subactionPath;

    XrActionStatePose state = {XR_TYPE_ACTION_STATE_POSE};
    OXR(xrGetActionStatePose(appState.Session, &getInfo, &state));
    return state.isActive != XR_FALSE;
}

typedef struct {
    XrSpaceLocation loc;
    XrSpaceVelocity vel;
} LocVel;

static LocVel GetSpaceLocVel(XrSpace space, XrTime time) {
    LocVel lv = {{XR_TYPE_SPACE_LOCATION}, {XR_TYPE_SPACE_VELOCITY}};
    lv.loc.next = &lv.vel;
    OXR(xrLocateSpace(space, appState.CurrentSpace, time, &lv.loc));
    lv.loc.next = NULL; // pointer no longer valid or necessary
    return lv;
}