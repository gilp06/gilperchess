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
#define CHECKMATE 30000

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

    if (((td->depth_finished > 0 && (td->nodes & 1023) == 1023) &&
         ((limits.depth != 0 && td->depth_finished >= limits.depth) ||
          td->depth_finished >= MAX_DEPTH)) ||
        (limits.time != 0 &&
         (get_real_time() - td->gs->start_time) >= limits.time) ||
        (limits.nodes != 0 && td->nodes >= limits.nodes / THREAD_COUNT)) {

        return true;
    }
    return false;
}

static int convert_mate(int16_t score) {
    if (score > CHECKMATE - 1000) {
        int ply_to_mate = CHECKMATE - score;
        return (ply_to_mate + 1) / 2;
    }
    if (score < -CHECKMATE + 1000) {
        int ply_to_mate = score + CHECKMATE;
        return -(((ply_to_mate) + 1) / 2);
    }
    return INT16_MAX;
}

static inline int16_t score_to_tt(int16_t score, int16_t ply) {
    if (score > CHECKMATE - 1000)
        return score + ply;
    if (score < -CHECKMATE + 1000)
        return score - ply;
    return score;
}

static inline int16_t score_from_tt(int16_t score, int16_t ply) {
    if (score > CHECKMATE - 1000)
        return score - ply;
    if (score < -CHECKMATE + 1000)
        return score + ply;
    return score;
}

static inline void store_killer(sthreaddata_t *td, move_t move, int ply) {
    if (td->killers[ply][0] != move) {
        td->killers[ply][1] = td->killers[ply][0];
        td->killers[ply][0] = move;
    }
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
        memset(td->killers, 0, sizeof(td->killers));
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

    printf("bestmove %s ", move_str);

    if (td[0].tdpv_len > 1) {
        move_t ponder_move = td[0].pvs[1];
        move_to_string(ponder_move, move_str);
        printf("ponder %s", move_str);
    }

    printf("\n");
    fflush(stdout);
}

void *search(void *arg) {
    sthreaddata_t *td = (sthreaddata_t *)arg;

    int16_t alpha = -INT16_MAX;
    int16_t beta = INT16_MAX;

    int16_t cur_depth = 1;
    while (1) {

        if (cur_depth >= MAX_DEPTH)
            break;

        memset(td->pv_length, 0, sizeof(td->pv_length));
        if (setjmp(td->jmp))
            break;

        if (cur_depth >= 5) {
            int16_t delta = 10 + cur_depth * 2;
            alpha = td->score - delta;
            beta = td->score + delta;
        }

        int16_t score = alphabeta(td, true, false, cur_depth, alpha, beta, 0);
        if (score <= alpha) {
            alpha = -INT16_MAX;
            score = alphabeta(td, true, false, cur_depth, alpha, beta, 0);

            if (score >= beta) {
                beta = INT16_MAX;
                score = alphabeta(td, true, false, cur_depth, alpha, beta, 0);
            }

        } else if (score >= beta) {
            beta = INT16_MAX;
            score = alphabeta(td, true, false, cur_depth, alpha, beta, 0);

            if (score <= alpha) {
                alpha = -INT16_MAX;
                score = alphabeta(td, true, false, cur_depth, alpha, beta, 0);
            }
        }

        // if (missed)
        //     printf("missed aspiration window\n");

        td->score = score;
        td->depth_finished = cur_depth;
        memcpy(td->pvs, td->pv_array[0], sizeof(td->pv_array[0]));
        td->tdpv_len = td->pv_length[0];
        cur_depth++;

        if (td->worker)
            continue;

        // output info (should probably include the sum of all node counts)
        double cur_time = get_real_time() - td->gs->start_time;
        printf("info nps %llu nodes %llu depth %d time %llu ",
               (uint64_t)(td[0].nodes * 1000.0 / cur_time), td[0].nodes,
               td[0].depth_finished, (uint64_t)cur_time);

        int mate = convert_mate(td->score);
        if (mate != INT16_MAX) {
            printf("score mate %d pv ", mate);
            ABORT_SIGNAL = 1;
        } else {
            printf("score cp %d pv ", td->score);
        }

        for (int i = 0; i < td[0].pv_length[0]; i++) {
            if (td[0].pv_array[0][i] == 0)
                break;
            char move_buf[6];
            move_to_string(td[0].pv_array[0][i], move_buf);
            printf("%s ", move_buf);
        }

        if (td[0].pv_length[0] == 0) {
            printf("\nbad PV\n");
            exit(1);
        }

        printf("\n");
        fflush(stdout);
    }
    return NULL;
}

