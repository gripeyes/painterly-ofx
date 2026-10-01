#pragma once

#include <functional>
#include <algorithm>
#include <future>
#include <thread>
#include <vector>

namespace pigment {

class ScratchArena;

using CancelCheck = std::function<bool()>;
using RowFunction = std::function<void(int, int)>;
using ParallelRows = std::function<void(int, int, const RowFunction&)>;

inline void serialRows(int begin, int end, const RowFunction& fn) {
  fn(begin, end);
}

// Independent rows/jobs only. Fixed contiguous partitions and no parallel
// reductions: every job keeps its CPU-reference arithmetic ordering.
inline void boundedParallelRows(int begin,int end,const RowFunction& fn) {
  const int jobs=std::min({4,std::max(1,int(std::thread::hardware_concurrency())),end-begin});
  if(jobs<=1){fn(begin,end);return;}
  std::vector<std::future<void>> pending;
  for(int i=0;i<jobs;++i){int lo=begin+(end-begin)*i/jobs,hi=begin+(end-begin)*(i+1)/jobs;
    pending.push_back(std::async(std::launch::async,[&,lo,hi]{fn(lo,hi);}));}
  // Future destruction joins even if one task throws; no partial cache is
  // installed by the caller on cancellation/failure.
  for(auto& task:pending)task.get();
}

struct ExecutionContext {
  CancelCheck cancelled = [] { return false; };
  ParallelRows parallelRows = serialRows;
  ScratchArena* scratch = nullptr;
};

}  // namespace pigment
