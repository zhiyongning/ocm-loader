#ifndef TILE_ID_CONVERTER_HPP
#define TILE_ID_CONVERTER_HPP

#include <string>
#include <cstdint>
#include <stdexcept>

/**
 * @brief 工具类：提供 Tile ID 与 Morton quadkey 的相互转换功能
 */
class TileIDConverter {
public:
    /**
     * @brief 将 Tile(X, Y) 坐标转换为 Morton quadkey 字符串
     * 
     * @param x Tile X 坐标（uint32_t，范围 [0, 2^level - 1]）
     * @param y Tile Y 坐标（uint32_t，范围 [0, 2^level - 1]）
     * @param level Tile 级别（uint32_t，≥1）
     * @return std::string 生成的 Morton quadkey（仅包含 0-3 字符）
     * @throws std::invalid_argument 输入参数非法（坐标越界或 level 无效）
     */
    static std::string TileToQuadkey(uint32_t x, uint32_t y, uint32_t level);

    /**
     * @brief 将 Morton quadkey 字符串转换为 HERE Tile ID（十进制）
     * 
     * @param quadkey Morton quadkey 字符串（仅包含 0-3 字符，以 "1" 开头）
     * @return uint64_t 转换后的 HERE Tile ID（十进制）
     * @throws std::invalid_argument 输入 quadkey 非法（格式错误或字符越界）
     * @throws std::overflow_error quadkey 过长导致超出 uint64_t 范围
     */
    static uint64_t QuadkeyToHereTileId(const std::string& quadkey);

    static uint64_t XYtoTileId(uint32_t x, uint32_t y, uint32_t level);
};

#endif // TILE_ID_CONVERTER_HPP