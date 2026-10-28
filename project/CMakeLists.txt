# Santa Destroy Recomp (Xbox 360, Japan) - ReXGlue recompiled project
#
# SDK boilerplate lives in generated/rexglue.cmake (written by "rexglue codegen").

cmake_minimum_required(VERSION 3.25)
project(santadestroy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional CPU-specific build (build.ps1 -CpuArch native). Empty = generic x86-64.
set(SDR_CPU_ARCH "" CACHE STRING "-march value for game code (empty = generic)")
if(SDR_CPU_ARCH)
    add_compile_options(-march=${SDR_CPU_ARCH})
endif()

include(generated/rexglue.cmake)

set(SDR_SOURCES
    src/main.cpp
    src/fast_fma.cpp
    src/crash_log.cpp
    src/santadestroy_fixes.cpp
)
# Keep fma() a plain multiply-add: no contraction back into fma, no builtin substitution.
set_source_files_properties(src/fast_fma.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-builtin")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/santadestroy.ico")
    enable_language(RC)
    list(APPEND SDR_SOURCES src/santadestroy.rc)
endif()

if(WIN32)
    add_executable(santadestroy WIN32 ${SDR_SOURCES})
else()
    add_executable(santadestroy ${SDR_SOURCES})
endif()

rexglue_setup_target(santadestroy GPU_PLUGINS xenos)
if(WIN32)
    # crash_log.cpp: StackWalk64 for the guest-function call stack in logs\crash_*.txt
    target_link_libraries(santadestroy PRIVATE dbghelp)
endif()

# ---------------------------------------------------------------------------
# Launcher (separate CMake project with its own static SDL3; output next to santadestroy.exe).
include(ExternalProject)
set(SDR_SDL3_SOURCE_DIR "" CACHE PATH "SDL3 source for the launcher (e.g. <rexglue-sdk>/thirdparty/sdl3)")
ExternalProject_Add(santadestroy_launcher
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/launcher-build
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
               -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
               -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
               -DSDL3_SOURCE_DIR=${SDR_SDL3_SOURCE_DIR}
               -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$<TARGET_FILE_DIR:santadestroy>
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target santadestroy_launcher
    BUILD_ALWAYS ON
    INSTALL_COMMAND "")
add_dependencies(santadestroy_launcher santadestroy)
