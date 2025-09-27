// GeometryUtils.cpp
#include "GeometryUtils.hpp"

namespace utils {

using json = nlohmann::json;

json ExtractGeometry(
    const com::here::platform::schema::clientmap::v1::layers::common::LineString& line_string,
    const olp::geo::TileKey& kTileKey,
    uint32_t world_coordinate_bits,
    json geometry) 
{
    int level = 14;
    //json geometry;
    geometry["type"] = "LineString";
    geometry["coordinates"] = json::array();

    int num_coords = line_string.xy_coords_size();
    for (int j = 0; j < num_coords; j += 2) {
        if (j + 1 >= num_coords) break;

        uint32_t x_coord = line_string.xy_coords(j);
        uint32_t y_coord = line_string.xy_coords(j + 1);

        uint32_t I_LNG_TILE = kTileKey.Column() << (world_coordinate_bits - level);
        uint32_t I_LNG = I_LNG_TILE + x_coord;
        double LNG = (I_LNG * 360.0) / (1 << world_coordinate_bits) - 180.0;

        uint32_t I_LAT_TILE = kTileKey.Row() << (world_coordinate_bits - level);
        uint32_t I_LAT = I_LAT_TILE + y_coord;
        double LAT = (I_LAT * 360.0) / (1 << world_coordinate_bits) - 90.0;

        geometry["coordinates"].push_back({ LNG, LAT });
    }

    return geometry;
}

} // namespace utils