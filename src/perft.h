#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#include "board.h"
#include "move_gen.h"
#include "hashtable.h"
#include "types.h"

static inline uint64_t perft(board_t *b, int depth) {
    move_t moves[256];
    size_t move_count = 0;
    uint64_t nodes = 0;

    if (depth == 0) {
        return 1ULL;
    }

    generate_pseudolegal_moves(b, b->side_to_move, moves, &move_count);

    if (depth == 1) {
        for (size_t i = 0; i < move_count; i++) {
            dstate_t undo;
            if (perform_move(b, moves[i], &undo))
                nodes++;
            undo_move(b, &undo);
        }
        return nodes;
    }

    for (int i = 0; i < move_count; i++) {
        dstate_t undo;
        if (perform_move(b, moves[i], &undo)) {
            uint64_t key = b->st.key;
            uint64_t key_regen = generate_key_from_scratch(b);
            
            if(key != key_regen)
            {
                print_pieces(b->pieces_at);
                printf("expected %llx got %llx!\n", key_regen, key);
            }
            nodes += perft(b, depth - 1);
        }
        undo_move(b, &undo);
    }
    return nodes;
}

static inline void perft_top(board_t *b, int depth) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    move_t moves[256];
    size_t move_count = 0;
    uint64_t nodes = 0;
    uint64_t node_total = 0;

    generate_pseudolegal_moves(b, b->side_to_move, moves, &move_count);
    for (int i = 0; i < move_count; i++) {
        dstate_t undo;
        if (perform_move(b, moves[i], &undo)) {

            // grab the key
            nodes = perft(b, depth - 1);
            node_total += nodes;

            char root_move[6];
            move_to_string(moves[i], root_move);
            printf("%s: %llu\n", root_move, nodes);
            fflush(stdout);
        }
        undo_move(b, &undo);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken =
        (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_sec) / 1e9;
    printf("[perft] time (sec): %lf, nodes/sec: %lf\n", time_taken,
           node_total / time_taken);
    printf("[perft] total nodes: %lld\n", node_total);
    fflush(stdout);
}
