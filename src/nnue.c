#include "nnue.h"


// load from bin
INCBIN(nnue_bin, "../nnue/1024-8b-400.bin");

// copy into here
const nnue_t *NNUE = (const nnue_t*)gnnue_binData;

