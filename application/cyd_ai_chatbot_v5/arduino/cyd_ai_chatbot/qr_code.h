// Tiny QR code encoder (byte mode, versions 1-6, error correction level M) for short texts such as
// the board's web address. Plain C++ so it can be unit-tested on a PC: see test/qr_code_test.cpp.
#pragma once

#include <cstdint>
#include <string>

namespace qr_code {

constexpr int kMaxVersion = 6;
constexpr int kMaxSize = 17 + 4 * kMaxVersion;  // 41 modules
constexpr int kAutoMask = -1;

struct Code {
    int version = 0;
    int size = 0;  // modules per side, 0 = nothing encoded
    int mask = 0;
    uint8_t modules[kMaxSize][kMaxSize] = {};  // [row][column], 1 = dark

    bool dark(int x, int y) const { return x >= 0 && y >= 0 && x < size && y < size && modules[y][x] != 0; }
};

// Longest text that still fits (106 bytes at version 6, level M).
size_t maxBytes();

// Encodes `text` (any bytes, UTF-8 is fine) in the smallest version that fits. With mask = kAutoMask the
// mask with the lowest penalty is chosen, otherwise 0..7 forces one. Returns false when the text is too long.
bool encode(const std::string &text, Code &out, int mask = kAutoMask);

}  // namespace qr_code
