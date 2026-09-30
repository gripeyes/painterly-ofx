#include "core/SparseTransitionField.h"
#include <Eigen/Eigenvalues>
#include <Eigen/SparseCholesky>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <stdexcept>

namespace pigment { namespace {
using Point=std::array<double,2>;
double distance(Point a,Point b){return std::hypot(a[0]-b[0],a[1]-b[1]);}
double median(std::vector<double> v){std::sort(v.begin(),v.end());return v.empty()?0:v[v.size()/2];}
std::array<ConstFloatPlaneView,3> channels(ConstYabPlanes p){return {p.y,p.a,p.b};}
std::array<FloatPlaneView,3> channels(YabPlanes p){return {p.y,p.a,p.b};}
void simplify(const std::vector<Point>&p,int a,int b,double tolerance,std::vector<Point>&out) {
  double maximum=0;int split=-1;double dx=p[b][0]-p[a][0],dy=p[b][1]-p[a][1];
  for(int i=a+1;i<b;++i){double t=std::clamp(((p[i][0]-p[a][0])*dx+(p[i][1]-p[a][1])*dy)/std::max(1e-12,dx*dx+dy*dy),0.0,1.0);
    double d=distance(p[i],{p[a][0]+t*dx,p[a][1]+t*dy});if(d>maximum){maximum=d;split=i;}}
  if(maximum>tolerance){simplify(p,a,split,tolerance,out);simplify(p,split,b,tolerance,out);}
  else out.push_back(p[a]);
}
}

SparseTransitionResult sparseTransitionField(const PublicPlateSet &plates,
    const Phase4RegionHierarchy &hierarchy,const ExecutionContext &execution) {
  auto b=hierarchy.bounds;int width=b.width(),height=b.height(),total=width*height;
  SparseTransitionResult result(b);
  auto neighbors=[&](int p,auto fn){int x=p%width,y=p/width;
    for(int q:{x>0?p-1:-1,x+1<width?p+1:-1,y>0?p-width:-1,y+1<height?p+width:-1})if(q>=0)fn(q);};
  for(int plate=0;plate<plates.count();++plate) {
    result.appearance.emplace_back(b);result.sideValues.emplace_back(b);
    result.yCurves.emplace_back(b);result.abCurves.emplace_back(b);
    result.yConstraints.emplace_back(b);result.abConstraints.emplace_back(b);
    auto source=channels(plates.appearance(plate));auto output=channels(result.appearance.back().view());
    auto display=channels(result.sideValues.back().view());
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)for(int k=0;k<3;++k)
      output[size_t(k)].at(x,y)=display[size_t(k)].at(x,y)=source[size_t(k)].at(x,y);
    for(int family=0;family<2;++family) {
      if(execution.cancelled())throw std::runtime_error("Sparse transition cancelled");
      auto support=family?plates.supportAB(plate):plates.supportY(plate);
      auto &labels=family?hierarchy.plates[size_t(plate)].abChunk:hierarchy.plates[size_t(plate)].yChunk;
      auto locus=family?result.abCurves.back().view():result.yCurves.back().view();
      auto rails=family?result.abConstraints.back().view():result.yConstraints.back().view();
      auto value=[&](auto plane,int p){return plane.at(b.x1+p%width,b.y1+p/width);};
      int spacing=family?16:8,radius=family?8:4,reach=family?12:6;
      int nx=(width-1)/spacing+1,ny=(height-1)/spacing+1;
      std::vector<Eigen::Vector3d> samples(size_t(nx*ny));std::vector<bool> valid(size_t(nx*ny),false);
      // Robust local measurements are used for geometry analysis only. They
      // are never interpolated into a target image or used as the output field.
      for(int gy=0;gy<ny;++gy)for(int gx=0;gx<nx;++gx) {
        std::array<std::vector<double>,3> values;int cx=gx*spacing,cy=gy*spacing;
        for(int y=std::max(0,cy-radius);y<=std::min(height-1,cy+radius);++y)
          for(int x=std::max(0,cx-radius);x<=std::min(width-1,cx+radius);++x)if(value(support,y*width+x)>=.1f)
            for(int k=0;k<3;++k)values[size_t(k)].push_back(value(source[size_t(k)],y*width+x));
        int n=gy*nx+gx;valid[size_t(n)]=values[0].size()>=size_t(radius*radius);
        if(valid[size_t(n)])for(int k=0;k<3;++k)samples[size_t(n)][k]=median(values[size_t(k)]);
      }
      Eigen::Vector3d projection(1,0,0);
      if(family) {
        Eigen::Vector2d mean=Eigen::Vector2d::Zero();double count=0;
        for(int n=0;n<nx*ny;++n)if(valid[size_t(n)]){mean+=samples[size_t(n)].tail<2>();++count;}
        if(count==0)continue;mean/=count;Eigen::Matrix2d covariance=Eigen::Matrix2d::Zero();
        for(int n=0;n<nx*ny;++n)if(valid[size_t(n)]){auto v=(samples[size_t(n)].tail<2>()-mean).eval();covariance+=v*v.transpose();}
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eigen(covariance);
        auto axis=eigen.eigenvectors().col(1).eval();if(axis[0]<0)axis=-axis;projection<<0,axis[0],axis[1];
      }
      std::vector<double> distribution;
      for(int n=0;n<nx*ny;++n)if(valid[size_t(n)])distribution.push_back(projection.dot(samples[size_t(n)]));
      if(distribution.size()<16)continue;std::sort(distribution.begin(),distribution.end());
      double range=distribution[distribution.size()*9/10]-distribution[distribution.size()/10];
      if(range<1e-6)continue;
      std::vector<SparseTransitionCurve> candidates;
      int levels=family?2:4;
      for(int level=1;level<=levels;++level) {
        double threshold=distribution[distribution.size()*size_t(level)/size_t(levels+1)];
        std::vector<Point> vertices;std::vector<std::vector<int>> adjacency;std::map<std::pair<int,int>,int> edgeVertex;
        auto crossing=[&](int a,int c){auto key=std::minmax(a,c);auto found=edgeVertex.find(key);if(found!=edgeVertex.end())return found->second;
          double va=projection.dot(samples[size_t(a)]),vc=projection.dot(samples[size_t(c)]);
          double t=(threshold-va)/(vc-va);Point p{spacing*((a%nx)*(1-t)+(c%nx)*t),spacing*((a/nx)*(1-t)+(c/nx)*t)};
          int id=int(vertices.size());vertices.push_back(p);adjacency.emplace_back();edgeVertex[key]=id;return id;};
        for(int y=0;y+1<ny;++y)for(int x=0;x+1<nx;++x) {
          int ids[4]{y*nx+x,y*nx+x+1,(y+1)*nx+x+1,(y+1)*nx+x};bool good=true;
          for(int id:ids)good&=valid[size_t(id)];if(!good)continue;
          std::vector<int> hits;
          for(int e=0;e<4;++e){int a=ids[e],c=ids[(e+1)%4];if((projection.dot(samples[size_t(a)])<threshold)!=(projection.dot(samples[size_t(c)])<threshold))hits.push_back(crossing(a,c));}
          // Ambiguous saddle cells are omitted rather than inventing topology.
          if(hits.size()==2){adjacency[size_t(hits[0])].push_back(hits[1]);adjacency[size_t(hits[1])].push_back(hits[0]);}
        }
        std::vector<bool> seen(vertices.size(),false);
        std::vector<int> seeds;for(int i=0;i<int(vertices.size());++i)if(adjacency[size_t(i)].size()==1)seeds.push_back(i);
        for(int i=0;i<int(vertices.size());++i)seeds.push_back(i);
        for(int seed:seeds)if(!seen[size_t(seed)] && !adjacency[size_t(seed)].empty()) {
          std::vector<Point> trace;int previous=-1,current=seed;
          while(current>=0 && !seen[size_t(current)]){seen[size_t(current)]=true;trace.push_back(vertices[size_t(current)]);int next=-1;
            for(int q:adjacency[size_t(current)])if(q!=previous && !seen[size_t(q)]){next=q;break;}previous=current;current=next;}
          double length=0;for(size_t j=1;j<trace.size();++j)length+=distance(trace[j-1],trace[j]);
          if(length<(family?96:48) || trace.size()<4)continue;
          SparseTransitionCurve curve;curve.plate=plate;curve.family=family;curve.length=length;
          simplify(trace,0,int(trace.size())-1,family?4:2,curve.points);curve.points.push_back(trace.back());
          candidates.push_back(std::move(curve));
        }
      }
      std::stable_sort(candidates.begin(),candidates.end(),[](auto &a,auto &c){return a.length>c.length;});
      int maximum=family?6:12;if(int(candidates.size())>maximum)candidates.resize(size_t(maximum));
      Eigen::MatrixXd constraint=Eigen::MatrixXd::Zero(total,3);Eigen::VectorXd constraintMass=Eigen::VectorXd::Zero(total);
      for(auto &curve:candidates) {
        std::array<std::array<std::vector<double>,3>,2> sideSamples;
        std::array<std::array<std::vector<double>,3>,2> broadSamples;
        std::vector<int> centers;
        struct Rail{int p,side;};std::vector<Rail> pixels;
        for(size_t j=1;j<curve.points.size();++j) {
          Point a=curve.points[j-1],c=curve.points[j];double length=distance(a,c);if(length<1e-6)continue;
          Point normal{-(c[1]-a[1])/length,(c[0]-a[0])/length};
          for(int step=0;step<=int(std::ceil(length));++step) {
            double t=double(step)/std::ceil(length);Point center{a[0]+t*(c[0]-a[0]),a[1]+t*(c[1]-a[1])};
            int cx=int(std::lround(center[0])),cy=int(std::lround(center[1]));
            if(cx<0 || cy<0 || cx>=width || cy>=height)continue;int cp=cy*width+cx;
            if(value(support,cp)<.02f)continue;
            centers.push_back(cp);
            for(int side=0;side<2;++side) {
              int x=int(std::lround(center[0]+(side?1:-1)*reach*normal[0]));
              int y=int(std::lround(center[1]+(side?1:-1)*reach*normal[1]));
              if(x<0 || y<0 || x>=width || y>=height)continue;int p=y*width+x;
              // Rails never jump across a frozen Gate-B region barrier.
              if(labels[size_t(p)]!=labels[size_t(cp)] || value(support,p)<.1f)continue;
              bool path=true;
              for(int d=1;d<=reach;++d){int px=int(std::lround(center[0]+(side?1:-1)*d*normal[0])),py=int(std::lround(center[1]+(side?1:-1)*d*normal[1]));
                if(px<0 || py<0 || px>=width || py>=height || labels[size_t(py*width+px)]!=labels[size_t(cp)] || value(support,py*width+px)<.02f){path=false;break;}}
              if(!path)continue;
              pixels.push_back({p,side});
              if(step%4==0){for(int k=0;k<3;++k)sideSamples[size_t(side)][size_t(k)].push_back(value(source[size_t(k)],p));
                int bx=int(std::lround(center[0]+(side?1:-1)*2*reach*normal[0])),by=int(std::lround(center[1]+(side?1:-1)*2*reach*normal[1]));
                if(bx>=0 && by>=0 && bx<width && by<height && labels[size_t(by*width+bx)]==labels[size_t(cp)] && value(support,by*width+bx)>=.1f)
                  for(int k=0;k<3;++k)broadSamples[size_t(side)][size_t(k)].push_back(value(source[size_t(k)],by*width+bx));
              }
            }
          }
        }
        curve.negativeSamples=int(sideSamples[0][0].size());curve.positiveSamples=int(sideSamples[1][0].size());
        if(curve.negativeSamples<8 || curve.positiveSamples<8)continue;
        for(int k=0;k<3;++k){curve.negative[size_t(k)]=median(sideSamples[0][size_t(k)]);curve.positive[size_t(k)]=median(sideSamples[1][size_t(k)]);}
        // Long-baseline appearance persistence: the two-sided change must
        // survive doubling the baseline, not merely cross one sampled level.
        if(broadSamples[0][0].size()<8 || broadSamples[1][0].size()<8)continue;
        Eigen::Vector3d delta,broadDelta;
        for(int k=0;k<3;++k){delta[k]=curve.positive[size_t(k)]-curve.negative[size_t(k)];
          broadDelta[k]=median(broadSamples[1][size_t(k)])-median(broadSamples[0][size_t(k)]);}
        double narrow=projection.dot(delta),wide=projection.dot(broadDelta);
        if(narrow*wide<=0 || std::abs(narrow)<.03*range || std::abs(wide)<.25*std::abs(narrow))continue;
        for(int p:centers)locus.at(b.x1+p%width,b.y1+p/width)=1;
        for(auto rail:pixels){double w=value(support,rail.p);constraintMass[rail.p]+=w;
          for(int k=0;k<3;++k)constraint(rail.p,k)+=w*(rail.side?curve.positive[size_t(k)]:curve.negative[size_t(k)]);}
        result.curves.push_back(curve);
      }
      std::vector<bool> fixed(size_t(total),false),visited(size_t(total),false);std::vector<int> index(size_t(total),-1);
      for(int p=0;p<total;++p)if(value(support,p)>=.02f) {
        bool retained=false;neighbors(p,[&](int q){retained|=labels[size_t(p)]!=labels[size_t(q)];});
        bool structural=retained && value(hierarchy.boundaryStrength.view(),p)>=.75f;
        fixed[size_t(p)]=structural || constraintMass[p]>0;
        if(constraintMass[p]>0 && !structural){rails.at(b.x1+p%width,b.y1+p/width)=1;
          for(int k=family?1:0;k<(family?3:1);++k)output[size_t(k)].at(b.x1+p%width,b.y1+p/width)=float(constraint(p,k)/constraintMass[p]);}
        if(fixed[size_t(p)])for(int k=family?1:0;k<(family?3:1);++k)display[size_t(k)].at(b.x1+p%width,b.y1+p/width)=output[size_t(k)].at(b.x1+p%width,b.y1+p/width);
      }
      for(int seed=0;seed<total;++seed)if(!visited[size_t(seed)] && value(support,seed)>=.02f) {
        if(execution.cancelled())throw std::runtime_error("Sparse transition cancelled");
        std::queue<int> queue;queue.push(seed);visited[size_t(seed)]=true;std::vector<int> domain,free;
        SparseTransitionSolve diagnostic;diagnostic.plate=plate;diagnostic.family=family;diagnostic.chunk=labels[size_t(seed)];
        while(!queue.empty()){int p=queue.front();queue.pop();domain.push_back(p);
          if(fixed[size_t(p)]){diagnostic.curveConstraints+=constraintMass[p]>0;diagnostic.structuralConstraints+=constraintMass[p]==0;}else free.push_back(p);
          neighbors(p,[&](int q){if(!visited[size_t(q)] && labels[size_t(q)]==labels[size_t(p)] && value(support,q)>=.02f){visited[size_t(q)]=true;queue.push(q);}});}
        diagnostic.pixels=int(domain.size());
        if((diagnostic.curveConstraints+diagnostic.structuralConstraints)==0 || free.empty()){result.solves.push_back(diagnostic);continue;}
        std::sort(free.begin(),free.end());int n=int(free.size());for(int i=0;i<n;++i)index[size_t(free[size_t(i)])]=i;
        std::vector<Eigen::Triplet<double>> entries;Eigen::MatrixXd rhs=Eigen::MatrixXd::Zero(n,3);
        for(int i=0;i<n;++i){int p=free[size_t(i)],degree=0;neighbors(p,[&](int q){if(labels[size_t(p)]!=labels[size_t(q)] || value(support,q)<.02f)return;
          ++degree;if(fixed[size_t(q)])for(int k=0;k<3;++k)rhs(i,k)+=value(output[size_t(k)],q);else entries.emplace_back(i,index[size_t(q)],-1);});entries.emplace_back(i,i,degree);}
        Eigen::SparseMatrix<double> matrix(n,n);matrix.setFromTriplets(entries.begin(),entries.end());Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solve(matrix);
        if(solve.info()!=Eigen::Success)throw std::runtime_error("Sparse curve factorization failed");Eigen::MatrixXd field=solve.solve(rhs);
        diagnostic.residual=(matrix*field-rhs).norm()/std::max(1.0,rhs.norm());diagnostic.solved=true;
        if(!field.allFinite() || diagnostic.residual>1e-7)throw std::runtime_error("Sparse curve harmonic reconstruction failed");
        for(int i=0;i<n;++i){int p=free[size_t(i)];for(int k=family?1:0;k<(family?3:1);++k)output[size_t(k)].at(b.x1+p%width,b.y1+p/width)=float(field(i,k));index[size_t(p)]=-1;}
        result.solves.push_back(diagnostic);
      }
    }
  }
  auto composite=channels(result.composite.view());
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)for(int k=0;k<3;++k){double sum=0;
    for(int p=0;p<plates.count();++p)sum+=plates.alpha(p).at(x,y)*channels(static_cast<const OwnedYabPlanes &>(result.appearance[size_t(p)]).view())[size_t(k)].at(x,y);
    composite[size_t(k)].at(x,y)=float(sum);}
  return result;
}
}
