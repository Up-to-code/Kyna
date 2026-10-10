// Owns typed mlpack regression exports and model resources.
#include <kyna/native/adapter.hpp>
#include <mlpack/core.hpp>
#include <mlpack/methods/linear_regression/linear_regression.hpp>
namespace {
using namespace kyna::adapter;
struct State { kyna_native_host_v2 host{}; Resources resources; };
constexpr Type model{KYNA_V2_RESOURCE,0,nullptr,"RegressionModel"};
constexpr Type training[]{matrix,numbers};constexpr Type prediction[]{model,matrix};constexpr Type disposal[]{model};
arma::mat features(const Value &v) {
  if(!v.size)throw std::runtime_error("features must contain at least one feature row");
  auto columns=v.items[0].size;if(!columns)throw std::runtime_error("features must contain observations");
  if(columns>10000000||v.size>10000000/columns)throw std::runtime_error("feature matrix exceeds ten million values");
  arma::mat result(v.size,columns);
  for(size_t row=0;row<v.size;++row){if(v.items[row].size!=columns)throw std::runtime_error("feature rows must have equal length");for(size_t col=0;col<columns;++col)result(row,col)=finite(v.items[row].items[col]);}return result;
}
kyna_native_result_v2 train(void *p,const Value *a,size_t) noexcept {return guarded([&](Buffers&){auto x=features(a[0]);if(a[1].size!=x.n_cols)throw std::runtime_error("response count must equal observation count");arma::rowvec y(a[1].size);for(size_t i=0;i<a[1].size;++i)y(i)=finite(a[1].items[i]);auto m=std::make_shared<mlpack::LinearRegression<>>(x,y);if(!m->Parameters().is_finite())throw std::runtime_error("regression produced non-finite model parameters");return static_cast<State*>(p)->resources.put("RegressionModel",m);});}
kyna_native_result_v2 predict(void *p,const Value *a,size_t) noexcept {return guarded([&](Buffers &b){auto m=static_cast<State*>(p)->resources.get<mlpack::LinearRegression<>>(a[0],"RegressionModel");auto x=features(a[1]);if(x.n_rows+1!=m->Parameters().n_rows)throw std::runtime_error("prediction feature count differs from training");arma::rowvec y;m->Predict(x,y);if(!y.is_finite())throw std::runtime_error("prediction produced non-finite output");std::vector<Value> values;for(double n:y)values.push_back(real(n));return b.array(std::move(values));});}
kyna_native_result_v2 dispose(void *p,const Value *a,size_t) noexcept {return guarded([&](Buffers&){static_cast<State*>(p)->resources.entries.erase(a[0].handle);return Value{};});}
const kyna_native_function_v2 functions[]{ {"mlTrain",training,2,model,train},{"mlPredict",prediction,2,numbers,predict},{"mlDispose",disposal,1,nothing,dispose} };
const kyna_native_module_v2_descriptor module{2,sizeof(module),"ml","1.0.16",functions,3,create<State>,close<State>,valid<State>,destroy<State>};
}
extern "C" KYNA_NATIVE_EXPORT const kyna_native_module_v2_descriptor *kyna_native_module_v2(){return &module;}
