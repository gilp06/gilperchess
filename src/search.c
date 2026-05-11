#include <setjmp.h>
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

volatile bool ABORT_SIGNAL = 1;

const searchparams_t infinite_search = {.movetime = 0,
                                        .wtime = 0,
                                        .btime = 0,
                                        .winc = 0,
                                        .binc = 0,
                                        .nodes = 0,
                                        .depth = 0,
                                        .movestogo = 0};

#define THREAD_COUNT 1

sthreaddata_t td[THREAD_COUNT];
pthread_t pts[THREAD_COUNT - 1]; // reuse main search thread

static void store_pv(int ply, move_t move, sthreaddata_t *td) {
    // store pv into the table and propogate it upward
    td->pv_array[ply][0] = move;
    for (int i = 0; i < td->pv_length[ply + 1]; i++) {
        td->pv_array[ply][i + 1] = td->pv_array[ply + 1][i];
    }
    td->pv_length[ply] = td->pv_length[ply + 1] + 1;
}

static bool should_abort(const sthreaddata_t *td) {
    searchlimits_t limits = td->gs->limits;

    if (td->depth_finished > 0 && (td->nodes & 1023) == 1023 &&
        ((limits.depth != 0 && td->depth_finished == limits.depth) ||
         (limits.time != 0 &&
          (get_real_time() - td->gs->start_time) >= limits.time) ||
         (limits.nodes != 0 && td->nodes >= limits.nodes / THREAD_COUNT))) {

        return true;
    }
    return false;
}

static searchlimits_t gen_limits(searchparams_t *params, side_t side_to_move) {

    searchlimits_t limits;
    limits.depth = params->depth;
    limits.nodes = params->nodes;
    limits.time = 0;

    if (params->movetime != 0) {
        limits.time = params->movetime;
    } else {
        if (side_to_move == SIDE_WHITE) {
            if (params->winc != 0) {
                limits.time += params->winc / 2;
            }
            if (params->wtime != 0) {
                limits.time += params->wtime / 20;
            }
        } else {
            if (params->binc != 0) {
                limits.time += params->binc / 2;
            }
            if (params->btime != 0) {
                limits.time += params->btime / 20;
            }
        }
    }

    if (params->depth != 0) {
        limits.depth = params->depth;
    }

    if (params->nodes != 0) {
        limits.nodes = params->nodes;
    }

    if (limits.time > 11) {
        limits.time -= 10;
    }

    return limits;
}

void *go_search(void *arg) {
    gosearchdata_t *search_data = (gosearchdata_t *)arg;
    board_t const *starting_board = search_data->starting_board;
    searchparams_t params = search_data->search_settings;
    globalstate_t *gs = search_data->gs;

    gs->limits = gen_limits(&params, starting_board->side_to_move);
    gs->start_time = get_real_time();

    search_bestmove(gs, starting_board);

    return NULL;
}

void search_bestmove(globalstate_t *gs, board_t const *starting_board) {

    ABORT_SIGNAL = 0;

    // init thread datas
    for (int i = 0; i < THREAD_COUNT; i++) {
        td[i].gs = gs;
        // we assume whatever is here is the most recent valid? data.
        td[i].best_move = 0;
        td[i].score = 0;
        td[i].depth_finished = 0;
        td[i].board = *starting_board; // give copy of board
        td[i].worker = true;
        td[i].nodes = 0;
        memset(td[i].pv_length, 0, sizeof(td[i].pv_length));
        memset(td[i].pv_array, 0, sizeof(td[i].pv_array));
    }

    // identify main thread
    td[0].worker = false;

    // start worker threads
    for (int i = 1; i < THREAD_COUNT; i++) {
        pthread_create(&pts[i - 1], NULL, search, &td[i]);
    }
    search(&td[0]);

    ABORT_SIGNAL = 1;
    for (int i = 1; i < THREAD_COUNT; i++) {
        pthread_join(pts[i - 1], NULL);
    }

    // for now, just pick best move from thread 0
    move_t best_move = td[0].pvs[0];
    char move_str[6];
    move_to_string(best_move, move_str);

    printf("bestmove %s\n", move_str);
    fflush(stdout);
}

