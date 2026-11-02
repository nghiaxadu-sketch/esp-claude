// Host test for the chess rules and player (no board needed):
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -I.. ../chess_engine.cpp chess_engine_test.cpp -o chess_engine_test
//       && ./chess_engine_test
// Perft counts every legal move sequence to a fixed depth; the reference numbers come from the chess
// programming wiki and catch nearly every rules bug (castling, en passant, promotion, pins, checks).
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

#include "chess_engine.h"

using namespace chess;

static int failures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

static uint64_t perft(const Position &pos, int depth)
{
    std::vector<Move> moves(kMaxMoves);
    const int count = legalMoves(pos, moves.data());
    if (depth == 1) {
        return (uint64_t)count;
    }
    uint64_t total = 0;
    for (int i = 0; i < count; ++i) {
        Position next = pos;
        makeMove(next, moves[i]);
        total += perft(next, depth - 1);
    }
    return total;
}

static void testPerft()
{
    struct Case {
        const char *fen;
        int depth;
        uint64_t nodes;
    };
    const Case cases[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890},
    };
    for (const Case &c : cases) {
        Position pos;
        EXPECT(setFen(pos, c.fen));
        const uint64_t got = perft(pos, c.depth);
        if (got != c.nodes) {
            std::printf("FAIL perft(%d) %s: got %llu want %llu\n", c.depth, c.fen, (unsigned long long)got,
                        (unsigned long long)c.nodes);
            ++failures;
        }
    }
}

static void testResults()
{
    std::vector<Move> scratch(kMaxMoves);
    Position pos;
    setStart(pos);
    EXPECT(result(pos, scratch.data()) == Result::Ongoing);
    // Fool's mate.
    EXPECT(setFen(pos, "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"));
    EXPECT(result(pos, scratch.data()) == Result::Checkmate);
    EXPECT(setFen(pos, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));
    EXPECT(result(pos, scratch.data()) == Result::Stalemate);
    EXPECT(setFen(pos, "8/8/4k3/8/8/3NK3/8/8 w - - 0 1"));
    EXPECT(result(pos, scratch.data()) == Result::Material);
    EXPECT(setFen(pos, "8/8/4k3/8/8/3RK3/8/8 w - - 100 80"));
    EXPECT(result(pos, scratch.data()) == Result::FiftyMoves);
}

static void testSearch()
{
    auto searcher = std::make_unique<Searcher>();
    Position pos;
    Move best;
    // Mate in one: Qh5xf7#.
    EXPECT(setFen(pos, "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4"));
    EXPECT(searcher->findBest(pos, 2, 200000, 0, 1, best));
    EXPECT(best.from == 31 && best.to == 13);  // h5 -> f7
    // Takes a free queen.
    EXPECT(setFen(pos, "4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1"));
    EXPECT(searcher->findBest(pos, 2, 200000, 0, 1, best));
    EXPECT(best.from == 51 && best.to == 27);  // d2 -> d5
    // Even with noise it never misses a mate in one.
    EXPECT(setFen(pos, "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1"));
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        EXPECT(searcher->findBest(pos, 1, 200000, 80, seed, best));
        EXPECT(best.from == 56 && best.to == 0);  // Ra8#
    }

    // Speed on this machine for the levels the board uses (node limits keep the board responsive).
    setStart(pos);
    Move m;
    for (int depth = 1; depth <= 3; ++depth) {
        const auto t0 = std::chrono::steady_clock::now();
        searcher->findBest(pos, depth, 60000, 0, 7, m);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0);
        std::printf("  depth %d: %u nodes, %lld ms on this PC\n", depth, searcher->nodes(), (long long)ms.count());
    }
    EXPECT(setFen(pos, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    const auto t0 = std::chrono::steady_clock::now();
    searcher->findBest(pos, 3, 60000, 0, 7, m);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0);
    std::printf("  busy middlegame depth 3: %u nodes, %lld ms on this PC\n", searcher->nodes(), (long long)ms.count());
}

int main()
{
    testPerft();
    testResults();
    testSearch();
    if (failures == 0) {
        std::printf("chess_engine: all tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
