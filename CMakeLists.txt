cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# vcpkg triplet – must be set before project() so the toolchain picks it up.
# On Windows we ask vcpkg for the "static" triplet so that Boost and the CRT
# are both linked statically (matching the /MT flag below).
# ---------------------------------------------------------------------------
if(WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
endif()

# ---------------------------------------------------------------------------
# PROJECT NAME
# Change the name here (first argument to project()) to rename the executable
# and all release archives. Every other reference in this file uses
# ${PROJECT_NAME} automatically – nothing else needs to be touched.
# ---------------------------------------------------------------------------
project(cpp_app
    VERSION 1.0.0
    DESCRIPTION "Cross-platform C++ console application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output the binary directly to the build root for easy packaging
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Static CRT on Windows (/MD → /MT)
# Must be done before any add_library / add_executable calls so that the
# flag replacement applies to all targets, including the test runner.
# ---------------------------------------------------------------------------
if(WIN32 AND MSVC)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Production code library
#
# All application logic lives here so it is shared by both the main
# executable and the test runner. Only main.cpp stays in the executable.
#
# Add one line per .cpp file you create. Header files (.h / .hpp) need no
# CMake entry – just #include them in your .cpp files as normal.
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME}_lib STATIC)

target_sources(${PROJECT_NAME}_lib PRIVATE
    src/math_utils.cpp
    # src/my_module.cpp   ← add new .cpp files here
)

# PUBLIC so the executable and test targets inherit the include path
# automatically when they link against this library.
target_include_directories(${PROJECT_NAME}_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Dependencies
#
# Boost.Asio and Boost.Beast are header-only; they appear as link targets
# (Boost::asio, Boost::beast) to let CMake propagate their include paths and
# any platform-specific system library requirements (e.g. Winsock on Windows,
# pthreads on Linux).
#
# Boost.JSON and Boost.ProgramOptions have compiled object files and must be
# listed in COMPONENTS so CMake locates and links the right .lib / .a files.
#
# std::filesystem, std::regex, std::thread, and std::future are part of the
# C++17 standard – no find_package required.
#
# To add more Boost libraries:
#   1. Add the vcpkg package name to vcpkg.json  (e.g. "boost-regex")
#   2. If it is a compiled library, add its component name to COMPONENTS below
#      (e.g. regex) and add Boost::<component> to target_link_libraries.
#   3. If it is header-only, just add Boost::<name> to target_link_libraries.
# ---------------------------------------------------------------------------
find_package(Boost REQUIRED COMPONENTS json program_options)
find_package(Threads REQUIRED)

target_link_libraries(${PROJECT_NAME}_lib PUBLIC
    Boost::asio             # header-only; handles Winsock on Windows
    Boost::beast            # header-only; depends on Boost::asio
    Boost::json             # compiled
    Boost::program_options  # compiled
    Threads::Threads        # pthreads on Linux, built-in on Windows/macOS
)

# Tell Boost.Asio which minimum Windows API version to target.
# 0x0A00 = Windows 10. Only relevant on Windows; ignored on other platforms.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME}_lib PUBLIC _WIN32_WINNT=0x0A00)
endif()

# ---------------------------------------------------------------------------
# Precompiled header (PCH)
# Listed headers are compiled once and reused by every .cpp in the library,
# which speeds up incremental builds when headers are large (e.g. Boost).
#
# Add any header that is included by most of your source files.
# ---------------------------------------------------------------------------
target_precompile_headers(${PROJECT_NAME}_lib PRIVATE
    <iostream>
    <string>
    <vector>
    <boost/asio.hpp>
    <boost/beast.hpp>
    <boost/json.hpp>
    <boost/program_options.hpp>
    # "src/my_header.h"   ← add your own frequently-used headers here
)

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME}_lib PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME}_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Main executable
# Only main.cpp belongs here. All other production code belongs in
# ${PROJECT_NAME}_lib above so it can be reached by tests.
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# ---------------------------------------------------------------------------
# Embedded resource: data/sample.json linked into the binary at build time.
#
# llvm-objcopy --input-target binary converts an arbitrary file into a native
# object (.o / .obj) that exports three linker symbols derived from the input
# filename:
#
#   _binary_sample_json_start   first byte
#   _binary_sample_json_end     one past the last byte
#   _binary_sample_json_size    byte count as a linker symbol (use pointer
#                               arithmetic: end - start, for portability)
#
# Running llvm-objcopy from the data/ directory means the input argument is
# simply "sample.json", keeping the symbol names short and predictable.
#
# The same extern "C" declaration works on ELF (Linux/Android), Mach-O
# (macOS), and COFF x64 (Windows) because llvm-objcopy follows each format's
# C-linkage naming convention automatically.
#
# embedded_resource.cpp and the generated object are added directly to the
# executable target (not the static library) so that the test runner, which
# also links the library, does not see unresolved linker symbols.
# ---------------------------------------------------------------------------
find_program(LLVM_OBJCOPY
    NAMES
        llvm-objcopy
        llvm-objcopy-18 llvm-objcopy-17 llvm-objcopy-16
        llvm-objcopy-15 llvm-objcopy-14 llvm-objcopy-13 llvm-objcopy-12
    HINTS
        /usr/local/opt/llvm/bin         # macOS Homebrew (Intel runner, macos-13)
        /opt/homebrew/opt/llvm/bin      # macOS Homebrew (ARM runner,   macos-14)
        "C:/Program Files/LLVM/bin"     # Windows LLVM install
        "$ENV{ANDROID_NDK_LATEST_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin"
    DOC "llvm-objcopy used to embed binary resources at link time"
)

