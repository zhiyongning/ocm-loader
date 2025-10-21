#include "CommonDataConverter.hpp"
#include "TimeDomainParser.hpp"
#include "TileIDConverter.hpp"
#include <olp/core/logging/Log.h>
#include <fstream>
#include <cmath>
#include <sstream>
#include <LayerFinder.hpp>
#include "GeometryUtils.hpp" 
#include <unordered_map>
#include <olp/clientmap/datastore/DataStoreClient.h>
#include <boost/algorithm/string/join.hpp>
// 修正：引入 boost::variant2 头文件
#include <boost/variant2/variant.hpp>
#include <google/protobuf/util/json_util.h>
#include <nlohmann/json.hpp>


namespace common_converter {

namespace ocm = ning::maps::ocm;
namespace layers = com::here::platform::schema::clientmap::v1::layers;
using json = nlohmann::json;
using namespace com::here::platform::schema::clientmap::v1::layers::common;

using LayerResponseType = olp::clientmap::datastore::TileLoadResult::LayerResponseType;
using LayerVariantPtr = clientmap::decoder::LayerVariantPtr;

constexpr auto kLogTag = "CommonDataConverter";

// 修正：variant2 不需要继承 static_visitor，直接定义函数对象
struct CommonDataConverter::ProtobufExtractor {
    // 处理所有 LayerStorage<T> 类型
    template <typename T>
    const google::protobuf::Message* operator()(const clientmap::decoder::LayerStorage<T>& layer_storage) const {
             if (layer_storage.content) {
            return layer_storage.content.get(); // 或直接 layer_storage.content.operator->()
        }
         OLP_SDK_LOG_WARNING(kLogTag, "LayerStorage content is null");
        return nullptr;
    }

    // 处理智能指针包裹的 LayerStorage<T>（根据实际类型调整）
    template <typename T>
    const google::protobuf::Message* operator()(const std::shared_ptr<const clientmap::decoder::LayerStorage<T>>& layer_storage) const {
        if (layer_storage && layer_storage->content) {
            return layer_storage->content.get(); // 同理访问 content
        }
        OLP_SDK_LOG_WARNING(kLogTag, "Shared LayerStorage content is null");
        return nullptr;
    }

    // 处理 EncodedLayer 类型
    const google::protobuf::Message* operator()(const clientmap::decoder::EncodedLayer&) const {
        OLP_SDK_LOG_WARNING(kLogTag, "EncodedLayer type is not supported, skip this layer");
        return nullptr;
    }

    // 处理智能指针包裹的 EncodedLayer
    const google::protobuf::Message* operator()(const std::shared_ptr<const clientmap::decoder::EncodedLayer>&) const {
        OLP_SDK_LOG_WARNING(kLogTag, "EncodedLayer type is not supported, skip this layer");
        return nullptr;
    }

