//
// Created by lowej2 on 12/18/2024.
//
#include <openxr/openxr.h>
//#include "../raylib.h"

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
#include <meta_openxr_preview/xr_linear.h>

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

void InitCube(ovrGeometry* cube, float color[3]) {
    // Cube vertices: Position (X, Y, Z)
    float vertices[] = {
            // Front face
            -0.5f, -0.5f,  0.5f,  color[0], color[1], color[2], // Add color to position
            0.5f, -0.5f,  0.5f,  color[0], color[1], color[2],
            0.5f,  0.5f,  0.5f,  color[0], color[1], color[2],
            -0.5f,  0.5f,  0.5f,  color[0], color[1], color[2],
            // Back face
            -0.5f, -0.5f, -0.5f,  color[0], color[1], color[2],
            0.5f, -0.5f, -0.5f,  color[0], color[1], color[2],
            0.5f,  0.5f, -0.5f,  color[0], color[1], color[2],
            -0.5f,  0.5f, -0.5f,  color[0], color[1], color[2]
    };

    // Index buffer (Two triangles per face, 6 faces)
    unsigned short indices[] = {
            0, 1, 2,  2, 3, 0,  // Front
            1, 5, 6,  6, 2, 1,  // Right
            5, 4, 7,  7, 6, 5,  // Back
            4, 0, 3,  3, 7, 4,  // Left
            3, 2, 6,  6, 7, 3,  // Top
            4, 5, 1,  1, 0, 4   // Bottom
    };

    // Store the number of indices
    cube->IndexCount = sizeof(indices) / sizeof(indices[0]);

    // Generate and bind VAO
    glGenVertexArrays(1, &cube->VertexArrayObject);
    glBindVertexArray(cube->VertexArrayObject);

    // Generate and bind VBO
    glGenBuffers(1, &cube->VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, cube->VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Generate and bind IBO (Index Buffer Object)
    glGenBuffers(1, &cube->IndexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube->IndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Define vertex attributes (position + color)
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Color attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    // Unbind VAO
    glBindVertexArray(0);
}

typedef struct{
    ovrGeometry objType;
    XrMatrix4x4f model;
} renderingObject;

typedef struct {
    renderingObject *array;    // Pointer to the array
    size_t size;   // Number of elements currently in the array
    size_t capacity;  // Total allocated size of the array
} ArrayList;

void initArrayList(ArrayList* list, size_t initialCapacity) {
    list->array = (renderingObject*)malloc(initialCapacity * sizeof(renderingObject));  // Allocate memory
    list->size = 0;  // No elements initially
    list->capacity = initialCapacity;  // Set initial capacity
}

// Function to add an element to the array list
void addToArrayList(ArrayList* list, renderingObject value) {
    // Resize the array if it's full
    if (list->size == list->capacity) {
        list->capacity *= 2;  // Double the capacity
        list->array = (renderingObject*)realloc(list->array, list->capacity * sizeof(renderingObject));  // Reallocate memory
    }

    list->array[list->size] = value;  // Add the new value
    list->size++;  // Increment the size of the list
}

// Function to remove the last element from the array list
void removeFromArrayList(ArrayList* list) {
    if (list->size > 0) {
        list->size--;  // Decrease the size
    }
}

// Function to get the element at a specific index
renderingObject* getFromArrayList(ArrayList* list, size_t index) {
    if (index < list->size) {
        return &list->array[index];
    } else {
        // Return some error value if the index is out of bounds
        return NULL;
    }
}

// Function to free the allocated memory
void freeArrayList(ArrayList* list) {
    free(list->array);  // Free the array memory
    list->array = NULL;  // Set the pointer to NULL
    list->size = 0;  // Reset size
    list->capacity = 0;  // Reset capacity
}

ArrayList* allObjects;


static void ovrRenderer_RenderFrame(
        ovrRenderer* renderer,
        const ovrScene* scene,
        const ovrSceneMatrices* sceneMatrices) {
    // Let the background layer show through if one is present.
    float clearAlpha = 1.0f;
    if (scene->BackGroundType != BACKGROUND_NONE) {
        clearAlpha = 0.0f;
    }
    allObjects = malloc(sizeof(ArrayList));
    initArrayList(allObjects, 1);

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
        for (int i = 0; i < 20; i++) {
            float color[3];
            color[0] = 1.0;
            color[1] = 1.0;
            color[2] = .02 * i;
            ovrGeometry sceneCube;
            InitCube(&sceneCube, color);
            XrMatrix4x4f pose;
            XrMatrix4x4f_CreateTranslation(&pose, i * 0.2f, 0.0f, -1.0f); // Spread them out
            XrMatrix4x4f scale;
            XrMatrix4x4f_CreateScale(&scale, 0.1f, 0.1f, 0.1f); // Small cubes
            XrMatrix4x4f model;
            XrMatrix4x4f_Multiply(&model, &pose, &scale);
            renderingObject obj;
            obj.model = model;
            obj.objType = sceneCube;
            addToArrayList(allObjects, obj);
        }
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
        for (int i = 0; i < 20; i++) { // Render 5 cubes
            glUniformMatrix4fv(
                    scene->Program.UniformLocation[MODEL_MATRIX], 1, GL_FALSE, &getFromArrayList(allObjects, i)->model.m[0]);

            GL(glBindVertexArray(getFromArrayList(allObjects, i)->objType.VertexArrayObject));
            GL(glDrawElements(GL_TRIANGLES, getFromArrayList(allObjects, i)->objType.IndexCount, GL_UNSIGNED_SHORT, NULL));
        }
        glUniformMatrix4fv(
                scene->Program.UniformLocation[MODEL_MATRIX], 1, GL_FALSE, &modelMatrix.m[0]);

        GL(glBindVertexArray(0));
        GL(glUseProgram(0));
        ovrFramebuffer_Resolve(frameBuffer);

        ovrFramebuffer_Release(frameBuffer);
    }
    freeArrayList(allObjects);
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

bool vrInitialized = false;


bool AppShouldClose(struct android_app *app) {
    return app->destroyRequested != 0;
}

void InitializeXRLoader(struct android_app *app) {
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    xrGetInstanceProcAddr(
            XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *) &xrInitializeLoaderKHR);
    if (xrInitializeLoaderKHR != NULL) {
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {
                XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
        loaderInitializeInfoAndroid.applicationContext = app->activity->clazz;
        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR *) &loaderInitializeInfoAndroid);
    }
}

void LogAvailableLayers() {
    // Log available layers.
    {
        uint32_t numLayers = 0;
        OXR(xrEnumerateApiLayerProperties(0, &numLayers, NULL));

        XrApiLayerProperties *layerProperties =
                (XrApiLayerProperties *) malloc(numLayers * sizeof(XrApiLayerProperties));

        for (uint32_t i = 0; i < numLayers; i++) {
            layerProperties[i].type = XR_TYPE_API_LAYER_PROPERTIES;
            layerProperties[i].next = NULL;
        }

        OXR(xrEnumerateApiLayerProperties(numLayers, &numLayers, layerProperties));

        for (uint32_t i = 0; i < numLayers; i++) {
            ALOGV("Found layer %s", layerProperties[i].layerName);
        }

        free(layerProperties);
    }
};

