cmake_minimum_required(VERSION 3.15)
project(ocmloader VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version" FORCE)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 
include_directories(
    "${PROJECT_SOURCE_DIR}/include/ocmloader/geojson"
    "${PROJECT_SOURCE_DIR}/include/ocmloader"
    "/opt/homebrew/Cellar/nlohmann-json/3.12.0/include" #MacBook
    #"${PROJECT_SOURCE_DIR}/.."                         #Windows, ensure "nlohmann/json.hpp" in this folder
    )



add_subdirectory(src/geojson)
add_subdirectory(src/core)
add_subdirectory(src/apps)


# Key Notes to build in Windows
#   
#.  1. Delete "build" folder
#.  2. Update CMakeLists.txt in current and core folter to corresponding folder
#   3.1. Executing command: C:\Users\zning\Downloads\cmake-4.1.0-rc1-windows-x86_64\bin\cmake.EXE 
#.     -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE --no-warn-unused-cli -S C:/Users/zning/ocm-loader 
#      -B c:/Users/zning/ocm-loader/build -G "Visual Studio 17 2022" -T host=x86 -A x64
#.  3.2 Visual Studio > Cmake > Configure > Choose "x86_x64" kit and "release"
#.  4.1. [proc] Executing command: C:\Users\zning\Downloads\cmake-4.1.0-rc1-windows-x86_64\bin\cmake.EXE 
#         --build c:/Users/zning/ocm-loader/build --config Release --target ALL_BUILD -j 8 --
#.  4.2 Visual Studio > Cmake > Configure > Choose Release

