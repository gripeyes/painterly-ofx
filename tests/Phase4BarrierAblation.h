#pragma once

#include "core/RegionHierarchy.h"
#include <algorithm>
#include <limits>
#include <numeric>

// Standalone diagnostic only. Never called by OFX or hierarchy construction.
inline pigment::Phase4RegionHierarchy diagnosticBarrierAblation(
    const pigment::Phase4RegionHierarchy &original, float threshold) {
  auto result=original;
  int width=original.bounds.width(),height=original.bounds.height();
  auto strength=static_cast<const pigment::OwnedPlane &>(original.boundaryStrength).view();
  auto cue=[&](int p) {return strength.at(original.bounds.x1+p%width,original.bounds.y1+p/width);};
  for(auto &plate:result.plates) {
    auto ablate=[&](auto &labels,int &count,auto &edgeX,auto &edgeY,
                   auto &retained,auto &removed) {
      auto before=labels;
      std::vector<int> parent(size_t(count),0);std::iota(parent.begin(),parent.end(),0);
      auto root=[&](int p) {while(parent[size_t(p)]!=p) {
        parent[size_t(p)]=parent[size_t(parent[size_t(p)])];p=parent[size_t(p)];
      }return p;};
      for(int p=0;p<width*height;++p)
        for(int q:{p%width+1<width?p+1:-1,p/width+1<height?p+width:-1})
          if(q>=0 && before[size_t(p)]!=before[size_t(q)] && std::max(cue(p),cue(q))<=threshold) {
            int a=root(before[size_t(p)]),b=root(before[size_t(q)]);
            parent[size_t(std::max(a,b))]=std::min(a,b);
          }
      std::vector<int> compact(parent.size(),-1);count=0;
      for(int &label:labels) {int r=root(label);if(compact[size_t(r)]<0)compact[size_t(r)]=count++;label=compact[size_t(r)];}
      std::fill_n(retained.view().data,width*height,0.0f);
      std::fill_n(removed.view().data,width*height,0.0f);
      for(int p=0;p<width*height;++p)
        for(int q:{p%width+1<width?p+1:-1,p/width+1<height?p+width:-1}) if(q>=0) {
          bool originallyRetained=before[size_t(p)]!=before[size_t(q)];
          bool hard=originallyRetained && std::max(cue(p),cue(q))>threshold;
          if(hard) (q==p+1?edgeX:edgeY)[size_t(p)]=std::numeric_limits<float>::infinity();
          auto &display=hard?retained:removed;
          if(originallyRetained) {
            display.view().at(original.bounds.x1+p%width,original.bounds.y1+p/width)=1;
            display.view().at(original.bounds.x1+q%width,original.bounds.y1+q/width)=1;
          }
        }
    };
    ablate(plate.yChunk,plate.yChunkCount,plate.yEdgeX,plate.yEdgeY,plate.yRetainedBoundaries,plate.yRemovedBoundaries);
    ablate(plate.abChunk,plate.abChunkCount,plate.abEdgeX,plate.abEdgeY,plate.abRetainedBoundaries,plate.abRemovedBoundaries);
  }
  return result;
}
