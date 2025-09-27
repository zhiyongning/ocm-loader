#ifndef SEARCH_DATA_TO_GEOJSON_CONVERTER_HPP
#define SEARCH_DATA_TO_GEOJSON_CONVERTER_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <olp/clientmap/datastore/DataStoreClient.h>
#include <OcmMapEngine.hpp>

using json = nlohmann::json;

namespace search_converter {

 namespace datastore = olp::clientmap::datastore;

 namespace ocm = ning::maps::ocm;

class SearchDataToGeoJsonConverter {
public:
    SearchDataToGeoJsonConverter() = default;
    ~SearchDataToGeoJsonConverter() = default;


    json convert(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath);

private:


};

} // namespace search_converter

#endif // √