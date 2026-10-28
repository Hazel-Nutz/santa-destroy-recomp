# Standalone launcher build. Uses its own static SDL3 with the 2D renderer and filesystem
# modules enabled (the ReXGlue SDK builds SDL3 without them).
cmake_minimum_required(VERSION 3.25)
project(santadestroy_launcher LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL3_SOURCE_DIR "" CACHE PATH "SDL3 source tree (e.g. <rexglue-sdk>/thirdparty/sdl3). Empty = download SDL 3.2.10")
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_RENDER ON CACHE BOOL "" FORCE)
set(SDL_FILESYSTEM ON CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
if(SDL3_SOURCE_DIR)
  add_subdirectory(${SDL3_SOURCE_DIR} sdl3 EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.2.10.tar.gz)
  FetchContent_MakeAvailable(SDL3)
endif()

add_executable(santadestroy_launcher WIN32
    launcher_main.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_sdlrenderer3.cpp)
if(WIN32)
    enable_language(RC)
    target_sources(santadestroy_launcher PRIVATE santadestroy.rc)
endif()
target_include_directories(santadestroy_launcher PRIVATE . imgui imgui/backends)
target_link_libraries(santadestroy_launcher PRIVATE SDL3::SDL3-static)
