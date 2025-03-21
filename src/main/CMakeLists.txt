# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.22.1)

# Set standards for C and C++
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Set the project name
project(raymob)

if(NOT TARGET OpenXR::openxr_loader)
    find_package(OpenXR REQUIRED)
endif()

# Add subdirectories for dependencies
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/raylib)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/raymob)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/Samples/SampleXrFramework)

# Fetch all source files recursively, excluding 'deps' source files
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/*.c" "${CMAKE_SOURCE_DIR}/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/deps/.*")

# Include headers for android_native_app_glue.c
include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)

# Add android_native_app_glue.c to source files
list(APPEND SOURCES ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# Create a shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Define compiler macros
target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_ANDROID)

# Set compile flags based on build type
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG DEBUG)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-limit-debug-info")
    set(CMAKE_C_FLAGS_DEBUG "-g -fno-limit-debug-info")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Include dependency headers
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/deps/raylib")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/deps/raymob")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/deps/Samples/SampleXrFramework")

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE raylib native_activity_framework)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")

# Common across platforms
target_include_directories(${PROJECT_NAME} PRIVATE Src)

# Add Clang-Tidy linter
set(CLANG_TIDY_EXE "C:/Program Files/LLVM/bin/clang-tidy.exe") # Update the path to clang-tidy
set(CLANG_TIDY_OPTIONS
        "-checks=*,-clang-analyzer-alpha*"  # Enable all checks except alpha ones
        "-header-filter=.*"                 # Include headers in the checks
        "-format-style=file"                # Use project .clang-format if available
        "-quiet"                            # Always produce output, even if no issues
)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")

message(STATUS "Clang-Tidy linting enabled.")