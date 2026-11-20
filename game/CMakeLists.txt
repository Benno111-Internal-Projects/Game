if(ANDROID)
    add_library(benno111-engene SHARED main.c title_screen.c)
    set_target_properties(benno111-engene PROPERTIES OUTPUT_NAME benno111_engene)
    target_link_options(benno111-engene PRIVATE "LINKER:-u,ANativeActivity_onCreate")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(benno111-engene MACOSX_BUNDLE main.c title_screen.c)
    target_compile_definitions(benno111-engene PRIVATE BENNO_IOS)
    target_link_libraries(benno111-engene PRIVATE SDL2::SDL2main SDL2::SDL2)
    target_link_options(benno111-engene PRIVATE "LINKER:-ObjC")
    set_target_properties(benno111-engene PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.benno111.engene"
        MACOSX_BUNDLE_BUNDLE_NAME "benno111 engene"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/iOS-Info.plist")
else()
    add_executable(benno111-engene main.c title_screen.c)
endif()
target_link_libraries(benno111-engene PRIVATE render-framework)

if(NOT ANDROID)
    set_target_properties(benno111-engene PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
    set(shader_dir "${PROJECT_SOURCE_DIR}/framework/shaders")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        string(APPEND shader_dir "/es")
        # Copy after linking so Xcode has already created the application bundle.
        add_custom_command(TARGET benno111-engene POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:benno111-engene>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${shader_dir}" "$<TARGET_FILE_DIR:benno111-engene>/shaders")
    else()
        add_custom_target(render-assets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:benno111-engene>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${shader_dir}" "$<TARGET_FILE_DIR:benno111-engene>/shaders")
        add_dependencies(benno111-engene render-assets)
    endif()
endif()
