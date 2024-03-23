#ifndef EVAL_HPP
#define EVAL_HPP

#include "defs.hpp"
#include "position.hpp"

Value eval(const Position &pos);

int toNNUEPiece(Piece piece);

#endif // EVAL_HPP