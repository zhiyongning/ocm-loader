#include "TileIDConverter.hpp"
#include <sstream>
#include <algorithm>
#include <cassert>
#include <iostream> 

// ------------------------- TileToQuadkey 实现 -------------------------

// 将数值转换为固定长度的二进制字符串（高位在前）
std::string to_binary_string(uint32_t num, uint32_t bits) {
   // assert(bits > 0 && "Bit length must be positive");
    assert(num < (1U << bits) && "Number exceeds bit length");

    std::string bin;
    for (int i = bits - 1; i >= 0; --i) {  // 从最高位开始遍历
        bin += (num & (1U << i)) ? '1' : '0';
    }
    return bin;
}

std::string TileIDConverter::TileToQuadkey(uint32_t x, uint32_t y, uint32_t level) {
    // 校验输入合法性
  //  assert(level > 0 && "Tile level must be positive");
    assert(x < (1U << level) && "X coordinate exceeds tile level");
    assert(y < (1U << level) && "Y coordinate exceeds tile level");

    // 步骤1：将 X/Y 转换为固定长度的二进制字符串（高位在前）
    std::string x_bin = to_binary_string(x, level);
    std::string y_bin = to_binary_string(y, level);

    // 步骤2：交错 Y/X 的二进制位，转换为四进制字符串
    std::string quadkey;
    for (int i = 0; i < level; ++i) {
        // 取 Y 和 X 的第 i 位（从高位到低位）
        char y_bit = y_bin[i];
        char x_bit = x_bin[i];

        // 组合两位二进制并转换为四进制（00->0, 01->1, 10->2, 11->3）
        std::string two_bits = std::string(1, y_bit) + x_bit;
        int value = std::stoi(two_bits, nullptr, 2);  // 二进制转十进制
        quadkey += "0123"[value];  // 十进制转四进制字符
    }

    return quadkey;
}

// ------------------------- QuadkeyToHereTileId 实现 -------------------------

uint64_t TileIDConverter::QuadkeyToHereTileId(const std::string& quadkey) {
    // 校验输入合法性
    //if (quadkey.empty() || quadkey[0] != '1') {
     //   throw std::invalid_argument("Quadkey must start with '1'");
    //}
    if (!std::all_of(quadkey.begin() + 1, quadkey.end(), [](char c) {
        return c >= '0' && c <= '3';
    })) {
        //throw std::invalid_argument("Quadkey contains invalid characters (must be 0-3 after '1')");
    }

    // 步骤1：添加前缀 "1"（题目描述要求）
    std::string prefixed = "1" + quadkey;

    // 步骤2：四进制转十进制（使用 stoull）
    size_t parsedChars = 0;
    uint64_t hereId;
    try {
        hereId = std::stoull(prefixed, &parsedChars, 4);
    } catch (const std::out_of_range&) {
        throw std::overflow_error("Quadkey is too long, exceeds uint64_t capacity");
    } catch (const std::invalid_argument&) {
        // 理论上不会触发（已校验字符）
        throw std::invalid_argument("Invalid quadkey format");
    }

    // 校验是否所有字符都被解析
    if (parsedChars != prefixed.size()) {
        throw std::invalid_argument("Quadkey contains trailing invalid characters");
    }

    return hereId;
}

uint64_t TileIDConverter::XYtoTileId(uint32_t x, uint32_t y, uint32_t level){
    return QuadkeyToHereTileId(TileToQuadkey(x,y,level));
}