void
CheckRequiredExtensions(uint32_t numRequiredExtensions, const char *const *requiredExtensionNames) {

    // Check the list of required extensions against what is supported by the runtime.
    {
        uint32_t numExtensions = 0;
        OXR(xrEnumerateInstanceExtensionProperties(
                NULL, 0, &numExtensions, NULL));
        ALOGV("xrEnumerateInstanceExtensionProperties found %u extension(s).", numExtensions);

        XrExtensionProperties *extensionProperties =
                (XrExtensionProperties *) malloc(numExtensions * sizeof(XrExtensionProperties));

        for (uint32_t i = 0; i < numExtensions; i++) {
            extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
            extensionProperties[i].next = NULL;
        }

        OXR(xrEnumerateInstanceExtensionProperties(
                NULL, numExtensions, &numExtensions, extensionProperties));
        for (uint32_t i = 0; i < numExtensions; i++) {
            ALOGV("Extension #%d = '%s'.", i, extensionProperties[i].extensionName);
        }

        for (uint32_t i = 0; i < numRequiredExtensions; i++) {
            bool found = false;
            for (uint32_t j = 0; j < numExtensions; j++) {
                if (!strcmp(requiredExtensionNames[i], extensionProperties[j].extensionName)) {
                    ALOGV("Found required extension %s", requiredExtensionNames[i]);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ALOGE("Failed to find required extension %s", requiredExtensionNames[i]);
                exit(1);
            }
        }

        free(extensionProperties);
    }
}

XrResult CreateXRResult(uint32_t numRequiredExtensions, const char *const *requiredExtensionNames) {
    // Create the OpenXR instance.
    XrApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    strcpy(appInfo.applicationName, "OpenXR_NativeActivity"); // NOTE: application name
    appInfo.applicationVersion = 0;
    strcpy(appInfo.engineName, "Oculus Mobile Sample");
    appInfo.engineVersion = 0;
    appInfo.apiVersion = XR_API_VERSION_1_0;

    XrInstanceCreateInfo instanceCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.createFlags = 0;
    instanceCreateInfo.applicationInfo = appInfo;
    instanceCreateInfo.enabledApiLayerCount = 0;
    instanceCreateInfo.enabledApiLayerNames = NULL;
    instanceCreateInfo.enabledExtensionCount = numRequiredExtensions;
    instanceCreateInfo.enabledExtensionNames = requiredExtensionNames;

    XrResult initResult;
    OXR(initResult = xrCreateInstance(&instanceCreateInfo, &appState.Instance));
    if (initResult != XR_SUCCESS) {
        ALOGE("Failed to create XR instance: %d.", initResult);
        exit(1);
    }

    XrInstanceProperties instanceInfo = {XR_TYPE_INSTANCE_PROPERTIES};
    OXR(xrGetInstanceProperties(appState.Instance, &instanceInfo));
    ALOGV(
            "Runtime %s: Version : %u.%u.%u",
            instanceInfo.runtimeName,
            XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
            XR_VERSION_MINOR(instanceInfo.runtimeVersion),
            XR_VERSION_PATCH(instanceInfo.runtimeVersion));
    return initResult;
}

XrSystemId CreateXRSystemID(XrResult initResult) {
    XrSystemGetInfo systemGetInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId;
    OXR(initResult = xrGetSystem(appState.Instance, &systemGetInfo, &systemId));
    if (initResult != XR_SUCCESS) {
        if (initResult == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
            ALOGE("Failed to get system; the specified form factor is not available. Is your headset connected?");
        } else {
            ALOGE("xrGetSystem failed, error %d", initResult);
        }
        exit(1);
    }

    XrSystemColorSpacePropertiesFB colorSpacePropertiesFB = {
            XR_TYPE_SYSTEM_COLOR_SPACE_PROPERTIES_FB};

    XrSystemProperties systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};
    systemProperties.next = &colorSpacePropertiesFB;
    OXR(xrGetSystemProperties(appState.Instance, systemId, &systemProperties));

    ALOGV(
            "System Properties: Name=%s VendorId=%x",
            systemProperties.systemName,
            systemProperties.vendorId);
    ALOGV(
            "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
            systemProperties.graphicsProperties.maxSwapchainImageWidth,
            systemProperties.graphicsProperties.maxSwapchainImageHeight,
            systemProperties.graphicsProperties.maxLayerCount);
    ALOGV(
            "System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
            systemProperties.trackingProperties.orientationTracking ? "True" : "False",
            systemProperties.trackingProperties.positionTracking ? "True" : "False");

    ALOGV("System Color Space Properties: colorspace=%d", colorSpacePropertiesFB.colorSpace);

    assert(ovrMaxLayerCount <= systemProperties.graphicsProperties.maxLayerCount);
    return systemId;
}

