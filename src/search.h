#pragma once

#include <setjmp.h>
#include <stdatomic.h>

#include "hashtable.h"
#include "move_select.h"
#include "board.h"
#include "types.h"


#define MAX_DEPTH 64

extern volatile bool ABORT_SIGNAL;

typedef struct s_searchparams {
    uint64_t movetime;
    uint64_t wtime;
    uint64_t btime;
    uint64_t winc;
    uint64_t binc;
    uint64_t nodes;
    uint64_t depth;
    uint64_t movestogo;
} searchparams_t;

extern const searchparams_t infinite_search;

typedef struct s_searchlimits {
    uint64_t time;
    uint64_t nodes;
    uint16_t depth;
} searchlimits_t;

typedef struct s_globalstate {
    ttable_t transposition_table;
    move_t completed_best_move;
    double start_time;
    searchlimits_t limits;
} globalstate_t;

typedef struct s_gosearchdata {
    searchparams_t search_settings;
    globalstate_t *gs;
    board_t *starting_board;
} gosearchdata_t;

typedef struct s_sthreaddata {
    globalstate_t *gs;
    board_t board;

    move_t pv_array[MAX_DEPTH][MAX_DEPTH];
    uint8_t pv_length[MAX_DEPTH];
    move_t pvs[MAX_DEPTH];
    
    int16_t depth_finished;

    move_t best_move;
    int16_t score;

    uint64_t nodes;
    jmp_buf jmp;
    bool worker;
} sthreaddata_t;

void *go_search(void *arg);
void *search(void *arg);
void search_bestmove(globalstate_t *gs, board_t const *starting_board);
int16_t alphabeta(sthreaddata_t *td, bool root, int16_t depth, int16_t alpha,
                  int16_t beta, int16_t ply);
int16_t qsearch(sthreaddata_t *td, int16_t alpha, int16_t beta, int16_t ply);
