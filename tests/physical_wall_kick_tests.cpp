#include "sim/simulation/world.hpp"
#include <Jolt/Core/JobSystemSingleThreaded.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool condition,const char *message) {
    if(condition)return;
    std::cerr<<"FAIL PHYSICAL_WALL_KICK "<<message<<'\n';std::exit(EXIT_FAILURE);
}
void tick(Simulation &s) {
    require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"native public tick");
    require(s.snapshot().death_count==0&&s.snapshot().player_gravity_factor==1,
            "gravity and ordinary failure remain active");
}
void stage(Simulation &s,Vector3 position) {
    require(s.debug_restart_at(position),"existing tall tower-column approach");
    require(s.set_facing(0,1)&&s.set_move_input(-1,0),"parallel facing and inward movement");
}
void reach_wall(Simulation &s) {
    for(int i=0;i<90;++i) {
        tick(s);const auto v=s.snapshot();
        require(!v.player_grounded&&v.traversal_hand_constraint_count==0,
                "wall approach is airborne without a handhold");
        if(v.player_position.x<27.155&&v.player_linear_velocity.x>-.05)return;
    }
    require(false,"real tower face stops inward movement");
}
void public_kick() {
    Simulation s(InitialSpawn::ExteriorGrade,WorldContent::Slingshot);
    stage(s,{27.17,3,-150});reach_wall(s);
    const auto before=s.snapshot();
    require(s.set_move_input(0,0)&&s.request_jump(),"ordinary Jump at actual wall contact");
    bool started=false,loaded=false;double peak_outward=0,peak_upward=-100;
    for(int i=0;i<35;++i) {
        if(i>0)require(s.request_jump(),"repeated Jump cannot renew the same wall kick");
        tick(s);const auto v=s.snapshot();
        require(v.traversal_hand_constraint_count==0,"kick never fabricates a grip");
        if(v.foot_push_start_count>before.foot_push_start_count) {
            started=true;
            require(v.foot_push_start_count==before.foot_push_start_count+1&&
                    v.foot_push_support_entity_id==Simulation::kTowerEntityId,
                    "one finite kick uses the real fresh tower contact");
            if(v.foot_push_active)
                require(v.foot_push_source_contact_tick>=before.tick_index,
                    "active wall foot publishes its actual source-contact receipt");
            require(v.foot_push_peak_load_n<=3500.01&&
                    v.foot_push_command_work_bound_j<=1285.62501&&v.foot_push_stroke_m<=.300001&&
                    v.foot_push_elapsed_seconds<=.250001,
                    "wall kick retains existing force work stroke and lifetime limits");
            loaded|=v.foot_push_last_load_n>0;
            peak_outward=std::max(peak_outward,v.player_linear_velocity.x);
            peak_upward=std::max(peak_upward,v.player_linear_velocity.y);
        }
    }
    const auto after=s.snapshot();
    require(started&&loaded&&peak_outward>1&&peak_upward>before.player_linear_velocity.y+.3,
            "actual foot work changes solved outward and upward momentum");
    require(!after.foot_push_active,"kick ends on its original finite clock or geometry");
    std::cout<<"PASS PHYSICAL_WALL_KICK public_jump outward_m_s="<<peak_outward
        <<" upward_m_s="<<peak_upward<<" work_j="<<after.foot_push_command_work_bound_j<<'\n';
}
void no_wall(bool expired) {
    Simulation s(InitialSpawn::ExteriorGrade,WorldContent::Slingshot);
    stage(s,expired?Vector3{28.3,5,-150}:Vector3{30,5,-150});
    if(!expired)require(s.set_move_input(0,0),"airborne neutral without wall");
    const auto before=s.snapshot();require(s.request_jump(),"ordinary airborne Jump intent");
    for(int i=0;i<(expired?80:25);++i)tick(s);
    require(s.snapshot().foot_push_start_count==before.foot_push_start_count&&
            !s.snapshot().foot_push_active,"absent contact or expired intent cannot start a kick");
    std::cout<<"PASS PHYSICAL_WALL_KICK "<<(expired?"expired_intent":"no_wall")<<'\n';
}
void cancel() {
    Simulation s(InitialSpawn::ExteriorGrade,WorldContent::Slingshot);
    stage(s,{27.17,3,-150});reach_wall(s);
    require(s.request_jump(),"wall Jump before deliberate cancellation");tick(s);
    const auto before=s.snapshot();require(before.foot_push_active,"finite wall kick starts");
    require(s.request_release(),"ordinary release cancels kick");tick(s);
    require(!s.snapshot().foot_push_active&&s.snapshot().foot_push_start_count==before.foot_push_start_count,
            "release clears kick without restarting it");
    std::cout<<"PASS PHYSICAL_WALL_KICK cancel\n";
}
void restart() {
    Simulation s(InitialSpawn::ExteriorGrade,WorldContent::Slingshot);
    stage(s,{27.17,3,-150});reach_wall(s);
    require(s.request_jump(),"real wall kick before restart");tick(s);
    require(s.snapshot().foot_push_active,"restart begins with an actual wall foot");
    require(s.debug_restart_at({30,3,-150})&&s.set_move_input(0,0),
            "restart clears contact and moves to open air");
    const auto before=s.snapshot();
    require(s.request_jump(),"fresh Jump after restart");tick(s);
    require(!s.snapshot().foot_push_active&&
            s.snapshot().foot_push_start_count==before.foot_push_start_count,
            "restart cannot reuse a prior wall's contact or material anchor");
    std::cout<<"PASS PHYSICAL_WALL_KICK restart\n";
}

