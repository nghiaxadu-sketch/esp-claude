// Brick layouts for "Pha gach" (plain C++, unit-tested on a PC). The first kDesignedCount levels are drawn
// by hand; later ones are generated, and every layout differs from all levels before it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace bricks_levels {

constexpr int kCols = 10;
constexpr int kRows = 6;

// Hits a brick needs: 0 = no brick, 1 = normal, 2 = hard (turns normal after the first hit).
enum : uint8_t { kEmpty = 0, kNormal = 1, kHard = 2 };

using Layout = uint8_t[kRows][kCols];

extern const size_t kDesignedCount;

// Fills `out` with level `level` (0-based). The same level always gives the same layout.
void build(int level, Layout &out);

// Number of bricks that can be hit (cells that are not empty).
int brickCount(const Layout &layout);

}  // namespace bricks_levels
