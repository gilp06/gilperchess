
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "hashtable.h"
#include "move_gen.h"
#include "perft.h"
#include "search.h"
#include "types.h"
#include "uci.h"

bool uci_loop(globalstate_t *gs, board_t *board) {
    char input[10000];
    fgets(input, 10000, stdin);

    char *last, *tok;

    tok = strtok_r(input, " \n", &last);
    if (tok != NULL) {
        uci_command_t command = COMMAND_NONE;
        if (strcmp(tok, "uci") == 0) {
            command = COMMAND_UCI;
        } else if (strcmp(tok, "isready") == 0) {
            command = COMMAND_ISREADY;
        } else if (strcmp(tok, "go") == 0) {
            command = COMMAND_GO;
        } else if (strcmp(tok, "position") == 0) {
            command = COMMAND_POSITION;
        } else if (strcmp(tok, "display") == 0) {
            command = COMMAND_DISPLAY;
        } else if (strcmp(tok, "quit") == 0) {
            command = COMMAND_QUIT;
        } else if (strcmp(tok, "ucinewgame") == 0) {
            command = COMMAND_UCINEWGAME;
        }

        switch (command) {
        case COMMAND_UCI: {
            printf("id name gilperchess 1.0\n");
            fflush(stdout);
            printf("id author giltong\n");
            fflush(stdout);
            printf("uciok\n");
            fflush(stdout);
            return false;
        }
        case COMMAND_ISREADY: {
            printf("readyok\n");
            fflush(stdout);
            return false;
        }
        case COMMAND_UCINEWGAME: {
            clear_ttable(&gs->transposition_table);
            return false;        
        }
        case COMMAND_GO: {
            tok = strtok_r(NULL, " \n", &last);
            if (tok != NULL && strcmp(tok, "perft") == 0) {
                tok = strtok_r(NULL, " \n", &last);
                int depth = atoi(tok);
                if (depth > 0) {
                    perft_top(board, depth);
                } else {
                    printf("invalid depth argument!\n");
                    fflush(stdout);
                    return false;
                }
            }
            else
            {
                start_search(gs, board, 7);
            }
            return false;
        }
        case COMMAND_POSITION: {
            tok = strtok_r(NULL, " \n", &last);
            if (tok == NULL) {
                printf("invalid arguments\n");
                fflush(stdout);
                return false;
            }
            if (strcmp(tok, "startpos") == 0) {
                init_board_from_start(board);
            } else if (strcmp(tok, "fen") == 0) {
                char *segments[6];
                char fen[256];
                for (int i = 0; i < 6; i++) {
                    segments[i] = strtok_r(NULL, " \n", &last);
                    if (!segments[i]) {
                        printf("Invalid FEN segments!\n");
                        fflush(stdout);
                        return false; // retry
                    }
                }
                snprintf(fen, sizeof(fen), "%s %s %s %s %s %s", segments[0],
                         segments[1], segments[2], segments[3], segments[4],
                         segments[5]);
                init_board_from_fen(board, fen);
            }

            tok = strtok_r(NULL, " \n", &last);
            if (tok == NULL)
                return false;
            if (strcmp(tok, "moves") != 0) {
                printf("invalid argument: %s\n", tok);
                fflush(stdout);
                return false;
            }
            while ((tok = strtok_r(NULL, " \n", &last))) {
                // generate moves
                move_t moves[256];
                size_t count = 0;
                generate_pseudolegal_moves(board, board->side_to_move, moves,
                                           &count);
                bool found = false;

                for (int i = 0; i < count; i++) {
                    // find a move that matches our string
                    char move_str[6];
                    move_to_string(moves[i], move_str);
                    if (strcmp(move_str, tok) != 0)
                        continue;

                    // perform move and check if legal
                    dstate_t undo;
                    bool legal = perform_move(board, moves[i], &undo);
                    if (legal) {
                        found = true;
                        break;
                    } else {
                        undo_move(board, &undo);
                    }
                }
                if (!found) {
                    printf("%s is not a legal move! stopping at this point\n",
                           tok);
                    fflush(stdout);
                    return false;
                }
            }

            break;
        }
        case COMMAND_DISPLAY: {
            tok = strtok_r(NULL, " \n", &last);
            if (tok == NULL) {
                print_pieces(board->pieces_at);
                uint8_t cr = board->st.castling_rights;
                if (cr & CASTLING_RIGHTS_WKINGSIDE)
                    printf("K");
                if (cr & CASTLING_RIGHTS_WQUEENSIDE)
                    printf("Q");
                if (cr & CASTLING_RIGHTS_BKINGSIDE)
                    printf("k");
                if (cr & CASTLING_RIGHTS_BQUEENSIDE)
                    printf("q");
                printf("\n");
                printf("halfmove: %d, fullmove: %d\n", board->st.halfmove_clock, board->st.fullmove_clock);
            }
            return false;
        }
        case COMMAND_QUIT: {
            return true;
            break;
        }
        case COMMAND_NONE: {
            printf("unknown command %s\n", tok);
            fflush(stdout);
            return false;
            break;
        }
        }
    } else {
        return false;
    }
    return false;
}
