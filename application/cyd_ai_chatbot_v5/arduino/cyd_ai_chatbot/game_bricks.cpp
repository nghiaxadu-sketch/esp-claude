#include "games.h"

#include <math.h>

#include "settings_store.h"

namespace {

using namespace ui;

constexpr uint32_t kStepMs = 16;
constexpr int16_t kBrickGap = 3;
constexpr int16_t kBrickW = (kScreenW - 8 - (BricksGame::kCols - 1) * kBrickGap) / BricksGame::kCols;
constexpr int16_t kBrickH = 11;
constexpr int16_t kBricksX = (kScreenW - BricksGame::kCols * kBrickW - (BricksGame::kCols - 1) * kBrickGap) / 2;
constexpr int16_t kBricksY = kHeaderH + 18;
constexpr int16_t kBrickRadius = 3;
constexpr int16_t kPaddleW = 52;
constexpr int16_t kPaddleH = 6;
constexpr int16_t kPaddleY = kScreenH - 12;
constexpr int16_t kBall = 5;
constexpr float kBaseSpeed = 3.0f;
constexpr float kSpeedPerLevel = 0.25f;
constexpr float kMaxSpeed = 6.5f;
constexpr float kMaxBounceRad = 1.05f;  // ~60 degrees from vertical at the paddle edge
constexpr uint8_t kLives = 3;
constexpr uint32_t kHardBonus = 20;
constexpr const char *kBestKey = "hs_bricks";
constexpr uint16_t kRowColors[BricksGame::kRows] = {
    rgb565(0xE5, 0x39, 0x35), rgb565(0xF2, 0x5C, 0x2A), rgb565(0xFF, 0xB3, 0x00),
    rgb565(0x43, 0xA0, 0x47), rgb565(0x29, 0xB6, 0xF6), rgb565(0xAB, 0x47, 0xBC),
};
constexpr uint16_t kHardColor = rgb565(0xB0, 0xBE, 0xC5);
constexpr uint16_t kHardEdge = rgb565(0xEC, 0xEF, 0xF1);

}  // namespace

void BricksGame::start()
{
    best_ = settings_store::loadInt(kBestKey, 0);
    lives_ = kLives;
    level_ = 0;
    score_ = 0;
    over_ = false;
    newLevel();
}

void BricksGame::newLevel()
{
    gfx_.fillScreen(kColorBlack);
    bricks_levels::build(level_, bricks_);
    bricksLeft_ = bricks_levels::brickCount(bricks_);
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (bricks_[r][c] != bricks_levels::kEmpty) {
                drawBrick(c, r);
            }
        }
    }
    speed_ = min(kMaxSpeed, kBaseSpeed + kSpeedPerLevel * level_);
    paddleX_ = (kScreenW - kPaddleW) / 2.0f;
    drawnPaddleX_ = -1;
    drawnBallX_ = drawnBallY_ = -1;
    drawStatus();
    drawPaddle();
    resetBall();
    clock_.reset(millis());
}

void BricksGame::resetBall()
{
    launched_ = false;
    ballX_ = paddleX_ + (kPaddleW - kBall) / 2.0f;
    ballY_ = kPaddleY - kBall - 1;
    vx_ = vy_ = 0;
}

void BricksGame::drawStatus()
{
    String lives;
    for (uint8_t i = 0; i < lives_; ++i) {
        lives += "o";
    }
    drawHud("Man " + String(level_ + 1) + "  " + String(score_) + "  " + lives + "  KL " + String(best_));
}

void BricksGame::drawBrick(int col, int row)
{
    const int16_t x = kBricksX + col * (kBrickW + kBrickGap), y = kBricksY + row * (kBrickH + kBrickGap);
    const uint8_t hits = bricks_[row][col];
    if (hits == bricks_levels::kHard) {
        gfx_.fillRoundRect(x, y, kBrickW, kBrickH, kBrickRadius, kHardColor);
        gfx_.drawRoundRect(x, y, kBrickW, kBrickH, kBrickRadius, kHardEdge);
    } else {
        gfx_.fillRoundRect(x, y, kBrickW, kBrickH, kBrickRadius, hits != 0 ? kRowColors[row] : kColorBlack);
    }
}

void BricksGame::drawPaddle()
{
    const int16_t x = (int16_t)paddleX_;
    if (x == drawnPaddleX_) {
        return;
    }
    if (drawnPaddleX_ >= 0) {
        gfx_.fillRect(drawnPaddleX_, kPaddleY, kPaddleW, kPaddleH, kColorBlack);
    }
    gfx_.fillRoundRect(x, kPaddleY, kPaddleW, kPaddleH, 3, kColorWhite);
    drawnPaddleX_ = x;
}