if(NOT LLVM_OBJCOPY)
    message(FATAL_ERROR
        "llvm-objcopy not found. Install LLVM tools for your platform:\n"
        "  Linux          : sudo apt-get install llvm\n"
        "  macOS          : brew install llvm\n"
        "  Windows        : install Visual Studio 'C++ Clang tools for Windows'\n"
        "  Termux         : pkg install llvm\n"
        "  Android NDK CI : ensure ANDROID_NDK_LATEST_HOME points to a valid NDK")
endif()

# Select the output object format and architecture for the current target.
if(WIN32)
    set(_EMBED_TARGET  "coff-x86-64")
    set(_EMBED_ARCH    "i386:x86-64")
    set(_EMBED_OBJ_EXT "obj")
elseif(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR
       CMAKE_SYSTEM_PROCESSOR  MATCHES "arm64|aarch64")
        set(_EMBED_TARGET "mach-o-arm64")
        set(_EMBED_ARCH   "aarch64")
    else()
        set(_EMBED_TARGET "mach-o-x86-64")
        set(_EMBED_ARCH   "i386:x86-64")
    endif()
    set(_EMBED_OBJ_EXT "o")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(_EMBED_TARGET  "elf64-littleaarch64")
    set(_EMBED_ARCH    "aarch64")
    set(_EMBED_OBJ_EXT "o")
else()  # Linux x86_64
    set(_EMBED_TARGET  "elf64-x86-64")
    set(_EMBED_ARCH    "i386:x86-64")
    set(_EMBED_OBJ_EXT "o")
endif()

set(_EMBED_OBJ "${CMAKE_CURRENT_BINARY_DIR}/sample_json.${_EMBED_OBJ_EXT}")

add_custom_command(
    OUTPUT   "${_EMBED_OBJ}"
    COMMAND  "${LLVM_OBJCOPY}"
             --input-target          binary
             --output-target         "${_EMBED_TARGET}"
             --binary-architecture   "${_EMBED_ARCH}"
             "sample.json"
             "${_EMBED_OBJ}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data"
    DEPENDS   "${CMAKE_SOURCE_DIR}/data/sample.json"
    COMMENT   "Embedding data/sample.json (${_EMBED_TARGET}) via llvm-objcopy"
    VERBATIM
)

set_source_files_properties("${_EMBED_OBJ}" PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED       TRUE
)

target_sources(${PROJECT_NAME} PRIVATE
    src/embedded_resource.cpp
    "${_EMBED_OBJ}"
)

# ---------------------------------------------------------------------------
# Platform-specific link flags to maximise self-containment
# ---------------------------------------------------------------------------
if(WIN32)
    # With MinGW we can pass -static to pull in libgcc and libstdc++.
    if(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE -static)
    endif()

elseif(APPLE)
    # macOS: rely on the OS-shipped libSystem / libc++ (present on every
    # supported macOS version). -dead_strip removes unused code.
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:-dead_strip")

elseif(UNIX)
    # Linux: statically link libgcc and libstdc++ so the binary runs on any
    # glibc-based distro from the last ~5 years. These flags work with both
    # Clang (which is required – see the workflow) and GCC.
    # glibc itself remains dynamic because its ABI is stable and a static
    # glibc is not safe to redistribute.
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Tests
# enable_testing() must be called here (the root CMakeLists.txt), not inside
# the subdirectory, for CTest to register and discover tests correctly.
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Install rules (used by CPack / GitHub Actions packaging)
# ---------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
)

# ---------------------------------------------------------------------------
# CPack – creates a simple ZIP/TGZ archive of the installed files
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_STRIP_FILES TRUE)

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
