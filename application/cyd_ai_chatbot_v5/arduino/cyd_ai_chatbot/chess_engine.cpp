#include "chess_engine.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace chess {
namespace {

constexpr int kMate = 30000;
constexpr int kInfinity = 32000;
constexpr int kMateThreshold = kMate - 1000;
constexpr int kFiftyMoveHalfmoves = 100;
constexpr int kEndgameMaterial = 1300;  // non-pawn material per side below which kings come out

// Corner squares (rooks start there) and king squares used by castling.
constexpr int kA8 = 0, kC8 = 2, kD8 = 3, kE8 = 4, kF8 = 5, kG8 = 6, kH8 = 7;
constexpr int kA1 = 56, kC1 = 58, kD1 = 59, kE1 = 60, kF1 = 61, kG1 = 62, kH1 = 63;
constexpr int kB8 = 1, kB1 = 57;

constexpr int kPieceValue[7] = {0, 100, 320, 330, 500, 900, 0};

const int8_t kKnightSteps[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};
const int8_t kKingSteps[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
const int8_t kRookDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
const int8_t kBishopDirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

// Piece-square tables (simplified evaluation function), white's view, row 0 = rank 8.
const int8_t kPawnTable[kSquares] = {
    0,  0,  0,  0,   0,   0,  0,  0,  50, 50, 50,  50, 50, 50,  50, 50, 10, 10, 20, 30, 30, 20,
    10, 10, 5,  5,  10,  25,  25, 10, 5,  5,  0,  0,   0,  20, 20, 0,   0,  0,  5,  -5, -10, 0,
    0,  -10, -5, 5, 5,  10, 10, -20, -20, 10, 10, 5,  0,  0,  0,  0,   0,  0,  0,  0};
const int8_t kKnightTable[kSquares] = {
    -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,   0,   -20, -40, -30, 0,   10,  15, 15, 10,
    0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,   -30, -30, 5,  10, 15,
    15,  10,  5,   -30, -40, -20, 0,   5,   5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};
const int8_t kBishopTable[kSquares] = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,   0,   0,   -10, -10, 0,   5,   10, 10, 5,
    0,   -10, -10, 5,   5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,   -10, -10, 10, 10, 10,
    10,  10,  10,  -10, -10, 5,   0,   0,   0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};
const int8_t kRookTable[kSquares] = {0,  0,  0,  0,  0,  0,  0,  0,  5,  10, 10, 10, 10, 10, 10, 5,
                                     -5, 0,  0,  0,  0,  0,  0,  -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                                     -5, 0,  0,  0,  0,  0,  0,  -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                                     -5, 0,  0,  0,  0,  0,  0,  -5, 0,  0,  0,  5,  5,  0,  0,  0};
const int8_t kQueenTable[kSquares] = {
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,  0,  -10, -10, 0,   5,   5,  5,  5,
    0,   -10, -5,  0,  5,  5,   5,   5,   0,   -5,  0,   0,  5,  5,  5,  5,   0,   -5,  -10, 5,  5,  5,
    5,   5,   0,   -10, -10, 0, 5,   0,   0,   0,   0,   -10, -20, -10, -10, -5, -5, -10, -10, -20};
const int8_t kKingMiddleTable[kSquares] = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40,
    -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20,
    -20, -20, -20, -10, 20,  20,  0,   0,   0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};
const int8_t kKingEndTable[kSquares] = {
    -50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0,   0,   -10, -20, -30, -30, -10, 20,  30,  30,  20,
    -10, -30, -30, -10, 30,  40,  40,  30,  -10, -30, -30, -10, 30,  40,  40,  30,  -10, -30, -30, -10, 20,  30,
    30,  20,  -10, -30, -30, -30, 0,   0,   0,   0,   -30, -30, -50, -30, -30, -30, -30, -30, -30, -50};

