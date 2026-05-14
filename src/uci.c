
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "hashtable.h"
#include "move_gen.h"
#include "perft.h"
#include "search.h"
#include "types.h"
#include "uci.h"

pthread_t main_search_thread;
gosearchdata_t search_data;

// returns 0 if wrong type, 1 if successful, -1 if invalid input
static int set_param(const char *cmp_str, char *tok, char **last,
                     uint64_t *val) {
    uint64_t v = 0;
    if (strcmp(tok, cmp_str) == 0) {
        // read next for value
        tok = strtok_r(NULL, " \n", last);
        if (tok == NULL) {
            return -1;
        }
        v = atoi(tok);
        if (v <= 0) {
            return -2;
        }
        *val = v;
    } else {
        return -3;
    }

    return 0;
}

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
        } else if (strcmp(tok, "stop") == 0) {
            command = COMMAND_STOP;
        } else if (strcmp(tok, "eval") == 0) {
            command = COMMAND_EVAL;
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
        case COMMAND_EVAL: {
            printf("eval: %d\n", evaluate(board));
            board->side_to_move = !board->side_to_move;
            printf("eval opp: %d\n", evaluate(board));
            board->side_to_move = !board->side_to_move;
            printf("white accums: ");
            for (int i = 0; i < 64; i++) {
                printf("%d ", board->white_accum.values[i]);
            }
            printf("\nblack accums: ");
            for (int i = 0; i < 64; i++) {
                printf("%d ", board->black_accum.values[i]);
            }
            printf("\n");
            fflush(stdout);
            return false;        
        }
        case COMMAND_UCINEWGAME: {
            clear_ttable(&gs->transposition_table);
            return false;
        }
        case COMMAND_GO: {
            if (ABORT_SIGNAL == 0) return false;

            searchparams_t cur_search = infinite_search;
            uint64_t perft_depth = 1;
            int res;

            while ((tok = strtok_r(NULL, " \n",
                                   &last))) // keep fetching while valid
            {
                if (strcmp(tok, "infinite") == 0) {
                    cur_search = infinite_search;
                    break;
                }

                if (set_param("depth", tok, &last, &cur_search.depth) == 0)
                    continue;

                if (set_param("wtime", tok, &last, &cur_search.wtime) == 0)
                    continue;

                if (set_param("btime", tok, &last, &cur_search.btime) == 0)
                    continue;

                if (set_param("winc", tok, &last, &cur_search.winc) == 0)
                    continue;

                if (set_param("binc", tok, &last, &cur_search.binc) == 0)
                    continue;

                if (set_param("nodes", tok, &last, &cur_search.nodes) == 0)
                    continue;

                if (set_param("movetime", tok, &last, &cur_search.movetime) ==
                    0)
                    continue;

                if(set_param("movestogo", tok, &last, &cur_search.movestogo) == 0)
                    continue;
                
                if ((res = set_param("perft", tok, &last, &perft_depth)) == 0 || res == -1) {
                    perft_top(board, perft_depth);
                    return false;
                }
                printf("failed to read token correctly, stopping\n");
                fflush(stdout);
                return false;
            }

            // start search here and enter search loop
            search_data.search_settings = cur_search;
            search_data.starting_board = board;
            search_data.gs = gs;

            pthread_create(&main_search_thread, NULL, &go_search, (void*)&search_data);
            pthread_detach(main_search_thread);
            
            return false;
        }

        case COMMAND_STOP: {
            ABORT_SIGNAL = 1;
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
                size_t quiet_count = 0, loud_count = 0;
                generate_pseudolegal_moves(board, board->side_to_move, moves,
                                           &quiet_count, false);
                generate_pseudolegal_moves(board, board->side_to_move, moves+quiet_count,
                                           &loud_count, true);
                bool found = false;

                for (int i = 0; i < quiet_count + loud_count; i++) {
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
                printf("halfmove: %d, fullmove: %d\n", board->st.halfmove_clock,
                       board->st.fullmove_clock);
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
