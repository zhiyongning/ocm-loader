#include "OcmMapEngine.hpp"
#include "TileIDConverter.hpp"
#include "RoadDataToGeoJsonConverter.hpp"
#include "ISADataToGeoJsonConverter.hpp"
#include "RoutingDataToGeoJsonConverter.hpp"
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>
#include <string>
#include <vector>
#include <olp/clientmap/datastore/DataStoreClient.h>
#include <olp/core/geo/coordinates/GeoRectangle.h>

using namespace std;
using namespace ning::maps::ocm;
isa_converter::ISADataToGeoJsonConverter isaConverter;
road_converter::RoadDataToGeoJsonConverter renderingConverter;
routing_converter::RoutingDataToGeoJsonConverter routingConverter;

#ifdef _WIN32
const char PATH_SEP = '\\';
#else
const char PATH_SEP = '/';
#endif

// 返回相对 geodata 目录下的文件路径
std::string getFilePath(const std::string& filename) {
    return std::string(".") + PATH_SEP + "geodata" + PATH_SEP + filename;
}

std::string getDiskCachePath() {
    return std::string(".") + PATH_SEP + "diskcache" + PATH_SEP;
}
// 工具函数：把 "13.08836,52.33812" 这种字符串拆分为 double
vector<double> parseCoordinates(const string& coordStr) {
    vector<double> coords;
    stringstream ss(coordStr);
    string token;
    while (getline(ss, token, ',')) {
        coords.push_back(stod(token));  // stod 转 double
    }
    return coords;
}


// 辅助函数：打印错误信息
void printError(const std::string& error) {
    std::cerr << "Error: " << error << std::endl;
}

/// Your Access Key ID to access the HERE Platform.
const std::string kHereAccessKeyId;

/// Your Access Key Secret to access the HERE Platform.
const std::string kHereAccessKeySecret;

/// Path to the file with credentials that was downloaded from the HERE Platform.
const std::string kPathToCredentialsFile;

const uint32_t zoom_level = 14u;
/// HERE Resource Name of the OCM catalog to download the data from.
constexpr auto kCatalogHrn = "hrn:here:data::olp-here:ocm";

/// The version of the OCM catalog to download the data from.
constexpr auto kCatalogVersion = 180;

//121.5222654, 25.0320536
/// The tag for log messages.
constexpr auto kLogTag = "OCMLoader.cpp";


/// Converts provided `olp::geo::GeoCoordinates` to `olp::geo::TileKey` with a specified level.
olp::geo::TileKey
TileKeyFromGeoCoordinates( const olp::geo::GeoCoordinates& geo_coordinates, uint32_t level )
{
    const olp::geo::HalfQuadTreeIdentityTilingScheme tiling_scheme;
    return olp::geo::TileKeyUtils::GeoCoordinatesToTileKey( tiling_scheme, geo_coordinates, level );
}

olp::geo::TileKey 
TileKeyFromTileId(const std::string& key)
{
    return olp::geo::TileKey::FromHereTile(key);
}
/**
 * @brief A function to demonstrate how to convert geo bounding box to a list of tiles.
 *
 * @param south_west A south west coordinate of the bounding box.
 * @param north_east A north east coordinate of the bounding box.
 *
 * @return A collection of tiles that represent the bounding box area on zoom level 10.
 */
datastore::TileKeys
CoverageFromGeoBBox( const olp::geo::GeoCoordinates& south_west,
                     const olp::geo::GeoCoordinates& north_east )
{
    const olp::geo::HalfQuadTreeIdentityTilingScheme tiling_scheme;

    return olp::geo::TileKeyUtils::GeoRectangleToTileKeys(
        tiling_scheme, olp::geo::GeoRectangle( south_west, north_east ), zoom_level );
}

std::string joinLayerNames(const std::vector<std::string>& layers) {
    std::string result;
    for (size_t i = 0; i < layers.size(); ++i) {
        result += layers[i];
        if (i != layers.size() - 1) {
            result += ","; 
        }
    }
    return result;
}


