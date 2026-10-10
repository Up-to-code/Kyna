// Owns triangle-mesh loading, validation, measurements, and opaque mesh lifetime.
#include <kyna/native/adapter.hpp>
#include <array>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
extern const kyna_native_function_v2 kyna_geometry_2d_functions[];
namespace {
using namespace kyna::adapter;using K=CGAL::Exact_predicates_inexact_constructions_kernel;using Mesh=CGAL::Surface_mesh<K::Point_3>;namespace PMP=CGAL::Polygon_mesh_processing;
struct State{kyna_native_host_v2 host{};Resources resources;};
constexpr Type mesh{KYNA_V2_RESOURCE,0,nullptr,"TriangleMesh"};constexpr Type loadArgs[]{text},meshArgs[]{mesh};
kyna_native_result_v2 load(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto m=std::make_shared<Mesh>();if(!CGAL::IO::read_polygon_mesh(string(a[0]),*m)||!CGAL::is_valid_polygon_mesh(*m)||!CGAL::is_triangle_mesh(*m))throw std::runtime_error("OFF input must be a valid triangle mesh");for(auto v:m->vertices()){auto point=m->point(v);if(!std::isfinite(CGAL::to_double(point.x()))||!std::isfinite(CGAL::to_double(point.y()))||!std::isfinite(CGAL::to_double(point.z())))throw std::runtime_error("mesh coordinates must be finite");}return static_cast<State*>(p)->resources.put("TriangleMesh",m);});}
kyna_native_result_v2 inspect(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers &b){auto m=static_cast<State*>(p)->resources.get<Mesh>(a[0],"TriangleMesh");bool self=PMP::does_self_intersect(*m),closed=CGAL::is_closed(*m);double volume=0;if(closed&&!self)volume=CGAL::to_double(PMP::volume(*m));double lo[3]{INFINITY,INFINITY,INFINITY},hi[3]{-INFINITY,-INFINITY,-INFINITY};for(auto v:m->vertices()){auto p=m->point(v);double c[]{CGAL::to_double(p.x()),CGAL::to_double(p.y()),CGAL::to_double(p.z())};for(int i=0;i<3;++i){lo[i]=std::min(lo[i],c[i]);hi[i]=std::max(hi[i],c[i]);}}if(m->is_empty())throw std::runtime_error("mesh is empty");return b.array({real(double(m->number_of_vertices())),real(double(m->number_of_faces())),real(closed),real(self),real(CGAL::to_double(PMP::area(*m))),real(volume),real(lo[0]),real(lo[1]),real(lo[2]),real(hi[0]),real(hi[1]),real(hi[2])});});}
kyna_native_result_v2 volume(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto m=static_cast<State*>(p)->resources.get<Mesh>(a[0],"TriangleMesh");if(!CGAL::is_closed(*m)||PMP::does_self_intersect(*m))throw std::runtime_error("volume requires a closed non-self-intersecting mesh");return real(CGAL::to_double(PMP::volume(*m)));});}
const auto functions=[] {std::array<kyna_native_function_v2,7> f{};for(int i=0;i<4;++i)f[i]=kyna_geometry_2d_functions[i];f[4]={"geometryLoadMesh",loadArgs,1,mesh,load};f[5]={"geometryInspectMesh",meshArgs,1,numbers,inspect};f[6]={"geometryVolume",meshArgs,1,number,volume};return f;}();
const kyna_native_module_v2_descriptor module{2,sizeof(module),"geometry","1.0.16",functions.data(),functions.size(),create<State>,close<State>,valid<State>,destroy<State>};
}
extern "C" KYNA_NATIVE_EXPORT const kyna_native_module_v2_descriptor *kyna_native_module_v2(){return &module;}
