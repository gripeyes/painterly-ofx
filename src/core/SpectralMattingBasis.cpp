#include "core/SpectralMattingBasis.h"

#include <Spectra/SymEigsSolver.h>
#include <Spectra/SymEigsShiftSolver.h>
#include <Eigen/SparseCholesky>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pigment {
namespace {

constexpr double kTiny = 1.0e-14;

struct SparseSymmetricMatrix {
  int size = 0;
  std::vector<int> offsets;
  std::vector<int> columns;
  std::vector<double> values;
};

double median(std::vector<double> values) {
  if (values.empty()) return 0.0;
  auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), middle, values.end());
  return *middle;
}

std::array<double, 3> robustScale(const std::vector<YabPixel>& image) {
  std::array<std::vector<double>, 3> channels;
  for (auto& channel : channels) channel.reserve(image.size());
  for (const auto& value : image) {
    channels[0].push_back(value.y); channels[1].push_back(value.a);
    channels[2].push_back(value.b);
  }
  std::array<double, 3> result{};
  for (int channel = 0; channel < 3; ++channel) {
    const double center = median(channels[channel]);
    for (double& value : channels[channel]) value = std::abs(value - center);
    result[channel] = std::max(1.0e-6, 1.4826 * median(channels[channel]));
  }
  return result;
}

bool inverse3x3(const double m[9], double inverse[9]) {
  const double c00 = m[4] * m[8] - m[5] * m[7];
  const double c01 = m[2] * m[7] - m[1] * m[8];
  const double c02 = m[1] * m[5] - m[2] * m[4];
  const double c10 = m[5] * m[6] - m[3] * m[8];
  const double c11 = m[0] * m[8] - m[2] * m[6];
  const double c12 = m[2] * m[3] - m[0] * m[5];
  const double c20 = m[3] * m[7] - m[4] * m[6];
  const double c21 = m[1] * m[6] - m[0] * m[7];
  const double c22 = m[0] * m[4] - m[1] * m[3];
  const double determinant = m[0] * c00 + m[1] * c10 + m[2] * c20;
  if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-20) return false;
  const double scale = 1.0 / determinant;
  const double cofactors[9]{c00,c01,c02,c10,c11,c12,c20,c21,c22};
  for (int i = 0; i < 9; ++i) inverse[i] = cofactors[i] * scale;
  return true;
}

SparseSymmetricMatrix buildMattingLaplacian(const std::vector<YabPixel>& image,
                                             int width, int height) {
  const int count = width * height;
  std::vector<std::vector<std::pair<int, double>>> rows(static_cast<size_t>(count));
  const auto scale = robustScale(image);
  std::vector<std::array<double, 3>> conditioned(static_cast<size_t>(count));
  for (int index = 0; index < count; ++index) {
    conditioned[index] = {image[index].y / scale[0], image[index].a / scale[1],
                          image[index].b / scale[2]};
  }
  // The reference formulation adds epsilon / |window| to the covariance.
  // Robust-normalized YAB has a much smaller noise floor than display RGB;
  // this conditioning term prevents 8-bit/JPEG block noise from becoming a
  // near-null matte mode without smoothing either the image or the mattes.
  constexpr double regularization = 1.0e-5 / 9.0;
  if (width < 3 || height < 3) {
    SparseSymmetricMatrix identity; identity.size = count;
    identity.offsets.resize(static_cast<size_t>(count) + 1);
    for (int i = 0; i < count; ++i) {
      identity.offsets[i] = i; identity.columns.push_back(i);
      identity.values.push_back(i == 0 ? 0.0 : 1.0);
    }
    identity.offsets[count] = count; return identity;
  }
  for (int cy = 1; cy + 1 < height; ++cy) {
    for (int cx = 1; cx + 1 < width; ++cx) {
      std::array<int, 9> indices{};
      std::array<std::array<double, 3>, 9> colors{};
      std::array<double, 3> mean{};
      int cursor = 0;
      for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
        const int index = (cy + oy) * width + cx + ox;
        indices[cursor] = index; colors[cursor] = conditioned[index];
        for (int c = 0; c < 3; ++c) mean[c] += colors[cursor][c] / 9.0;
        ++cursor;
      }
      double covariance[9]{};
      for (const auto& color : colors) for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
          covariance[r*3+c] += (color[r]-mean[r])*(color[c]-mean[c]) / 9.0;
      covariance[0] += regularization; covariance[4] += regularization;
      covariance[8] += regularization;
      double inverse[9]{};
      if (!inverse3x3(covariance, inverse)) {
        covariance[0] += 1.0e-5; covariance[4] += 1.0e-5; covariance[8] += 1.0e-5;
        inverse3x3(covariance, inverse);
      }
      for (int i = 0; i < 9; ++i) for (int j = 0; j < 9; ++j) {
        double quadratic = 0.0;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c)
          quadratic += (colors[i][r]-mean[r]) * inverse[r*3+c] *
                       (colors[j][c]-mean[c]);
        const double value = (i == j ? 1.0 : 0.0) - (1.0 + quadratic) / 9.0;
        rows[indices[i]].push_back({indices[j], value});
      }
    }
  }
  SparseSymmetricMatrix matrix; matrix.size = count;
  matrix.offsets.resize(static_cast<size_t>(count) + 1);
  for (int row = 0; row < count; ++row) {
    auto& entries = rows[row];
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.first < rhs.first;
    });
    matrix.offsets[row] = static_cast<int>(matrix.columns.size());
    for (size_t first = 0; first < entries.size();) {
      size_t last = first + 1; double value = entries[first].second;
      while (last < entries.size() && entries[last].first == entries[first].first)
        value += entries[last++].second;
      if (std::abs(value) > 1.0e-15) {
        matrix.columns.push_back(entries[first].first); matrix.values.push_back(value);
      }
      first = last;
    }
  }
  matrix.offsets[count] = static_cast<int>(matrix.columns.size());
  return matrix;
}

