#include "eval.h"
#include "board.h"



const int16_t piece_values[16] = {
    [PIECE_NONE]    = 0,

    // white pieces
    [PIECE_WPAWN]   = VAL_PAWN,
    [PIECE_WKNIGHT] = VAL_KNIGHT,
    [PIECE_WBISHOP] = VAL_BISHOP,
    [PIECE_WROOK]   = VAL_ROOK,
    [PIECE_WQUEEN]  = VAL_QUEEN,
    [PIECE_WKING]   = VAL_KING,

    // black pieces
    [PIECE_BPAWN]   = VAL_PAWN,
    [PIECE_BKNIGHT] = VAL_KNIGHT,
    [PIECE_BBISHOP] = VAL_BISHOP,
    [PIECE_BROOK]   = VAL_ROOK,
    [PIECE_BQUEEN]  = VAL_QUEEN,
    [PIECE_BKING]   = VAL_KING,
};

static int16_t pawn_table[64] = {
    0,  0,  0,  0,   0,   0,  0,  0,  50, 50, 50,  50, 50, 50,  50, 50,
    10, 10, 20, 30,  30,  20, 10, 10, 5,  5,  10,  25, 25, 10,  5,  5,
    0,  0,  0,  20,  20,  0,  0,  0,  5,  -5, -10, 0,  0,  -10, -5, 5,
    5,  10, 10, -20, -20, 10, 10, 5,  0,  0,  0,   0,  0,  0,   0,  0};

static int16_t knight_table[64] = {
    0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
    15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
    -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
    5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50,
};

static int16_t bishop_table[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
    0,   0,   -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,
    5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,
    -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,
    0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20,
};

static int16_t rook_table[64] = {
    0,  0, 0, 0, 0, 0, 0, 0,  5,  10, 10, 10, 10, 10, 10, 5,
    -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
    -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
    -5, 0, 0, 0, 0, 0, 0, -5, 0,  0,  0,  5,  5,  0,  0,  0};

static int16_t queen_table[64] = {
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10,
    -10, 0,   5,   5,  5,  5,   0,   -10, -5,  0,   5,   5,  5,  5,   0,   -5,
    0,   0,   5,   5,  5,  5,   0,   -5,  -10, 5,   5,   5,  5,  5,   0,   -10,
    -10, 0,   5,   0,  0,  0,   0,   -10, -20, -10, -10, -5, -5, -10, -10, -20};

static int16_t king_middle_table[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50,
    -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40,
    -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30,
    -20, -10, -20, -20, -20, -20, -20, -20, -10, 20,  20,  0,   0,
    0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

static int16_t king_end_table[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0,   0,
    -10, -20, -30, -30, -10, 20,  30,  30,  20,  -10, -30, -30, -10,
    30,  40,  40,  30,  -10, -30, -30, -10, 30,  40,  40,  30,  -10,
    -30, -30, -10, 20,  30,  30,  20,  -10, -30, -30, -30, 0,   0,
    0,   0,   -30, -30, -50, -30, -30, -30, -30, -30, -30, -50};

int16_t evaluate(board_t *board)
{
    int eval = 0;

    int queens =
        __builtin_popcountll(board->pieces_occ[PIECETYPE_QUEEN]);

    int is_endgame = (queens == 0);

    for (int sq = 0; sq < 64; sq++) {
        piece_t p = board->pieces_at[sq];
        if (p == PIECE_NONE)
            continue;

        int side = 1 - 2 * piece_side(p);
        int pt   = piece_type(p); // assuming (piece = type<<1 | side)

        int value = 0;
        int pst   = 0;

        // mirror square for black
        int psq = (side == 1) ? sq : (sq ^ 56);

        switch (pt) {
        case PIECETYPE_PAWN:
            value = VAL_PAWN;
            pst = pawn_table[psq];
            break;

        case PIECETYPE_KNIGHT:
            value = VAL_KNIGHT;
            pst = knight_table[psq];
            break;

        case PIECETYPE_BISHOP:
            value = VAL_BISHOP;
            pst = bishop_table[psq];
            break;

        case PIECETYPE_ROOK:
            value = VAL_ROOK;
            pst = rook_table[psq];
            break;

        case PIECETYPE_QUEEN:
            value = VAL_QUEEN;
            pst = queen_table[psq];
            break;

        case PIECETYPE_KING:
            value = VAL_KING;
            pst = is_endgame
                ? king_end_table[psq]
                : king_middle_table[psq];
            break;
        }

        eval += side * (value + pst);
    }

    // convert to side-to-move perspective (negamax)
    int stm = (board->side_to_move == SIDE_WHITE) ? 1 : -1;
    return (int16_t)(eval * stm);
}