int16_t alphabeta(sthreaddata_t *td, bool root_node, bool from_null,
                  int16_t depth, int16_t alpha, int16_t beta, int16_t ply) {

    depth = depth > 0 ? depth : 0;
    globalstate_t *gs = td->gs;
    board_t *board = &td->board;

    if (ABORT_SIGNAL || should_abort(td)) {
        longjmp(td->jmp, 1);
    }

    td->nodes++;

    bool pv_node = alpha != beta - 1;
    bool incheck = in_check(board, board->side_to_move);
    int16_t alpha_orig = alpha;
    int16_t tt_move = 0;

    int16_t eval = evaluate(board);

    if (ply >= MAX_DEPTH) {
        return eval;
    }

    td->pv_length[ply] = 0;

    if (is_draw(board)) {
        return 0;
    }

    tt_entry_t entry = get_tt_entry(&gs->transposition_table, board->st.key);
    if (entry.flag != INVALID && entry.hash == board->st.key) {

        int16_t tt_score = score_from_tt(entry.value, ply);
        if (!root_node && entry.depth >= depth && (depth == 0 || !pv_node)) {
            if (entry.flag == EXACT ||
                (entry.flag == LOWERBOUND && tt_score >= beta) ||
                (entry.flag == UPPERBOUND && tt_score <= alpha))
                return tt_score;
        }

        tt_move = entry.bestmove;
    }

    if (!root_node) {

        if (depth == 0)
            return qsearch(td, alpha, beta, ply);

        // Reverse Futility Pruning (129.31 +/- 18.22 ELO [0, 2.5] SPRT)
        if (!pv_node && !incheck && depth < 4 && eval >= beta + 100 * depth) {
            return eval;
        } 

        
    }

    moveselect_t move_select;
    init_select(board, &move_select, tt_move, td->killers[ply], BOTH_TYPES);

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

        bool caused_check = in_check(board, board->side_to_move);

        int16_t reductions = 0;

        if (played == 1) {
            value =
                -alphabeta(td, false, false, depth - 1, -beta, -alpha, ply + 1);
        } else if (reductions != 0) {
            value = -alphabeta(td, false, false, depth - 1 - reductions,
                               -alpha - 1, -alpha, ply + 1);
            if (value > alpha && reductions > 0) {
                // research with original depth if we aren't extending
                value = -alphabeta(td, false, false, depth - 1, -alpha - 1,
                                   -alpha, ply + 1);
            }
        } else {
            value = -alphabeta(td, false, false, depth - 1, -alpha - 1, -alpha,
                               ply + 1);
        }
        if (pv_node && value > alpha) {
            value =
                -alphabeta(td, false, false, depth - 1, -beta, -alpha, ply + 1);
        }

        undo_move(board, &undo);

        if (value > best_value) {
            best_value = value;
            best_move = cur_move;

            if (value > alpha) {
                store_pv(ply, best_move, td);
                alpha = value;
                if (alpha >= beta) {
                    if (!iscapture && move_type(cur_move) != PROMOTION) {
                        store_killer(td, best_move, ply);
                    }
                    break;
                }
            }
        }
    }

    if (played == 0) {
        if (incheck)
            best_value = -CHECKMATE + ply;
        else
            best_value = 0;
    }

    if (!root_node) {

        tt_entry_t entry;
        entry.depth = depth;
        entry.hash = board->st.key;
        entry.value = score_to_tt(best_value, ply);
        entry.flag = best_value >= beta         ? LOWERBOUND
                     : best_value <= alpha_orig ? UPPERBOUND
                                                : EXACT;
        entry.bestmove = (entry.flag == UPPERBOUND) ? 0 : best_move;

        write_tt_entry(&gs->transposition_table, entry);
    }

    if (root_node && td->pv_length[ply] == 0) {
        if (best_move != 0)
            store_pv(ply, best_move, td);
        else {
            printf("failed to find move! terminating!\n");
            exit(1);
        }
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

    td->nodes++;

    if (ply >= MAX_DEPTH) {
        return evaluate(board);
    }

    move_t tt_move = 0, best_move = 0;
    tt_entry_t entry = get_tt_entry(&gs->transposition_table, board->st.key);
    if (entry.flag != INVALID && entry.hash == board->st.key) {

        int16_t tt_score = score_from_tt(entry.value, ply);
        // if (entry.depth >= depth && (depth == 0 || !pv_node)) {
        if (entry.flag == EXACT ||
            (entry.flag == LOWERBOUND && tt_score >= beta) ||
            (entry.flag == UPPERBOUND && tt_score <= alpha))
            return tt_score;
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
    selecttype_t type = incheck ? BOTH_TYPES : NON_QUIET;

    int32_t played = 0;
    move_t none_killers[2] = {0, 0};
    init_select(&td->board, &move_select, tt_move, none_killers, type);

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
            if (alpha >= beta) {
                break;
            }
        }
    }

    if (played == 0) {
        if (incheck)
            best_val = -CHECKMATE + ply;
    }

    entry.depth = 0;
    entry.hash = board->st.key;
    entry.value = score_to_tt(best_val, ply);
    entry.flag = best_val >= beta         ? LOWERBOUND
                 : best_val <= alpha_orig ? UPPERBOUND
                                          : EXACT;
    entry.bestmove = (entry.flag == UPPERBOUND) ? 0 : best_move;

    write_tt_entry(&gs->transposition_table, entry);

    return best_val;
}
