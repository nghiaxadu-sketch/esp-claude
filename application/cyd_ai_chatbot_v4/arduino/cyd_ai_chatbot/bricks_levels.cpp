#include "bricks_levels.h"

#include <array>
#include <cstring>
#include <vector>

namespace bricks_levels {
namespace {

// '#' normal brick, '@' hard brick (2 hits), '.' empty. Each level is kRows strings of kCols chars.
const char *const kDesigned[][kRows] = {
    {"..........", "##########", "##########", "##########", "##########", ".........."},  // 1 wall
    {"....##....", "...####...", "..######..", ".########.", "##########", ".........."},  // 2 pyramid
    {"#.#.#.#.#.", ".#.#.#.#.#", "#.#.#.#.#.", ".#.#.#.#.#", "#.#.#.#.#.", ".#.#.#.#.#"},  // 3 checker
    {"@@@@@@@@@@", "@........@", "@.######.@", "@.######.@", "@........@", "@@@@@@@@@@"},  // 4 frame
    {"....##....", "..######..", "##########", "..######..", "....##....", ".........."},  // 5 diamond
    {"##..##..##", "##..##..##", "##..##..##", "##..##..##", "@@..@@..@@", ".........."},  // 6 columns
    {"..#....#..", "...#..#...", "..######..", ".##.##.##.", "##########", "#.#....#.#"},  // 7 invader
    {".##....##.", "##########", "##########", ".########.", "..######..", "....##...."},  // 8 heart
    {"##......##", ".##....##.", "..##..##..", "...####...", "....##....", ".........."},  // 9 V
    {"@@@@......", "...@@@@...", "......@@@@", "...@@@@...", "@@@@......", ".........."},  // 10 zigzag
    {"###....###", "#@#....#@#", "###....###", "#@#....#@#", "###....###", ".........."},  // 11 towers
    {"....##....", "....##....", "##########", "##########", "....##....", "....##...."},  // 12 cross
    {"#...#...#.", ".#...#...#", "..#...#...", "#..#...#..", ".#..#...#.", "..#..#...#"},  // 13 rain
    {"..######..", ".#......#.", "#.@....@.#", "#........#", "#.#....#.#", ".#.####.#."},  // 14 smiley
    {"#.#.##.#.#", "##########", "#@#@##@#@#", "##########", "##########", ".........."},  // 15 castle
    {"##########", "#@@@@@@@@#", "#@######@#", "#@@@@@@@@#", "##########", ".........."},  // 16 rings
    {"...#..#...", "..##..##..", ".###..###.", "####..####", ".###..###.", "..##..##.."},  // 17 arrows
    {"##########", "#........#", "#.@@@@@@.#", "#........#", "##########", "@........@"},  // 18 bridge
    {"##....##..", ".##..##..#", "..####..##", "...##..##.", "......##..", ".........."},  // 19 wave
    {"@@@@@@@@@@", "@########@", "@########@", "@########@", "@@@@@@@@@@", ".........."},  // 20 fortress
};

constexpr int kMinGeneratedBricks = 12;
constexpr uint32_t kMinDensityPercent = 45;
constexpr uint32_t kDensityRangePercent = 36;
constexpr uint32_t kBaseHardPercent = 10;
constexpr uint32_t kHardPercentPerLevel = 2;
constexpr uint32_t kMaxHardPercent = 40;
constexpr uint32_t kSeedMultiplier = 2654435761u;
constexpr int kHalfCols = kCols / 2;

using Cells = std::array<uint8_t, kRows * kCols>;

std::vector<Cells> generated;  // levels kDesignedCount.. built so far (in order)

uint32_t nextRandom(uint32_t &state)
{
    // xorshift32: small, fast and the same on the board and on a PC.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

Cells toCells(const Layout &layout)
{
    Cells cells;
    std::memcpy(cells.data(), layout, cells.size());
    return cells;
}

void designed(size_t index, Layout &out)
{
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const char ch = kDesigned[index][r][c];
            out[r][c] = ch == '@' ? kHard : (ch == '#' ? kNormal : kEmpty);
        }
    }
}

// A mirrored random layout; denser and with more hard bricks as levels go up.
void randomLayout(int level, uint32_t attempt, Layout &out)
{
    uint32_t state = (uint32_t)(level + 1) * kSeedMultiplier + attempt * 7919u + 1u;
    const uint32_t density = kMinDensityPercent + nextRandom(state) % kDensityRangePercent;
    const uint32_t hardStep = (uint32_t)(level - (int)kDesignedCount) * kHardPercentPerLevel;
    const uint32_t hard = kBaseHardPercent + (hardStep < kMaxHardPercent ? hardStep : kMaxHardPercent);
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kHalfCols; ++c) {
            uint8_t cell = kEmpty;
            if (nextRandom(state) % 100 < density) {
                cell = nextRandom(state) % 100 < hard ? kHard : kNormal;
            }
            out[r][c] = cell;
            out[r][kCols - 1 - c] = cell;
        }
    }
}

bool seenBefore(const Cells &cells)
{
    for (size_t i = 0; i < kDesignedCount; ++i) {
        Layout layout;
        designed(i, layout);
        if (toCells(layout) == cells) {
            return true;
        }
    }
    for (const Cells &g : generated) {
        if (g == cells) {
            return true;
        }
    }
    return false;
}

void generateNext()
{
    const int level = (int)(kDesignedCount + generated.size());
    Layout layout;
    for (uint32_t attempt = 0;; ++attempt) {
        randomLayout(level, attempt, layout);
        if (brickCount(layout) >= kMinGeneratedBricks && !seenBefore(toCells(layout))) {
            break;
        }
    }
    generated.push_back(toCells(layout));
}

}  // namespace

const size_t kDesignedCount = sizeof(kDesigned) / sizeof(kDesigned[0]);

void build(int level, Layout &out)
{
    if (level < 0) {
        level = 0;
    }
    if ((size_t)level < kDesignedCount) {
        designed((size_t)level, out);
        return;
    }
    const size_t index = (size_t)level - kDesignedCount;
    while (generated.size() <= index) {
        generateNext();
    }
    std::memcpy(out, generated[index].data(), generated[index].size());
}

int brickCount(const Layout &layout)
{
    int count = 0;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            count += layout[r][c] != kEmpty ? 1 : 0;
        }
    }
    return count;
}

}  // namespace bricks_levels
