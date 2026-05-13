#include "board.h"
#include "hashtable.h"
#include "search.h"
#include "types.h"
#include "uci.h"
#include "utils.h"
#include "nnue.h"

#include <time.h>

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (!load_nnue("nnue/quantised.bin")) {
        printf("failed to load NNUE!\n");
        exit(1);
    }

    // for (int i = 0; i < 16; i++) {
    //     printf("HL biases: %d\n", NNUE.feature_biases.values[i]);
    // }
    // printf("output bias: %d\n", NNUE.output_bias);

    init_pext_table();

    srand(time(NULL));

    board_t b;
    init_board_from_start(&b);

    globalstate_t gs;
    gs.transposition_table = init_transposition_table(1ULL << 25);
    clear_ttable(&gs.transposition_table);
    bool done = false;
    while (!done) {
        done = uci_loop(&gs, &b);
    }

    exit(0);
}
