// The built-in games. Pong, Caro and Tic-tac-toe are played against the board; Bricks and Snake keep a
// high score; Paint is a drawing board for the stylus.
#pragma once

#include "game.h"

// Pong against the board: drag the left paddle with the stylus, first to 7 points wins.
class PongGame : public Game {
public:
    using Game::Game;
    const char *title() const override { return "Pong"; }
    void start() override;
    void update(uint32_t nowMs) override;
    void press(int16_t x, int16_t y) override;
    void drag(int16_t x, int16_t y) override;

private:
    void serve(int8_t direction);
    void step();
    void drawPaddle(int16_t x, float &drawnY, float y);
    void drawNet(int16_t fromY, int16_t toY);
    void drawScore();
    void finish(bool playerWon);

    StepClock clock_;
    float ballX_ = 0, ballY_ = 0, vx_ = 0, vy_ = 0;
    int16_t drawnBallX_ = -1, drawnBallY_ = -1;
    float playerY_ = 0, cpuY_ = 0, drawnPlayerY_ = -1, drawnCpuY_ = -1;
    float cpuAim_ = 0;
    uint8_t playerScore_ = 0, cpuScore_ = 0;
    uint16_t serveDelay_ = 0;
    bool over_ = false;
};

// Caro (gomoku) against the board: five in a row wins.
class CaroGame : public Game {
public:
    using Game::Game;
    const char *title() const override { return "Caro"; }
    void start() override;
    void update(uint32_t nowMs) override;
    void press(int16_t x, int16_t y) override;

    static constexpr int kCols = 19;
    static constexpr int kRows = 13;

private:
    enum Cell : uint8_t { Empty = 0, Player = 1, Cpu = 2 };

    void newRound();
    void drawBoard();
    void drawCell(int col, int row);
    void drawStatus();
    bool place(int col, int row, Cell who);
    bool fiveFrom(int col, int row, int &dc, int &dr, int &startCol, int &startRow) const;
    int lineScore(int col, int row, int dc, int dr, Cell who) const;
    int cellScore(int col, int row, Cell who) const;
    void cpuMove();

    uint8_t board_[kRows][kCols] = {};
    int lastCol_ = -1, lastRow_ = -1;
    int moves_ = 0;
    bool over_ = false;
    bool cpuToMove_ = false;
    bool playerStarts_ = true;
    uint32_t cpuDueMs_ = 0;
    uint16_t playerWins_ = 0, cpuWins_ = 0;
};

// Tic-tac-toe against the board, easy or unbeatable.
class TicTacToeGame : public Game {
public:
    using Game::Game;
    const char *title() const override { return "Co XO"; }
    void start() override;
    void update(uint32_t nowMs) override;
    void press(int16_t x, int16_t y) override;

private:
    void newRound();
    void drawAll();
    void drawCell(int i);
    void drawPanel();
    int winner(const uint8_t *b) const;  // 0 none, 1 player, 2 cpu, 3 draw
    int minimax(uint8_t *b, bool cpuTurn, int depth) const;
    int bestMove();
    void endIfDone();

    uint8_t cells_[9] = {};
    bool hard_ = true;
    bool over_ = false;
    bool cpuToMove_ = false;
    bool playerStarts_ = true;
    uint32_t cpuDueMs_ = 0;
    int winLine_ = -1;
    uint16_t playerWins_ = 0, cpuWins_ = 0, draws_ = 0;
};

// Breakout: drag the paddle, break every brick.
class BricksGame : public Game {
public:
    using Game::Game;
    const char *title() const override { return "Pha gach"; }
    void start() override;
    void update(uint32_t nowMs) override;
    void press(int16_t x, int16_t y) override;
    void drag(int16_t x, int16_t y) override;

    static constexpr int kCols = 8;
    static constexpr int kRows = 5;

private:
    void newLevel();
    void resetBall();
    void step();
    void drawBrick(int col, int row);
    void drawPaddle();
    void drawStatus();
    bool hitBricks();
    void loseLife();

    StepClock clock_;
    bool bricks_[kRows][kCols] = {};
    int bricksLeft_ = 0;
    float ballX_ = 0, ballY_ = 0, vx_ = 0, vy_ = 0, speed_ = 0;
    int16_t drawnBallX_ = -1, drawnBallY_ = -1;
    float paddleX_ = 0;
    int16_t drawnPaddleX_ = -1;
    bool launched_ = false;
    bool over_ = false;
    uint8_t lives_ = 0;
    uint8_t level_ = 0;
    uint32_t score_ = 0;
    int32_t best_ = 0;
};

// Snake: tap on the side of the head where it should turn.
class SnakeGame : public Game {
public:
    using Game::Game;
    const char *title() const override { return "Con ran"; }
    void start() override;
    void update(uint32_t nowMs) override;
    void press(int16_t x, int16_t y) override;

    static constexpr int kCols = 26;
    static constexpr int kRows = 18;

private:
    struct Cell {
        int8_t col, row;
    };

    void newGame();
    void step();
    void placeFood();
    void drawFood();
    bool occupied(int col, int row) const;
    void drawCell(Cell c, uint16_t color);
    void drawStatus();

    StepClock clock_;
    Cell body_[kCols * kRows];
    int head_ = 0;    // index of the head in the ring buffer
    int length_ = 0;
    int8_t dirCol_ = 1, dirRow_ = 0;
    int8_t nextCol_ = 1, nextRow_ = 0;
    Cell food_ = {0, 0};
    uint16_t stepMs_ = 0;
    uint32_t score_ = 0;
    int32_t best_ = 0;
    bool over_ = false;
    bool started_ = false;
};

// Drawing board for the stylus: colors, pen size, eraser and clear.
class PaintGame : public Game {
public:
    using Game::Game;
    const char *title() const override { return "Ve tranh"; }
    void start() override;
    void update(uint32_t nowMs) override { (void)nowMs; }
    void press(int16_t x, int16_t y) override;
    void drag(int16_t x, int16_t y) override;
    void release() override { drawing_ = false; }

private:
    void drawPalette();
    void stroke(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

    uint8_t color_ = 0;
    uint8_t sizeIndex_ = 1;
    bool drawing_ = false;
    int16_t lastX_ = 0, lastY_ = 0;
};