struct BroadPhase final:JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers()const override{return 2;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l)const override{return JPH::BroadPhaseLayer(l);}
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer)const override{return "wall_kick";}
};
struct BroadFilter final:JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b)const override{return a!=0||b.GetValue()!=0;}
};
struct PairFilter final:JPH::ObjectLayerPairFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b)const override{return a!=0||b!=0;}
};
struct Fixture {
    BroadPhase broad;BroadFilter broad_filter;PairFilter pair_filter;
    JPH::PhysicsSystem system;JPH::TempAllocatorImpl allocator{8*1024*1024};
    JPH::JobSystemSingleThreaded jobs{1024};detail::PlayerContactListener contacts;
    JPH::BodyID player,wall;
    explicit Fixture(bool dynamic=false,bool floor=false,bool sensor=false,float friction=.85F) {
        using namespace JPH;
        system.Init(32,0,64,128,broad,broad_filter,pair_filter);system.SetContactListener(&contacts);
        BodyCreationSettings w(new BoxShape(floor?Vec3(4,.1F,4):Vec3(.1F,4,3)),
            RVec3::sZero(),Quat::sIdentity(),dynamic?EMotionType::Dynamic:EMotionType::Static,dynamic?1:0);
        w.mUserData=9911;w.mFriction=friction;w.mIsSensor=sensor;
        w.mLinearDamping=0;w.mAngularDamping=0;
        if(dynamic){w.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;w.mMassPropertiesOverride.mMass=400;}
        wall=system.GetBodyInterface().CreateAndAddBody(w,EActivation::Activate);
        BodyCreationSettings p(new CapsuleShape(.55F,.35F),floor?RVec3(0,.99,0):RVec3(.44,2,0),
            Quat::sIdentity(),EMotionType::Dynamic,1);
        p.mUserData=Simulation::kPlayerEntityId;p.mFriction=0;p.mLinearDamping=0;p.mAngularDamping=0;
        p.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;p.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
        p.mMassPropertiesOverride.mMass=85;
        player=system.GetBodyInterface().CreateAndAddBody(p,EActivation::Activate);
    }
    ~Fixture(){system.SetContactListener(nullptr);for(auto id:{player,wall}){system.GetBodyInterface().RemoveBody(id);system.GetBodyInterface().DestroyBody(id);}}
    void step(){contacts.begin_collision_step();require(system.Update(1.0F/360,1,&allocator,&jobs)==JPH::EPhysicsUpdateError::None,"actual native contact solve");}
};
struct PushStep final:JPH::PhysicsStepListener {
    PhysicalFootPush &push;explicit PushStep(PhysicalFootPush &p):push(p){}
    void OnStep(const JPH::PhysicsStepListenerContext &context)override{push.pre_step(context.mDeltaTime);}
};
void contacts_and_reaction() {
    using namespace JPH;
    RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();
    for(int negative=0;negative<3;++negative) {
        Fixture f(false,negative==0,negative==1,negative==2?0:.85F);f.step();
        require(!f.contacts.wall_sample().valid,"ground sensor and zero-friction contacts cannot plant a wall foot");
    }
    {
        Fixture f(true);auto &bodies=f.system.GetBodyInterface();
        bodies.SetLinearVelocity(f.player,Vec3(-1,0,2));f.step();
        const auto contact=f.contacts.wall_sample();require(contact.valid,"actual dynamic wall manifold supplies foot point");
        const auto at=bodies.GetCenterOfMassPosition(f.player);
        require(contact.body==f.wall&&contact.shape_identity!=nullptr&&contact.world_point.GetY()<at.GetY()&&
                std::abs(contact.world_point.GetX()-.1)<.03,
                "selected point stays on actual wall patch below COM");
        PhysicalFootPush push(f.system,f.player);PushStep step(push);f.system.AddStepListener(&step);
        require(push.begin_passive_plant(contact.body,contact.world_point,contact.world_normal,contact.friction)&&
                push.activate_planted_push(),"existing finite foot motor activates on actual contact");
        const auto initial_player=bodies.GetLinearVelocity(f.player),initial_wall=bodies.GetLinearVelocity(f.wall);
        const auto initial_momentum=85*initial_player+400*initial_wall;
        double elapsed=0;bool loaded=false;float peak_outward=0;
        for(int i=0;i<60;++i) {
            f.step();push.post_step(1.0F/360);elapsed+=1.0/360;
            const auto &r=push.readback();loaded|=r.last_load_n>0;
            require(r.peak_load_n<=3500.01F&&r.command_work_bound_j<=1285.62501&&
                    r.last_command_work_bound_j<=5000.0/360+.0001&&r.elapsed_seconds<=.250001,
                    "actual collision impulses obey existing finite force power work and clock");
            peak_outward=std::max(peak_outward,bodies.GetLinearVelocity(f.player).GetX()-initial_player.GetX());
        }
        const auto final_momentum=85*bodies.GetLinearVelocity(f.player)+400*bodies.GetLinearVelocity(f.wall);
        const auto residual=final_momentum-initial_momentum-Vec3(0,float(-9.81*485*elapsed),0);
        require(loaded&&peak_outward>1&&bodies.GetLinearVelocity(f.wall).GetX()<initial_wall.GetX()-.1F,
                "dynamic wall receives opposite actual kick reaction");
        require(residual.Length()<.03F,"paired linear momentum closes with actual gravity");
        // The anchored radial leg can add tangent impulse as the body moves;
        // it must retain the incoming tangent motion rather than reset it.
        require(bodies.GetLinearVelocity(f.player).GetZ()>=initial_player.GetZ()-.001F,
                "kick retains incoming wall-tangent momentum");
        f.contacts.begin_collision_step();require(!f.contacts.wall_sample().valid,"a previous contact cannot renew itself");
        f.system.RemoveStepListener(&step);
        std::cout<<"PASS PHYSICAL_WALL_KICK actual_dynamic_contact reciprocal_residual_ns="<<residual.Length()
            <<" outward_gain_m_s="<<peak_outward
            <<" tangent_m_s="<<bodies.GetLinearVelocity(f.player).GetZ()<<" work_j="<<push.readback().command_work_bound_j<<'\n';
    }
    UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;
}
}
int main(int argc,char **argv) {
    const std::string mode=argc>1?argv[1]:"public";
    if(mode=="contacts")contacts_and_reaction();
    else if(mode=="no-wall")no_wall(false);
    else if(mode=="expired")no_wall(true);
    else if(mode=="cancel")cancel();
    else if(mode=="restart")restart();
    else public_kick();
}
