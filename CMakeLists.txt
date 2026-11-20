cmake_minimum_required(VERSION 3.20)
project(benno111_engene DESCRIPTION "benno111 engene" LANGUAGES C)

if(ANDROID)
    set(PLATFORM Android CACHE STRING "raylib platform" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(CMAKE_VERSION VERSION_LESS 3.24)
        message(FATAL_ERROR "The iOS build requires CMake 3.24 or newer")
    endif()
    set(PLATFORM SDL CACHE STRING "raylib platform" FORCE)
    set(OPENGL_VERSION "ES 2.0" CACHE STRING "raylib graphics API" FORCE)

    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
        URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-2.32.10
        URL_HASH SHA256=03f9d7c191a837525c9cda6406af2f2e48be02b5e7eb03d949cc9f1e9ca41c8b
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        OVERRIDE_FIND_PACKAGE)
    FetchContent_MakeAvailable(SDL2)
endif()

add_subdirectory(framework)
add_subdirectory(game)
