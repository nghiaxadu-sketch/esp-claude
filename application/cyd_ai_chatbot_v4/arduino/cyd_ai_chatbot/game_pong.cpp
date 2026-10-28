#include "games.h"

namespace {

using namespace ui;

constexpr uint32_t kStepMs = 16;
constexpr int16_t kPaddleW = 5;
constexpr int16_t kPaddleH = 40;
constexpr int16_t kPlayerX = 6;
constexpr int16_t kCpuX = kScreenW - 6 - kPaddleW;
constexpr int16_t kBall = 6;
constexpr int16_t kNetX = kScreenW / 2 - 1;
constexpr int16_t kNetW = 2;
constexpr int16_t kNetDash = 6;
constexpr float kServeSpeed = 3.2f;
constexpr float kMaxSpeed = 7.0f;
constexpr float kSpeedUp = 1.06f;
constexpr float kSpin = 3.4f;
constexpr float kCpuBaseSpeed = 2.3f;
constexpr float kCpuSpeedPerPoint = 0.18f;
constexpr int16_t kCpuAimError = 16;
constexpr uint16_t kServeDelaySteps = 45;
constexpr uint8_t kWinScore = 7;
constexpr uint16_t kPlayerColor = rgb565(0x7F, 0xD1, 0xFF);
constexpr uint16_t kCpuColor = kColorAccent;

}  // namespace

void PongGame::start()
{
    gfx_.fillScreen(kColorBlack);
    playerScore_ = cpuScore_ = 0;
    over_ = false;
    playerY_ = cpuY_ = kTop + (kFieldH - kPaddleH) / 2.0f;
    drawnPlayerY_ = drawnCpuY_ = -1;
    drawnBallX_ = drawnBallY_ = -1;
    drawScore();
    drawNet(kTop, kScreenH);
    drawPaddle(kPlayerX, drawnPlayerY_, playerY_);
    drawPaddle(kCpuX, drawnCpuY_, cpuY_);
    serve(random(2) == 0 ? -1 : 1);
    clock_.reset(millis());
}

void PongGame::serve(int8_t direction)
{
    ballX_ = (kScreenW - kBall) / 2.0f;
    ballY_ = kTop + (kFieldH - kBall) / 2.0f;
    vx_ = direction * kServeSpeed;
    vy_ = random(10, 25) / 10.0f * (random(2) == 0 ? -1 : 1);
    cpuAim_ = random(-kCpuAimError, kCpuAimError + 1);
    serveDelay_ = kServeDelaySteps;
}

void PongGame::drawNet(int16_t fromY, int16_t toY)
{
    for (int16_t y = kTop + 2; y < kScreenH; y += 2 * kNetDash) {
        if (y + kNetDash > fromY && y < toY) {
            gfx_.fillRect(kNetX, y, kNetW, kNetDash, kColorDivider);
        }
    }
}

void PongGame::drawScore()
{
    drawHud(twoPlayers_ ? "P1 " + String(playerScore_) + " : " + String(cpuScore_) + " P2"
                        : "Ban " + String(playerScore_) + " : " + String(cpuScore_) + " May");
}

// Only repaints the strip the paddle left and the strip it entered.
void PongGame::drawPaddle(int16_t x, float &drawnY, float y)
{
    const uint16_t color = x == kPlayerX ? kPlayerColor : kCpuColor;
    const int16_t from = (int16_t)drawnY, to = (int16_t)y;
    if (from == to) {
        return;
    }
    if (drawnY < 0 || abs(to - from) >= kPaddleH) {
        if (drawnY >= 0) {
            gfx_.fillRect(x, from, kPaddleW, kPaddleH, kColorBlack);
        }
        gfx_.fillRect(x, to, kPaddleW, kPaddleH, color);
    } else if (to > from) {
        gfx_.fillRect(x, from, kPaddleW, to - from, kColorBlack);
        gfx_.fillRect(x, from + kPaddleH, kPaddleW, to - from, color);
    } else {
        gfx_.fillRect(x, to + kPaddleH, kPaddleW, from - to, kColorBlack);
        gfx_.fillRect(x, to, kPaddleW, from - to, color);
    }
    drawnY = y;
}

void PongGame::finish(bool playerWon)
{
    over_ = true;
    sound(playerWon ? 1800 : 300, 400);
    if (twoPlayers_) {
        drawBanner(playerWon ? "Nguoi 1 thang!" : "Nguoi 2 thang!", "Cham de choi lai",
                   playerWon ? kPlayerColor : kCpuColor);
        return;
    }
    drawBanner(playerWon ? "Ban thang!" : "May thang!", "Cham de choi lai", playerWon ? kColorOk : kColorAccent);
}

