#include "sim/portable_line_path.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace scraperx::sim;
using namespace JPH;
namespace {
using Path=PortableLinePath;
double maximum_translation_error=0,maximum_rotation_error=0;
void check(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char *message) {
    if(std::abs(a-b)>tolerance) { std::cerr<<a<<" != "<<b<<' '; check(false,message); }
}
struct BP final:BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "portable-line-path"; }
};
struct BV final:ObjectVsBroadPhaseLayerFilter { bool ShouldCollide(ObjectLayer,BroadPhaseLayer) const override { return false; } };
struct LP final:ObjectLayerPairFilter { bool ShouldCollide(ObjectLayer,ObjectLayer) const override { return false; } };
struct World {
    BP bp; BV bv; LP lp; PhysicsSystem world;
    World() { world.Init(128,0,128,128,bp,bv,lp); world.SetGravity(Vec3::sZero()); }
    Body *add(const Shape *shape,RVec3Arg position,EMotionType type=EMotionType::Dynamic) {
        BodyCreationSettings settings(shape,position,Quat::sIdentity(),type,1);
        settings.mAllowSleeping=false; settings.mLinearDamping=settings.mAngularDamping=0;
        const auto id=world.GetBodyInterface().CreateAndAddBody(settings,EActivation::Activate);
        auto *body=world.GetBodyLockInterfaceNoLock().TryGetBody(id); check(body!=nullptr,"actual body exists"); return body;
    }
    void move(Body *body,RVec3Arg position,QuatArg rotation=Quat::sIdentity()) {
        world.GetBodyInterface().SetPositionAndRotation(body->GetID(),position,rotation,EActivation::Activate);
    }
};
struct Scene:World {
    Body *fitting,*anchor,*obstacle;
    Path::Input input; Path::State state;
    Scene(float radius=.05F,RVec3 obstacle_position=RVec3::sZero()) {
        fitting=add(new BoxShape(Vec3(.1F,.1F,.1F)),RVec3(-3,0,-.7F));
        anchor=add(new BoxShape(Vec3(.5F,.2F,.2F)),RVec3(4,0,.8F));
        obstacle=add(new BoxShape(Vec3(1,1,5),radius),obstacle_position);
        input.fitting=fitting->GetID(); input.anchor_body=anchor->GetID(); input.anchor_shape_identity=anchor->GetShape();
        input.anchor_shape_local_point=Vec3(-1,0,0);
    }
    Path::Result evaluate() { input.endpoint_a=fitting->GetCenterOfMassPosition(); return Path::evaluate(world,input,state); }
};
void valid(const Path::Result &r,bool wrapped) {
    if(!r.valid) std::cerr<<"reason="<<int(r.reason)<<' ';
    check(r.valid && r.reason==Path::Reason::None,"actual path admitted");
    check(r.has_contact==wrapped && r.participant_count==(wrapped?3U:2U),"actual contact participants");
    check(r.visual_point_count>1 && r.visual_point_count<=Path::visual_capacity,"bounded presentation points");
    check(r.length_m>0,"real path length");
    Vec3 reaction=Vec3::sZero(); for(unsigned i=0;i<r.participant_count;++i) reaction+=r.participants[i].linear;
    near(reaction.Length(),0,2e-6,"path gradients preserve reciprocal translation");
}
void discover_move_slide_release() {
    for(float radius:{0.0F,.05F}) {
        Scene s(radius,RVec3(0,4,0));
        valid(s.evaluate(),false); check(!s.state.has_contact,"clear chord has no retained obstacle");
        s.move(s.obstacle,RVec3::sZero()); auto first=s.evaluate(); valid(first,true);
        check(first.contact.body==s.obstacle->GetID() && first.contact.topology.axis==2,"actual obstacle leaf identity");
        const auto token=s.state.token; const float old_local_z=(s.obstacle->GetCenterOfMassTransform().InversedRotationTranslation()*first.visual_points[1]).GetZ();
        s.move(s.obstacle,RVec3(0,0,.2F)); const auto slid=s.evaluate(); valid(slid,true);
        const float new_local_z=(s.obstacle->GetCenterOfMassTransform().InversedRotationTranslation()*slid.visual_points[1]).GetZ();
        near(new_local_z-old_local_z,-.2,2e-6,"contact slides on real moving extrusion");
        check(s.state.token==token && slid.contact.topology.direction==first.contact.topology.direction,"retained side and identity survive sliding");
        s.move(s.obstacle,RVec3(.1F,.05F,.2F),Quat::sRotation(Vec3::sAxisZ(),.1F)); valid(s.evaluate(),true);
        auto checkpoint=s.state; Path::State restored=checkpoint;
        const auto from_copy=Path::evaluate(s.world,s.input,restored); valid(from_copy,true);
        check(restored.token==s.state.token && restored.contact.body==s.state.contact.body,"checkpoint topology rebuild uses IDs and shape ownership");
        s.move(s.obstacle,RVec3(0,4,0)); valid(s.evaluate(),false);
        check(!s.state.has_contact && s.state.token>token,"actual shortcut releases contact");
    }
}
void obstacle_derivatives() {
    for(float radius:{0.0F,.05F}) {
        Scene s(radius); s.move(s.obstacle,RVec3(.1F,.03F,.2F),Quat::sRotation(Vec3::sAxisZ(),.1F));
        const auto base=s.evaluate(); valid(base,true); const auto position=s.obstacle->GetPosition(); const auto rotation=s.obstacle->GetRotation();
        constexpr float step=.002F;
        for(int axis=0;axis<3;++axis) {
            Vec3 unit=Vec3::sZero(); unit.SetComponent(axis,1);
            s.move(s.obstacle,position-unit*step,rotation); auto left_state=s.state;
            const auto left=Path::evaluate(s.world,s.input,left_state); valid(left,true); const double lo=s.obstacle->GetPosition()[axis];
            s.move(s.obstacle,position+unit*step,rotation); auto right_state=s.state;
            const auto right=Path::evaluate(s.world,s.input,right_state); valid(right,true); const double hi=s.obstacle->GetPosition()[axis];
            const double derivative=(right.length_m-left.length_m)/(hi-lo);
            maximum_translation_error=std::max(maximum_translation_error,std::abs(derivative-base.participants[2].linear[axis]));
            near(derivative,base.participants[2].linear[axis],1.5e-4,"actual obstacle translation gradient");
            s.move(s.obstacle,position,Quat::sRotation(unit,-step)*rotation); left_state=s.state;
            const auto minus=Path::evaluate(s.world,s.input,left_state); valid(minus,true);
            s.move(s.obstacle,position,Quat::sRotation(unit,step)*rotation); right_state=s.state;
            const auto plus=Path::evaluate(s.world,s.input,right_state); valid(plus,true);
            const double angular=(plus.length_m-minus.length_m)/(2*step);
            maximum_rotation_error=std::max(maximum_rotation_error,std::abs(angular-base.participants[2].angular[axis]));
            near(angular,base.participants[2].angular[axis],2e-4,"actual obstacle rotation gradient");
            s.move(s.obstacle,position,rotation);
        }
    }
}
void anchor_compound_and_decorators() {
    World s;
    RefConst<Shape> box=new BoxShape(Vec3(1,1,5),.05F);
    RefConst<Shape> decorated=new RotatedTranslatedShape(Vec3::sZero(),Quat::sRotation(Vec3::sAxisZ(),.1F),new ScaledShape(box,Vec3(1,.9F,1)));
    StaticCompoundShapeSettings compound;
    compound.AddShape(Vec3::sZero(),Quat::sIdentity(),decorated);
    compound.AddShape(Vec3(4,0,.8F),Quat::sIdentity(),new BoxShape(Vec3(.5F,.2F,.2F)));
    const auto created=compound.Create(); check(!created.HasError(),"actual compound creation");
    auto *anchor=s.add(created.Get(),RVec3::sZero());
    auto *fitting=s.add(new BoxShape(Vec3(.1F,.1F,.1F)),RVec3(-3,0,-.7F));
    Path::Input input; input.fitting=fitting->GetID(); input.anchor_body=anchor->GetID(); input.anchor_shape_identity=anchor->GetShape();
    input.endpoint_a=fitting->GetCenterOfMassPosition(); input.anchor_shape_local_point=Vec3(3,0,.8F);
    Path::State state; const auto result=Path::evaluate(s.world,input,state); valid(result,true);
    check(result.contact.body==anchor->GetID() && result.contact.leaf_shape_identity==box,"anchor compound's distinct decorated leaf is discovered");
    check(result.participants[1].body==result.participants[2].body,"repeated anchor/obstacle body preserved for row combination");
    const Vec3 aggregate=result.participants[1].linear+result.participants[2].linear;
    near((aggregate+result.participants[0].linear).Length(),0,2e-6,"same-body aggregate reaction");
}
void rounded_outer_ray_false_positive() {
    World s;
    auto *fitting=s.add(new BoxShape(Vec3(.1F,.1F,.1F)),RVec3(-4,0,0));
    auto *anchor=s.add(new BoxShape(Vec3(.4F,.2F,.2F)),RVec3(5,0,0));
    // The chord enters the sharp outer ray box near its rounded edge. Its cap
    // plane is tangent to z=0, so either real beam detour also stays outside.
    auto *false_positive=s.add(new BoxShape(Vec3(.3F,.3F,.3F),.2F),RVec3(-2,-.29F,-.3F));
    Path::Input input; input.fitting=fitting->GetID(); input.anchor_body=anchor->GetID(); input.anchor_shape_identity=anchor->GetShape();
    input.endpoint_a=fitting->GetCenterOfMassPosition(); input.anchor_shape_local_point=Vec3(-1,0,0);
    RayCastResult hit;
    check(s.world.GetNarrowPhaseQueryNoLock().CastRay(RRayCast(input.endpoint_a,Vec3(8,0,0)),hit,{},{},IgnoreSingleBodyFilter(fitting->GetID())) &&
          hit.mBodyID==false_positive->GetID(),"actual outer-box ray false positive exercised");
    Path::State state; valid(Path::evaluate(s.world,input,state),false);
    auto *actual=s.add(new BoxShape(Vec3(.5F,.6F,3),.05F),RVec3::sZero());
    const auto path=Path::evaluate(s.world,input,state); valid(path,true);
    check(path.contact.body==actual->GetID(),"true clear rounded ray false positive skipped before next actual hit");
}
void invalidation_and_limits() {
    Scene s; valid(s.evaluate(),true); const auto old_token=s.state.token;
    s.world.GetBodyInterface().SetShape(s.obstacle->GetID(),new BoxShape(Vec3(1,1,5),0),true,EActivation::Activate);
    auto r=s.evaluate(); check(!r.valid && r.participant_count==0 && r.reason==Path::Reason::ContactChanged && !s.state.has_contact && s.state.token>old_token,"shape change invalidates retained support without force");
    valid(s.evaluate(),true);
    const auto obstacle_id=s.obstacle->GetID(); s.world.GetBodyInterface().RemoveBody(obstacle_id); s.world.GetBodyInterface().DestroyBody(obstacle_id); s.obstacle=nullptr;
    r=s.evaluate(); check(!r.valid && r.reason==Path::Reason::ContactChanged && r.participant_count==0,"destroyed body identity rejected");
    s.world.GetBodyInterface().SetShape(s.anchor->GetID(),new BoxShape(Vec3(.5F,.2F,.2F)),true,EActivation::Activate);
    r=s.evaluate(); check(!r.valid && r.reason==Path::Reason::AnchorChanged && r.participant_count==0,"anchor shape identity invalidates support");
    Scene finite(0); valid(finite.evaluate(),true);
    finite.move(finite.fitting,RVec3(-3,0,4.5F)); finite.move(finite.anchor,RVec3(4,0,5.5F));
    r=finite.evaluate(); check(!r.valid && r.reason==Path::Reason::UnsupportedGeometry && r.participant_count==0,"finite extrusion end explicitly suspends force admission");
    Scene multiple; multiple.add(new BoxShape(Vec3(.2F,.5F,3),0),RVec3(-2,0,0));
    r=multiple.evaluate(); check(!r.valid && r.reason==Path::Reason::AdditionalContact && r.participant_count==0,"multiple actual chord contacts explicitly unsupported");
    Scene boundary; const auto first=boundary.evaluate(); valid(first,true);
    const RVec3 middle=first.visual_points[first.visual_point_count/2];
    boundary.add(new BoxShape(Vec3(.1F,.1F,.1F)),middle);
    r=boundary.evaluate(); check(!r.valid && r.reason==Path::Reason::NearbyBoundaryGeometry && r.participant_count==0,"curved-boundary envelope rejects unresolved geometry without chord physics");
    Scene unsupported; unsupported.move(unsupported.obstacle,RVec3(0,4,0)); unsupported.add(new SphereShape(.5F),RVec3::sZero());
    r=unsupported.evaluate(); check(!r.valid && r.reason==Path::Reason::UnsupportedShape && r.participant_count==0,"non-Box obstruction explicit");
}
void query_overflow() {
    World s; auto *fitting=s.add(new BoxShape(Vec3(.1F,.1F,.1F)),RVec3(-20,0,0));
    auto *anchor=s.add(new BoxShape(Vec3(.4F,.2F,.2F)),RVec3(21,0,0));
    for(unsigned i=0;i<Path::query_capacity+1;++i) s.add(new BoxShape(Vec3(.1F,.4F,2),0),RVec3(-14.4F+.9F*i,0,0));
    Path::Input input; input.fitting=fitting->GetID(); input.anchor_body=anchor->GetID(); input.anchor_shape_identity=anchor->GetShape();
    input.endpoint_a=fitting->GetCenterOfMassPosition(); input.anchor_shape_local_point=Vec3(-1,0,0);
    Path::State state; const auto result=Path::evaluate(s.world,input,state);
    check(!result.valid && result.reason==Path::Reason::CapacityExceeded && result.participant_count==0,"query overflow never silently truncates obstacles");
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance=new Factory; RegisterTypes(); int result=0;
    try {
        const std::pair<const char *,void (*)()> checks[]={
            {"discover_move_slide_release",discover_move_slide_release},{"obstacle_derivatives",obstacle_derivatives},
            {"anchor_compound_and_decorators",anchor_compound_and_decorators},{"rounded_outer_ray_false_positive",rounded_outer_ray_false_positive},
            {"invalidation_and_limits",invalidation_and_limits},{"query_overflow",query_overflow}};
        for(const auto &test:checks) { std::cout<<"RUN "<<test.first<<std::endl; test.second(); }
        std::cout<<"maximum_translation_gradient_error="<<maximum_translation_error<<" maximum_rotation_gradient_error_m="<<maximum_rotation_error<<'\n';
        std::cout<<"PASS portable_line_path actual_narrowphase sharp_rounded moving_slide release anchor_compound decorated_scaled_leaf obstacle_derivatives false_positive_skip invalidation finite_end multiple_limit boundary_envelope overflow\n";
    } catch(const std::exception &error) { std::cerr<<"FAIL "<<error.what()<<'\n'; result=1; }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance=nullptr; return result;
}
