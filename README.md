# Maestro Chess Engine
A chess engine project for learning c++. It has a decent search speed (Average of depth 20 - Blitz 1/5) and a fast move generator.

> [!IMPORTANT]
> * This engine uses ideas from different open source chess engines (Stockfish, Ethreal, etc) so it is not fully original
> * For example, it uses tuned paramters from Ethreal Chess Engine for dynamic depth reductions and it also uses a NNUE network from Stockfish.
> * Credits to Daniel Shawul for his NNUE probe library.
> * Credits to Maksim Korzh and BluefeverSoft for their amazing videos on chess engines.
> * Credits to JA for his contributions to the engine (Adding support for embedded net).

## Demo
![alt text](https://github.com/0mn1verze/Maestro-Chess-Engine/blob/942e85bddfd8f62b51647bf0183ab3c3778e2357/demo.png)
[Website](https://0mn1verze.pythonanywhere.com/)

## Performance
* Rough estimate of 2900 - 3000 elo

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
* Daniel Shawul's NNUE probe library
* Time control based on search stability
* UCI protocol compatible
