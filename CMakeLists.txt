cmake_minimum_required(VERSION 3.14)

project(boomer C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FORMAT_CHECK clang-format-20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
configure_file(.clangd.in ${CMAKE_SOURCE_DIR}/.clangd @ONLY)

include(FetchContent)

# Raylib 5.5
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.5
)
FetchContent_MakeAvailable(raylib)

# QuickJS
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(quickjs)

# Miniz
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 2.2.0
)
FetchContent_MakeAvailable(miniz)

# Raygui 4.0
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
)
FetchContent_MakeAvailable(raygui)

add_executable(boomer
  src/main.c
  src/video/video.c
  src/video/texture.c
  src/render/renderer.c
  src/world/world.c
  src/world/map_loader.c
  src/core/fs.c
  src/core/script_sys.c
  src/core/config.c
  src/game/entity.c
  src/editor/editor.c
  src/ui/console.c
  src/ui/gui_impl.c
)

# Include directories
target_include_directories(boomer PRIVATE 
    src 
    ${quickjs_SOURCE_DIR}
    ${miniz_SOURCE_DIR}
    ${raygui_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(boomer PRIVATE raylib qjs miniz -lm)

if(EMSCRIPTEN)
    set_target_properties(boomer PROPERTIES SUFFIX ".html")
    # Emscripten flags for Raylib
    target_link_options(boomer PRIVATE 
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        --shell-file "${CMAKE_SOURCE_DIR}/src/wasm_shell.html"
        --preload-file "${CMAKE_SOURCE_DIR}/games/demo@games/demo"
        -lidbfs.js
    )
endif()