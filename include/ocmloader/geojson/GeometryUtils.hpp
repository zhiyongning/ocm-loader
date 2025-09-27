// GeometryUtils.h
#pragma once
#include <nlohmann/json.hpp>
#include <olp/clientmap/datastore/DataStoreClient.h>

namespace utils {

nlohmann::json ExtractGeometry(
    const com::here::platform::schema::clientmap::v1::layers::common::LineString& line_string,
    const olp::geo::TileKey& kTileKey,
    uint32_t world_coordinate_bits);

}