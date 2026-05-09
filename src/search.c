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
//
//

const searchsettings_t infinite_search = {.movetime = 0,
                                          .wtime = 0,
                                          .btime = 0,
                                          .winc = 0,
                                          .binc = 0,
                                          .nodes = 0,
                                          .depth = 200,
                                          .movestogo = 0};

static void *search_thread(void *arg);

sthreaddata_t td[1];
#define THREAD_COUNT 1

static bool all_threads_done() {
    bool done;
    for (int i = 0; i < THREAD_COUNT; i++) {
        done = atomic_load(&td[i].done);
        if (!done)
            return false;
    }
    return true;
}

void *go_search(void *arg) {
    gosearchdata_t *search_data = (gosearchdata_t *)arg;
    board_t *starting_board = search_data->starting_board;
    searchsettings_t search_settings = search_data->search_settings;
    globalstate_t *gs = search_data->gs;

    start_search(gs, starting_board, search_settings);
    return NULL;
}

void start_search(globalstate_t *gs, board_t *starting_board,
                  searchsettings_t search_settings) {

    double start_time;
    double elapsed_ms;

    gs->best_move = 0;
    gs->nodes = 0;
    gs->alpha = -INT16_MAX;
    gs->beta = INT16_MAX;
    gs->stop = false;

    int16_t cur_score;
    pthread_t thread;

    // TODO: better time control
    uint64_t timelimit = 0;
    if (search_settings.movetime != 0) {
        timelimit = search_settings.movetime;
    }
    if (starting_board->side_to_move == SIDE_WHITE &&
        search_settings.wtime != 0) {
        timelimit = search_settings.wtime / 20 + search_settings.winc / 2;
    }
    if (starting_board->side_to_move == SIDE_BLACK &&
        search_settings.btime != 0) {
        timelimit = search_settings.btime / 20 + search_settings.binc / 2;
    }
    timelimit = timelimit > 5 ? timelimit-5 : 0;

    start_time = get_real_time();

    for (int i = 1; i <= search_settings.depth; i++) {
        memcpy(&td[0].board, starting_board, sizeof(board_t));
        td[0].gs = gs;
        td[0].depth = i;
        td[0].done = false;

        if (i > 5) {
            // guess
            int16_t delta = 60 + i * 2;
            gs->alpha = cur_score - delta;
            gs->beta = cur_score + delta;
        }

        pthread_create(&thread, NULL, search_thread, (void *)&td[0]);

        while (!all_threads_done() && !atomic_load(&gs->stop)) {
            elapsed_ms = get_real_time() - start_time;
            if (search_settings.nodes != 0 &&
                gs->nodes >= search_settings.nodes) {
                atomic_store(&gs->stop, true);
                break;
            }

            if (timelimit != 0) {
                if (elapsed_ms >= timelimit) {
                    atomic_store(&gs->stop, true);
                    break;
                }
            }
        }

        pthread_join(thread, NULL);

        if (atomic_load(&gs->stop))
            break;

        if (i > 5) {
            // check if we failed aspiration
            int16_t score = td[0].score;
            bool fail = false;
            if (score <= gs->alpha) {
                gs->alpha = -INT16_MAX;
                fail = true;
            } else if (score >= gs->beta) {
                gs->beta = INT16_MAX;
                fail = true;
            }
            if (fail) {
                // restart search without aspiration
                // printf("missed aspiration\n");
                memcpy(&td[0].board, starting_board, sizeof(board_t));
                td[0].gs = gs;
                td[0].depth = i;
                pthread_create(&thread, NULL, search_thread, (void *)&td[0]);
                // wait for thread to finish threading
                while (!all_threads_done() && !atomic_load(&gs->stop)) {
                    elapsed_ms = get_real_time() - start_time;
                    if (search_settings.nodes != 0 &&
                        gs->nodes >= search_settings.nodes) {
                        atomic_store(&gs->stop, true);
                        break;
                    }

                    if (timelimit != 0) {
                        if (elapsed_ms >= timelimit) {
                            atomic_store(&gs->stop, true);
                            break;
                        }
                    }
                }
                pthread_join(thread, NULL);
            }
        }

        
        if (atomic_load(&gs->stop))
            break;


        elapsed_ms = get_real_time() - start_time;
        uint64_t nps =
            (elapsed_ms > 0) ? (gs->nodes * 1000ULL) / elapsed_ms : 0;

        // retreive the score and the current best move
        cur_score = td[0].score;
        move_t move = td[0].best_move;

        // update current available best move and score
        gs->best_move = move;
        printf("info nodes %llu depth %d score cp %d nps %llu time %llu\n",
               gs->nodes, i, cur_score, nps, (uint64_t)elapsed_ms);
        fflush(stdout);
    }


    elapsed_ms = get_real_time() - start_time;

    // printf("info nodes %llu time %llu\n", gs->nodes, (uint64_t)elapsed_ms);

    char m[6];
    move_to_string(gs->best_move, m);
    printf("bestmove %s\n", m);
    fflush(stdout);
}

static void *search_thread(void *arg) {
    sthreaddata_t *td = (sthreaddata_t *)arg;
    globalstate_t *gs = td->gs;

    if (setjmp(td[0].jmp)) {
        fflush(stdout);
        atomic_store(&td->done, true);
        return NULL;
    };

    td->score = alphabeta(td, true, td->depth, gs->alpha, gs->beta, 0);

    atomic_store(&td->done, true);
    return NULL;
}

int16_t alphabeta(sthreaddata_t *td, bool root_node, int16_t depth,
                  int16_t alpha, int16_t beta, int16_t ply) {

    globalstate_t *gs = td->gs;
    board_t *board = &td->board;

    // bool stop = atomic_load(&gs->stop);
    if (atomic_load(&gs->stop)) {
        longjmp(td->jmp, 1);
    }

    gs->nodes++;

    int16_t alpha_orig = alpha;
    int16_t tt_move = 0;

    if (is_draw(board)) {
        return 0;
    }

    // check transposition table
    tt_entry_t entry = get_tt_entry(gs->transposition_table, board->st.key);
    if (entry.flag != INVALID && entry.hash == board->st.key) {

        if (entry.depth >= depth) {
            // if (entry.flag == EXACT) {
            //     return entry.value;
            // }
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

        if (atomic_load(&gs->stop))
            longjmp(td->jmp, 1);
        
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

        // value = -alphabeta(td, false, depth-1, -beta, -alpha, ply+1);
        // sketchy PV node thing, assume that the first move is PV node (its
        // not)
        if (played == 1) {
            value = -alphabeta(td, false, depth - 1, -beta, -alpha, ply + 1);
        } else {
            value =
                -alphabeta(td, false, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (alpha < value && value < beta) {
                value =
                    -alphabeta(td, false, depth - 1, -beta, -alpha, ply + 1);
            }
        }

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

        write_tt_entry(gs->transposition_table, entry);
    } else {
        td->best_move = best_move;
    }

    return best_value;
}

int16_t qsearch(sthreaddata_t *td, int16_t alpha, int16_t beta, int16_t ply) {

    globalstate_t *gs = td->gs;
    board_t *board = &td->board;

    // bool stop = atomic_load(&gs->stop);
    if (atomic_load(&gs->stop)) {
        longjmp(td->jmp, 1);
    }

    gs->nodes++;

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
    init_select(&td->board, &move_select, 0, !incheck);

    while (true) {

        if (atomic_load(&gs->stop))
            longjmp(td->jmp, 1);
        
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

    if (gs->stop) {
        return 0;
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
