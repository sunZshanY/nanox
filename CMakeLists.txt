cmake_minimum_required(VERSION 3.20)

project(NanoX
    VERSION 0.1.0
    DESCRIPTION "A small, modern, easy-to-learn programming language and compiler"
    LANGUAGES CXX)

# C++17: modern enough for std::string_view / std::optional / std::variant,
# but widely available on all three target platforms without requiring the
# very latest compilers.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Give single-config generators (Unix Makefiles, MinGW, ...) a sane default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(NANOX_BUILD_TESTS "Build NanoX unit tests" ON)
option(NANOX_BUILD_EXAMPLES "Build NanoX examples" ON)

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(nanox_core
    src/Lexer.cpp
    src/Diagnostic.cpp
    src/TokenKind.cpp
    src/FileTree.cpp
    src/Project.cpp
)
add_library(nanox::core ALIAS nanox_core)

target_include_directories(nanox_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Warnings: treat code strictly so real bugs surface early. /utf-8 makes MSVC
# interpret the UTF-8 box-drawing characters in source files correctly.
function(nanox_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

nanox_set_warnings(nanox_core)

# ---------------------------------------------------------------------------
# Editor library (TUI editor for the nanox CLI)
# ---------------------------------------------------------------------------
add_library(nanox_editor
    src/editor/TextBuffer.cpp
    src/editor/Terminal.cpp
    src/editor/Editor.cpp
)
add_library(nanox::editor ALIAS nanox_editor)

target_include_directories(nanox_editor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(nanox_editor PUBLIC nanox::core)
nanox_set_warnings(nanox_editor)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(NANOX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(NANOX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
