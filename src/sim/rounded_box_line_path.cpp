#include "sim/rounded_box_line_path.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
namespace {
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double eps = 1.0e-8;
struct V2 { double x, y; };
V2 operator+(V2 a,V2 b) { return {a.x+b.x,a.y+b.y}; }
V2 operator-(V2 a,V2 b) { return {a.x-b.x,a.y-b.y}; }
V2 operator*(V2 a,double b) { return {a.x*b,a.y*b}; }
double dot(V2 a,V2 b) { return a.x*b.x+a.y*b.y; }
double norm(V2 a) { return std::hypot(a.x,a.y); }
struct BoundaryPoint { V2 p, tangent; double s; int sharp_corner=-1; };
struct Section {
    double ix,iy,r;
    double perimeter() const { return 4*(ix+iy)+2*pi*r; }
    double quarter() const { return pi*r/2; }
    double arc_start(int k) const {
        const double starts[] = {2*iy,2*iy+quarter()+2*ix,
            4*iy+2*quarter()+2*ix,4*iy+3*quarter()+4*ix};
        return starts[k];
    }
    V2 center(int k) const { return {k==0 || k==3 ? ix:-ix,k<2 ? iy:-iy}; }
    double distance(V2 p) const {
        const double x=std::abs(p.x)-ix,y=std::abs(p.y)-iy;
        return std::hypot(std::max(0.0,x),std::max(0.0,y))+std::min(std::max(x,y),0.0)-r;
    }
    BoundaryPoint at(double s) const {
        s=std::fmod(s,perimeter()); if(s<0) s+=perimeter();
        const double lengths[]={2*iy,quarter(),2*ix,quarter(),2*iy,quarter(),2*ix,quarter()};
        for(int i=0;i<8;++i) {
            if(lengths[i]==0) continue;
            if(s<=lengths[i] || i==7) {
                if(i%2) {
                    const int k=i/2; const double angle=k*pi/2+s/r;
                    const V2 n{std::cos(angle),std::sin(angle)};
                    return {center(k)+n*r,{-n.y,n.x},0};
                }
                switch(i) {
                case 0: return {{ix+r,-iy+s},{0,1},0};
                case 2: return {{ix-s,iy+r},{-1,0},0};
                case 4: return {{-ix-r,iy-s},{0,-1},0};
                default:return {{-ix+s,-iy-r},{1,0},0};
                }
            }
            s-=lengths[i];
        }
        return {};
    }
};
V2 sharp_after(int corner) {
    const V2 tangents[]={{-1,0},{0,-1},{1,0},{0,1}};
    return tangents[corner];
}
bool valid_leg(const BoundaryPoint &p,V2 leg,double length,int direction) {
    if(length<=eps) return true;
    const V2 travel=leg*(1/length);
    if(p.sharp_corner<0) return dot(travel,p.tangent)*direction>=1-1.0e-7;
    // A sharp rectangle's supporting normal can be any vector in the corner's
    // outward normal cone. The right normal of CCW travel must lie in it.
    const V2 n{direction*travel.y,-direction*travel.x};
    return n.x*(p.p.x>0 ? 1:-1)>=-eps && n.y*(p.p.y>0 ? 1:-1)>=-eps;
}
V2 travel_at(const BoundaryPoint &p,int direction,bool departing) {
    if(p.sharp_corner<0) return p.tangent*double(direction);
    const V2 after=sharp_after(p.sharp_corner),before{after.y,-after.x};
    // At an endpoint coincident with a sharp vertex the derivative has a kink;
    // choose the one-sided boundary tangent belonging to this retained path.
    return direction>0 ? (departing ? after:before):(departing ? before*(-1):after*(-1));
}
struct Tangents { std::array<BoundaryPoint,10> p{}; int count=0; };
Tangents tangents(const Section &s,V2 p) {
    Tangents result;
    const auto add=[&](BoundaryPoint point) {
        for(int i=0;i<result.count;++i) if(norm(point.p-result.p[i].p)<eps) return;
        if(result.count<int(result.p.size())) result.p[result.count++]=point;
    };
    if(s.r==0) {
        for(int k=0;k<4;++k) add({s.center(k),sharp_after(k),s.arc_start(k),k});
    }
    for(int k=0;s.r>0 && k<4;++k) {
        const V2 d=p-s.center(k); const double d2=dot(d,d);
        if(d2<s.r*s.r-eps) continue;
        if(d2<=eps*eps) continue;
        const double angle=std::atan2(d.y,d.x);
        const double delta=std::acos(std::clamp(s.r/std::sqrt(d2),-1.0,1.0));
        for(int sign : {-1,1}) {
            double a=angle+sign*delta;
            while(a<0) a+=2*pi;
            while(a>=2*pi) a-=2*pi;
            // The last corner's endpoint at 2pi is equivalent to zero.
            if(k==3 && a<eps) a=2*pi;
            if(a<k*pi/2-eps || a>(k+1)*pi/2+eps) continue;
            a=std::clamp(a,k*pi/2,(k+1)*pi/2);
            V2 n{std::cos(a),std::sin(a)};
            add({s.center(k)+n*s.r,{-n.y,n.x},s.arc_start(k)+s.r*(a-k*pi/2)});
        }
    }
    // An endpoint already on a face has a zero-length tangent leg there.
    if(std::abs(s.distance(p))<=eps) {
        if(std::abs(p.x-s.ix-s.r)<=eps && std::abs(p.y)<=s.iy+eps)
            add({p,{0,1},p.y+s.iy});
        if(std::abs(p.y-s.iy-s.r)<=eps && std::abs(p.x)<=s.ix+eps)
            add({p,{-1,0},s.arc_start(0)+s.quarter()+s.ix-p.x});
        if(std::abs(p.x+s.ix+s.r)<=eps && std::abs(p.y)<=s.iy+eps)
            add({p,{0,-1},s.arc_start(1)+s.quarter()+s.iy-p.y});
        if(std::abs(p.y+s.iy+s.r)<=eps && std::abs(p.x)<=s.ix+eps)
            add({p,{1,0},s.arc_start(2)+s.quarter()+p.x+s.ix});
    }
    return result;
}
bool finite(JPH::Vec3Arg v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
// Exact minimum distance between a segment and the reduced 3D AABB: split
// where each coordinate crosses a core face, then minimize each quadratic.
double segment_core_distance_squared(const double *a,const double *b,const double *h) {
    std::array<double,8> cuts{}; int n=2; cuts[0]=0; cuts[1]=1;
    for(int j=0;j<3;++j) if(b[j]!=a[j]) for(int sign : {-1,1}) {
        const double t=(sign*h[j]-a[j])/(b[j]-a[j]);
        if(t>0 && t<1) cuts[n++]=t;
    }
    for(int i=1;i<n;++i) {
        const double value=cuts[i]; int j=i;
        while(j>0 && cuts[j-1]>value) { cuts[j]=cuts[j-1]; --j; }
        cuts[j]=value;
    }
    double best=std::numeric_limits<double>::infinity();
    const auto evaluate=[&](double t) {
        double d2=0;
        for(int j=0;j<3;++j) { const double d=std::max(0.0,std::abs(a[j]+(b[j]-a[j])*t)-h[j]); d2+=d*d; }
        return d2;
    };
    for(int i=1;i<n;++i) {
        const double lo=cuts[i-1],hi=cuts[i],mid=(lo+hi)/2;
        double aa=0,ab=0;
        for(int j=0;j<3;++j) {
            const double m=a[j]+(b[j]-a[j])*mid;
            if(std::abs(m)>h[j]) {
                const double c=a[j]-(m>0 ? h[j]:-h[j]),d=b[j]-a[j]; aa+=d*d; ab+=c*d;
            }
        }
        const double t=aa>0 ? std::clamp(-ab/aa,lo,hi):mid;
        best=std::min({best,evaluate(lo),evaluate(hi),evaluate(t)});
    }
    return best;
}
bool sharp_segment_blocked(const double *a,const double *b,const double *h) {
    double lo=0,hi=1;
    for(int j=0;j<3;++j) {
        const double d=b[j]-a[j];
        if(d==0) { if(std::abs(a[j])>=h[j]) return false; }
        else { double t0=(-h[j]-a[j])/d,t1=(h[j]-a[j])/d; if(t0>t1) std::swap(t0,t1);
            lo=std::max(lo,t0); hi=std::min(hi,t1); }
    }
    return hi>lo && hi>0 && lo<1;
}
} // namespace

RoundedBoxLineResult rounded_box_line_path(const RoundedBoxLineQuery &q) {
    RoundedBoxLineResult result;
    if(!finite(q.half_extent) || !finite(JPH::Vec3(q.endpoint_a)) || !finite(JPH::Vec3(q.endpoint_b)) ||
       !std::isfinite(q.convex_radius) || q.convex_radius<0 || q.retained.axis<-1 || q.retained.axis>2 ||
       (q.retained.direction!=0 && q.retained.direction!=1 && q.retained.direction!=-1) ||
       (q.retained.axis==-1 && q.retained.direction!=0)) return result;
    const JPH::Mat44 rotation=q.leaf_to_world.GetRotation();
    if(!finite(rotation.GetAxisX()) || !finite(rotation.GetAxisY()) || !finite(rotation.GetAxisZ()) ||
       !finite(JPH::Vec3(q.leaf_to_world.GetTranslation())) ||
       std::abs(rotation.GetAxisX().LengthSq()-1)>1.0e-5 ||
       std::abs(rotation.GetAxisY().LengthSq()-1)>1.0e-5 ||
       std::abs(rotation.GetAxisZ().LengthSq()-1)>1.0e-5 ||
       std::abs(rotation.GetAxisX().Dot(rotation.GetAxisY()))>1.0e-5 ||
       std::abs(rotation.GetAxisX().Dot(rotation.GetAxisZ()))>1.0e-5 ||
       std::abs(rotation.GetAxisY().Dot(rotation.GetAxisZ()))>1.0e-5) return result;
    // Evaluate the rigid inverse in double: multiplying world positions by a
    // float inverse matrix first loses small endpoint changes far from origin.
    const auto origin=q.leaf_to_world.GetTranslation();
    const auto local=[&](JPH::RVec3Arg p,double *out) {
        const double dx=double(p.GetX())-origin.GetX(),dy=double(p.GetY())-origin.GetY(),dz=double(p.GetZ())-origin.GetZ();
        const JPH::Vec3 axes[]={rotation.GetAxisX(),rotation.GetAxisY(),rotation.GetAxisZ()};
        for(int j=0;j<3;++j) out[j]=dx*axes[j].GetX()+dy*axes[j].GetY()+dz*axes[j].GetZ();
    };
    double a[3],b[3]; local(q.endpoint_a,a); local(q.endpoint_b,b);
    double h[3]={q.half_extent.GetX(),q.half_extent.GetY(),q.half_extent.GetZ()},core[3];
    for(int j=0;j<3;++j) { if(h[j]<=0 || q.convex_radius>h[j]) return result; core[j]=h[j]-q.convex_radius; }
    const JPH::Vec3 delta(q.endpoint_b-q.endpoint_a);
    const double direct=delta.Length();
    const bool blocked=q.convex_radius>0 ? segment_core_distance_squared(a,b,core)<q.convex_radius*q.convex_radius : sharp_segment_blocked(a,b,h);
    if(!blocked) {
        result.status=RoundedBoxLineStatus::Clear; result.length_m=direct;
        if(direct>0) { result.gradient_b=delta/float(direct); result.gradient_a=-result.gradient_b; }
        return result;
    }
    result.status=RoundedBoxLineStatus::Unsupported;
    if(q.convex_radius>0 && q.convex_radius<=eps) return result;
    double best=std::numeric_limits<double>::infinity();
    for(int axis=0;axis<3;++axis) {
        if(q.retained.axis>=0 && q.retained.axis!=axis) continue;
        const int u=(axis+1)%3,v=(axis+2)%3;
        const Section section{core[u],core[v],q.convex_radius};
        const V2 pa{a[u],a[v]},pb{b[u],b[v]};
        if(section.distance(pa)<-eps || section.distance(pb)<-eps) continue;
        const Tangents ta=tangents(section,pa),tb=tangents(section,pb);
        for(int direction : {-1,1}) {
            if(q.retained.direction!=0 && q.retained.direction!=direction) continue;
            for(int ia=0;ia<ta.count;++ia) for(int ib=0;ib<tb.count;++ib) {
                const auto &ca=ta.p[ia],&cb=tb.p[ib];
                const V2 la=ca.p-pa,lb=pb-cb.p;
                const double al=norm(la),bl=norm(lb);
                if(!valid_leg(ca,la,al,direction) || !valid_leg(cb,lb,bl,direction)) continue;
                double boundary=std::fmod(direction*(cb.s-ca.s)+section.perimeter(),section.perimeter());
                if(boundary<eps) boundary=0;
                const double cross=al+boundary+bl,dz=b[axis]-a[axis];
                if(cross<=eps) continue;
                const double za=a[axis]+dz*al/cross,zb=a[axis]+dz*(al+boundary)/cross;
                if(std::abs(za)>core[axis]+eps || std::abs(zb)>core[axis]+eps) continue;
                const double length=std::hypot(cross,dz);
                if(length>=best) continue;
                best=length;
                result.status=RoundedBoxLineStatus::Wrapped; result.topology={axis,direction};
                result.length_m=length; result.cross_section_length_m=cross; result.boundary_length_m=boundary;
                const V2 ga=al>eps ? la*(-1/al):travel_at(ca,direction,true)*(-1);
                const V2 gb=bl>eps ? lb*(1/bl):travel_at(cb,direction,false);
                float ag[3]={},bg[3]={};
                ag[u]=float(ga.x*cross/length); ag[v]=float(ga.y*cross/length); ag[axis]=float(-dz/length);
                bg[u]=float(gb.x*cross/length); bg[v]=float(gb.y*cross/length); bg[axis]=float(dz/length);
                result.gradient_a=rotation.Multiply3x3(JPH::Vec3(ag[0],ag[1],ag[2]));
                result.gradient_b=rotation.Multiply3x3(JPH::Vec3(bg[0],bg[1],bg[2]));
                result.surface_point_count=boundary>eps ? std::uint32_t(result.surface_points.size()):1;
                for(std::uint32_t i=0;i<result.surface_point_count;++i) {
                    const double fraction=result.surface_point_count>1 ? double(i)/(result.surface_point_count-1):0;
                    const double arc=boundary*fraction;
                    const auto p=section.at(ca.s+direction*arc);
                    float xyz[3]={}; xyz[u]=float(p.p.x); xyz[v]=float(p.p.y); xyz[axis]=float(a[axis]+dz*(al+arc)/cross);
                    result.surface_points[i]=q.leaf_to_world*JPH::Vec3(xyz[0],xyz[1],xyz[2]);
                }
            }
        }
    }
    return result;
}
} // namespace scraperx::sim
