#include "games.h"

namespace {

using namespace ui;

constexpr int16_t kCell = 60;
constexpr int16_t kBoardX = 14;
constexpr int16_t kBoardY = kHeaderH + 14;
constexpr int16_t kMark = 18;
constexpr int16_t kPanelX = 208;
constexpr int16_t kPanelW = kScreenW - kPanelX - 8;
constexpr Rect kLevelButton = {kPanelX, 112, kPanelW, 40};
constexpr Rect kNewButton = {kPanelX, 164, kPanelW, 40};
constexpr uint32_t kCpuThinkMs = 400;
constexpr int kEasyRandomPercent = 45;
constexpr uint16_t kPlayerColor = rgb565(0x7F, 0xD1, 0xFF);
constexpr uint16_t kCpuColor = kColorAccent;
constexpr uint16_t kWinColor = rgb565(0xFF, 0xD5, 0x4F);

const uint8_t kLines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                              {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

}  // namespace

void TicTacToeGame::start()
{
    playerWins_ = cpuWins_ = draws_ = 0;
    playerStarts_ = true;
    newRound();
}

void TicTacToeGame::newRound()
{
    memset(cells_, 0, sizeof(cells_));
    over_ = false;
    winLine_ = -1;
    cpuToMove_ = !twoPlayers_ && !playerStarts_;
    turn_ = playerStarts_ ? 1 : 2;
    cpuDueMs_ = millis() + kCpuThinkMs;
    playerStarts_ = !playerStarts_;
    drawAll();
}

void TicTacToeGame::drawAll()
{
    gfx_.fillScreen(kColorBlack);
    drawHud(twoPlayers_ ? "2 nguoi" : (hard_ ? "Kho" : "De"));
    for (int i = 1; i < 3; ++i) {
        gfx_.fillRect(kBoardX + i * kCell - 1, kBoardY, 3, 3 * kCell, kColorDivider);
        gfx_.fillRect(kBoardX, kBoardY + i * kCell - 1, 3 * kCell, 3, kColorDivider);
    }
    for (int i = 0; i < 9; ++i) {
        drawCell(i);
    }
    drawPanel();
}

void TicTacToeGame::drawCell(int i)
{
    const int16_t cx = kBoardX + (i % 3) * kCell + kCell / 2;
    const int16_t cy = kBoardY + (i / 3) * kCell + kCell / 2;
    const uint8_t *line = winLine_ >= 0 ? kLines[winLine_] : nullptr;
    const bool win = line != nullptr && (line[0] == i || line[1] == i || line[2] == i);
    if (cells_[i] == 1) {
        const uint16_t c = win ? kWinColor : kPlayerColor;
        for (int16_t t = -2; t <= 2; ++t) {
            gfx_.drawLine(cx - kMark + t, cy - kMark, cx + kMark + t, cy + kMark, c);
            gfx_.drawLine(cx + kMark + t, cy - kMark, cx - kMark + t, cy + kMark, c);
        }
    } else if (cells_[i] == 2) {
        const uint16_t c = win ? kWinColor : kCpuColor;
        for (int16_t r = kMark - 3; r <= kMark + 1; ++r) {
            gfx_.drawCircle(cx, cy, r, c);
        }
    }
}

void TicTacToeGame::drawPanel()
{
    gfx_.fillRect(kPanelX, kBoardY, kPanelW, kLevelButton.y - kBoardY, kColorBlack);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kPlayerColor);
    gfx_.setCursor(kPanelX, kBoardY + 4);
    gfx_.print((twoPlayers_ ? "X    " : "Ban  ") + String(playerWins_));
    gfx_.setTextColor(kCpuColor);
    gfx_.setCursor(kPanelX, kBoardY + 26);
    gfx_.print((twoPlayers_ ? "O    " : "May  ") + String(cpuWins_));
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kPanelX, kBoardY + 48);
    gfx_.print("Hoa  " + String(draws_));
    if (twoPlayers_) {
        gfx_.fillRect(kLevelButton.x, kLevelButton.y, kLevelButton.w, kLevelButton.h, kColorBlack);
        drawCentered(gfx_, over_ ? String() : String("Luot: ") + (turn_ == 1 ? "X" : "O"), kLevelButton.x,
                     kLevelButton.w, kLevelButton.y + 12, 2, turn_ == 1 ? kPlayerColor : kCpuColor);
    } else {
        drawButton(gfx_, kLevelButton, hard_ ? "Muc: Kho" : "Muc: De", kColorButton, kColorWhite);
    }
    drawButton(gfx_, kNewButton, "Van moi", kColorAccent, kColorWhite);
}

