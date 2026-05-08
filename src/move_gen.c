#include <assert.h>
#include <stdbool.h>

#include "move_gen.h"
#include "types.h"
#include "utils.h"

#include "board.h"

void generate_pawn_moves(board_t *board, side_t side, move_t *moves,
                         size_t *count, bool only_nonquiet) {

    bb_t pawns = board->pieces_occ[PIECETYPE_PAWN] & board->sides_occ[side];
    bb_t blockers = board->sides_occ[side] | board->sides_occ[!side];
    bb_t potential_captures = board->sides_occ[!side];

    bb_t double_push_mask =
        RANK_MASKS[side ? RANK_5 : RANK_4]; // these are the only places where
                                            // double pushes can occur.

    bb_t fpush_bb = side ? (pawns >> 8) & ~blockers : (pawns << 8) & ~blockers;
    bb_t spush_bb = side ? (fpush_bb >> 8) & ~blockers & double_push_mask
                         : (fpush_bb << 8) & ~blockers & double_push_mask;

    // grab the attackers and mask out ones that wrap. i.e. attacking on right
    // should never end up on H file
    bb_t right_attacks = side ? (pawns >> 9 & ~FILE_MASKS[FILE_H])
                              : (pawns << 7 & ~FILE_MASKS[FILE_H]);
    bb_t left_attacks = side ? (pawns >> 7 & ~FILE_MASKS[FILE_A])
                             : (pawns << 9 & ~FILE_MASKS[FILE_A]);

    bb_t ra_captures = right_attacks & potential_captures;
    bb_t la_captures = left_attacks & potential_captures;

    bb_t promo_rank = RANK_MASKS[side ? RANK_1 : RANK_8];

    bb_t promo = fpush_bb & promo_rank;

    fpush_bb &= ~promo;

    bb_t ra_cap_promo = ra_captures & promo_rank;
    ra_captures &= ~ra_cap_promo;
    bb_t la_cap_promo = la_captures & promo_rank;
    la_captures &= ~la_cap_promo;

    // print_bb(ra_captures);
    // print_bb(la_captures);
    // print_bb(fpush_bb);
    // print_bb(RANK_MASKS[RANK_8]);

    // add moves to list
    while (promo) {
        bindex_t end_pos = __builtin_ctzll(promo);
        bindex_t start_pos = end_pos - PUSH_DIR[side];
        promo = (promo & (promo - 1));
        add_promotions(moves, count, start_pos, end_pos);
    }

    while (ra_cap_promo) {
        bindex_t end_pos = __builtin_ctzll(ra_cap_promo);
        bindex_t start_pos = end_pos - PUSH_DIR[side] + 1;
        ra_cap_promo = (ra_cap_promo & (ra_cap_promo - 1));
        add_promotions(moves, count, start_pos, end_pos);
    }

    while (la_cap_promo) {
        bindex_t end_pos = __builtin_ctzll(la_cap_promo);
        bindex_t start_pos = end_pos - PUSH_DIR[side] - 1;
        la_cap_promo = (la_cap_promo & (la_cap_promo - 1));
        add_promotions(moves, count, start_pos, end_pos);
    }

    while (ra_captures) {
        bindex_t end_pos = __builtin_ctzll(ra_captures);
        bindex_t start_pos = end_pos - PUSH_DIR[side] + 1;
        ra_captures = (ra_captures & (ra_captures - 1));
        add_move(moves, count, start_pos, end_pos, NORMAL);
    }

    while (la_captures) {
        bindex_t end_pos = __builtin_ctzll(la_captures);
        bindex_t start_pos = end_pos - PUSH_DIR[side] - 1;
        la_captures = (la_captures & (la_captures - 1));
        add_move(moves, count, start_pos, end_pos, NORMAL);
    }

    if (board->st.ep_square != 64) {
        // see if we can take it
        bb_t ep_target = (1ULL << board->st.ep_square);

        // we are guaranteed there is only one possible left or right capture
        // so we just check if they exist and use that
        if (ep_target & left_attacks) {
            bindex_t end_pos = board->st.ep_square;
            bindex_t start_pos = end_pos - PUSH_DIR[side] - 1;
            add_move(moves, count, start_pos, end_pos, EN_PASSANT);
        }
        if (ep_target & right_attacks) {
            bindex_t end_pos = board->st.ep_square;
            bindex_t start_pos = end_pos - PUSH_DIR[side] + 1;
            add_move(moves, count, start_pos, end_pos, EN_PASSANT);
        }
    }

    if (only_nonquiet) return;

    while (fpush_bb) {
        bindex_t end_pos = __builtin_ctzll(fpush_bb);
        bindex_t start_pos = end_pos - PUSH_DIR[side];
        fpush_bb = (fpush_bb & (fpush_bb - 1));
        add_move(moves, count, start_pos, end_pos, NORMAL);
    }

    while (spush_bb) {
        bindex_t end_pos = __builtin_ctzll(spush_bb);
        bindex_t start_pos = end_pos - PUSH_DIR[side] * 2;
        spush_bb = (spush_bb & (spush_bb - 1));
        add_move(moves, count, start_pos, end_pos, NORMAL);
    }
}

