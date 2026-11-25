cmake_minimum_required(VERSION 3.20)
project(benno111_engene DESCRIPTION "benno111 engene" LANGUAGES C)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJC)
endif()

set(BENNO111_FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/Benno111Engene" CACHE PATH
    "Path to the standalone Benno111Engene repository")
if(NOT EXISTS "${BENNO111_FRAMEWORK_ROOT}/framework/CMakeLists.txt")
    message(FATAL_ERROR "Clone Benno111Engene into .deps/Benno111Engene or set BENNO111_FRAMEWORK_ROOT. See README.md.")
endif()

add_subdirectory("${BENNO111_FRAMEWORK_ROOT}/framework" "${CMAKE_CURRENT_BINARY_DIR}/framework")
add_subdirectory(game)
