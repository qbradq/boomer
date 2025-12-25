cmake_minimum_required(VERSION 3.14)

project(boomer C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FORMAT_CHECK clang-format-20)

include(FetchContent)

# SDL2
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-2.30.0 # Pinning to a recent stable release
)
FetchContent_MakeAvailable(SDL2)

# stb
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# QuickJS
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(quickjs)



# Fetch miniz (zip reader)
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 2.2.0
)
FetchContent_MakeAvailable(miniz)

# Fetch Nuklear (Immediate Mode GUI)
FetchContent_Declare(
  nuklear
  GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear.git
  GIT_TAG master
)
FetchContent_MakeAvailable(nuklear)

# Create Interface Library for Nuklear (Header Only)
add_library(nuklear INTERFACE)
target_include_directories(nuklear INTERFACE ${nuklear_SOURCE_DIR})

add_executable(boomer
  src/main.c
  src/video/video.c
  src/video/texture.c
  src/render/renderer.c
  src/world/world.c
  src/core/fs.c
  src/core/script_sys.c
  src/game/entity.c
  src/editor/editor.c
)

# Include directories
target_include_directories(boomer PRIVATE 
    src 
    ${SDL2_INCLUDE_DIRS} 
    ${stb_SOURCE_DIR}
    ${quickjs_SOURCE_DIR}
    ${miniz_SOURCE_DIR}
    ${nuklear_SOURCE_DIR}
    ${nuklear_SOURCE_DIR}/demo/sdl_renderer
)

# Link libraries
target_link_libraries(boomer PRIVATE SDL2::SDL2 -lm qjs miniz)

# Copy compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
