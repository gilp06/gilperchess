#pragma once




#include "types.h"
#include "move_gen.h"
#include "eval.h"
#include "board.h"


typedef struct s_sthreaddata sthreaddata_t;

typedef enum e_selectphase { TT_MOVE, GEN_LOUD, GOOD_LOUD_MOVES, GEN_QUIET, QUIET_MOVES, BAD_LOUD_MOVES, DONE } selectphase_t;
typedef enum e_selecttype {
    ALL_QUIET,
    NON_QUIET,
    BOTH_TYPES
} selecttype_t;

typedef struct s_moveselect {
    sthreaddata_t* td;
    move_t moves[256];
    int16_t move_scores[256];
    size_t loud_count;
    size_t quiet_count;
    size_t quiet_offset;
    move_t tt_move;
    move_t killer[2];
    selectphase_t phase;
    selecttype_t select_type;
} moveselect_t;



void update_history(sthreaddata_t* td, side_t stm, bindex_t from, bindex_t to, int16_t bonus);
void init_select(sthreaddata_t* td, moveselect_t *move_select, move_t tt_move, move_t killers[2], selecttype_t type);
move_t select_move(board_t *board, moveselect_t *move_select);

bool see(board_t *board, move_t move, int16_t threshold);
