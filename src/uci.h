#pragma once

#include <stdbool.h>

#include "types.h"
#include "board.h"
#include "search.h"

bool uci_loop(globalstate_t* gs, board_t* board);


typedef enum e_uci_command {
    COMMAND_UCI,
    COMMAND_POSITION,
    COMMAND_ISREADY,
    COMMAND_GO,
    COMMAND_STOP,
    COMMAND_QUIT,
    COMMAND_DISPLAY,
    COMMAND_UCINEWGAME,
    COMMAND_EVAL,
    COMMAND_NONE,
    COMMAND_MOVES,
} uci_command_t;
