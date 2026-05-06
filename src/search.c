#include <stdint.h>
#include <stdio.h>

#include "move_gen.h"
#include "search.h"
#include "utils.h"
#include "eval.h"

void perform_search(board_t *starting_board, int depth) {
    move_t result = search_root(starting_board, depth);
    if (result == 0) {
        printf("idk what to do :(\n");
    } else {
        // we have the move, lets try it
        char f[6];
        move_to_string(result, f);
        printf("bestmove %s\n", f);
        fflush(stdout);
    }
}

move_t search_root(board_t *board, int depth) {
    if (depth == 0)
        return 0;
    int16_t max_v = INT16_MIN;
    move_t best_move = 0;
    move_t move_list[256];
    size_t size;
    generate_pseudolegal_moves(board, board->side_to_move, move_list, &size);

    for (int i = 0; i < size; i++) {
        // perform move
        dstate_t st;
        if (perform_move(board, move_list[i], &st)) {
            int16_t score = -search(board, depth - 1);
            if (score > max_v) {
                best_move = move_list[i];
                max_v = score;
            }
        }
        undo_move(board, &st);
    }
    return best_move;
}

int16_t search(board_t *board, int depth) {

    if(board->st.halfmove_clock == 100)
        return 0;    

    int repetitions = 0;
    for (int i = board->move_number-2; i >= 0; i--)
    {
        if(board->st.key == board->key_hist[i]) {
            repetitions++;
        }
    }

    if(repetitions >= 2) {
         return INT16_MIN+1;   
    }
    
    if (depth == 0)
        return evaluate(board);
        
    int16_t max_v = INT16_MIN+1;
    move_t move_list[256];
    size_t size,legal_moves=0;
    generate_pseudolegal_moves(board, board->side_to_move, move_list, &size);

    for (int i = 0; i < size; i++) {
        // perform move
        dstate_t st;
        if (perform_move(board, move_list[i], &st)) {
            legal_moves++;
            int16_t score = -search(board, depth - 1);
            if (score > max_v) {
                max_v = score;
            }
        }
        undo_move(board, &st);
    }

    if(legal_moves == 0)
    {
        if(in_check(board, board->side_to_move))
            return INT16_MIN + board->st.fullmove_clock;
        else
            return 0;
    }

    return max_v;
}
