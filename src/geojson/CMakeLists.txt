add_library(ocmloader-geojson
    GeometryUtils.cpp
    ISADataToGeoJsonConverter.cpp
    RoadDataToGeoJsonConverter.cpp
    RoutingDataToGeoJsonConverter.cpp
    SearchDataToGeoJsonConverter.cpp
    TimeDomainParser.cpp
)

target_include_directories(ocmloader-geojson PUBLIC
    here::ocm-access-manager-cpp
)


target_link_libraries(ocmloader-geojson PUBLIC ocmloader-core)