SparseSymmetricMatrix addInformationFlow(const SparseSymmetricMatrix& matte,
                                         const SparseAffinityGraph& graph,
                                         double strength) {
  if (strength <= 0.0 || graph.nodeCount() != matte.size) return matte;
  std::vector<std::unordered_map<int,double>> rows(static_cast<size_t>(matte.size));
  for (int row = 0; row < matte.size; ++row)
    for (int edge = matte.offsets[row]; edge < matte.offsets[row+1]; ++edge)
      rows[row][matte.columns[edge]] += matte.values[edge];
  std::vector<std::pair<int,double>> stencil;
  for (int p = 0; p < matte.size; ++p) {
    stencil.clear(); stencil.emplace_back(p,1.0);
    for (int edge = graph.rowOffsets[p]; edge < graph.rowOffsets[p+1]; ++edge) {
      const auto& value = graph.edges[edge];
      if(std::abs(value.signedMixtureWeight)>1.0e-12f)
        stencil.emplace_back(value.target,-value.signedMixtureWeight);
    }
    for (const auto& lhs : stencil) for (const auto& rhs : stencil)
      rows[lhs.first][rhs.first] += strength * lhs.second * rhs.second;
  }
  SparseSymmetricMatrix result; result.size = matte.size;
  result.offsets.resize(static_cast<size_t>(matte.size)+1);
  for (int row = 0; row < matte.size; ++row) {
    std::vector<std::pair<int,double>> entries(rows[row].begin(),rows[row].end());
    std::sort(entries.begin(),entries.end(),[](const auto& lhs,const auto& rhs){
      return lhs.first < rhs.first;
    });
    result.offsets[row] = static_cast<int>(result.columns.size());
    for (const auto& entry : entries) if (std::abs(entry.second) > 1.0e-15) {
      result.columns.push_back(entry.first); result.values.push_back(entry.second);
    }
  }
  result.offsets[matte.size] = static_cast<int>(result.columns.size());
  return result;
}

void multiply(const SparseSymmetricMatrix& matrix, const std::vector<double>& input,
              int columns, std::vector<double>& output) {
  output.assign(static_cast<size_t>(matrix.size) * columns, 0.0);
  for (int row = 0; row < matrix.size; ++row) {
    for (int edge = matrix.offsets[row]; edge < matrix.offsets[row+1]; ++edge) {
      const int column = matrix.columns[edge]; const double weight = matrix.values[edge];
      for (int k = 0; k < columns; ++k)
        output[static_cast<size_t>(k)*matrix.size+row] +=
            weight * input[static_cast<size_t>(k)*matrix.size+column];
    }
  }
}

int orthonormalize(std::vector<double>& vectors, int size, int columns,
                   const std::vector<double>* against = nullptr, int againstColumns = 0) {
  int retained = 0;
  for (int column = 0; column < columns; ++column) {
    double* value = vectors.data() + static_cast<size_t>(column) * size;
    if (against) for (int previous = 0; previous < againstColumns; ++previous) {
      const double* basis = against->data() + static_cast<size_t>(previous) * size;
      double dot = 0.0; for (int i = 0; i < size; ++i) dot += value[i] * basis[i];
      for (int i = 0; i < size; ++i) value[i] -= dot * basis[i];
    }
    for (int previous = 0; previous < retained; ++previous) {
      const double* basis = vectors.data() + static_cast<size_t>(previous) * size;
      double dot = 0.0; for (int i = 0; i < size; ++i) dot += value[i] * basis[i];
      for (int i = 0; i < size; ++i) value[i] -= dot * basis[i];
    }
    // A second pass prevents loss of orthogonality in clustered null spaces.
    for (int previous = 0; previous < retained; ++previous) {
      const double* basis = vectors.data() + static_cast<size_t>(previous) * size;
      double dot = 0.0; for (int i = 0; i < size; ++i) dot += value[i] * basis[i];
      for (int i = 0; i < size; ++i) value[i] -= dot * basis[i];
    }
    double norm = 0.0; for (int i = 0; i < size; ++i) norm += value[i] * value[i];
    if (norm < 1.0e-18 || !std::isfinite(norm)) continue;
    const double inverse = 1.0 / std::sqrt(norm);
    if (retained != column)
      std::copy(value, value + size, vectors.data() + static_cast<size_t>(retained) * size);
    value = vectors.data() + static_cast<size_t>(retained) * size;
    for (int i = 0; i < size; ++i) value[i] *= inverse;
    ++retained;
  }
  vectors.resize(static_cast<size_t>(size) * retained); return retained;
}

