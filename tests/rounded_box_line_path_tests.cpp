#include "sim/rounded_box_line_path.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
using namespace JPH;
namespace {
constexpr double pi=3.1415926535897932384626433832795;
double max_endpoint_derivative_error=0;
double max_surface_roundtrip_error=0;
void require(bool ok,const char *what) { if(!ok) { std::cerr<<"FAIL "<<what<<'\n'; std::exit(1); } }
void near(double a,double b,double tol,const char *what) { if(std::abs(a-b)>tol) { std::cerr<<a<<" != "<<b<<' '; require(false,what); } }
RoundedBoxLineQuery beam() {
    RoundedBoxLineQuery q;
    BoxShape actual(Vec3(1,1,5),.2F);
    q.half_extent=actual.GetHalfExtent(); q.convex_radius=actual.GetConvexRadius();
    q.endpoint_a=RVec3(-3,0,-.7F); q.endpoint_b=RVec3(3,0,.8F); q.retained={2,1};
    return q;
}
void check_surface(const RoundedBoxLineQuery &q,const RoundedBoxLineResult &r) {
    require(r.status==RoundedBoxLineStatus::Wrapped,"surface query wrapped");
    const auto inverse=q.leaf_to_world.InversedRotationTranslation();
    const Vec3 inner=q.half_extent-Vec3::sReplicate(float(q.convex_radius));
    for(std::uint32_t i=0;i<r.surface_point_count;++i) {
        const Vec3 p(inverse*r.surface_points[i]);
        const Vec3 d=Vec3::sMax(p.Abs()-inner,Vec3::sZero());
        // Presentation world samples and Jolt's inverse use float arithmetic.
        // Bound their transform roundoff by world-coordinate magnitude.
        const double roundoff=8*std::numeric_limits<float>::epsilon()*
            (1+Vec3(r.surface_points[i]).Abs().ReduceMax()+Vec3(q.leaf_to_world.GetTranslation()).Abs().ReduceMax());
        max_surface_roundtrip_error=std::max(max_surface_roundtrip_error,std::abs(d.Length()-q.convex_radius));
        near(d.Length(),q.convex_radius,roundoff,"sample on rounded solid surface");
        if(q.convex_radius==0) {
            const int u=(r.topology.axis+1)%3,v=(r.topology.axis+2)%3;
            near(std::min(std::abs(std::abs(p[u])-inner[u]),std::abs(std::abs(p[v])-inner[v])),0,
                 roundoff,"sharp sample lies on actual face, not inside it");
        }
        require(std::abs(p[r.topology.axis])<=inner[r.topology.axis]+2e-6,"finite axial bounds");
    }
    near(r.gradient_a.Length(),1,2e-6,"a gradient unit");
    near(r.gradient_b.Length(),1,2e-6,"b gradient unit");
}
void gradients(RoundedBoxLineQuery q) {
    const auto r=rounded_box_line_path(q); check_surface(q,r);
    q.retained=r.topology;
    constexpr float step=.002F;
    for(int endpoint=0;endpoint<2;++endpoint) for(int axis=0;axis<3;++axis) {
        auto minus=q,plus=q;
        Vec3 offset=Vec3::sZero(); offset.SetComponent(axis,step);
        if(endpoint==0) { minus.endpoint_a-=offset; plus.endpoint_a+=offset; }
        else { minus.endpoint_b-=offset; plus.endpoint_b+=offset; }
        const auto rm=rounded_box_line_path(minus),rp=rounded_box_line_path(plus);
        require(rm.status==RoundedBoxLineStatus::Wrapped && rp.status==RoundedBoxLineStatus::Wrapped,"gradient perturb remains wrapped");
        // Jolt's configured world scalar is float; use the represented endpoint
        // displacement, rather than claiming the requested step survived rounding.
        const double span=endpoint==0 ? double(plus.endpoint_a[axis])-minus.endpoint_a[axis] :
            double(plus.endpoint_b[axis])-minus.endpoint_b[axis];
        const double numerical=(rp.length_m-rm.length_m)/span;
        max_endpoint_derivative_error=std::max(max_endpoint_derivative_error,
            std::abs(numerical-(endpoint==0 ? r.gradient_a[axis]:r.gradient_b[axis])));
        near(numerical,endpoint==0 ? r.gradient_a[axis]:r.gradient_b[axis],1.5e-4,"endpoint derivative");
    }
}
void clear_and_invalid() {
    auto q=beam(); q.endpoint_a=RVec3(-3,2,0); q.endpoint_b=RVec3(3,2,0);
    auto r=rounded_box_line_path(q);
    require(r.status==RoundedBoxLineStatus::Clear && r.surface_point_count==0 && r.topology.axis==-1,"clear releases topology");
    near(r.length_m,6,1e-6,"clear chord length");
    // Both endpoints beyond the rounded cap: cross-section is blocked but
    // actual 3D chord is clear. An infinite extrusion would falsely wrap.
    q.endpoint_a=RVec3(-3,0,5.1F); q.endpoint_b=RVec3(3,0,5.1F);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Clear,"actual finite chord clear");
    q.endpoint_a=RVec3(-3,.99F,4.99F); q.endpoint_b=RVec3(3,.99F,4.99F);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Clear,"rounded edge clears chord inside sharp outer box");
    q.convex_radius=2;
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Invalid,"invalid radius rejected");
    q=beam(); q.leaf_to_world=RMat44::sScale(Vec3(2,1,1));
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Invalid,"scale must be resolved before query");
}
void rounded_wrap() {
    auto q=beam(); const auto r=rounded_box_line_path(q); check_surface(q,r);
    // y=0 symmetric tangent to lower-left/lower-right rounded corners:
    // derive independent tangent lengths and arc angle from corner centers.
    const double radius=q.convex_radius,inner=1-radius;
    const double corner_distance=std::hypot(3-inner,inner);
    const double tangent=std::sqrt(corner_distance*corner_distance-radius*radius);
    const double tangent_angle=std::atan2(inner,-(3-inner))+std::acos(radius/corner_distance);
    const double arc=2*radius*(3*pi/2-tangent_angle);
    const double cross=2*tangent+2*inner+arc;
    near(r.cross_section_length_m,cross,1e-7,"independent exact arc length");
    near(r.length_m,std::hypot(cross,1.5),1e-7,"unrolled 3D geodesic length");
    require(r.boundary_length_m>1.6 && r.boundary_length_m<2.4,"real face plus corner arcs");
    auto auto_query=q; auto_query.retained={};
    const auto selected=rounded_box_line_path(auto_query); check_surface(auto_query,selected);
    require(selected.topology.axis==2,"long beam selects finite extrusion");
    q.endpoint_a=RVec3(-1,0,0); q.endpoint_b=RVec3(3,0,0);
    check_surface(q,rounded_box_line_path(q));
}
void retained_slide_release() {
    auto q=beam(); q.endpoint_a=RVec3(-3,.3F,0); q.endpoint_b=RVec3(3,.3F,0);
    const auto lower=rounded_box_line_path(q); check_surface(q,lower);
    auto shortest=q; shortest.retained={}; const auto upper=rounded_box_line_path(shortest);
    require(lower.length_m>upper.length_m+.1,"retained longer side cannot pop to shorter side");
    for(int i=0;i<20;++i) {
        q.endpoint_a+=Vec3(0,.01F,.02F); q.endpoint_b+=Vec3(0,.005F,-.01F);
        const auto next=rounded_box_line_path(q); check_surface(q,next);
        require(next.topology.axis==2 && next.topology.direction==1,"retained topology slides");
    }
    q.endpoint_a=RVec3(-3,1.1F,0); q.endpoint_b=RVec3(3,1.1F,0);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Clear,"real shortcut releases");
    q=beam(); q.endpoint_a=RVec3(-3,0,4.95F); q.endpoint_b=RVec3(3,0,4.95F);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Unsupported,"rounded endcap unsupported");
    q=beam(); q.endpoint_a=RVec3(0,0,0);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Unsupported,"embedded endpoint unsupported");
}
void sharp_wrap() {
    auto q=beam();
    BoxShape actual(Vec3(1,1,5),0);
    q.half_extent=actual.GetHalfExtent(); q.convex_radius=actual.GetConvexRadius();
    const auto path=rounded_box_line_path(q); check_surface(q,path);
    const double cross=2*std::sqrt(5.0)+2;
    near(path.cross_section_length_m,cross,1e-12,"exact sharp tangent legs plus face length");
    near(path.length_m,std::hypot(cross,1.5),1e-12,"exact sharp 3D extrusion length");
    near(path.boundary_length_m,2,1e-12,"sharp boundary is the actual two-metre face");
    const auto first=path.surface_points[0],last=path.surface_points[path.surface_point_count-1];
    near(first.GetX(),-1,1e-7,"actual lower-left corner identity"); near(first.GetY(),-1,1e-7,"actual first face identity");
    near(last.GetX(),1,1e-7,"actual lower-right corner identity"); near(last.GetY(),-1,1e-7,"actual last face identity");
    gradients(q);
    // The obstacle's outward force on the cable lies inside each real corner's
    // normal cone. This is a vertex contact, not an arbitrary surface pin.
    q.endpoint_a=RVec3(-3,0,0); q.endpoint_b=RVec3(3,0,0);
    const auto flat=rounded_box_line_path(q); check_surface(q,flat);
    const Vec3 normal_a=-(flat.gradient_a+Vec3::sAxisX());
    const Vec3 normal_b=-(flat.gradient_b-Vec3::sAxisX());
    require(normal_a.GetX()<0 && normal_a.GetY()<0 && normal_b.GetX()>0 && normal_b.GetY()<0,
            "contact forces lie in actual outward corner normal cones");
    auto retained=q; retained.endpoint_a+=Vec3(0,.3F,0); retained.endpoint_b+=Vec3(0,.3F,0);
    const auto lower=rounded_box_line_path(retained); check_surface(retained,lower);
    auto choose=retained; choose.retained={}; const auto upper=rounded_box_line_path(choose); check_surface(choose,upper);
    require(lower.length_m>upper.length_m+.1,"sharp retained side cannot pop to shorter side");
    const float old_contact_z=lower.surface_points[0].GetZ();
    retained.endpoint_a+=Vec3(0,.02F,.2F); retained.endpoint_b+=Vec3(0,-.01F,.4F);
    const auto slid=rounded_box_line_path(retained); check_surface(retained,slid);
    require(slid.topology.axis==2 && slid.topology.direction==1 &&
            slid.surface_points[0].GetZ()>old_contact_z+.2F,"sharp contact slides axially on real edge");
    gradients(retained);
    q.endpoint_a=RVec3(-3,1.1F,0); q.endpoint_b=RVec3(3,1.1F,0);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Clear,"sharp actual shortcut releases");
    q.endpoint_a=RVec3(-3,0,4.5F); q.endpoint_b=RVec3(3,0,5.5F);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Unsupported,"sharp retained path reaches finite end");
    q.endpoint_a=RVec3(-3,0,5.1F); q.endpoint_b=RVec3(3,0,5.1F);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Clear,"sharp finite beam shortcut clears beyond end");
    q.endpoint_a=RVec3(0,0,0); q.endpoint_b=RVec3(3,0,0);
    require(rounded_box_line_path(q).status==RoundedBoxLineStatus::Unsupported,"sharp embedded endpoint rejected");
    q=beam(); q.convex_radius=0;
    const Quat rotation=Quat::sRotation(Vec3(1,2,3).Normalized(),.47F);
    const auto transform=RMat44::sRotationTranslation(rotation,RVec3(17,-11,6));
    q.leaf_to_world=transform; q.endpoint_a=transform*q.endpoint_a; q.endpoint_b=transform*q.endpoint_b;
    const auto rotated=rounded_box_line_path(q); check_surface(q,rotated); gradients(q);
    near(rotated.length_m,path.length_m,3e-6,"sharp rigid transform length covariance");
    near((rotated.gradient_a-rotation*path.gradient_a).Length(),0,2e-6,"sharp gradient rotation covariance");
    q=beam(); q.convex_radius=0; q.half_extent=Vec3(5,1,.6F); q.endpoint_a=RVec3(-.7F,-3,0); q.endpoint_b=RVec3(.8F,3,0); q.retained={0,-1}; gradients(q);
    q=beam(); q.convex_radius=0; q.half_extent=Vec3(.6F,5,1); q.endpoint_a=RVec3(0,-.7F,-3); q.endpoint_b=RVec3(0,.8F,3); q.retained={1,1}; gradients(q);
}
void covariance_and_derivatives() {
    auto x=beam(); x.half_extent=Vec3(5,1,.6F); x.endpoint_a=RVec3(-.7F,-3,0); x.endpoint_b=RVec3(.8F,3,0); x.retained={0,1}; gradients(x);
    auto y=beam(); y.half_extent=Vec3(.6F,5,1); y.endpoint_a=RVec3(0,-.7F,-3); y.endpoint_b=RVec3(0,.8F,3); y.retained={1,-1}; gradients(y);
    for(int i=0;i<12;++i) {
        auto q=beam(); q.endpoint_a+=Vec3(-.03F*i,.02F*i,.01F*i); q.endpoint_b+=Vec3(.015F*i,-.01F*i,.03F*i);
        gradients(q);
        const auto original=rounded_box_line_path(q);
        const Quat rotation=Quat::sRotation(Vec3(1,2,3).Normalized(),.47F);
        const auto transform=RMat44::sRotationTranslation(rotation,RVec3(17,-11,6));
        q.leaf_to_world=transform;
        q.endpoint_a=transform*q.endpoint_a; q.endpoint_b=transform*q.endpoint_b;
        const auto rotated=rounded_box_line_path(q); check_surface(q,rotated);
        near(rotated.length_m,original.length_m,3e-6,"rigid transform length covariance");
        near((rotated.gradient_a-rotation*original.gradient_a).Length(),0,2e-6,"gradient rotation covariance");
        near((rotated.gradient_b-rotation*original.gradient_b).Length(),0,2e-6,"b gradient rotation covariance");
        gradients(q);
    }
    // Independent obstacle translation/rotation derivatives equal reciprocal
    // endpoint reaction and moment, including nonzero axial endpoint slope.
    auto q=beam(); const auto r=rounded_box_line_path(q); constexpr float h=.001F;
    const Vec3 reaction=-r.gradient_a-r.gradient_b;
    const Vec3 moment=-Vec3(q.endpoint_a).Cross(r.gradient_a)-Vec3(q.endpoint_b).Cross(r.gradient_b);
    for(int i=0;i<3;++i) {
        Vec3 axis=Vec3::sZero(); axis.SetComponent(i,1);
        auto a=q,b=q; a.leaf_to_world=RMat44::sTranslation(-axis*h); b.leaf_to_world=RMat44::sTranslation(axis*h);
        near((rounded_box_line_path(b).length_m-rounded_box_line_path(a).length_m)/(2*h),reaction[i],1e-4,"obstacle translation derivative");
        a=q; b=q; a.leaf_to_world=RMat44::sRotation(Quat::sRotation(axis,-h)); b.leaf_to_world=RMat44::sRotation(Quat::sRotation(axis,h));
        near((rounded_box_line_path(b).length_m-rounded_box_line_path(a).length_m)/(2*h),moment[i],2e-4,"obstacle rotation derivative");
    }
}
} // namespace
int main() {
    RegisterDefaultAllocator();
    clear_and_invalid(); rounded_wrap(); retained_slide_release(); sharp_wrap(); covariance_and_derivatives();
    std::cout<<"max_endpoint_derivative_error="<<max_endpoint_derivative_error<<" max_surface_roundtrip_error_m="<<max_surface_roundtrip_error<<'\n';
    std::cout<<"PASS rounded_box_line_path clear exact_arcs exact_sharp_polygon corner_normal_cones gradients actual_surface covariance retained_slide release finite_end reaction_moment\n";
}
