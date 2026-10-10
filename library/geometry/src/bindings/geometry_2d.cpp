// Owns exact-predicate 2D geometry exports, using approximate constructed coordinates.
#include <kyna/native/adapter.hpp>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/intersections.h>
namespace {
using namespace kyna::adapter;using Kernel=CGAL::Exact_predicates_inexact_constructions_kernel;using Point=Kernel::Point_2;
struct State {kyna_native_host_v2 host{};Resources resources;};
Point point(const Value &v){if(v.size!=2)throw std::runtime_error("point requires two coordinates");return {finite(v.items[0]),finite(v.items[1])};}
Value encode(Buffers &b,const Point &p){return b.array({real(CGAL::to_double(p.x())),real(CGAL::to_double(p.y()))});}
constexpr Type three[]{numbers,numbers,numbers},two[]{numbers,numbers},four[]{numbers,numbers,numbers,numbers},one[]{matrix};
kyna_native_result_v2 orientation(void*,const Value*a,size_t) noexcept{return guarded([&](Buffers&){return whole(int(CGAL::orientation(point(a[0]),point(a[1]),point(a[2]))));});}
kyna_native_result_v2 distance(void*,const Value*a,size_t) noexcept{return guarded([&](Buffers&){return real(std::sqrt(CGAL::to_double(CGAL::squared_distance(point(a[0]),point(a[1])))));});}
kyna_native_result_v2 intersection(void*,const Value*a,size_t) noexcept{return guarded([&](Buffers &b){auto result=CGAL::intersection(Kernel::Segment_2(point(a[0]),point(a[1])),Kernel::Segment_2(point(a[2]),point(a[3])));if(!result)return b.array({});if(auto p=std::get_if<Point>(&*result))return b.array({encode(b,*p)});auto s=std::get<Kernel::Segment_2>(*result);return b.array({encode(b,s.source()),encode(b,s.target())});});}
kyna_native_result_v2 hull(void*,const Value*a,size_t) noexcept{return guarded([&](Buffers &b){std::vector<Point> input,output;for(size_t i=0;i<a[0].size;++i)input.push_back(point(a[0].items[i]));CGAL::convex_hull_2(input.begin(),input.end(),std::back_inserter(output));std::vector<Value> values;for(auto &p:output)values.push_back(encode(b,p));return b.array(std::move(values));});}
}
// The 3D module supplies the combined descriptor and shared context.
extern const kyna_native_function_v2 kyna_geometry_2d_functions[]{ {"geometryOrientation",three,3,integer,orientation},{"geometryDistance",two,2,number,distance},{"geometryIntersection",four,4,matrix,intersection},{"geometryHull",one,1,matrix,hull} };
