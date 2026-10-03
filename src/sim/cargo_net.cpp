#include "sim/cargo_net.hpp"
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <algorithm>
#include <cmath>
#include <limits>
namespace scraperx::sim {
namespace {
constexpr int kColumns=9, kRows=23;
constexpr float kDX=0.5F, kDY=10.3F/float(kRows-1), kHalf=0.04F;
constexpr std::uint32_t vertex(int c, int r, int corner) { return std::uint32_t(4*(r*kColumns+c)+corner); }
}
CargoNet::CargoNet(JPH::PhysicsSystem &system, JPH::ObjectLayer layer):system_(system) {
    using namespace JPH;
    Ref<SoftBodySharedSettings> settings=new SoftBodySharedSettings;
    // Four corners per woven knot; ribbons between knots leave actual open
    // cells. No invisible cloth faces span the mesh holes.
    for(int r=0;r<kRows;++r) for(int c=0;c<kColumns;++c) for(int corner=0;corner<4;++corner) {
        SoftBodySharedSettings::Vertex v;
        Vec3(c*kDX+((corner&1)?kHalf:-kHalf), r*kDY+((corner&2)?kHalf:-kHalf),0).StoreFloat3(&v.mPosition);
        v.mInvMass=(r==0 || r==kRows-1)?0.0F:1.0F/0.06F;
        settings->mVertices.push_back(v);
    }
    const auto face=[&](std::uint32_t a,std::uint32_t b,std::uint32_t c) {
        SoftBodySharedSettings::Face f; f.mVertex[0]=a;f.mVertex[1]=b;f.mVertex[2]=c;
        settings->AddFace(f); indices_.insert(indices_.end(),{a,b,c});
    };
    const auto quad=[&](std::uint32_t a,std::uint32_t b,std::uint32_t c,std::uint32_t d) { face(a,b,c);face(a,c,d); };
    for(int r=0;r<kRows;++r) for(int c=0;c<kColumns;++c) {
        quad(vertex(c,r,0),vertex(c,r,1),vertex(c,r,3),vertex(c,r,2));
        if(c+1<kColumns) quad(vertex(c,r,1),vertex(c+1,r,0),vertex(c+1,r,2),vertex(c,r,3));
        if(r+1<kRows) quad(vertex(c,r,2),vertex(c,r,3),vertex(c,r+1,1),vertex(c,r+1,0));
    }
    // SI compliance, not a render-rate spring. Jolt's native soft solver
    // owns stretch, shear, bending, gravity, vertex/rigid collision and time.
    const SoftBodySharedSettings::VertexAttributes material(5.0e-6F,5.0e-5F,2.0e-3F);
    settings->CreateConstraints(&material,1,SoftBodySharedSettings::EBendType::Distance);
    settings->Optimize();
    SoftBodyCreationSettings creation(settings,RVec3(18.0,0.45,-117.9),Quat::sIdentity(),layer);
    creation.mUserData=kEntity;creation.mUpdatePosition=false;creation.mAllowSleeping=false;
    creation.mNumIterations=8;creation.mLinearDamping=1.2F;creation.mVertexRadius=0.035F;
    creation.mFriction=0.8F;creation.mFacesDoubleSided=true;
    body_=system_.GetBodyInterface().CreateAndAddSoftBody(creation,EActivation::Activate);
    refresh();
}
CargoNet::~CargoNet() { auto &b=system_.GetBodyInterface();b.RemoveBody(body_);b.DestroyBody(body_); }
void CargoNet::refresh() {
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return;
    const auto &body=lock.GetBody();
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(body.GetMotionProperties());
    const auto transform=body.GetCenterOfMassTransform();
    previous_vertices_=vertices_;
    vertices_.clear();vertices_.reserve(motion->GetVertices().size());
    for(const auto &v:motion->GetVertices()) vertices_.push_back(transform*JPH::RVec3(v.mPosition));
    if(previous_vertices_.size()!=vertices_.size()) previous_vertices_=vertices_;
}
std::vector<JPH::RVec3> CargoNet::render_vertices(float alpha) const {
    std::vector<JPH::RVec3> out;out.reserve(vertices_.size());
    alpha=std::clamp(alpha,0.0F,1.0F);
    for(std::size_t i=0;i<vertices_.size();++i) out.push_back(previous_vertices_[i]*(1-alpha)+vertices_[i]*alpha);
    return out;
}
JPH::RVec3 CargoNet::node(int c,int r) const noexcept {
    JPH::RVec3 sum=JPH::RVec3::sZero();
    for(int k=0;k<4;++k) sum+=vertices_[vertex(c,r,k)];
    return sum*0.25;
}
JPH::RVec3 CargoNet::surface(float c,float r) const noexcept {
    c=std::clamp(c,0.0F,float(kColumns-1));r=std::clamp(r,0.0F,float(kRows-1));
    const int x=std::min(int(c),kColumns-2),y=std::min(int(r),kRows-2);
    const float u=c-x,v=r-y;
    return (node(x,y)*(1-u)+node(x+1,y)*u)*(1-v)+(node(x,y+1)*(1-u)+node(x+1,y+1)*u)*v;
}
JPH::RVec3 CargoNet::surface(float c,float r,const std::vector<JPH::RVec3> &vertices) const noexcept {
    c=std::clamp(c,0.0F,float(kColumns-1));r=std::clamp(r,0.0F,float(kRows-1));
    const int x=std::min(int(c),kColumns-2),y=std::min(int(r),kRows-2);
    const float u=c-x,v=r-y;
    const auto at=[&](int column,int row) {
        JPH::RVec3 sum=JPH::RVec3::sZero();
        for(int k=0;k<4;++k) sum+=vertices[vertex(column,row,k)];
        return sum*0.25;
    };
    return (at(x,y)*(1-u)+at(x+1,y)*u)*(1-v)+(at(x,y+1)*(1-u)+at(x+1,y+1)*u)*v;
}
JPH::RVec3 CargoNet::render_world_point(JPH::Vec3 material,float alpha) const noexcept {
    alpha=std::clamp(alpha,0.0F,1.0F);
    return surface(material.GetX(),material.GetY(),previous_vertices_)*(1-alpha)+
           surface(material.GetX(),material.GetY(),vertices_)*alpha+JPH::RVec3(0,0,material.GetZ());
}
JPH::Vec3 CargoNet::material_point(JPH::RVec3 world) const noexcept {
    float c=std::clamp(float(world.GetX()-18)/kDX,0.0F,float(kColumns-1));
    float r=std::clamp(float(world.GetY()-0.45)/kDY,0.0F,float(kRows-1));
    // Invert the actual deformed surface in X/Y. Store material coordinates,
    // so holding still follows the mesh instead of hanging on the rest pose.
    for(int i=0;i<6;++i) {
        const auto at=surface(c,r);
        const float cx=c<kColumns-1.01F?0.01F:-0.01F,ry=r<kRows-1.01F?0.01F:-0.01F;
        const JPH::Vec3 dc((surface(c+cx,r)-at)/cx),dr((surface(c,r+ry)-at)/ry);
        const float det=dc.GetX()*dr.GetY()-dr.GetX()*dc.GetY();
        if(std::abs(det)<1e-7F) break;
        const float dx=float(world.GetX()-at.GetX()),dy=float(world.GetY()-at.GetY());
        c=std::clamp(c+(dx*dr.GetY()-dy*dr.GetX())/det,0.0F,float(kColumns-1));
        r=std::clamp(r+(dy*dc.GetX()-dx*dc.GetY())/det,0.0F,float(kRows-1));
    }
    return JPH::Vec3(c,r,float(world.GetZ()-surface(c,r).GetZ()));
}
JPH::Vec3 CargoNet::material_velocity(JPH::Vec3 material) const noexcept {
    const int c=std::min(int(material.GetX()),kColumns-2),r=std::min(int(material.GetY()),kRows-2);
    const float u=std::clamp(material.GetX()-c,0.0F,1.0F),v=std::clamp(material.GetY()-r,0.0F,1.0F);
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return JPH::Vec3::sZero();
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    JPH::Vec3 velocity=JPH::Vec3::sZero();
    for(int y=0;y<2;++y) for(int x=0;x<2;++x) for(int k=0;k<4;++k) {
        const float share=(x?u:1-u)*(y?v:1-v)*0.25F;
        velocity+=motion->GetVertices()[vertex(c+x,r+y,k)].mVelocity*share;
    }
    return lock.GetBody().GetRotation()*velocity;
}
JPH::RVec3 CargoNet::world_point(JPH::Vec3 material) const noexcept {
    return surface(material.GetX(),material.GetY())+JPH::RVec3(0,0,material.GetZ());
}
bool CargoNet::grip(JPH::RVec3 centre,JPH::RVec3 aim,JPH::Vec3 facing,JPH::RVec3 &point) const noexcept {
    if(centre.GetX()<17.2 || centre.GetX()>22.8 || centre.GetY()<-0.2 || centre.GetY()>12.0 || std::abs(centre.GetZ()+117.9)>2.0) return false;
    const JPH::Vec3 right(-facing.GetZ(),0,facing.GetX());
    float nearest=std::numeric_limits<float>::max();
    const auto candidate=[&](JPH::RVec3 a,JPH::RVec3 b) {
        const JPH::Vec3 d(b-a);const float t=std::clamp(JPH::Vec3(aim-a).Dot(d)/d.LengthSq(),0.0F,1.0F);
        const auto p=a+t*d;const JPH::Vec3 delta(p-aim);
        if(std::abs(delta.Dot(right))>0.34F || std::abs(delta.GetY())>0.34F || std::abs(delta.Dot(facing))>0.39F || JPH::Vec3(p-centre).Dot(facing)<0.16F) return;
        if(delta.LengthSq()<nearest) { nearest=delta.LengthSq();point=p; }
    };
    for(int r=0;r<kRows;++r) for(int c=0;c<kColumns;++c) {
        if(c+1<kColumns) candidate(node(c,r),node(c+1,r));
        if(r+1<kRows) candidate(node(c,r),node(c,r+1));
    }
    return std::isfinite(nearest) && nearest<std::numeric_limits<float>::max();
}
void CargoNet::load(JPH::RVec3 point,JPH::Vec3 force,float dt) {
    const auto material=material_point(point);
    const int c=std::min(int(material.GetX()),kColumns-2),r=std::min(int(material.GetY()),kRows-2);
    const float u=material.GetX()-c,v=material.GetY()-r;
    const JPH::BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return;
    auto *motion=static_cast<JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    // Partition one force; fixed vertices take their fraction as frame reaction.
    for(int y=0;y<2;++y) for(int x=0;x<2;++x) for(int k=0;k<4;++k) {
        auto &vertex_state=motion->GetVertex(vertex(c+x,r+y,k));
        const float share=(x?u:1-u)*(y?v:1-v)*0.25F;
        vertex_state.mVelocity+=force*(dt*share*vertex_state.mInvMass);
    }
}
std::string CargoNet::capture() const {
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    JPH::StateRecorderImpl stream;
    static_cast<const JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties())->SaveState(stream);
    return stream.GetData();
}
void CargoNet::restore(const std::string &state) {
    if(state.empty()) return;
    JPH::StateRecorderImpl stream;stream.WriteBytes(state.data(),state.size());stream.Rewind();
    { const JPH::BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
      static_cast<JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties())->RestoreState(stream); }
    auto &b=system_.GetBodyInterface();
    b.NotifyShapeChanged(body_,JPH::Vec3::sZero(),false,JPH::EActivation::Activate);
    refresh();
    previous_vertices_=vertices_;
}
}
