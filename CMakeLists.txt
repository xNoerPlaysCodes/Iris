cmake_minimum_required(VERSION 3.30)

if ({CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_ARCHITECTURES arm64)
    set(CMAKE_OSX_SYSROOT macosx)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(BINARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIRECTORY})
set(CMAKE_BUILD_TYPE Debug)

macro (SetBinaryOutputDirectory output)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIRECTORY}/${output})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIRECTORY}/${output})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIRECTORY}/${output})
endmacro()

project(Iris CXX C)

include(FetchContent)

macro (SetupDesktopBuildDependencies)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
    find_package(GLEW REQUIRED)
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        find_package(OpenGL REQUIRED)
        set(OPENGL_LIB OpenGL::OpenGL)
    endif()
    set(NATIVE_LINKED_LIBS ${NATIVE_LINKED_LIBS} glfw GLEW::GLEW)
endmacro()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(NATIVE_SOURCE darwin.cpp)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
    set(NATIVE_LINKED_LIBS ${IOKIT_FRAMEWORK} ${CORE_GRAPHICS})

    SetBinaryOutputDirectory(darwin)
    SetupDesktopBuildDependencies()
    message(STATUS "IrisNative found for Darwin")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(NATIVE_SOURCE linux.cpp)

    SetBinaryOutputDirectory(linux)
    SetupDesktopBuildDependencies()
    message(STATUS "IrisNative found for Linux")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    if ("${ANDROID_NDK}" STREQUAL "")
        message(FATAL_ERROR "Unrecognized ANDROID_NDK path, set with -DANDROID_NDK")
    endif()
    set(NATIVE_SOURCE android.cpp)
    set(NATIVE_LINKED_LIBS android GLESv3 log egl)
    set(NATIVE_PUBLIC_INCLUDES ${ANDROID_NDK}/sources/android/native_app_glue)

    SetBinaryOutputDirectory(android)
    message(STATUS "Android NDK found at ${ANDROID_NDK}")
    message(STATUS "IrisNative found for Android")
else()
    message(FATAL_ERROR "Unsupported platform, IrisNative for this platform is not available")
endif()

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.x
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)
FetchContent_MakeAvailable(glm)

if ("${IRIS_WINDOW_BACKEND}" STREQUAL "GLFW")
    set(IRIS_WINDOW_SOURCE glfw.cpp)
    message(STATUS "Iris will be using GLFW as the windowing and input layer")
elseif("${IRIS_WINDOW_BACKEND}" STREQUAL "ANativeWindow")
    set(IRIS_WINDOW_SOURCE anativewindow.cpp)
    message(STATUS "Iris will be using ANativeWindow as the windowing and input layer")
else()
    message(FATAL_ERROR "Unrecognized window backend (-DIRIS_WINDOW_BACKEND)")
endif()

if ("${IRIS_RENDERER_BACKEND}" STREQUAL "OpenGL")
    set(IRIS_RENDERER_SOURCES 
        src/graphics/rendering/opengl.cpp
        src/graphics/rendering/gl_render_layer.cpp
    )
    message(STATUS "Iris will be using OpenGL as the renderer")
else()
    message(FATAL_ERROR "Unrecognized renderer backend (-DIRIS_RENDERER_BACKEND)")
endif()

add_library(${PROJECT_NAME}
    src/runtime.cpp
    src/internal/dbglog.cpp
    src/internal/state.cpp
    src/internal/crash_handler.cpp
    src/internal/error.cpp
    src/internal/utilities.cpp
    src/core/asset_manager.cpp
    ${IRIS_RENDERER_SOURCES}
    ${NATIVE_EXTRA_SOURCES}
    src/graphics/windowing/${IRIS_WINDOW_SOURCE}
    src/internal/native/${NATIVE_SOURCE}

    src/lib/stb_truetype.c
    src/lib/stb_image.c
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE Iris_Debug)
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC 
        include
        ${glm_SOURCE_DIR}
        ${NATIVE_PUBLIC_INCLUDES}
    PRIVATE
        ${NATIVE_INCLUDES}
        src/include
        src/include/internal
        src/include/resources
)
target_link_libraries(${PROJECT_NAME} 
    PUBLIC 
        spdlog::spdlog 
    PRIVATE 
        ${NATIVE_LINKED_LIBS}
        ${OPENGL_LIB}
)


set(TEST_NAMES
    core/runtime.cpp
    core/window.cpp
    core/rendering.cpp
)

foreach(test_name IN LISTS TEST_NAMES)
    string(REPLACE "/" "_" test_target "${test_name}")

    if(CMAKE_SYSTEM_NAME MATCHES "Android")
        add_library(${test_target} SHARED tests/${test_name} ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    else()
        add_executable(${test_target} tests/${test_name})
    endif()

    get_filename_component(test_category "${test_name}" DIRECTORY)

    string(REGEX REPLACE "\\.cpp$" "" test_output "${test_name}")
    string(REPLACE "/" "_" test_output "${test_output}")

    set_target_properties(${test_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests"

        OUTPUT_NAME
        "${test_output}"
    )

    target_link_libraries(${test_target} PRIVATE Iris)
endforeach()

set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/src/resources")
set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/src/include/resources")

file(GLOB_RECURSE GLSL_FILES CONFIGURE_DEPENDS
    "${RESOURCE_DIR}/*.glsl"
)

set(GENERATED_GLSL_HEADERS)

foreach(GLSL_FILE ${GLSL_FILES})
    file(RELATIVE_PATH REL_PATH "${RESOURCE_DIR}" "${GLSL_FILE}")

    get_filename_component(DIR "${REL_PATH}" DIRECTORY)
    get_filename_component(NAME "${REL_PATH}" NAME_WE)

    string(REPLACE "/" "_" OUTPUT_NAME "${DIR}/${NAME}")
    set(OUTPUT_NAME "${OUTPUT_NAME}.glsl.h")

    set(OUTPUT "${GENERATED_DIR}/${OUTPUT_NAME}")

    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT=${GLSL_FILE}"
            "-DOUTPUT=${OUTPUT}"
            "-P ${CMAKE_SOURCE_DIR}/cmake/embed_glsl.cmake"
        DEPENDS "${GLSL_FILE}"
        VERBATIM
    )

    list(APPEND GENERATED_GLSL_HEADERS "${OUTPUT}")
endforeach()

add_custom_target(generate_glsl
    DEPENDS ${GENERATED_GLSL_HEADERS}
)

add_dependencies(Iris generate_glsl)