    // 处理未匹配的其他类型
    template <typename T>
    const google::protobuf::Message* operator()(const T&) const {
        OLP_SDK_LOG_WARNING(kLogTag, "Unsupported layer type, skip this layer");
        return nullptr;
    }
};

// 修正：函数声明与定义完全一致
const google::protobuf::Message* CommonDataConverter::ExtractProtobufFromLayer(const LayerResponseType& layer_response) {
    try {
        // 获取 variant2 对象（根据实际 GetContent() 返回值调整）
        LayerVariantPtr variant_ptr = layer_response.GetResult().GetContent();
        if (!variant_ptr) {
            OLP_SDK_LOG_ERROR_F(kLogTag, "Layer content is null");
            return nullptr;
        }

        // 修正：使用 boost::variant2::visit 访问 variant
        ProtobufExtractor extractor;
        return boost::variant2::visit(extractor, *variant_ptr);
    } catch (const std::exception& e) {
        OLP_SDK_LOG_ERROR_F(kLogTag, "Extract protobuf failed: %s", e.what());
        return nullptr;
    }
}


// 修正：处理 Protobuf JSON 转换的警告
json CommonDataConverter::ProtobufToJson(const google::protobuf::Message& msg) {
    try {
        std::string json_str;
        // 修正：用 JsonPrintOptions 替代废弃的 JsonOptions
        google::protobuf::util::JsonPrintOptions print_options;
        print_options.add_whitespace = true;  // 保留缩进，便于阅读

        // 修正：不忽略 nodiscard 返回值，判断转换状态
        (void)google::protobuf::util::MessageToJsonString(msg, &json_str, print_options);


        return json::parse(json_str);
    } catch (const json::parse_error& e) {
        OLP_SDK_LOG_ERROR_F(kLogTag, "JSON parse failed: %s", e.what());
        return json::object();
    }
}

// 写入文件函数
bool CommonDataConverter::WriteJsonToFile(const json& merged_json, const std::string& out_path) {
    try {
        std::ofstream output_file(out_path);
        if (!output_file.is_open()) {
            OLP_SDK_LOG_ERROR_F(kLogTag, "Failed to open output file: %s", out_path.c_str());
            return false;
        }

        output_file << std::setw(4) << merged_json << std::endl;
        output_file.close();
        OLP_SDK_LOG_INFO_F(kLogTag, "Successfully write merged JSON to: %s", out_path.c_str());
        return true;
    } catch (const std::exception& e) {
        OLP_SDK_LOG_ERROR_F(kLogTag, "Write file failed: %s", e.what());
        return false;
    }
}

// 核心转换方法
void CommonDataConverter::convert(
    const olp::clientmap::datastore::Response<olp::clientmap::datastore::TileLoadResult>& response, 
    const olp::geo::TileKey& tile_key, 
    const std::string& outPath) {

    if (!response.IsSuccessful()) {
        OLP_SDK_LOG_ERROR_F(kLogTag, "Tile load response is failed, error: %s", response.GetError());
        return;
    }

    const auto& layer_results = response.GetResult().GetLayersResults();
    if (layer_results.empty()) {
        OLP_SDK_LOG_WARNING(kLogTag, "No layers found in the response");
        return;
    }

    json merged_json;
    merged_json["type"] = "FeatureCollection";
    merged_json["tile_key"] = tile_key.ToHereTile();
    merged_json["layer_count"] = layer_results.size();
    merged_json["features"] = json::array();

    for (size_t i = 0; i < layer_results.size(); ++i) {
        const auto& layer_response = layer_results[i];

        try {
            const auto& payload = layer_response.GetPayload();
            const std::string& layer_name = payload.layer_name;
            OLP_SDK_LOG_INFO_F(kLogTag, "Processing layer: %s (index: %zu)", layer_name.c_str(), i);

            const google::protobuf::Message* proto_msg = ExtractProtobufFromLayer(layer_response);
            if (!proto_msg) {
                OLP_SDK_LOG_WARNING_F(kLogTag, "Skip layer %s: failed to extract protobuf data", layer_name.c_str());
                continue;
            }

            json layer_json = ProtobufToJson(*proto_msg);
            if (layer_json.is_null() || layer_json.empty()) {
                OLP_SDK_LOG_WARNING_F(kLogTag, "Skip layer %s: empty JSON data", layer_name.c_str());
                continue;
            }

            layer_json["layer_meta"] = {
                {"layer_name", layer_name},
                {"layer_index", i},
                {"tile_key", tile_key.ToHereTile()}
            };

            merged_json["features"].push_back(layer_json);

        } catch (const std::exception& e) {
            OLP_SDK_LOG_ERROR_F(kLogTag, "Process layer %zu failed: %s", i, e.what());
            continue;
        }
    }

    if (!merged_json["features"].empty()) {
        WriteJsonToFile(merged_json, outPath);
    } else {
        OLP_SDK_LOG_WARNING(kLogTag, "No valid layer data to write");
    }
}

} // namespace common_converter