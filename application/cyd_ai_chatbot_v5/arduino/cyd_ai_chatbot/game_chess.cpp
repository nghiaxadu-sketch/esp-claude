#include "games.h"

namespace {

using namespace ui;
using chess::Move;

constexpr int16_t kSquare = 26;
constexpr int16_t kBoardX = 4;
constexpr int16_t kBoardY = kHeaderH + 5;
constexpr int16_t kBoardSize = 8 * kSquare;
constexpr int16_t kArtSize = 20;
constexpr int16_t kArtInset = (kSquare - kArtSize) / 2;
constexpr int16_t kPanelX = kBoardX + kBoardSize + 6;
constexpr int16_t kPanelW = kScreenW - kPanelX - 4;
constexpr int16_t kTurnY = kHeaderH + 6;
constexpr int16_t kStateY = kTurnY + 22;
constexpr int16_t kScoreY = kStateY + 14;
constexpr int16_t kButtonH = 34;
constexpr Rect kLevelButton = {kPanelX, 96, kPanelW, kButtonH};
constexpr Rect kUndoButton = {kPanelX, 136, kPanelW, kButtonH};
constexpr Rect kNewButton = {kPanelX, 176, kPanelW, kButtonH};
constexpr int16_t kPromoTop = 92;
constexpr int16_t kPromoCell = kPanelW / 2;
constexpr int16_t kDotR = 4;
constexpr int16_t kRingR = 11;
constexpr uint32_t kCpuDelayMs = 60;  // lets the "thinking" text appear before the search blocks
constexpr size_t kMaxHistory = 300;

constexpr uint16_t kLight = rgb565(0xF0, 0xD9, 0xB5);
constexpr uint16_t kDark = rgb565(0xB5, 0x88, 0x63);
constexpr uint16_t kLastLight = rgb565(0xCD, 0xD2, 0x6A);
constexpr uint16_t kLastDark = rgb565(0xAA, 0xA2, 0x3A);
constexpr uint16_t kSelectColor = rgb565(0xF6, 0xF6, 0x69);
constexpr uint16_t kCheckColor = rgb565(0xE5, 0x39, 0x35);
constexpr uint16_t kMarkColor = rgb565(0x2E, 0x6B, 0x30);
constexpr uint16_t kWhiteBody = kColorWhite;
constexpr uint16_t kWhiteEdge = kColorBlack;
constexpr uint16_t kBlackBody = rgb565(0x28, 0x28, 0x28);
constexpr uint16_t kBlackEdge = rgb565(0xC8, 0xC8, 0xC8);

struct Level {
    const char *label;
    int depth;
    uint32_t nodes;  // bounds the thinking time on the ESP32
    int noise;       // centipawns of randomness: lower levels make more mistakes
};
const Level kLevels[] = {{"Muc: De", 1, 20000, 120}, {"Muc: Vua", 2, 30000, 30}, {"Muc: Kho", 3, 40000, 8}};
constexpr uint8_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

const int8_t kPromotions[4] = {chess::kQueen, chess::kRook, chess::kBishop, chess::kKnight};

// 20x20 piece art: 'X' outline, 'O' body, '.' transparent. Index = piece type - 1.
const char *const kArt[6][kArtSize] = {
    {"....................", "....................", "....................", "....................",
     "........XXXX........", ".......XOOOOX.......", ".......XOOOOX.......", ".......XOOOOX.......",
     "........XOOX........", "......XXOOOOXX......", "........XOOX........", "........XOOX........",
     ".......XOOOOX.......", "......XOOOOOOX......", ".....XOOOOOOOOX.....", "....XOOOOOOOOOOX....",
     "....XOOOOOOOOOOX....", "....XXXXXXXXXXXX....", "....................", "...................."},
    {"....................", "........XX.XX.......", ".......XOOXOOX......", "......XOOOOOOOX.....",
     ".....XOOXOOOOOOX....", "....XOOOOOOOOOOOX...", "...XOOOOOOOOOOOOX...", "..XOOOOOOOOOOOOOX...",
     "..XOOOXXOOOOOOOOX...", "...XXX..XOOOOOOOX...", "........XOOOOOOOX...", ".......XOOOOOOOOX...",
     "......XOOOOOOOOOX...", ".....XOOOOOOOOOOX...", "....XXXXXXXXXXXXXX..", "....XOOOOOOOOOOOOX..",
     "....XOOOOOOOOOOOOX..", "....XXXXXXXXXXXXXX..", "....................", "...................."},
    {"....................", ".........XX.........", "........XOOX........", ".......XOOOOX.......",
     "......XOOOXOOX......", "......XOOXOOOX......", "......XOXOOOOX......", "......XOOOOOOX......",
     ".......XOOOOX.......", "......XXXXXXXX......", "........XOOX........", "........XOOX........",
     ".......XOOOOX.......", "......XOOOOOOX......", ".....XOOOOOOOOX.....", "....XOOOOOOOOOOX....",
     "....XOOOOOOOOOOX....", "....XXXXXXXXXXXX....", "....................", "...................."},
    {"....................", "....................", "...XXXX.XXXX.XXXX...", "...XOOX.XOOX.XOOX...",
     "...XOOXXXOOXXXOOX...", "...XOOOOOOOOOOOOX...", "....XXXXXXXXXXXX....", ".....XOOOOOOOOX.....",
     ".....XOOOOOOOOX.....", ".....XOOOOOOOOX.....", ".....XOOOOOOOOX.....", ".....XOOOOOOOOX.....",
     ".....XOOOOOOOOX.....", "....XXXXXXXXXXXX....", "...XOOOOOOOOOOOOX...", "..XOOOOOOOOOOOOOOX..",
     "..XOOOOOOOOOOOOOOX..", "..XXXXXXXXXXXXXXXX..", "....................", "...................."},
    {"....................", "..XX....XXXX....XX..", "..XOX..XOOOOX..XOX..", "..XOOX.XOOOOX.XOOX..",
     "..XOOOXOOOOOOXOOOX..", "..XOOOOOOOOOOOOOOX..", "...XOOOOOOOOOOOOX...", "....XOOOOOOOOOOX....",
     "....XXXXXXXXXXXX....", ".....XOOOOOOOOX.....", "......XOOOOOOX......", "......XOOOOOOX......",
     ".....XOOOOOOOOX.....", "....XXXXXXXXXXXX....", "...XOOOOOOOOOOOOX...", "..XOOOOOOOOOOOOOOX..",
     "..XOOOOOOOOOOOOOOX..", "..XXXXXXXXXXXXXXXX..", "....................", "...................."},
    {".........XX.........", "........XOOX........", "......XXXOOXXX......", "......XOOOOOOX......",
     "......XXXOOXXX......", "........XOOX........", "...XXXXXXXXXXXXXX...", "..XOOOOOOOOOOOOOOX..",
     "..XOOOOOOOOOOOOOOX..", "...XOOOOOOOOOOOOX...", "....XOOOOOOOOOOX....", ".....XXXXXXXXXX.....",
     ".....XOOOOOOOOX.....", "....XXXXXXXXXXXX....", "...XOOOOOOOOOOOOX...", "..XOOOOOOOOOOOOOOX..",
     "..XOOOOOOOOOOOOOOX..", "..XXXXXXXXXXXXXXXX..", "....................", "...................."},
};

int8_t typeOf(int8_t piece)
{
    return piece >= 0 ? piece : (int8_t)-piece;
}

Rect promotionCell(int i)
{
    return {int16_t(kPanelX + (i % 2) * kPromoCell), int16_t(kPromoTop + 16 + (i / 2) * (kPromoCell + 4)), kPromoCell,
            kPromoCell};
}

}  // namespace

