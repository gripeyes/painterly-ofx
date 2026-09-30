#include "core/RegionalEigenField.h"
#include <Eigen/Eigenvalues>
#include <Eigen/SparseCholesky>
#include <Spectra/SymEigsShiftSolver.h>
#include <algorithm>
#include <cmath>
#include <queue>
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
    const Phase4RegionHierarchy &hierarchy,const ExecutionContext &execution) {
  RectI b=hierarchy.bounds;int width=b.width(),height=b.height(),total=width*height;
  RegionalEigenSweep output;
  for(int trial=0;trial<4;++trial) {
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
            if(fixed[size_t(q)]) for(int k=0;k<3;++k)rhs(i,k)+=at(automatic[size_t(k)],q);
            else triplets.emplace_back(i,index[size_t(q)],-1);
          });
          triplets.emplace_back(i,i,degree);
        }
        Sparse laplacian(n,n);laplacian.setFromTriplets(triplets.begin(),triplets.end());
        ShiftOp op(laplacian);op.set_shift(0);
        Eigen::MatrixXd lift=op.factor.solve(rhs);
        double liftResidual=(laplacian*lift-rhs).norm()/std::max(1.0,rhs.norm());
        if(!lift.allFinite() || liftResidual>1e-7)throw std::runtime_error("Regional boundary lift failed");
        int wanted=std::min(n,counts.back());
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
        for(int trial=0;trial<4;++trial) {
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
          if(trial==3) for(int k=0;k<use;++k) {
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