inline int rowOf(int sq) { return sq >> 3; }
inline int colOf(int sq) { return sq & 7; }
inline bool onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
inline int8_t colorOf(int8_t piece) { return piece > 0 ? kWhite : (piece < 0 ? kBlack : 0); }
inline int8_t typeOf(int8_t piece) { return piece >= 0 ? piece : (int8_t)-piece; }
inline int kingIndex(int8_t side) { return side == kWhite ? 0 : 1; }
// The same square seen from the other side (row flipped), for black's table lookups.
inline int mirror(int sq) { return (7 - rowOf(sq)) * 8 + colOf(sq); }

struct MoveList {
    Move *moves;
    int count;
    void add(int from, int to, int8_t promo, uint8_t flags)
    {
        if (count < kMaxMoves) {
            moves[count++] = {(uint8_t)from, (uint8_t)to, promo, flags};
        }
    }
};

void addPawnMove(MoveList &list, int from, int to, uint8_t flags, int8_t side)
{
    const int lastRow = side == kWhite ? 0 : 7;
    if (rowOf(to) == lastRow) {
        for (int8_t promo : {kQueen, kRook, kBishop, kKnight}) {
            list.add(from, to, promo, flags);
        }
    } else {
        list.add(from, to, kEmpty, flags);
    }
}

void pawnMoves(const Position &pos, int sq, MoveList &list)
{
    const int8_t side = pos.side;
    const int dir = side == kWhite ? -1 : 1;
    const int startRow = side == kWhite ? 6 : 1;
    const int r = rowOf(sq), c = colOf(sq);
    const int r1 = r + dir;
    if (!onBoard(r1, c)) {
        return;
    }
    const int one = r1 * 8 + c;
    if (pos.board[one] == kEmpty) {
        addPawnMove(list, sq, one, 0, side);
        const int two = (r + 2 * dir) * 8 + c;
        if (r == startRow && pos.board[two] == kEmpty) {
            list.add(sq, two, kEmpty, kFlagDoublePush);
        }
    }
    for (int dc = -1; dc <= 1; dc += 2) {
        if (!onBoard(r1, c + dc)) {
            continue;
        }
        const int target = r1 * 8 + c + dc;
        if (colorOf(pos.board[target]) == -side) {
            addPawnMove(list, sq, target, kFlagCapture, side);
        } else if (target == pos.epSquare) {
            list.add(sq, target, kEmpty, kFlagCapture | kFlagEnPassant);
        }
    }
}

void stepMoves(const Position &pos, int sq, const int8_t (*steps)[2], MoveList &list)
{
    const int r = rowOf(sq), c = colOf(sq);
    for (int i = 0; i < 8; ++i) {
        const int rr = r + steps[i][0], cc = c + steps[i][1];
        if (!onBoard(rr, cc)) {
            continue;
        }
        const int target = rr * 8 + cc;
        const int8_t owner = colorOf(pos.board[target]);
        if (owner != pos.side) {
            list.add(sq, target, kEmpty, owner != 0 ? kFlagCapture : 0);
        }
    }
}

void slideMoves(const Position &pos, int sq, const int8_t (*dirs)[2], MoveList &list)
{
    const int r = rowOf(sq), c = colOf(sq);
    for (int i = 0; i < 4; ++i) {
        int rr = r + dirs[i][0], cc = c + dirs[i][1];
        while (onBoard(rr, cc)) {
            const int target = rr * 8 + cc;
            const int8_t owner = colorOf(pos.board[target]);
            if (owner == pos.side) {
                break;
            }
            list.add(sq, target, kEmpty, owner != 0 ? kFlagCapture : 0);
            if (owner != 0) {
                break;
            }
            rr += dirs[i][0];
            cc += dirs[i][1];
        }
    }
}

