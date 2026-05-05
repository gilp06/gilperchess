#include "types.h"
#include <stdint.h>

// piecetype_t piece_type(piece_t piece) {
//     return __builtin_ctzll(piece & PIECE_TYPE_MASK) - 2;
// }

// side_t piece_side(piece_t piece) {
//     return (piece & PIECE_COLOR_MASK) != PIECE_WHITE_MASK;
// }

// piece_t make_piece(piecetype_t type, side_t side) {
//     return (4ULL << (type)) | (1ULL << side); 
// }


// const uint8_t CASTLING_RIGHTS_WKINGSIDE = 1ULL;
// const uint8_t CASTLING_RIGHTS_WQUEENSIDE = 2ULL;
// const uint8_t CASTLING_RIGHTS_WBOTH = 3ULL;
// const uint8_t CASTLING_RIGHTS_BKINGSIDE = 4ULL;
// const uint8_t CASTLING_RIGHTS_BQUEENSIDE = 8ULL;
// const uint8_t CASTLING_RIGHTS_BBOTH = 12ULL;