void InitializeGraphics(XrSystemId systemId, XrResult initResult) {
    // Get the graphics requirements.
    PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
    OXR(xrGetInstanceProcAddr(
            appState.Instance,
            "xrGetOpenGLESGraphicsRequirementsKHR",
            (PFN_xrVoidFunction *) (&pfnGetOpenGLESGraphicsRequirementsKHR)));

    XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {
            XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    OXR(pfnGetOpenGLESGraphicsRequirementsKHR(appState.Instance, systemId, &graphicsRequirements));

    // Create the EGL Context
    ovrEgl_CreateContext(&appState.Egl, NULL);

    // Check the graphics requirements.
    int eglMajor = 0;
    int eglMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &eglMajor);
    glGetIntegerv(GL_MINOR_VERSION, &eglMinor);
    const XrVersion eglVersion = XR_MAKE_VERSION(eglMajor, eglMinor, 0);
    if (eglVersion < graphicsRequirements.minApiVersionSupported ||
        eglVersion > graphicsRequirements.maxApiVersionSupported) {
        ALOGE("GLES version %d.%d not supported", eglMajor, eglMinor);
        exit(0);
    }

    appState.CpuLevel = CPU_LEVEL;
    appState.GpuLevel = GPU_LEVEL;
    appState.MainThreadTid = gettid();

    appState.SystemId = systemId;


    // Create the OpenXR Session.
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingAndroidGLES = {
            XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBindingAndroidGLES.display = appState.Egl.Display;
    graphicsBindingAndroidGLES.config = appState.Egl.Config;
    graphicsBindingAndroidGLES.context = appState.Egl.Context;

    XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &graphicsBindingAndroidGLES;
    sessionCreateInfo.createFlags = 0;
    sessionCreateInfo.systemId = appState.SystemId;

    OXR(initResult = xrCreateSession(appState.Instance, &sessionCreateInfo, &appState.Session));
    if (initResult != XR_SUCCESS) {
        ALOGE("Failed to create XR session: %d.", initResult);
        exit(1);
    }

    // App only supports the primary stereo view config.
    const XrViewConfigurationType supportedViewConfigType =
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    // Enumerate the viewport configurations.
    uint32_t viewportConfigTypeCount = 0;
    OXR(xrEnumerateViewConfigurations(
            appState.Instance, appState.SystemId, 0, &viewportConfigTypeCount, NULL));

    XrViewConfigurationType *viewportConfigurationTypes =
            (XrViewConfigurationType *) malloc(
                    viewportConfigTypeCount * sizeof(XrViewConfigurationType));

    OXR(xrEnumerateViewConfigurations(
            appState.Instance,
            appState.SystemId,
            viewportConfigTypeCount,
            &viewportConfigTypeCount,
            viewportConfigurationTypes));

    ALOGV("Available Viewport Configuration Types: %d", viewportConfigTypeCount);

    for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
        const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

        ALOGV(
                "Viewport configuration type %d : %s",
                viewportConfigType,
                viewportConfigType == supportedViewConfigType ? "Selected" : "");

        XrViewConfigurationProperties viewportConfig = {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
        OXR(xrGetViewConfigurationProperties(
                appState.Instance, appState.SystemId, viewportConfigType, &viewportConfig));
        ALOGV(
                "FovMutable=%s ConfigurationType %d",
                viewportConfig.fovMutable ? "true" : "false",
                viewportConfig.viewConfigurationType);

        uint32_t viewCount;
        OXR(xrEnumerateViewConfigurationViews(
                appState.Instance, appState.SystemId, viewportConfigType, 0, &viewCount, NULL));

        if (viewCount > 0) {
            XrViewConfigurationView *elements =
                    (XrViewConfigurationView *) malloc(viewCount * sizeof(XrViewConfigurationView));

            for (uint32_t e = 0; e < viewCount; e++) {
                elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                elements[e].next = NULL;
            }

            OXR(xrEnumerateViewConfigurationViews(
                    appState.Instance,
                    appState.SystemId,
                    viewportConfigType,
                    viewCount,
                    &viewCount,
                    elements));

            // Log the view config info for each view type for debugging purposes.
            for (uint32_t e = 0; e < viewCount; e++) {
                const XrViewConfigurationView *element = &elements[e];

                ALOGV(
                        "Viewport [%d]: Recommended Width=%d Height=%d SampleCount=%d",
                        e,
                        element->recommendedImageRectWidth,
                        element->recommendedImageRectHeight,
                        element->recommendedSwapchainSampleCount);

                ALOGV(
                        "Viewport [%d]: Max Width=%d Height=%d SampleCount=%d",
                        e,
                        element->maxImageRectWidth,
                        element->maxImageRectHeight,
                        element->maxSwapchainSampleCount);
            }

            // Cache the view config properties for the selected config type.
            if (viewportConfigType == supportedViewConfigType) {
                assert(viewCount == ovrMaxNumEyes);
                for (uint32_t e = 0; e < viewCount; e++) {
                    appState.ViewConfigurationView[e] = elements[e];
                }
            }

            free(elements);
        } else {
            ALOGE("Empty viewport configuration type: %d", viewCount);
        }
    }

    free(viewportConfigurationTypes);

    // Get the viewport configuration info for the chosen viewport configuration type.
    appState.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;

    OXR(xrGetViewConfigurationProperties(
            appState.Instance, appState.SystemId, supportedViewConfigType,
            &appState.ViewportConfig));

    // Enumerate the supported color space options for the system.
    {
        PFN_xrEnumerateColorSpacesFB pfnxrEnumerateColorSpacesFB = NULL;
        OXR(xrGetInstanceProcAddr(
                appState.Instance,
                "xrEnumerateColorSpacesFB",
                (PFN_xrVoidFunction *) (&pfnxrEnumerateColorSpacesFB)));

        uint32_t colorSpaceCountOutput = 0;
        OXR(pfnxrEnumerateColorSpacesFB(appState.Session, 0, &colorSpaceCountOutput, NULL));

        XrColorSpaceFB *colorSpaces =
                (XrColorSpaceFB *) malloc(colorSpaceCountOutput * sizeof(XrColorSpaceFB));

        OXR(pfnxrEnumerateColorSpacesFB(
                appState.Session, colorSpaceCountOutput, &colorSpaceCountOutput, colorSpaces));
        ALOGV("Supported ColorSpaces:");

        for (uint32_t i = 0; i < colorSpaceCountOutput; i++) {
            ALOGV("%d:%d", i, colorSpaces[i]);
        }

        const XrColorSpaceFB requestColorSpace = XR_COLOR_SPACE_REC2020_FB;

        PFN_xrSetColorSpaceFB pfnxrSetColorSpaceFB = NULL;
        OXR(xrGetInstanceProcAddr(
                appState.Instance, "xrSetColorSpaceFB",
                (PFN_xrVoidFunction *) (&pfnxrSetColorSpaceFB)));

        OXR(pfnxrSetColorSpaceFB(appState.Session, requestColorSpace));

        free(colorSpaces);
    }

    // Get the supported display refresh rates for the system.
    {
        PFN_xrEnumerateDisplayRefreshRatesFB pfnxrEnumerateDisplayRefreshRatesFB = NULL;
        OXR(xrGetInstanceProcAddr(
                appState.Instance,
                "xrEnumerateDisplayRefreshRatesFB",
                (PFN_xrVoidFunction *) (&pfnxrEnumerateDisplayRefreshRatesFB)));

        OXR(pfnxrEnumerateDisplayRefreshRatesFB(
                appState.Session, 0, &appState.NumSupportedDisplayRefreshRates, NULL));

        appState.SupportedDisplayRefreshRates =
                (float *) malloc(appState.NumSupportedDisplayRefreshRates * sizeof(float));
        OXR(pfnxrEnumerateDisplayRefreshRatesFB(
                appState.Session,
                appState.NumSupportedDisplayRefreshRates,
                &appState.NumSupportedDisplayRefreshRates,
                appState.SupportedDisplayRefreshRates));
        ALOGV("Supported Refresh Rates:");
        for (uint32_t i = 0; i < appState.NumSupportedDisplayRefreshRates; i++) {
            ALOGV("%d:%f", i, appState.SupportedDisplayRefreshRates[i]);
        }

        OXR(xrGetInstanceProcAddr(
                appState.Instance,
                "xrGetDisplayRefreshRateFB",
                (PFN_xrVoidFunction *) (&appState.pfnGetDisplayRefreshRate)));

        float currentDisplayRefreshRate = 0.0f;
        OXR(appState.pfnGetDisplayRefreshRate(appState.Session, &currentDisplayRefreshRate));
        ALOGV("Current System Display Refresh Rate: %f", currentDisplayRefreshRate);

        OXR(xrGetInstanceProcAddr(
                appState.Instance,
                "xrRequestDisplayRefreshRateFB",
                (PFN_xrVoidFunction *) (&appState.pfnRequestDisplayRefreshRate)));

        // Test requesting the system default.
        OXR(appState.pfnRequestDisplayRefreshRate(appState.Session, 0.0f));
        ALOGV("Requesting system default display refresh rate");
    }
}

XrView *ConfigureSpaces() {
    bool stageSupported = false;

    uint32_t numOutputSpaces = 0;
    OXR(xrEnumerateReferenceSpaces(appState.Session, 0, &numOutputSpaces, NULL));

    XrReferenceSpaceType *referenceSpaces =
            (XrReferenceSpaceType *) malloc(numOutputSpaces * sizeof(XrReferenceSpaceType));

    OXR(xrEnumerateReferenceSpaces(
            appState.Session, numOutputSpaces, &numOutputSpaces, referenceSpaces));

    for (uint32_t i = 0; i < numOutputSpaces; i++) {
        if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
            stageSupported = true;
            break;
        }
    }

    free(referenceSpaces);

    // Create a space to the first path
    XrReferenceSpaceCreateInfo spaceCreateInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
    OXR(xrCreateReferenceSpace(appState.Session, &spaceCreateInfo, &appState.HeadSpace));

    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    OXR(xrCreateReferenceSpace(appState.Session, &spaceCreateInfo, &appState.LocalSpace));

    // Create a default stage space to use if SPACE_TYPE_STAGE is not
    // supported, or calls to xrGetReferenceSpaceBoundsRect fail.
    {
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
        OXR(xrCreateReferenceSpace(appState.Session, &spaceCreateInfo, &appState.FakeStageSpace));
        ALOGV("Created fake stage space from local space with offset");
        appState.CurrentSpace = appState.FakeStageSpace;
    }

    if (stageSupported) {
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
        OXR(xrCreateReferenceSpace(appState.Session, &spaceCreateInfo, &appState.StageSpace));
        ALOGV("Created stage space");
        appState.CurrentSpace = appState.StageSpace;
    }

    XrView *projections = (XrView *) (malloc(ovrMaxNumEyes * sizeof(XrView)));
    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        memset(&projections[eye], 0, sizeof(XrView));
        projections[eye].type = XR_TYPE_VIEW;
    }
    return projections;
}

void CreateActions() {

}

