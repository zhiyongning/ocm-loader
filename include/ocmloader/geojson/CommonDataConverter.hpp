#ifndef COMMON_DATA_CONVERTER_HPP
#define COMMON_DATA_CONVERTER_HPP

#include <nlohmann/json.hpp>
#include <boost/variant2/variant.hpp>
#include <google/protobuf/message.h>
#include <string>
#include <olp/clientmap/datastore/DataStoreClient.h>
 namespace datastore = olp::clientmap::datastore;
namespace common_converter {

class CommonDataConverter {
public:
    // 核心转换接口
    void convert(
        const datastore::Response<olp::clientmap::datastore::TileLoadResult>& response,
        const olp::geo::TileKey& tile_key,
        const std::string& outPath);

private:
    // 访问器结构体：用于提取variant中的protobuf消息
    struct ProtobufExtractor;

    // 从LayerResponseType中提取protobuf消息
    const google::protobuf::Message* ExtractProtobufFromLayer(const datastore::TileLoadResult::LayerResponseType& layer_response);

    // 将protobuf消息转换为JSON
    nlohmann::json ProtobufToJson(const google::protobuf::Message& msg);

    // 将合并后的JSON写入文件
    bool WriteJsonToFile(const nlohmann::json& merged_json, const std::string& out_path);
};

} // namespace common_converter

#endif // COMMON_DATA_CONVERTER_HPP