void generate_knight_moves(board_t *board, side_t side, move_t *moves,
                           size_t *count, bool only_nonquiet) {
    bb_t knights = board->pieces_occ[PIECETYPE_KNIGHT] & board->sides_occ[side];
    bb_t our_pieces = board->sides_occ[side];
    bb_t potential_captures = board->sides_occ[!side];

    while (knights != 0) {
        bindex_t start_pos = __builtin_ctzll(knights);
        bb_t move_mask = KNIGHT_MOVES[start_pos];
        insert_mask_moves(moves, count, start_pos, move_mask,
                          potential_captures, our_pieces, only_nonquiet);
        knights = (knights & (knights - 1));
    }
}

void generate_bishop_moves(board_t *board, side_t side, move_t *moves,
                           size_t *count, bool only_nonquiet) {
    bb_t bishops = board->pieces_occ[PIECETYPE_BISHOP] & board->sides_occ[side];
    bb_t our_pieces = board->sides_occ[side];
    bb_t potential_captures = board->sides_occ[!side];
    bb_t blockers = our_pieces | potential_captures;

    while (bishops != 0) {
        bindex_t start_pos = __builtin_ctzll(bishops);
        bb_t move_mask = fetch_bishop_moves(start_pos, blockers);
        insert_mask_moves(moves, count, start_pos, move_mask,
                          potential_captures, our_pieces, only_nonquiet);
        bishops = (bishops & (bishops - 1));
    }
}

void generate_rook_moves(board_t *board, side_t side, move_t *moves,
                         size_t *count, bool only_nonquiet) {
    bb_t rooks = board->pieces_occ[PIECETYPE_ROOK] & board->sides_occ[side];
    bb_t our_pieces = board->sides_occ[side];
    bb_t potential_captures = board->sides_occ[!side];
    bb_t blockers = our_pieces | potential_captures;

    while (rooks != 0) {
        bindex_t start_pos = __builtin_ctzll(rooks);
        bb_t move_mask = fetch_rook_moves(start_pos, blockers);
        insert_mask_moves(moves, count, start_pos, move_mask,
                          potential_captures, our_pieces, only_nonquiet);
        rooks = (rooks & (rooks - 1));
    }
}

void generate_queen_moves(board_t *board, side_t side, move_t *moves,
                          size_t *count, bool only_nonquiet) {
    bb_t queens = board->pieces_occ[PIECETYPE_QUEEN] & board->sides_occ[side];
    bb_t our_pieces = board->sides_occ[side];
    bb_t potential_captures = board->sides_occ[!side];
    bb_t blockers = our_pieces | potential_captures;

    while (queens != 0) {
        bindex_t start_pos = __builtin_ctzll(queens);
        bb_t move_mask = fetch_rook_moves(start_pos, blockers) |
                         fetch_bishop_moves(start_pos, blockers);
        insert_mask_moves(moves, count, start_pos, move_mask,
                          potential_captures, our_pieces, only_nonquiet);
        queens = (queens & (queens - 1));
    }
}

void generate_king_moves(board_t *board, side_t side, move_t *moves,
                         size_t *count, bool only_nonquiet) {
    // assume 1 king
    bb_t king = board->pieces_occ[PIECETYPE_KING] & board->sides_occ[side];
    bb_t our_pieces = board->sides_occ[side];
    bb_t potential_captures = board->sides_occ[!side];

    bindex_t start_pos = __builtin_ctzll(king);
    bb_t move_mask = KING_MOVES[start_pos];
    insert_mask_moves(moves, count, start_pos, move_mask, potential_captures,
                      our_pieces, only_nonquiet);
}

// void insert_mask_moves(move_t *moves, size_t *count, bindex_t from, bb_t
// mask,
//                        bb_t potential_captures, bb_t our_pieces) {
//     bb_t possible_moves = (mask & ~our_pieces);
//     while (possible_moves != 0) {
//         bindex_t end_pos = __builtin_ctzll(possible_moves);
//         possible_moves = (possible_moves & (possible_moves - 1));
//         add_move(moves, count, from, end_pos, NORMAL);
//     }
// }