void CloseXRPlatform(
        XrView *projections) { //TODO: CHANGE THIS to ClosePlatform() once we figure out how to determine which recore_... to choose at runtime
    ovrRenderer_Destroy(&appState.Renderer);


    free(projections);

    ovrScene_Destroy(&appState.Scene);
    ovrEgl_DestroyContext(&appState.Egl);

    OXR(xrDestroySpace(appState.HeadSpace));
    OXR(xrDestroySpace(appState.LocalSpace));
    // StageSpace is optional.
    if (appState.StageSpace != XR_NULL_HANDLE) {
        OXR(xrDestroySpace(appState.StageSpace));
    }
    OXR(xrDestroySpace(appState.FakeStageSpace));
    appState.CurrentSpace = XR_NULL_HANDLE;
    OXR(xrDestroySession(appState.Session));
    OXR(xrDestroyInstance(appState.Instance));

    ovrApp_Destroy(&appState);
}

XrView *projections;

//XrActionSuggestedBinding bindings;
int currBinding;
XrSessionActionSetsAttachInfo attachInfo;

// Enumerate actions
XrPath actionPathsBuffer[16];
char stringBuffer[256];
XrAction actionsToEnumerate;

uint32_t countOutput;

XrSpace leftControllerAimSpace;
XrSpace rightControllerAimSpace;
XrSpace leftControllerGripSpace;
XrSpace rightControllerGripSpace;

bool stageBoundsDirty;

// App-specific input
float appQuadPositionX;
float appQuadPositionY;
float appCylPositionX;
float appCylPositionY;
XrAction LTrigger;
XrAction RTrigger;
XrAction XButton;
XrAction YButton;
XrAction AButton;
XrAction BButton;
XrAction LSqueeze;
XrAction RSqueeze;
XrAction LTriggerValue;
XrAction RTriggerValue;
XrAction LThumbstick;
XrAction RThumbstick;
XrAction LThumbstickClick;
XrAction RThumbstickClick;
XrAction vibrateLeftToggle;
XrAction vibrateRightToggle;
XrAction vibrateLeftFeedback;
XrAction vibrateRightFeedback;
XrAction aimPoseAction;
XrAction gripPoseAction;
XrPath rightHandPath;
XrPath leftHandPath;
bool useSimpleProfile = false;
XrActionSet runningActionSet;
XrAction toggleAction;
XrAction moveOnXAction;
XrAction moveOnYAction;
XrAction moveOnJoystickAction;
XrAction thumbstickClickAction;
XrActionSuggestedBinding bindings[22]; // large enough for all profiles

void InitActionSet() {
    // Actions
    runningActionSet =
            CreateActionSet(1, "running_action_set", "Action Set used on main loop");
    toggleAction =
            CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "toggle", "Toggle", 0,
                         NULL);
    moveOnXAction = CreateAction(
            runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "move_on_x", "Move on X", 0, NULL);
    moveOnYAction = CreateAction(
            runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "move_on_y", "Move on Y", 0, NULL);
    LThumbstick = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_VECTOR2F_INPUT,
            "move_on_left_joystick",
            "Move on Left Joystick",
            0,
            NULL);
    RThumbstick = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_VECTOR2F_INPUT,
            "move_on_right_joystick",
            "Move on Right Joystick",
            0,
            NULL);
    LThumbstickClick = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "left_thumbstick_click",
            "Left Thumbstick Click",
            0,
            NULL);
    RThumbstickClick = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "right_thumbstick_click",
            "Right Thumbstick Click",
            0,
            NULL);

    vibrateLeftToggle = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "vibrate_left",
            "Vibrate Left Controller",
            0,
            NULL);
    vibrateRightToggle = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "vibrate_right",
            "Vibrate Right Controller",
            0,
            NULL);
    vibrateLeftFeedback = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_VIBRATION_OUTPUT,
            "vibrate_left_feedback",
            "Vibrate Left Controller Feedback",
            0,
            NULL);
    vibrateRightFeedback = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_VIBRATION_OUTPUT,
            "vibrate_right_feedback",
            "Vibrate Right Controller Feedback",
            0,
            NULL);

    LTrigger = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "left_trigger",
            "Left Trigger",
            0,
            NULL);
    RTrigger = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "right_trigger",
            "Right Trigger",
            0,
            NULL);
    XButton = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "x_button",
            "X Button",
            0,
            NULL);
    YButton = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "y_button",
            "Y Button",
            0,
            NULL);
    AButton = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "a_button",
            "A Button",
            0,
            NULL);
    BButton = CreateAction(
            runningActionSet,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "b_button",
            "B Button",
            0,
            NULL);
    XrPath handSubactionPaths[2] = {leftHandPath, rightHandPath};

    aimPoseAction = CreateAction(
            runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", NULL, 2, handSubactionPaths);
    gripPoseAction = CreateAction(
            runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "grip_pose", NULL, 2, handSubactionPaths);
}

