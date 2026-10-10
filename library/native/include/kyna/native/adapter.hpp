// Shared ABI-only utilities for optional native adapters. No VM objects cross this boundary.
#pragma once
#include <kyna/language/native_abi_v2.h>
#include <cmath>
#include <deque>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
namespace kyna::adapter {
using Value = kyna_native_value_v2;
using Type = kyna_native_type_v2;
inline constexpr Type integer{KYNA_V2_INT}, number{KYNA_V2_FLOAT}, text{KYNA_V2_STRING}, boolean{KYNA_V2_BOOL}, nothing{KYNA_V2_VOID};
inline constexpr Type numbers{KYNA_V2_ARRAY,0,&number};
inline constexpr Type matrix{KYNA_V2_ARRAY,0,&numbers};
inline Value real(double n) { Value v{}; v.kind=KYNA_V2_FLOAT; v.number=n; return v; }
inline Value whole(int64_t n) { Value v{}; v.kind=KYNA_V2_INT; v.integer=n; return v; }
inline Value truth(bool b) { Value v{}; v.kind=KYNA_V2_BOOL; v.integer=b; return v; }
inline double finite(const Value &v) { double n=v.kind==KYNA_V2_INT?double(v.integer):v.number; if (!std::isfinite(n)) throw std::runtime_error("numeric input must be finite"); return n; }
inline std::string string(const Value &v) { return std::string(v.text?v.text:"",v.size); }
struct Buffers {
  std::deque<std::string> strings;
  std::deque<std::vector<Value>> arrays;
  std::string code,message;
  Value array(std::vector<Value> a) { arrays.push_back(std::move(a)); Value v{};v.kind=KYNA_V2_ARRAY;v.items=arrays.back().data();v.size=arrays.back().size();return v; }
  Value string(std::string s) { strings.push_back(std::move(s));Value v{};v.kind=KYNA_V2_STRING;v.text=strings.back().data();v.size=strings.back().size();return v; }
};
struct Failure : std::runtime_error {
  std::string code;
  Failure(std::string code, std::string message) : std::runtime_error(std::move(message)), code(std::move(code)) {}
};
template<class F> kyna_native_result_v2 guarded(F work) noexcept {
  try {
    auto b=std::make_unique<Buffers>(); kyna_native_result_v2 r{};
    try { r.value=work(*b); } catch(const Failure &e) { b->code=e.code;b->message=e.what(); } catch(const std::exception &e) { b->code="KNATIVE1005";b->message=e.what(); } catch(...) { b->code="KNATIVE1005";b->message="unknown C++ exception"; }
    if(!b->code.empty()){r.error_code=b->code.c_str();r.error_message=b->message.c_str();}
    r.context=b.release();r.release=[](void *p){delete static_cast<Buffers*>(p);};return r;
  } catch(...) { kyna_native_result_v2 r{};r.error_code="KNATIVE1005";r.error_message="native result allocation failed";return r; }
}
struct Resources {
  struct Entry { std::string type; std::shared_ptr<void> object; };
  std::unordered_map<uint64_t,Entry> entries;uint64_t next{1};
  template<class T> Value put(std::string type,std::shared_ptr<T> object) { if(next==std::numeric_limits<uint64_t>::max())throw std::runtime_error("resource handle space exhausted");auto id=next++;auto [it,_]=entries.emplace(id,Entry{std::move(type),std::move(object)});Value v{};v.kind=KYNA_V2_RESOURCE;v.handle=id;v.opaque_type=it->second.type.c_str();return v; }
  template<class T> std::shared_ptr<T> get(const Value &v,const char *type) { auto it=entries.find(v.handle);if(it==entries.end()||it->second.type!=type)throw std::runtime_error("stale or foreign resource");return std::static_pointer_cast<T>(it->second.object); }
  bool valid(uint64_t id,const char *type) const {auto it=entries.find(id);return it!=entries.end()&&type&&it->second.type==type;}
};
template<class State> void *create(const kyna_native_host_v2 *host) noexcept {try {if(!host||host->abi_version!=2||host->struct_size!=sizeof(*host))return nullptr;auto s=new State;s->host=*host;return s;}catch(...){return nullptr;}}
template<class State> void close(void *p) noexcept {delete static_cast<State*>(p);}
template<class State> int valid(void *p,uint64_t id,const char *type) noexcept {try{return static_cast<State*>(p)->resources.valid(id,type);}catch(...){return 0;}}
template<class State> void destroy(void *p,uint64_t id) noexcept {static_cast<State*>(p)->resources.entries.erase(id);}
}