void castleMoves(const Position &pos, MoveList &list)
{
    const int8_t side = pos.side;
    const int8_t enemy = -side;
    const bool white = side == kWhite;
    const int king = white ? kE1 : kE8;
    if (pos.kings[kingIndex(side)] != king || squareAttacked(pos, king, enemy)) {
        return;
    }
    const uint8_t kingSide = white ? kCastleWK : kCastleBK;
    const uint8_t queenSide = white ? kCastleWQ : kCastleBQ;
    const int8_t rook = (int8_t)(kRook * side);
    const int f = white ? kF1 : kF8, g = white ? kG1 : kG8, h = white ? kH1 : kH8;
    if ((pos.castling & kingSide) && pos.board[h] == rook && pos.board[f] == kEmpty && pos.board[g] == kEmpty &&
        !squareAttacked(pos, f, enemy) && !squareAttacked(pos, g, enemy)) {
        list.add(king, g, kEmpty, kFlagCastle);
    }
    const int d = white ? kD1 : kD8, cSq = white ? kC1 : kC8, b = white ? kB1 : kB8, a = white ? kA1 : kA8;
    if ((pos.castling & queenSide) && pos.board[a] == rook && pos.board[d] == kEmpty && pos.board[cSq] == kEmpty &&
        pos.board[b] == kEmpty && !squareAttacked(pos, d, enemy) && !squareAttacked(pos, cSq, enemy)) {
        list.add(king, cSq, kEmpty, kFlagCastle);
    }
}

int pseudoMoves(const Position &pos, Move *out)
{
    MoveList list = {out, 0};
    for (int sq = 0; sq < kSquares; ++sq) {
        const int8_t piece = pos.board[sq];
        if (colorOf(piece) != pos.side) {
            continue;
        }
        switch (typeOf(piece)) {
        case kPawn: pawnMoves(pos, sq, list); break;
        case kKnight: stepMoves(pos, sq, kKnightSteps, list); break;
        case kBishop: slideMoves(pos, sq, kBishopDirs, list); break;
        case kRook: slideMoves(pos, sq, kRookDirs, list); break;
        case kQueen:
            slideMoves(pos, sq, kBishopDirs, list);
            slideMoves(pos, sq, kRookDirs, list);
            break;
        case kKing: stepMoves(pos, sq, kKingSteps, list); break;
        default: break;
        }
    }
    castleMoves(pos, list);
    return list.count;
}

void clearCastlingAt(Position &pos, int sq)
{
    switch (sq) {
    case kA8: pos.castling &= ~kCastleBQ; break;
    case kH8: pos.castling &= ~kCastleBK; break;
    case kA1: pos.castling &= ~kCastleWQ; break;
    case kH1: pos.castling &= ~kCastleWK; break;
    default: break;
    }
}

// Most valuable victim, least valuable attacker: good captures are searched first.
int moveOrderScore(const Position &pos, const Move &m)
{
    int score = 0;
    if (m.flags & kFlagCapture) {
        const int victim = (m.flags & kFlagEnPassant) ? (int)kPawn : (int)typeOf(pos.board[m.to]);
        score += 10 * kPieceValue[victim] - typeOf(pos.board[m.from]);
    }
    if (m.promo != kEmpty) {
        score += kPieceValue[m.promo];
    }
    return score;
}

bool isNoisy(const Move &m)
{
    return (m.flags & kFlagCapture) || m.promo != kEmpty;
}

uint32_t nextRandom(uint32_t &state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

}  // namespace

