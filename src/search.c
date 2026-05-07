#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "hashtable.h"
#include "move_gen.h"
#include "search.h"
#include "utils.h"

move_t alphabeta_root(global_state_t* gs, board_t *board, int16_t depth);
int16_t alphabeta(global_state_t* gs, board_t *board, int depth, int16_t alpha, int16_t beta);

void perform_search(global_state_t* gs, board_t *starting_board, int depth) {

    move_t result = alphabeta_root(gs, starting_board, depth);
    if (result == 0) {
        printf("idk what to do :(\n");
    } else {
        char f[6];
        move_to_string(result, f);
        printf("bestmove %s\n", f);
        fflush(stdout);
    }
}

int16_t alphabeta(global_state_t *gs, board_t *board, int depth, int16_t alpha, int16_t beta) {
    int16_t alpha_orig = alpha;
            
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



    tt_entry_t entry = get_tt_entry(gs->transposition_table, board->st.key);

    if (entry.flag != INVALID && entry.hash == board->st.key && entry.depth >= depth)
    {
        if (entry.flag == EXACT)
            return entry.score;
        else if (entry.flag == LOWERBOUND && entry.score >= beta)
            return entry.score;
        else if (entry.flag == UPPERBOUND && entry.score <= alpha)
            return entry.score;
    }
    

    if (depth == 0)
        return evaluate(board);

    int16_t best_val = INT16_MIN + 1;

    move_t move_list[256];
    size_t size;
    uint8_t legal_moves = 0;

    generate_pseudolegal_moves(board, board->side_to_move, move_list, &size);
    for (int i = 0; i < size; i++) {
        dstate_t st;
        if (perform_move(board, move_list[i], &st)) {
            legal_moves++;
            int16_t score = -alphabeta(gs, board, depth - 1, -beta, -alpha);
            if (score > best_val) {
                best_val = score;
                if (score > alpha)
                    alpha = score;
            } 
            if (score >= beta) {
                undo_move(board, &st);
                break;
            }
        }
        undo_move(board, &st);
    }

    if (legal_moves == 0) {
        if (in_check(board, board->side_to_move))
            return INT16_MIN + board->st.fullmove_clock + 1;
        else
            return 0;
    }

    tt_entry_t new_entry;
    new_entry.hash = board->st.key;
    new_entry.score = best_val;
    if (best_val <= alpha_orig)
        new_entry.flag = UPPERBOUND;
    else if (best_val >= beta)
        new_entry.flag = LOWERBOUND;
    else
        new_entry.flag = EXACT;

    write_tt_entry(gs->transposition_table, new_entry);

    return best_val;
}

move_t alphabeta_root(global_state_t *gs, board_t *board, int16_t depth) {

    int16_t alpha = INT16_MIN + 1, beta = INT16_MAX;
    if (depth == 0)
        return 0;

    int16_t best_val = INT16_MIN + 1;
    move_t best_move = 0;

    move_t move_list[256];
    size_t size;

    bool found = false;
    generate_pseudolegal_moves(board, board->side_to_move, move_list, &size);
    for (int i = 0; i < size; i++) {
        dstate_t st;
        if (perform_move(board, move_list[i], &st)) {

            int16_t score = -alphabeta(gs, board, depth - 1, -beta, -alpha);
            if (!found || score > best_val) {
                found = true;
                best_move = move_list[i];
                best_val = score;
                if (score > alpha)
                    alpha = score;
            }
            if (score >= beta) {
                undo_move(board, &st);
                return best_move;
            }
        }
        undo_move(board, &st);
    }

    return best_move;
}

// move_t search_root(board_t *board, int depth) {
//     if (depth == 0)
//         return 0;
//     int16_t max_v = INT16_MIN;
//     move_t best_move = 0;
//     move_t move_list[256];
//     size_t size;
//     generate_pseudolegal_moves(board, board->side_to_move, move_list, &size);

//     for (int i = 0; i < size; i++) {
//         // perform move
//         dstate_t st;
//         if (perform_move(board, move_list[i], &st)) {
//             int16_t score = -search(board, depth - 1);
//             if (score > max_v) {
//                 best_move = move_list[i];
//                 max_v = score;
//             }
//         }
//         undo_move(board, &st);
//     }
//     return best_move;
// }

// int16_t search(board_t *board, int depth) {

//     if(board->st.halfmove_clock == 100)
//         return 0;

//     int repetitions = 0;
//     for (int i = board->move_number-2; i >= 0; i--)
//     {
//         if(board->st.key == board->key_hist[i]) {
//             repetitions++;
//         }
//     }

//     if(repetitions >= 2) {
//          return INT16_MIN+1;
//     }

//     if (depth == 0)
//         return evaluate(board);

//     int16_t max_v = INT16_MIN+1;
//     move_t move_list[256];
//     size_t size,legal_moves=0;
//     generate_pseudolegal_moves(board, board->side_to_move, move_list, &size);

//     for (int i = 0; i < size; i++) {
//         // perform move
//         dstate_t st;
//         if (perform_move(board, move_list[i], &st)) {
//             legal_moves++;
//             int16_t score = -search(board, depth - 1);
//             if (score > max_v) {
//                 max_v = score;
//             }
//         }
//         undo_move(board, &st);
//     }

//      if(legal_moves == 0)
//     {
//         if(in_check(board, board->side_to_move))
//             return INT16_MIN + board->st.fullmove_clock;
//         else
//             return 0;
//     }

//     return max_v;
// }
