# Maestro Chess Engine
A chess engine project for learning c++

> [!IMPORTANT]
> This engine uses ideas from different open source chess engines (Stockfish, Ethreal, etc) so it is not fully original

## Features
* Fast move generator (200Mnps on an i7-10750H) (Following Gigantua move generator)
* Move ordering (MVV-LVA, killer moves, history heuristics)
* Negamax with alpha beta pruning
* Principal variation search
* Null move pruning
* Late move reduction
* Late move pruning
* Transposition table with buckets
* Polyglot book
* Stockfish NNUE evaluation (Incremental update)
* Time control based on search stability
* UCI protocol compatible
