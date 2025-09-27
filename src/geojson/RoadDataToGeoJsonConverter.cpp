#include "RoadDataToGeoJsonConverter.hpp"
#include <olp/core/logging/Log.h>
#include <fstream>
#include <cmath>
#include <sstream>
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/SegmentAttributeLayer.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/AccessBitMask.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/SpecialSpeedSituation.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/AccessBitMask.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/FunctionalClass.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/RelativeDirection.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/SpeedCategory.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/IntersectionCategory.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/RoadDivider.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/RouteLevel.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/SpecialTrafficAreaCategory.pb.h"
#include "com/here/platform/schema/clientmap/v1/layers/common/RoadUsageBitMask.pb.h"
#include "com/here/platform/schema/clientmap/v1/layers/common/LocalRoadBitMask.pb.h"

// 如果你需要 boost::algorithm::join，取消下面注释并添加到 cmake/link libs
// #include <boost/algorithm/string/join.hpp>

namespace road_converter {

namespace datastore = olp::clientmap::datastore;
namespace ocm = ning::maps::ocm;
using json = nlohmann::json;
using namespace com::here::platform::schema::clientmap::v1::layers::common;
constexpr auto kLogTag = "RoadDataToGeoJsonConverter";


void validate_layer_sizes(
    const clientmap::decoder::RoadLayer& road_layer,
    const clientmap::decoder::RoadNameLayer& road_name_layer,
    const clientmap::decoder::RoadGeometryLayer& road_geom_layer,
    const clientmap::decoder::RoadAttributeLayer& road_attr_layer) 
{
    const size_t num_roads = static_cast<size_t>(road_layer.roads_size());
    if (num_roads != static_cast<size_t>(road_name_layer.roads_size()) ||
        num_roads != static_cast<size_t>(road_geom_layer.roads_size()) ||
        num_roads != static_cast<size_t>(road_attr_layer.roads_size())) {
        std::ostringstream oss;
        oss << "Layer road count mismatch: RoadLayer=" << num_roads
            << ", RoadNameLayer=" << road_name_layer.roads_size()
            << ", RoadGeometryLayer=" << road_geom_layer.roads_size()
            << ", RoadAttributeLayer=" << road_attr_layer.roads_size();
        throw std::runtime_error(oss.str());
    }
}
json extract_geometry(
    const com::here::platform::schema::clientmap::v1::layers::common::LineString& line_string, 
    const olp::geo::TileKey& kTileKey,
    uint32_t world_coordinate_bits) 
{
    int level = 14;
    json geometry;
    geometry["type"] = "LineString";
    geometry["coordinates"] = json::array();

    int num_coords = line_string.xy_coords_size();
    // 格式化 xy_coords 为 (Latitude, Longitude), (Latitude, Longitude),... 的字符串
    std::stringstream coords_stream;
    for (int j = 0; j < num_coords; j += 2) {  // 每次处理两个坐标（经度和纬度）
        // 检查是否越界（避免 j+1 超过数组范围）
        if (j + 1 >= num_coords) break;

        // 获取当前点的经度（x）和纬度（y）的坐标值（uint32）
        uint32_t x_coord = line_string.xy_coords(j);   // 经度（第 j 个元素）
        uint32_t y_coord = line_string.xy_coords(j + 1); // 纬度（第 j+1 个元素）

        // 计算经度（LNG）的实际值（度）
        uint32_t I_LNG_TILE = kTileKey.Column() << (world_coordinate_bits - level);
        uint32_t I_LNG = I_LNG_TILE + x_coord;
        double LNG = (I_LNG * 360.0) / (1 << world_coordinate_bits) - 180.0;

        // 计算纬度（LAT）的实际值（度）
        uint32_t I_LAT_TILE = kTileKey.Row() << (world_coordinate_bits - level);
        uint32_t I_LAT = I_LAT_TILE + y_coord;
        double LAT = (I_LAT * 360.0) / (1 << world_coordinate_bits) - 90.0;


        geometry["coordinates"].push_back({ LNG, LAT });
    }

    return geometry;
}
json convert_local_road_bitmask(uint32_t bitmask) {
    json j = json::array();

    if (bitmask & LOCAL_ROAD_EMPTY)       j.push_back("LOCAL_ROAD_EMPTY");
    if (bitmask & FRONTAGE)               j.push_back("FRONTAGE");
    if (bitmask & PARKING_LOT_ROAD)       j.push_back("PARKING_LOT_ROAD");
    if (bitmask & POI_ACCESS)             j.push_back("POI_ACCESS");

    return j;
}
// 将 bitmask 转为 JSON 数组，列出所有 active 的标记名称
json convert_road_usage_bitmask(uint32_t bitmask) {
    json j = json::array();

    if (bitmask & ROAD_USAGE_EMPTY)        j.push_back("ROAD_USAGE_EMPTY");
    if (bitmask & CARPOOL_ROAD)           j.push_back("CARPOOL_ROAD");
    if (bitmask & CONTROLLED_ACCESS)      j.push_back("CONTROLLED_ACCESS");
    if (bitmask & EXPRESS_LANE)           j.push_back("EXPRESS_LANE");
    if (bitmask & LIMITED_ACCESS)         j.push_back("LIMITED_ACCESS");
    if (bitmask & PRIORITY_ROAD)          j.push_back("PRIORITY_ROAD");
    if (bitmask & RAMP)                   j.push_back("RAMP");
    if (bitmask & REVERSIBLE)             j.push_back("REVERSIBLE");
    if (bitmask & TOLLWAY)                j.push_back("TOLLWAY");
    if (bitmask & DIMINISHED_PRIORITY)    j.push_back("DIMINISHED_PRIORITY");
    if (bitmask & PUBLIC_ACCESS)          j.push_back("PUBLIC_ACCESS");

    return j;
}
enum PhysicalBitMask {
    PHYSICAL_EMPTY = 0,
    BOAT_FERRY = 1 << 0, // 1
    BRIDGE = 1 << 1, //2
    MULTIPLY_DIGITIZED = 1 << 2, // 3
    PAVED = 1 << 3, // 4
    PRIVATE = 1 << 4, // 5
    RAIL_FERRY = 1 << 5, // 6
    TUNNEL = 1 << 6, // 7
    DELIVERY_ROAD = 1 << 7, // 8
    MOVABLE_BRIDGE = 1 << 8 //9
};

// Bitmask 转字符串函数
std::string PhisicalBitmaskToString(uint32_t bitmask) {
    if (bitmask == PHYSICAL_EMPTY) {
        return "PHYSICAL_EMPTY";
    }

    std::vector<std::string> parts;

    if (bitmask & BOAT_FERRY) {
        parts.emplace_back("BOAT_FERRY");
    }
    if (bitmask & BRIDGE) {
        parts.emplace_back("BRIDGE");
    }
        if (bitmask & MULTIPLY_DIGITIZED) {
        parts.emplace_back("MULTIPLY_DIGITIZED");
    }
    if (bitmask & PAVED) {
        parts.emplace_back("PAVED");
    }

    if (bitmask & PRIVATE) {
        parts.emplace_back("PRIVATE");
    }
    if (bitmask & RAIL_FERRY) {
        parts.emplace_back("RAIL_FERRY");
    }
        if (bitmask & TUNNEL) {
        parts.emplace_back("TUNNEL");
    }
    if (bitmask & DELIVERY_ROAD) {
        parts.emplace_back("DELIVERY_ROAD");
    }
        if (bitmask & MOVABLE_BRIDGE) {
        parts.emplace_back("MOVABLE_BRIDGE");
    }
    // 拼接成逗号分隔的字符串
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parts[i];
    }
    std::string ossstr = oss.str();
    return ossstr;
}

