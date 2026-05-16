#pragma once


#define MIN(a,b) a < b ? a : b;
#define MAX(a,b) a > b ? a : b;

#include <stdint.h>
#include <sys/time.h>

#include "types.h"

extern const bindex_t PUSH_DIR[2];
extern const bb_t DOUBLE_PUSH_MASK[2][2];
extern bindex_t ROOK_CASTLING_POS[2][2][2];
extern bindex_t KING_CASTLING_POS[2][2][2];
extern bb_t CASTLING_OCC_MASK[2][2];
extern bb_t CASTLING_ATTACK_MASK[2][2];

extern const bb_t RANK_MASKS[8];
extern const bb_t FILE_MASKS[8];

extern const brank_t SQUARE_TO_RANK[64];
extern const bfile_t SQUARE_TO_FILE[64];

extern const bb_t KNIGHT_MOVES[64];
extern const bb_t KING_MOVES[64];
extern const bb_t PAWN_ATTACKS[2][64];

extern bb_t ROOK_SLIDER_MASK[64];
extern bb_t BISHOP_SLIDER_MASK[64];

extern bb_t ROOK_PEXT_MASK[64];
extern bb_t BISHOP_PEXT_MASK[64];
extern bb_t ROOK_PEXT_INDEX[64];
extern bb_t BISHOP_PEXT_INDEX[64];

extern bb_t PEXT_TABLE[107648];

extern const uint8_t CASTLING_RIGHTS_WKINGSIDE;
extern const uint8_t CASTLING_RIGHTS_WQUEENSIDE;
extern const uint8_t CASTLING_RIGHTS_WBOTH;
extern const uint8_t CASTLING_RIGHTS_BKINGSIDE;
extern const uint8_t CASTLING_RIGHTS_BQUEENSIDE;
extern const uint8_t CASTLING_RIGHTS_BBOTH;

extern uint8_t CASTLE_MASK[2][64];
extern uint8_t CASTLE_CAPTURE_MASK[2][64];


// indexed from -> to
extern bb_t BETWEEN_MASK[64][64];


bb_t fetch_bishop_moves(bindex_t square, bb_t blockers);
bb_t fetch_rook_moves(bindex_t square, bb_t blockers);
bb_t generate_sliding_mask(bindex_t sq, const int8_t directions[4][2],
                           bb_t blockers);
void init_pext_table();
void init_between_table();
bb_t get_edge_filter(bindex_t sq);

void print_bb(bb_t bboard);
void print_pieces(piece_t *board);

static inline double get_real_time() {
    struct timeval tv;
    double secsInMilli, usecsInMilli;
    gettimeofday(&tv, NULL);
    secsInMilli = ((double)tv.tv_sec) * 1000.0;
    usecsInMilli = tv.tv_usec / 1000.0;
    return secsInMilli + usecsInMilli;
}

static inline void move_to_string(move_t m, char f[6]) {
    bindex_t from = move_from(m);
    bindex_t to = move_to(m);
    f[0] = 'a' + (from % 8);
    f[1] = '1' + (from / 8);
    f[2] = 'a' + (to % 8);
    f[3] = '1' + (to / 8);
    f[4] = '\0';

    if(move_type(m) == PROMOTION)
    {
        piecetype_t pt = move_promo(m);
        switch(pt)
        {
            case PIECETYPE_ROOK: {
                f[4] = 'r';
                break;
            }
            case PIECETYPE_QUEEN: {
                f[4] = 'q';
                break;
            }
            case PIECETYPE_BISHOP: {
                f[4] = 'b';
                break;
            }
            case PIECETYPE_KNIGHT: {
                f[4] = 'n';
                break;
            }
            default: {
                f[4] = '\0';
            }
        }
        f[5] = '\0';
    }
}
