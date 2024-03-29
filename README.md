# Maestro Chess Engine
A chess engine project for learning c++. It has a decent search speed (Average of depth 20 - Blitz 1/5) and a fast move generator.

> [!IMPORTANT]
> This engine uses ideas from different open source chess engines (Stockfish, Ethreal, etc) so it is not fully original
> For example, it uses tuned paramters from Ethreal Chess Engine for dynamic depth reductions and it also uses a NNUE network from Stockfish.

## Features
* Fast move generator (200Mnps on an i7-10750H) (Following Gigantua move generator)
* Move ordering (SEE, MVV-LVA, killer moves, Piece to history, Capture history)
* Negamax with alpha beta pruning
* Principal variation search
* Null move pruning
* Late move reduction (From Ethreal)
* Late move pruning (From Ethreal)
* Futility Pruning
* Reverse futility pruning
* Prob cut pruning
* Dynamic search reduction
* Transposition table with buckets
* Polyglot book
* Stockfish NNUE evaluation (Incremental update)
* Time control based on search stability
* UCI protocol compatible