void ChessGame::start()
{
    whiteWins_ = blackWins_ = draws_ = 0;
    newGame();
}

void ChessGame::newGame()
{
    chess::setStart(pos_);
    history_.clear();
    selected_ = lastFrom_ = lastTo_ = -1;
    over_ = cpuToMove_ = choosingPromotion_ = false;
    resultText_ = String();
    legalCount_ = chess::legalMoves(pos_, legal_);
    drawAll();
}

bool ChessGame::humanToMove() const
{
    return twoPlayers_ || pos_.side == chess::kWhite;
}

// ---------------------------------------------------------------- drawing

void ChessGame::drawStatus()
{
    drawHud(twoPlayers_ ? "Trang " + String(whiteWins_) + "-" + String(blackWins_) + " Den"
                        : "Ban " + String(whiteWins_) + "-" + String(blackWins_) + " May");
}

void ChessGame::drawAll()
{
    gfx_.fillScreen(kColorBlack);
    drawStatus();
    drawBoard();
    drawPanel();
}

void ChessGame::drawPiece(int16_t x, int16_t y, int8_t piece)
{
    const bool white = piece > 0;
    const uint16_t edge = white ? kWhiteEdge : kBlackEdge;
    const uint16_t body = white ? kWhiteBody : kBlackBody;
    const char *const *art = kArt[typeOf(piece) - 1];
    for (int16_t row = 0; row < kArtSize; ++row) {
        const char *line = art[row];
        int16_t col = 0;
        while (col < kArtSize) {
            const char ch = line[col];
            int16_t end = col + 1;
            while (end < kArtSize && line[end] == ch) {
                ++end;
            }
            if (ch != '.') {
                gfx_.drawFastHLine(x + col, y + row, end - col, ch == 'X' ? edge : body);
            }
            col = end;
        }
    }
}