olp::geo::TileKey  kTileKey ;

datastore::TileKeys tileKeys;

void printTileRequestInfo(olp::geo::TileKey tileKey)
{
    uint32_t x = tileKey.Column(), y = tileKey.Row(), level = tileKey.Level();
    std::string quadkey = TileIDConverter::TileToQuadkey(x, y, level);
    uint64_t hereId = TileIDConverter::QuadkeyToHereTileId(quadkey);
    std::stringstream ss;
    ss << "请求瓦片信息 - Tile (X,Y) Indexes: (" << x << ", " << y << ") Tile Level: " << level
       << " → Quadkey: " << quadkey << " → HERE Tile ID: " << hereId;
    std::string mystr = ss.str();

    // 使用 OLP_SDK_LOG_INFO 输出
    OLP_SDK_LOG_INFO_F(kLogTag, "%s", mystr.c_str());
}



// 示例方法：接受经纬度
auto processPoint(string layerGroup, double lon, double lat) {
    cout << "Processing Point: lon=" << lon << ", lat=" << lat << endl;
    auto coordinates = olp::geo::GeoCoordinates::FromDegrees(lat,lon);
    return TileKeyFromGeoCoordinates( coordinates, zoom_level );
}

auto processBBox(string layerGroup, double lon1, double lat1, double lon2, double lat2) {
    cout << "Processing BBox: lon1=" << lon1 << ", lat1=" << lat1
         << ", lon2=" << lon2 << ", lat2=" << lat2 << endl;
    return  CoverageFromGeoBBox(olp::geo::GeoCoordinates::FromDegrees( lat1, lon1 ),
                            olp::geo::GeoCoordinates::FromDegrees( lat2, lon2 ));
}



struct Condition {
    std::string key;
    std::string op;
    std::string value;
};

enum class NodeType { AND, OR, CONDITION };

struct Node {
    NodeType type;
    std::vector<Node> children;  // 对于 AND/OR
    Condition cond;              // 对于叶子条件
};

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

Condition parseCondition(const std::string& s) {
    static const std::vector<std::string> ops = { ">=", "<=", "=", ">", "<" };
    for (const auto& op : ops) {
        size_t pos = s.find(op);
        if (pos != std::string::npos) {
            return { trim(s.substr(0,pos)), op, trim(s.substr(pos+op.size())) };
        }
    }
    throw std::runtime_error("Invalid condition: " + s);
}

Node parseExpression(const std::string& s) {
    std::string expr = trim(s);

    if (expr.empty()) throw std::runtime_error("Empty expression");

    // AND(...) 或 OR(...)
    if (expr.substr(0, 4) == "AND(" && expr.back() == ')') {
        Node node{ NodeType::AND };
        std::string inner = expr.substr(4, expr.size()-5);
        std::stringstream ss(inner);
        std::string token;
        int paren = 0;
        std::string buf;
        for (char c : inner) {
            if (c=='(') paren++;
            if (c==')') paren--;
            if ((c==',' || c==';') && paren==0) {
                if (!buf.empty()) node.children.push_back(parseExpression(buf));
                buf.clear();
            } else buf += c;
        }
        if (!buf.empty()) node.children.push_back(parseExpression(buf));
        return node;
    } 
    else if (expr.substr(0,3)=="OR(" && expr.back()==')') {
        Node node{ NodeType::OR };
        std::string inner = expr.substr(3, expr.size()-4);
        std::stringstream ss(inner);
        std::string token;
        int paren=0;
        std::string buf;
        for (char c:inner){
            if(c=='(') paren++;
            if(c==')') paren--;
            if((c==','||c==';') && paren==0){
                if(!buf.empty()) node.children.push_back(parseExpression(buf));
                buf.clear();
            } else buf += c;
        }
        if(!buf.empty()) node.children.push_back(parseExpression(buf));
        return node;
    } 
    else {
        // 单个条件
        Node node{ NodeType::CONDITION };
        node.cond = parseCondition(expr);
        return node;
    }
}

