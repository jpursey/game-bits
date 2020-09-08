#ifndef BLOCK_H_
#define BLOCK_H_

#include <stdint.h>

#include "glm/glm.hpp"

// ID for a block in a chunk.
using BlockId = uint8_t;

// Available block types.
inline constexpr BlockId kBlockAir = 0;
inline constexpr BlockId kBlockDirt = 1;
inline constexpr BlockId kBlockGrass = 2;
inline constexpr BlockId kBlockRock1 = 3;
inline constexpr BlockId kBlockRock2 = 4;

inline constexpr float kBlockTextureSize = 256.0f;
inline constexpr float kBlockTextureTileCount = 2.0f;
inline constexpr float kBlockTexturePixelSize = 1.0f / kBlockTextureSize;
inline constexpr float kBlockUvScale = 1.0f / kBlockTextureTileCount;
inline constexpr float kBlockUvEndScale =
    kBlockUvScale - kBlockTexturePixelSize;

inline constexpr glm::vec2 kBlockUvOffset[5] = {
    {0, 0},                                  // kBlockAir
    {1 * kBlockUvScale, 1 * kBlockUvScale},  // kBlockDirt
    {0 * kBlockUvScale, 1 * kBlockUvScale},  // kBlockGrass
    {0 * kBlockUvScale, 0 * kBlockUvScale},  // kBlockRock1
    {1 * kBlockUvScale, 0 * kBlockUvScale},  // kBlockRock2
};

#endif BLOCK_H_