bool ChessGame::isTarget(int sq) const
{
    if (selected_ < 0) {
        return false;
    }
    for (int i = 0; i < legalCount_; ++i) {
        if (legal_[i].from == selected_ && legal_[i].to == sq) {
            return true;
        }
    }
    return false;
}

void ChessGame::drawSquare(int sq)
{
    const int row = sq / 8, col = sq % 8;
    const int16_t x = kBoardX + col * kSquare, y = kBoardY + row * kSquare;
    const bool light = (row + col) % 2 == 0;
    const int8_t piece = pos_.board[sq];
    uint16_t color = light ? kLight : kDark;
    if (sq == lastFrom_ || sq == lastTo_) {
        color = light ? kLastLight : kLastDark;
    }
    if (sq == selected_) {
        color = kSelectColor;
    }
    if (typeOf(piece) == chess::kKing && (piece > 0 ? chess::kWhite : chess::kBlack) == pos_.side &&
        chess::inCheck(pos_, pos_.side)) {
        color = kCheckColor;
    }
    gfx_.fillRect(x, y, kSquare, kSquare, color);
    if (piece != chess::kEmpty) {
        drawPiece(x + kArtInset, y + kArtInset, piece);
    }
    if (isTarget(sq)) {
        if (piece != chess::kEmpty) {
            gfx_.drawCircle(x + kSquare / 2, y + kSquare / 2, kRingR, kMarkColor);
            gfx_.drawCircle(x + kSquare / 2, y + kSquare / 2, kRingR + 1, kMarkColor);
        } else {
            gfx_.fillCircle(x + kSquare / 2, y + kSquare / 2, kDotR, kMarkColor);
        }
    }
}

void ChessGame::drawBoard()
{
    for (int sq = 0; sq < chess::kSquares; ++sq) {
        drawSquare(sq);
    }
}

void ChessGame::drawPanel()
{
    gfx_.fillRect(kPanelX, kHeaderH + 1, kPanelW, kScreenH - kHeaderH - 1, kColorBlack);
    const bool whiteTurn = pos_.side == chess::kWhite;
    String turn = whiteTurn ? "Trang di" : "Den di";
    if (cpuToMove_) {
        turn = "May nghi";
    }
    gfx_.setTextWrap(false);
    gfx_.setTextSize(2);
    gfx_.setTextColor(whiteTurn ? kColorWhite : kColorMuted);
    gfx_.setCursor(kPanelX, kTurnY);
    gfx_.print(over_ ? String("Het van") : turn);
    gfx_.setTextSize(1);
    String state = resultText_;
    uint16_t stateColor = kColorSystem;
    if (!over_ && chess::inCheck(pos_, pos_.side)) {
        state = "Chieu!";
        stateColor = kCheckColor;
    }
    gfx_.setTextColor(stateColor);
    gfx_.setCursor(kPanelX, kStateY);
    gfx_.print(ellipsize(state, kPanelW / kFontW));
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kPanelX, kScoreY);
    gfx_.print("Hoa " + String(draws_));
    if (choosingPromotion_) {
        drawPromotionChoice();
        return;
    }
    if (twoPlayers_) {
        drawCentered(gfx_, "2 nguoi", kLevelButton.x, kLevelButton.w, kLevelButton.y + 12, 1, kColorMuted);
    } else {
        drawButton(gfx_, kLevelButton, kLevels[level_].label, kColorButton, kColorWhite);
    }
    drawButton(gfx_, kUndoButton, "Lui", kColorButton, kColorWhite);
    drawButton(gfx_, kNewButton, "Van moi", kColorAccent, kColorWhite);
}

void ChessGame::drawPromotionChoice()
{
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorWhite);
    gfx_.setCursor(kPanelX, kPromoTop);
    gfx_.print("Phong cap:");
    for (int i = 0; i < 4; ++i) {
        const Rect r = promotionCell(i);
        gfx_.fillRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 4, (i % 2) == (i / 2) ? kLight : kDark);
        drawPiece(r.x + (r.w - kArtSize) / 2, r.y + (r.h - kArtSize) / 2, (int8_t)(kPromotions[i] * pos_.side));
    }
}

// ---------------------------------------------------------------- play

void ChessGame::play(const Move &move)
{
    if (history_.size() >= kMaxHistory) {
        history_.erase(history_.begin());
    }
    history_.push_back(pos_);
    const bool capture = pos_.board[move.to] != chess::kEmpty || (move.flags & chess::kFlagEnPassant);
    chess::makeMove(pos_, move);
    lastFrom_ = move.from;
    lastTo_ = move.to;
    selected_ = -1;
    sound(capture ? 700 : 1100, 30);
    afterMove();
}

