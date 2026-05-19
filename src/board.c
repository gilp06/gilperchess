#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "board.h"
#include "hashtable.h"
#include "move_gen.h"
#include "nnue.h"
#include "types.h"
#include "utils.h"

uint8_t king_square(const board_t *board, side_t side) {
    return __builtin_ctzll(board->pieces_occ[PIECETYPE_KING] &
                           board->sides_occ[side]);
}

void set_piece(board_t *board, piece_t piece, bindex_t pos) {
    board->pieces_at[pos] = piece;
    board->pieces_occ[piece_type(piece)] |= 1ULL << pos;
    board->sides_occ[piece_side(piece)] |= 1ULL << pos;
}

void init_board_from_start(board_t *board) {
    init_board_from_fen(
        board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void init_board_from_fen(board_t *board, const char *str) {

    for (int i = 0; i < 64; i++)
        board->pieces_at[i] = PIECE_NONE;

    for (int i = 0; i < 6; i++) {
        board->pieces_occ[i] = 0;
    }

    board->sides_occ[SIDE_WHITE] = 0;
    board->sides_occ[SIDE_BLACK] = 0;

    int rank = 7; // rank 8 in chess terms
    int file = 0; // file a

    while (*str && rank >= 0) {
        char c = *str++;
        if (c == ' ')
            break;

        if (c == '/') {
            // move to next rank down
            rank--;
            file = 0;
            continue;
        }

        if (c >= '1' && c <= '8') {
            file += (c - '0');
            continue;
        }

        uint8_t pos = rank * 8 + file;

        switch (c) {

        case 'P':
            set_piece(board, PIECE_WPAWN, pos);
            break;
        case 'N':
            set_piece(board, PIECE_WKNIGHT, pos);
            break;
        case 'B':
            set_piece(board, PIECE_WBISHOP, pos);
            break;
        case 'R':
            set_piece(board, PIECE_WROOK, pos);
            break;
        case 'Q':
            set_piece(board, PIECE_WQUEEN, pos);
            break;
        case 'K':
            set_piece(board, PIECE_WKING, pos);
            break;

        case 'p':
            set_piece(board, PIECE_BPAWN, pos);
            break;
        case 'n':
            set_piece(board, PIECE_BKNIGHT, pos);
            break;
        case 'b':
            set_piece(board, PIECE_BBISHOP, pos);
            break;
        case 'r':
            set_piece(board, PIECE_BROOK, pos);
            break;
        case 'q':
            set_piece(board, PIECE_BQUEEN, pos);
            break;
        case 'k':
            set_piece(board, PIECE_BKING, pos);
            break;

        default:
            break;
        }
        file++;
    }

    while (*str == ' ')
        str++;

    board->side_to_move = !(*str == 'w');
    board->st.castling_rights = 0;
    str++;

    while (*str == ' ')
        str++;

    if (*str != '-') {
        while (*str && *str != ' ') {
            switch (*str) {
            case 'K':
                board->st.castling_rights |= CASTLING_RIGHTS_WKINGSIDE;
                break;
            case 'Q':
                board->st.castling_rights |= CASTLING_RIGHTS_WQUEENSIDE;
                break;
            case 'k':
                board->st.castling_rights |= CASTLING_RIGHTS_BKINGSIDE;
                break;
            case 'q':
                board->st.castling_rights |= CASTLING_RIGHTS_BQUEENSIDE;
                break;
            }

            str++;
        }
    } else {
        str++;
    }

    while (*str == ' ')
        str++;

    board->st.ep_square = 64; // none

    if (*str != '-' && *str != ' ') {
        char f = str[0];
        char r = str[1];

        if (f >= 'a' && f <= 'h' && r >= '1' && r <= '8') {
            uint8_t file = f - 'a';
            uint8_t rank = r - '1';
            board->st.ep_square = rank * 8 + file;
        }

        str += 2;
    } else if (*str == '-') {
        str++;
    }

    while (*str == ' ')
        str++;

    int halfmove = 0;
    while (*str >= '0' && *str <= '9') {
        halfmove = halfmove * 10 + (*str - '0');
        str++;
    }
    board->st.halfmove_clock = halfmove;

    while (*str == ' ')
        str++;

    int fullmove = 0;
    while (*str >= '0' && *str <= '9') {
        fullmove = fullmove * 10 + (*str - '0');
        str++;
    }

    board->st.key = generate_key_from_scratch(board);
    board->key_hist[0] = board->st.key;
    board->st.fullmove_clock = fullmove;
    board->move_number = 0;

    update_pinners_and_blockers(board, SIDE_WHITE);
    update_pinners_and_blockers(board, SIDE_BLACK);

    // INIT NNUE

    board->white_accum = NNUE->feature_biases;
    board->black_accum = NNUE->feature_biases;

    for (bindex_t sq = 0; sq < 64; sq++) {
        piece_t piece = board->pieces_at[sq];
        if (piece == PIECE_NONE)
            continue;
        nnue_add_piece(board, sq, piece);
    }
}

typedef bool (*move_fn)(board_t *board, move_t move, dstate_t *undo);

static bool handle_normal(board_t *board, move_t move, dstate_t *undo) {

    state_t *st = &(board->st);
    bb_t *pieces_occ = board->pieces_occ;
    bb_t *sides_occ = board->sides_occ;
    piece_t *pieces_at = board->pieces_at;

    bindex_t from = move_from(move);
    bindex_t to = move_to(move);
    side_t us = board->side_to_move;
    side_t them = !us;

    bb_t from_bb = 1ULL << from;
    bb_t to_bb = 1ULL << to;
    bb_t move_bb = from_bb | to_bb;

    piece_t pc = pieces_at[from];
    piece_t pcaptured = pieces_at[to];

    piecetype_t pc_type = piece_type(pc);

    bool cap = (pcaptured != PIECE_NONE);

    uint8_t castling_perm_change_mask = 0;

    pieces_occ[pc_type] ^= move_bb;
    sides_occ[us] ^= move_bb;
    pieces_at[to] = pc;
    pieces_at[from] = PIECE_NONE;

    if (cap) {
        nnue_remove_piece(board, to, pcaptured);

        st->halfmove_clock = 0;

        pieces_occ[piece_type(pcaptured)] ^= to_bb;
        sides_occ[them] ^= to_bb;
        undo->captured = pcaptured;
        st->key ^= fetch_random_piece(pcaptured, to);
    }

    // check if pawn
    if (pc_type == PIECETYPE_PAWN) {
        st->halfmove_clock = 0;

        bb_t from_mask = RANK_MASKS[DOUBLE_PUSH_MASK[us][0]];
        bb_t to_mask = RANK_MASKS[DOUBLE_PUSH_MASK[us][1]];

        if ((from_bb & from_mask) && (to_bb & to_mask)) {
            st->ep_square = to - PUSH_DIR[us];

            // polyglot requires checking if the ep square is actually possible
            // to take
            if (PAWN_ATTACKS[them][board->st.ep_square] &
                (pieces_occ[PIECETYPE_PAWN] & sides_occ[them])) {
                st->key ^=
                    RANDOM_64[RANDOM_ENPASSANT + SQUARE_TO_FILE[st->ep_square]];
                st->ep_was_possible = 1;
            }
        }
    }

    castling_perm_change_mask |= CASTLE_MASK[us][from];
    castling_perm_change_mask |= CASTLE_CAPTURE_MASK[us][to];

    board->st.key ^= fetch_random_piece(pc, from);
    board->st.key ^= fetch_random_piece(pc, to);
    board->st.key ^= castling_changes_key(castling_perm_change_mask &
                                          board->st.castling_rights);
    board->st.castling_rights &= ~castling_perm_change_mask;

    nnue_add_piece(board, to, pc);
    nnue_remove_piece(board, from, pc);

    return cap;
}

static bool handle_ep(board_t *board, move_t move, dstate_t *undo) {
    board->st.halfmove_clock = 0;

    bindex_t from = move_from(move);
    bindex_t to = move_to(move);
    side_t us = board->side_to_move;
    side_t them = !us;

    bb_t move_bb = (1ULL << from) | (1ULL << to);

    piece_t pc = board->pieces_at[from];
    piece_t pcaptured = make_piece(PIECETYPE_PAWN, them);
    piecetype_t pc_type = piece_type(pc);
    bindex_t cs = to + PUSH_DIR[them];
    undo->captured = board->pieces_at[cs];
    // assert(board->pieces_at[cs] == pcaptured);

    bb_t cap_board = (1ULL << cs);

    // update our boards
    board->pieces_occ[pc_type] ^=
        move_bb |
        cap_board; // we know its going to be a pawn so we can safely remove
    board->sides_occ[us] ^= move_bb;
    board->sides_occ[them] ^= cap_board;

    board->pieces_at[to] = pc;
    board->pieces_at[from] = PIECE_NONE;
    board->pieces_at[cs] = PIECE_NONE; // en passant capture

    board->st.key ^= fetch_random_piece(pc, from);
    board->st.key ^= fetch_random_piece(pc, to);
    board->st.key ^= fetch_random_piece(pcaptured,
                                        cs); // en passant capture

    nnue_add_piece(board, to, pc);
    nnue_remove_piece(board, from, pc);
    nnue_remove_piece(board, cs, pcaptured);

    return true;
}

static bool handle_castling(board_t *board, move_t move, dstate_t *undo) {
    bindex_t from = move_from(move);
    bindex_t to = move_to(move);
    side_t us = board->side_to_move;
    piece_t pc = board->pieces_at[from];
    piecetype_t pc_type = piece_type(pc);

    bb_t move_bb = (1ULL << from) | (1ULL << to);
    castling_side_t cs = (to > from) ? KINGSIDE : QUEENSIDE;
    uint8_t castling_perm_change_mask = 0;

    castling_perm_change_mask =
        (us ? CASTLING_RIGHTS_BBOTH : CASTLING_RIGHTS_WBOTH);

    bindex_t rsq_from = ROOK_CASTLING_POS[us][cs][0];
    bindex_t rsq_to = ROOK_CASTLING_POS[us][cs][1];
    piece_t rook = make_piece(PIECETYPE_ROOK, us);

    bb_t rook_move_bb = (1ULL << rsq_from) | (1ULL << rsq_to);

    // move king to ending location
    board->pieces_at[to] = pc;
    board->pieces_at[from] = PIECE_NONE;
    board->pieces_at[rsq_to] = rook;
    board->pieces_at[rsq_from] = PIECE_NONE;

    board->pieces_occ[pc_type] ^= move_bb;
    board->pieces_occ[PIECETYPE_ROOK] ^= rook_move_bb;
    board->sides_occ[us] ^= move_bb | rook_move_bb;

    board->st.key ^= fetch_random_piece(pc, from);
    board->st.key ^= fetch_random_piece(pc, to);
    board->st.key ^= fetch_random_piece(rook, rsq_from);
    board->st.key ^= fetch_random_piece(rook, rsq_to);

    board->st.key ^= castling_changes_key(castling_perm_change_mask &
                                          board->st.castling_rights);
    board->st.castling_rights &= ~castling_perm_change_mask;

    nnue_remove_piece(board, from, pc);
    nnue_add_piece(board, to, pc);
    nnue_remove_piece(board, rsq_from, rook);
    nnue_add_piece(board, rsq_to, rook);

    return false;
}

static bool handle_promotion(board_t *board, move_t move, dstate_t *undo) {
    board->st.halfmove_clock = 0;

    bindex_t from = move_from(move);
    bindex_t to = move_to(move);
    side_t us = board->side_to_move;
    side_t them = !us;
    piece_t pc = board->pieces_at[from];
    piece_t pcaptured = board->pieces_at[to];
    piecetype_t promo = move_promo(move);
    piece_t promo_piece = make_piece(promo, us);
    piecetype_t pc_type = piece_type(pc);

    bb_t move_bb = (1ULL << from) | (1ULL << to);

    board->pieces_at[to] = make_piece(promo, us);
    board->pieces_at[from] = PIECE_NONE;

    board->pieces_occ[pc_type] ^= (1ULL << from);
    board->pieces_occ[promo] ^= (1ULL << to);
    board->sides_occ[us] ^= move_bb;

    bool cap = (pcaptured != PIECE_NONE);

    board->st.key ^= fetch_random_piece(pc, from);
    board->st.key ^= fetch_random_piece(promo_piece, to);

    if (cap) {
        board->pieces_occ[piece_type(pcaptured)] ^= (1ULL << to);
        board->sides_occ[them] ^= (1ULL << to);
        undo->captured = pcaptured;
        board->st.key ^= fetch_random_piece(pcaptured, to);

        board->st.key ^= castling_changes_key(board->st.castling_rights &
                                              CASTLE_CAPTURE_MASK[us][to]);
        board->st.castling_rights &= ~CASTLE_CAPTURE_MASK[us][to];

        nnue_remove_piece(board, to, pcaptured);
    }

    nnue_add_piece(board, to, promo_piece);
    nnue_remove_piece(board, from, pc);

    return cap;
}

static move_fn move_handlers[4] = {handle_normal, handle_promotion, handle_ep,
                                   handle_castling};
// assume the move is legal
bool perform_move(board_t *board, move_t move, dstate_t *undo) {

    undo->move = move;
    undo->captured = PIECE_NONE;
    undo->prev_state = board->st;

    // increment previous position into history
    board->key_hist[board->move_number++] = board->st.key;

    // stupid polyglot thing
    if (board->st.ep_was_possible == 1) {
        board->st.key ^=
            RANDOM_64[RANDOM_ENPASSANT + SQUARE_TO_FILE[board->st.ep_square]];
    }
    board->st.ep_was_possible = 0;

    board->st.ep_square = 64;
    side_t us = board->side_to_move;
    side_t them = !us;

    movetype_t mt = move_type(move);

    board->st.halfmove_clock++;

    if (us == SIDE_BLACK)
        board->st.fullmove_clock++;

    move_handlers[mt >> 14](board, move, undo);

    update_pinners_and_blockers(board, SIDE_WHITE);
    update_pinners_and_blockers(board, SIDE_BLACK);

    board->side_to_move = them;
    board->st.key ^= RANDOM_64[RANDOM_TURN];

    return !in_check(board, us);
}

typedef void (*undo_fn)(board_t *board, dstate_t *undo);

static void undo_normal(board_t *board, dstate_t *undo) {

    side_t them = board->side_to_move; // next move will have the opponent
                                       // as side_to_move
    side_t us = !them;
    move_t move = undo->move;
    bindex_t from = move_from(move);
    bindex_t to = move_to(move);

    piece_t pc = board->pieces_at[to];
    piecetype_t pc_type = piece_type(pc);
    piece_t captured = undo->captured;
    bool cap = (captured != PIECE_NONE);

    bb_t move_bb = (1ULL << from) | (1ULL << to);

    board->pieces_at[from] = pc;
    board->pieces_at[to] = captured;
    board->pieces_occ[pc_type] ^= move_bb;
    board->sides_occ[us] ^= move_bb;

    if (cap) {
        piecetype_t captured_type = piece_type(captured);
        board->pieces_occ[captured_type] ^= (1ULL << to);
        board->sides_occ[them] ^= (1ULL << to);

        nnue_add_piece(board, to, captured);
    }

    nnue_add_piece(board, from, pc);
    nnue_remove_piece(board, to, pc);
}

static void undo_promotion(board_t *board, dstate_t *undo) {

    side_t them = board->side_to_move; // next move will have the opponent
                                       // as side_to_move
    side_t us = !them;
    move_t move = undo->move;
    bindex_t from = move_from(move);
    bindex_t to = move_to(move);
    piece_t pc = board->pieces_at[to];
    piece_t pawn = make_piece(PIECETYPE_PAWN, us);

    piece_t captured = undo->captured;
    bool cap = (captured != PIECE_NONE);

    bb_t move_bb = (1ULL << from) | (1ULL << to);

    board->pieces_at[from] = pawn;
    board->pieces_at[to] = captured;
    board->pieces_occ[PIECETYPE_PAWN] ^= (1ULL << from);
    board->pieces_occ[move_promo(move)] ^= (1ULL << to);
    board->sides_occ[us] ^= move_bb;

    if (cap) {
        nnue_add_piece(board, to, captured);
        piecetype_t captured_type = piece_type(captured);
        board->pieces_occ[captured_type] ^= (1ULL << to);
        board->sides_occ[them] ^= (1ULL << to);
    }

    nnue_add_piece(board, from, pawn);
    nnue_remove_piece(board, to, pc);
}

static void undo_ep(board_t *board, dstate_t *undo) {

    side_t them = board->side_to_move; // next move will have the opponent
                                       // as side_to_move
    side_t us = !them;

    piece_t *pieces_at = board->pieces_at;

    move_t move = undo->move;
    bindex_t from = move_from(move);
    bindex_t to = move_to(move);

    piece_t pc = pieces_at[to];
    piecetype_t pc_type = piece_type(pc);
    piece_t captured = make_piece(PIECETYPE_PAWN, them);

    bb_t move_bb = (1ULL << from) | (1ULL << to);

    pieces_at[from] = pc;
    pieces_at[to] = PIECE_NONE; // guaranteed no capture
    board->pieces_occ[pc_type] ^= move_bb;
    board->sides_occ[us] ^= move_bb;

    bindex_t cs = to + PUSH_DIR[them];
    pieces_at[cs] = captured;
    board->pieces_occ[PIECETYPE_PAWN] ^= (1ULL << cs);
    board->sides_occ[them] ^= (1ULL << cs);

    nnue_remove_piece(board, to, pc);
    nnue_add_piece(board, from, pc);
    nnue_add_piece(board, cs, captured);
}

static void undo_castling(board_t *board, dstate_t *undo) {
    side_t them = board->side_to_move; // next move will have the opponent
                                       // as side_to_move
    side_t us = !them;

    piece_t *pieces_at = board->pieces_at;

    move_t move = undo->move;
    bindex_t from = move_from(move);
    bindex_t to = move_to(move);

    piece_t pc = pieces_at[to];
    piecetype_t pc_type = piece_type(pc);
    piece_t captured = undo->captured;

    bb_t move_bb = (1ULL << from) | (1ULL << to);

    pieces_at[from] = pc;
    pieces_at[to] = captured;
    board->pieces_occ[pc_type] ^= move_bb;
    board->sides_occ[us] ^= move_bb;

    // reconstruct rook_move_bb
    castling_side_t cs = (to > from) ? KINGSIDE : QUEENSIDE;

    bindex_t rsq_from = ROOK_CASTLING_POS[us][cs][0];
    bindex_t rsq_to = ROOK_CASTLING_POS[us][cs][1];
    piece_t rook = make_piece(PIECETYPE_ROOK, us);

    bb_t rook_move_bb = (1ULL << rsq_from) | (1ULL << rsq_to);

    // move rook back
    pieces_at[rsq_from] = make_piece(PIECETYPE_ROOK, us);
    pieces_at[rsq_to] = PIECE_NONE;
    board->pieces_occ[PIECETYPE_ROOK] ^= rook_move_bb;
    board->sides_occ[us] ^= rook_move_bb;

    nnue_add_piece(board, from, pc);
    nnue_remove_piece(board, to, pc);
    nnue_add_piece(board, rsq_from, rook);
    nnue_remove_piece(board, rsq_to, rook);
}

static undo_fn undo_handlers[4] = {undo_normal, undo_promotion, undo_ep,
                                   undo_castling};

void undo_move(board_t *board, dstate_t *undo) {

    board->st = undo->prev_state;
    move_t move = undo->move;
    movetype_t mt = move_type(move);
    undo_handlers[mt >> 14](board, undo);

    board->move_number--;
    board->key_hist[board->move_number] = 0;
    board->side_to_move = !board->side_to_move;
}

bool in_check(board_t *board, side_t side) {
    bb_t king = board->pieces_occ[PIECETYPE_KING] & board->sides_occ[side];

    bindex_t king_sq = __builtin_ctzll(king);
    return is_attacked(board, side, king_sq);
}

bool sufficient_material(board_t *board) {

    bb_t white_bishops =
        board->pieces_occ[PIECETYPE_BISHOP] & board->sides_occ[SIDE_WHITE];
    bb_t black_bishops =
        board->pieces_occ[PIECETYPE_BISHOP] & board->sides_occ[SIDE_BLACK];

    bool white_bishop_pair = (white_bishops & 0x55aa55aa55aa55aaULL) &&
                             (white_bishops & 0xaa55aa55aa55aa55ULL);
    bool black_bishop_pair = (black_bishops & 0x55aa55aa55aa55aaULL) &&
                             (black_bishops & 0xaa55aa55aa55aa55ULL);

    bool white_knight_mate =
        __builtin_popcountll(board->pieces_occ[PIECETYPE_KNIGHT] &
                             board->sides_occ[SIDE_WHITE]) >= 3;
    bool black_knight_mate =
        __builtin_popcountll(board->pieces_occ[PIECETYPE_KNIGHT] &
                             board->sides_occ[SIDE_BLACK]) >= 3;

    bool white_bishop_knight_mate =
        (board->pieces_occ[PIECETYPE_KNIGHT] & board->sides_occ[SIDE_WHITE]) &&
        (board->pieces_occ[PIECETYPE_BISHOP] & board->sides_occ[SIDE_WHITE]);
    bool black_bishop_knight_mate =
        (board->pieces_occ[PIECETYPE_KNIGHT] & board->sides_occ[SIDE_BLACK]) &&
        (board->pieces_occ[PIECETYPE_BISHOP] & board->sides_occ[SIDE_BLACK]);

    return board->pieces_occ[PIECETYPE_QUEEN] ||
           board->pieces_occ[PIECETYPE_ROOK] ||
           board->pieces_occ[PIECETYPE_PAWN] || white_bishop_pair ||
           black_bishop_pair || white_knight_mate || black_knight_mate ||
           white_bishop_knight_mate || black_bishop_knight_mate;
}

bool is_draw(board_t *board) {

    // if (!sufficient_material(board))
    //     return true;

    if (board->st.halfmove_clock >= 100)
        return true;

    int repetitions = 0;

    int start = board->move_number - board->st.halfmove_clock;

    if (start < 0)
        start = 0;

    for (int i = board->move_number - 2; i >= start; i -= 2) {

        if (board->st.key == board->key_hist[i]) {
            repetitions++;
        }
    }

    return repetitions >= 2;
}

void perform_null_move(board_t *board, dstate_t *undo) {

    undo->move = 0;
    undo->captured = PIECE_NONE;
    undo->prev_state = board->st;

    // increment previous position into history
    board->key_hist[board->move_number++] = board->st.key;

    // stupid polyglot thing
    if (board->st.ep_was_possible == 1) {
        board->st.key ^=
            RANDOM_64[RANDOM_ENPASSANT + SQUARE_TO_FILE[board->st.ep_square]];
    }
    board->st.ep_was_possible = 0;

    board->st.ep_square = 64;
    side_t us = board->side_to_move;
    side_t them = !us;

    board->st.halfmove_clock++;

    if (us == SIDE_BLACK)
        board->st.fullmove_clock++;

    board->side_to_move = them;
    board->st.key ^= RANDOM_64[RANDOM_TURN];
}

void undo_null_move(board_t *board, dstate_t *undo) {
    board->st = undo->prev_state;

    board->move_number--;
    board->key_hist[board->move_number] = 0;
    board->side_to_move = !board->side_to_move;
}
