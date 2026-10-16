cmake_minimum_required(VERSION 3.15)
project(CarGame3D CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(CarGame3D
    src/main.cpp
    src/AiTraffic.cpp
    src/Camera.cpp
    src/Car.cpp
    src/CarBody.cpp
    src/CarDatabase.cpp
    src/Engine.cpp
    src/Fence.cpp
    src/FileAssoc.cpp
    src/Gearbox.cpp
    src/Input.cpp
    src/Lighting.cpp
    src/Menu.cpp
    src/MeshBuilder.cpp
    src/Road.cpp
    src/Settings.cpp
    src/SkidMarks.cpp
    src/Terrain.cpp
    src/MapFile.cpp
    src/Track.cpp
    src/Upgrades.cpp
    src/Water.cpp
    src/World.cpp
)

target_link_libraries(CarGame3D PRIVATE raylib)

if (WIN32)
    target_link_libraries(CarGame3D PRIVATE winmm)
endif()

add_custom_command(TARGET CarGame3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/shaders
        $<TARGET_FILE_DIR:CarGame3D>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:CarGame3D>/assets
)
