#ifndef EVAL_HPP
#pragma once
#define EVAL_HPP

#include "defs.hpp"

#include "position.hpp"

namespace Maestro {

namespace Eval {

extern Score psqt[PIECE_N][SQ_N];

void initEval();

Value evaluate(const Position &pos);

} // namespace Eval

} // namespace Maestro

#endif // EVAL_HPP