json nodeToJson(const Node& node) {
    if (node.type == NodeType::CONDITION) {
        return json{{"key", node.cond.key}, {"op", node.cond.op}, {"value", node.cond.value}};
    } else {
        json j;
        j[node.type == NodeType::AND ? "and" : "or"] = json::array();
        for (const auto& child : node.children)
            j[node.type == NodeType::AND ? "and" : "or"].push_back(nodeToJson(child));
        return j;
    }
}


static bool compareValues(const json& featureValue, const std::string& op, const json& filterValue) {
    if (op == "=") {
        double fv, val;
        bool fvIsNum = false, valIsNum = false;

        if (featureValue.is_number()) { fv = featureValue.get<double>(); fvIsNum = true; }
        else if (featureValue.is_string()) { try { fv = std::stod(featureValue.get<std::string>()); fvIsNum = true; } catch (...) {} }

        if (filterValue.is_number()) { val = filterValue.get<double>(); valIsNum = true; }
        else if (filterValue.is_string()) { try { val = std::stod(filterValue.get<std::string>()); valIsNum = true; } catch (...) {} }

        if (fvIsNum && valIsNum) return fv == val;

        // ---- 忽略大小写的字符串比较 ----
        if (featureValue.is_string() && filterValue.is_string()) {
            std::string fvStr = featureValue.get<std::string>();
            std::string valStr = filterValue.get<std::string>();

            auto toLower = [](std::string& s) {
                std::transform(s.begin(), s.end(), s.begin(),
                               [](unsigned char c){ return std::tolower(c); });
            };

            toLower(fvStr);
            toLower(valStr);

            return fvStr == valStr;
        }

        // 兜底
        return featureValue.dump() == filterValue.dump();
    }

    // 其他比较符要求数字
    double fv, val;
    try {
        fv = featureValue.is_number() ? featureValue.get<double>() : std::stod(featureValue.get<std::string>());
        val = filterValue.is_number() ? filterValue.get<double>() : std::stod(filterValue.get<std::string>());
    } catch (...) { return false; }

    if (op == ">") return fv > val;
    if (op == "<") return fv < val;
    if (op == ">=") return fv >= val;
    if (op == "<=") return fv <= val;

    return false;
}