void PongGame::step()
{
    // The board's paddle follows the ball with a small aiming error and limited speed.
    if (!twoPlayers_) {
        const float target = vx_ > 0 ? ballY_ + kBall / 2.0f - kPaddleH / 2.0f + cpuAim_
                                     : kTop + (kFieldH - kPaddleH) / 2.0f;
        const float cpuSpeed = kCpuBaseSpeed + kCpuSpeedPerPoint * playerScore_;
        cpuY_ += constrain(target - cpuY_, -cpuSpeed, cpuSpeed);
        cpuY_ = constrain(cpuY_, (float)kTop, (float)(kScreenH - kPaddleH));
    }

    if (serveDelay_ > 0) {
        --serveDelay_;
    } else {
        ballX_ += vx_;
        ballY_ += vy_;
    }
    if (ballY_ < kTop) {
        ballY_ = kTop;
        vy_ = -vy_;
    } else if (ballY_ + kBall > kScreenH) {
        ballY_ = kScreenH - kBall;
        vy_ = -vy_;
    }

    auto hitPaddle = [&](float paddleY) {
        const float offset = (ballY_ + kBall / 2.0f - (paddleY + kPaddleH / 2.0f)) / (kPaddleH / 2.0f);
        vx_ = constrain(-vx_ * kSpeedUp, -kMaxSpeed, kMaxSpeed);
        vy_ = offset * kSpin;
    };
    if (vx_ < 0 && ballX_ <= kPlayerX + kPaddleW && ballX_ + kBall >= kPlayerX && ballY_ + kBall >= playerY_ &&
        ballY_ <= playerY_ + kPaddleH) {
        ballX_ = kPlayerX + kPaddleW;
        hitPaddle(playerY_);
        cpuAim_ = random(-kCpuAimError, kCpuAimError + 1);
        sound(900, 25);
    } else if (vx_ > 0 && ballX_ + kBall >= kCpuX && ballX_ <= kCpuX + kPaddleW && ballY_ + kBall >= cpuY_ &&
               ballY_ <= cpuY_ + kPaddleH) {
        ballX_ = kCpuX - kBall;
        hitPaddle(cpuY_);
        sound(700, 25);
    }

    if (ballX_ < -kBall || ballX_ > kScreenW) {
        const bool playerScored = ballX_ > kScreenW;
        if (playerScored) {
            ++playerScore_;
        } else {
            ++cpuScore_;
        }
        drawScore();
        sound(playerScored ? 1400 : 250, 150);
        if (drawnBallX_ >= 0) {
            gfx_.fillRect(drawnBallX_, drawnBallY_, kBall, kBall, kColorBlack);
            drawnBallX_ = -1;
        }
        if (playerScore_ >= kWinScore || cpuScore_ >= kWinScore) {
            finish(playerScore_ >= kWinScore);
            return;
        }
        serve(playerScored ? 1 : -1);
    }

    drawPaddle(kPlayerX, drawnPlayerY_, playerY_);
    drawPaddle(kCpuX, drawnCpuY_, cpuY_);
    const int16_t bx = (int16_t)ballX_, by = (int16_t)ballY_;
    if (bx != drawnBallX_ || by != drawnBallY_) {
        if (drawnBallX_ >= 0) {
            gfx_.fillRect(drawnBallX_, drawnBallY_, kBall, kBall, kColorBlack);
            if (drawnBallX_ + kBall > kNetX && drawnBallX_ < kNetX + kNetW) {
                drawNet(drawnBallY_, drawnBallY_ + kBall);
            }
        }
        gfx_.fillRect(bx, by, kBall, kBall, kColorWhite);
        drawnBallX_ = bx;
        drawnBallY_ = by;
    }
}

void PongGame::update(uint32_t nowMs)
{
    if (!over_) {
        clock_.run(nowMs, kStepMs, [this] { step(); });
    }
}

void PongGame::press(int16_t x, int16_t y)
{
    if (over_) {
        start();
        return;
    }
    drag(x, y);
}

// Against the board any touch moves the left paddle; with two players each half moves its own paddle.
void PongGame::drag(int16_t x, int16_t y)
{
    const float paddleY = constrain((float)(y - kPaddleH / 2), (float)kTop, (float)(kScreenH - kPaddleH));
    if (twoPlayers_ && x >= kScreenW / 2) {
        cpuY_ = paddleY;
    } else {
        playerY_ = paddleY;
    }
}