void setStart(Position &pos)
{
    setFen(pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

bool setFen(Position &pos, const char *fen)
{
    std::memset(&pos, 0, sizeof(pos));
    pos.epSquare = -1;
    pos.fullmove = 1;
    int sq = 0;
    const char *p = fen;
    for (; *p != '\0' && *p != ' '; ++p) {
        const char ch = *p;
        if (ch == '/') {
            continue;
        }
        if (ch >= '1' && ch <= '8') {
            sq += ch - '0';
            continue;
        }
        static const char kLetters[] = "pnbrqk";
        const char lower = (char)(ch | 0x20);
        const char *found = std::strchr(kLetters, lower);
        if (found == nullptr || sq >= kSquares) {
            return false;
        }
        const int8_t type = (int8_t)(found - kLetters + 1);
        const int8_t side = ch == lower ? kBlack : kWhite;
        pos.board[sq] = (int8_t)(type * side);
        if (type == kKing) {
            pos.kings[kingIndex(side)] = (uint8_t)sq;
        }
        ++sq;
    }
    if (sq != kSquares || *p != ' ') {
        return false;
    }
    ++p;
    pos.side = *p == 'b' ? kBlack : kWhite;
    p += 2;
    for (; *p != '\0' && *p != ' '; ++p) {
        switch (*p) {
        case 'K': pos.castling |= kCastleWK; break;
        case 'Q': pos.castling |= kCastleWQ; break;
        case 'k': pos.castling |= kCastleBK; break;
        case 'q': pos.castling |= kCastleBQ; break;
        default: break;
        }
    }
    if (*p == ' ' && p[1] >= 'a' && p[1] <= 'h' && p[2] >= '1' && p[2] <= '8') {
        pos.epSquare = (int8_t)((8 - (p[2] - '0')) * 8 + (p[1] - 'a'));
        p += 3;
    } else if (*p == ' ') {
        p += 2;
    }
    if (*p == ' ') {
        pos.halfmove = (uint8_t)std::atoi(p + 1);
        const char *next = std::strchr(p + 1, ' ');
        if (next != nullptr) {
            pos.fullmove = (uint16_t)std::atoi(next + 1);
        }
    }
    return true;
}

bool squareAttacked(const Position &pos, int square, int8_t bySide)
{
    const int r = rowOf(square), c = colOf(square);
    // Pawns of `bySide` attack toward the enemy: a white pawn one row below attacks this square.
    const int pawnRow = r + (bySide == kWhite ? 1 : -1);
    for (int dc = -1; dc <= 1; dc += 2) {
        if (onBoard(pawnRow, c + dc) && pos.board[pawnRow * 8 + c + dc] == kPawn * bySide) {
            return true;
        }
    }
    for (const auto &s : kKnightSteps) {
        const int rr = r + s[0], cc = c + s[1];
        if (onBoard(rr, cc) && pos.board[rr * 8 + cc] == kKnight * bySide) {
            return true;
        }
    }
    for (const auto &s : kKingSteps) {
        const int rr = r + s[0], cc = c + s[1];
        if (onBoard(rr, cc) && pos.board[rr * 8 + cc] == kKing * bySide) {
            return true;
        }
    }
    for (int kind = 0; kind < 2; ++kind) {
        const int8_t (*dirs)[2] = kind == 0 ? kRookDirs : kBishopDirs;
        const int8_t slider = (int8_t)((kind == 0 ? kRook : kBishop) * bySide);
        const int8_t queen = (int8_t)(kQueen * bySide);
        for (int i = 0; i < 4; ++i) {
            int rr = r + dirs[i][0], cc = c + dirs[i][1];
            while (onBoard(rr, cc)) {
                const int8_t piece = pos.board[rr * 8 + cc];
                if (piece != kEmpty) {
                    if (piece == slider || piece == queen) {
                        return true;
                    }
                    break;
                }
                rr += dirs[i][0];
                cc += dirs[i][1];
            }
        }
    }
    return false;
}

bool inCheck(const Position &pos, int8_t side)
{
    return squareAttacked(pos, pos.kings[kingIndex(side)], (int8_t)-side);
}

void makeMove(Position &pos, const Move &m)
{
    const int8_t side = pos.side;
    const int8_t piece = pos.board[m.from];
    const bool capture = pos.board[m.to] != kEmpty || (m.flags & kFlagEnPassant);
    pos.board[m.to] = m.promo != kEmpty ? (int8_t)(m.promo * side) : piece;
    pos.board[m.from] = kEmpty;
    if (m.flags & kFlagEnPassant) {
        pos.board[m.to + (side == kWhite ? 8 : -8)] = kEmpty;
    }
    if (typeOf(piece) == kKing) {
        pos.kings[kingIndex(side)] = m.to;
        pos.castling &= side == kWhite ? ~(kCastleWK | kCastleWQ) : ~(kCastleBK | kCastleBQ);
        if (m.flags & kFlagCastle) {
            // The rook jumps over the king.
            const int rookFrom = m.to > m.from ? m.to + 1 : m.to - 2;
            const int rookTo = m.to > m.from ? m.to - 1 : m.to + 1;
            pos.board[rookTo] = pos.board[rookFrom];
            pos.board[rookFrom] = kEmpty;
        }
    }
    clearCastlingAt(pos, m.from);
    clearCastlingAt(pos, m.to);
    pos.epSquare = (m.flags & kFlagDoublePush) ? (int8_t)((m.from + m.to) / 2) : (int8_t)-1;
    pos.halfmove = (typeOf(piece) == kPawn || capture) ? 0 : (uint8_t)(pos.halfmove + 1);
    if (side == kBlack) {
        ++pos.fullmove;
    }
    pos.side = (int8_t)-side;
}

int legalMoves(const Position &pos, Move *out)
{
    const int count = pseudoMoves(pos, out);
    int kept = 0;
    for (int i = 0; i < count; ++i) {
        Position next = pos;
        makeMove(next, out[i]);
        if (!inCheck(next, pos.side)) {
            out[kept++] = out[i];
        }
    }
    return kept;
}

Result result(const Position &pos, Move *scratch)
{
    if (legalMoves(pos, scratch) == 0) {
        return inCheck(pos, pos.side) ? Result::Checkmate : Result::Stalemate;
    }
    if (pos.halfmove >= kFiftyMoveHalfmoves) {
        return Result::FiftyMoves;
    }
    // King against king, or king and one bishop/knight against king.
    int minors = 0;
    for (int sq = 0; sq < kSquares; ++sq) {
        const int8_t type = typeOf(pos.board[sq]);
        if (type == kPawn || type == kRook || type == kQueen) {
            return Result::Ongoing;
        }
        if (type == kKnight || type == kBishop) {
            ++minors;
        }
    }
    return minors <= 1 ? Result::Material : Result::Ongoing;
}

int evaluate(const Position &pos)
{
    int nonPawn[2] = {0, 0};
    for (int sq = 0; sq < kSquares; ++sq) {
        const int8_t type = typeOf(pos.board[sq]);
        if (type >= kKnight && type <= kQueen) {
            nonPawn[pos.board[sq] > 0 ? 0 : 1] += kPieceValue[type];
        }
    }
    const bool endgame = nonPawn[0] <= kEndgameMaterial && nonPawn[1] <= kEndgameMaterial;
    int score = 0;
    for (int sq = 0; sq < kSquares; ++sq) {
        const int8_t piece = pos.board[sq];
        if (piece == kEmpty) {
            continue;
        }
        const int t = piece > 0 ? sq : mirror(sq);
        int value = kPieceValue[typeOf(piece)];
        switch (typeOf(piece)) {
        case kPawn: value += kPawnTable[t]; break;
        case kKnight: value += kKnightTable[t]; break;
        case kBishop: value += kBishopTable[t]; break;
        case kRook: value += kRookTable[t]; break;
        case kQueen: value += kQueenTable[t]; break;
        case kKing: value += endgame ? kKingEndTable[t] : kKingMiddleTable[t]; break;
        default: break;
        }
        score += piece > 0 ? value : -value;
    }
    return pos.side == kWhite ? score : -score;
}

// Moves the best-ordered remaining move to `start` (selection sort, one step at a time).
void Searcher::pickNext(Move *moves, int count, int start, const Position &pos) const
{
    int best = start, bestScore = moveOrderScore(pos, moves[start]);
    for (int i = start + 1; i < count; ++i) {
        const int s = moveOrderScore(pos, moves[i]);
        if (s > bestScore) {
            best = i;
            bestScore = s;
        }
    }
    if (best != start) {
        const Move tmp = moves[start];
        moves[start] = moves[best];
        moves[best] = tmp;
    }
}

int Searcher::quiesce(const Position &pos, int alpha, int beta, int ply)
{
    if (++nodes_ >= nodeLimit_) {
        aborted_ = true;
        return 0;
    }
    Move *moves = moves_[ply];
    int count = -1;
    // A side in check may have no way out: that is mate, not a quiet position to stand on.
    if (inCheck(pos, pos.side)) {
        count = legalMoves(pos, moves);
        if (count == 0) {
            return -(kMate - ply);
        }
    }
    const int stand = evaluate(pos);
    if (stand >= beta) {
        return beta;
    }
    if (stand > alpha) {
        alpha = stand;
    }
    if (ply >= kMaxPly) {
        return alpha;
    }
    if (count < 0) {
        count = legalMoves(pos, moves);
    }
    int noisy = 0;
    for (int i = 0; i < count; ++i) {
        if (isNoisy(moves[i])) {
            moves[noisy++] = moves[i];
        }
    }
    for (int i = 0; i < noisy; ++i) {
        pickNext(moves, noisy, i, pos);
        Position next = pos;
        makeMove(next, moves[i]);
        const int score = -quiesce(next, -beta, -alpha, ply + 1);
        if (aborted_) {
            return 0;
        }
        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    return alpha;
}

int Searcher::negamax(const Position &pos, int depth, int alpha, int beta, int ply)
{
    if (depth <= 0 || ply >= kMaxPly) {
        return quiesce(pos, alpha, beta, ply);
    }
    if (++nodes_ >= nodeLimit_) {
        aborted_ = true;
        return 0;
    }
    Move *moves = moves_[ply];
    const int count = legalMoves(pos, moves);
    if (count == 0) {
        return inCheck(pos, pos.side) ? -(kMate - ply) : 0;
    }
    if (pos.halfmove >= kFiftyMoveHalfmoves) {
        return 0;
    }
    for (int i = 0; i < count; ++i) {
        pickNext(moves, count, i, pos);
        Position next = pos;
        makeMove(next, moves[i]);
        const int score = -negamax(next, depth - 1, -beta, -alpha, ply + 1);
        if (aborted_) {
            return 0;
        }
        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    return alpha;
}

bool Searcher::findBest(const Position &pos, int maxDepth, uint32_t nodeLimit, int noise, uint32_t seed, Move &best)
{
    Move *root = moves_[0];
    const int count = legalMoves(pos, root);
    if (count == 0) {
        return false;
    }
    nodes_ = 0;
    nodeLimit_ = nodeLimit;
    aborted_ = false;
    uint32_t random = seed | 1u;
    int *noiseOf = rootNoise_;
    for (int i = 0; i < count; ++i) {
        noiseOf[i] = noise > 0 ? (int)(nextRandom(random) % (uint32_t)(noise + 1)) : 0;
    }
    best = root[0];
    for (int depth = 1; depth <= maxDepth && !aborted_; ++depth) {
        int alpha = -kInfinity;
        int bestIndex = -1;
        for (int i = 0; i < count; ++i) {
            Position next = pos;
            makeMove(next, root[i]);
            int score = -negamax(next, depth - 1, -kInfinity, -alpha + noise, 1);
            if (aborted_) {
                break;
            }
            // Never let the noise hide a forced mate.
            if (score < kMateThreshold && score > -kMateThreshold) {
                score += noiseOf[i];
            }
            if (score > alpha) {
                alpha = score;
                bestIndex = i;
            }
        }
        if (aborted_ && depth > 1) {
            break;  // keep the move of the last finished depth
        }
        if (bestIndex >= 0) {
            best = root[bestIndex];
            // Search the best move first at the next depth (better cut-offs).
            const Move m = root[bestIndex];
            const int s = noiseOf[bestIndex];
            root[bestIndex] = root[0];
            noiseOf[bestIndex] = noiseOf[0];
            root[0] = m;
            noiseOf[0] = s;
        }
    }
    return true;
}

}  // namespace chess
