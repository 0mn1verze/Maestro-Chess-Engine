#ifndef EVAL_HPP
#define EVAL_HPP

#include "defs.hpp"

namespace Maestro {

class Position;

namespace Eval {

Value evaluate(const Position &pos);

} // namespace Eval

} // namespace Maestro

#endif