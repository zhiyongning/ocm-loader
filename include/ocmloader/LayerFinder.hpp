#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <olp/clientmap/datastore/DataStoreClient.h>


namespace ning {
namespace maps {
namespace ocm {

class LayerFinder {
public:
    // 构造函数，传入 TileLoadResult 的 layer 容器引用
    explicit LayerFinder(const std::vector<olp::clientmap::datastore::TileLoadResult::LayerResponseType>& layers)
        : layers_(layers) {}

    // ==========================
    // 获取必选 layer，如果找不到或 cast 失败会抛异常
    template <typename LayerType>
    const LayerType& GetRequiredLayer(const std::string& layer_name) const {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [&](const auto& item) { return item.GetPayload().layer_name == layer_name; });

        if (it == layers_.end()) {
            throw std::runtime_error("Required layer not found: " + layer_name);
        }
        
        const LayerType* data = it->GetResult().template Cast<LayerType>();
        if (!data) {
            throw std::runtime_error("Failed to cast layer data pointer: " + layer_name);
        }

        return *data;
    }

    // ==========================
    // 获取可选 layer，如果找不到或 cast 失败返回 nullptr
    template <typename LayerType>
    const LayerType* TryGetLayer(const std::string& layer_name) const {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [&](const auto& item) { return item.GetPayload().layer_name == layer_name; });

        if (it == layers_.end()) {
            return nullptr;
        }

        return it->GetResult().template Cast<LayerType>();
    }

    uint32_t GetWorldCoordinateBits(const std::string& layer_name) const {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [&](const auto& item) { return item.GetPayload().layer_name == layer_name; });

        if (it == layers_.end()) {
            throw std::runtime_error("Required layer not found: " + layer_name);
        }

        auto config = it->GetResult().GetConfiguration();
        if (!config) {
            throw std::runtime_error("Layer has no configuration: " + layer_name);
        }
        return config->world_coordinate_bits();
    }

private:
    const std::vector<olp::clientmap::datastore::TileLoadResult::LayerResponseType>& layers_;
};

}  // namespace ocm
}  // namespace maps
}  // namespace ning