static bool matchCondition(const json& feature, const json& cond) {
    if (!cond.contains("key") || !cond.contains("op") || !cond.contains("value")) {
        return false;
    }

    std::string key = cond["key"].get<std::string>();
    std::string op  = cond["op"].get<std::string>();
    const json& val = cond["value"];

    // 在 properties 里找
    if (feature.contains("properties") && feature["properties"].contains(key)) {
        if (compareValues(feature["properties"][key], op, val)) {
            return true;
        }
    }

    // 在 attributes 数组里找
    if (feature.contains("properties") &&
        feature["properties"].contains("attributes") &&
        feature["properties"]["attributes"].is_array())
    {
        for (auto& attr : feature["properties"]["attributes"]) {
            if (attr.contains(key)) {
                if (compareValues(attr[key], op, val)) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool matchFeature(const json& feature, const json& filter) {
    // filter 是数组
    if (!filter.is_array()) {
        return false;
    }

    // 每个元素是一个 and/or 逻辑对象
    for (auto& logicGroup : filter) {
        if (logicGroup.contains("and")) {
            bool allMatch = true;
            for (auto& cond : logicGroup["and"]) {
                if (!matchCondition(feature, cond)) {
                    allMatch = false;
                    break;
                }
            }
            if (!allMatch) return false; // and 组里有一个不满足，整个失败
        }
        else if (logicGroup.contains("or")) {
            bool anyMatch = false;
            for (auto& cond : logicGroup["or"]) {
                if (matchCondition(feature, cond)) {
                    anyMatch = true;
                    break;
                }
            }
            if (!anyMatch) return false; // or 组里全不满足，整个失败
        }
    }

    return true;
}
void appendFeaturesToGeoJSON(const std::string& outpath, 
                             const json& newFeatures, 
                             bool firstTile,
                             const json& filter) 
{
    json geoJson;

    // 第一次写入：新建 FeatureCollection
    if (firstTile) {
        geoJson["type"] = "FeatureCollection";
        geoJson["features"] = json::array();
    } else {
        // 读取已有文件
        std::ifstream inFile(outpath);
        if (!inFile.is_open()) {
            throw std::runtime_error("Failed to open existing GeoJSON file: " + outpath);
        }

        try {
            inFile >> geoJson;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to parse existing GeoJSON: ") + e.what());
        }

        if (!geoJson.contains("features") || !geoJson["features"].is_array()) {
            throw std::runtime_error("Existing GeoJSON does not contain a valid 'features' array");
        }
    }

    // 遍历新 features，并按 filter 逻辑过滤
    for (const auto& feature : newFeatures["features"]) {
        if (matchFeature(feature, filter)) {
        geoJson["features"].push_back(feature);
        }
    }


    // 写回文件
    std::ofstream outFile(outpath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        throw std::runtime_error("Failed to open GeoJSON file for writing: " + outpath);
    }

    outFile << geoJson.dump(4);
    if (!outFile.good()) {
        throw std::runtime_error("Failed to write GeoJSON to file: " + outpath);
    }
}


bool exceedFileLimit(const std::string& filename, std::size_t MB) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    if (!in.is_open()) {
        return false; // 文件不存在或无法打开，不算超限
    }
    std::size_t filesize = static_cast<std::size_t>(in.tellg());
    return filesize > MB * 1024 * 1024;
}

void calculateRoadLength()
{
 try {
        // 1. 读取GeoJSON文件
        std::ifstream file("/Users/zhiyong/mytools/ocmam-examples/render/geodata/routing.geojson");
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // 2. 解析JSON
        json geojson = json::parse(buffer.str());
        
        // 3. 遍历所有features并累加length
        double total_length = 0.0;
        for (auto& feature : geojson["features"]) {
            if (feature.contains("properties") && 
                feature["properties"].contains("length")) {
                total_length += feature["properties"]["length"].get<double>();
            }
        }
        
        // 4. 输出结果
        std::cout << "Total length: " << total_length << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
     
    }
   
}
int main(int argc, char* argv[]) {
    //Arg pattern: programName, layerGroupName, area
    //Examples
    //1. OCMLoader isa point:13.08836,52.33812
    //2. OCMLoader rendering bbox:13.08836,52.33812,13.761,52.6755
    //OCMLoader isa point
//Berlin: 52.5308, 13.3847

    string layerGroupName = "isa";
   //string area = "bbox:121.50673, 25.10969,121.55205, 25.14746"; //Whole Taiwan
   //string area = "bbox:120.01465, 21.88189 ,122.14600, 25.38374";
    //string area = "point:121.50673,25.10969";
    std::string area = "point: 114.1477,22.3602";
    
    //string area = "point:121.416435,25.061654";
    //std::string filterStr = "AND(forward_speed_limit=30)";
    //std::string filterStr = "AND(functional_class=functional_class_1)";
      std::string filterStr = "";
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <layerGroupName> <area>" << endl;
        //return 1;
    }
    else{
     layerGroupName = argv[1];
     area = argv[2];
     if(argv[3])
        filterStr = argv[3];
     else
        filterStr = "";
    }


    
    cout << "Layer Group: " << layerGroupName << endl;
    cout << "Area Input: " << area << endl;

    if(filterStr.find("filter:") == 0)
        filterStr = filterStr.substr(7);
    
     cout << "Filter string: " << filterStr << endl;

    // 按 ; 分割顶层 AND / OR
    std::stringstream ss(filterStr);
    std::string part;
    json finalJson = json::array();


     
  

    while (std::getline(ss, part, ';')) {
        if (part.empty()) continue;
        Node n = parseExpression(part);
        finalJson.push_back(nodeToJson(n));
    }



    // 判断是 point 还是 bbox
    if (area.find("point:") == 0) {
        string coordPart = area.substr(6); // 去掉 "point:"
        vector<double> coords = parseCoordinates(coordPart);
        if (coords.size() == 2) {
           kTileKey =  processPoint(layerGroupName, coords[0], coords[1]);
        }
    } else if (area.find("bbox:") == 0) {
        string coordPart = area.substr(5); // 去掉 "bbox:"
        vector<double> coords;
        stringstream ss(coordPart);
        string token;
        while (getline(ss, token, ',')) {
            coords.push_back(stod(token));
        }
        if (coords.size() == 4) {
            tileKeys = processBBox(layerGroupName, coords[0], coords[1], coords[2], coords[3]);
        }
    } else {
        cerr << "Unknown area format: " << area << endl;
    }

    //tileKeys = {TileKeyFromTileId("377782696"), TileKeyFromTileId("377782525")};
        // ------------------------------
    // 步骤 1：配置 HereMapEngine 参数
    // ------------------------------
    ning::maps::ocm::Settings settings;
    settings.offline_enable = false;       // 在线模式（若需离线，设为 true 并配置缓存路径）
    settings.catalog_hrn = kCatalogHrn; // 替换为您的目录 HRN（如 "here:catalog:your-domain:your-catalog"）
    settings.catalog_version = kCatalogVersion;   // 目录版本（或具体版本号，如 "2024-01-01"）
    settings.access_key_id = kHereAccessKeyId;   // 替换为您的访问密钥 ID
    settings.path_to_credentials_file = kPathToCredentialsFile;
    settings.access_key_secret = kHereAccessKeySecret; // 替换为您的访问密钥 Secret
    settings.cache_folder = getDiskCachePath();

      // ------------------------------
    // 步骤 2：创建地图引擎实例
    // ------------------------------
    ning::maps::ocm::OcmMapEngine engine(settings);


    OLP_SDK_LOG_INFO_F(kLogTag, "%s", "引擎初始化成功！");

    // 构造需要加载的图层列表（示例：道路、行政区域）
    std::vector<std::string > layers;
    if("isa" == layerGroupName)
    {
        layers.push_back(clientmap::isa::kIsaSegmentLayerName);
        layers.push_back(clientmap::isa::kIsaSegmentAttributeLayerName);
        layers.push_back(clientmap::isa::kIsaSegmentGeometryLayerName);
        layers.push_back(clientmap::isa::kIsaForeignSegmentGeometryLayerName);
        layers.push_back(clientmap::isa::kIsaForeignSegmentLayerName);
        layers.push_back(clientmap::isa::kIsaNodeLayerName);
       // layers.push_back(clientmap::interop::kLinkIdMappingLayerName);
        layers.push_back(clientmap::interop::kSegmentIdMappingLayerName);

    }
    else if("rendering" == layerGroupName)
    {
        layers.push_back(clientmap::rendering::kRoadLayerName);
        layers.push_back(clientmap::rendering::kRoadAttributeLayerName);
        layers.push_back(clientmap::rendering::kRoadGeometryLayerName);
        layers.push_back(clientmap::rendering::kRoadNameLayerName);
    }
    else if("routing" == layerGroupName)
    {
        layers.push_back(clientmap::routing::kSegmentLayerName);
        layers.push_back(clientmap::routing::kSegmentAttributeLayerName);
        //layers.push_back(clientmap::routing::kSegmentGeometryLayerName);
                layers.push_back(clientmap::interop::kSegmentIdMappingLayerName);
    }
    else if("search" == layerGroupName)
    {
        layers.push_back(clientmap::search::kAdministrativeUnitLayerName);
        layers.push_back(clientmap::search::kAdministrativeUnitIndexLayerName);
    }

    if(!tileKeys.empty())
    {
          cout << "Total Tile size : " << tileKeys.size() << endl;

          bool first = true; // 标记是否是第一个 TileKey
          int tileLoaded = 0;
      for(olp::geo::TileKey tileKey : tileKeys)
      {
          try{
 
           
                printTileRequestInfo(tileKey);
                 cout << "Total Tile size : " << tileKeys.size() << "  Loaded size:" << tileLoaded << endl;
                OLP_SDK_LOG_INFO_F(kLogTag, "待加载图层 - %s", joinLayerNames(layers).c_str());
                OLP_SDK_LOG_INFO_F(kLogTag, "开始获取瓦片数据...");

                const datastore::Response<datastore::TileLoadResult> load_response =
                    engine.FetchTileAsync(tileKey, layers);

                if ("isa" == layerGroupName)
                {
                    std::string outpath =  getFilePath("isa.geojson");
                    json feature_collection = isaConverter.convert(load_response, tileKey, outpath);
                     appendFeaturesToGeoJSON(outpath, feature_collection, first, finalJson);

                    if (exceedFileLimit(outpath, 50)) { // 50MB
                        std::cout << "文件超过 50MB，停止写入\n";
                        break;
                    }
                    OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
                }
                else if ("rendering" == layerGroupName)
                {
                     std::string outpath =  getFilePath("data.geojson");
                    json feature_collection = renderingConverter.convert(load_response, tileKey, outpath);
                     appendFeaturesToGeoJSON(outpath, feature_collection, first, finalJson);

                    if (exceedFileLimit(outpath, 50)) { // 50MB
                        std::cout << "文件超过 50MB，停止写入\n";
                        break;
                    }

                    OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
                }
                else if("routing" == layerGroupName)
                {
                     std::string outpath =  getFilePath("routing.geojson");
                   json feature_collection = routingConverter.convert(load_response, tileKey, outpath);
                     appendFeaturesToGeoJSON(outpath, feature_collection, first, finalJson);

                    if (exceedFileLimit(outpath, 50)) { // 50MB
                        std::cout << "文件超过 50MB，停止写入\n";
                        break;
                    }

                    OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
                }

                first = false; // 后续写都走追加
                tileLoaded ++;
         // cout << "Total Tile size : " << tileKeys.size() << "  Loaded size:" << tileLoaded << endl;
            }catch(...)
            {
                  OLP_SDK_LOG_INFO_F(kLogTag, "Error in load tile.");
            }
            
      }

      calculateRoadLength();

    }
    else if(kTileKey.IsValid()){
        printTileRequestInfo(kTileKey);
        OLP_SDK_LOG_INFO_F(kLogTag, "待加载图层 - %s", joinLayerNames(layers).c_str());
        OLP_SDK_LOG_INFO_F(kLogTag, "开始获取瓦片数据...");
        const datastore::Response< datastore::TileLoadResult > load_response = engine.FetchTileAsync(kTileKey, layers);
        if("isa" == layerGroupName)
        {
            std::string outpath =  getFilePath("isa.geojson");
            json feature_collection = isaConverter.convert(load_response, kTileKey, outpath);
             appendFeaturesToGeoJSON(outpath, feature_collection, true, finalJson);
            OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
            // writeGeoJsonFeatures(outpath, feature_collection, true);
            // finalizeGeoJsonFile(outpath);
        }
        else if("rendering" == layerGroupName)
        {
             std::string outpath =  getFilePath("data.geojson");
            json feature_collection = renderingConverter.convert(load_response, kTileKey, outpath);
             appendFeaturesToGeoJSON(outpath, feature_collection, true, finalJson);
            OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
        }
        else if("routing" == layerGroupName)
        {
            std::string outpath =  getFilePath("routing.geojson");
            json feature_collection = routingConverter.convert(load_response, kTileKey, outpath);
             appendFeaturesToGeoJSON(outpath, feature_collection, true, finalJson);
            OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
        }
        else if("search" == layerGroupName)
        {
            std::string outpath =  getFilePath("routing.geojson");
            json feature_collection = routingConverter.convert(load_response, kTileKey, outpath);
             appendFeaturesToGeoJSON(outpath, feature_collection, true, finalJson);
            OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
        }
    }

     cout << "Filter string: " << filterStr << endl;

    return 0;
}
