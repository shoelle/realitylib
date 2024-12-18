# Define a library for raylib
add_library(raylib STATIC
    rcore.c
    rmodels.c
    rshapes.c
    rtext.c
    rtextures.c
    raudio.c
    utils.c
    )

# Include headers directory for android_native_app_glue.c
include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)

# Add OpenXR include paths
target_include_directories(raylib PRIVATE
        ${CMAKE_SOURCE_DIR}/xr
        ${CMAKE_SOURCE_DIR}/../xr  # May need this depending on relative path
)

target_include_directories(raylib PRIVATE
        ${CMAKE_SOURCE_DIR}/deps/OpenXR/meta_openxr_preview
        ${CMAKE_SOURCE_DIR}/deps/OpenXR/meta_openxr_preview/include
)

message(STATUS "CMAKE_SOURCE_DIR is: ${CMAKE_SOURCE_DIR}")

if(NOT TARGET OpenXR::openxr_loader)
    find_package(OpenXR REQUIRED)
endif()
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/raylib/Samples)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/deps/raylib/Samples")

# Add android_native_app_glue.c to the source files
list(APPEND SOURCES ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# Define compiler macros for raylib
target_compile_definitions(raylib PRIVATE
    GRAPHICS_API_OPENGL_ES2
    PLATFORM_ANDROID
    __ANDROID__
    )

# Add specific compilation options based on target Android architecture
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=vfpv3-d16")
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-835769")
endif()

# Link required libraries to raylib
target_link_libraries(raylib log android EGL GLESv2 OpenSLES dl m c)

# Add library-specific binding option
target_link_options(raylib PRIVATE "-u ANativeActivity_onCreate")