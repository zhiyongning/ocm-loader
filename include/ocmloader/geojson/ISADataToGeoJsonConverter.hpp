#ifndef ISA_DATA_TO_GEOJSON_CONVERTER_HPP
#define ISA_DATA_TO_GEOJSON_CONVERTER_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

// 下面两行根据你的工程实际头文件路径调整
#include <olp/clientmap/datastore/DataStoreClient.h>
#include <OcmMapEngine.hpp>

using json = nlohmann::json;

namespace isa_converter {

 namespace datastore = olp::clientmap::datastore;
 // 注意：你的 ocm 命名空间在源码中是 ning::maps::ocm（你原来写的是这样），保留如下：
 namespace ocm = ning::maps::ocm;

class ISADataToGeoJsonConverter {
public:
    ISADataToGeoJsonConverter() = default;
    ~ISADataToGeoJsonConverter() = default;

    /**
     * 从 FetchTileResult 中提取各层然后调用 convertInternal
     */
    json convert(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath);

    json convertAdmin(const datastore::Response< datastore::TileLoadResult >& response, const olp::geo::TileKey &tile_key, const std::string& outPath);

private:


};

} // namespace isa_converter

#endif // ISA_DATA_TO_GEOJSON_CONVERTER_HPP