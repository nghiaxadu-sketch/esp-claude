// Host test for the QR encoder (no board needed):
//   g++ -std=c++17 -Wall -Wextra -Werror -I.. ../qr_code.cpp qr_code_test.cpp -o qr_code_test && ./qr_code_test
// The expected matrix was made with the Python "qrcode" package (version 2, level M, mask 3, byte mode).
#include <cstdio>
#include <string>

#include "qr_code.h"

static int failures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

static const char *const kExpected[] = {
    "#######.#...#.#...#######",
    "#.....#.##.####...#.....#",
    "#.###.#.....####..#.###.#",
    "#.###.#.######....#.###.#",
    "#.###.#...#..#.#..#.###.#",
    "#.....#..#.#.#....#.....#",
    "#######.#.#.#.#.#.#######",
    "........#.#..............",
    "#.##.###..###.....#..#.##",
    ".#.#.#.#..####...#.....#.",
    "#...###.##...#...#.##....",
    "...#.#.##.#..###..#####..",
    "##....#..#........###.###",
    ".#.##..###.#.###.##.#...#",
    ".##...#####.#.#.###...##.",
    "#.#.##..##..##.#.....#..#",
    "......#.####...##########",
    "........###.##.##...#...#",
    "#######.#.###...#.#.#####",
    "#.....#.#...#...#...#..#.",
    "#.###.#..#.###..#####..#.",
    "#.###.#.#.#.#####.###.###",
    "#.###.#.######.#..#.####.",
    "#.....#..#.....#....#.#..",
    "#######.#.#.#.##.########",
};

static void testKnownMatrix()
{
    qr_code::Code code;
    EXPECT(qr_code::encode("http://192.168.1.39/", code, 3));
    EXPECT(code.version == 2);
    EXPECT(code.size == 25);
    EXPECT(code.mask == 3);
    for (int y = 0; y < code.size && y < 25; ++y) {
        std::string row;
        for (int x = 0; x < code.size; ++x) {
            row += code.dark(x, y) ? '#' : '.';
        }
        if (row != kExpected[y]) {
            std::printf("FAIL row %d\n  got:  %s\n  want: %s\n", y, row.c_str(), kExpected[y]);
            ++failures;
        }
    }
}

static void testVersionsAndLimits()
{
    qr_code::Code code;
    EXPECT(qr_code::encode("a", code));
    EXPECT(code.version == 1 && code.size == 21);
    EXPECT(code.mask >= 0 && code.mask < 8);
    // Finder pattern corners are always dark, the separator next to them light.
    EXPECT(code.dark(0, 0) && code.dark(20, 0) && code.dark(0, 20));
    EXPECT(!code.dark(7, 0) && !code.dark(13, 0));
    EXPECT(!code.dark(-1, 0) && !code.dark(21, 0));

    EXPECT(qr_code::encode(std::string(qr_code::maxBytes(), 'z'), code));
    EXPECT(code.version == qr_code::kMaxVersion && code.size == qr_code::kMaxSize);
    EXPECT(!qr_code::encode(std::string(qr_code::maxBytes() + 1, 'z'), code));
    EXPECT(code.size == 0);
    EXPECT(!qr_code::encode("a", code, 8));
}

int main()
{
    testKnownMatrix();
    testVersionsAndLimits();
    if (failures == 0) {
        std::printf("qr_code: all tests passed\n");
        return 0;
    }
    std::printf("qr_code: %d failure(s)\n", failures);
    return 1;
}
