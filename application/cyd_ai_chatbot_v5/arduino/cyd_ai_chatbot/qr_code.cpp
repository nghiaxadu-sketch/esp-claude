#include "qr_code.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace qr_code {
namespace {

constexpr int kMaskCount = 8;
constexpr uint8_t kModeByte = 0x4;
constexpr int kModeBits = 4;
constexpr int kCountBits = 8;  // byte mode, versions 1-9
constexpr uint8_t kPadBytes[2] = {0xEC, 0x11};
constexpr uint16_t kFormatGenerator = 0x537;
constexpr uint16_t kFormatXor = 0x5412;
constexpr uint8_t kEclBitsM = 0;  // format bits of level M
constexpr uint16_t kGfPoly = 0x11D;
constexpr int kMaxEcc = 30;

// Penalty weights from the QR specification.
constexpr int kPenaltyRun = 3;
constexpr int kPenaltyBox = 3;
constexpr int kPenaltyFinder = 40;
constexpr int kPenaltyBalance = 10;

// Level M block layout for versions 1-6.
struct Layout {
    int eccPerBlock;
    int blocks;
    int dataPerBlock;  // all blocks are the same size up to version 6
    int alignPos;      // second alignment coordinate (first is always 6), 0 = none
};
constexpr Layout kLayouts[kMaxVersion + 1] = {
    {0, 0, 0, 0},   {10, 1, 16, 0},  {16, 1, 28, 18}, {26, 1, 44, 22},
    {18, 2, 32, 26}, {24, 2, 43, 30}, {16, 4, 27, 34},
};

struct Grid {
    int size;
    uint8_t dark[kMaxSize][kMaxSize];
    uint8_t function[kMaxSize][kMaxSize];

    void set(int x, int y, bool isDark)
    {
        dark[y][x] = isDark ? 1 : 0;
        function[y][x] = 1;
    }
};

uint8_t gfMultiply(uint8_t a, uint8_t b)
{
    uint16_t result = 0;
    for (int i = 7; i >= 0; --i) {
        result = (uint16_t)((result << 1) ^ ((result >> 7) * kGfPoly));
        result ^= ((b >> i) & 1) * a;
    }
    return (uint8_t)result;
}

// Coefficients of the Reed-Solomon generator polynomial of `degree`, highest power first (leading 1 dropped).
void rsGenerator(int degree, uint8_t *out)
{
    std::memset(out, 0, degree);
    out[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            out[j] = gfMultiply(out[j], root);
            if (j + 1 < degree) {
                out[j] ^= out[j + 1];
            }
        }
        root = gfMultiply(root, 0x02);
    }
}

void rsRemainder(const uint8_t *data, int len, const uint8_t *generator, int degree, uint8_t *out)
{
    std::memset(out, 0, degree);
    for (int i = 0; i < len; ++i) {
        const uint8_t factor = data[i] ^ out[0];
        std::memmove(out, out + 1, degree - 1);
        out[degree - 1] = 0;
        for (int j = 0; j < degree; ++j) {
            out[j] ^= gfMultiply(generator[j], factor);
        }
    }
}

void appendBits(std::vector<uint8_t> &bits, uint32_t value, int count)
{
    for (int i = count - 1; i >= 0; --i) {
        bits.push_back((value >> i) & 1);
    }
}

