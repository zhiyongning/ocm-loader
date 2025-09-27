#include "RoutingDataToGeoJsonConverter.hpp"
#include "TimeDomainParser.hpp"
#include "TileIDConverter.hpp"
#include <olp/core/logging/Log.h>
#include <fstream>
#include <cmath>
#include <sstream>
#include <LayerFinder.hpp>
#include "GeometryUtils.hpp" 
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
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/RoadUsageBitMask.pb.h"
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/common/TimeDomainDescription.pb.h"
#include <unordered_map>

 #include <boost/algorithm/string/join.hpp>



namespace routing_converter {

namespace ocm = ning::maps::ocm;
namespace layers = com::here::platform::schema::clientmap::v1::layers;
using json = nlohmann::json;
using namespace com::here::platform::schema::clientmap::v1::layers::common;


struct CoordHash {
    std::size_t operator()(uint64_t key) const {
        return std::hash<uint64_t>()(key);
    }
};

using CoordKey = uint64_t;

CoordKey makeCoordKey(uint32_t x, uint32_t y) {
    return (static_cast<uint64_t>(x) << 32) | y;
}

std::unordered_map<CoordKey, const layers::NodeLayer::Node*> buildNodeMap(
    const layers::NodeLayer& layer)
{
    std::unordered_map<CoordKey, const layers::NodeLayer::Node*> nodeMap;
    for (const auto& node : layer.nodes()) {
        CoordKey key = makeCoordKey(node.x(), node.y());
        nodeMap[key] = &node;
        /*
                std::cout << "Node key: " << key 
                  << " (x=" << node.x() << ", y=" << node.y() << ")"
                  << " ptr=" << &node << std::endl;
                  */
    }
    return nodeMap;
}

const layers::NodeLayer::Segment* getSegmentByIndex(const layers::NodeLayer& layer, size_t index) {
    if (index < layer.segments_size()) {
        return &layer.segments(index);
    }
    return nullptr;
}

static constexpr uint32_t LOCAL_ROAD_EMPTY      = 0;
static constexpr uint32_t FRONTAGE              = 1u << 0; // 1
static constexpr uint32_t PARKING_LOT_ROAD      = 1u << 1; // 2
static constexpr uint32_t POI_ACCESS            = 1u << 2; // 4


// bitmask -> json
inline json LocalRoadBitmaskToJson(uint32_t mask) {
    json j;
    j["raw"] = mask;

    // active names 数组（便于人读）
    json active = json::array();
    if (mask & FRONTAGE) active.push_back("FRONTAGE");
    if (mask & PARKING_LOT_ROAD) active.push_back("PARKING_LOT_ROAD");
    if (mask & POI_ACCESS) active.push_back("POI_ACCESS");
    j["active"] = active;

    // 是否为空（所有位都为 0）
    j["is_empty"] = (mask == LOCAL_ROAD_EMPTY);

    return j;
}

inline json RoadUsageBitMaskToJson(uint32_t mask) {
    static const struct { uint32_t bit; const char* name; } kFlags[] = {
        {1,   "CARPOOL_ROAD"},
        {2,   "CONTROLLED_ACCESS"},
        {4,   "EXPRESS_LANE"},
        {8,   "LIMITED_ACCESS"},
        {16,  "PRIORITY_ROAD"},
        {32,  "RAMP"},
        {64,  "REVERSIBLE"},
        {128, "TOLLWAY"},
        {256, "DIMINISHED_PRIORITY"},
        {512, "PUBLIC_ACCESS"}
    };

    json j;
    j["raw"] = mask;
   // j["flags"] = json::object();
    json active = json::array();

    for (const auto& f : kFlags) {
        bool set = (mask & f.bit) != 0;
       // j["flags"][f.name] = set;
        if (set) active.push_back(f.name);
    }

    j["active"] = active;
    j["is_empty"] = (mask == 0);
    return j;
}


inline json PhysicalBitMaskToJson(uint32_t mask) {
    static const struct { uint32_t bit; const char* name; } kFlags[] = {
        {1,  "BOAT_FERRY"},
        {2,  "BRIDGE"},
        {4,  "MULTIPLY_DIGITIZED"},
        {8,  "PAVED"},
        {16, "PRIVATE"},
        {32, "RAIL_FERRY"},
        {64, "TUNNEL"},
        {128,"DELIVERY_ROAD"},
        {256,"MOVABLE_BRIDGE"}
    };

    json j;
    j["raw"] = mask;
   // j["flags"] = json::object();
    json active = json::array();

    for (const auto& f : kFlags) {
        bool set = (mask & f.bit) != 0;
       // j["flags"][f.name] = set;
        if (set) active.push_back(f.name);
    }

    j["active"] = active;
    j["is_empty"] = (mask == 0);
    return j;
}

// 小工具：bitmask 转数组
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

// 转换 TimedAccess
json convert_timed_access(const TimedAccess& t) {
    json j;
    j["applies_to"] = bitmaskToJson(t.applies_to());

    // 2. applies_during (直接是 string 列表)
    j["applies_during"] = json::array();
    for (const auto& t : t.applies_during()) {
        j["applies_during"].push_back(t);
    }

    // 3. seasonal_applies_during
    j["seasonal_applies_during"] = json::array();
    for (const auto& t : t.seasonal_applies_during()) {
        j["seasonal_applies_during"].push_back(t);
    }

    return j;
}

// 转换 SpecialSpeedSituation
json convert_special_speed(const SpecialSpeedSituation& s) {
    json j;
    j["special_speed_type"] = SpecialSpeedSituation::SpecialSpeedType_Name(s.special_speed_type());
    j["special_speed_limit"] = s.special_speed_limit();
    j["applies_during"] = s.applies_during();
    const auto& periods = s.applies_during();
    /* Human readable TimeDomain (If needed, remove comment)
    json active = json::array();
    for (const auto& p : periods) {
        active.push_back(TimeDomainParser::Parser::TimeDomainToReadable(p));
    }

    j["applies_during_readable"] = active;
    */
   
    return j;
}


json convertInternal(
    const clientmap::decoder::SegmentLayer& segmentLayer,
    const clientmap::decoder::SegmentAttributeLayer& attributeLayer, 
    const clientmap::decoder::SegmentIdMappingLayer& segIdMapLayer,
    const olp::geo::TileKey& tile_key,
    const std::string& output_path
);


constexpr auto kLogTag = "RoutingDataToGeoJsonConverter";

//const google::protobuf::EnumDescriptor* desc = layers::AdministrativeRoutingContextLayer_LegalRequirements_ParkingSideBitMask_descriptor();



// 辅助函数：将枚举转换为字符串

inline json SpeedLimitRainingBitMaskToJson(uint32_t mask) {
    // ⚠️ 这里要根据 proto 实际枚举值补全
    static const struct { uint32_t bit; const char* name; } kFlags[] = {
        {1, "MAX_SPEED_RAINING_EMPTY"},
        {2, "MOTORWAYS_AND_CONTROLLED_ACCESS_ROADS_LIMIT_110_KMH"}
        // ... 继续补充其它枚举值
    };

    json j;
    j["raw"] = mask;
   // j["flags"] = json::object();
    json active = json::array();

    for (const auto& f : kFlags) {
        bool set = (mask & f.bit) != 0;
     //   j["flags"][f.name] = set;
        if (set) active.push_back(f.name);
    }

    j["active"] = active;
    j["is_empty"] = (mask == 0);
    return j;
}

// 对应 proto 中的 ParkingSideBitMask

inline json ParkingSideBitMaskToJson(uint32_t mask) {
    static const struct { uint32_t bit; const char* name; } kFlags[] = {
        {1, "BOTH_SIDES_ONE_WAY_ROAD"},
        {2, "OPPOSITE_LANE_TWO_WAY_ROAD"}
    };

    json j;
    j["raw"] = mask;
    //j["flags"] = json::object();
    json active = json::array();

    for (const auto& f : kFlags) {
        bool set = (mask & f.bit) != 0;
      //  j["flags"][f.name] = set;
        if (set) active.push_back(f.name);
    }

    j["active"] = active;
    j["is_empty"] = (mask == 0);
    return j;
}

json convert_usage_fee(const UsageFeeRequired& u) {
    json j;
    j["toll_feature_type"] = UsageFeeRequired::TollFeatureType_Name(u.toll_feature_type());
    j["relative_direction"] = RelativeDirection_Name(u.relative_direction());
    j["applies_to"] = bitmaskToJson(u.applies_to());

    // 时间限制（可能为空）
    json arr = json::array();
    for (const auto& s : u.applies_during()) arr.push_back(s);
    j["applies_during"] = arr;

    j["toll_system_id"] = u.toll_system_id();
    return j;
}

std::string builtUpAreaToString(BuiltUpArea b) {
    switch (b) {
        case BUILT_UP_AREA_UNKNOWN: return "BUILT_UP_AREA_UNKNOWN";
        case BUILT_UP_AREA_YES: return "BUILT_UP_AREA_YES";
        case BUILT_UP_AREA_NO: return "BUILT_UP_AREA_NO";
        case BUILT_UP_AREA_YES_VERIFIED: return "BUILT_UP_AREA_YES_VERIFIED";
        case BUILT_UP_AREA_NO_VERIFIED: return "BUILT_UP_AREA_NO_VERIFIED";
        default: return "UNKNOWN";
    }
}

json convert_env_zone(const EnvironmentalZoneCondition& e) {
    json j;
    j["environmental_zone_id"] = e.environmental_zone_id();
    j["applies_to"] = bitmaskToJson(e.applies_to());
    return j;
}

json RoutingDataToGeoJsonConverter::convert(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath)
{


    const auto& layer_results = response.GetResult().GetLayersResults();

    ocm::LayerFinder finder(layer_results);


    const auto& segmentLayer = finder.GetRequiredLayer<clientmap::decoder::SegmentLayer>(clientmap::routing::kSegmentLayerName);
    const auto* segmenAttributetLayer = finder.TryGetLayer<clientmap::decoder::SegmentAttributeLayer>(clientmap::routing::kSegmentAttributeLayerName);   
    const auto* segIdMapLayer = finder.TryGetLayer<clientmap::decoder::SegmentIdMappingLayer>(clientmap::interop::kSegmentIdMappingLayerName);

    return convertInternal(segmentLayer, *segmenAttributetLayer,*segIdMapLayer, tile_key, outPath);


}

json convert_attribute(const clientmap::decoder::SegmentAttributeLayer::Attributes& attr) {
    json a;
    a["start_offset"] = attr.start_offset();
    /*
    a["access"] = bitmaskToJson(attr.access());
    a["physical"] = PhysicalBitMaskToJson(attr.physical());
    a["local_road"] = LocalRoadBitmaskToJson(attr.local_road());
    a["road_usage"] = RoadUsageBitMaskToJson(attr.road_usage());
    a["forward_speed_limit"] = attr.forward_speed_limit();
    a["forward_speed_limit_unlimited"] = attr.forward_speed_limit_unlimited();
    a["backward_speed_limit"] = attr.backward_speed_limit();
    a["backward_speed_limit_unlimited"] = attr.backward_speed_limit_unlimited();
    a["forward_free_flow_speed"] = attr.forward_free_flow_speed();
    a["backward_free_flow_speed"] = attr.backward_free_flow_speed();
    a["special_traffic_area_category"] = attr.special_traffic_area_category();
    a["intersection_category"] = attr.intersection_category();
    a["administrative_routing_context_id"] = attr.administrative_routing_context_id();
    a["built_up_area"] = attr.built_up_area();
    a["urban"] = attr.urban();
    a["forward_through_lane_count"] = attr.forward_through_lane_count();
    a["backward_through_lane_count"] = attr.backward_through_lane_count();
    a["forward_speed_limit_source"] = attr.forward_speed_limit_source();
    a["backward_speed_limit_source"] = attr.backward_speed_limit_source();
    a["forward_variable_speed_limit"] = attr.forward_variable_speed_limit();
    a["backward_variable_speed_limit"] = attr.backward_variable_speed_limit();
    a["rest_area"] = attr.rest_area();
*/

    // 枚举字段
    a["functional_class"] = FunctionalClass_Name(attr.functional_class());

    /*
    a["travel_direction"] = RelativeDirection_Name(attr.travel_direction());
    a["speed_category"] = SpeedCategory_Name(attr.speed_category());
    a["intersection_category"] = IntersectionCategory_Name(attr.intersection_category());
    a["road_divider"] = RoadDivider_Name(attr.road_divider());
    //a["route_level"] = RouteLevel_Name(attr.route_levels());
    a["special_traffic_area_category"] = SpecialTrafficAreaCategory_Name(attr.special_traffic_area_category());

    // repeated TimedAccess
    json forwardArr = json::array();
    for (const auto& t : attr.forward_access_permissions()) {
        forwardArr.push_back(convert_timed_access(t));
    }
    a["forward_access_permissions"] = forwardArr;

        json backwardArr = json::array();
    for (const auto& t : attr.backward_access_permissions()) {
        backwardArr.push_back(convert_timed_access(t));
    }
    a["backward_access_permissions"] = backwardArr;

    json accessRestrictionArr = json::array();
    for (const auto& t : attr.access_restrictions()) {
        accessRestrictionArr.push_back(convert_timed_access(t));
    }
    a["access_restrictions"] = accessRestrictionArr;


    json constructionStatusesAttr = json::array();
    for (const auto& t : attr.construction_statuses()) {
        constructionStatusesAttr.push_back(convert_timed_access(t));
    }
    a["construction_statuses"] = constructionStatusesAttr;

    // repeated SpecialSpeedSituation
    json speedArr = json::array();
    for (const auto& s : attr.special_speed_situations()) {
        speedArr.push_back(convert_special_speed(s));
    }
    a["special_speed_situations"] = speedArr;

    // UsageFeeRequired
    json usageArr = json::array();
    for (const auto& u : attr.usage_fee_required()) {
        usageArr.push_back(convert_usage_fee(u));
    }
    a["usage_fee_required"] = usageArr;

    // EnvironmentalZoneCondition
    json envArr = json::array();
    for (const auto& e : attr.environmental_zone()) {
        envArr.push_back(convert_env_zone(e));
    }
    a["environmental_zone_conditions"] = envArr;

    */
    // BuiltUpArea
   // a["built_up_area"] = builtUpAreaToString(attr.built_up_area());

        return a;
    }


std::string joinLinkIds(const layers::LinkIdMappingLayer::Segment& segment) {
    std::string result;
    for (int i = 0; i < segment.links_size(); ++i) {
        if (i > 0) {
            result += ";";
        }
        result += std::to_string(segment.links(i).link_id());
    }
    return result;
}

json convertInternal(
    const clientmap::decoder::SegmentLayer& segLayer,
    const clientmap::decoder::SegmentAttributeLayer& attrLayer, 
    const clientmap::decoder::SegmentIdMappingLayer& segIdMapLayer,
    const olp::geo::TileKey& tile_key,
    const std::string& output_path )
{



    json feature_collection;
    feature_collection["type"] = "FeatureCollection";
    feature_collection["features"] = json::array();


    int m = segIdMapLayer.segments_size();

    int n = segLayer.segments_size();
    OLP_SDK_LOG_INFO_F(kLogTag, "Isa segment size: %d, inteop segment size: %d", m, n);
    for (int i = 0; i < n; ++i) {
        const auto& seg = segLayer.segments(i);

        const auto& attr = attrLayer.segments(i);

        const auto& hmcIdSeg =  segIdMapLayer.segments(i);

        // === properties ===
        const auto& tileID = TileIDConverter::XYtoTileId(tile_key.Column(), tile_key.Row(), tile_key.Level());
        json properties;
        properties["tile_id"] = tileID;
        properties["local_id"] = seg.local_id();
        properties["length"]   = seg.meter_length();
        properties["host_tile_id"] = seg.host_tile_id();
        properties["hmc_id"] = hmcIdSeg.hmc_id();

        json attributes = json::array();
        for (const auto& attr : attr.attributes()) {
            attributes.push_back(convert_attribute(attr));
        }
        properties["attributes"] = attributes;


    
        // === feature ===
        json feature;
        feature["type"] = "Feature";
        feature["properties"] = properties;

        feature_collection["features"].push_back(feature);
    }

        OLP_SDK_LOG_INFO(kLogTag, "完成转换 GeoJson...");
        return feature_collection;

   // return geojson;
}




} // namespace road_converter