int TicTacToeGame::winner(const uint8_t *b) const
{
    for (const auto &l : kLines) {
        if (b[l[0]] != 0 && b[l[0]] == b[l[1]] && b[l[1]] == b[l[2]]) {
            return b[l[0]];
        }
    }
    for (int i = 0; i < 9; ++i) {
        if (b[i] == 0) {
            return 0;
        }
    }
    return 3;
}

int TicTacToeGame::minimax(uint8_t *b, bool cpuTurn, int depth) const
{
    const int w = winner(b);
    if (w == 2) {
        return 10 - depth;
    }
    if (w == 1) {
        return depth - 10;
    }
    if (w == 3) {
        return 0;
    }
    int best = cpuTurn ? -100 : 100;
    for (int i = 0; i < 9; ++i) {
        if (b[i] != 0) {
            continue;
        }
        b[i] = cpuTurn ? 2 : 1;
        const int score = minimax(b, !cpuTurn, depth + 1);
        b[i] = 0;
        best = cpuTurn ? max(best, score) : min(best, score);
    }
    return best;
}

int TicTacToeGame::bestMove()
{
    int empty[9], count = 0;
    for (int i = 0; i < 9; ++i) {
        if (cells_[i] == 0) {
            empty[count++] = i;
        }
    }
    if (count == 0) {
        return -1;
    }
    if (!hard_ && random(100) < kEasyRandomPercent) {
        return empty[random(count)];
    }
    int best = -100, choice = empty[0], ties = 0;
    for (int k = 0; k < count; ++k) {
        cells_[empty[k]] = 2;
        const int score = minimax(cells_, false, 1);
        cells_[empty[k]] = 0;
        if (score > best) {
            best = score;
            choice = empty[k];
            ties = 1;
        } else if (score == best && random(++ties) == 0) {
            choice = empty[k];
        }
    }
    return choice;
}

void TicTacToeGame::endIfDone()
{
    const int w = winner(cells_);
    if (w == 0) {
        return;
    }
    over_ = true;
    for (int l = 0; l < 8 && w != 3; ++l) {
        const uint8_t *line = kLines[l];
        if (cells_[line[0]] == w && cells_[line[1]] == w && cells_[line[2]] == w) {
            winLine_ = l;
            for (int k = 0; k < 3; ++k) {
                drawCell(line[k]);
            }
            break;
        }
    }
    if (w == 1) {
        ++playerWins_;
        sound(1800, 300);
    } else if (w == 2) {
        ++cpuWins_;
        sound(300, 400);
    } else {
        ++draws_;
        sound(600, 150);
    }
    drawPanel();
    const char *msg = w == 1 ? (twoPlayers_ ? "X thang!" : "Ban thang!")
                             : (w == 2 ? (twoPlayers_ ? "O thang!" : "May thang!") : "Hoa!");
    ui::drawCentered(gfx_, msg, kPanelX - 4, kPanelW + 8, kNewButton.y + kNewButton.h + 10, 2,
                     w == 1 ? kColorOk : (w == 2 ? kCpuColor : kColorText));
}

void TicTacToeGame::update(uint32_t nowMs)
{
    if (!cpuToMove_ || over_ || (int32_t)(nowMs - cpuDueMs_) < 0) {
        return;
    }
    cpuToMove_ = false;
    const int move = bestMove();
    if (move >= 0) {
        cells_[move] = 2;
        drawCell(move);
        sound(800, 30);
    }
    endIfDone();
}

void TicTacToeGame::press(int16_t x, int16_t y)
{
    if (!twoPlayers_ && kLevelButton.contains(x, y)) {
        hard_ = !hard_;
        drawHud(hard_ ? "Kho" : "De");
        drawButton(gfx_, kLevelButton, hard_ ? "Muc: Kho" : "Muc: De", kColorButton, kColorWhite);
        return;
    }
    if (kNewButton.contains(x, y) || over_) {
        newRound();
        return;
    }
    if (cpuToMove_ || x < kBoardX || y < kBoardY || x >= kBoardX + 3 * kCell || y >= kBoardY + 3 * kCell) {
        return;
    }
    const int i = (y - kBoardY) / kCell * 3 + (x - kBoardX) / kCell;
    if (cells_[i] != 0) {
        return;
    }
    if (twoPlayers_) {
        cells_[i] = turn_;
        drawCell(i);
        sound(turn_ == 1 ? 1200 : 800, 30);
        turn_ = turn_ == 1 ? 2 : 1;
        endIfDone();
        drawPanel();
        return;
    }
    cells_[i] = 1;
    drawCell(i);
    sound(1200, 30);
    endIfDone();
    if (!over_) {
        cpuToMove_ = true;
        cpuDueMs_ = millis() + kCpuThinkMs;
    }
}
