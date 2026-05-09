#pragma once

#include <stdatomic.h>

#include "types.h"
#include "board.h"
#include "hashtable.h"

typedef struct s_searchsettings {
    uint64_t movetime;
    uint64_t wtime;
    uint64_t btime;
    uint64_t winc;
    uint64_t binc;
    uint64_t nodes;
    uint64_t depth;
} searchsettings_t;

extern const searchsettings_t infinite_search;

typedef struct s_globalstate {
    ttable_t transposition_table;
    move_t completed_best_move;
    move_t best_move;
    uint64_t nodes;

    int16_t alpha;
    int16_t beta;

    atomic_bool stop;
} globalstate_t;

typedef struct s_sthreaddata {
    globalstate_t *gs;
    board_t board;
    int16_t depth;


    move_t best_move;
    int16_t score;
    atomic_bool done;
} sthreaddata_t;


typedef enum e_selectphase
{
    TT_MOVE,
    GEN_MOVES,
    MOVES,
    DONE
} selectphase_t;


typedef struct s_moveselect {
    move_t moves[256];
    int16_t move_scores[256];
    size_t count;
    move_t tt_move;
    selectphase_t phase;
    bool nonquiet_only;
} moveselect_t;


void init_select(board_t* board, moveselect_t* move_select, move_t tt_move, bool nonquiet_only);
move_t select_move(board_t* board, moveselect_t* move_select);


void start_search(globalstate_t* gs, board_t* starting_board, searchsettings_t search_settings);
int16_t alphabeta(sthreaddata_t* td, bool root, int16_t depth, int16_t alpha, int16_t beta, int16_t ply);
int16_t qsearch(sthreaddata_t* td, int16_t alpha, int16_t beta, int16_t ply);
