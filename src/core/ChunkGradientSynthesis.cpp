#include "core/ChunkGradientSynthesis.h"
#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/SparseCholesky>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace pigment {
namespace {
constexpr double kSupport = .02;
struct Model {
  int kind = 0;
  Eigen::Vector3d c = Eigen::Vector3d::Zero();
  double rx = 0, ry = 0;
  double evaluate(double x, double y) const {
    if (kind == 0) return c[0];
    if (kind == 1) return c[0] + c[1]*x + c[2]*y;
    return c[0] + c[1]*std::hypot(x-rx,y-ry);
  }
};
struct ChannelResult {
  std::vector<float> values, gradient, model, error;
  std::vector<float> broadTarget, broadInfluence;
  Phase4PoissonDiagnostics solver;
};

ChannelResult synthesizeChannel(ConstFloatPlaneView automatic,
    ConstFloatPlaneView support, const std::vector<int>&chunks, int chunkCount,
    const std::vector<float>&edgeX, const std::vector<float>&edgeY,
    float chunkScale, float complexity, const ExecutionContext&execution,
    bool interiorEnabled, float interiorSpacing, float interiorStrength,
    float firstStrength, float secondStrength) {
  RectI b=automatic.bounds;
  int width=b.width(),height=b.height(),count=width*height;
  ChannelResult out;
  out.values.resize(size_t(count));out.gradient.assign(size_t(count),0);
  out.model.assign(size_t(count),0);out.error.assign(size_t(count),0);
  out.broadTarget.assign(size_t(count),0);out.broadInfluence.assign(size_t(count),0);
  std::vector<double> input(size_t(count),0),coverage(size_t(count),0);
  for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
    int p=y*width+x;
    input[size_t(p)]=automatic.at(x+b.x1,y+b.y1);
    coverage[size_t(p)]=support.at(x+b.x1,y+b.y1);
    out.values[size_t(p)]=float(input[size_t(p)]);
  }
  if(chunkScale<=0 || complexity>=1) return out;
  complexity=std::clamp(complexity,0.0f,1.0f);
  std::vector<std::vector<int>> pixels{size_t(chunkCount)};
  double sum=0,sum2=0,mass=0;
  for(int p=0;p<count;++p) if(coverage[size_t(p)]>=kSupport) {
    pixels[size_t(chunks[size_t(p)])].push_back(p);
    double w=coverage[size_t(p)],v=input[size_t(p)];
    sum+=w*v;sum2+=w*v*v;mass+=w;
  }
  double sceneScale=std::max(1e-5,std::sqrt(std::max(0.0,
      sum2/std::max(1e-12,mass)-std::pow(sum/std::max(1e-12,mass),2))));
  std::vector<Model> models{size_t(chunkCount)};
  std::vector<int> selected(size_t(chunkCount),3);
  std::vector<float> errors(size_t(chunkCount),0);
  auto xy=[&](int p) {return Eigen::Vector2d(double(p%width)/std::max(1,width-1),
                                           double(p/width)/std::max(1,height-1));};
  for(int chunk=0;chunk<chunkCount;++chunk) {
    if((chunk&127)==0 && execution.cancelled()) throw std::runtime_error("Phase 4 synthesis cancelled");
    const auto &domain=pixels[size_t(chunk)];
    if(domain.empty()) continue;
    double m=0;Eigen::Vector2d center=Eigen::Vector2d::Zero();
    Eigen::Vector2d lo(1,1),hi(0,0);
    for(int p:domain) {auto pos=xy(p);double w=coverage[size_t(p)];
      m+=w;center+=w*pos;lo=lo.cwiseMin(pos);hi=hi.cwiseMax(pos);}
    center/=m;
    std::vector<Model> candidates(11);
    candidates[0].kind=0;candidates[1].kind=1;
    for(int k=0;k<9;++k) {
      auto &model=candidates[size_t(k+2)];model.kind=2;
      double angle=2*3.141592653589793*(k-1)/8;
      model.rx=center.x()+(k==0?0:.35*(hi.x()-lo.x())*std::cos(angle));
      model.ry=center.y()+(k==0?0:.35*(hi.y()-lo.y())*std::sin(angle));
    }
    for(auto &model:candidates) {
      Eigen::Matrix3d normal=Eigen::Matrix3d::Zero();
      Eigen::Vector3d rhs=Eigen::Vector3d::Zero();
      for(int p:domain) {
        auto pos=xy(p);Eigen::Vector3d phi(1,0,0);
        if(model.kind==1) {phi[1]=pos.x();phi[2]=pos.y();}
        if(model.kind==2) phi[1]=std::hypot(pos.x()-model.rx,pos.y()-model.ry);
        double w=coverage[size_t(p)];normal+=w*phi*phi.transpose();rhs+=w*phi*input[size_t(p)];
      }
      normal.diagonal().array()+=1e-10*std::max(1.0,m);
      model.c=normal.ldlt().solve(rhs);
    }
    // Qualify broad gradient hypotheses against source chords, not every
    // fine oscillation the model is intentionally meant to extinguish.
    // Chords stay wholly inside this supported solve domain; they neither
    // average source pixels nor cross retained contours. A local second-
    // difference estimate removes oscillatory variance from this ANALYSIS
    // score only. The original gradients still enter reconstruction below.
    struct Chord {int first,second,length;double weight;};
    std::vector<Chord> chords;
    double noise=0,noiseMass=0;
    for(int p:domain) {
      int x=p%width,y=p/width;
      for(int axis=0;axis<2;++axis) {
        int step=axis==0?1:width;
        int coordinate=axis==0?x:y,extent=axis==0?width:height;
        if(coordinate>0 && coordinate+1<extent &&
           chunks[size_t(p-step)]==chunk && chunks[size_t(p+step)]==chunk &&
           coverage[size_t(p-step)]>=kSupport && coverage[size_t(p+step)]>=kSupport) {
          double second=input[size_t(p-step)]-2*input[size_t(p)]+input[size_t(p+step)];
          noise+=coverage[size_t(p)]*second*second/6;
          noiseMass+=coverage[size_t(p)];
        }
        for(int length:{8,16}) {
          if(coordinate+length>=extent) continue;
          bool valid=true;
          for(int offset=1;offset<=length;++offset)
            if(chunks[size_t(p+offset*step)]!=chunk ||
               coverage[size_t(p+offset*step)]<kSupport) {valid=false;break;}
          if(valid) chords.push_back({p,p+length*step,length,
              std::sqrt(coverage[size_t(p)]*coverage[size_t(p+length*step)])});
        }
      }
    }
    noise/=std::max(1e-20,noiseMass);
    double best=std::numeric_limits<double>::infinity();
    bool accepted=false;
    for(int k=0;k<int(candidates.size());++k) {
      const auto &model=candidates[size_t(k)];double err=0,ge=0,gs=0,noiseEnergy=0;
      for(int p:domain) {
        auto pos=xy(p);double estimate=model.evaluate(pos.x(),pos.y());
        err+=coverage[size_t(p)]*std::pow(estimate-input[size_t(p)],2);
      }
      for(const auto &chord:chords) {
        auto pp=xy(chord.first),qq=xy(chord.second);
        double g=(input[size_t(chord.second)]-input[size_t(chord.first)])/chord.length;
        double bg=(model.evaluate(qq.x(),qq.y())-model.evaluate(pp.x(),pp.y()))/chord.length;
        ge+=chord.weight*(g-bg)*(g-bg);gs+=chord.weight*g*g;
        noiseEnergy+=chord.weight*2*noise/(chord.length*chord.length);
      }
      double ve=std::sqrt(std::max(0.0,err/m-noise))/sceneScale;
      double gradientError=std::sqrt(std::max(0.0,ge-noiseEnergy)/
                                    std::max(1e-20,gs-noiseEnergy));
      double combined=ve+.25*gradientError;
      if(combined<best) best=combined;
      if(!accepted && ve<=.04+.20*(1-complexity) && gradientError<=.10+.70*(1-complexity)) {
        models[size_t(chunk)]=model;selected[size_t(chunk)]=model.kind;
        errors[size_t(chunk)]=float(ve);accepted=true;
      }
    }
    if(!accepted) {
      // A rejected primitive must not secretly generate the final shading.
      // In the source-gradient fallback the retained contour constraints
      // provide the broad harmonic field, with hierarchy-weighted original
      // gradients supplying interior variation. No failed ramp is imposed.
      models[size_t(chunk)]=Model{};
      errors[size_t(chunk)]=float(best);
    }
  }
  std::vector<double> wx(size_t(count),0),wy(size_t(count),0),rhs(size_t(count),0),diag(size_t(count),0);
  std::vector<bool> fixed(size_t(count),false);
  // Explicit infinite internal edges are diagnostic hard barriers. Normal
  // hierarchy levels remain unchanged; this also preserves barriers when an
  // ablated domain connects around the other end of a retained contour.
  auto hardEdge=[&](int p,int q) {
    return std::abs(p-q)==1?!std::isfinite(edgeX[size_t(std::min(p,q))]):
                            !std::isfinite(edgeY[size_t(std::min(p,q))]);
  };
  for(int p=0;p<count;++p) {
    int x=p%width,y=p/width,c=chunks[size_t(p)];
    fixed[size_t(p)]=coverage[size_t(p)]<kSupport || x==0 || y==0 || x+1==width || y+1==height;
    for(int q:{x>0?p-1:-1,x+1<width?p+1:-1,y>0?p-width:-1,y+1<height?p+width:-1})
      if(q>=0 && (chunks[size_t(q)]!=c || coverage[size_t(q)]<kSupport || hardEdge(p,q))) fixed[size_t(p)]=true;
    out.model[size_t(p)]=float(selected[size_t(c)])/3;
    out.error[size_t(p)]=errors[size_t(c)];
  }
  struct Moment {
    std::vector<std::pair<int,double>> weights; // normalized supported region
    double target=0,adjustedTarget=0,strength=0;
    bool first=false;
    bool second=false;
  };
  std::vector<Moment> moments;
  if(interiorEnabled && interiorStrength>0) {
    int spacing=std::max(8,int(std::lround(interiorSpacing)));
    int radius=std::max(4,int(std::lround(.375*spacing)));
    std::vector<int> distance(size_t(count),0),nearest(size_t(count),0);
    constexpr int infinity=std::numeric_limits<int>::max()/4;
    auto neighbors=[&](int p,auto fn) {
      int x=p%width,y=p/width;
      for(int q:{x>0?p-1:-1,x+1<width?p+1:-1,y>0?p-width:-1,y+1<height?p+width:-1})
        if(q>=0 && chunks[size_t(q)]==chunks[size_t(p)] && coverage[size_t(q)]>=kSupport && !hardEdge(p,q)) fn(q);
    };
    for(const auto &domain:pixels) {
      if(domain.size()<size_t(spacing*spacing/2)) continue;
      if(execution.cancelled()) throw std::runtime_error("Phase 4 broad-form analysis cancelled");
      std::queue<int> queue;
      for(int p:domain) {
        distance[size_t(p)]=fixed[size_t(p)]?0:infinity;
        nearest[size_t(p)]=infinity;
        if(fixed[size_t(p)]) queue.push(p);
      }
      while(!queue.empty()) {
        int p=queue.front();queue.pop();
        neighbors(p,[&](int q) {if(distance[size_t(q)]>distance[size_t(p)]+1) {
          distance[size_t(q)]=distance[size_t(p)]+1;queue.push(q);
        }});
      }
      // Source/barrier-driven farthest sites, not a rectangular control grid.
      // Patch boundaries are measurements only, never output solve barriers.
      for(int site=0;site<64;++site) {
        int seed=-1,best=-1;
        for(int p:domain) if(distance[size_t(p)]>=radius && !fixed[size_t(p)] &&
                                 coverage[size_t(p)]>=.1) {
          int score=site==0?distance[size_t(p)]:nearest[size_t(p)];
          if(score>best) {seed=p;best=score;}
        }
        if(seed<0 || (site>0 && best<spacing)) break;
        std::vector<int> patch;
        for(int p:domain) distance[size_t(p)]=infinity;
        distance[size_t(seed)]=0;queue.push(seed);
        while(!queue.empty()) {
          int p=queue.front();queue.pop();
          int d=distance[size_t(p)];
          nearest[size_t(p)]=std::min(nearest[size_t(p)],d);
          if(d<=radius) patch.push_back(p);
          neighbors(p,[&](int q) {if(distance[size_t(q)]>d+1) {
            distance[size_t(q)]=d+1;queue.push(q);
          }});
        }
        // Restore depth-to-contour, used for every candidate's confidence.
        for(int p:domain) {
          distance[size_t(p)]=fixed[size_t(p)]?0:infinity;
          if(fixed[size_t(p)]) queue.push(p);
        }
        while(!queue.empty()) {
          int p=queue.front();queue.pop();
          neighbors(p,[&](int q) {if(distance[size_t(q)]>distance[size_t(p)]+1) {
            distance[size_t(q)]=distance[size_t(p)]+1;queue.push(q);
          }});
        }
        double weight=0,target=0;
        for(int p:patch) {weight+=coverage[size_t(p)];target+=coverage[size_t(p)]*input[size_t(p)];}
        // Do not let near-absent appearance become a trusted form anchor.
        if(weight<.1*patch.size() || patch.size()<size_t(radius*radius)) continue;
        Moment moment;
        moment.target=target/weight;
        moment.adjustedTarget=moment.target;
        // A large-scale average constraint, NOT a pixelwise source screen.
        // Integrating the gradient energy over a region gives area/radius².
        moment.strength=interiorStrength*weight/(radius*radius);
        for(int p:patch) {
          double w=coverage[size_t(p)]/weight;
          out.broadTarget[size_t(p)]+=float(w*moment.target);
          out.broadInfluence[size_t(p)]+=float(w);
          if(fixed[size_t(p)]) moment.adjustedTarget-=w*input[size_t(p)];
          else moment.weights.emplace_back(p,w);
        }
        if(!moment.weights.empty()) moments.push_back(std::move(moment));
        if(firstStrength>0 || secondStrength>0) {
          // Centered, support-weighted first moments. No primitive-fit gate:
          // these describe regional direction, not a replacement pixel field.
          double cx=0,cy=0;
          for(int p:patch) {
            double mu=coverage[size_t(p)]/weight;
            cx+=mu*(p%width);cy+=mu*(p/width);
          }
          for(int axis=0;axis<2 && firstStrength>0;++axis) {
            double variance=0;
            for(int p:patch) {
              double dx=axis==0?p%width-cx:p/width-cy;
              variance+=coverage[size_t(p)]/weight*dx*dx;
            }
            double extent=std::sqrt(variance);
            // Reject a nearly one-dimensional measurement on this axis.
            if(extent<.15*radius) continue;
            Moment first;first.first=true;
            first.strength=firstStrength*weight/(radius*radius);
            for(int p:patch) {
              double dx=axis==0?p%width-cx:p/width-cy;
              double w=coverage[size_t(p)]/weight*dx/extent;
              first.target+=w*input[size_t(p)];
              if(!fixed[size_t(p)]) first.weights.emplace_back(p,w);
            }
            first.adjustedTarget=first.target;
            for(int p:patch) if(fixed[size_t(p)]) {
              double dx=axis==0?p%width-cx:p/width-cy;
              first.adjustedTarget-=coverage[size_t(p)]/weight*dx/extent*input[size_t(p)];
            }
            if(!first.weights.empty()) moments.push_back(std::move(first));
          }
          if(secondStrength>0) {
            // Weighted orthogonalization removes redundant mean/tilt content
            // and conditions anisotropic patches. This transforms statistics
            // only; no quadratic target image is ever constructed.
            std::vector<std::vector<double>> frame;
            auto inner=[&](const auto &a,const auto &b) {
              double sum=0;
              for(size_t k=0;k<patch.size();++k)
                sum+=coverage[size_t(patch[k])]/weight*a[k]*b[k];
              return sum;
            };
            auto qualify=[&](std::vector<double> &v) {
              double original=inner(v,v);
              for(int pass=0;pass<2;++pass) for(const auto &axis:frame) {
                double dot=inner(v,axis);
                for(size_t k=0;k<v.size();++k) v[k]-=dot*axis[k];
              }
              double norm=inner(v,v);
              if(norm<1e-6 || norm<1e-4*original) return false;
              for(double &value:v) value/=std::sqrt(norm);
              frame.push_back(v);return true;
            };
            std::vector<double> v(patch.size(),1);
            qualify(v);
            for(int axis=0;axis<2;++axis) {
              for(size_t k=0;k<patch.size();++k) {
                int p=patch[k];v[k]=(axis==0?p%width-cx:p/width-cy)/radius;
              }
              qualify(v);
            }
            for(int axis=0;axis<3;++axis) {
              for(size_t k=0;k<patch.size();++k) {
                int p=patch[k];double x=(p%width-cx)/radius,y=(p/width-cy)/radius;
                v[k]=axis==0?x*x:axis==1?x*y:y*y;
              }
              if(!qualify(v)) continue;
              Moment second;second.second=true;
              second.strength=secondStrength*weight/(radius*radius);
              for(size_t k=0;k<patch.size();++k) {
                int p=patch[k];double w=coverage[size_t(p)]/weight*v[k];
                second.target+=w*input[size_t(p)];
                if(!fixed[size_t(p)]) second.weights.emplace_back(p,w);
              }
              second.adjustedTarget=second.target;
              for(size_t k=0;k<patch.size();++k) if(fixed[size_t(patch[k])])
                second.adjustedTarget-=coverage[size_t(patch[k])]/weight*v[k]*input[size_t(patch[k])];
              if(!second.weights.empty()) moments.push_back(std::move(second));
            }
          }
        }
      }
    }
  }
  for(const auto &moment:moments)
    if(moment.second) ++out.solver.secondConstraints;
    else if(moment.first) ++out.solver.firstConstraints;
    else ++out.solver.broadConstraints;
  for(int p=0;p<count;++p) if(out.broadInfluence[size_t(p)]>0)
    out.broadTarget[size_t(p)]/=out.broadInfluence[size_t(p)];
  auto appendEdge=[&](int p,int q,float level,double &weight) {
    if(chunks[size_t(p)]!=chunks[size_t(q)] || coverage[size_t(p)]<kSupport || coverage[size_t(q)]<kSupport || !std::isfinite(level)) return;
    weight=std::sqrt(coverage[size_t(p)]*coverage[size_t(q)]);
    int c=chunks[size_t(p)];auto pp=xy(p),qq=xy(q);const auto &model=models[size_t(c)];
    double gb=model.evaluate(qq.x(),qq.y())-model.evaluate(pp.x(),pp.y());
    double t=std::clamp((double(level)/chunkScale-.35)/(.95-.35),0.0,1.0);
    // The approved survival equation applies to all candidates. A qualifying
    // primitive is a broad gradient hypothesis, not permission to erase every
    // source gradient regardless of the artist's Complexity control.
    double r=complexity+(1-complexity)*t*t*(3-2*t);
    double g=gb+r*(input[size_t(q)]-input[size_t(p)]-gb);
    rhs[size_t(p)]-=weight*g;rhs[size_t(q)]+=weight*g;
    diag[size_t(p)]+=weight;diag[size_t(q)]+=weight;
    out.gradient[size_t(p)]+=float(g*g);out.gradient[size_t(q)]+=float(g*g);
  };
  for(int p=0;p<count;++p) {
    if(p%width+1<width) appendEdge(p,p+1,edgeX[size_t(p)],wx[size_t(p)]);
    if(p/width+1<height) appendEdge(p,p+width,edgeY[size_t(p)],wy[size_t(p)]);
  }
  // Eliminate fixed contour values from the unknown system. No source data
  // screen is added to the chunk-interior Poisson equation.
  auto edges=[&](auto fn) {for(int p=0;p<count;++p) {
    if(wx[size_t(p)]>0) fn(p,p+1,wx[size_t(p)]);
    if(wy[size_t(p)]>0) fn(p,p+width,wy[size_t(p)]);
  }};
  edges([&](int p,int q,double w) {
    if(!fixed[size_t(p)] && fixed[size_t(q)]) rhs[size_t(p)]+=w*input[size_t(q)];
    if(!fixed[size_t(q)] && fixed[size_t(p)]) rhs[size_t(q)]+=w*input[size_t(p)];
  });
  for(const auto &moment:moments) for(auto [p,w]:moment.weights)
    rhs[size_t(p)]+=moment.strength*w*moment.adjustedTarget;
  auto apply=[&](const std::vector<double>&v,std::vector<double>&a) {
    for(int p=0;p<count;++p) a[size_t(p)]=fixed[size_t(p)]?0:diag[size_t(p)]*v[size_t(p)];
    edges([&](int p,int q,double w) {if(!fixed[size_t(p)]&&!fixed[size_t(q)]) {
      a[size_t(p)]-=w*v[size_t(q)];a[size_t(q)]-=w*v[size_t(p)];
    }});
    for(const auto &moment:moments) {
      double average=0;
      for(auto [p,w]:moment.weights) average+=w*v[size_t(p)];
      for(auto [p,w]:moment.weights) a[size_t(p)]+=moment.strength*w*average;
    }
  };
  auto dot=[&](const std::vector<double>&a,const std::vector<double>&v) {
    double s=0;for(int p=0;p<count;++p) if(!fixed[size_t(p)]) s+=a[size_t(p)]*v[size_t(p)];return s;
  };
  std::vector<Eigen::Triplet<double>> triplets;
  triplets.reserve(size_t(count)*5);
  for(int p=0;p<count;++p)
    triplets.emplace_back(p,p,fixed[size_t(p)]?1.0:diag[size_t(p)]);
  edges([&](int p,int q,double w) {if(!fixed[size_t(p)]&&!fixed[size_t(q)]) {
    triplets.emplace_back(p,q,-w);triplets.emplace_back(q,p,-w);
  }});
  // Auxiliary moment variables avoid a dense all-pairs patch matrix. The
  // Schur complement is exactly A_poisson + Σ strength*m*m^T, still SPD.
  for(size_t i=0;i<moments.size();++i) {
    int auxiliary=count+int(i);
    triplets.emplace_back(auxiliary,auxiliary,-1/moments[i].strength);
    for(auto [p,w]:moments[i].weights) {
      triplets.emplace_back(p,auxiliary,w);triplets.emplace_back(auxiliary,p,w);
    }
  }
  Eigen::SparseMatrix<double> matrix(count+int(moments.size()),count+int(moments.size()));
  matrix.setFromTriplets(triplets.begin(),triplets.end());
  // The CPU gate uses a complete sparse factor as the PCG preconditioner.
  // This preserves the exact bounded operator while making solver error
  // negligible before judging the photographic formulation.  A faster
  // preconditioner is a later implementation optimization, not a new look.
  Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> preconditioner;
  preconditioner.compute(matrix);
  if(preconditioner.info()!=Eigen::Success)
    throw std::runtime_error("Phase 4 Poisson preconditioner failed");
  auto precondition=[&](const std::vector<double>&residual,std::vector<double>&output) {
    Eigen::VectorXd inputVector=Eigen::VectorXd::Zero(matrix.rows());
    inputVector.head(count)=Eigen::Map<const Eigen::VectorXd>(residual.data(),count);
    Eigen::Map<Eigen::VectorXd> outputVector(output.data(),count);
    outputVector=preconditioner.solve(inputVector).head(count);
  };
  std::vector<double> u=input,r(size_t(count),0),z(size_t(count),0),direction(size_t(count),0),ad(size_t(count),0);
  apply(u,ad);
  for(int p=0;p<count;++p) if(!fixed[size_t(p)] && diag[size_t(p)]>0) {
    r[size_t(p)]=rhs[size_t(p)]-ad[size_t(p)];
  }
  precondition(r,z);direction=z;
  double initial=std::sqrt(dot(r,r)),rz=dot(r,z);
  out.solver.converged=initial<1e-14;
  for(int it=0;it<400 && !out.solver.converged;++it) {
    if(execution.cancelled()) throw std::runtime_error("Phase 4 Poisson cancelled");
    apply(direction,ad);double denominator=dot(direction,ad);
    if(denominator<=1e-30) break;
    double step=rz/denominator;
    for(int p=0;p<count;++p) if(!fixed[size_t(p)]) {
      u[size_t(p)]+=step*direction[size_t(p)];r[size_t(p)]-=step*ad[size_t(p)];
    }
    out.solver.iterations=it+1;
    out.solver.relativeResidual=std::sqrt(dot(r,r))/std::max(1e-14,initial);
    out.solver.converged=out.solver.relativeResidual<=1e-5;
    if(out.solver.converged) break;
    precondition(r,z);
    double next=dot(r,z),beta=next/std::max(1e-30,rz);rz=next;
    for(int p=0;p<count;++p) direction[size_t(p)]=z[size_t(p)]+beta*direction[size_t(p)];
  }
  apply(u,ad);for(int p=0;p<count;++p) r[size_t(p)]=fixed[size_t(p)]?0:rhs[size_t(p)]-ad[size_t(p)];
  out.solver.relativeResidual=std::sqrt(dot(r,r))/std::max(1e-14,initial);
  out.solver.converged=initial<1e-14 || out.solver.relativeResidual<=1e-5;
  double after=0,firstAfter=0,secondAfter=0;
  for(const auto &moment:moments) {
    double c=0;
    for(auto [p,w]:moment.weights) c+=w*u[size_t(p)];
    double error=(c-moment.adjustedTarget)*(c-moment.adjustedTarget);
    if(moment.second) secondAfter+=error;
    else if(moment.first) firstAfter+=error; else after+=error;
  }
  out.solver.broadResultRmse=std::sqrt(after/std::max(1,out.solver.broadConstraints));
  out.solver.firstResultRmse=std::sqrt(firstAfter/std::max(1,out.solver.firstConstraints));
  out.solver.secondResultRmse=std::sqrt(secondAfter/std::max(1,out.solver.secondConstraints));
  for(int p=0;p<count;++p) {out.values[size_t(p)]=float(u[size_t(p)]);
                           out.gradient[size_t(p)]=std::sqrt(out.gradient[size_t(p)]);}
  return out;
}