// Data codewords followed by the interleaved blocks and their error correction codewords.
std::vector<uint8_t> buildCodewords(const std::string &text, const Layout &layout)
{
    const int dataCount = layout.blocks * layout.dataPerBlock;
    std::vector<uint8_t> bits;
    appendBits(bits, kModeByte, kModeBits);
    appendBits(bits, (uint32_t)text.size(), kCountBits);
    for (unsigned char c : text) {
        appendBits(bits, c, 8);
    }
    const size_t capacity = (size_t)dataCount * 8;
    appendBits(bits, 0, (int)std::min<size_t>(4, capacity - bits.size()));  // terminator
    while (bits.size() % 8 != 0) {
        bits.push_back(0);
    }
    std::vector<uint8_t> data;
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (size_t j = 0; j < 8; ++j) {
            byte = (uint8_t)((byte << 1) | bits[i + j]);
        }
        data.push_back(byte);
    }
    for (size_t i = 0; data.size() < (size_t)dataCount; ++i) {
        data.push_back(kPadBytes[i % 2]);
    }

    uint8_t generator[kMaxEcc];
    rsGenerator(layout.eccPerBlock, generator);
    std::vector<uint8_t> ecc((size_t)layout.blocks * layout.eccPerBlock);
    for (int b = 0; b < layout.blocks; ++b) {
        rsRemainder(&data[(size_t)b * layout.dataPerBlock], layout.dataPerBlock, generator, layout.eccPerBlock,
                    &ecc[(size_t)b * layout.eccPerBlock]);
    }
    std::vector<uint8_t> out;
    for (int i = 0; i < layout.dataPerBlock; ++i) {
        for (int b = 0; b < layout.blocks; ++b) {
            out.push_back(data[(size_t)b * layout.dataPerBlock + i]);
        }
    }
    for (int i = 0; i < layout.eccPerBlock; ++i) {
        for (int b = 0; b < layout.blocks; ++b) {
            out.push_back(ecc[(size_t)b * layout.eccPerBlock + i]);
        }
    }
    return out;
}

void drawFinder(Grid &g, int cx, int cy)
{
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const int x = cx + dx, y = cy + dy;
            if (x < 0 || y < 0 || x >= g.size || y >= g.size) {
                continue;
            }
            const int dist = std::max(std::abs(dx), std::abs(dy));
            g.set(x, y, dist != 2 && dist != 4);
        }
    }
}

void drawAlignment(Grid &g, int cx, int cy)
{
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            g.set(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
        }
    }
}

void drawFormat(Grid &g, int mask)
{
    const uint16_t data = (uint16_t)(kEclBitsM << 3 | mask);
    uint16_t rem = data;
    for (int i = 0; i < 10; ++i) {
        rem = (uint16_t)((rem << 1) ^ ((rem >> 9) * kFormatGenerator));
    }
    const uint16_t bits = (uint16_t)((data << 10 | rem) ^ kFormatXor);
    auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; ++i) {
        g.set(8, i, bit(i));
    }
    g.set(8, 7, bit(6));
    g.set(8, 8, bit(7));
    g.set(7, 8, bit(8));
    for (int i = 9; i < 15; ++i) {
        g.set(14 - i, 8, bit(i));
    }
    for (int i = 0; i < 8; ++i) {
        g.set(g.size - 1 - i, 8, bit(i));
    }
    for (int i = 8; i < 15; ++i) {
        g.set(8, g.size - 15 + i, bit(i));
    }
    g.set(8, g.size - 8, true);  // the "dark module"
}

void drawFunctionPatterns(Grid &g, const Layout &layout)
{
    for (int i = 0; i < g.size; ++i) {
        g.set(6, i, i % 2 == 0);
        g.set(i, 6, i % 2 == 0);
    }
    drawFinder(g, 3, 3);
    drawFinder(g, g.size - 4, 3);
    drawFinder(g, 3, g.size - 4);
    if (layout.alignPos != 0) {
        drawAlignment(g, layout.alignPos, layout.alignPos);  // the others overlap finders
    }
    drawFormat(g, 0);  // reserves the format areas; redrawn with the real mask later
}

void placeData(Grid &g, const std::vector<uint8_t> &codewords)
{
    size_t i = 0;
    const size_t totalBits = codewords.size() * 8;
    for (int right = g.size - 1; right >= 1; right -= 2) {
        if (right == 6) {
            right = 5;
        }
        const bool upward = ((right + 1) & 2) == 0;
        for (int vert = 0; vert < g.size; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const int y = upward ? g.size - 1 - vert : vert;
                if (!g.function[y][x] && i < totalBits) {
                    g.dark[y][x] = (codewords[i >> 3] >> (7 - (i & 7))) & 1;
                    ++i;
                }
            }
        }
    }
}