void completeBasis(std::vector<double>& vectors, int size, int wanted, int salt) {
  int retained = static_cast<int>(vectors.size() / std::max(1, size));
  int attempt = 0;
  while (retained < wanted) {
    vectors.resize(static_cast<size_t>(size) * wanted, 0.0);
    for (int column = retained; column < wanted; ++column) {
      const int impulse = (column + salt*wanted + attempt*wanted) % size;
      vectors[static_cast<size_t>(column)*size+impulse] = 1.0;
    }
    retained = orthonormalize(vectors,size,wanted);
    if (++attempt > 16) break;
  }
}

void jacobiEigen(std::vector<double> matrix, int size, std::vector<double>& values,
                 std::vector<double>& vectors) {
  vectors.assign(static_cast<size_t>(size) * size, 0.0);
  for (int i = 0; i < size; ++i) vectors[static_cast<size_t>(i)*size+i] = 1.0;
  for (int sweep = 0; sweep < 40; ++sweep) {
    double maximum = 0.0;
    for (int p = 0; p < size; ++p) for (int q = p + 1; q < size; ++q) {
      const double apq = matrix[static_cast<size_t>(p)*size+q];
      maximum = std::max(maximum, std::abs(apq));
      if (std::abs(apq) < 1.0e-13) continue;
      const double app = matrix[static_cast<size_t>(p)*size+p];
      const double aqq = matrix[static_cast<size_t>(q)*size+q];
      const double angle = 0.5 * std::atan2(2.0 * apq, aqq - app);
      const double c = std::cos(angle), s = std::sin(angle);
      for (int k = 0; k < size; ++k) if (k != p && k != q) {
        const double mkp = matrix[static_cast<size_t>(k)*size+p];
        const double mkq = matrix[static_cast<size_t>(k)*size+q];
        const double rp = c * mkp - s * mkq, rq = s * mkp + c * mkq;
        matrix[static_cast<size_t>(k)*size+p] = matrix[static_cast<size_t>(p)*size+k] = rp;
        matrix[static_cast<size_t>(k)*size+q] = matrix[static_cast<size_t>(q)*size+k] = rq;
      }
      matrix[static_cast<size_t>(p)*size+p] = c*c*app - 2*s*c*apq + s*s*aqq;
      matrix[static_cast<size_t>(q)*size+q] = s*s*app + 2*s*c*apq + c*c*aqq;
      matrix[static_cast<size_t>(p)*size+q] = matrix[static_cast<size_t>(q)*size+p] = 0.0;
      for (int k = 0; k < size; ++k) {
        const double vkp = vectors[static_cast<size_t>(k)*size+p];
        const double vkq = vectors[static_cast<size_t>(k)*size+q];
        vectors[static_cast<size_t>(k)*size+p] = c*vkp-s*vkq;
        vectors[static_cast<size_t>(k)*size+q] = s*vkp+c*vkq;
      }
    }
    if (maximum < 1.0e-11) break;
  }
  values.resize(size); for (int i = 0; i < size; ++i) values[i] = matrix[static_cast<size_t>(i)*size+i];
  std::vector<int> order(size); std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return values[a] < values[b]; });
  std::vector<double> sortedValues(size), sortedVectors(vectors.size());
  for (int column = 0; column < size; ++column) {
    sortedValues[column] = values[order[column]];
    for (int row = 0; row < size; ++row)
      sortedVectors[static_cast<size_t>(row)*size+column] = vectors[static_cast<size_t>(row)*size+order[column]];
  }
  values.swap(sortedValues); vectors.swap(sortedVectors);
}