int InitVRController() {
    // copies filepaths to appropriate vars
    OXR(xrStringToPath(appState.Instance, "/user/hand/left", &leftHandPath));
    OXR(xrStringToPath(appState.Instance, "/user/hand/right", &rightHandPath));

    // inits const action set
    // TODO: re-evaluate whether this should be modifiable
    InitActionSet();

    XrPath interactionProfilePath = XR_NULL_PATH;
    XrPath interactionProfilePathTouch = XR_NULL_PATH;
    XrPath interactionProfilePathKHRSimple = XR_NULL_PATH;

    OXR(xrStringToPath(
            appState.Instance,
            "/interaction_profiles/oculus/touch_controller",
            &interactionProfilePathTouch));
    OXR(xrStringToPath(
            appState.Instance,
            "/interaction_profiles/khr/simple_controller",
            &interactionProfilePathKHRSimple));

    // Toggle this to force simple as a first choice, otherwise use it as a last resort
    if (useSimpleProfile) {
        ALOGV("xrSuggestInteractionProfileBindings found bindings for Khronos SIMPLE controller");
        interactionProfilePath = interactionProfilePathKHRSimple;
    } else {
        // Query Set
        XrActionSet queryActionSet =
                CreateActionSet(1, "query_action_set", "Action Set used to query device caps");
        XrAction dummyAction = CreateAction(
                queryActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dummy_action", "Dummy Action", 0,
                NULL);

        // Map bindings
        XrActionSuggestedBinding bindings[1];
        int currBinding = 0;
        bindings[currBinding++] =
                ActionSuggestedBinding(dummyAction, "/user/hand/right/input/system/click");

        XrInteractionProfileSuggestedBinding suggestedBindings = {
                XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.countSuggestedBindings = currBinding;

        // Try all
        suggestedBindings.interactionProfile = interactionProfilePathTouch;
        XrResult suggestTouchResult =
                xrSuggestInteractionProfileBindings(appState.Instance, &suggestedBindings);
        OXR(suggestTouchResult);

        if (XR_SUCCESS == suggestTouchResult) {
            ALOGV("xrSuggestInteractionProfileBindings found bindings for QUEST controller");
            interactionProfilePath = interactionProfilePathTouch;
        }

        if (interactionProfilePath == XR_NULL_PATH) {
            // Simple as a fallback
            bindings[0] =
                    ActionSuggestedBinding(dummyAction, "/user/hand/right/input/select/click");
            suggestedBindings.interactionProfile = interactionProfilePathKHRSimple;
            XrResult suggestKHRSimpleResult =
                    xrSuggestInteractionProfileBindings(appState.Instance, &suggestedBindings);
            OXR(suggestKHRSimpleResult);
            if (XR_SUCCESS == suggestKHRSimpleResult) {
                ALOGV(
                        "xrSuggestInteractionProfileBindings found bindings for Khronos SIMPLE controller");
                interactionProfilePath = interactionProfilePathKHRSimple;
            } else {
                ALOGE("xrSuggestInteractionProfileBindings did NOT find any bindings.");
                assert(false);
            }
        }
    }
    // Action creation

    // Map bindings
    int currBinding = 0;

    ALOGV("TEST MESSAGE\n");
    if (interactionProfilePath == interactionProfilePathTouch) {
        ALOGV("USING TOUCH\n");
        bindings[currBinding++] =
                ActionSuggestedBinding(LTrigger, "/user/hand/left/input/trigger");
        bindings[currBinding++] =
                ActionSuggestedBinding(RTrigger, "/user/hand/right/input/trigger");
        bindings[currBinding++] =
                ActionSuggestedBinding(XButton, "/user/hand/left/input/x/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(AButton, "/user/hand/right/input/a/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(moveOnXAction,
                                       "/user/hand/left/input/squeeze/value");
        bindings[currBinding++] =
                ActionSuggestedBinding(moveOnXAction,
                                       "/user/hand/right/input/squeeze/value");
        bindings[currBinding++] =
                ActionSuggestedBinding(moveOnYAction,
                                       "/user/hand/left/input/trigger/value");
        bindings[currBinding++] =
                ActionSuggestedBinding(moveOnYAction,
                                       "/user/hand/right/input/trigger/value");
        bindings[currBinding++] = ActionSuggestedBinding(
                LThumbstick, "/user/hand/left/input/thumbstick");
        bindings[currBinding++] = ActionSuggestedBinding(
                RThumbstick, "/user/hand/right/input/thumbstick");
        bindings[currBinding++] = ActionSuggestedBinding(
                LThumbstickClick, "/user/hand/left/input/thumbstick/click");
        bindings[currBinding++] = ActionSuggestedBinding(
                RThumbstickClick, "/user/hand/right/input/thumbstick/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(YButton, "/user/hand/left/input/y/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(BButton,
                                       "/user/hand/right/input/b/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(vibrateLeftFeedback,
                                       "/user/hand/left/output/haptic");
        bindings[currBinding++] =
                ActionSuggestedBinding(vibrateRightFeedback,
                                       "/user/hand/right/output/haptic");
        bindings[currBinding++] =
                ActionSuggestedBinding(aimPoseAction, "/user/hand/left/input/aim/pose");
        bindings[currBinding++] =
                ActionSuggestedBinding(aimPoseAction, "/user/hand/right/input/aim/pose");
        bindings[currBinding++] =
                ActionSuggestedBinding(gripPoseAction, "/user/hand/left/input/grip/pose");
        bindings[currBinding++] =
                ActionSuggestedBinding(gripPoseAction, "/user/hand/right/input/grip/pose");
    }

    if (interactionProfilePath == interactionProfilePathKHRSimple) {
        bindings[currBinding++] =
                ActionSuggestedBinding(toggleAction, "/user/hand/left/input/select/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(toggleAction, "/user/hand/right/input/select/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(vibrateLeftToggle,
                                       "/user/hand/left/input/menu/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(vibrateRightToggle,
                                       "/user/hand/right/input/menu/click");
        bindings[currBinding++] =
                ActionSuggestedBinding(vibrateLeftFeedback,
                                       "/user/hand/left/output/haptic");
        bindings[currBinding++] =
                ActionSuggestedBinding(vibrateRightFeedback,
                                       "/user/hand/right/output/haptic");
        bindings[currBinding++] =
                ActionSuggestedBinding(aimPoseAction, "/user/hand/left/input/aim/pose");
        bindings[currBinding++] =
                ActionSuggestedBinding(aimPoseAction, "/user/hand/right/input/aim/pose");
        bindings[currBinding++] =
                ActionSuggestedBinding(gripPoseAction, "/user/hand/left/input/grip/pose");
        bindings[currBinding++] =
                ActionSuggestedBinding(gripPoseAction, "/user/hand/right/input/grip/pose");
    }

    XrInteractionProfileSuggestedBinding suggestedBindings = {
            XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = interactionProfilePath;
    suggestedBindings.suggestedBindings = bindings;
    suggestedBindings.countSuggestedBindings = currBinding;
    OXR(xrSuggestInteractionProfileBindings(appState.Instance, &suggestedBindings));

    // Attach to session
    XrSessionActionSetsAttachInfo attachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &runningActionSet;
    OXR(xrAttachSessionActionSets(appState.Session, &attachInfo));

    // Enumerate actions
    XrPath actionPathsBuffer[16];
    char stringBuffer[256];
    XrAction actionsToEnumerate[] = {
            toggleAction,
            moveOnXAction,
            moveOnYAction,
            LThumbstickClick,
            RThumbstickClick,
            LThumbstick,
            RThumbstick,
            vibrateLeftToggle,
            vibrateRightToggle,
            vibrateLeftFeedback,
            vibrateRightFeedback,
            aimPoseAction,
            gripPoseAction,
            LTrigger,
            RTrigger,
            XButton,
            YButton,
            AButton,
            BButton
    };
    for (size_t i = 0; i < sizeof(actionsToEnumerate) / sizeof(actionsToEnumerate[0]); ++i) {
        XrBoundSourcesForActionEnumerateInfo enumerateInfo = {
                XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
        enumerateInfo.action = actionsToEnumerate[i];

        // Get Count
        uint32_t countOutput = 0;
        OXR(xrEnumerateBoundSourcesForAction(
                appState.Session, &enumerateInfo, 0 /* request size */, &countOutput, NULL));
        ALOGV(
                "xrEnumerateBoundSourcesForAction action=%lld count=%u",
                (long long) enumerateInfo.action,
                countOutput);

        if (countOutput < 16) {
            OXR(xrEnumerateBoundSourcesForAction(
                    appState.Session, &enumerateInfo, 16, &countOutput, actionPathsBuffer));
            for (uint32_t a = 0; a < countOutput; ++a) {
                XrInputSourceLocalizedNameGetInfo nameGetInfo = {
                        XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
                nameGetInfo.sourcePath = actionPathsBuffer[a];
                nameGetInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                              XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                              XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

                uint32_t stringCount = 0u;
                OXR(xrGetInputSourceLocalizedName(
                        appState.Session, &nameGetInfo, 0, &stringCount, NULL));
                if (stringCount < 256) {
                    OXR(xrGetInputSourceLocalizedName(
                            appState.Session, &nameGetInfo, 256, &stringCount, stringBuffer));
                    char pathStr[256];
                    uint32_t strLen = 0;
                    OXR(xrPathToString(
                            appState.Instance,
                            actionPathsBuffer[a],
                            (uint32_t) sizeof(pathStr),
                            &strLen,
                            pathStr));
                    ALOGV(
                            "  -> path = %lld `%s` -> `%s`",
                            (long long) actionPathsBuffer[a],
                            pathStr,
                            stringBuffer);
                }
            }
        }
    }
    leftControllerAimSpace = XR_NULL_HANDLE;
    rightControllerAimSpace = XR_NULL_HANDLE;
    leftControllerGripSpace = XR_NULL_HANDLE;
    rightControllerGripSpace = XR_NULL_HANDLE;
    return 0;
}

void InitApp(struct android_app *app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    JNIEnv *Env;
#if defined(__cplusplus)
    app->activity->vm->AttachCurrentThread(&Env, nullptr);
#else
    (*app->activity->vm)->AttachCurrentThread(app->activity->vm, &Env, NULL);
#endif

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long) "OVR::Main", 0, 0, 0);
    ovrApp_Clear(&appState);

    InitializeXRLoader(app);

    LogAvailableLayers();

    // Check that the extensions required are present.
    const char *const requiredExtensionNames[] = {
            XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
            XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
            XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
            XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME,
            XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME,
            XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME,
            XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
            XR_FB_COLOR_SPACE_EXTENSION_NAME,
            XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
            XR_FB_SWAPCHAIN_UPDATE_STATE_OPENGL_ES_EXTENSION_NAME,
            XR_FB_FOVEATION_EXTENSION_NAME,
            XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME};
    const uint32_t numRequiredExtensions =
            sizeof(requiredExtensionNames) / sizeof(requiredExtensionNames[0]);

    CheckRequiredExtensions(numRequiredExtensions, requiredExtensionNames);
    XrResult initResult = CreateXRResult(numRequiredExtensions, requiredExtensionNames);
    XrSystemId systemId = CreateXRSystemID(initResult);
    InitializeGraphics(systemId, initResult);
    projections = ConfigureSpaces();

    InitVRController();

    ovrRenderer_Create(
            appState.Session,
            &appState.Renderer,
            appState.ViewConfigurationView[0].recommendedImageRectWidth,
            appState.ViewConfigurationView[0].recommendedImageRectHeight);

    ovrRenderer_SetFoveation(
            &appState.Instance,
            &appState.Session,
            &appState.Renderer,
            XR_FOVEATION_LEVEL_HIGH_FB,
            0,
            XR_FOVEATION_DYNAMIC_DISABLED_FB);

    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    bool stageBoundsDirty = true;


    // App-specific input
    float appQuadPositionX = 0.0f;
    float appQuadPositionY = 0.0f;
    float appCylPositionX = 0.0f;
    float appCylPositionY = 0.0f;

#if defined(__cplusplus)
    app->activity->vm->DetachCurrentThread();
#else
    (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
#endif
}

void CloseApp(struct android_app *app) {
    CloseXRPlatform(projections);
    (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}

XrFrameState frameState = {XR_TYPE_FRAME_STATE};

void SyncControllers() {
    if (leftControllerAimSpace == XR_NULL_HANDLE) {
        leftControllerAimSpace = CreateActionSpace(aimPoseAction, leftHandPath);
    }
    if (rightControllerAimSpace == XR_NULL_HANDLE) {
        rightControllerAimSpace = CreateActionSpace(aimPoseAction, rightHandPath);
    }
    if (leftControllerGripSpace == XR_NULL_HANDLE) {
        leftControllerGripSpace = CreateActionSpace(gripPoseAction, leftHandPath);
    }
    if (rightControllerGripSpace == XR_NULL_HANDLE) {
        rightControllerGripSpace = CreateActionSpace(gripPoseAction, rightHandPath);
    }

    XrAction controller[] = {aimPoseAction, gripPoseAction, aimPoseAction, gripPoseAction};
    XrPath subactionPath[] = {leftHandPath, leftHandPath, rightHandPath, rightHandPath};
    XrSpace controllerSpace[] = {
            leftControllerAimSpace,
            leftControllerGripSpace,
            rightControllerAimSpace,
            rightControllerGripSpace,
    };
    for (int i = 0; i < 4; i++) {
        if (ActionPoseIsActive(controller[i], subactionPath[i])) {
            LocVel lv = GetSpaceLocVel(controllerSpace[i], frameState.predictedDisplayTime);
            appState.Scene.TrackedController[i].Active =
                    (lv.loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
            appState.Scene.TrackedController[i].Pose = lv.loc.pose;
            for (int j = 0; j < 3; j++) {
                float dt = 0.01f; // use 0.2f for for testing velocity vectors
                (&appState.Scene.TrackedController[i].Pose.position.x)[j] +=
                        (&lv.vel.linearVelocity.x)[j] * dt;
            }
        } else {
            ovrTrackedController_Clear(&appState.Scene.TrackedController[i]);
        }
    }

    // sync action data
    XrActiveActionSet activeActionSet = {0};
    activeActionSet.actionSet = runningActionSet;
    activeActionSet.subactionPath = XR_NULL_PATH;

    XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    OXR(xrSyncActions(appState.Session, &syncInfo));

    // query input action states
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.subactionPath = XR_NULL_PATH;
}

bool IsVRButtonPressed(int button) {
    XrActionStateBoolean buttonState = GetActionStateBoolean(bindings[button].action);
    return buttonState.changedSinceLastSync & buttonState.currentState;
}

bool IsVRButtonDown(int button) {
//    XrActionStateBoolean vibrateRightState = GetActionStateBoolean(vibrateRightToggle);
    return GetActionStateBoolean(bindings[button].action).currentState;
}

bool IsVRButtonReleased(int button) {
    XrActionStateBoolean vibrateRightState = GetActionStateBoolean(bindings[button].action);
    return vibrateRightState.changedSinceLastSync & !vibrateRightState.currentState;
}

bool IsVRButtonUp(int button) {
//    XrActionStateBoolean vibrateRightState = GetActionStateBoolean(vibrateRightToggle);
    return !GetActionStateBoolean(bindings[button].action).currentState;
}

float GetVRFloat(int button) {
    XrActionStateFloat inputState = GetActionStateFloat(bindings[button].action);
    return inputState.currentState;
}

Vector4 GetVROrientation(int controller) {
    int POSE_TYPE = 0;
    XrAction controllers[] = {aimPoseAction, gripPoseAction, aimPoseAction, gripPoseAction};
    XrPath subactionPath[] = {leftHandPath, leftHandPath, rightHandPath, rightHandPath};
    XrSpace controllerSpace[] = {
            leftControllerAimSpace,
            leftControllerGripSpace,
            rightControllerAimSpace,
            rightControllerGripSpace,
    };
    if (ActionPoseIsActive(controllers[controller * 2], subactionPath[controller * 2])) {
        POSE_TYPE = 0;
    }
    else {
        POSE_TYPE = 1;
    }
    XrPosef pose =  appState.Scene.TrackedController[controller * 2 + POSE_TYPE].Pose;
    Vector4 v = {pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w};
    return v;
}

Vector3 GetVRPosition(int controller) {
    //TODO: fix code duplication here and above
    int POSE_TYPE = 0;
    XrAction controllers[] = {aimPoseAction, gripPoseAction, aimPoseAction, gripPoseAction};
    XrPath subactionPath[] = {leftHandPath, leftHandPath, rightHandPath, rightHandPath};
    XrSpace controllerSpace[] = {
            leftControllerAimSpace,
            leftControllerGripSpace,
            rightControllerAimSpace,
            rightControllerGripSpace,
    };
    if (ActionPoseIsActive(controllers[controller * 2], subactionPath[controller * 2])) {
        POSE_TYPE = 0;
    }
    else {
        POSE_TYPE = 1;
    }
    XrPosef pose =  appState.Scene.TrackedController[controller * 2 + POSE_TYPE].Pose;
    Vector3 v = {pose.position.x, pose.position.y, pose.position.z};
    ALOGV("TEST: position of right controller is %f %f %f\n", v.x, v.y, v.z);
    return v;
}

void setVRControllerVibration(int controller, float frequency, float amplitude, long duration) {
    controller == 0 ? ALOGV("Firing Haptics on L ... ") : ALOGV("Firing Haptics on R ... ");
    XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
    vibration.amplitude = amplitude;
    vibration.duration = duration;
    vibration.frequency = frequency;
    XrHapticActionInfo hapticActionInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
    hapticActionInfo.action = controller == 0 ? vibrateLeftFeedback : vibrateRightFeedback;
    OXR(xrApplyHapticFeedback(
            appState.Session, &hapticActionInfo,
            (const XrHapticBaseHeader *) &vibration));
}

Vector2 GetThumbstickAxisMovement(int controller) {
    XrActionStateVector2f joystickState = GetActionStateVector2(controller ? RThumbstick : LThumbstick);
    struct Vector2 vec = {joystickState.currentState.x, joystickState.currentState.y};
    return vec;
}

void inLoop(struct android_app *app) {
    // Read all pending events.
    for (;;) {
        int events;
        struct android_poll_source *source;
        // If the timeout is zero, returns immediately without blocking.
        // If the timeout is negative, waits indefinitely until an event appears.
        const int timeoutMilliseconds =
                (appState.Resumed == false && appState.SessionActive == false &&
                 app->destroyRequested == 0)
                ? -1
                : 0;
        if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void **) &source) < 0) {
            break;
        }

        // Process this event.
        if (source != NULL) {
            source->process(app, source);
        }
    }

    ovrApp_HandleXrEvents(&appState);

    // Create the scene if not yet created.
    // The scene is created here to be able to show a loading icon.
    if (!ovrScene_IsCreated(&appState.Scene)) {
        ALOGV("Creating Scene\n");
        ovrScene_Create(
                app->activity->assetManager, appState.Instance, appState.Session,
                &appState.Scene);
    }

    if (stageBoundsDirty) {
        UpdateStageBounds(&appState);
        stageBoundsDirty = false;
    }

    // NOTE: OpenXR does not use the concept of frame indices. Instead,
    // XrWaitFrame returns the predicted display time.

    // update input information


//    // OpenXR input
//    {
//        XrActionStateBoolean toggleState = GetActionStateBoolean(toggleAction);
//        XrActionStateBoolean vibrateLeftState = GetActionStateBoolean(vibrateLeftToggle);
//        XrActionStateBoolean thumbstickClickState =
//                GetActionStateBoolean(thumbstickClickAction);
//
//        // Update app logic based on input
//        if (toggleState.changedSinceLastSync) {
//            // Also stop haptics
//            XrHapticActionInfo hapticActionInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
//            hapticActionInfo.action = vibrateLeftFeedback;
//            OXR(xrStopHapticFeedback(appState.Session, &hapticActionInfo));
//            hapticActionInfo.action = vibrateRightFeedback;
//            OXR(xrStopHapticFeedback(appState.Session, &hapticActionInfo));
//        }
//
//        if (thumbstickClickState.changedSinceLastSync &&
//            thumbstickClickState.currentState == XR_TRUE) {
//            float currentRefreshRate = 0.0f;
//            OXR(appState.pfnGetDisplayRefreshRate(appState.Session, &currentRefreshRate));
//            ALOGV("Current Display Refresh Rate: %f", currentRefreshRate);
//
//            const int requestedRateIndex = appState.RequestedDisplayRefreshRateIndex++ %
//                                           appState.NumSupportedDisplayRefreshRates;
//
//            const float requestRefreshRate =
//                    appState.SupportedDisplayRefreshRates[requestedRateIndex];
//            ALOGV("Requesting Display Refresh Rate: %f", requestRefreshRate);
//            OXR(appState.pfnRequestDisplayRefreshRate(appState.Session, requestRefreshRate));
//        }
//
//        // The KHR simple profile doesn't have these actions, so the getters will fail
//        // and flood the log with errors.
//        if (useSimpleProfile == false) {
//            XrActionStateFloat moveXState = GetActionStateFloat(moveOnXAction);
//            XrActionStateFloat moveYState = GetActionStateFloat(moveOnYAction);
//            if (moveXState.changedSinceLastSync) {
//                appQuadPositionX = moveXState.currentState;
//            }
//            if (moveYState.changedSinceLastSync) {
//                appQuadPositionY = moveYState.currentState;
//            }
//
//            XrActionStateVector2f moveJoystickState =
//                    GetActionStateVector2(moveOnJoystickAction);
//            if (moveJoystickState.changedSinceLastSync) {
//                appCylPositionX = moveJoystickState.currentState.x;
//                appCylPositionY = moveJoystickState.currentState.y;
//            }
//        }
//
//        // Haptics
//        // NOTE: using the values from the example in the spec
//        if (vibrateLeftState.changedSinceLastSync && vibrateLeftState.currentState) {
//            ALOGV("Firing Haptics on L ... ");
//            // fire haptics using output action
//            XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
//            vibration.amplitude = 0.5;
//            vibration.duration = ToXrTime(0.5); // half a second
//            vibration.frequency = 3000;
//            XrHapticActionInfo hapticActionInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
//            hapticActionInfo.action = vibrateLeftFeedback;
//            OXR(xrApplyHapticFeedback(
//                    appState.Session, &hapticActionInfo,
//                    (const XrHapticBaseHeader *) &vibration));
//        }
//    }



    // Set-up the compositor layers for this frame.
    // NOTE: Multiple independent layers are allowed, but they need to be added
    // in a depth consistent order.

    // Compose the layers for this frame.
}

bool hasCubeMapBackground;
bool shouldRenderWorldLayer;
ovrSceneMatrices sceneMatrices;
XrCompositionLayerProjectionView projection_layer_elements[2];
XrPosef viewTransform[2];

void DrawVRCubemap();

void DrawVREquirect();

void DrawVRWorld(int xOffset, int yOffset);

void SetupProjectionLayerForEye(int eye, XrCompositionLayerProjectionView *pView, int xOffset,
                                int yOffset);

void BeginVRMode(void) {
    XrFrameWaitInfo waitFrameInfo = {XR_TYPE_FRAME_WAIT_INFO};

    OXR(xrWaitFrame(appState.Session, &waitFrameInfo, &frameState));

    // Get the HMD pose, predicted for the middle of the time period during which
    // the new eye images will be displayed. The number of frames predicted ahead
    // depends on the pipeline depth of the engine and the synthesis rate.
    // The better the prediction, the less black will be pulled in at the edges.
    XrFrameBeginInfo beginFrameDesc = {XR_TYPE_FRAME_BEGIN_INFO};
    OXR(xrBeginFrame(appState.Session, &beginFrameDesc));

    XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
    OXR(xrLocateSpace(
            appState.HeadSpace, appState.CurrentSpace, frameState.predictedDisplayTime, &loc));
    XrPosef xfStageFromHead = loc.pose;
    OXR(xrLocateSpace(
            appState.HeadSpace, appState.LocalSpace, frameState.predictedDisplayTime, &loc));

    XrViewLocateInfo projectionInfo = {XR_TYPE_VIEW_LOCATE_INFO};
    projectionInfo.viewConfigurationType = appState.ViewportConfig.viewConfigurationType;
    projectionInfo.displayTime = frameState.predictedDisplayTime;
    projectionInfo.space = appState.HeadSpace;

    XrViewState viewState = {XR_TYPE_VIEW_STATE};

    uint32_t projectionCapacityInput = ovrMaxNumEyes;
    uint32_t projectionCountOutput = projectionCapacityInput;

    OXR(xrLocateViews(
            appState.Session,
            &projectionInfo,
            &viewState,
            projectionCapacityInput,
            &projectionCountOutput,
            projections));
    //

    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        XrPosef xfHeadFromEye = projections[eye].pose;
        XrPosef xfStageFromEye;
        XrPosef_Multiply(&xfStageFromEye, &xfStageFromHead, &xfHeadFromEye);
        XrPosef_Invert(&viewTransform[eye], &xfStageFromEye);

        XrMatrix4x4f_CreateFromRigidTransform(&sceneMatrices.ViewMatrix[eye],
                                              &viewTransform[eye]);

        const XrFovf fov = projections[eye].fov;
        XrMatrix4x4f_CreateProjectionFov(
                &sceneMatrices.ProjectionMatrix[eye], GRAPHICS_OPENGL_ES, fov, 0.1f, 0.0f);
    }
    projection_layer_elements[0] = (struct XrCompositionLayerProjectionView) {
            XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
    projection_layer_elements[1] = (struct XrCompositionLayerProjectionView) {
            XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};

    appState.LayerCount = 0;
    memset(appState.Layers, 0, sizeof(ovrCompositorLayer_Union) * ovrMaxLayerCount);
    shouldRenderWorldLayer = true;
    hasCubeMapBackground = appState.Scene.CubeMapSwapChain.Handle != XR_NULL_HANDLE;
}

//helper for DrawVRBackground
void DrawVREquirect() {
    XrCompositionLayerEquirect2KHR equirect = {XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR};
    equirect.layerFlags = 0;
    equirect.space = appState.CurrentSpace;
    equirect.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

    memset(&equirect.subImage, 0, sizeof(XrSwapchainSubImage));
    equirect.subImage.swapchain = appState.Scene.EquirectSwapChain.Handle;
    equirect.subImage.imageRect.extent.width = appState.Scene.EquirectSwapChain.Width;
    equirect.subImage.imageRect.extent.height = appState.Scene.EquirectSwapChain.Height;

    XrPosef_CreateIdentity(&equirect.pose);
    equirect.radius = 10.0f;
    equirect.centralHorizontalAngle = (2.0f * MATH_PI) / 3.0f;    // 120 degrees horizontal
    equirect.upperVerticalAngle = (MATH_PI / 2.0f) * (2.0f / 3.0f); // 60 degrees up
    equirect.lowerVerticalAngle = 0.0f;                             // 0 degrees down (equator)

    appState.Layers[appState.LayerCount++].Equirect2 = equirect;
}

//helper for DrawVRBackground
void DrawVRCubemap() {
    XrCompositionLayerCubeKHR cube_layer = {XR_TYPE_COMPOSITION_LAYER_CUBE_KHR};
    cube_layer.layerFlags = 0;
    cube_layer.space = appState.CurrentSpace;
    cube_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    cube_layer.swapchain = appState.Scene.CubeMapSwapChain.Handle;
    XrQuaternionf_CreateIdentity(&cube_layer.orientation);

    appState.Layers[appState.LayerCount++].Cube = cube_layer;

}

//helper for DrawVRBackground
void AddObjectsToLayer(int xOffset, int yOffset) {
    ovrRenderer_RenderFrame(&appState.Renderer, &appState.Scene, &sceneMatrices);

    XrCompositionLayerProjection projection_layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                                  XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    projection_layer.space = appState.CurrentSpace;
    projection_layer.viewCount = ovrMaxNumEyes;
    projection_layer.views = projection_layer_elements;

    for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
        SetupProjectionLayerForEye(eye, &projection_layer_elements[eye], xOffset, yOffset);
    }

    appState.Layers[appState.LayerCount++].Projection = projection_layer;
}

//helper for AddObjectsToLayer
void SetupProjectionLayerForEye(int eye, XrCompositionLayerProjectionView *layer, int xOffset,
                                int yOffset) {
    ovrFramebuffer *frameBuffer = &appState.Renderer.FrameBuffer[eye];

    memset(layer, 0, sizeof(XrCompositionLayerProjectionView));
    layer->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;

    XrPosef_Invert(&layer->pose, &viewTransform[eye]);
    layer->fov = projections[eye].fov;

    memset(&layer->subImage, 0, sizeof(XrSwapchainSubImage));
    layer->subImage.swapchain = frameBuffer->ColorSwapChain.Handle;
    layer->subImage.imageRect.extent.width = frameBuffer->ColorSwapChain.Width;
    layer->subImage.imageRect.extent.height = frameBuffer->ColorSwapChain.Height;
//    layer->subImage.imageRect.offset.x = xOffset;
//    layer->subImage.imageRect.offset.y = yOffset;
}


//reorganized and renamed from ClearBackgroundVR()
void DrawVRBackground(int xOffset, int yOffset) {
    shouldRenderWorldLayer = true;

    if (appState.Scene.BackGroundType == BACKGROUND_CUBEMAP &&
        appState.Scene.CubeMapSwapChain.Handle != XR_NULL_HANDLE) {
        DrawVRCubemap();
        shouldRenderWorldLayer = false;
    } else if (appState.Scene.BackGroundType == BACKGROUND_EQUIRECT) {
        DrawVREquirect();
    }
}


void DrawVRCylinder(Vector3 position, Vector3 axis, float radius, float aspectRatio) {
    XrCompositionLayerCylinderKHR cylinder = {XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
    cylinder.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    cylinder.space = appState.LocalSpace;
    cylinder.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

    //setup swapchain
    memset(&cylinder.subImage, 0, sizeof(XrSwapchainSubImage));
    cylinder.subImage.swapchain = appState.Scene.CylinderSwapChain.Handle;
    cylinder.subImage.imageRect.extent.width = appState.Scene.CylinderSwapChain.Width;
    cylinder.subImage.imageRect.extent.height = appState.Scene.CylinderSwapChain.Height;

    //convert position and axis to XR format
    XrVector3f xrPosition = {position.x, position.y, position.z};
    XrVector3f xrAxis = {axis.x, axis.y, axis.z};

    //setup cylinder dimensions
    cylinder.radius = radius;
    cylinder.centralAngle = MATH_PI / 4.0;
    cylinder.aspectRatio = aspectRatio;

    //setup position and orientation
    cylinder.pose.position = xrPosition;
    XrQuaternionf_CreateFromAxisAngle(&cylinder.pose.orientation, &xrAxis,
                                      -45.0f * MATH_PI / 180.0f);

    appState.Layers[appState.LayerCount++].Cylinder = cylinder;
}

//helper for DrawVRQuad
XrCompositionLayerQuad
CreateQuadLayer(XrEyeVisibility eye, Vector3 position, Vector3 axis, float width, float height) {
    XrCompositionLayerQuad quad = {XR_TYPE_COMPOSITION_LAYER_QUAD};
//    quad.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    quad.space = appState.CurrentSpace;
    quad.eyeVisibility = eye;
    memset(&quad.subImage, 0, sizeof(XrSwapchainSubImage));
    quad.subImage.swapchain = appState.Scene.QuadSwapChain.Handle;
    quad.subImage.imageRect.extent.width = appState.Scene.QuadSwapChain.Width;
    quad.subImage.imageRect.extent.height = appState.Scene.QuadSwapChain.Height;

    //convert position and axis to XR format
    XrVector3f xrPosition = {position.x, position.y, position.z};
    XrVector3f xrAxis = {axis.x, axis.y, axis.z};

    //setup position, orientation and size
    XrPosef_CreateIdentity(&quad.pose);
    XrQuaternionf_CreateFromAxisAngle(&quad.pose.orientation, &xrAxis, 45.0f * MATH_PI / 180.0f);
    quad.pose.position = xrPosition;
    quad.size = (XrExtent2Df) {width, height};

    return quad;
}

void DrawVRQuad(Vector3 position, Vector3 axis, float width, float height) {
    //drawing left eye quad
    XrCompositionLayerQuad leftQuad = CreateQuadLayer(
            XR_EYE_VISIBILITY_LEFT,
            position,
            axis,
            width,
            height
    );
    appState.Layers[appState.LayerCount++].Quad = leftQuad;

    //drawing the right eye quad
    XrCompositionLayerQuad rightQuad = CreateQuadLayer(
            XR_EYE_VISIBILITY_RIGHT,
            position,
            axis,
            width,
            height
    );
    appState.Layers[appState.LayerCount++].Quad = rightQuad;
}

void EndVRMode(void) {

    if (shouldRenderWorldLayer) {
        AddObjectsToLayer(0.0, 0.0);
    }
    const XrCompositionLayerBaseHeader *layers[ovrMaxLayerCount] = {0};
    for (int i = 0; i < appState.LayerCount; i++) {
        layers[i] = (const XrCompositionLayerBaseHeader *) &appState.Layers[i];
    }

    XrFrameEndInfo endFrameInfo = {XR_TYPE_FRAME_END_INFO};
    endFrameInfo.displayTime = frameState.predictedDisplayTime;
    endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endFrameInfo.layerCount = appState.LayerCount;
    endFrameInfo.layers = layers;

    OXR(xrEndFrame(appState.Session, &endFrameInfo));
}
