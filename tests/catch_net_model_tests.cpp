#include "sim/simulation.hpp"
#include "sim/catch_net_route.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyManifold.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace JPH;
using namespace scraperx::sim;
bool soft_contact_regression();
namespace {
void check(bool c,const char *m){if(!c)throw std::runtime_error(m);}
struct BP:BroadPhaseLayerInterface{uint GetNumBroadPhaseLayers()const override{return 2;}BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l)const override{return BroadPhaseLayer(l);}const char *GetBroadPhaseLayerName(BroadPhaseLayer)const override{return "catch";}};
struct BV:ObjectVsBroadPhaseLayerFilter{bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a!=0||b.GetValue()!=0;}};
struct LP:ObjectLayerPairFilter{bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a!=0||b!=0;}};
struct Contacts:SoftBodyContactListener {
    BodyID player;unsigned contacts=0;bool this_tick=false;
    void OnSoftBodyContactAdded(const Body &,const SoftBodyManifold &m)override {
        for(const auto &v:m.GetVertices())if(m.HasContact(v)&&m.GetContactBodyID(v)==player){++contacts;this_tick=true;break;}
    }
};
struct Listener:PhysicsStepListener {
    CargoNet *net=nullptr;
    unsigned calls=0;
    void OnStep(const PhysicsStepListenerContext &c)override{++calls;if(net)net->pre_step(c.mDeltaTime);}
};
struct World {
    BP bp;BV bv;LP lp;PhysicsSystem system;TempAllocatorImpl temp{32*1024*1024};JobSystemSingleThreaded jobs{1024};
    std::unique_ptr<kit::Kit> kit;std::unique_ptr<CargoNet> net;Listener listener;Contacts contacts;std::vector<std::pair<std::uint32_t,RVec3>> clips;std::vector<std::pair<RVec3,RVec3>> mounts;RVec3 frame_reference;BodyID ground,player;kit::BodyIndex frame;
    World(){system.Init(64,0,512,2048,bp,bv,lp);system.SetGravity(Vec3(0,-9.81F,0));
        kit=std::make_unique<scraperx::sim::kit::Kit>(system,0,1);
        BodyCreationSettings g(new BoxShape(Vec3(60,.5F,30)),RVec3(-34,-.5,-150),Quat::sIdentity(),EMotionType::Static,0);
        ground=system.GetBodyInterface().CreateAndAddBody(g,EActivation::DontActivate);
        frame=build_catch_net_route(*kit);
        net=std::make_unique<CargoNet>(system,1,kit->body_id(frame),catch_net_config(*kit));frame_reference=system.GetBodyInterface().GetCenterOfMassPosition(kit->body_id(frame));
        const auto inverse=system.GetBodyInterface().GetCenterOfMassTransform(kit->body_id(frame)).Inversed();
        const auto &cfg=net->config();
        const int corners=cfg.surface==CargoNetSurface::OpenWeave?4:1;
        for(int r=0;r<cfg.rows;++r)for(int c=0;c<cfg.columns;++c)
            if(r==0||r==cfg.rows-1||c==0||c==cfg.columns-1)for(int k=0;k<corners;++k){
                const auto i=std::uint32_t(corners*(r*cfg.columns+c)+k);
                clips.push_back({i,inverse*net->vertices()[i]});
            }
        for(float x:{-40.3F,-27.7F})for(float z:{-156.3F,-143.7F}){
            const RVec3 world(x,.30F,z);mounts.push_back({world,inverse*world});
        }
        listener.net=net.get();system.AddStepListener(&listener);system.SetSoftBodyContactListener(&contacts);
    }
    ~World(){system.RemoveStepListener(&listener);net.reset();kit.reset();auto &b=system.GetBodyInterface();if(!player.IsInvalid()){b.RemoveBody(player);b.DestroyBody(player);}b.RemoveBody(ground);b.DestroyBody(ground);}
    void tick(){contacts.this_tick=false;kit->pre_step(1.0F/90);check(system.Update(1.0F/90,4,&temp,&jobs)==EPhysicsUpdateError::None,"native update capacity");kit->post_step(1.0F/90);net->refresh();net->post_step(1.0F/90,contacts.this_tick);}
    void capsule(float y,EMotionQuality quality=EMotionQuality::LinearCast,float offset_z=0){BodyCreationSettings p(new CapsuleShape(.55F,.35F),RVec3(-34,y,-150+offset_z),Quat::sIdentity(),EMotionType::Dynamic,1);
        p.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;p.mMassPropertiesOverride.mMass=85;
        p.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
        p.mLinearDamping=0;p.mAngularDamping=0;p.mFriction=.8F;p.mRestitution=0;p.mMotionQuality=quality;
        player=system.GetBodyInterface().CreateAndAddBody(p,EActivation::Activate);contacts.player=player;}
};
void defaults(){World w;
    const auto frame_transform=w.system.GetBodyInterface().GetWorldTransform(w.kit->body_id(w.frame));
    for(const auto &part:w.kit->body_parts(w.frame))for(int i=0;i<8;++i){
        const Vec3 corner((i&1)?part.half.GetX():-part.half.GetX(),(i&2)?part.half.GetY():-part.half.GetY(),(i&4)?part.half.GetZ():-part.half.GetZ());
        check((frame_transform*(part.offset+part.rotation*corner)).GetY()>.01,"authored frame has no initial grade penetration or unpaid preload");
    }
    for(const float ratio:{-1.0F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto config=catch_net_config();config.axial_damping_ratio=ratio;bool rejected=false;
        try{CargoNet invalid(w.system,1,w.kit->body_id(w.frame),config);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"invalid axial material damping is rejected before native creation");
    }
    for(const float mass:{-1.0F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto config=catch_net_config();config.piston_pad_mass_kg=mass;bool rejected=false;
        try{CargoNet invalid(w.system,1,w.kit->body_id(w.frame),config);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"invalid reinforcement mass is rejected before native creation");
    }
    for(const bool zero_compliance:{false,true}){
        auto config=catch_net_config();
        if(zero_compliance)config.stretch_compliance=0;else config.internally_integrated_clips=false;
        bool rejected=false;try{CargoNet invalid(w.system,1,w.kit->body_id(w.frame),config);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"axial damping rejects unsupported integration or infinite stiffness");
    }
    CargoNet vertical(w.system,1,w.kit->body_id(w.frame));
    check(vertical.config().axial_damping_ratio==0&&w.net->config().axial_damping_ratio==.05F,
          "strain-rate material loss preserves legacy default");
    check(!vertical.config().paired_sleep&&w.net->config().paired_sleep,"paired sleep preserves legacy default");
    check(vertical.config().sleep_coupled_bodies.empty()&&w.net->config().sleep_coupled_bodies.size()==3,
          "catch sleep lifecycle includes all real slider stages and preserves legacy defaults");
    for(const auto id:{BodyID(),w.ground}){
        auto cfg=catch_net_config();cfg.sleep_coupled_bodies={id};bool rejected=false;
        try{CargoNet invalid(w.system,1,w.kit->body_id(w.frame),cfg);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"assembly sleep rejects invalid or non-dynamic members before native creation");
    }
    check(!vertical.config().internally_integrated_clips&&w.net->config().internally_integrated_clips,
          "internal clip integration preserves legacy default");
    {BodyLockRead lock(w.system.GetBodyLockInterface(),vertical.body());check(!lock.GetBody().GetAllowSleeping(),"legacy net native sleeping remains disabled");}
    check(vertical.vertices().size()==828&&vertical.indices().size()==3534,"legacy woven topology");
    check(vertical.columns()==9&&vertical.rows()==23&&vertical.attachment_count()==72,"legacy end rows");
    check(vertical.config().piston_pad_mass_kg==0&&vertical.reinforcement_indices().empty(),
          "disabled reinforcement preserves legacy vertical cloth");
    check(w.net->vertices().size()==1681&&w.net->indices().size()==9600&&w.net->attachment_count()==160,"continuous physical catch has unique nodes and real faces");
    const auto check_reinforcement=[&](const CargoNet &net){
        const auto &cfg=net.config();const auto &pad=net.reinforcement_indices();
        if(cfg.piston_pad_mass_kg>0){
            check(pad.size()==3&&pad[0]!=pad[1]&&pad[0]!=pad[2]&&pad[1]!=pad[2],
                  "reinforcement names three unique actual native vertices");
            bool native_face=false;const auto &faces=net.indices();
            for(std::size_t i=0;i<faces.size();i+=3){
                bool same=true;for(int corner=0;corner<3;++corner)
                    same&=std::find(pad.begin(),pad.end(),faces[i+corner])!=pad.end();
                native_face|=same;
            }
            check(native_face,"reinforcement corners belong to one actual collision triangle");
        }else check(pad.empty(),"disabled reinforcement has no selected native triangle");
        const BodyLockRead lock(w.system.GetBodyLockInterface(),net.body());
        const auto *motion=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
        for(std::size_t i=0;i<motion->GetVertices().size();++i){
            const bool reinforced=std::find(pad.begin(),pad.end(),std::uint32_t(i))!=pad.end();
            const float expected=cfg.vertex_mass+(reinforced?cfg.piston_pad_mass_kg/3:0);
            check(std::abs(1/motion->GetVertex(i).mInvMass-expected)<1e-6F,
                  "live triangle corner masses gain equal pad thirds and other vertices retain base mass");
        }
    };
    check_reinforcement(vertical);check_reinforcement(*w.net);
    {
        CargoNetConfig cfg;cfg.surface=CargoNetSurface::ContinuousMembrane;cfg.columns=cfg.rows=2;
        cfg.piston_pad_mass_kg=1.35F;cfg.piston_material=Vec3(.2F,.7F,0);
        CargoNet pad(w.system,1,w.kit->body_id(w.frame),cfg);
        check(cfg.piston_body.IsInvalid(),"pad fixture disables the piston attachment");
        check_reinforcement(pad);
        auto corners=pad.reinforcement_indices();std::sort(corners.begin(),corners.end());
        check(corners==std::vector<std::uint32_t>({0,2,3}),
              "pad follows the sampled native triangle even with unequal force weights and no piston body");
    }
    for(CargoNet *net:{&vertical,w.net.get()}) {
        const auto &cfg=net->config();float mass=0;
        {BodyLockRead lock(w.system.GetBodyLockInterface(),net->body());
         const auto *motion=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
         for(const auto &v:motion->GetVertices())mass+=1/v.mInvMass;}
        check(std::abs(mass-(cfg.columns*cfg.rows*(cfg.surface==CargoNetSurface::OpenWeave?4:1)*cfg.vertex_mass+cfg.piston_pad_mass_kg))<.01F,"actual cloth mass includes declared base and reinforcement mass");
        const Vec3 hole(cfg.dx*.5F,cfg.dy*.5F,0);
        const auto cross=[](Vec3 a,Vec3 b){return a.GetX()*b.GetY()-a.GetY()*b.GetX();};
        const auto &indices=net->indices();
        bool covered=false;
        for(std::size_t i=0;i<indices.size();i+=3){Vec3 q[3];
            for(int k=0;k<3;++k)q[k]=cfg.rotation.Conjugated()*Vec3(net->vertices()[indices[i+k]]-cfg.origin);
            const float a=cross(q[1]-q[0],hole-q[0]),b=cross(q[2]-q[1],hole-q[1]),c=cross(q[0]-q[2],hole-q[2]);
            const bool inside=(a>=0&&b>=0&&c>=0)||(a<=0&&b<=0&&c<=0);
            covered|=inside;
            if(cfg.surface==CargoNetSurface::OpenWeave)
                check(!inside,"woven cell centre has no collision proxy triangle");
        }
        if(cfg.surface==CargoNetSurface::ContinuousMembrane)
            check(covered,"continuous catch renders its actual native triangle topology");
    }
    for(CargoNet *net:{&vertical,w.net.get()})for(Vec3 material:{Vec3(1.2F,2.7F,.12F),Vec3(3.9F,5.1F,-.08F)}) {
        const auto point=net->world_point(material),round=net->material_point(point);
        check((round-material).Length()<.001F,"orientation-aware material round trip");
        check((net->render_world_point(material,1)-point).Length()<.001F,"render uses same material basis");
    }
}
bool asleep(World &w){auto &b=w.system.GetBodyInterface();
    if(b.IsActive(w.net->body())||b.IsActive(w.kit->body_id(w.frame)))return false;
    for(auto entity=kCatchPistonFirstEntity;entity<=kCatchPistonTopEntity;++entity)
        if(b.IsActive(w.kit->body_id(w.kit->body_for_entity(entity))))return false;
    return true;}
void settle(World &w){int ticks=0;while(!asleep(w)&&ticks<1800){w.tick();++ticks;}
    float max_speed_sq=0,kinetic=0;
    {const BodyLockRead lock(w.system.GetBodyLockInterface(),w.net->body());
     const auto *motion=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
     for(const auto &v:motion->GetVertices()){max_speed_sq=std::max(max_speed_sq,v.mVelocity.LengthSq());kinetic+=.5F*v.mVelocity.LengthSq()/v.mInvMass;}}
    std::cout<<"CATCH_SLEEP settle_ticks="<<ticks<<" both_asleep="<<asleep(w)<<" max_speed_sq="<<max_speed_sq<<" cloth_kinetic_j="<<kinetic<<'\n';
    check(asleep(w),"unloaded physical net/frame reach native paired sleep");}
std::vector<std::pair<Vec3,Vec3>> vertex_state(World &w){
    const BodyLockRead lock(w.system.GetBodyLockInterface(),w.net->body());
    const auto *m=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    std::vector<std::pair<Vec3,Vec3>> out;for(const auto &v:m->GetVertices())out.emplace_back(v.mPosition,v.mVelocity);return out;
}
void clip_reciprocity(){World w;
    // This closure measures clips only; piston reciprocity has its own fixture.
    auto clips_only=w.net->config();clips_only.piston_body=BodyID();clips_only.piston_stiffness=clips_only.piston_damping=0;
    w.net.reset();w.net=std::make_unique<CargoNet>(w.system,1,w.kit->body_id(w.frame),clips_only);w.listener.net=w.net.get();
    auto &b=w.system.GetBodyInterface();const auto frame=w.kit->body_id(w.frame);
    w.system.SetGravity(Vec3::sZero());const auto centre=b.GetCenterOfMassPosition(frame);
    const auto cloth_momentum=[&](){const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),w.net->body());
        const auto &body=lock.GetBody();const auto *motion=static_cast<const SoftBodyMotionProperties *>(body.GetMotionProperties());
        Vec3 linear=Vec3::sZero(),angular=Vec3::sZero();for(const auto &v:motion->GetVertices()){
            const auto momentum=body.GetRotation()*v.mVelocity/v.mInvMass;linear+=momentum;
            angular+=Vec3(body.GetCenterOfMassTransform()*RVec3(v.mPosition)-centre).Cross(momentum);}
        return std::make_pair(linear,angular);};
    const auto energy=[&](){const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),w.net->body());
        const auto &body=lock.GetBody();const auto *motion=static_cast<const SoftBodyMotionProperties *>(body.GetMotionProperties());
        double total=w.kit->body_kinetic_energy(w.frame)+w.net->tether_elastic_energy();
        for(const auto &v:motion->GetVertices())total+=.5*v.mVelocity.LengthSq()/v.mInvMass;
        for(const auto &edge:motion->GetSettings()->mEdgeConstraints){
            const double extension=(motion->GetVertex(edge.mVertex[1]).mPosition-motion->GetVertex(edge.mVertex[0]).mPosition).Length()-edge.mRestLength;
            total+=.5*extension*extension/edge.mCompliance;}
        const auto frame_transform=b.GetCenterOfMassTransform(frame);
        for(const auto &clip:w.clips){const auto vertex=body.GetCenterOfMassTransform()*RVec3(motion->GetVertex(clip.first).mPosition);
            total+=.5*w.net->config().clip_stiffness*(frame_transform*clip.second-vertex).LengthSq();}
        return total;};
    w.net->impulse(Vec3(12,0,0),Vec3(0,-.5F,0));const auto initial=cloth_momentum();
    const double initial_energy=energy();
    // Owner thread, with no concurrent Update. CustomUpdate itself locks the
    // rigid receivers; do not hold a possibly shared body-mutex stripe here.
    {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),w.net->body());
     auto *motion=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());motion->SetLinearDamping(0);
     motion->CustomUpdate(1.0F/360,lock.GetBody(),w.system);}
    w.net->refresh();
    const auto after=cloth_momentum();const auto frame_linear=b.GetLinearVelocity(frame)*w.kit->body_mass(w.frame);
    Vec3 frame_angular;{const BodyLockRead lock(w.system.GetBodyLockInterface(),frame);
        const auto &body=lock.GetBody();const auto inverse=body.GetMotionProperties()->GetInverseInertiaForRotation(Mat44::sRotation(body.GetRotation()));
        frame_angular=inverse.Inversed3x3()*body.GetAngularVelocity();}
    const float linear_error=(after.first+frame_linear-initial.first).Length(),angular_error=(after.second+frame_angular-initial.second).Length();
    std::cout<<"CATCH_CLIP frame_impulse="<<frame_linear.Length()<<" frame_torque_impulse="<<frame_angular.Length()<<" linear_error="<<linear_error<<" angular_error="<<angular_error<<'\n';
    const double final_energy=energy();
    std::cout<<"CATCH_MATERIAL initial_energy_j="<<initial_energy<<" final_energy_j="<<final_energy<<'\n';
    check(final_energy<initial_energy,"native strain-rate damping and clips cannot add unpaid stored energy");
    check(frame_linear.Length()>.005F&&frame_angular.Length()>.01F,"native clips transfer real finite force and point torque");
    check(linear_error<.005F&&angular_error<.05F,"native clip reactions preserve total linear and angular momentum");
}
// Reject a whole-tick kick, dropped outer-step remainder, replay, wrong
// transpose weights/live masses, overlap replacement, and stale restore work.
// One actual native contact solve: selecting a coefficient must reproduce the
// existing Coulomb solver, including reciprocal rigid impulse and dissipation.
void contact_friction(){
    check(soft_contact_regression(),"native soft contact is unilateral and preserves restitution history");
    struct Receipt { Vec3 rigid,cloth;float energy;unsigned calls; };
    struct Policy {
        BodyID expected;float selected=0;unsigned calls=0;bool geometry=true;
        static float select(void *context,BodyID id,Vec3 point,Vec3 normal,float original) noexcept {
            auto &p=*static_cast<Policy *>(context);++p.calls;
            p.geometry &= id==p.expected && point.GetY()<-.75F && point.GetY()>-1.0F &&
                normal.GetY()>.8F && std::isfinite(point.LengthSq()+normal.LengthSq()) && original==0;
            return id==p.expected?p.selected:original;
        }
    };
    const auto run=[](float body_friction,float selected,bool callback,bool matching) {
        World w;w.system.SetGravity(Vec3::sZero());
        CargoNetConfig cfg;cfg.columns=cfg.rows=2;cfg.origin=RVec3(100,100,100);
        cfg.rotation=Quat::sRotation(Vec3::sAxisX(),JPH_PI/2);cfg.dx=cfg.dy=.1F;
        cfg.knot_half_width=.02F;cfg.iterations=1;cfg.damping=cfg.clip_stiffness=cfg.clip_damping=0;
        cfg.stretch_compliance=cfg.shear_compliance=cfg.bend_compliance=1e20F;
        CargoNet net(w.system,1,w.kit->body_id(w.frame),cfg);
        BodyCreationSettings creation(new CapsuleShape(.55F,.35F),RVec3(100.05,100.9,100.05),Quat::sIdentity(),EMotionType::Dynamic,1);
        creation.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;creation.mMassPropertiesOverride.mMass=85;
        creation.mLinearDamping=creation.mAngularDamping=0;creation.mFriction=body_friction;creation.mRestitution=0;
        creation.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
        w.player=w.system.GetBodyInterface().CreateAndAddBody(creation,EActivation::Activate);
        Policy policy;policy.expected=matching?w.player:w.ground;policy.selected=selected;
        if(callback)net.set_contact_friction_callback(&Policy::select,&policy);
        {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
         auto *m=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
         for(auto &v:m->GetVertices())v.mVelocity=lock.GetBody().GetRotation().Conjugated()*Vec3(2,1,0);
         m->CustomUpdate(1.0F/90,lock.GetBody(),w.system);}
        Receipt result;result.calls=policy.calls;result.rigid=w.system.GetBodyInterface().GetLinearVelocity(w.player)*85;
        result.cloth=Vec3::sZero();result.energy=.5F*result.rigid.LengthSq()/85;
        {const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
         const auto *m=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
         for(const auto &v:m->GetVertices()){const auto velocity=lock.GetBody().GetRotation()*v.mVelocity;
             result.cloth+=velocity/v.mInvMass;result.energy+=.5F*velocity.LengthSq()/v.mInvMass;}}
        if(callback&&matching)check(policy.calls>0&&policy.geometry,"friction hook receives actual rigid ID, sole point and world normal");
        // Callback is caller-owned; remove it before the stack context expires.
        net.set_contact_friction_callback(nullptr,nullptr);
        return result;
    };
    const auto zero=run(0,0,false,true),identity=run(0,0,true,true),native=run(.8F,0,false,true),selected=run(0,.8F,true,true);
    const auto same=[](const Receipt &a,const Receipt &b){return (a.rigid-b.rigid).Length()<1e-6F&&(a.cloth-b.cloth).Length()<1e-6F;};
    check(same(zero,identity),"identity contact selector preserves native zero-friction impulses");
    check(same(native,selected),"selected coefficient reproduces actual native Coulomb contact impulses");
    check(selected.rigid.GetX()>zero.rigid.GetX()+.01F,"friction transfers a real reciprocal tangential impulse");
    check(selected.energy<zero.energy,"native tangential friction dissipates kinetic energy");
    check(((selected.rigid+selected.cloth)-(zero.rigid+zero.cloth)).Length()<1e-5F,"friction preserves combined cloth/rigid momentum");
    for(float invalid:{-1.0F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        check(same(zero,run(0,invalid,true,true)),"invalid selected coefficient falls back to original native friction");
    check(same(zero,run(0,.8F,true,false)),"caller body exclusion preserves native contact coefficient");
    std::cout<<"CATCH_FRICTION calls="<<selected.calls<<" rigid_tangent_impulse="<<selected.rigid.GetX()
        <<" zero_energy_j="<<zero.energy<<" friction_energy_j="<<selected.energy<<'\n';
}
void phased_force(CargoNetSurface surface=CargoNetSurface::OpenWeave,bool upper=false){World w;w.system.SetGravity(Vec3::sZero());
    CargoNetConfig cfg;cfg.surface=surface;cfg.columns=cfg.rows=2;cfg.origin=RVec3(100,100,100);
    cfg.rotation=Quat::sRotation(Vec3::sAxisX(),.7F);cfg.iterations=12;
    // Compact free cloth keeps XPBD position-difference roundoff below
    // 2e-4 m/s; the 90 Hz force integral is about 125 N s, tolerance .02 N s.
    cfg.dx=cfg.dy=.01F;cfg.knot_half_width=.001F;
    cfg.damping=cfg.clip_stiffness=cfg.clip_damping=0;
    cfg.stretch_compliance=cfg.shear_compliance=cfg.bend_compliance=1e20F;
    cfg.paired_sleep=true;cfg.internally_integrated_clips=true;
    CargoNet net(w.system,1,w.kit->body_id(w.frame),cfg);
    const bool membrane=surface==CargoNetSurface::ContinuousMembrane;
    const unsigned count=membrane?4U:16U;
    const Vec3 material(upper?.75F:.25F,upper?.25F:.75F,0),f(3000,-6000,9000);const float duration=1.0F/90;
    {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
     auto *m=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
     for(unsigned i=0;i<count;++i)m->GetVertex(i).mInvMass=1.0F/(1+i);}
    const auto saved=net.capture();
    const auto advance=[&](float dt){const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
        auto *m=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
        m->CustomUpdate(dt,lock.GetBody(),w.system);};
    const auto verify=[&](float elapsed){const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
        const auto &body=lock.GetBody();const auto *m=static_cast<const SoftBodyMotionProperties *>(body.GetMotionProperties());
        Vec3 momentum=Vec3::sZero(),sample=Vec3::sZero();
        const float weights[4]={.046875F,.015625F,.140625F,.046875F};
        const float triangle_weights[4]={.25F,upper?.5F:0.0F,upper?0.0F:.5F,.25F};float inverse=0;
        for(unsigned i=0;i<count;++i){const auto &v=m->GetVertex(i);const float weight=membrane?triangle_weights[i]:weights[i/4];
            const auto velocity=body.GetRotation()*v.mVelocity;
            const auto expected=f*(elapsed*weight*v.mInvMass);
            if((velocity-expected).Length()>=2e-4F)std::cout<<"FORCE_VERTEX i="<<i<<" elapsed="<<elapsed<<" actual="<<velocity.GetX()<<","<<velocity.GetY()<<","<<velocity.GetZ()<<" expected="<<expected.GetX()<<","<<expected.GetY()<<","<<expected.GetZ()<<'\n';
            check((velocity-expected).Length()<2e-4F,"each live mass receives transpose share continuously");
            momentum+=velocity/v.mInvMass;sample+=velocity*weight;inverse+=weight*weight*v.mInvMass;}
        check((momentum-f*elapsed).Length()<.02F,"total momentum equals one finite force integral");
        check((sample-f*(elapsed*inverse)).Length()<2e-4F,"material velocity uses the same transpose weights");};
    auto &b=w.system.GetBodyInterface();const auto frame=w.kit->body_id(w.frame);
    b.DeactivateBody(net.body());b.DeactivateBody(frame);
    net.force(material,Vec3::sZero(),duration);
    check(!b.IsActive(net.body())&&!b.IsActive(frame),"zero force does not wake pair");
    net.force(material,f,duration);check(b.IsActive(net.body())&&b.IsActive(frame),"nonzero force wakes both bodies");
    verify(0);bool rejected=false;try{net.force(material,f,duration);}catch(const std::logic_error &){rejected=true;}
    check(rejected,"overlap rejects instead of replacing remaining work");
    for(int i=1;i<=4;++i){advance(1.0F/360);verify(duration*i/4);}
    advance(1.0F/360);verify(duration);
    net.restore(saved);
    {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
     static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties())->SetNumIterations(1);}
    net.force(material,f,duration);
    for(int i=1;i<=48;++i){advance(duration/48);verify(duration*i/48);}
    advance(duration/48);verify(duration);
    net.restore(saved);
    {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
     static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties())->SetNumIterations(12);}
    net.force(material,f,duration);net.restore(saved);advance(1.0F/360);verify(0);
    for(float invalid:{-1.0F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        rejected=false;try{net.force(material,f,invalid);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"invalid duration rejects before state change");}
    rejected=false;try{net.force(Vec3(std::numeric_limits<float>::infinity(),0,0),f,duration);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected,"nonfinite material rejects before indexing");
    rejected=false;try{net.force(material,Vec3(0,std::numeric_limits<float>::quiet_NaN(),0),duration);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected,"nonfinite force rejects before state change");
    net.force(material,f,duration*.37F);for(int i=0;i<4;++i)advance(1.0F/360);verify(duration*.37F);
    net.restore(saved);const auto empty=net.capture();advance(1.0F/360);const auto baseline=net.capture();
    net.restore(empty);net.force(material,Vec3::sZero(),duration);advance(1.0F/360);
    check(net.capture()==baseline,"empty and zero queues preserve identical native state");
    cfg.internally_integrated_clips=false;CargoNet legacy(w.system,1,frame,cfg);const auto legacy_saved=legacy.capture();
    legacy.impulse(material,f*duration);const auto kicked=legacy.capture();legacy.restore(legacy_saved);
    legacy.force(material,f,duration);check(legacy.capture()==kicked,"legacy force is exactly original impulse arithmetic");
    std::cout<<"CATCH_FORCE phases=4 native_callbacks=48 individually_checked_callbacks=48 transpose/live_mass/momentum/wake/overlap/restore/legacy PASS\n";
}
void native_tether(){World w;w.system.SetGravity(Vec3::sZero());auto &b=w.system.GetBodyInterface();const auto frame=w.kit->body_id(w.frame);
    b.SetPosition(frame,RVec3(99,98,100),EActivation::Activate);
    CargoNetConfig cfg;cfg.surface=CargoNetSurface::ContinuousMembrane;cfg.columns=cfg.rows=2;cfg.origin=RVec3(100,100,100);
    cfg.dx=cfg.dy=.01F;cfg.knot_half_width=.001F;cfg.iterations=1;cfg.internally_integrated_clips=true;
    cfg.damping=cfg.clip_stiffness=cfg.clip_damping=0;cfg.stretch_compliance=cfg.shear_compliance=cfg.bend_compliance=1e20F;
    cfg.tether_enabled=true;cfg.tether_material=Vec3(.25F,.75F,0);cfg.tether_stiffness=1000;cfg.tether_damping=40;
    const RVec3 anchor=cfg.origin+RVec3(.0025,-.9925,0);
    cfg.tether_anchor_body_local=Vec3(b.GetWorldTransform(frame).Inversed()*anchor);
    const auto centre=b.GetCenterOfMassPosition(frame);
    for(const bool taut:{false,true}){
        b.SetLinearAndAngularVelocity(frame,Vec3::sZero(),Vec3::sZero());cfg.tether_rest_length_m=taut?.5F:2.0F;
        CargoNet net(w.system,1,frame,cfg);
        {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
         auto *m=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
         for(unsigned i=0;i<4;++i)m->GetVertex(i).mInvMass=1.0F/(1+i);}
        const auto saved=net.capture();
        const auto measure=[&](){const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
            const auto &body=lock.GetBody();const auto *m=static_cast<const SoftBodyMotionProperties *>(body.GetMotionProperties());
            Vec3 linear=Vec3::sZero(),angular=Vec3::sZero();double energy=w.kit->body_kinetic_energy(w.frame)+net.tether_elastic_energy();
            for(const auto &v:m->GetVertices()){
                const auto momentum=body.GetRotation()*v.mVelocity/v.mInvMass;linear+=momentum;
                angular+=Vec3(body.GetCenterOfMassTransform()*RVec3(v.mPosition)-centre).Cross(momentum);
                energy+=.5*v.mVelocity.LengthSq()/v.mInvMass;}
            for(const auto &edge:m->GetSettings()->mEdgeConstraints){const double extension=
                (m->GetVertex(edge.mVertex[1]).mPosition-m->GetVertex(edge.mVertex[0]).mPosition).Length()-edge.mRestLength;
                energy+=.5*extension*extension/edge.mCompliance;}
            return std::make_pair(std::make_pair(linear,angular),energy);};
        check((net.tether_anchor_endpoint()-anchor).Length()<2e-5,"tether anchor converts authored body origin to actual COM space");
        const auto initial=measure();const double preload=net.tether_elastic_energy();
        check(taut?preload>100:preload==0,"initial positive tether preload is included in measured total energy");
        check(std::abs(initial.second-preload)<1e-8,"resting tether has no unaccounted kinetic or strain energy");
        {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
         auto *m=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());m->CustomUpdate(1.0F/360,lock.GetBody(),w.system);}
        net.refresh();const auto after=measure();const auto frame_linear=b.GetLinearVelocity(frame)*w.kit->body_mass(w.frame);
        Vec3 frame_angular;{const BodyLockRead lock(w.system.GetBodyLockInterface(),frame);const auto &body=lock.GetBody();
            frame_angular=body.GetMotionProperties()->GetInverseInertiaForRotation(Mat44::sRotation(body.GetRotation())).Inversed3x3()*body.GetAngularVelocity();}
        const float linear_error=(after.first.first+frame_linear-initial.first.first).Length();
        const float angular_error=(after.first.second+frame_angular-initial.first.second).Length();
        check(linear_error<5e-4F&&angular_error<5e-4F,"actual native tether preserves cloth plus frame linear and angular momentum");
        check(std::isfinite(after.second)&&after.second<=initial.second+1e-4,"actual native kinetic plus geometric elastic energy cannot increase");
        if(taut){check(std::isfinite(net.tether_tension())&&net.tether_tension()>0&&after.first.first.Length()>.1F&&frame_linear.Length()>.1F&&frame_angular.Length()>.1F,
                       "taut native tether transfers finite tension and nonzero reciprocal point torque");
            net.restore(saved);check(net.tether_tension()==0&&net.tether_dissipated_work()==0,"restore clears tether transient diagnostics");
            auto invalid=cfg;invalid.tether_rest_length_m=0;bool rejected=false;
            try{CargoNet bad(w.system,1,frame,invalid);}catch(const std::invalid_argument &){rejected=true;}
            check(rejected,"invalid enabled tether rejects before native creation");
        }else check(net.tether_tension()==0&&after.second<1e-8&&frame_linear.LengthSq()==0&&frame_angular.LengthSq()==0,
                   "slack native tether adds no tension or kinetic energy");
        std::cout<<"CATCH_TETHER taut="<<taut<<" preload_j="<<preload<<" initial_j="<<initial.second<<" final_j="<<after.second
                 <<" linear_error="<<linear_error<<" angular_error="<<angular_error<<'\n';
    }
}
// Read the actual authored native rows; a freely moving cosmetic pole fails.
void piston_route(){World w;auto &b=w.system.GetBodyInterface();
    const auto top=w.kit->body_for_entity(1977);
    check(top.valid()&&w.net->config().piston_body==w.kit->body_id(top),"cloth attachment names actual telescopic top body");
    float low=float((b.GetWorldTransform(w.kit->body_id(top))*RVec3(w.net->config().piston_body_local)).GetY()),high=low;
    unsigned rows=0;
    for(std::uint64_t entity=1975;entity<=1977;++entity){
        const auto stage=w.kit->body_for_entity(entity);check(stage.valid(),"three actual telescopic stage bodies exist");
        const float mass=w.kit->body_mass(stage);
        check(std::isfinite(mass)&&mass>0,"telescopic stage has finite positive native mass");
        const BodyID id=w.kit->body_id(stage);unsigned matching=0;
        for(const auto &constraint:w.system.GetConstraints())if(constraint->GetSubType()==EConstraintSubType::Slider){
            const auto *slider=static_cast<const SliderConstraint *>(constraint.GetPtr());
            if(slider->GetBody2()->GetID()!=id)continue;
            ++matching;++rows;
            check(slider->GetBody1()->GetID()==w.kit->body_id(entity==1975?w.frame:w.kit->body_for_entity(entity-1)),"nested slider has actual preceding carrier");
            check(slider->HasLimits()&&std::abs(slider->GetLimitsMax()-slider->GetLimitsMin()-.95F)<1e-5F,
                  "each actual slider has finite authored .95 metre stroke");
            check(slider->GetMaxFrictionForce()>0&&std::isfinite(slider->GetMaxFrictionForce())&&slider->GetMotorState()==EMotorState::Off,
                  "sliding restriction has finite passive friction and no driving motor");
            check(std::abs(slider->GetCurrentPosition())<1e-5F,"native slider initial offset is accounted in travel limits");
            low+=slider->GetLimitsMin();high+=slider->GetLimitsMax();
        }
        check(matching==1,"each telescopic stage owns exactly one actual slider row");
    }
    check(rows==3&&std::abs(low-1.56F)<.01F&&std::abs(high-4.41F)<.01F,"actual nested constraints yield full 1.56 to 4.41 metre eye stroke");
    std::cout<<"CATCH_PISTON_ROUTE rows="<<rows<<" low_y="<<low<<" high_y="<<high<<'\n';
}
// Isolate the same native attachment owner from gravity, mounts and slider
// reactions: actual native steps advance both endpoints. Include body spin
// and orbital momentum about the same origin; no unmeasured external load.
void native_piston(int iterations=1){World w;w.system.SetGravity(Vec3::sZero());auto &b=w.system.GetBodyInterface();
    // An isotropic receiver isolates attachment torque from Jolt's default
    // disabled gyroscopic correction; COM offset and all3 torque axes remain.
    kit::Part part;part.half=Vec3(.2F,.2F,.2F);part.offset=Vec3(.1F,-.05F,.04F);
    const auto piston=w.kit->add_body(1998,{part},RVec3(100,100,100),Quat::sIdentity(),7,.2F);
    w.kit->set_damping(piston,0,0);const auto id=w.kit->body_id(piston),frame=w.kit->body_id(w.frame);
    CargoNetConfig cfg;cfg.surface=CargoNetSurface::ContinuousMembrane;cfg.columns=cfg.rows=2;
    cfg.origin=RVec3(100.1,100.2,100.4);cfg.dx=cfg.dy=.1F;cfg.knot_half_width=.01F;
    cfg.iterations=iterations;cfg.internally_integrated_clips=true;cfg.damping=cfg.clip_stiffness=cfg.clip_damping=0;
    cfg.stretch_compliance=cfg.shear_compliance=cfg.bend_compliance=1e20F;
    cfg.piston_body=id;cfg.piston_material=Vec3(.25F,.75F,0);
    cfg.piston_body_local=Vec3(.145F,.245F,.44F);cfg.piston_stiffness=40000;cfg.piston_damping=400;
    CargoNet net(w.system,1,frame,cfg);const auto centre=b.GetCenterOfMassPosition(id);
    check((net.piston_anchor_endpoint()-(b.GetWorldTransform(id)*RVec3(cfg.piston_body_local))).Length()<2e-5,
          "piston attachment converts authored local point through actual displaced centre of mass");
    const auto energy=[&](){double total=w.kit->body_kinetic_energy(piston)+net.piston_elastic_energy();
        const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
        const auto *m=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
        for(const auto &v:m->GetVertices())total+=.5*v.mVelocity.LengthSq()/v.mInvMass;
        for(const auto &edge:m->GetSettings()->mEdgeConstraints){const double extension=
            (m->GetVertex(edge.mVertex[1]).mPosition-m->GetVertex(edge.mVertex[0]).mPosition).Length()-edge.mRestLength;
            total+=.5*extension*extension/edge.mCompliance;}
        return total;};
    const auto cloth_momentum=[&](){const BodyLockRead lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
        const auto &body=lock.GetBody();const auto *m=static_cast<const SoftBodyMotionProperties *>(body.GetMotionProperties());
        Vec3 linear=Vec3::sZero(),angular=Vec3::sZero();
        for(const auto &v:m->GetVertices()){const auto momentum=body.GetRotation()*v.mVelocity/v.mInvMass;
            linear+=momentum;angular+=Vec3(body.GetCenterOfMassTransform()*RVec3(v.mPosition)-centre).Cross(momentum);}
        return std::make_pair(linear,angular);};
    {const BodyLockWrite lock(w.system.GetBodyLockInterfaceNoLock(),net.body());
     auto *m=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
     for(unsigned i=0;i<4;++i)m->GetVertex(i).mInvMass=1.0F/(1+i);}
    Listener piston_listener;piston_listener.net=&net;
    w.system.AddStepListener(&piston_listener);
    struct RemoveListener {
        PhysicsSystem &system;PhysicsStepListener *listener;
        ~RemoveListener(){system.RemoveStepListener(listener);}
    } listener_lifetime{w.system,&piston_listener};
    net.impulse(cfg.piston_material,Vec3(.3F,-.5F,.7F));const auto before=cloth_momentum();const double initial_energy=energy();
    // No initial collision overlap; advance the complete physical pair, not
    // CustomUpdate of cloth alone with the rigid endpoint frozen in place.
    for(int step=0;step<4;++step) {
        check(w.system.Update(1.0F/360,1,&w.temp,&w.jobs)==EPhysicsUpdateError::None,
              "isolated complete native pair advances without capacity failure");
        net.refresh();const double step_energy=energy();
        std::cout<<"CATCH_PISTON_STEP tick="<<step<<" energy="<<step_energy<<std::endl;
        check(step_energy<=initial_energy+1e-4,"actual complete pair cannot create unpaid energy at an intermediate tick");
    }
    const auto after=cloth_momentum();const auto rigid=b.GetLinearVelocity(id)*w.kit->body_mass(piston);
    Vec3 angular;{const BodyLockRead lock(w.system.GetBodyLockInterface(),id);const auto &body=lock.GetBody();
        angular=body.GetMotionProperties()->GetInverseInertiaForRotation(Mat44::sRotation(body.GetRotation())).Inversed3x3()*body.GetAngularVelocity()+
                Vec3(body.GetCenterOfMassPosition()-centre).Cross(rigid);}
    const float linear_error=(after.first+rigid-before.first).Length(),angular_error=(after.second+angular-before.second).Length();
    net.refresh();const double final_energy=energy();
    std::cout<<"CATCH_PISTON_ENERGY initial="<<initial_energy<<" final="<<final_energy<<" stored="<<net.piston_elastic_energy()<<" radial_loss="<<net.piston_dissipated_work()<<std::endl;
    check(std::isfinite(final_energy)&&final_energy<=initial_energy+1e-4,"isolated actual piston spring and dashpot cannot add unpaid kinetic or elastic energy");
    check(std::abs(rigid.GetX())>.001F&&std::abs(rigid.GetY())>.001F&&std::abs(rigid.GetZ())>.001F&&angular.Length()>.001F,
          "actual three dimensional piston attachment transposes finite offaxis impulse and body point torque");
    std::cout<<"CATCH_PISTON_MOMENTUM linear="<<linear_error<<" angular="<<angular_error<<std::endl;
    check(linear_error<5e-4F&&angular_error<5e-4F,"actual cloth plus finite piston conserve equal opposite linear and angular impulse");
    std::cout<<"CATCH_PISTON iterations="<<iterations<<" initial_energy_j="<<initial_energy<<" final_energy_j="<<final_energy<<" impulse="<<rigid.Length()<<" torque_impulse="<<angular.Length()<<" linear_error="<<linear_error<<" angular_error="<<angular_error<<'\n';
}
void sleeping(){World w;settle(w);const auto state=vertex_state(w);const auto point=w.net->world_point(Vec3(12,12,0));const auto saved=w.net->capture();
    for(int i=0;i<90;++i){w.tick();check(asleep(w),"native paired sleep remains sustained for every real slider stage");}
    const auto after=vertex_state(w);check(after.size()==state.size(),"sleep retains topology");
    for(std::size_t i=0;i<state.size();++i){
        check((state[i].first-after[i].first).LengthSq()==0,"sleep preserves every vertex position");
        check((state[i].second-after[i].second).LengthSq()==0,"sleep does not accumulate clip velocity");
    }
    auto &b=w.system.GetBodyInterface();const auto frame=w.kit->body_id(w.frame);
    w.net->impulse(Vec3(12,12,0),Vec3::sZero());check(asleep(w),"zero impulse does not create a wake event");
    w.net->impulse(Vec3(12,12,0),Vec3(0,-10,0));check(b.IsActive(w.net->body())&&b.IsActive(frame),"material impulse wakes both native bodies");
    w.tick();check((w.net->world_point(Vec3(12,12,0))-point).Length()>.001F,"material wake enters actual solver");
    const auto frame_point=b.GetCenterOfMassPosition(frame);float rebound=0,frame_motion=0;
    for(int i=0;i<90;++i){w.tick();rebound=std::max(rebound,w.net->material_velocity(Vec3(12,12,0)).GetY());
        frame_motion=std::max(frame_motion,float((b.GetCenterOfMassPosition(frame)-frame_point).Length()));}
    check(rebound>.01F,"woken material impulse produces natural upward rebound");
    check(frame_motion>0,"woken material transfers reaction into the finite mounted frame");
    std::cout<<"CATCH_SLEEP material_rebound_vy="<<rebound<<" frame_motion="<<frame_motion<<'\n';
    settle(w);b.AddImpulse(frame,Vec3(1000,0,0));check(b.IsActive(frame),"real frame impulse wakes native frame");
    w.tick();check(b.IsActive(w.net->body()),"moving physical frame wakes net before clips");
    settle(w);const auto lower=w.kit->body_id(w.kit->body_for_entity(kCatchPistonFirstEntity));
    b.AddImpulse(lower,Vec3(0,1,0));check(b.IsActive(lower),"real lower slider impulse wakes its native body");
    w.tick();check(b.IsActive(w.net->body())&&b.IsActive(frame),"moving lower slider wakes the actual cloth and mounted frame");
    for(auto entity=kCatchPistonFirstEntity;entity<=kCatchPistonTopEntity;++entity)
        check(b.IsActive(w.kit->body_id(w.kit->body_for_entity(entity))),"lower slider load wakes every actual coupled stage");
    settle(w);b.SetPosition(lower,b.GetPosition(lower)+RVec3(0,.001,0),EActivation::DontActivate);
    check(asleep(w),"external pose edit leaves native activation unchanged until owner step");
    w.net->pre_step(1.0F/90);
    check(b.IsActive(w.net->body())&&b.IsActive(frame)&&b.IsActive(lower),"changed inactive lower slider pose wakes the assembly before coupling");
    settle(w);w.net->restore(saved);check(b.IsActive(w.net->body())&&b.IsActive(frame),"restored material state wakes paired bodies");
    std::cout<<"CATCH_SLEEP sustained_ticks=90 material_wake=1 frame_wake=1 lower_stage_wake=1 inactive_stage_pose_wake=1 restore_wake=1\n";
    for(const auto quality:{EMotionQuality::Discrete,EMotionQuality::LinearCast}){
        World impact;check(!asleep(impact),"restart creates active physical net and frame");settle(impact);
        impact.capsule(8,quality);impact.system.GetBodyInterface().SetLinearVelocity(impact.player,Vec3(0,quality==EMotionQuality::LinearCast?-40.0F:-5.0F,0));
        bool woke=false;for(int i=0;i<180&&!impact.contacts.contacts;++i){impact.tick();woke|=impact.system.GetBodyInterface().IsActive(impact.net->body());}
        check(woke&&impact.contacts.contacts>0,"falling rigid body wakes and solves real soft contact");
        std::cout<<"CATCH_SLEEP motion_quality="<<int(quality)<<" wake="<<woke<<" contacts="<<impact.contacts.contacts<<'\n';
    }
}
void fall(float height,float offset_z=0){World w;
    const auto piston_body_energy=[&](){double energy=0;for(std::uint64_t entity=1975;entity<=1977;++entity){
        const auto stage=w.kit->body_for_entity(entity);
        check(stage.valid(),"energy diagnostic includes every actual piston stage");
        energy+=w.kit->body_kinetic_energy(stage)+w.kit->body_mass(stage)*9.81*w.kit->body_center_of_mass_position(stage).GetY();
    }return energy;};
    const double initial_piston_energy=piston_body_energy()+w.net->piston_elastic_energy();
    const double initial_tether_energy=w.net->tether_elastic_energy();for(int i=0;i<180;++i)w.tick();w.capsule(height,EMotionQuality::LinearCast,offset_z);
    auto &b=w.system.GetBodyInterface();float min_vertex=100,min_player=100,min_caught_player=100,max_rebound=0,max_frame_speed=0,peak_ke=0,peak_strain=0,peak_clip_energy=0,peak_exposed_energy=0;
    bool descending=false,reversed=false,contacted=false,separated=false,recontacted=false;const int steps=height>=40?1800:4500;
    float late_peak_speed=0,late_min_y=100,late_max_y=-100;unsigned late_contacts=0;
    float apex=0,first_apex=0,impact=0;float peak_terms[8]{};unsigned clear_ticks=0;
    RVec3 previous_player=b.GetCenterOfMassPosition(w.player);float late_actual_speed=0;
    for(int i=0;i<steps;++i){w.contacts.this_tick=false;w.tick();
        if(w.contacts.this_tick){if(separated)recontacted=true;contacted=true;clear_ticks=0;}
        else if(contacted&&++clear_ticks>=9)separated=true;
        const auto p=b.GetCenterOfMassPosition(w.player);const auto v=b.GetLinearVelocity(w.player);
        if(i>=steps-180){late_peak_speed=std::max(late_peak_speed,v.Length());late_actual_speed=std::max(late_actual_speed,float((p-previous_player).Length()*90));late_min_y=std::min(late_min_y,float(p.GetY()));late_max_y=std::max(late_max_y,float(p.GetY()));if(w.contacts.this_tick)++late_contacts;}
        const float actual_vy=float((p-previous_player).GetY()*90);previous_player=p;
        min_player=std::min(min_player,float(p.GetY()));
        if(w.contacts.this_tick)min_caught_player=std::min(min_caught_player,float(p.GetY()));
        max_frame_speed=std::max(max_frame_speed,b.GetLinearVelocity(w.kit->body_id(w.frame)).Length());
        for(const auto &q:w.net->vertices()){check(std::isfinite(q.GetY()),"finite mesh");min_vertex=std::min(min_vertex,float(q.GetY()));}
        if(v.GetY()<-1){descending=true;impact=std::max(impact,-v.GetY());}
        if(descending&&v.GetY()>0.1F)reversed=true;
        if(separated&&!recontacted)first_apex=std::max(first_apex,float(p.GetY()));
        if(reversed){max_rebound=std::max(max_rebound,v.GetY());apex=std::max(apex,float(p.GetY()));}
        BodyLockRead lock(w.system.GetBodyLockInterface(),w.net->body());const auto *m=static_cast<const SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
        float ke=0;for(const auto &q:m->GetVertices())ke+=.5F*q.mVelocity.LengthSq()/q.mInvMass;peak_ke=std::max(peak_ke,ke);
        float strain=0;for(const auto &edge:m->GetSettings()->mEdgeConstraints)if(edge.mCompliance>0) {
            const float extension=(m->GetVertex(edge.mVertex[0]).mPosition-m->GetVertex(edge.mVertex[1]).mPosition).Length()-edge.mRestLength;
            strain+=.5F*extension*extension/edge.mCompliance;
        }
        peak_strain=std::max(peak_strain,strain);
        if(i>=steps-360&&i%90==89)std::cout<<"CATCH_TAIL tick="<<i+1<<" y="<<p.GetY()<<" velocity_y="<<v.GetY()<<" position_speed_y="<<actual_vy<<" cloth_ke_j="<<ke<<" strain_j="<<strain<<'\n';
        float clip_energy=0;const auto transform=b.GetCenterOfMassTransform(w.kit->body_id(w.frame));
        for(const auto &clip:w.clips){const auto d=transform*clip.second-w.net->vertices()[clip.first];clip_energy+=.5F*w.net->config().clip_stiffness*d.LengthSq();}
        peak_clip_energy=std::max(peak_clip_energy,clip_energy);
        float cloth_gravity=0;for(std::size_t vertex=0;vertex<m->GetVertices().size();++vertex)
            cloth_gravity+=9.81F*w.net->vertices()[vertex].GetY()/m->GetVertex(vertex).mInvMass;
        float mount_energy=0;for(const auto &mount:w.mounts){const auto d=transform*mount.second-mount.first;mount_energy+=.5F*3e6F*d.LengthSq();}
        const auto q=b.GetRotation(w.kit->body_id(w.frame));const float angle=2*std::atan2(q.GetXYZ().Length(),std::abs(q.GetW()));
        // Native mounts have isotropic rotational stiffness. Their measured
        // rotation remains tiny; angle norm gives the small-angle elastic work.
        check(angle<.01F,"mount small-angle energy diagnostic remains valid");
        mount_energy+=4*.5F*5e6F*angle*angle;
        const float frame_gravity=w.kit->body_mass(w.frame)*9.81F*(b.GetCenterOfMassPosition(w.kit->body_id(w.frame)).GetY()-w.frame_reference.GetY());
        const float piston_energy=float(piston_body_energy()+w.net->piston_elastic_energy());
        const float exposed=piston_energy+ke+strain+clip_energy+mount_energy+cloth_gravity+frame_gravity+float(w.net->tether_elastic_energy())+
            85*(.5F*v.LengthSq()+9.81F*p.GetY())+w.kit->body_kinetic_energy(w.frame);
        if(exposed>peak_exposed_energy){peak_exposed_energy=exposed;
            peak_terms[0]=ke;peak_terms[1]=strain;peak_terms[2]=clip_energy;peak_terms[3]=mount_energy;
            peak_terms[4]=cloth_gravity+frame_gravity;peak_terms[5]=85*(.5F*v.LengthSq()+9.81F*p.GetY());peak_terms[6]=w.kit->body_kinetic_energy(w.frame);peak_terms[7]=piston_energy;}

        if(i%450==449)std::cout<<"CATCH_TRACE tick="<<i+1<<" x="<<p.GetX()<<" z="<<p.GetZ()<<" y="<<p.GetY()<<" vy="<<v.GetY()<<" sag_min="<<min_vertex<<" rebound="<<max_rebound<<" apex="<<apex<<" strain="<<strain<<std::endl;
    }
    const auto final=b.GetCenterOfMassPosition(w.player),velocity=b.GetLinearVelocity(w.player);
    std::cout<<"CATCH_MODEL height="<<height<<" offset_z="<<offset_z<<" impact_speed="<<impact<<" mesh_min_y="<<min_vertex<<" caught_capsule_min_y="<<min_caught_player<<" capsule_min_y="<<min_player<<" rebound_vy="<<max_rebound<<" apex_y="<<apex<<" first_apex_y="<<first_apex<<" frame_speed="<<max_frame_speed<<" mesh_peak_ke_j="<<peak_ke<<" edge_peak_strain_j="<<peak_strain<<" clip_peak_strain_j="<<peak_clip_energy<<" exposed_peak_energy_j="<<peak_exposed_energy<<" final_y="<<final.GetY()<<" final_speed="<<velocity.Length()<<" contact_events="<<w.contacts.contacts<<" separation="<<separated<<" recontact="<<recontacted<<" late_peak_speed="<<late_peak_speed<<" late_y_range="<<(late_max_y-late_min_y)<<" late_contact_ticks="<<late_contacts<<" simulated_seconds="<<steps/90.0F<<" listener_calls="<<w.listener.calls<<'\n';
    std::cout<<"CATCH_ENERGY simultaneous_peak net_ke="<<peak_terms[0]<<" edge="<<peak_terms[1]<<" clips="<<peak_terms[2]<<" mounts="<<peak_terms[3]<<" gravity="<<peak_terms[4]<<" player="<<peak_terms[5]<<" frame_ke="<<peak_terms[6]<<" piston_gravity_ke_elastic_j="<<peak_terms[7]<<" tether_stored_j="<<w.net->tether_elastic_energy()<<" tether_radial_loss_j="<<w.net->tether_dissipated_work()<<" late_actual_position_speed="<<late_actual_speed<<'\n';
    check(w.listener.calls==4*unsigned(180+steps),"actual four-substep90Hz cadence");
    check(min_vertex<3.7F,"actual sag under load");check(reversed&&max_rebound>1,"neutral elastic rebound");
    check(min_vertex>.12F&&min_caught_player>.95F,"mesh and contacted capsule retain grade clearance");
    check(contacted&&separated&&recontacted,"real solved soft contacts separate and recontact");
    if(height>=40)check(first_apex>.9F+float(scraperx::sim::Simulation::kLethalImpactSpeedMps*scraperx::sim::Simulation::kLethalImpactSpeedMps)/(2*9.81F),"first rebound can supply rigid fatal grade descent");
    else {
        check(final.GetY()>1.2&&final.GetX()>-40&&final.GetX()<-28&&final.GetZ()>-156&&final.GetZ()<-144,
              "lower drop settles on actual net above grade");
        check(late_contacts>=171,"settled capsule has sustained actual solved net contact");
    }
    check(max_frame_speed>.0001F&&max_frame_speed<2,"finite reactive mounted frame");
    // Conservative native energy falsifier: finite solver/discretization tolerance;
    // no solved contact-impulse or dissipation receipt is available here.
    check(peak_exposed_energy<1.02F*(85*9.81F*height+(150+w.net->config().piston_pad_mass_kg)*9.81F*4+initial_tether_energy+initial_piston_energy),
          "native total stored energy cannot create gross unpaid work");
    check(late_peak_speed<.2F&&late_max_y-late_min_y<.05F,"passive settling persists across final two seconds");
}
}
int main(int argc,char **argv){std::cout<<std::unitbuf;RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();
    int result=0;try{if(argc>1&&std::string(argv[1])=="friction"){contact_friction();}else if(argc>1&&std::string(argv[1])=="force"){phased_force();phased_force(CargoNetSurface::ContinuousMembrane);phased_force(CargoNetSurface::ContinuousMembrane,true);native_tether();piston_route();native_piston();native_piston(12);}else{defaults();clip_reciprocity();if(argc>1&&std::string(argv[1])=="clips"){}else if(argc>1&&std::string(argv[1])=="sleep")sleeping();else if(argc>1)fall(std::stof(argv[1]),argc>2?std::stof(argv[2]):0);else{sleeping();fall(45);fall(45,.45F);fall(10);}}std::cout<<"PASS catch native model\n";}
    catch(const std::exception &e){std::cerr<<"FAIL catch native model: "<<e.what()<<'\n';result=1;}
    UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return result;}