bool BricksGame::hitBricks()
{
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (bricks_[r][c] == bricks_levels::kEmpty) {
                continue;
            }
            const float bx = kBricksX + c * (kBrickW + kBrickGap), by = kBricksY + r * (kBrickH + kBrickGap);
            if (ballX_ + kBall <= bx || ballX_ >= bx + kBrickW || ballY_ + kBall <= by || ballY_ >= by + kBrickH) {
                continue;
            }
            if (bricks_[r][c] == bricks_levels::kHard) {
                score_ += kHardBonus;
            }
            --bricks_[r][c];
            if (bricks_[r][c] == bricks_levels::kEmpty) {
                --bricksLeft_;
            }
            drawBrick(c, r);
            // Bounce on the axis with the smaller overlap.
            const float overlapX = min(ballX_ + kBall - bx, bx + kBrickW - ballX_);
            const float overlapY = min(ballY_ + kBall - by, by + kBrickH - ballY_);
            if (overlapX < overlapY) {
                vx_ = -vx_;
            } else {
                vy_ = -vy_;
            }
            score_ += (kRows - r) * 10;
            sound(600 + (kRows - r) * 150, 30);
            drawStatus();
            return true;
        }
    }
    return false;
}

void BricksGame::loseLife()
{
    sound(200, 300);
    if (lives_ > 0) {
        --lives_;
    }
    if (lives_ == 0) {
        over_ = true;
        const bool record = (int32_t)score_ > best_;
        if (record) {
            best_ = (int32_t)score_;
            settings_store::saveInt(kBestKey, best_);
        }
        drawStatus();
        drawBanner(record ? "Ky luc moi!" : "Het luot!", "Diem " + String(score_) + " - cham de choi lai",
                   record ? kColorOk : kColorAccent);
        return;
    }
    drawStatus();
    resetBall();
}

void BricksGame::step()
{
    if (!launched_) {
        ballX_ = paddleX_ + (kPaddleW - kBall) / 2.0f;
    } else {
        ballX_ += vx_;
        ballY_ += vy_;
        if (ballX_ < 0) {
            ballX_ = 0;
            vx_ = -vx_;
        } else if (ballX_ + kBall > kScreenW) {
            ballX_ = kScreenW - kBall;
            vx_ = -vx_;
        }
        if (ballY_ < kTop) {
            ballY_ = kTop;
            vy_ = -vy_;
        }
        if (vy_ > 0 && ballY_ + kBall >= kPaddleY && ballY_ + kBall <= kPaddleY + kPaddleH + vy_ &&
            ballX_ + kBall >= paddleX_ && ballX_ <= paddleX_ + kPaddleW) {
            const float offset = (ballX_ + kBall / 2.0f - (paddleX_ + kPaddleW / 2.0f)) / (kPaddleW / 2.0f);
            const float angle = constrain(offset, -1.0f, 1.0f) * kMaxBounceRad;
            vx_ = speed_ * sinf(angle);
            vy_ = -speed_ * cosf(angle);
            ballY_ = kPaddleY - kBall;
            sound(900, 20);
        }
        hitBricks();
        if (ballY_ > kScreenH) {
            if (drawnBallX_ >= 0) {
                gfx_.fillRect(drawnBallX_, drawnBallY_, kBall, kBall, kColorBlack);
                drawnBallX_ = -1;
            }
            loseLife();
            if (over_) {
                return;
            }
        }
        if (bricksLeft_ == 0) {
            ++level_;
            sound(1600, 300);
            newLevel();
            return;
        }
    }

    drawPaddle();
    const int16_t bx = (int16_t)ballX_, by = (int16_t)ballY_;
    if (bx != drawnBallX_ || by != drawnBallY_) {
        if (drawnBallX_ >= 0) {
            gfx_.fillRect(drawnBallX_, drawnBallY_, kBall, kBall, kColorBlack);
            if (drawnBallY_ + kBall >= kPaddleY) {
                drawnPaddleX_ = -1;  // the ball overlapped the paddle: repaint it
                drawPaddle();
            }
        }
        gfx_.fillRect(bx, by, kBall, kBall, kColorWhite);
        drawnBallX_ = bx;
        drawnBallY_ = by;
    }
}

void BricksGame::update(uint32_t nowMs)
{
    if (!over_) {
        clock_.run(nowMs, kStepMs, [this] { step(); });
    }
}

void BricksGame::press(int16_t x, int16_t y)
{
    if (over_) {
        start();
        return;
    }
    if (!launched_) {
        launched_ = true;
        const float angle = random(-40, 41) / 100.0f;
        vx_ = speed_ * sinf(angle);
        vy_ = -speed_ * cosf(angle);
    }
    drag(x, y);
}

void BricksGame::drag(int16_t x, int16_t y)
{
    (void)y;
    paddleX_ = constrain((float)(x - kPaddleW / 2), 0.0f, (float)(kScreenW - kPaddleW));
}
