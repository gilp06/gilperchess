#include "nnue.h"

// load from bin
INCBIN(nnue_bin, "../nnue/lichess-elite-wdl-v0.bin");

// copy into here
const nnue_t *NNUE = (const nnue_t*)gnnue_binData;

