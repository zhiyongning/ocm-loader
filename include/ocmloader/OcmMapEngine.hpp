// OcmMapEngine.hpp (C++14-friendly)
#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <olp/clientmap/datastore/DataStoreClient.h>
#include "olp/core/logging/Log.h"
#include "ThreadPool.hpp"


namespace ning {
namespace maps {
namespace ocm {

namespace datastore = olp::clientmap::datastore;


struct Settings {
    bool offline_enable = false;
    std::string access_key_id;
    std::string access_key_secret;
    std::string path_to_credentials_file;
    std::string catalog_hrn;
    uint64_t catalog_version = 0;
    std::string cache_folder = "";
};

class OcmMapEngine {
public:
    explicit OcmMapEngine(const Settings& settings);
    ~OcmMapEngine();

    // 禁止拷贝
    OcmMapEngine(const OcmMapEngine&) = delete;
    OcmMapEngine& operator=(const OcmMapEngine&) = delete;

    /**
     * @brief 异步获取瓦片数据（需先调用 InitializeAsync）
     * @param tileKey 瓦片键
     * @param layers 需要加载的图层列表
     * @return future<FetchTileResult> 瓦片数据或错误信息
     */
    datastore::Response<datastore::TileLoadResult> FetchTileAsync(
        const olp::geo::TileKey& tileKey,
        const datastore::TileRequest::Layers& layers);

private:
    class OcmMapEngineImpl;
    std::shared_ptr<OcmMapEngineImpl> m_impl;
};

} // namespace ocm
} // namespace maps
} // namespace ning
