# No More Heroes: Heroes' Paradise (Xbox 360, Japan) - ReXGlue recompiled project
#
# SDK boilerplate lives in generated/rexglue.cmake (written by "rexglue codegen").

cmake_minimum_required(VERSION 3.25)
project(nmh LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional CPU-specific build (build.ps1 -CpuArch native). Empty = generic x86-64.
set(NMH_CPU_ARCH "" CACHE STRING "-march value for game code (empty = generic)")
if(NMH_CPU_ARCH)
    add_compile_options(-march=${NMH_CPU_ARCH})
endif()

include(generated/rexglue.cmake)

set(NMH_SOURCES
    src/main.cpp
    src/fast_fma.cpp
    src/crash_log.cpp
    src/nmh_fixes.cpp
)
# Keep fma() a plain multiply-add: no contraction back into fma, no builtin substitution.
set_source_files_properties(src/fast_fma.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-builtin")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/nmh.ico")
    enable_language(RC)
    list(APPEND NMH_SOURCES src/nmh.rc)
endif()

if(WIN32)
    add_executable(nmh WIN32 ${NMH_SOURCES})
else()
    add_executable(nmh ${NMH_SOURCES})
endif()

rexglue_setup_target(nmh GPU_PLUGINS xenos)
if(WIN32)
    # crash_log.cpp: StackWalk64 for the guest-function call stack in logs\crash_*.txt
    target_link_libraries(nmh PRIVATE dbghelp)
endif()

# ---------------------------------------------------------------------------
# Launcher (separate CMake project with its own static SDL3; output next to nmh.exe).
include(ExternalProject)
set(NMH_SDL3_SOURCE_DIR "" CACHE PATH "SDL3 source for the launcher (e.g. <rexglue-sdk>/thirdparty/sdl3)")
ExternalProject_Add(nmh_launcher
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/launcher-build
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
               -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
               -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
               -DSDL3_SOURCE_DIR=${NMH_SDL3_SOURCE_DIR}
               -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$<TARGET_FILE_DIR:nmh>
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target nmh_launcher
    BUILD_ALWAYS ON
    INSTALL_COMMAND "")
add_dependencies(nmh_launcher nmh)
