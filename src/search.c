#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "hashtable.h"
#include "move_gen.h"
#include "search.h"
#include "types.h"
#include "utils.h"

// move_t alphabeta_root(global_state_t* gs, board_t *board, int16_t depth);

void perform_search(global_state_t *gs, board_t *starting_board, int depth) {

    gs->best_move = 0;
    alphabeta(gs, starting_board, true, depth, INT16_MIN+1, INT16_MAX, 0);
    move_t result = gs->best_move;
    char f[6];
    move_to_string(result, f);
    printf("bestmove %s\n", f);
    fflush(stdout);
}

int16_t alphabeta(global_state_t *gs, board_t *board, bool root_node,
                  int16_t depth, int16_t alpha, int16_t beta, int16_t ply) {

    int16_t alpha_orig = alpha;

    if (!root_node) {
        if (board->st.halfmove_clock == 100)
            return 0;

        int repetitions = 0;
        for (int i = board->move_number - 2; i >= 0; i--) {
            if (board->st.key == board->key_hist[i]) {
                repetitions++;
            }
        }

        if (repetitions >= 2) {
            return 0;
        }
    }

    // check transposition table
    tt_entry_t entry = get_tt_entry(gs->transposition_table, board->st.key);
    if (entry.flag != INVALID && entry.hash == board->st.key) {
        
        if (entry.depth >= depth) {
            if (entry.flag == EXACT ||
                (entry.flag == LOWERBOUND && entry.value >= beta) ||
                (entry.flag == UPPERBOUND && entry.value <= alpha)) {
                return entry.value;
            }
        }
    }
    
    if (depth == 0)
        return evaluate(board);

    // generate move list
    move_t moves[256];
    size_t move_count;
    generate_pseudolegal_moves(board, board->side_to_move, moves, &move_count);

    int16_t best_value = INT16_MIN;
    int16_t played = 0;
    move_t best_move = 0;
    int16_t value;
    // TODO: REPLACE WITH MOVE PICKER
    for (int i = 0; i < move_count; i++) {
        dstate_t undo;
        if (!perform_move(board, moves[i], &undo)) {
            // illegal move; try next
            undo_move(board, &undo);
            continue;
        }
        played++;
        value = -alphabeta(gs, board, false, depth - 1, -beta, -alpha, ply+1);            
        if (value > best_value) {
            best_value = value;
            best_move = moves[i];

            if (value > alpha) {
                alpha = value;

                if (alpha >= beta) {
                    undo_move(board, &undo);
                    break;
                }
            }
        }
        undo_move(board, &undo);
    }

    if (played == 0) {
        if (in_check(board, board->side_to_move))
            return INT16_MIN + ply;
        else
            return 0;
    }

    if (!root_node) {
        tt_entry_t entry;
        entry.depth = depth;
        entry.hash = board->st.key;
        entry.value = best_value;
        entry.flag = best_value >= beta        ? LOWERBOUND
                     : best_value <= alpha_orig ? UPPERBOUND
                                               : EXACT;
        entry.bestmove = (entry.flag == UPPERBOUND) ? 0 : best_move;

        write_tt_entry(gs->transposition_table, entry);
    } else {
        gs->best_move = best_move;
    }

    return best_value;
}