void *search(void *arg) {
    sthreaddata_t *td = (sthreaddata_t *)arg;

    int16_t alpha = -INT16_MAX;
    int16_t beta = INT16_MAX;

    int16_t cur_depth = 1;
    while (1) {
        if (setjmp(td->jmp))
            break;

        if (cur_depth >= 5) {
            int16_t delta = 120 + cur_depth * 5;
            alpha = td->score - delta;
            beta = td->score + delta;
        }

        int16_t score = alphabeta(td, true, cur_depth, alpha, beta, 0);
        if (score < alpha) {
            alpha = -INT16_MAX;
            score = alphabeta(td, true, cur_depth, alpha, beta, 0);
        } else if (score > beta) {
            beta = INT16_MAX;
            score = alphabeta(td, true, cur_depth, alpha, beta, 0);
        }

        td->score = score;
        td->depth_finished = cur_depth;
        memcpy(td->pvs, td->pv_array[0], sizeof(td->pv_array[0]));
        cur_depth++;

        if (td->worker)
            continue;

        // output info (should probably include the sum of all node counts)
        double cur_time = get_real_time() - td->gs->start_time;
        printf("info nodes %llu depth %d score cp %d time %llu pv ",
               td[0].nodes, td[0].depth_finished, td[0].score,
               (uint64_t)cur_time);
        for (int i = 0; i < td[0].pv_length[0]; i++) {
            char move_buf[6];
            move_to_string(td[0].pv_array[0][i], move_buf);
            printf("%s ", move_buf);
        }
        printf("\n");
        fflush(stdout);
    }
    return NULL;
}

