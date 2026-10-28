#include "games.h"

#include "settings_store.h"

namespace {

using namespace ui;

constexpr int16_t kCell = 12;
constexpr int16_t kOriginX = (kScreenW - SnakeGame::kCols * kCell) / 2;
constexpr int16_t kOriginY = kHeaderH + (kScreenH - kHeaderH - SnakeGame::kRows * kCell) / 2;
constexpr uint16_t kStartStepMs = 170;
constexpr uint16_t kMinStepMs = 75;
constexpr uint16_t kSpeedUpMs = 4;
constexpr int kStartLength = 4;
constexpr const char *kBestKey = "hs_snake";
constexpr uint16_t kFieldColor = rgb565(0x10, 0x18, 0x10);
constexpr uint16_t kBodyColor = rgb565(0x66, 0xBB, 0x6A);
constexpr uint16_t kHeadColor = rgb565(0xC5, 0xE1, 0xA5);
constexpr uint16_t kFoodColor = kColorAccent;

}  // namespace

void SnakeGame::start()
{
    best_ = settings_store::loadInt(kBestKey, 0);
    newGame();
}

void SnakeGame::newGame()
{
    gfx_.fillScreen(kColorBlack);
    gfx_.fillRect(kOriginX, kOriginY, kCols * kCell, kRows * kCell, kFieldColor);
    length_ = kStartLength;
    head_ = kStartLength - 1;
    for (int i = 0; i < kStartLength; ++i) {
        body_[i] = {(int8_t)(kCols / 4 + i), (int8_t)(kRows / 2)};
        drawCell(body_[i], i == head_ ? kHeadColor : kBodyColor);
    }
    dirCol_ = nextCol_ = 1;
    dirRow_ = nextRow_ = 0;
    stepMs_ = kStartStepMs;
    score_ = 0;
    over_ = false;
    started_ = false;
    placeFood();
    drawStatus();
    ui::drawCentered(gfx_, "Cham phia muon re de bat dau", kOriginX, kCols * kCell, kOriginY + 2, 1, kColorMuted);
    clock_.reset(millis());
}

void SnakeGame::drawStatus()
{
    drawHud("Diem " + String(score_) + "  KL " + String(best_));
}

void SnakeGame::drawCell(Cell c, uint16_t color)
{
    gfx_.fillRoundRect(kOriginX + c.col * kCell + 1, kOriginY + c.row * kCell + 1, kCell - 2, kCell - 2, 3, color);
}

bool SnakeGame::occupied(int col, int row) const
{
    for (int i = 0; i < length_; ++i) {
        const Cell &c = body_[(head_ - i + kCols * kRows) % (kCols * kRows)];
        if (c.col == col && c.row == row) {
            return true;
        }
    }
    return false;
}

void SnakeGame::placeFood()
{
    do {
        food_ = {(int8_t)random(kCols), (int8_t)random(kRows)};
    } while (occupied(food_.col, food_.row));
    drawFood();
}

void SnakeGame::drawFood()
{
    gfx_.fillCircle(kOriginX + food_.col * kCell + kCell / 2, kOriginY + food_.row * kCell + kCell / 2, kCell / 2 - 1,
                    kFoodColor);
}

void SnakeGame::step()
{
    dirCol_ = nextCol_;
    dirRow_ = nextRow_;
    const int capacity = kCols * kRows;
    const Cell head = body_[head_];
    const int col = head.col + dirCol_, row = head.row + dirRow_;
    const bool eats = col == food_.col && row == food_.row;
    // The tail moves away this step unless the snake grows, so the head may enter it.
    const Cell tail = body_[(head_ - length_ + 1 + capacity) % capacity];
    const bool hitsSelf = occupied(col, row) && !(!eats && col == tail.col && row == tail.row);
    if (col < 0 || row < 0 || col >= kCols || row >= kRows || hitsSelf) {
        over_ = true;
        sound(200, 400);
        const bool record = (int32_t)score_ > best_;
        if (record) {
            best_ = (int32_t)score_;
            settings_store::saveInt(kBestKey, best_);
        }
        drawStatus();
        drawBanner(record ? "Ky luc moi!" : "Thua roi!", "Diem " + String(score_) + " - cham de choi lai",
                   record ? kColorOk : kColorAccent);
        return;
    }
    if (!eats) {
        gfx_.fillRect(kOriginX + tail.col * kCell, kOriginY + tail.row * kCell, kCell, kCell, kFieldColor);
    } else {
        ++length_;
    }
    drawCell(head, kBodyColor);
    head_ = (head_ + 1) % capacity;
    body_[head_] = {(int8_t)col, (int8_t)row};
    drawCell(body_[head_], kHeadColor);
    if (eats) {
        score_ += 10;
        stepMs_ = max<uint16_t>(kMinStepMs, stepMs_ - kSpeedUpMs);
        sound(1500, 40);
        drawStatus();
        if (length_ >= capacity) {
            over_ = true;
            drawBanner("Ban thang!", "Ran da day san", kColorOk);
            return;
        }
        placeFood();
    }
}

void SnakeGame::update(uint32_t nowMs)
{
    if (started_ && !over_) {
        clock_.run(nowMs, stepMs_, [this] { step(); }, 1);
    }
}

void SnakeGame::press(int16_t x, int16_t y)
{
    if (over_) {
        newGame();
        return;
    }
    if (!started_) {
        started_ = true;
        gfx_.fillRect(kOriginX, kOriginY, kCols * kCell, kCell, kFieldColor);  // remove the hint
        if (food_.row == 0) {
            drawFood();
        }
        clock_.reset(millis());
    }
    // Turn towards the tap, measured from the head; reversing into the body is ignored.
    const Cell head = body_[head_];
    const int dx = x - (kOriginX + head.col * kCell + kCell / 2);
    const int dy = y - (kOriginY + head.row * kCell + kCell / 2);
    int8_t col = 0, row = 0;
    if (dirCol_ != 0) {
        row = dy < 0 ? -1 : 1;  // moving sideways: tap above or below to turn
    } else {
        col = dx < 0 ? -1 : 1;
    }
    if (col != -dirCol_ || row != -dirRow_) {
        nextCol_ = col;
        nextRow_ = row;
    }
}
