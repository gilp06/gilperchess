#include "move_select.h"

static const uint8_t MVV_LVA[7][7] = {
    {15, 14, 13, 12, 11, 10, 0}, {25, 24, 23, 22, 21, 20, 0},
    {35, 34, 33, 32, 31, 30, 0}, {45, 44, 43, 42, 41, 40, 0},
    {55, 54, 53, 52, 51, 50, 0}, {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
};

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

        // todo remove pinned pieces

        res ^= 1;

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
            return (attackers & ~sides_occ[stm]) ? res ^ 1 : res;
        }
    }
    return res;
}

static int16_t score_move(board_t *board, move_t move) {
    piece_t from = board->pieces_at[move_from(move)];
    piece_t to = board->pieces_at[move_to(move)];
    if (to == PIECE_NONE)
        return 0;

    int16_t offset = MVV_LVA[piece_type(to)][piece_type(from)];
    if (see(board, move, 0))
        offset += 1000;
    else
        offset -= 1000;
    

    return offset;
}

void score_moves(board_t *board, moveselect_t *move_select) {
    for (int i = 0; i < move_select->count; i++) {
        move_select->move_scores[i] = score_move(board, move_select->moves[i]);
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
