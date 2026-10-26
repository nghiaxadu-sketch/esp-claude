#include "games.h"

namespace {

using namespace ui;

constexpr int16_t kCell = 16;
constexpr int16_t kBoardW = CaroGame::kCols * kCell;
constexpr int16_t kBoardH = CaroGame::kRows * kCell;
constexpr int16_t kOriginX = (kScreenW - kBoardW) / 2;
constexpr int16_t kOriginY = kHeaderH + (kScreenH - kHeaderH - kBoardH) / 2;
constexpr int16_t kMark = 5;  // half size of X / radius of O
constexpr int kWinLength = 5;
constexpr int kNearDistance = 2;
constexpr uint32_t kCpuThinkMs = 300;
constexpr uint16_t kBoardColor = rgb565(0x14, 0x18, 0x22);
constexpr uint16_t kGridColor = rgb565(0x34, 0x3A, 0x48);
constexpr uint16_t kPlayerColor = rgb565(0x7F, 0xD1, 0xFF);
constexpr uint16_t kCpuColor = kColorAccent;
constexpr uint16_t kLastMoveColor = rgb565(0x4A, 0x3A, 0x22);
constexpr uint16_t kWinColor = rgb565(0xFF, 0xD5, 0x4F);

// Value of a line of `count` stones with `open` free ends (0..2) for the side that would play there.
int patternValue(int count, int open)
{
    if (count >= kWinLength) {
        return 100000;
    }
    if (open == 0) {
        return 0;
    }
    static const int kOpenTwo[kWinLength] = {0, 10, 200, 2500, 20000};
    static const int kOpenOne[kWinLength] = {0, 2, 30, 300, 3000};
    return open == 2 ? kOpenTwo[count] : kOpenOne[count];
}

const int kDirections[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

}  // namespace

void CaroGame::start()
{
    playerWins_ = cpuWins_ = 0;
    playerStarts_ = true;
    newRound();
}

void CaroGame::newRound()
{
    memset(board_, 0, sizeof(board_));
    lastCol_ = lastRow_ = -1;
    moves_ = 0;
    over_ = false;
    drawBoard();
    cpuToMove_ = !playerStarts_;
    cpuDueMs_ = millis() + kCpuThinkMs;
    playerStarts_ = !playerStarts_;  // the other side starts next round
}

void CaroGame::drawStatus()
{
    drawHud("Ban " + String(playerWins_) + " - " + String(cpuWins_) + " May");
}

void CaroGame::drawBoard()
{
    gfx_.fillScreen(kColorBlack);
    drawStatus();
    gfx_.fillRect(kOriginX, kOriginY, kBoardW, kBoardH, kBoardColor);
    for (int c = 0; c <= kCols; ++c) {
        gfx_.drawFastVLine(kOriginX + c * kCell, kOriginY, kBoardH + 1, kGridColor);
    }
    for (int r = 0; r <= kRows; ++r) {
        gfx_.drawFastHLine(kOriginX, kOriginY + r * kCell, kBoardW + 1, kGridColor);
    }
}

void CaroGame::drawCell(int col, int row)
{
    const int16_t x = kOriginX + col * kCell, y = kOriginY + row * kCell;
    const int16_t cx = x + kCell / 2, cy = y + kCell / 2;
    const bool last = col == lastCol_ && row == lastRow_;
    gfx_.fillRect(x + 1, y + 1, kCell - 1, kCell - 1, last ? kLastMoveColor : kBoardColor);
    if (board_[row][col] == Player) {
        for (int16_t t = 0; t < 2; ++t) {
            gfx_.drawLine(cx - kMark + t, cy - kMark, cx + kMark + t - 1, cy + kMark, kPlayerColor);
            gfx_.drawLine(cx + kMark + t - 1, cy - kMark, cx - kMark + t, cy + kMark, kPlayerColor);
        }
    } else if (board_[row][col] == Cpu) {
        gfx_.drawCircle(cx, cy, kMark, kCpuColor);
        gfx_.drawCircle(cx, cy, kMark - 1, kCpuColor);
    }
}

bool CaroGame::fiveFrom(int col, int row, int &dc, int &dr, int &startCol, int &startRow) const
{
    const uint8_t who = board_[row][col];
    for (const auto &d : kDirections) {
        int back = 0, fwd = 0;
        while (true) {
            const int c = col - d[0] * (back + 1), r = row - d[1] * (back + 1);
            if (c < 0 || r < 0 || c >= kCols || r >= kRows || board_[r][c] != who) {
                break;
            }
            ++back;
        }
        while (true) {
            const int c = col + d[0] * (fwd + 1), r = row + d[1] * (fwd + 1);
            if (c < 0 || r < 0 || c >= kCols || r >= kRows || board_[r][c] != who) {
                break;
            }
            ++fwd;
        }
        if (back + fwd + 1 >= kWinLength) {
            dc = d[0];
            dr = d[1];
            startCol = col - d[0] * back;
            startRow = row - d[1] * back;
            return true;
        }
    }
    return false;
}

// Returns true when the move ended the round.
bool CaroGame::place(int col, int row, Cell who)
{
    board_[row][col] = who;
    ++moves_;
    const int prevCol = lastCol_, prevRow = lastRow_;
    lastCol_ = col;
    lastRow_ = row;
    if (prevCol >= 0) {
        drawCell(prevCol, prevRow);
    }
    drawCell(col, row);
    sound(who == Player ? 1200 : 800, 30);

    int dc, dr, sc, sr;
    if (fiveFrom(col, row, dc, dr, sc, sr)) {
        for (int t = -1; t <= 1; ++t) {
            const int16_t x0 = kOriginX + sc * kCell + kCell / 2, y0 = kOriginY + sr * kCell + kCell / 2;
            const int16_t x1 = x0 + dc * (kWinLength - 1) * kCell, y1 = y0 + dr * (kWinLength - 1) * kCell;
            gfx_.drawLine(x0 + (dr != 0 ? t : 0), y0 + (dr == 0 ? t : 0), x1 + (dr != 0 ? t : 0),
                          y1 + (dr == 0 ? t : 0), kWinColor);
        }
        over_ = true;
        const bool playerWon = who == Player;
        if (playerWon) {
            ++playerWins_;
        } else {
            ++cpuWins_;
        }
        drawStatus();
        sound(playerWon ? 1800 : 300, 400);
        drawBanner(playerWon ? "Ban thang!" : "May thang!", "Cham de choi van moi", playerWon ? kColorOk : kCpuColor);
        return true;
    }
    if (moves_ >= kCols * kRows) {
        over_ = true;
        drawBanner("Hoa!", "Cham de choi van moi", kColorText);
        return true;
    }
    return false;
}

int CaroGame::lineScore(int col, int row, int dc, int dr, Cell who) const
{
    int count = 1, open = 0;
    for (int sign = -1; sign <= 1; sign += 2) {
        int c = col + sign * dc, r = row + sign * dr;
        while (c >= 0 && r >= 0 && c < kCols && r < kRows && board_[r][c] == who) {
            ++count;
            c += sign * dc;
            r += sign * dr;
        }
        if (c >= 0 && r >= 0 && c < kCols && r < kRows && board_[r][c] == Empty) {
            ++open;
        }
    }
    return patternValue(count, open);
}

int CaroGame::cellScore(int col, int row, Cell who) const
{
    int score = 0;
    for (const auto &d : kDirections) {
        score += lineScore(col, row, d[0], d[1], who);
    }
    return score;
}

void CaroGame::cpuMove()
{
    cpuToMove_ = false;
    if (moves_ == 0) {
        place(kCols / 2, kRows / 2, Cpu);
        return;
    }
    long best = -1;
    int bestCol = -1, bestRow = -1, ties = 0;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (board_[r][c] != Empty) {
                continue;
            }
            bool near = false;
            for (int dr = -kNearDistance; dr <= kNearDistance && !near; ++dr) {
                for (int dc = -kNearDistance; dc <= kNearDistance && !near; ++dc) {
                    const int rr = r + dr, cc = c + dc;
                    near = rr >= 0 && cc >= 0 && rr < kRows && cc < kCols && board_[rr][cc] != Empty;
                }
            }
            if (!near) {
                continue;
            }
            // Attacking counts a little more than defending, so the board takes a win over a block.
            const long score = (long)cellScore(c, r, Cpu) * 11 / 10 + cellScore(c, r, Player);
            if (score > best) {
                best = score;
                bestCol = c;
                bestRow = r;
                ties = 1;
            } else if (score == best && random(++ties) == 0) {
                bestCol = c;
                bestRow = r;
            }
        }
    }
    if (bestCol >= 0) {
        place(bestCol, bestRow, Cpu);
    }
}

void CaroGame::update(uint32_t nowMs)
{
    if (cpuToMove_ && !over_ && (int32_t)(nowMs - cpuDueMs_) >= 0) {
        cpuMove();
    }
}

void CaroGame::press(int16_t x, int16_t y)
{
    if (over_) {
        newRound();
        return;
    }
    if (cpuToMove_ || x < kOriginX || y < kOriginY || x >= kOriginX + kBoardW || y >= kOriginY + kBoardH) {
        return;
    }
    const int col = (x - kOriginX) / kCell, row = (y - kOriginY) / kCell;
    if (board_[row][col] != Empty) {
        return;
    }
    if (!place(col, row, Player)) {
        cpuToMove_ = true;
        cpuDueMs_ = millis() + kCpuThinkMs;
    }
}
