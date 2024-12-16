# Maestro Chess Engine
A chess engine project for learning c++. It has a decent search speed (Average of depth 20 - Blitz 1/5) and a fast move generator.

> [!IMPORTANT]
> * This engine uses ideas from different open source chess engines (Stockfish, Ethreal, etc) so it is not fully original
> * Credits to Maksim Korzh and BluefeverSoft for their amazing videos on chess engines.
> * Uses a NNUE network from Stockfish.
> * Credits to Daniel Shawul for his NNUE probe library.
> * Credits to JA for his contributions to embedded NNUE nets

## Demo
![alt text](https://github.com/0mn1verze/Maestro-Chess-Engine/blob/942e85bddfd8f62b51647bf0183ab3c3778e2357/demo.png)
[Website](https://0mn1verze.pythonanywhere.com/)

## Performance
* Rough estimate of 2800 elo

## Features
### Move generation
* Magic Bitboards ([wiki](https://www.chessprogramming.org/Magic_Bitboards))
  * PEXT intrinsics (Optional) ([wiki](https://www.chessprogramming.org/BMI2#PEXTBitboards))
  * Fancy Magic Bitboards ([wiki](https://www.chessprogramming.org/Magic_Bitboards))
* Fully Legal and Fast Move Generator
  * Keep tracks of pins and checks etc
  * Generation speeds up to 400Mnps on a i7-10750H with Turbo boost
### Move ordering
* Capture History Table
* Killer Move Heuristics
* History Table ([wiki](https://www.chessprogramming.org/History_Heuristic))
* Continuation History Table
* MVV-LVA ([wiki](https://www.chessprogramming.org/MVV-LVA))
* Staged Move Generation
* Static Exchange Evaluation ([wiki](https://www.chessprogramming.org/Static_Exchange_Evaluation))
### Search
* Iterative Deepening ([wiki](https://www.chessprogramming.org/Iterative_Deepening))
* Classic Alpha Beta Search ([wiki](https://www.chessprogramming.org/Alpha-Beta))
  * Negamax
* Quiescence Search ([wiki](https://www.chessprogramming.org/Quiescence_Search))
* Transposition Table ([wiki](https://www.chessprogramming.org/Transposition_Table))
  * Dynamic allocation
  * Buckets
* Futility Pruning ([wiki](https://www.chessprogramming.org/Futility_Pruning))
* Reverse Futility Pruning
* Null Move Pruning ([wiki](https://www.chessprogramming.org/Null_Move_Pruning))
* Internal Iterative Deepening/Reductions ([wiki](https://www.chessprogramming.org/Internal_Iterative_Deepening))
* Prob Cut Pruning ([wiki](https://www.chessprogramming.org/ProbCut))
* Razoring ([wiki](https://www.chessprogramming.org/Razoring))
* Late Move Reductions ([wiki](https://www.chessprogramming.org/Late_Move_Reductions))
* Static Exchange Evaluation Pruning 
* Singular Extension Search ([wiki](https://www.chessprogramming.org/Singular_Extensions))
* Multicut Pruning
* Late Move Pruning 
* Principal variation search ([wiki](https://www.chessprogramming.org/Principal_Variation_Search))
### Evaluation
* PeSTO tables ([wiki](https://www.chessprogramming.org/PeSTO))
