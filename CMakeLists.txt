cmake_minimum_required(VERSION 3.15)
project(ocmloader VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version" FORCE)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 
include_directories(
    "${PROJECT_SOURCE_DIR}/include/ocmloader/geojson"
    "${PROJECT_SOURCE_DIR}/include/ocmloader"
    "/opt/homebrew/Cellar/nlohmann-json/3.12.0/include"
    "${PROJECT_SOURCE_DIR}/../../mydev/ocm-access-manager-cpp/build/final-install-image/include"
    )



add_subdirectory(src/geojson)
add_subdirectory(src/core)
add_subdirectory(src/apps)

