#pragma once

#include "types.h"
#include "utils.h"

#include <string.h>
#include <stdint.h>

extern const uint64_t PIECE_TO_HASH_INDEX[16];
extern const uint64_t RANDOM_64[781];
extern const uint64_t RANDOM_CASTLE;
extern const uint64_t RANDOM_ENPASSANT;
extern const uint64_t RANDOM_TURN;

static inline uint64_t fetch_random_piece(piece_t piece, bindex_t sq) {
    return RANDOM_64[PIECE_TO_HASH_INDEX[piece] + sq];
}

static inline uint64_t generate_key_from_scratch(board_t *board) {
    uint64_t piece = 0, castle = 0, enpassant = 0, turn = 0;

    for (bindex_t i = 0; i < 64; i++) {
        if (board->pieces_at[i] == PIECE_NONE)
            continue;
        piece ^= fetch_random_piece(board->pieces_at[i], i);
    }

    if (board->st.castling_rights & CASTLING_RIGHTS_WKINGSIDE)
        castle ^= RANDOM_64[0 + RANDOM_CASTLE];
    if (board->st.castling_rights & CASTLING_RIGHTS_WQUEENSIDE)
        castle ^= RANDOM_64[1 + RANDOM_CASTLE];
    if (board->st.castling_rights & CASTLING_RIGHTS_BKINGSIDE)
        castle ^= RANDOM_64[2 + RANDOM_CASTLE];
    if (board->st.castling_rights & CASTLING_RIGHTS_BQUEENSIDE)
        castle ^= RANDOM_64[3 + RANDOM_CASTLE];

    if (board->st.ep_square != 64 &&
        PAWN_ATTACKS[board->side_to_move][board->st.ep_square] &
            (board->pieces_occ[PIECETYPE_PAWN] &
             board->sides_occ[board->side_to_move])) {
        enpassant =
            RANDOM_64[RANDOM_ENPASSANT + SQUARE_TO_FILE[board->st.ep_square]];
    }

    turn = board->side_to_move ? 0 : RANDOM_64[RANDOM_TURN];
    return piece ^ castle ^ enpassant ^ turn;
}


extern const uint64_t CASTLING_CHANGE_KEYS[16];

static inline uint64_t castling_changes_key(uint8_t castling_changes_mask) {
    return CASTLING_CHANGE_KEYS[castling_changes_mask];
}

typedef enum e_tt_flag {
    INVALID=0,
    BOOK,
    LOWERBOUND,
    UPPERBOUND,
    EXACT
} tt_flag;

typedef struct s_tt_entry {
    uint64_t hash;
    uint16_t bestmove;
    int16_t score;
    uint8_t depth;
    tt_flag flag;
} tt_entry_t;

typedef struct s_ttable {
    tt_entry_t* entries;
    size_t count;
    uint64_t mask;
} ttable_t;

static inline ttable_t init_transposition_table(size_t pow2size) {
    ttable_t table;
    table.entries = malloc(sizeof(tt_entry_t) * pow2size);
    table.count = pow2size;
    table.mask = pow2size-1;
    return table;
}

static inline void free_ttable(ttable_t* table)
{
    free(table->entries);
    table->count = 0;
}

static inline void clear_ttable(ttable_t* table) {
    memset(table->entries, 0, table->count * sizeof(tt_entry_t));
}

static inline tt_entry_t get_tt_entry(ttable_t table, uint64_t hash) {
    return table.entries[hash & table.mask];
}

static inline void write_tt_entry(ttable_t table, tt_entry_t entry) {
    table.entries[entry.hash & table.mask] = entry;
}