json bitmaskToJson(uint32_t mask) {
    json arr = json::array();
    if (mask & AccessBitMask::AUTOMOBILES) arr.push_back("AUTOMOBILES");
    if (mask & AccessBitMask::BUSES) arr.push_back("BUSES");
    if (mask & AccessBitMask::TRUCKS) arr.push_back("TRUCKS");
    if (mask & AccessBitMask::PEDESTRIANS) arr.push_back("PEDESTRIANS");
    if (mask & AccessBitMask::MOTORCYCLES) arr.push_back("MOTORCYCLES");
    // ... 其他 bitmask
    return arr;
}

static uint32_t GetWorldCoordinateBits(const datastore::LayerLoadResult &layer_result)
{
    // 这里示例性读取 layer configuration 中的 world_coordinate_bits 字段
    // 根据你的 SDK，可能需要调整成员名或访问方法
    const clientmap::decoder::LayerConfigurationPtr layer_configuration = layer_result.GetConfiguration();
    if (layer_configuration)
    {
        return layer_configuration->world_coordinate_bits();
    }
    return 0;
}
json convert_attribute(const clientmap::decoder::RoadAttributeLayer::Attributes& attr) {
    json a;
    // 下面按存在的方法调用：如果你的 proto 生成类方法名不同，请按实际修改

    // 如果 PolylineOffset 有 value() 或者 to_string，这里需要调整
    if (attr.has_start_offset())
    {
        const auto& polyline_offset = attr.start_offset();
        uint32_t index = polyline_offset.shape_point_index();
        uint32_t ratio = polyline_offset.shape_point_ratio();
            json start_offset_json;
        start_offset_json["shape_point_index"] = index;
        start_offset_json["shape_point_ratio"] = ratio;
        a["start_offset"] = start_offset_json;
    }

    //if (attr.has_start_offset()) a["start_offset"] = attr.start_offset().SerializeAsString();

    // travel_direction 可能是枚举
    a["access"] = bitmaskToJson(attr.access());
    a["functional_class"] = FunctionalClass_Name(attr.functional_class());
    a["travel_direction"] = RelativeDirection_Name(attr.travel_direction());
    a["physical"] = PhisicalBitmaskToString(attr.physical());
    a["road_usage"] = convert_road_usage_bitmask(attr.road_usage());
    a["under_construction"] = attr.under_construction();
    // a["country_code"] = attr.country_code().code(); // 若存在
    a["z_level"] = attr.z_level();
    a["state_code"] = attr.state_code();
    a["has_polygonal_geometry"] = attr.has_polygonal_geometry();
    a["local_road"] = convert_local_road_bitmask(attr.local_road());
    // 出于稳定性考虑，如果 originating_road_attributes 是 message，则序列化为 string
    if (attr.has_originating_road_attributes()) {
        std::string tmp;
       // attr.originating_road_attributes().SerializeToString(&tmp);
       // a["originating_road_attributes"] = tmp;
    }
    a["truck_toll"] = attr.truck_toll();
    a["min_zoom_level"] = attr.min_zoom_level();
    a["administrative_road_context_id"] = attr.administrative_road_context_id();
    return a;
}

