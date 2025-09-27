#ifndef ROUTING_DATA_TO_GEOJSON_CONVERTER_HPP
#define ROUTING_DATA_TO_GEOJSON_CONVERTER_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <olp/clientmap/datastore/DataStoreClient.h>
#include <OcmMapEngine.hpp>

using json = nlohmann::json;

namespace routing_converter {

 namespace datastore = olp::clientmap::datastore;

 namespace ocm = ning::maps::ocm;

class RoutingDataToGeoJsonConverter {
public:
    RoutingDataToGeoJsonConverter() = default;
    ~RoutingDataToGeoJsonConverter() = default;


    json convert(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath);

private:


};

} // namespace routing_converter

#endif // √