void generate_pseudolegal_moves(board_t *board, side_t side, move_t *moves,
                                size_t *count, bool only_nonquiet) {

    *count = 0;

    // check if spaees are occupied
    bb_t blockers = board->sides_occ[SIDE_WHITE] | board->sides_occ[SIDE_BLACK];
    bool ks_perm = board->st.castling_rights & (side ? CASTLING_RIGHTS_BKINGSIDE : CASTLING_RIGHTS_WKINGSIDE);
    bool ks_occ = CASTLING_OCC_MASK[side][KINGSIDE] & blockers;
    if (!ks_occ && ks_perm) {

        bool attacked = side ? (is_attacked(board, side, E8) ||
                                is_attacked(board, side, F8) ||
                                is_attacked(board, side, G8))
                             : (is_attacked(board, side, E1) ||
                                is_attacked(board, side, F1) ||
                                is_attacked(board, side, G1));
        if (!attacked) {
            bindex_t start_pos = KING_CASTLING_POS[side][KINGSIDE][0];
            bindex_t end_pos = KING_CASTLING_POS[side][KINGSIDE][1];
            add_move(moves, count, start_pos, end_pos, CASTLING);
        }
    }
    bool qs_occ = CASTLING_OCC_MASK[side][QUEENSIDE] & blockers;
    bool qs_perm = board->st.castling_rights & (side ? CASTLING_RIGHTS_BQUEENSIDE : CASTLING_RIGHTS_WQUEENSIDE);
    if (!qs_occ && qs_perm) {

        // replace with is attacked

        bool attacked = side ? (is_attacked(board, side, E8) ||
                                is_attacked(board, side, D8) ||
                                is_attacked(board, side, C8))
                             : (is_attacked(board, side, E1) ||
                                is_attacked(board, side, D1) ||
                                is_attacked(board, side, C1));

        if (!attacked) {
            bindex_t start_pos = KING_CASTLING_POS[side][QUEENSIDE][0];
            bindex_t end_pos = KING_CASTLING_POS[side][QUEENSIDE][1];
            add_move(moves, count, start_pos, end_pos, CASTLING);
        }
    }

    generate_king_moves(board, side, moves, count, only_nonquiet);
    generate_queen_moves(board, side, moves, count, only_nonquiet);
    generate_bishop_moves(board, side, moves, count, only_nonquiet);
    generate_knight_moves(board, side, moves, count, only_nonquiet);
    generate_rook_moves(board, side, moves, count, only_nonquiet);
    generate_pawn_moves(board, side, moves, count, only_nonquiet);
}

// generate attacks FROM side
bb_t generate_attack_mask(board_t *board, side_t side) {
    bb_t blockers = board->sides_occ[side] | board->sides_occ[!side];
    bb_t attack_mask = 0;

    // pawn attacks
    bb_t pawns = board->pieces_occ[PIECETYPE_PAWN] & board->sides_occ[side];

    bb_t right_attacks = side ? (pawns >> 9 & ~FILE_MASKS[FILE_H])
                              : (pawns << 7 & ~FILE_MASKS[FILE_H]);
    bb_t left_attacks = side ? (pawns >> 7 & ~FILE_MASKS[FILE_A])
                             : (pawns << 9 & ~FILE_MASKS[FILE_A]);
    attack_mask |= left_attacks | right_attacks;

    bb_t bishops = board->pieces_occ[PIECETYPE_BISHOP] & board->sides_occ[side];
    while (bishops) {
        bindex_t sq = __builtin_ctzll(bishops);
        bb_t moves = fetch_bishop_moves(sq, blockers);
        bishops = bishops & (bishops - 1);
        attack_mask |= moves;
    }

    bb_t rooks = board->pieces_occ[PIECETYPE_ROOK] & board->sides_occ[side];
    while (rooks) {
        bindex_t sq = __builtin_ctzll(rooks);
        bb_t moves = fetch_rook_moves(sq, blockers);
        rooks = rooks & (rooks - 1);
        attack_mask |= moves;
    }

    bb_t queens = board->pieces_occ[PIECETYPE_QUEEN] & board->sides_occ[side];
    while (queens) {
        bindex_t sq = __builtin_ctzll(queens);
        bb_t moves =
            fetch_rook_moves(sq, blockers) | fetch_bishop_moves(sq, blockers);
        queens = queens & (queens - 1);
        attack_mask |= moves;
    }

    bb_t knights = board->pieces_occ[PIECETYPE_KNIGHT] & board->sides_occ[side];
    while (knights) {
        bindex_t sq = __builtin_ctzll(knights);
        bb_t moves = KNIGHT_MOVES[sq];
        knights = knights & (knights - 1);
        attack_mask |= moves;
    }

    bb_t king = board->pieces_occ[PIECETYPE_KING] & board->sides_occ[side];
    bindex_t king_sq = __builtin_ctzll(king);
    attack_mask |= KING_MOVES[king_sq];

    return attack_mask;
}