bool maskBit(int mask, int x, int y)
{
    switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}

void applyMask(Grid &g, int mask)
{
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            if (!g.function[y][x] && maskBit(mask, x, y)) {
                g.dark[y][x] ^= 1;
            }
        }
    }
}

int penalty(const Grid &g)
{
    const int n = g.size;
    int score = 0;
    auto at = [&](int x, int y, bool columns) { return columns ? g.dark[x][y] : g.dark[y][x]; };
    static const uint8_t kFinderA[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
    static const uint8_t kFinderB[11] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
    for (int pass = 0; pass < 2; ++pass) {
        const bool columns = pass == 1;
        for (int y = 0; y < n; ++y) {
            int run = 1;
            for (int x = 1; x <= n; ++x) {
                if (x < n && at(x, y, columns) == at(x - 1, y, columns)) {
                    ++run;
                    continue;
                }
                if (run >= 5) {
                    score += kPenaltyRun + run - 5;
                }
                run = 1;
            }
            for (int x = 0; x + 11 <= n; ++x) {
                bool a = true, b = true;
                for (int k = 0; k < 11; ++k) {
                    a = a && at(x + k, y, columns) == kFinderA[k];
                    b = b && at(x + k, y, columns) == kFinderB[k];
                }
                score += (a ? kPenaltyFinder : 0) + (b ? kPenaltyFinder : 0);
            }
        }
    }
    int darkCount = 0;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            darkCount += g.dark[y][x];
            if (x + 1 < n && y + 1 < n) {
                const uint8_t c = g.dark[y][x];
                if (c == g.dark[y][x + 1] && c == g.dark[y + 1][x] && c == g.dark[y + 1][x + 1]) {
                    score += kPenaltyBox;
                }
            }
        }
    }
    const int total = n * n;
    const int k = (std::abs(darkCount * 20 - total * 10) + total - 1) / total - 1;
    return score + std::max(0, k) * kPenaltyBalance;
}

}  // namespace

size_t maxBytes()
{
    const Layout &l = kLayouts[kMaxVersion];
    return (size_t)(l.blocks * l.dataPerBlock) - 2;  // 4 mode bits + 8 count bits, rounded up
}

bool encode(const std::string &text, Code &out, int mask)
{
    out.size = 0;
    int version = 0;
    for (int v = 1; v <= kMaxVersion; ++v) {
        const int dataBits = kLayouts[v].blocks * kLayouts[v].dataPerBlock * 8;
        if (kModeBits + kCountBits + (int)text.size() * 8 <= dataBits) {
            version = v;
            break;
        }
    }
    if (version == 0 || mask >= kMaskCount) {
        return false;
    }
    const Layout &layout = kLayouts[version];
    static Grid base;  // ~3.4 KB, kept off the task stack
    static Grid trial;
    std::memset(&base, 0, sizeof(base));
    base.size = 17 + 4 * version;
    drawFunctionPatterns(base, layout);
    placeData(base, buildCodewords(text, layout));

    int bestMask = mask;
    if (mask < 0) {
        int bestScore = -1;
        for (int m = 0; m < kMaskCount; ++m) {
            trial = base;
            applyMask(trial, m);
            drawFormat(trial, m);
            const int score = penalty(trial);
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                bestMask = m;
            }
        }
    }
    trial = base;
    applyMask(trial, bestMask);
    drawFormat(trial, bestMask);

    out.version = version;
    out.size = trial.size;
    out.mask = bestMask;
    std::memset(out.modules, 0, sizeof(out.modules));
    for (int y = 0; y < trial.size; ++y) {
        std::memcpy(out.modules[y], trial.dark[y], (size_t)trial.size);
    }
    return true;
}

}  // namespace qr_code
