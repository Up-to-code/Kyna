// Owns a deterministic ABI 2 fixture for retained callbacks and resource cleanup.
#include <kyna/native/adapter.hpp>
#include <thread>
namespace {
using namespace kyna::adapter;
struct State {kyna_native_host_v2 host{};Resources resources;uint64_t callback{};int released{};~State(){if(callback)host.release_callback(host.context,callback);}};
constexpr Type resource{KYNA_V2_RESOURCE,0,nullptr,"CounterResource"};constexpr Type cb{KYNA_V2_CALLBACK,0,nullptr,nullptr,nullptr,0,&integer};constexpr Type cbArgs[]{cb},resourceArgs[]{resource};
kyna_native_result_v2 keep(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);if(s->callback)s->host.release_callback(s->host.context,s->callback);if(!s->host.retain_callback(s->host.context,a[0].handle))throw std::runtime_error("retain failed");s->callback=a[0].handle;return Value{};});}
kyna_native_result_v2 fire(void*p,const Value*,size_t) noexcept{auto s=static_cast<State*>(p);return s->host.invoke_callback(s->host.context,s->callback,nullptr,0);}
kyna_native_result_v2 wrongThread(void*p,const Value*,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);bool refused=false;std::thread t([&]{auto r=s->host.invoke_callback(s->host.context,s->callback,nullptr,0);refused=r.error_code&&std::string(r.error_code)=="KNATIVE1007";if(r.release)r.release(r.context);});t.join();return truth(refused);});}
kyna_native_result_v2 make(void*p,const Value*,size_t) noexcept{return guarded([&](Buffers&){return static_cast<State*>(p)->resources.put("CounterResource",std::make_shared<int>(42));});}
kyna_native_result_v2 read(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){return whole(*static_cast<State*>(p)->resources.get<int>(a[0],"CounterResource"));});}
kyna_native_result_v2 dispose(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){static_cast<State*>(p)->resources.entries.erase(a[0].handle);return Value{};});}
const kyna_native_function_v2 functions[]{ {"retainTestCallback",cbArgs,1,nothing,keep},{"fireTestCallback",nullptr,0,integer,fire},{"wrongThreadTest",nullptr,0,boolean,wrongThread},{"createTestResource",nullptr,0,resource,make},{"readTestResource",resourceArgs,1,integer,read},{"disposeTestResource",resourceArgs,1,nothing,dispose} };
const kyna_native_module_v2_descriptor module{2,sizeof(module),"fixture","1.0.16",functions,6,create<State>,close<State>,valid<State>,destroy<State>};
}
extern "C" KYNA_NATIVE_EXPORT const kyna_native_module_v2_descriptor *kyna_native_module_v2(){return &module;}