json convertInternal(
    const clientmap::decoder::RoadLayer& road_layer,
    const clientmap::decoder::RoadNameLayer& road_name_layer,
    const clientmap::decoder::RoadGeometryLayer& road_geom_layer,
    const clientmap::decoder::RoadAttributeLayer& road_attr_layer,
    const olp::geo::TileKey& tile_key,
    const std::string& output_path,
    uint32_t world_coordinate_bits)
{
    // 校验
    validate_layer_sizes(road_layer, road_name_layer, road_geom_layer, road_attr_layer);

    OLP_SDK_LOG_INFO(kLogTag, "开始转换 GeoJson...");

    json feature_collection;
    feature_collection["type"] = "FeatureCollection";
    feature_collection["features"] = json::array();

    const size_t num_roads = static_cast<size_t>(road_layer.roads_size());
    //feature_collection["metadata"]["num_roads"] = num_roads;

    for (size_t i = 0; i < num_roads; ++i) {
        const auto& road_proto = road_layer.roads(static_cast<int>(i));
        const auto& name_road = road_name_layer.roads(static_cast<int>(i));
        const auto& geom_road = road_geom_layer.roads(static_cast<int>(i));
        const auto& attr_road = road_attr_layer.roads(static_cast<int>(i));

        json properties;
        properties["local_id"] = road_proto.local_id();

        // 街道名称
        json street_names = json::array();
        for (const auto& street_name : name_road.street_names()) {
            json sn;
            // 有些生成的类里可能没有 language()/full_name()，按实际调整
            sn["language"]  = street_name.language();
            sn["full_name"] = street_name.full_name();
            // 可以添加 splitting 信息等
            street_names.push_back(sn);
        }
        properties["street_names"] = street_names;

        // 路线编号
        json route_numbers = json::array();
        for (const auto& route_num : name_road.route_numbers()) {
            json rn;
            rn["language"] = route_num.language();
            rn["number"] = route_num.number();
           // if (route_num.has_shield_text()) rn["shield_text"] = route_num.shield_text();
            // 如果需要级别转字符串，可在此处添加
            //route_numbers.push_back(rn);
        }
        properties["route_numbers"] = route_numbers;

        // 道路属性
        json attributes = json::array();
        for (const auto& attr : attr_road.attributes()) {
            attributes.push_back(convert_attribute(attr));
        }
        properties["attributes"] = attributes;

        // 构建 Feature
        json feature;
        feature["type"] = "Feature";

        // geometry: 使用 geom_road.geometry()
        // 这里假设 geom_road.geometry() 返回 com::here::platform::schema::clientmap::v1::layers::common::LineString
        feature["geometry"] = extract_geometry(geom_road.geometry(), tile_key, world_coordinate_bits);
        feature["properties"] = properties;

        feature_collection["features"].push_back(feature);
    }

    OLP_SDK_LOG_INFO(kLogTag, "完成转换 GeoJson...");
    return feature_collection;

}

