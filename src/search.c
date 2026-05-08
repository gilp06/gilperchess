#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "hashtable.h"
#include "move_gen.h"
#include "pthread.h"
#include "search.h"
#include "types.h"
#include "utils.h"

// move_t alphabeta_root(global_state_t* gs, board_t *board, int16_t depth);

static void *search_thread(void *arg);

sthreaddata_t td[1];

void start_search(globalstate_t *gs, board_t *starting_board, int depth) {

    gs->best_move = 0;
    gs->nodes = 0;

    pthread_t thread;

    for (int i = 1; i <= depth; i++) {
        memcpy(&td[0].board, starting_board, sizeof(board_t));
        td[0].gs = gs;
        td[0].depth = i;

        pthread_create(&thread, NULL, search_thread, (void *)&td);

        // wait for thread to finish threading
        pthread_join(thread, NULL);

        // retreive the score and the current best move
        int16_t score = td[0].score;
        move_t move = td[0].best_move;

        // update current available best move and score
        gs->best_move = move;
        printf("info nodes %llu depth %d score cp %d\n", gs->nodes, i, score);
        fflush(stdout);
    }

    // when done output bestmove
    char m[6];
    move_to_string(gs->best_move, m);
    printf("bestmove %s\n", m);
    fflush(stdout);
}

static void *search_thread(void *arg) {
    sthreaddata_t *td = (sthreaddata_t *)arg;

    td->score = alphabeta(td, true, td->depth, INT16_MIN + 1, INT16_MAX, 0);

    return NULL;
}

int16_t alphabeta(sthreaddata_t *td, bool root_node, int16_t depth,
                  int16_t alpha, int16_t beta, int16_t ply) {

    globalstate_t *gs = td->gs;
    board_t *board = &td->board;

    gs->nodes++;

    int16_t alpha_orig = alpha;
    int16_t tt_move = 0;

    if (is_draw(board)) {
        return 0 - ply * 100;
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
        // return evaluate(board);
        return qsearch(td, alpha, beta, ply + 1);

    moveselect_t move_select;
    init_select(board, &move_select, tt_move, false);

    int16_t best_value = INT16_MIN;
    int16_t played = 0;
    move_t best_move = 0;
    int16_t value;

    while (true) {
        move_t cur_move = select_move(board, &move_select);
        if (cur_move == 0)
            break;

        dstate_t undo;
        if (!perform_move(board, cur_move, &undo)) {
            // illegal move; try next
            undo_move(board, &undo);
            continue;
        }
        played++;
        value = -alphabeta(td, false, depth - 1, -beta, -alpha, ply + 1);
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
            return INT16_MIN + ply + 1;
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
        td->best_move = best_move;
    }

    return best_value;
}

int16_t qsearch(sthreaddata_t *td, int16_t alpha, int16_t beta, int16_t ply) {

    globalstate_t *gs = td->gs;
    board_t *board = &td->board;

    gs->nodes++;

    if (is_draw(board))
        return 0 - ply * 100;

    int16_t best_val = INT16_MIN;

    bool incheck = in_check(board, board->side_to_move);

    if (!incheck) {
        int16_t stand_eval = evaluate(board);
        best_val = stand_eval;
        if (best_val >= beta)
            return best_val;
        if (best_val > alpha)
            alpha = best_val;
    }

    moveselect_t move_select;

    int32_t played = 0;
    init_select(&td->board, &move_select, 0, !incheck);

    while (true) {
        move_t cur_move = select_move(board, &move_select);
        if (cur_move == 0)
            break;
        dstate_t undo;
        if (!perform_move(board, cur_move, &undo)) {
            // illegal move; try next
            undo_move(board, &undo);
            continue;
        }

        played++;

        int16_t score = -qsearch(td, -beta, -alpha, ply + 1);
        undo_move(board, &undo);
        if (score >= beta) {
            return score;
        }
        if (score > best_val) {
            best_val = score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    if (played == 0) {
        if (incheck)
            return INT16_MIN + 1 + ply;
    }

    return best_val;
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

void init_select(board_t *board, moveselect_t *move_select, move_t tt_move,
                 bool nonquiet_only) {
    move_select->phase = (tt_move != 0) ? TT_MOVE : GEN_MOVES;
    move_select->tt_move = tt_move;
    move_select->nonquiet_only = nonquiet_only;
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
                                   &ms->count, ms->nonquiet_only);
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
