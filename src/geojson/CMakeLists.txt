add_library(ocmloader-geojson
    GeometryUtils.cpp
    ISADataToGeoJsonConverter.cpp
    RoadDataToGeoJsonConverter.cpp
    RoutingDataToGeoJsonConverter.cpp
    SearchDataToGeoJsonConverter.cpp
    CommonDataConverter.cpp
    TimeDomainParser.cpp
)


target_link_libraries(ocmloader-geojson PUBLIC ocmloader-core)
