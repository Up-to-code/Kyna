// Owns direct C++ / Kyna-native regression parity and conversion-overhead measurements.
#include <kyna/language/native_modules.hpp>
#include <mlpack/core.hpp>
#include <mlpack/methods/linear_regression/linear_regression.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
int main(int argc,char **argv){
 assert(argc==2);auto functions=kyna::loadNativeModule(argv[1]);kyna::Heap heap;auto roots=heap.rootScope();
 auto array=[&](std::initializer_list<double> numbers){auto p=heap.allocateArray();for(double n:numbers)p->elements.emplace_back(n);return kyna::Value(p);};
 auto feature=heap.allocateArray();feature->elements.push_back(array({1,2,3}));kyna::Value x(feature),y=array({3,5,7});roots.protect(x);roots.protect(y);
 kyna::Value args[]{x,y};auto trained=kyna::invokeNativeFunction(functions[0],args,heap,{});assert(!trained.failure);roots.protect(trained.value);
 auto test=heap.allocateArray();test->elements.push_back(array({4,5}));kyna::Value data(test);roots.protect(data);kyna::Value prediction[]{trained.value,data};
 arma::mat cppX(1,3);cppX.row(0)=arma::rowvec{1,2,3};arma::rowvec cppY{3,5,7};mlpack::LinearRegression<> direct(cppX,cppY);arma::mat cppTest(1,2);cppTest.row(0)=arma::rowvec{4,5};arma::rowvec cppOutput;direct.Predict(cppTest,cppOutput);
 auto converted=kyna::invokeNativeFunction(functions[1],prediction,heap,{});assert(!converted.failure);auto values=std::get<kyna::ArrayPtr>(converted.value.data);for(size_t i=0;i<2;++i)assert(std::abs(std::get<double>(values->elements[i].data)-cppOutput(i))<1e-12);
 constexpr int count=1000;auto begin=std::chrono::steady_clock::now();for(int i=0;i<count;++i)direct.Predict(cppTest,cppOutput);auto cppTime=std::chrono::steady_clock::now()-begin;
 begin=std::chrono::steady_clock::now();for(int i=0;i<count;++i){auto output=kyna::invokeNativeFunction(functions[1],prediction,heap,{});assert(!output.failure);}auto nativeTime=std::chrono::steady_clock::now()-begin;
 std::cout<<"{\"schema\":\"kyna.native-benchmark/v1\",\"iterations\":"<<count<<",\"cpp_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(cppTime).count()<<",\"native_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(nativeTime).count()<<",\"correct\":true}\n";
 for(auto &f:functions)if(f.shutdown)f.shutdown();
}
