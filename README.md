# gilperchess
gilperchess is my chess engine I've been working on with a couple of goals in mind: having fun writing C code for a larger lower-level project, and making a decently strong chess engine while learning how a lot of the common algorithms and heuristics that interest me work. 

Currently the search is a AB-pruning engine with a few heuristics implemented and SPRTed:
* Transposition Table
* Reverse Futility Pruning
* Null Move Pruning
* Late Move Pruning
* Late Move Reductions
* Quiet History Heuristics
* SEE-based move ordering and pruning
* QS futility pruning
* Killer Moves

A couple other heuristics have been written but I can't seem to get them to play nice when SPRTed, mainly internal iterative reductions and futility pruning in the main search.

The evaluation originally was a piece-square table that was rather weak ([here](https://www.chessprogramming.org/Simplified_Evaluation_Function)), and I was mainly interested in NNUE evaluation as I didn't have much interest in developing a nice evaluation function. Currently, I am using a pretty simple (768->256)x2->1 model trained solely on WDL stats from the Lichess Elite Database. When compared to a network I trained off of Lc0/Stockfish training data it is much weaker of course, however I would like to see how a model starting from this could grow upon rescoring and self-play.

Currently upon testing against Stash and other various bots, I'd say the ELO is currently around 2850, which is pretty poor for a NNUE engine; however, I'm hoping that once the model becomes a bit stronger I can more aggressively prune in the heuristics.
# References
- [bullet](https://github.com/jw1912/bullet) - Training library used for the NNUE. I've placed the current version of the training code based off of the examples given in the repo.
- [incbin](https://github.com/graphitemaster/incbin) - used for embedding the NNUE weights into the engine at compile time.
- [Lichess Elite Database](https://database.nikonoel.fr/) - as mentioned prior, this was used for training data. I combined the 2013-2020 release along with the remaining 2020-2025 individual downloads into a binpack using the [pgn2binpack](https://github.com/Disservin/pgn2binpack) tool which I modified to allow parsing of PGNs without evaluation comments.
- A couple really nice engines that I have referred to when working on the project that were are [Ethereal](github.com/AndyGrant/Ethereal), [Viridithas](https://github.com/cosmobobak/viridithas), and [Stockfish](https://github.com/official-stockfish/Stockfish).
- Chess programming wiki
