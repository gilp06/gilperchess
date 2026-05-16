#include <stdlib.h>
#include <string.h>

#include "search.h"
#include "move_select.h"
#include "move_gen.h"
#include "utils.h"

static const uint8_t MVV_LVA[7][7] = {
    {15, 14, 13, 12, 11, 10, 0}, {25, 24, 23, 22, 21, 20, 0},
    {35, 34, 33, 32, 31, 30, 0}, {45, 44, 43, 42, 41, 40, 0},
    {55, 54, 53, 52, 51, 50, 0}, {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
};

#define MAX_HISTORY 25000


void update_history(sthreaddata_t* td, side_t stm, bindex_t from, bindex_t to, int16_t bonus) {
    int16_t clamped_bonus = CLAMP(bonus, -MAX_HISTORY, MAX_HISTORY);
    int16_t cur_val = td->quiet_history[stm][from][to];
    td->quiet_history[stm][from][to] += clamped_bonus - cur_val * abs(clamped_bonus) / MAX_HISTORY;
}

bool see(board_t *board, move_t move, int16_t threshold) {
    // static exchange from stockfish
    piece_t *pieces = board->pieces_at;
    bb_t *sides_occ = board->sides_occ;
    bb_t *pieces_occ = board->pieces_occ;

    if (move_type(move) != NORMAL)
        return 0 >= threshold;

    bindex_t from = move_from(move);
    bindex_t to = move_to(move);

    int16_t swap = piece_values[pieces[to]] - threshold;
    if (swap < 0)
        return false;

    swap = piece_values[pieces[from]] - swap;
    if (swap <= 0)
        return true;

    bb_t occupied = board->sides_occ[SIDE_WHITE] | board->sides_occ[SIDE_BLACK];
    occupied ^= (1ULL << from) ^ (1ULL << to);
    side_t stm = board->side_to_move;

    bb_t attackers = attackers_to(board, to, occupied);
    bb_t stm_attackers, bb;
    int res = 1;

    while (true) {
        stm = !stm;
        attackers &= occupied;


        if (!(stm_attackers = attackers & sides_occ[stm]))
            break;

        if (board->st.pinners[!stm] & occupied) {
            stm_attackers &= ~board->st.blockers[stm];

            if (!stm_attackers) {
                break;
            }
        }

        res = !res;

        if ((bb = stm_attackers & pieces_occ[PIECETYPE_PAWN])) {
            if ((swap = VAL_PAWN - swap) < res) {
                break;
            }
            occupied ^= (1ULL << __builtin_ctzll(bb));
            attackers |=
                fetch_bishop_moves(to, occupied) &
                (pieces_occ[PIECETYPE_BISHOP] | pieces_occ[PIECETYPE_QUEEN]);
        } else if ((bb = stm_attackers & pieces_occ[PIECETYPE_KNIGHT])) {
            if ((swap = VAL_KNIGHT - swap) < res) {
                break;
            }
            occupied ^= (1ULL << __builtin_ctzll(bb));
        } else if ((bb = stm_attackers & pieces_occ[PIECETYPE_BISHOP])) {
            if ((swap = VAL_BISHOP - swap) < res) {
                break;
            }
            occupied ^= (1ULL << __builtin_ctzll(bb));
            attackers |=
                fetch_bishop_moves(to, occupied) &
                (pieces_occ[PIECETYPE_BISHOP] | pieces_occ[PIECETYPE_QUEEN]);
        } else if ((bb = stm_attackers & pieces_occ[PIECETYPE_ROOK])) {
            if ((swap = VAL_ROOK - swap) < res) {
                break;
            }
            occupied ^= (1ULL << __builtin_ctzll(bb));
            attackers |=
                fetch_rook_moves(to, occupied) &
                (pieces_occ[PIECETYPE_ROOK] | pieces_occ[PIECETYPE_QUEEN]);
        } else if ((bb = stm_attackers & pieces_occ[PIECETYPE_QUEEN])) {
            if ((swap = VAL_QUEEN - swap) < res) {
                break;
            }
            occupied ^= (1ULL << __builtin_ctzll(bb));
            attackers |=
                (fetch_bishop_moves(to, occupied) &
                 (pieces_occ[PIECETYPE_BISHOP] | pieces_occ[PIECETYPE_QUEEN])) |
                (fetch_rook_moves(to, occupied) &
                 (pieces_occ[PIECETYPE_ROOK] | pieces_occ[PIECETYPE_QUEEN]));
        }
        else {
            return (attackers & ~sides_occ[stm]) ? !res : res;
        }
    }
    // printf("%d\n", res);
    return res;
}

static int16_t score_move(board_t *board, moveselect_t* ms, move_t move) {
    piece_t from = board->pieces_at[move_from(move)];
    piece_t to = board->pieces_at[move_to(move)];
    if (to == PIECE_NONE) {
        if(move == ms->killer[0]) return 30000;        
        if(move == ms->killer[1]) return 29000;
        return ms->td->quiet_history[board->side_to_move][move_from(move)][move_to(move)];
    }

    int16_t offset = MVV_LVA[piece_type(to)][piece_type(from)];
    

    return offset;
}

void score_moves(board_t *board, moveselect_t *move_select, size_t start, size_t end) {
    for (int i = start; i < end; i++) {
        move_select->move_scores[i] = score_move(board, move_select, move_select->moves[i]);
    }
}

void init_select(sthreaddata_t* td, moveselect_t *move_select, move_t tt_move, move_t killers[2], selecttype_t type) {
    move_select->td = td;
    move_select->phase = (tt_move != 0) ? TT_MOVE : (type != ALL_QUIET) ? GEN_LOUD : GEN_QUIET;
    move_select->tt_move = tt_move;
    move_select->select_type = type;
    move_select->loud_count = 0;
    move_select->quiet_count = 0;
    move_select->quiet_offset = 0;
    move_select->killer[0] = killers[0];
    move_select->killer[1] = killers[1];
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

// static size_t get_index(size_t *length, move_t *moves, move_t move) {
//     // returns length if not found
//     for (size_t i = 0; i < *length; i++) {
//         if (moves[i] == move) {
//             return i;
//         }
//     }
//     return *length;
// }

move_t select_move(board_t *board, moveselect_t *ms) {

    move_t *moves = ms->moves;
    int16_t *scores = ms->move_scores;

    move_t best_move = 0;
    size_t best;

    switch (ms->phase) {
        case TT_MOVE:
            // printf("tt move\n");
            ms->phase = GEN_LOUD;
            return ms->tt_move;
        case GEN_LOUD:
            // printf("gen loud\n");
            generate_pseudolegal_moves(board, board->side_to_move, moves, &ms->loud_count, false);
            score_moves(board, ms, 0, ms->loud_count);
            ms->quiet_offset = ms->loud_count;
            ms->phase = GOOD_LOUD_MOVES;
        case GOOD_LOUD_MOVES:
            while (ms->loud_count) {
            // printf("gen loud moves\n");
                best = get_best(ms, 0, ms->loud_count);

                if (ms->move_scores[best] < 0)
                    break; // looking at bad loud moves

                if (!see(board, ms->moves[best], 0)) {
                    ms->move_scores[best] = -1;
                    continue;
                }

                best_move = pop_move(&ms->loud_count, moves, scores, best);
                if (best_move == ms->tt_move)
                    continue;

                return best_move;
            }

            if (ms->select_type == NON_QUIET) {
                ms->phase = BAD_LOUD_MOVES;
                return select_move(board, ms);
            }

            ms->phase = GEN_QUIET;

        case GEN_QUIET:
            // printf("gen quiet moves\n");
            if (ms->select_type != NON_QUIET) {
                generate_pseudolegal_moves(board, board->side_to_move, moves + ms->quiet_offset, &ms->quiet_count, true);
                score_moves(board, ms, ms->quiet_offset, ms->quiet_offset + ms->quiet_count);
            }
            ms->phase = QUIET_MOVES;
        case QUIET_MOVES:
            while (ms->select_type != NON_QUIET && ms->quiet_count) {
                best = get_best(ms, ms->quiet_offset, ms->quiet_offset + ms->quiet_count);
                best_move = pop_move(&ms->quiet_count, moves + ms->quiet_offset, scores + ms->quiet_offset, best - ms->quiet_offset);
                if (best_move == ms->tt_move) continue;
                return best_move;
            }
            
            ms->phase = BAD_LOUD_MOVES;
        case BAD_LOUD_MOVES:
            // printf("bad loud moves\n");
            while(ms->loud_count) {
                best_move = pop_move(&ms->loud_count, moves, scores, 0);

                if (best_move == ms->tt_move)
                    continue;

                return best_move;
            }
            ms->phase = DONE;
        case DONE:
            // printf("done!\n");
            return 0;
        default:
            return 0;
    }
}
