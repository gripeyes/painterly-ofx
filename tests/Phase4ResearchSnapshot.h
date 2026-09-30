#pragma once
#include "core/LatentPlateGraph.h"
#include "core/RegionHierarchy.h"
#include <cstring>
#include <fstream>
#include <filesystem>

namespace research {
template<class T>void io(std::ostream &f,const T &v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));}
template<class T>void io(std::istream &f,T &v){f.read(reinterpret_cast<char*>(&v),sizeof(v));if(!f)throw std::runtime_error("Truncated research snapshot");}
template<class T>void vec(std::ostream &f,const std::vector<T>&v){uint64_t n=v.size();io(f,n);f.write(reinterpret_cast<const char*>(v.data()),std::streamsize(n*sizeof(T)));}
template<class T>void vec(std::istream &f,std::vector<T>&v){uint64_t n;io(f,n);if(n>100000000)throw std::runtime_error("Invalid snapshot vector");v.resize(size_t(n));f.read(reinterpret_cast<char*>(v.data()),std::streamsize(n*sizeof(T)));if(!f)throw std::runtime_error("Truncated snapshot vector");}
inline void plane(std::ostream &f,pigment::ConstFloatPlaneView p){for(int y=p.bounds.y1;y<p.bounds.y2;++y)for(int x=p.bounds.x1;x<p.bounds.x2;++x)io(f,p.at(x,y));}
inline void plane(std::istream &f,pigment::FloatPlaneView p){for(int y=p.bounds.y1;y<p.bounds.y2;++y)for(int x=p.bounds.x1;x<p.bounds.x2;++x)io(f,p.at(x,y));}
inline uint64_t key(pigment::ConstYabPlanes source,const pigment::Phase4Params &p){uint64_t hash=1469598103934665603ull;
  auto add=[&](float v){uint32_t bits;std::memcpy(&bits,&v,4);hash^=bits;hash*=1099511628211ull;};
  for(auto field:{source.y,source.a,source.b})for(int y=field.bounds.y1;y<field.bounds.y2;++y)for(int x=field.bounds.x1;x<field.bounds.x2;++x)add(field.at(x,y));
  for(float v:{float(p.latentCount),float(p.plateCount),p.plateScale,p.plateOverlap,p.chromaSupportRatio,p.boundaryLock,p.lumaChromaCoupling,p.lumaChunkScale,p.chromaChunkScale,p.mergeSelectivity,p.internalVariation})add(v);
  return hash;
}
inline void saveSnapshot(const std::filesystem::path &path,uint64_t hash,const pigment::Phase4AutomaticResult &a,const pigment::Phase4RegionHierarchy &h){
  std::ofstream f(path.string()+".tmp",std::ios::binary);uint64_t magic=0x5046344341434801ull;io(f,magic);io(f,hash);io(f,h.bounds);
  int count=a.plates.count();io(f,count);
  for(int i=0;i<count;++i){plane(f,a.plates.alpha(i));plane(f,a.plates.supportY(i));plane(f,a.plates.supportAB(i));auto v=a.plates.appearance(i);plane(f,v.y);plane(f,v.a);plane(f,v.b);}
  io(f,a.analysisGraph.width);io(f,a.analysisGraph.height);vec(f,a.analysisGraph.rowOffsets);vec(f,a.analysisGraph.edges);
  io(f,h.atomicRegionCount);vec(f,h.atomicRegion);plane(f,h.boundaryStrength.view());
  for(auto &p:h.plates){vec(f,p.yChunk);vec(f,p.abChunk);vec(f,p.yEdgeX);vec(f,p.yEdgeY);vec(f,p.abEdgeX);vec(f,p.abEdgeY);io(f,p.yChunkCount);io(f,p.abChunkCount);plane(f,p.yRetainedBoundaries.view());plane(f,p.abRetainedBoundaries.view());}
  if(!f)throw std::runtime_error("Failed research snapshot write");f.close();std::filesystem::rename(path.string()+".tmp",path);
}
inline void loadSnapshot(const std::filesystem::path &path,uint64_t hash,pigment::Phase4AutomaticResult &a,pigment::Phase4RegionHierarchy &h){
  std::ifstream f(path,std::ios::binary);uint64_t magic,stored;io(f,magic);io(f,stored);pigment::RectI b;io(f,b);int count;io(f,count);
  if(magic!=0x5046344341434801ull || hash!=stored || count!=a.plates.count() || b.x1!=a.plates.bounds().x1 || b.y1!=a.plates.bounds().y1 || b.x2!=a.plates.bounds().x2 || b.y2!=a.plates.bounds().y2)throw std::runtime_error("Research snapshot key/layout mismatch");
  for(int i=0;i<count;++i){plane(f,a.plates.alpha(i));plane(f,a.plates.supportY(i));plane(f,a.plates.supportAB(i));auto v=a.plates.appearance(i);plane(f,v.y);plane(f,v.a);plane(f,v.b);}
  io(f,a.analysisGraph.width);io(f,a.analysisGraph.height);vec(f,a.analysisGraph.rowOffsets);vec(f,a.analysisGraph.edges);
  h.bounds=b;io(f,h.atomicRegionCount);vec(f,h.atomicRegion);h.boundaryStrength=pigment::OwnedPlane(b);plane(f,h.boundaryStrength.view());
  for(int i=0;i<count;++i){h.plates.emplace_back(b);auto &p=h.plates.back();vec(f,p.yChunk);vec(f,p.abChunk);vec(f,p.yEdgeX);vec(f,p.yEdgeY);vec(f,p.abEdgeX);vec(f,p.abEdgeY);io(f,p.yChunkCount);io(f,p.abChunkCount);plane(f,p.yRetainedBoundaries.view());plane(f,p.abRetainedBoundaries.view());}
}
}
