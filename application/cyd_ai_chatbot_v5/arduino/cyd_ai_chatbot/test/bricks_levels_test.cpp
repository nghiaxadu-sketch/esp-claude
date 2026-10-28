// Host test for the brick layouts (no board needed):
//   g++ -std=c++17 -Wall -Wextra -Werror -I.. ../bricks_levels.cpp bricks_levels_test.cpp -o bricks_levels_test
//       && ./bricks_levels_test
#include <cstdio>
#include <cstring>
#include <vector>

#include "bricks_levels.h"

using namespace bricks_levels;

static int failures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

int main()
{
    constexpr int kLevelsChecked = 200;
    std::vector<std::vector<uint8_t>> seen;
    for (int level = 0; level < kLevelsChecked; ++level) {
        Layout layout;
        build(level, layout);
        EXPECT(brickCount(layout) > 0);
        std::vector<uint8_t> cells(&layout[0][0], &layout[0][0] + kRows * kCols);
        for (uint8_t cell : cells) {
            EXPECT(cell == kEmpty || cell == kNormal || cell == kHard);
        }
        for (size_t other = 0; other < seen.size(); ++other) {
            if (seen[other] == cells) {
                std::printf("FAIL level %d repeats level %zu\n", level + 1, other + 1);
                ++failures;
            }
        }
        seen.push_back(cells);
    }
    // Same level, same layout (also when built again later).
    Layout a, b;
    build(kDesignedCount + 5, a);
    build(kDesignedCount + 5, b);
    EXPECT(std::memcmp(a, b, sizeof(a)) == 0);
    if (failures == 0) {
        std::printf("bricks_levels: all tests passed (%d levels, %zu designed)\n", kLevelsChecked, kDesignedCount);
    }
    return failures == 0 ? 0 : 1;
}
