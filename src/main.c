#include "board.h"
#include "hashtable.h"
#include "search.h"
#include "types.h"
#include "uci.h"
#include "utils.h"

#include <time.h>

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

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
}
