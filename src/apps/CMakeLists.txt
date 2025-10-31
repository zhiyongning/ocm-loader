set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


find_package(here.ocm-access-manager-cpp REQUIRED)


set(OCM_LOADER_SOURCES
    OCMLoader.cpp
)


add_executable(ocm-loader ${OCM_LOADER_SOURCES})
target_link_libraries(ocm-loader PRIVATE  ocmloader-geojson)

add_executable(mytest MyTest.cpp)
target_link_libraries(mytest PRIVATE  ocmloader-geojson)


