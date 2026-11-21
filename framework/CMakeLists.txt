if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_package(raylib 5.5 QUIET)
endif()
if(NOT TARGET raylib)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(raylib
        URL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/5.5
        URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e)
    FetchContent_MakeAvailable(raylib)
endif()

add_library(render-framework STATIC
    render.c camera.c physics.c input.c ui.c dynamic_buffer.c
    scene.c lod.c occlusion.c game_logic.c audio.c scene_music.c window.c prefab.c terrain.c)
target_include_directories(render-framework PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_features(render-framework PUBLIC c_std_11)
else()
    target_compile_features(render-framework PUBLIC c_std_99)
endif()
target_link_libraries(render-framework PUBLIC raylib)
if(UNIX AND NOT APPLE)
    target_link_libraries(render-framework PUBLIC m)
endif()

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(render-framework PRIVATE RENDER_FRAMEWORK_MOBILE)
endif()