json RoadDataToGeoJsonConverter::convert(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath)
{
    const auto& layer_results = response.GetResult().GetLayersResults();


    auto find_layer_result = [&](const std::string& name) -> const datastore::LayerLoadResult& {
        auto it = std::find_if(layer_results.begin(), layer_results.end(),
            [&](const auto& item) { return item.GetPayload().layer_name == name; });
        if (it == layer_results.end()) {
            throw std::runtime_error("Required layer not found: " + name);
        }
        return it->GetResult();
    };

    // 注意：这些 k*LayerName 常量在你的 clientmap::rendering 中定义
    const datastore::LayerLoadResult &road_layer_result = find_layer_result(clientmap::rendering::kRoadLayerName);
    const datastore::LayerLoadResult &road_attribute_layer_result = find_layer_result(clientmap::rendering::kRoadAttributeLayerName);
    const datastore::LayerLoadResult &road_geometry_layer_result = find_layer_result(clientmap::rendering::kRoadGeometryLayerName);
    const datastore::LayerLoadResult &road_name_layer_result = find_layer_result(clientmap::rendering::kRoadNameLayerName);

    // 将 LayerLoadResult 转换为 decoder 类型指针
    const clientmap::decoder::RoadLayer* road_layer_data = road_layer_result.Cast<clientmap::decoder::RoadLayer>();
    const clientmap::decoder::RoadAttributeLayer* road_attribute_layer_data = road_attribute_layer_result.Cast<clientmap::decoder::RoadAttributeLayer>();
    const clientmap::decoder::RoadGeometryLayer* road_geometry_layer_data = road_geometry_layer_result.Cast<clientmap::decoder::RoadGeometryLayer>();
    const clientmap::decoder::RoadNameLayer* road_name_layer_data = road_name_layer_result.Cast<clientmap::decoder::RoadNameLayer>();

    if (!road_layer_data || !road_attribute_layer_data || !road_geometry_layer_data || !road_name_layer_data) {
        throw std::runtime_error("Failed to cast one or more layer data pointers.");
    }

    uint32_t world_coordinate_bits = GetWorldCoordinateBits(road_geometry_layer_result);

    // 调用内部转换
    return convertInternal(*road_layer_data, *road_name_layer_data, *road_geometry_layer_data, *road_attribute_layer_data, 
        tile_key, outPath, world_coordinate_bits);
}








} // namespace road_converter