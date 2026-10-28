// Chess rules and a small alpha-beta player for the "Co vua" game (plain C++, unit-tested on a PC with perft).
// Squares are 0..63 row by row from the top-left: 0 = a8, 7 = h8, 56 = a1, 63 = h1. White plays up the screen.
#pragma once

#include <cstdint>

namespace chess {

constexpr int8_t kWhite = 1;
constexpr int8_t kBlack = -1;
constexpr int kSquares = 64;
constexpr int kMaxMoves = 256;  // more than any position can have (218)
constexpr int kMaxPly = 10;     // search depth plus capture extensions

// Board values: piece type times the color (+ white, - black), 0 = empty.
enum Piece : int8_t { kEmpty = 0, kPawn = 1, kKnight = 2, kBishop = 3, kRook = 4, kQueen = 5, kKing = 6 };

enum : uint8_t { kCastleWK = 1, kCastleWQ = 2, kCastleBK = 4, kCastleBQ = 8 };
enum : uint8_t { kFlagCapture = 1, kFlagEnPassant = 2, kFlagCastle = 4, kFlagDoublePush = 8 };

struct Move {
    uint8_t from = 0;
    uint8_t to = 0;
    int8_t promo = kEmpty;  // piece type a pawn becomes, or kEmpty
    uint8_t flags = 0;
    bool operator==(const Move &o) const { return from == o.from && to == o.to && promo == o.promo; }
};

struct Position {
    int8_t board[kSquares];
    int8_t side;        // kWhite or kBlack to move
    uint8_t castling;   // kCastle* bits still allowed
    int8_t epSquare;    // square a pawn can capture en passant onto, or -1
    uint8_t halfmove;   // moves since the last capture or pawn move (50-move rule)
    uint16_t fullmove;
    uint8_t kings[2];   // king squares, [0] white, [1] black
};

enum class Result : uint8_t { Ongoing, Checkmate, Stalemate, FiftyMoves, Material };

void setStart(Position &pos);
// Standard FEN (tests and debugging); false when it cannot be read.
bool setFen(Position &pos, const char *fen);

bool squareAttacked(const Position &pos, int square, int8_t bySide);
bool inCheck(const Position &pos, int8_t side);
// Legal moves for the side to move into out[kMaxMoves]; returns how many.
int legalMoves(const Position &pos, Move *out);
void makeMove(Position &pos, const Move &move);
// `scratch` holds kMaxMoves moves (used for the legal move count).
Result result(const Position &pos, Move *scratch);

// Material and piece placement from the side to move's point of view (centipawns).
int evaluate(const Position &pos);

// Iterative deepening alpha-beta. Owns its move buffers, so nothing large goes on the task stack.
class Searcher {
public:
    // Picks a move for the side to move. `noise` adds up to that many centipawns of randomness per root
    // move (an easier, less predictable player); `nodeLimit` bounds the thinking time.
    bool findBest(const Position &pos, int maxDepth, uint32_t nodeLimit, int noise, uint32_t seed, Move &best);
    uint32_t nodes() const { return nodes_; }

private:
    int negamax(const Position &pos, int depth, int alpha, int beta, int ply);
    int quiesce(const Position &pos, int alpha, int beta, int ply);
    void pickNext(Move *moves, int count, int start, const Position &pos) const;

    Move moves_[kMaxPly][kMaxMoves];
    int rootNoise_[kMaxMoves];
    uint32_t nodes_ = 0;
    uint32_t nodeLimit_ = 0;
    bool aborted_ = false;
};

}  // namespace chess
