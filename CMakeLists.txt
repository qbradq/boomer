cmake_minimum_required(VERSION 3.14)

project(boomer C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

# Lua 5.4
FetchContent_Declare(
  lua
  GIT_REPOSITORY https://github.com/lua/lua.git
  GIT_TAG        v5.4.6
)
FetchContent_MakeAvailable(lua)

# Define Lua library manually since the repo doesn't use CMake
if(NOT TARGET lua)
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
    list(FILTER LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
    list(FILTER LUA_SOURCES EXCLUDE REGEX "luac\\.c$")
    
    add_library(lua STATIC ${LUA_SOURCES})
    target_include_directories(lua PUBLIC "${lua_SOURCE_DIR}")
    
    if(UNIX AND NOT APPLE)
        target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
        target_link_libraries(lua PRIVATE dl)
    elseif(APPLE)
        target_compile_definitions(lua PRIVATE LUA_USE_MACOSX)
    endif()
endif()

add_executable(boomer src/main.c src/video/video.c src/render/renderer.c)

# Include directories
target_include_directories(boomer PRIVATE 
    ${stb_SOURCE_DIR} 
    ${SDL2_SOURCE_DIR}/include
    ${lua_SOURCE_DIR}
)

# Link libraries
target_link_libraries(boomer PRIVATE SDL2::SDL2 m lua)

# Copy compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
