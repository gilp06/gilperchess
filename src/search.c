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

void perform_search(globalstate_t *gs, board_t *starting_board, int depth) {

    gs->best_move = 0;
    alphabeta(gs, starting_board, true, depth, INT16_MIN + 1, INT16_MAX, 0);
    move_t result = gs->best_move;
    char f[6];
    move_to_string(result, f);
    printf("bestmove %s\n", f);
    fflush(stdout);
}

int16_t alphabeta(globalstate_t *gs, board_t *board, bool root_node,
                  int16_t depth, int16_t alpha, int16_t beta, int16_t ply) {

    int16_t alpha_orig = alpha;
    int16_t tt_move = 0;

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
            return -1000;
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
        tt_move = entry.bestmove;
    }

    if (depth == 0)
        return evaluate(board);

    moveselect_t move_select;
    init_select(board, &move_select, tt_move);

    int16_t best_value = INT16_MIN;
    int16_t played = 0;
    move_t best_move = 0;
    int16_t value;

    while (true) {
        move_t cur_move = select_move(board, &move_select);
        if (cur_move == 0) break;

        dstate_t undo;
        if (!perform_move(board, cur_move, &undo)) {
            // illegal move; try next
            undo_move(board, &undo);
            continue;
        }
        played++;
        value = -alphabeta(gs, board, false, depth - 1, -beta, -alpha, ply + 1);
        if (value > best_value) {
            best_value = value;
            best_move = cur_move;

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
        entry.flag = best_value >= beta         ? LOWERBOUND
                     : best_value <= alpha_orig ? UPPERBOUND
                                                : EXACT;
        entry.bestmove = (entry.flag == UPPERBOUND) ? 0 : best_move;

        write_tt_entry(gs->transposition_table, entry);
    } else {
        gs->best_move = best_move;
    }

    return best_value;
}

static const uint8_t MVV_LVA[7][7] = {
    {15, 14, 13, 12, 11, 10, 0}, {25, 24, 23, 22, 21, 20, 0},
    {35, 34, 33, 32, 31, 30, 0}, {45, 44, 43, 42, 41, 40, 0},
    {55, 54, 53, 52, 51, 50, 0}, {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
};

void score_moves(board_t *board, moveselect_t *move_select) {
    for (int i = 0; i < move_select->count; i++) {
        move_select->move_scores[i] = 0;
        move_t move = move_select->moves[i];
        piece_t from = board->pieces_at[move_from(move)];
        piece_t to = board->pieces_at[move_to(move)];
        if (to == PIECE_NONE)
            continue;

        move_select->move_scores[i] = MVV_LVA[piece_type(to)][piece_type(from)];
    }
}

void init_select(board_t *board, moveselect_t *move_select, move_t tt_move) {
    move_select->phase = (tt_move != 0) ? TT_MOVE : GEN_MOVES;
    move_select->tt_move = tt_move;
}

// from ethereal, i like this
static size_t get_best(moveselect_t *move_select, size_t start, size_t end) {
    size_t best = start;
    for (int i = start + 1; i < end; i++) {
        if (move_select->move_scores[i] > move_select->move_scores[best]) {
            best = i;
        }
    }
    return best;
}

static move_t pop_move(size_t *length, move_t *moves, int16_t *scores,
                       size_t index) {
    // basically we want to swap the index with the last position, then
    // decrement length.
    move_t popped = moves[index];
    moves[index] = moves[--*length];
    scores[index] = scores[*length];
    return popped;
}

move_t select_move(board_t *board, moveselect_t *ms) {

    move_t *moves = ms->moves;
    int16_t *scores = ms->move_scores;

    move_t best_move = 0;

    switch (ms->phase) {
    case TT_MOVE:
        ms->phase = GEN_MOVES;
        return ms->tt_move;
        // next case
    case GEN_MOVES:
        generate_pseudolegal_moves(board, board->side_to_move, ms->moves,
                                   &ms->count);
        score_moves(board, ms);
        ms->phase = MOVES;
    case MOVES:
        while (ms->count > 0) {
            size_t best = get_best(ms, 0, ms->count);
            // fetch move
            best_move = pop_move(&ms->count, moves, scores, best);
            if (best_move == ms->tt_move)
                continue;
            return best_move;
        }
        ms->phase = DONE;
    case DONE:
        return 0;
    default:
        return 0;
    }
}
