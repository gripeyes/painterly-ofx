#include "core/RegionalEigenField.h"
#include <Eigen/Eigenvalues>
#include <Eigen/SparseCholesky>
#include <Spectra/SymEigsShiftSolver.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <map>
#include <stdexcept>

namespace pigment {
namespace {
using Sparse=Eigen::SparseMatrix<double>;
struct ShiftOp {
  using Scalar=double;
  const Sparse &matrix;
  Eigen::SimplicialLDLT<Sparse> factor;
  explicit ShiftOp(const Sparse &a):matrix(a){}
  int rows()const{return int(matrix.rows());}
  int cols()const{return int(matrix.cols());}
  void set_shift(double shift) {
    Sparse a=matrix;
    for(int i=0;i<rows();++i)a.coeffRef(i,i)-=shift;
    factor.compute(a);
    if(factor.info()!=Eigen::Success)throw std::runtime_error("Regional Dirichlet factor failed");
  }
  void perform_op(const double *x,double *y)const {
    Eigen::Map<Eigen::VectorXd>(y,rows())=factor.solve(Eigen::Map<const Eigen::VectorXd>(x,rows()));
  }
};
constexpr std::array<int,4> yCounts{2,4,8,12},abCounts{1,2,4,6};
std::array<ConstFloatPlaneView,3> channels(ConstYabPlanes p){return {p.y,p.a,p.b};}
std::array<FloatPlaneView,3> channels(YabPlanes p){return {p.y,p.a,p.b};}
}

RegionalEigenSweep regionalEigenFieldSweep(const PublicPlateSet &plates,
    const Phase4RegionHierarchy &hierarchy,const ExecutionContext &execution,
    bool broadSideBoundary,bool lowBudgetOnly) {
  RectI b=hierarchy.bounds;int width=b.width(),height=b.height(),total=width*height;
  RegionalEigenSweep output;
  int trials=lowBudgetOnly?1:4;
  for(int trial=0;trial<trials;++trial) {
    output.results.emplace_back(b);
    for(int plate=0;plate<plates.count();++plate) {
      output.results.back().appearance.emplace_back(b);
      auto src=channels(plates.appearance(plate));
      auto dst=channels(output.results.back().appearance.back().view());
      for(int k=0;k<3;++k) for(int y=b.y1;y<b.y2;++y) for(int x=b.x1;x<b.x2;++x)
        dst[size_t(k)].at(x,y)=src[size_t(k)].at(x,y);
    }
  }
  for(int plate=0;plate<plates.count();++plate) {
    output.yModeAtlas.emplace_back();output.abModeAtlas.emplace_back();
    for(int k=0;k<12;++k) output.yModeAtlas.back().emplace_back(b);
    for(int k=0;k<6;++k) output.abModeAtlas.back().emplace_back(b);
    auto automatic=channels(plates.appearance(plate));
    output.boundaryAppearance.emplace_back(b);
    auto boundary=channels(output.boundaryAppearance.back().view());
    for(int k=0;k<3;++k)for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)
      boundary[size_t(k)].at(x,y)=automatic[size_t(k)].at(x,y);
    for(int family=0;family<2;++family) {
      const auto &h=hierarchy.plates[size_t(plate)];
      const auto &labels=family?h.abChunk:h.yChunk;
      auto support=family?plates.supportAB(plate):plates.supportY(plate);
      auto &atlas=family?output.abModeAtlas.back():output.yModeAtlas.back();
      const auto &counts=family?abCounts:yCounts;
      std::vector<bool> fixed(size_t(total),false),visited(size_t(total),false);
      std::vector<int> index(size_t(total),-1);
      auto neighbors=[&](int p,auto fn) {
        int x=p%width,y=p/width;
        for(int q:{x>0?p-1:-1,x+1<width?p+1:-1,y>0?p-width:-1,y+1<height?p+width:-1})
          if(q>=0)fn(q);
      };
      auto at=[&](auto p,int i){return p.at(b.x1+i%width,b.y1+i/width);};
      for(int p=0;p<total;++p) {
        int x=p%width,y=p/width;
        fixed[size_t(p)]=at(support,p)<.02f || x==0 || y==0 || x+1==width || y+1==height;
        neighbors(p,[&](int q){if(labels[size_t(p)]!=labels[size_t(q)] || at(support,q)<.02f)fixed[size_t(p)]=true;});
      }
      if(broadSideBoundary) {
        // Directed side groups: each side is fit solely from its own A3 values.
        // No samples cross the retained contour. Split disconnected side traces.
        std::map<std::pair<int,int>,std::vector<int>> sides;
        for(int p=0;p<total;++p)if(at(support,p)>=.02f)
          neighbors(p,[&](int q){if(labels[size_t(p)]!=labels[size_t(q)])
            sides[{labels[size_t(p)],labels[size_t(q)]}].push_back(p);});
        Eigen::MatrixXd sum=Eigen::MatrixXd::Zero(total,3);
        Eigen::VectorXd mass=Eigen::VectorXd::Zero(total);
        for(auto &[key,pixels]:sides) {
          std::sort(pixels.begin(),pixels.end());pixels.erase(std::unique(pixels.begin(),pixels.end()),pixels.end());
          std::vector<unsigned char> member(size_t(total),0);
          for(int p:pixels)member[size_t(p)]=1;
          for(int seed:pixels)if(member[size_t(seed)]==1) {
            std::queue<int> todo;todo.push(seed);member[size_t(seed)]=2;
            std::vector<int> trace;
            while(!todo.empty()) {
              int p=todo.front();todo.pop();trace.push_back(p);
              // Eight-connected tracing only; this does not expand the mask.
              for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                int x=p%width+dx,y=p/width+dy;
                if(x<0 || y<0 || x>=width || y>=height)continue;
                int q=y*width+x;if(member[size_t(q)]==1){member[size_t(q)]=2;todo.push(q);}
              }
            }
            std::sort(trace.begin(),trace.end());
            double sw=0,cx=0,cy=0;
            for(int p:trace){double w=at(support,p);sw+=w;cx+=w*(p%width);cy+=w*(p/width);}
            cx/=sw;cy/=sw;Eigen::Matrix2d covariance=Eigen::Matrix2d::Zero();
            for(int p:trace){Eigen::Vector2d v(p%width-cx,p/width-cy);covariance+=at(support,p)*v*v.transpose();}
            Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> axis(covariance/sw);
            Eigen::Vector2d direction=axis.eigenvectors().col(1);
            double scale=std::sqrt(std::max(1.0,axis.eigenvalues()[1]));
            bool trend=family==0 && trace.size()>=8 && axis.eigenvalues()[1]>=4;
            int columns=trend?2:1,n=int(trace.size());
            Eigen::MatrixXd design(n,columns),values(n,3);
            Eigen::VectorXd base(n),weights(n);
            for(int i=0;i<n;++i){int p=trace[size_t(i)];base[i]=at(support,p);design(i,0)=1;
              if(trend)design(i,1)=direction.dot(Eigen::Vector2d(p%width-cx,p/width-cy))/scale;
              for(int k=0;k<3;++k)values(i,k)=at(automatic[size_t(k)],p);}
            for(int k=family?1:0;k<(family?3:1);++k) {
              weights=base;Eigen::VectorXd coefficients;
              for(int pass=0;pass<3;++pass){
                Eigen::MatrixXd gram=design.transpose()*weights.asDiagonal()*design;
                gram.diagonal().array()+=1e-10*sw;
                coefficients=gram.ldlt().solve(design.transpose()*weights.asDiagonal()*values.col(k));
                Eigen::VectorXd residual=values.col(k)-design*coefficients;
                std::vector<double> magnitude;for(int i=0;i<n;++i)magnitude.push_back(std::abs(residual[i]));
                std::sort(magnitude.begin(),magnitude.end());
                double robustScale=std::max(1e-6,1.4826*magnitude[size_t(n/2)]);
                for(int i=0;i<n;++i)weights[i]=base[i]/(1+std::pow(residual[i]/(2*robustScale),2));
              }
              Eigen::VectorXd fitted=design*coefficients;
              for(int i=0;i<n;++i)sum(trace[size_t(i)],k)+=base[i]*fitted[i];
            }
            for(int i=0;i<n;++i)mass[trace[size_t(i)]]+=base[i];
          }
        }
        for(int p=0;p<total;++p)if(mass[p]>0)
          for(int k=family?1:0;k<(family?3:1);++k) {
            float v=float(sum(p,k)/mass[p]);boundary[size_t(k)].at(b.x1+p%width,b.y1+p/width)=v;
            for(auto &result:output.results)channels(result.appearance[size_t(plate)].view())[size_t(k)].at(b.x1+p%width,b.y1+p/width)=v;
          }
      }
      int component=0;
      for(int seed=0;seed<total;++seed) if(!fixed[size_t(seed)] && !visited[size_t(seed)]) {
        if(execution.cancelled())throw std::runtime_error("Regional eigenfield cancelled");
        std::vector<int> region;std::queue<int> queue;queue.push(seed);visited[size_t(seed)]=true;
        while(!queue.empty()) {
          int p=queue.front();queue.pop();region.push_back(p);
          neighbors(p,[&](int q){if(!fixed[size_t(q)] && !visited[size_t(q)] && labels[size_t(p)]==labels[size_t(q)]) {
            visited[size_t(q)]=true;queue.push(q);
          }});
        }
        std::sort(region.begin(),region.end());int n=int(region.size());
        for(int i=0;i<n;++i)index[size_t(region[size_t(i)])]=i;
        std::vector<Eigen::Triplet<double>> triplets;
        Eigen::MatrixXd rhs=Eigen::MatrixXd::Zero(n,3),target(n,3);
        Eigen::VectorXd weight(n);
        for(int i=0;i<n;++i) {
          int p=region[size_t(i)],degree=0;weight[i]=at(support,p);
          for(int k=0;k<3;++k)target(i,k)=at(automatic[size_t(k)],p);
          neighbors(p,[&](int q) {
            if(labels[size_t(p)]!=labels[size_t(q)] || at(support,q)<.02f)return;
            ++degree;
            if(fixed[size_t(q)]) for(int k=0;k<3;++k)rhs(i,k)+=at(boundary[size_t(k)],q);
            else triplets.emplace_back(i,index[size_t(q)],-1);
          });
          triplets.emplace_back(i,i,degree);
        }
        Sparse laplacian(n,n);laplacian.setFromTriplets(triplets.begin(),triplets.end());
        ShiftOp op(laplacian);op.set_shift(0);
        Eigen::MatrixXd lift=op.factor.solve(rhs);
        double liftResidual=(laplacian*lift-rhs).norm()/std::max(1.0,rhs.norm());
        if(!lift.allFinite() || liftResidual>1e-7)throw std::runtime_error("Regional boundary lift failed");
        int wanted=std::min(n,lowBudgetOnly?counts.front():counts.back());
        Eigen::MatrixXd phi;Eigen::VectorXd eigenvalues;
        if(n<=48) {
          Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solve{Eigen::MatrixXd(laplacian)};
          if(solve.info()!=Eigen::Success)throw std::runtime_error("Regional dense eigensolve failed");
          phi=solve.eigenvectors().leftCols(wanted);eigenvalues=solve.eigenvalues().head(wanted);
        } else {
          Spectra::SymEigsShiftSolver<ShiftOp> solve(op,wanted,std::min(n,std::max(48,3*wanted+1)),0);
          Eigen::VectorXd initial(n);
          for(int i=0;i<n;++i)initial[i]=std::sin(.754877666*(region[size_t(i)]+1))+.5*std::cos(.569840291*(i+1));
          solve.init(initial.data());
          int converged=solve.compute(Spectra::SortRule::LargestMagn,1400,1e-10,Spectra::SortRule::SmallestAlge);
          if(converged<wanted || solve.info()!=Spectra::CompInfo::Successful)
            throw std::runtime_error("Regional sparse eigenbasis did not converge");
          phi=solve.eigenvectors();eigenvalues=solve.eigenvalues();
        }
        if((phi.transpose()*phi-Eigen::MatrixXd::Identity(wanted,wanted)).cwiseAbs().maxCoeff()>1e-6)
          throw std::runtime_error("Regional modes are not orthonormal");
        for(int k=0;k<wanted;++k) {
          Eigen::Index pivot;phi.col(k).cwiseAbs().maxCoeff(&pivot);
          if(phi(pivot,k)<0)phi.col(k)*=-1;
          double residual=(laplacian*phi.col(k)-eigenvalues[k]*phi.col(k)).norm();
          if(residual>1e-6 || eigenvalues[k]<=0)throw std::runtime_error("Invalid regional Dirichlet mode");
          double peak=phi.col(k).cwiseAbs().maxCoeff();
          for(int i=0;i<n;++i)atlas[size_t(k)].view().at(b.x1+region[size_t(i)]%width,b.y1+region[size_t(i)]/width)=float(phi(i,k)/peak);
          RegionalModeDiagnostic record;record.plate=plate;record.family=family;record.chunk=labels[size_t(seed)];
          record.component=component;record.interior=n;record.mode=k;record.eigenvalue=eigenvalues[k];record.residual=residual;
          output.modes.push_back(record);
        }
        for(int trial=0;trial<trials;++trial) {
          int use=std::min(wanted,counts[size_t(trial)]);
          Eigen::MatrixXd basis=phi.leftCols(use);
          Eigen::MatrixXd gram=basis.transpose()*weight.asDiagonal()*basis;
          Eigen::MatrixXd coeff=gram.ldlt().solve(basis.transpose()*weight.asDiagonal()*(target-lift));
          Eigen::MatrixXd fitted=lift+basis*coeff;
          if(!fitted.allFinite())throw std::runtime_error("Regional weighted coefficient fit failed");
          auto dst=channels(output.results[size_t(trial)].appearance[size_t(plate)].view());
          RegionalFitDiagnostic fit;fit.plate=plate;fit.family=family;fit.chunk=labels[size_t(seed)];
          fit.component=component;fit.interior=n;fit.requested=counts[size_t(trial)];fit.used=use;fit.mass=weight.sum();
          for(int k=family?1:0;k<(family?3:1);++k) {
            fit.rmse[size_t(k)]=std::sqrt(((fitted.col(k)-target.col(k)).array().square()*weight.array()).sum()/weight.sum());
            for(int i=0;i<n;++i)dst[size_t(k)].at(b.x1+region[size_t(i)]%width,b.y1+region[size_t(i)]/width)=float(fitted(i,k));
          }
          output.fits.push_back(fit);
          if(trial==trials-1) for(int k=0;k<use;++k) {
            auto &record=output.modes[output.modes.size()-size_t(wanted)+size_t(k)];
            for(int channel=family?1:0;channel<(family?3:1);++channel) {
              record.coefficient[size_t(channel)]=coeff(k,channel);
              record.gradientEnergy[size_t(channel)]=eigenvalues[k]*coeff(k,channel)*coeff(k,channel);
            }
          }
        }
        for(int p:region)index[size_t(p)]=-1;
        ++component;
      }
    }
  }
  for(auto &result:output.results) {
    auto composite=channels(result.composite.view());
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x) {
      std::array<double,3> sum{};
      for(int i=0;i<plates.count();++i) {
        auto value=channels(static_cast<const OwnedYabPlanes &>(result.appearance[size_t(i)]).view());
        double alpha=plates.alpha(i).at(x,y);
        for(int k=0;k<3;++k)sum[size_t(k)]+=alpha*value[size_t(k)].at(x,y);
      }
      for(int k=0;k<3;++k)composite[size_t(k)].at(x,y)=float(sum[size_t(k)]);
    }
  }
  return output;
}
} // namespace pigment
