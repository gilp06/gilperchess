
#include "board.h"
#include "hashtable.h"
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

    bool done = false;
    while (!done) {
        done = uci_loop(&b);
    }
}
