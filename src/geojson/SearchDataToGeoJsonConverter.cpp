#include "SearchDataToGeoJsonConverter.hpp"
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
#include "here-devel/com/here/platform/schema/clientmap/v1/layers/AdministrativeUnitLayer.pb.h"
#include <unordered_map>

 #include <boost/algorithm/string/join.hpp>



namespace search_converter {

namespace ocm = ning::maps::ocm;
namespace layers = com::here::platform::schema::clientmap::v1::layers;
using json = nlohmann::json;
using namespace com::here::platform::schema::clientmap::v1::layers::common;



json convertInternal(
    const clientmap::decoder::AdministrativeUnitLayer& adminUnitLayer,
    const clientmap::decoder::AdministrativeUnitIndexLayer& adminUnitIndexLayer, 
    const olp::geo::TileKey& tile_key,
    const std::string& output_path
);


constexpr auto kLogTag = "SearchDataToGeoJsonConverter";



json SearchDataToGeoJsonConverter::convert(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath)
{


    const auto& layer_results = response.GetResult().GetLayersResults();

    ocm::LayerFinder finder(layer_results);

    const auto& adminUnitLayer = finder.GetRequiredLayer<clientmap::decoder::AdministrativeUnitLayer>(clientmap::search::kAdministrativeUnitLayerName);
    const auto* adminUnitIndexLayer = finder.TryGetLayer<clientmap::decoder::AdministrativeUnitIndexLayer>(clientmap::search::kAdministrativeUnitIndexLayerName);   


    return convertInternal(adminUnitLayer,*adminUnitIndexLayer, tile_key, outPath);


}



json convertInternal(
    const clientmap::decoder::AdministrativeUnitLayer& adminUnitLayer,
    const clientmap::decoder::AdministrativeUnitIndexLayer& adminUnitIndexLayer, 
    const olp::geo::TileKey& tile_key,
    const std::string& output_path )
{


int unitSize = adminUnitLayer.units_size();
for(int i=0; i< unitSize; i ++)
{
   const auto& unit = adminUnitLayer.units(i);
   
}
    json feature_collection;
    feature_collection["type"] = "FeatureCollection";
    feature_collection["features"] = json::array();

    
    
        // === feature ===
        json feature;
        feature["type"] = "Feature";
        //feature["properties"] = properties;

        feature_collection["features"].push_back(feature);
    

        OLP_SDK_LOG_INFO(kLogTag, "完成转换 GeoJson...");
        return feature_collection;

   // return geojson;
}




} // namespace road_converter