int16_t alphabeta(sthreaddata_t *td, bool root_node, int16_t depth,
                  int16_t alpha, int16_t beta, int16_t ply) {

    depth = depth > 0 ? depth : 0;
    globalstate_t *gs = td->gs;
    board_t *board = &td->board;

    if (ABORT_SIGNAL || should_abort(td)) {
        longjmp(td->jmp, 1);
    }

    td->nodes++;
    // td->pv_length[ply] = 0;

    bool pv_node = alpha != beta - 1;
    bool incheck = in_check(board, board->side_to_move);
    int16_t alpha_orig = alpha;
    int16_t tt_move = 0;

    int16_t eval = evaluate(board);

    if (is_draw(board)) {
        return 0;
    }

    // check transposition table
    tt_entry_t entry = get_tt_entry(&gs->transposition_table, board->st.key);
    if (entry.flag != INVALID && entry.hash == board->st.key) {

        if (entry.depth >= depth && (depth == 0 || !pv_node)) {
            if (entry.flag == EXACT ||
                (entry.flag == LOWERBOUND && entry.value >= beta) ||
                (entry.flag == UPPERBOUND && entry.value <= alpha))
                return entry.value;
        }

        tt_move = entry.bestmove;
    }

    if (depth <= 3 && !incheck && eval + (150 + 100 * depth) < alpha) {
        int16_t qscore = qsearch(td, alpha, beta, ply + 1);
        if (qscore < alpha)
            return qscore;
    }

    if (depth == 0)
        return qsearch(td, alpha, beta, ply + 1);

    if (depth >= 3 && !incheck && !pv_node) {
        int16_t R = 3 + depth / 6;
        dstate_t undo;
        perform_null_move(board, &undo);
        int16_t score =
            -alphabeta(td, false, depth - 1 - R, -beta, -(beta - 1), ply);
        undo_null_move(board, &undo);
        if (score >= beta) {
            return score;
        }
    }

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

        bool iscapture = is_capture(board, cur_move);

        dstate_t undo;
        if (!perform_move(board, cur_move, &undo)) {
            // illegal move; try next
            undo_move(board, &undo);
            continue;
        }
        played++;

        if (played == 1) {
            value = -alphabeta(td, false, depth - 1, -beta, -alpha, ply + 1);
        }
        // late move pruning
        else if (depth >= 3 && played >= 4 && !pv_node && !incheck &&
                   !iscapture && (move_type(cur_move) != PROMOTION)) {
            value =
                -alphabeta(td, false, depth - 2, -alpha - 1, -alpha, ply + 1);
            if (value > alpha) {
                value =
                    -alphabeta(td, false, depth - 1, -beta, -alpha, ply + 1);
            }
        }
        // PVS search
        else {
            value =
                -alphabeta(td, false, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (pv_node && value > alpha) {
                value =
                    -alphabeta(td, false, depth - 1, -beta, -alpha, ply + 1);
            }
        }

        undo_move(board, &undo);

        if (value > best_value) {
            best_value = value;
            best_move = cur_move;

            if (value > alpha) {
                alpha = value;
                store_pv(ply, best_move, td);
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }

    if (played == 0) {
        if (incheck)
            return INT16_MIN + ply + 1;
        else
            return 0;
    }

    if (!root_node) {

        // bool stop = atomic_load(&gs->stop);
        // if stopped, we don't want to write this value because its children
        // are tainted.

        tt_entry_t entry;
        entry.depth = depth;
        entry.hash = board->st.key;
        entry.value = best_value;
        entry.flag = best_value >= beta         ? LOWERBOUND
                     : best_value <= alpha_orig ? UPPERBOUND
                                                : EXACT;
        entry.bestmove = (entry.flag == UPPERBOUND) ? 0 : best_move;

        write_tt_entry(&gs->transposition_table, entry);
    }

    return best_value;
}

int16_t qsearch(sthreaddata_t *td, int16_t alpha, int16_t beta, int16_t ply) {

    globalstate_t *gs = td->gs;
    board_t *board = &td->board;
    int16_t alpha_orig = alpha;

    if (ABORT_SIGNAL || should_abort(td)) {
        longjmp(td->jmp, 1);
    }

    // atomic_fetch_add(&gs->nodes, 1);
    td->nodes++;
    // atomic_store(&gs->nodes, atomic_load(&gs->nodes)+1);
    // gs->nodes++;

    move_t tt_move = 0, best_move = 0;
    tt_entry_t entry = get_tt_entry(&gs->transposition_table, board->st.key);
    if (entry.flag != INVALID && entry.hash == board->st.key) {

        // if (entry.depth >= depth && (depth == 0 || !pv_node)) {
        if (entry.flag == EXACT ||
            (entry.flag == LOWERBOUND && entry.value >= beta) ||
            (entry.flag == UPPERBOUND && entry.value <= alpha))
            return entry.value;
        // }

        tt_move = entry.bestmove;
    }

    if (is_draw(board))
        return 0;

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
    init_select(&td->board, &move_select, tt_move, !incheck);

    while (true) {
        move_t cur_move = select_move(board, &move_select);
        if (cur_move == 0)
            break;
        if (!see(board, cur_move, 0))
            continue;
        dstate_t undo;
        if (!perform_move(board, cur_move, &undo)) {
            // illegal move; try next
            undo_move(board, &undo);
            continue;
        }

        played++;

        int16_t score = -qsearch(td, -beta, -alpha, ply + 1);
        undo_move(board, &undo);

        if (score > best_val) {
            best_val = score;
            best_move = cur_move;

            if (score > alpha) {
                alpha = score;
            }
            if (alpha >= beta)
                break;
        }
    }

    if (played == 0) {
        if (incheck)
            return INT16_MIN + 1 + ply;
    }

    entry.depth = 0;
    entry.hash = board->st.key;
    entry.value = best_val;
    entry.flag = best_val >= beta         ? LOWERBOUND
                 : best_val <= alpha_orig ? UPPERBOUND
                                          : EXACT;
    entry.bestmove = (entry.flag == UPPERBOUND) ? 0 : best_move;

    write_tt_entry(&gs->transposition_table, entry);

    return best_val;
}
