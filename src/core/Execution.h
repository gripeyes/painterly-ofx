#pragma once

#include <functional>

namespace pigment {

using CancelCheck = std::function<bool()>;
using RowFunction = std::function<void(int, int)>;
using ParallelRows = std::function<void(int, int, const RowFunction&)>;

inline void serialRows(int begin, int end, const RowFunction& fn) {
  fn(begin, end);
}

struct ExecutionContext {
  CancelCheck cancelled = [] { return false; };
  ParallelRows parallelRows = serialRows;
};

}  // namespace pigment

