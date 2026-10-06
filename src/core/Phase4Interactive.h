#pragma once
#include "core/PigmentPhase4.h"

namespace pigment {
inline int phase4PreviewAnalysisSize(int quality) {
  return quality<=0?64:(quality==1?128:256);
}
// Explicit preview execution class. Never used by the Full Reference entry.
struct Phase4GuidedSample {
  std::array<int,4> index{};
  std::array<float,4> weight{};
};
struct Phase4InteractiveCache {
  Phase4ResearchCache stages;
  std::vector<double> sourceKey;
  std::vector<float> source,result;
  std::vector<Phase4GuidedSample> guidance;
  RectI analysisBounds{};
  size_t guidanceBuilds=0;
};
Phase4RenderDiagnostics processPhase4Interactive(const Phase4RenderInputs& in,
    Phase4InteractiveCache& cache,int analysisLongEdge,
    const ExecutionContext& execution = {});
}