void ChessGame::afterMove()
{
    const chess::Result r = chess::result(pos_, legal_);
    legalCount_ = chess::legalMoves(pos_, legal_);
    if (r == chess::Result::Ongoing) {
        cpuToMove_ = !twoPlayers_ && pos_.side == chess::kBlack;
        cpuDueMs_ = millis() + kCpuDelayMs;
        drawBoard();
        drawPanel();
        return;
    }
    over_ = true;
    cpuToMove_ = false;
    String line1;
    uint16_t color = kColorText;
    if (r == chess::Result::Checkmate) {
        const bool whiteWon = pos_.side == chess::kBlack;  // the side to move is mated
        if (whiteWon) {
            ++whiteWins_;
        } else {
            ++blackWins_;
        }
        if (twoPlayers_) {
            line1 = whiteWon ? "Trang thang!" : "Den thang!";
        } else {
            line1 = whiteWon ? "Ban thang!" : "May thang!";
        }
        color = whiteWon || twoPlayers_ ? kColorOk : kColorAccent;
        resultText_ = "Chieu het";
        sound(whiteWon ? 1800 : 300, 400);
    } else {
        ++draws_;
        line1 = "Hoa!";
        resultText_ = r == chess::Result::Stalemate ? "Het nuoc di" : (r == chess::Result::FiftyMoves ? "50 nuoc" : "Thieu quan");
        sound(600, 200);
    }
    drawStatus();
    drawBoard();
    drawPanel();
    drawBanner(line1, resultText_ + " - cham ban co de choi lai", color);
}

void ChessGame::undo()
{
    if (history_.empty()) {
        return;
    }
    pos_ = history_.back();
    history_.pop_back();
    // Against the board, go back to the player's own turn.
    if (!twoPlayers_ && pos_.side == chess::kBlack && !history_.empty()) {
        pos_ = history_.back();
        history_.pop_back();
    }
    over_ = cpuToMove_ = choosingPromotion_ = false;
    resultText_ = String();
    selected_ = lastFrom_ = lastTo_ = -1;
    legalCount_ = chess::legalMoves(pos_, legal_);
    if (!twoPlayers_ && pos_.side == chess::kBlack) {
        cpuToMove_ = true;  // only the board's first move was left
        cpuDueMs_ = millis() + kCpuDelayMs;
    }
    drawAll();
}

void ChessGame::update(uint32_t nowMs)
{
    if (!cpuToMove_ || over_ || (int32_t)(nowMs - cpuDueMs_) < 0) {
        return;
    }
    const Level &level = kLevels[level_];
    Move best;
    const bool found = searcher_.findBest(pos_, level.depth, level.nodes, level.noise, esp_random(), best);
    cpuToMove_ = false;
    if (found) {
        play(best);
    }
}

void ChessGame::pressBoard(int sq)
{
    const int8_t piece = pos_.board[sq];
    const bool own = piece != chess::kEmpty && (piece > 0 ? chess::kWhite : chess::kBlack) == pos_.side;
    if (selected_ >= 0 && isTarget(sq)) {
        Move chosen;
        bool promotes = false;
        for (int i = 0; i < legalCount_; ++i) {
            if (legal_[i].from == selected_ && legal_[i].to == sq) {
                chosen = legal_[i];
                promotes = legal_[i].promo != chess::kEmpty;
                break;
            }
        }
        if (promotes) {
            choosingPromotion_ = true;
            promotion_ = chosen;
            drawPanel();
            return;
        }
        play(chosen);
        return;
    }
    selected_ = own && sq != selected_ ? sq : -1;
    drawBoard();
}

void ChessGame::press(int16_t x, int16_t y)
{
    const bool onBoard = x >= kBoardX && x < kBoardX + kBoardSize && y >= kBoardY && y < kBoardY + kBoardSize;
    if (choosingPromotion_) {
        for (int i = 0; i < 4; ++i) {
            if (promotionCell(i).contains(x, y)) {
                choosingPromotion_ = false;
                Move m = promotion_;
                m.promo = kPromotions[i];
                play(m);
                return;
            }
        }
        if (onBoard) {  // changed their mind
            choosingPromotion_ = false;
            selected_ = -1;
            drawBoard();
            drawPanel();
        }
        return;
    }
    if (!twoPlayers_ && kLevelButton.contains(x, y)) {
        level_ = (level_ + 1) % kLevelCount;
        drawButton(gfx_, kLevelButton, kLevels[level_].label, kColorButton, kColorWhite);
        return;
    }
    if (kUndoButton.contains(x, y)) {
        undo();
        return;
    }
    if (kNewButton.contains(x, y)) {
        newGame();
        return;
    }
    if (over_) {
        if (onBoard) {
            newGame();
        }
        return;
    }
    if (onBoard && humanToMove() && !cpuToMove_) {
        pressBoard((y - kBoardY) / kSquare * 8 + (x - kBoardX) / kSquare);
    }
}
