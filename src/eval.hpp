#ifndef EVAL_HPP
#pragma once
#define EVAL_HPP

#include "defs.hpp"
#include "position.hpp"

namespace Maestro {

namespace Eval {

// Piece scores
constexpr Score pawnScore = _S(82, 94);
constexpr Score knightScore = _S(337, 281);
constexpr Score bishopScore = _S(365, 297);
constexpr Score rookScore = _S(477, 512);
constexpr Score queenScore = _S(1025, 936);

constexpr Value pawnValue = pawnScore.first;
constexpr Value knightValue = knightScore.first;
constexpr Value bishopValue = bishopScore.first;
constexpr Value rookValue = rookScore.first;
constexpr Value queenValue = queenScore.first;

constexpr Score pieceBonus[PIECE_TYPE_N] = {SCORE_ZERO,  pawnScore, knightScore,
                                            bishopScore, rookScore, queenScore,
                                            SCORE_ZERO,  SCORE_ZERO};

constexpr Value pieceValue[PIECE_N] = {VAL_ZERO,    pawnValue, knightValue,
                                       bishopValue, rookValue, queenValue,
                                       VAL_ZERO};

constexpr int gamePhaseInc[PIECE_TYPE_N] = {0, 0, 1, 1, 2, 4, 0, 0};

extern Score psqt[PIECE_N][SQ_N];

void initEval();

Value evaluate(const Position &pos);

} // namespace Eval

} // namespace Maestro

#endif // EVAL_HPP