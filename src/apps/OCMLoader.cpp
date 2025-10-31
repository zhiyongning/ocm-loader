#include "OcmMapEngine.hpp"
#include "TileIDConverter.hpp"
#include "RoadDataToGeoJsonConverter.hpp"
#include "ISADataToGeoJsonConverter.hpp"
#include "RoutingDataToGeoJsonConverter.hpp"
#include "CommonDataConverter.hpp"
#include "FileUtils.hpp"
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>
#include <string>
#include <vector>
#include <olp/clientmap/datastore/DataStoreClient.h>
#include <olp/core/geo/coordinates/GeoRectangle.h>

using namespace std;
//using namespace ning::maps::ocm;
isa_converter::ISADataToGeoJsonConverter isaConverter;
road_converter::RoadDataToGeoJsonConverter renderingConverter;
routing_converter::RoutingDataToGeoJsonConverter routingConverter;
common_converter::CommonDataConverter commonConverter;


// 返回相对 geodata 目录下的文件路径
std::string getGeoDataFilePath(const std::string& filename) {
    return FileUtils::getFilePath(filename, "geodata");
}

std::string getRawDataFilePath(const std::string& filename) {
    return FileUtils::getFilePath(filename, "rawdata");
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
pair<string, string> splitKeyVal(const string& s) {
    size_t colonPos = s.find(':');
    if (colonPos == string::npos) {
        // 如果没有冒号，key为整个字符串，value为空
        return {s, ""};
    }
    string key = s.substr(0, colonPos);
    string value = s.substr(colonPos + 1);
    return {key, value};
}

int main(int argc, char* argv[]) {
    //Arg pattern: programName, layerGroupName, area
    //Examples
    //1. ocm-loader lg:isa point:13.08836,52.33812 tile:377893287 version:188
    //2. ocm-loader lg:rendering bbox:13.08836,52.33812,13.761,52.6755 version:180 filter:AND(forward_speed_limit=30)
    //3. ocm-loader tile:377893287 version:188
    //Default parameter, when command line parameter is not enough
    //    //std::string filterStr = "AND(functional_class=functional_class_1)";
    const std::vector<const char*> default_args = {
        "ocm-loader",                  // argv[0]：程序名
        "lg:isa",                      // argv[1]
        //"point:13.08836,52.33812",     // argv[2]
        "tile:377893287",
        "version:default"                  // argv[3]
        // 可根据需要添加更多默认参数
    };
        // 存储最终使用的参数（要么用命令行传入的，要么用默认的）
    std::vector<char*> final_argv;
    int final_argc;

    if (argc < 3) {
        // 命令行参数不足，使用默认参数
        final_argc = default_args.size();
        // 复制默认参数到final_argv（转为char*类型）
        for (const char* arg : default_args) {
            final_argv.push_back(strdup(arg)); // 复制字符串到堆
        }
        final_argv.push_back(nullptr); // 末尾添加nullptr（符合标准）
    } else {
        // 命令行参数足够，直接使用
        final_argc = argc;
        for (int i = 0; i < argc; ++i) {
            final_argv.push_back(argv[i]); // 直接引用命令行参数
        }
        final_argv.push_back(nullptr);
    }

        std::cout << "Used Parameters:：" << std::endl;
    for (int i = 0; i < final_argc; ++i) {
        std::cout << "final_argv[" << i << "] = " << final_argv[i] << std::endl;
    }



    int catalogVersion = 0;
    string layerGroupName = "isa";
    string point = "13.08836,52.33812";
    string bbox = "13.08836,52.33812,13.761,52.6755";
    string filterStr = "";

    map<string, string> params;

    for (int i = 1; i < final_argc; ++i) {
        string param = final_argv[i];
        // 兼容 C++14：用 pair 的 first/second 提取键值，替代结构化绑定
        pair<string, string> keyVal = splitKeyVal(param);
        string key = keyVal.first;  // 提取 key
        string value = keyVal.second; // 提取 value
        params[key] = value;
    }

    // 示例：打印解析结果
    cout << "Parameters list:" << endl;
    for (const auto& item : params) { // C++11 范围 for 循环，C++14 兼容
        cout << "key: " << item.first << ", value: " << item.second << endl;
    }
    
    if(params.find("lg") != params.end())
    {
        layerGroupName = params["lg"];

        if (params.find("version") != params.end()) {
            catalogVersion = atoi(params["version"].c_str());
        }
        cout << "CatalogVersion = " << catalogVersion << endl;

        if (params.find("filter") != params.end()) {
            filterStr = params["filter"];
        }

    }


    // 按 ; 分割顶层 AND / OR
    std::stringstream ss(filterStr);
    std::string part;
    json finalJson = json::array();


     
  

    while (std::getline(ss, part, ';')) {
        if (part.empty()) continue;
        Node n = parseExpression(part);
        finalJson.push_back(nodeToJson(n));
    }


    if (params.find("point") != params.end() ){
        string coordPart = params["point"]; 
        vector<double> coords = parseCoordinates(coordPart);
        if (coords.size() == 2) {
           kTileKey =  processPoint(layerGroupName, coords[0], coords[1]);
        }
    } else if (params.find("bbox") != params.end()) {
        string coordPart = params["bbox"];
        vector<double> coords;
        stringstream ss(coordPart);
        string token;
        while (getline(ss, token, ',')) {
            coords.push_back(stod(token));
        }
        if (coords.size() == 4) {
            tileKeys = processBBox(layerGroupName, coords[0], coords[1], coords[2], coords[3]);
        }
    } else if (params.find("tile") != params.end()) 
    {
         string tileId = params["tile"];
         kTileKey = TileKeyFromTileId(tileId);
    }
    else {
        cerr << "Parameter wrong. "  << endl;
        return 0;
    }
 
    //tileKeys = {TileKeyFromTileId("377782696"), TileKeyFromTileId("377782525")};
        // ------------------------------
    // 步骤 1：配置 HereMapEngine 参数
    // ------------------------------
    ning::maps::ocm::Settings settings;
    settings.offline_enable = false;       // 在线模式（若需离线，设为 true 并配置缓存路径）
    settings.catalog_hrn = kCatalogHrn; // 替换为您的目录 HRN（如 "here:catalog:your-domain:your-catalog"）
    settings.catalog_version = catalogVersion;   // 目录版本（或具体版本号，如 "2024-01-01"）
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
        //layers.push_back(clientmap::interop::kSegmentIdMappingLayerName);

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
        layers.push_back(clientmap::routing::kAdministrativeRoutingContextLayerName);
        layers.push_back(clientmap::routing::kEnvironmentalZoneLayerName);
        layers.push_back(clientmap::routing::kNodeLayerName);
        layers.push_back(clientmap::routing::kPermittedManeuverLayerName);
        layers.push_back(clientmap::routing::kRestrictedManeuverLayerName);
        layers.push_back(clientmap::routing::kSegmentConnectionLayerName);
        layers.push_back(clientmap::routing::kSegmentReferenceLayerName);
        layers.push_back(clientmap::routing::kSegmentTollStructureLayerName);
        layers.push_back(clientmap::routing::kTollCostLayerName);
    }
    else if("interop" == layerGroupName)
    {
        layers.push_back(clientmap::interop::kSegmentIdMappingLayerName);
        layers.push_back(clientmap::interop::kLinkIdMappingLayerName);
    }
    else if("search" == layerGroupName)
    {
        layers.push_back(clientmap::search::kAdministrativeUnitLayerName);
        layers.push_back(clientmap::search::kAdministrativeUnitIndexLayerName);
    }
    else if("ehorizon" == layerGroupName)
    {
        layers.push_back(clientmap::ehorizon::kSegmentGeometryLayerName);
        layers.push_back(clientmap::ehorizon::kForeignSegmentGeometryLayerName);
        layers.push_back(clientmap::ehorizon::kForeignSegmentLayerName);
        layers.push_back(clientmap::ehorizon::kForeignLaneLayerName);
        layers.push_back(clientmap::ehorizon::kForeignSegmentAttributeLayerName);
        layers.push_back(clientmap::ehorizon::kForeignSegmentGeometryAccuracyLayerName);
        layers.push_back(clientmap::ehorizon::kForeignSegmentOvertakeAttributeLayerName);
        layers.push_back(clientmap::ehorizon::kForeignTrafficSignalLayerName);
        layers.push_back(clientmap::ehorizon::kForeignTrafficSignLayerName);
        layers.push_back(clientmap::ehorizon::kForeignVariableSpeedSignLayerName);

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
                    std::string outpath =  getGeoDataFilePath("isa.geojson");
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
                     std::string outpath =  getGeoDataFilePath("data.geojson");
                    json feature_collection = renderingConverter.convert(load_response, tileKey, outpath);
                     appendFeaturesToGeoJSON(outpath, feature_collection, first, finalJson);

                    if (exceedFileLimit(outpath, 50)) { // 50MB
                        std::cout << "文件超过 50MB，停止写入\n";
                        break;
                    }

                    OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
                }


                first = false; // 后续写都走追加
                tileLoaded ++;


                 //Write raw json data into file
            std::string  fileName = kTileKey.ToHereTile() +"-"+ layerGroupName+ ".json";
            std::string outpath =  getRawDataFilePath(fileName);
            commonConverter.convert(load_response, kTileKey, outpath);
             OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());

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
            std::string outpath =  getGeoDataFilePath("isa.geojson");
            json feature_collection = isaConverter.convert(load_response, kTileKey, outpath);
             appendFeaturesToGeoJSON(outpath, feature_collection, true, finalJson);
            OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
            // writeGeoJsonFeatures(outpath, feature_collection, true);
            // finalizeGeoJsonFile(outpath);
        }
        else if("rendering" == layerGroupName)
        {
             std::string outpath =  getGeoDataFilePath("data.geojson");
            json feature_collection = renderingConverter.convert(load_response, kTileKey, outpath);
             appendFeaturesToGeoJSON(outpath, feature_collection, true, finalJson);
            OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
        }
        
        //Write raw json data into file
            std::string  fileName = kTileKey.ToHereTile() +"-"+ layerGroupName+ ".json";
            std::string outpath =  getRawDataFilePath(fileName);
            commonConverter.convert(load_response, kTileKey, outpath);
             OLP_SDK_LOG_INFO_F(kLogTag, "瓦片数据成功写入 %s", outpath.c_str());
        
    }

     cout << "Filter string: " << filterStr << endl;

    return 0;
}