void copyChannel(const std::vector<float>&v,FloatPlaneView out) {
  for(int y=out.bounds.y1;y<out.bounds.y2;++y) for(int x=out.bounds.x1;x<out.bounds.x2;++x)
    out.at(x,y)=v[size_t(y-out.bounds.y1)*out.bounds.width()+x-out.bounds.x1];
}
} // namespace

Phase4ChunkSynthesis synthesizePhase4Chunks(ConstYabPlanes source,
    const PublicPlateSet &plates,const Phase4RegionHierarchy &hierarchy,
    const Phase4Params &params,const ExecutionContext &execution,
    const Phase4BroadFormOptions &broadForm) {
  (void)source;
  Phase4ChunkSynthesis result(hierarchy.bounds);
  for(int plate=0;plate<plates.count();++plate) {
    result.plateAppearance.emplace_back(hierarchy.bounds);
    result.sourceGradient.emplace_back(hierarchy.bounds);
    result.simplifiedGradient.emplace_back(hierarchy.bounds);
    result.primitiveSelection.emplace_back(hierarchy.bounds);
    result.fitError.emplace_back(hierarchy.bounds);
    result.broadConstraintTargets.emplace_back(hierarchy.bounds);
    result.broadConstraintInfluence.emplace_back(hierarchy.bounds);
    auto automatic=plates.appearance(plate);const auto &h=hierarchy.plates[size_t(plate)];
    auto y=synthesizeChannel(automatic.y,plates.supportY(plate),h.yChunk,h.yChunkCount,h.yEdgeX,h.yEdgeY,
                             params.lumaChunkScale,params.yGradientComplexity<0?params.gradientComplexity:params.yGradientComplexity,execution,
                             broadForm.enabled,broadForm.spacingY,broadForm.strengthY,broadForm.firstStrengthY,broadForm.secondStrengthY);
    auto a=synthesizeChannel(automatic.a,plates.supportAB(plate),h.abChunk,h.abChunkCount,h.abEdgeX,h.abEdgeY,
                             params.chromaChunkScale,params.abGradientComplexity<0?params.gradientComplexity:params.abGradientComplexity,execution,
                             broadForm.enabled,broadForm.spacingAB,broadForm.strengthAB,broadForm.firstStrengthAB,broadForm.secondStrengthAB);
    auto b=synthesizeChannel(automatic.b,plates.supportAB(plate),h.abChunk,h.abChunkCount,h.abEdgeX,h.abEdgeY,
                             params.chromaChunkScale,params.abGradientComplexity<0?params.gradientComplexity:params.abGradientComplexity,execution,
                             broadForm.enabled,broadForm.spacingAB,broadForm.strengthAB,broadForm.firstStrengthAB,broadForm.secondStrengthAB);
    result.solver.push_back({y.solver,a.solver,b.solver});
    auto out=result.plateAppearance.back().view();copyChannel(y.values,out.y);copyChannel(a.values,out.a);copyChannel(b.values,out.b);
    copyChannel(y.gradient,result.simplifiedGradient.back().view());
    copyChannel(y.model,result.primitiveSelection.back().view());copyChannel(y.error,result.fitError.back().view());
    auto target=result.broadConstraintTargets.back().view();
    copyChannel(y.broadTarget,target.y);copyChannel(a.broadTarget,target.a);copyChannel(b.broadTarget,target.b);
    auto influence=result.broadConstraintInfluence.back().view();
    copyChannel(y.broadInfluence,influence.y);copyChannel(a.broadInfluence,influence.a);copyChannel(b.broadInfluence,influence.b);
    auto sg=result.sourceGradient.back().view();
    for(int yy=hierarchy.bounds.y1;yy<hierarchy.bounds.y2;++yy) for(int x=hierarchy.bounds.x1;x<hierarchy.bounds.x2;++x) {
      int xp=std::min(hierarchy.bounds.x2-1,x+1),yp=std::min(hierarchy.bounds.y2-1,yy+1);
      sg.at(x,yy)=std::hypot(automatic.y.at(xp,yy)-automatic.y.at(x,yy),automatic.y.at(x,yp)-automatic.y.at(x,yy));
    }
  }
  auto output=result.preSpill.view();
  for(int y=hierarchy.bounds.y1;y<hierarchy.bounds.y2;++y) for(int x=hierarchy.bounds.x1;x<hierarchy.bounds.x2;++x) {
    float yy=0,aa=0,bb=0;
    for(int i=0;i<plates.count();++i) {float w=plates.alpha(i).at(x,y);auto app=result.plateAppearance[size_t(i)].view();
      yy+=w*app.y.at(x,y);aa+=w*app.a.at(x,y);bb+=w*app.b.at(x,y);}
    output.y.at(x,y)=yy;output.a.at(x,y)=aa;output.b.at(x,y)=bb;
  }
  return result;
}
} // namespace pigment