SpectralMattingBasis smallestEigenvectors(const SparseSymmetricMatrix& matrix, int wanted,
                                           int width, int height,
                                           const ExecutionContext& execution) {
  class MatrixOperation {
   public:
    using Scalar = double;
    explicit MatrixOperation(const SparseSymmetricMatrix& value) : matrix_(value) {}
    int rows() const noexcept { return matrix_.size; }
    int cols() const noexcept { return matrix_.size; }
    void perform_op(const double* input, double* output) const {
      for(int row=0;row<matrix_.size;++row){double sum=0;for(int edge=matrix_.offsets[row];edge<matrix_.offsets[row+1];++edge)sum+=matrix_.values[edge]*input[matrix_.columns[edge]];output[row]=sum;}
    }
   private:
    const SparseSymmetricMatrix& matrix_;
  } operation(matrix);
  class ShiftOperation {
   public:
    using Scalar = double;
    explicit ShiftOperation(const SparseSymmetricMatrix& value) : size_(value.size) {
      std::vector<Eigen::Triplet<double>> entries;entries.reserve(value.values.size());
      for(int row=0;row<value.size;++row)for(int edge=value.offsets[row];edge<value.offsets[row+1];++edge)entries.emplace_back(row,value.columns[edge],value.values[edge]);
      matrix_.resize(size_,size_);matrix_.setFromTriplets(entries.begin(),entries.end());matrix_.makeCompressed();
    }
    int rows() const noexcept{return size_;}int cols() const noexcept{return size_;}
    void set_shift(double shift){Eigen::SparseMatrix<double> shifted=matrix_;for(int i=0;i<size_;++i)shifted.coeffRef(i,i)-=shift;shifted.makeCompressed();solver_.compute(shifted);}
    void perform_op(const double*input,double*output)const{Eigen::Map<const Eigen::VectorXd>source(input,size_);Eigen::Map<Eigen::VectorXd>destination(output,size_);destination=solver_.solve(source);}
   private:
    int size_=0;Eigen::SparseMatrix<double>matrix_;Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>>solver_;
  } shiftOperation(matrix);
  const int n=matrix.size;wanted=std::max(1,std::min(wanted,n-1));
  const int krylov=std::min(n,std::max(wanted+12,wanted*2+1));
  Spectra::SymEigsShiftSolver<ShiftOperation> solver(shiftOperation,wanted,krylov,-1.0e-7);
  Eigen::VectorXd initial(n);for(int i=0;i<n;++i){uint32_t hash=uint32_t(i+1)*0x9e3779b9u;hash^=hash>>16;initial[i]=double(int(hash&0xffffu)-32768)/32768.0;}
  solver.init(initial.data());
  solver.compute(Spectra::SortRule::LargestMagn,1400,1.0e-12,Spectra::SortRule::SmallestAlge);
  if(execution.cancelled()||solver.info()!=Spectra::CompInfo::Successful)return {};
  const Eigen::VectorXd eigenvalues=solver.eigenvalues();const Eigen::MatrixXd eigenvectors=solver.eigenvectors();
  SpectralMattingBasis result;result.width=width;result.height=height;result.count=int(eigenvalues.size());result.values.resize(size_t(n)*result.count);result.eigenvalues.resize(result.count);result.residuals.resize(result.count);std::vector<double>product(n);
  for(int mode=0;mode<result.count;++mode){result.eigenvalues[mode]=float(eigenvalues[mode]);operation.perform_op(eigenvectors.col(mode).data(),product.data());double rr=0;for(int p=0;p<n;++p){double value=eigenvectors(p,mode);result.values[size_t(mode)*n+p]=float(value);double difference=product[p]-eigenvalues[mode]*value;rr+=difference*difference;}result.residuals[mode]=float(std::sqrt(rr));}
  for(int i=1;i<result.count;++i)for(int j=i+1;j<result.count;++j){double mi=eigenvectors.col(i).mean(),mj=eigenvectors.col(j).mean(),cross=0,ii=0,jj=0;for(int p=0;p<n;++p){double a=eigenvectors(p,i)-mi,b=eigenvectors(p,j)-mj;cross+=a*b;ii+=a*a;jj+=b*b;}result.maximumAbsoluteCorrelation=std::max(result.maximumAbsoluteCorrelation,float(std::abs(cross)/std::sqrt(std::max(kTiny,ii*jj))));}
  return result;
}

}  // namespace

SpectralMattingBasis buildSpectralMattingBasis(const std::vector<YabPixel>& image,
                                                int width, int height, int modeCount,
                                                const ExecutionContext& execution,
                                                const SparseAffinityGraph* informationFlow,
                                                float informationFlowWeight) {
  if (width <= 0 || height <= 0 || int(image.size()) != width * height) return {};
  auto matrix=buildMattingLaplacian(image,width,height);
  if(informationFlow)matrix=addInformationFlow(matrix,*informationFlow,informationFlowWeight);
  return smallestEigenvectors(matrix,modeCount,width,height,execution);
}

}  // namespace pigment
