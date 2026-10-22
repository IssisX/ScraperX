#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <memory>
#include <chrono>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>

namespace {
using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::detail;
struct Fixture {
    JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    TempAllocatorImpl temp{32 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    PhysicsSystem physics;
    std::unique_ptr<kit::Kit> kit;
    std::unique_ptr<Matron> matron;
    std::vector<BodyID> solids;
    float min_y=10000, max_x=-10000, penetration=0, controlled_penetration=0, measured_force=0, measured_power=0;
    Fixture(RVec3 start) {
        physics.Init(8192, 0, 16384, 8192, broad, vs, pair);
        physics.SetGravity(Vec3(0,-9.81F,0));
        matron=std::make_unique<Matron>(physics,object_layers::kStatic,object_layers::kMoving,object_layers::kOnSteel,BodyID());
        Matron::Saved saved; saved.state=Matron::State::Hunting;
        matron->restore(saved);
        physics.GetBodyInterface().SetPosition(matron->body(),start,EActivation::Activate); // fixture initial condition only
    }
    ~Fixture() {
        matron.reset(); kit.reset();
        for (auto id:solids) { if (physics.GetBodyInterface().IsAdded(id)) physics.GetBodyInterface().RemoveBody(id); physics.GetBodyInterface().DestroyBody(id); }
    }
    BodyID box(RVec3 at, Vec3 half) {
        BodyCreationSettings settings(new BoxShape(half),at,Quat::sIdentity(),EMotionType::Static,object_layers::kStatic);
        const BodyID id=physics.GetBodyInterface().CreateAndAddBody(settings,EActivation::DontActivate);
        solids.push_back(id); return id;
    }
    void ladder(float x,float bottom,float top,float z=0) {
        for(float y=bottom+0.3F;y<=top;y+=0.3F) box(RVec3(x,y,z),Vec3(0.28F,0.02F,0.02F));
    }
    RVec3 at() { return physics.GetBodyInterface().GetPosition(matron->body()); }
    void tick() {
        constexpr float dt=1.0F/90;
        const auto old_plans=matron->climb_report().plans;
        const auto old_queries=matron->climb_report().native_queries;
        const auto started=std::chrono::steady_clock::now();
        matron->pre_step(dt,RVec3(300,154,300),0);
        if(matron->climb_report().plans>old_plans) std::cout<<"routecraft fault replan pre_step_ms="
            <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
            <<" native_queries="<<matron->climb_report().native_queries-old_queries<<'\n';
        {
            BodyLockRead body(physics.GetBodyLockInterface(),matron->body());
            const Vec3 force=body.GetBody().GetAccumulatedForce();
            require(std::abs(force.Length()-matron->climb_report().force)<0.01F,"routecraft: reported force matches actual body accumulator");
            measured_force=std::max(measured_force,force.Length());
            measured_power=std::max(measured_power,std::abs(force.Dot(body.GetBody().GetLinearVelocity())));
            require(force.Length()<=MatronRoutecraft::kForce+0.1F,"routecraft: actual accumulated force cap");
            require(measured_power<=MatronRoutecraft::kPower+0.1F,"routecraft: actual mechanical power cap");
            if(matron->climb_report().force>0) require((force+matron->climb_report().reaction).Length()<0.01F,"routecraft: equal static support reaction");
        }
        physics.Update(dt,1,&temp,&jobs);
        matron->post_step(dt);
        require(physics.GetBodyInterface().GetGravityFactor(matron->body())==1,"routecraft: gravity stays enabled");
        min_y=std::min(min_y,float(at().GetY())); max_x=std::max(max_x,float(at().GetX()));
        CapsuleShape capsule(Matron::kHalfHeight,Matron::kRadius);
        AllHitCollisionCollector<CollideShapeCollector> hits;
        physics.GetNarrowPhaseQuery().CollideShape(&capsule,Vec3::sReplicate(1),RMat44::sTranslation(at()),
            CollideShapeSettings(),at(),hits,{}, {},IgnoreSingleBodyFilter(matron->body()));
        const auto status=matron->climb_report().status;
        for(const auto &hit:hits.mHits) {
            penetration=std::max(penetration,hit.mPenetrationDepth);
            if(status==MatronRoutecraft::Status::Moving || status==MatronRoutecraft::Status::Holding)
                controlled_penetration=std::max(controlled_penetration,hit.mPenetrationDepth);
        }
        require(controlled_penetration<=0.025F,"routecraft: controlled capsule penetration under 25 mm");
        // Released falls in these fixtures reach under 10 m/s. Jolt may
        // retain one 90 Hz travel step plus contact slop before correction;
        // the recovery checks below also require actual settled floor height.
        require(penetration<=10.0F/90.0F+0.015F,"routecraft: recovery overlap bounded by one impact step");
        const auto &r=matron->climb_report();
        require(r.force<=MatronRoutecraft::kForce+0.01F,"routecraft: finite force");
        require(r.peak_power<=MatronRoutecraft::kPower+0.1F,"routecraft: finite power");
        require(r.max_reach<=MatronRoutecraft::kReach+0.001F,"routecraft: contact reach");
    }
    bool go(RVec3 goal,int ticks=9000) {
        const bool planned=matron->request_climb_goal(goal,Vec3::sAxisZ());
        if (!planned) { std::cout<<"routecraft plan rejected status="<<int(matron->climb_report().status)<<" queries="<<matron->climb_report().queries<<'\n'; return false; }
        for(int i=0;i<ticks;++i) {
            tick();
            if(matron->climb_report().status==MatronRoutecraft::Status::Holding) return Vec3(at()-goal).Length()<0.1F;
            if(matron->climb_report().status!=MatronRoutecraft::Status::Moving) break;
        }
        const auto p=at(); std::cout<<"routecraft stopped at "<<p.GetX()<<','<<p.GetY()<<','<<p.GetZ()<<" status="<<int(matron->climb_report().status)<<" plans="<<matron->climb_report().plans<<'\n';
        return false;
    }
};
}
void run_matron_routecraft() {
    using namespace JPH;
    using namespace scraperx::sim;
    RVec3 replay=RVec3::sZero();
    for(int run=0;run<2;++run) {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        f.ladder(0,154,160);
        require(f.go(RVec3(0,159,-0.55)),"routecraft: fixture ascent executes");
        if(run==0) replay=f.at(); else require(f.at()==replay,"routecraft: identical replay");
        const auto up=f.matron->climb_report();
        require(f.go(RVec3(0,155.2,-0.55)),"routecraft: fixture descent executes");
        std::cout<<"routecraft replay="<<run<<" ascent_y="<<replay.GetY()<<" descent_y="<<f.at().GetY()<<" acquisitions="<<up.acquisitions<<" peak_force="<<up.peak_force<<" peak_power="<<up.peak_power<<'\n';
        f.matron->cancel_climb_goal();
        require(f.matron->climb_report().force==0,"routecraft: exit clears load");
    }
    {
        Fixture f(RVec3(5,155.07,-129.6));
        f.box(RVec3(5,153.9,-130),Vec3(3,0.1F,2));
        f.box(RVec3(5,176.15,-127.5),Vec3(3,0.1F,1.4F));
        f.kit=std::make_unique<kit::Kit>(f.physics,object_layers::kStatic,object_layers::kMoving);
        bands::CounterweightWell well;
        bands::build_counterweight_well(*f.kit,well); // production collision geometry, including its compound ladder
        require(f.go(RVec3(5,177.37,-128.6)),"routecraft: authored ladder ascent and mantle");
        std::cout<<"routecraft authored ascent/mantle y="<<f.at().GetY()<<" grips="<<f.matron->climb_report().acquisitions<<'\n';
        require(f.go(RVec3(5,155.2,-129.6)),"routecraft: authored ladder descent");
        std::cout<<"routecraft authored descent y="<<f.at().GetY()<<'\n';
    }
    {
        Simulation world(InitialSpawn::Deck154);
        world.launch_matron_for_test({5,155.07,-129.6});
        auto started=std::chrono::steady_clock::now();
        const bool planned=world.request_matron_climb_goal({5,177.37,-128.6},{0,0,1});
        std::cout<<"full tower initial plan_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
            <<" native_queries="<<world.matron_climb_queries()<<" status="<<world.matron_climb_status()<<std::endl;
        require(planned,"routecraft: full tower plans ascent");
        for(int tick=0;tick<9000 && world.matron_climb_status()==1;++tick) {
            if(tick==900 || tick==1800) {
                started=std::chrono::steady_clock::now();
                require(world.request_matron_climb_goal({5,177.37,-128.6},{0,0,1}),"routecraft: full tower mid-climb replan request");
                std::cout<<"full tower replan_request_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
                    <<" native_queries="<<world.matron_climb_queries()<<" at_y="<<world.matron_state().position.y<<'\n';
            }
            (void)world.advance_frame(Simulation::kFixedStepSeconds);
        }
        std::cout<<"routecraft full tower ascent y="<<world.matron_state().position.y<<" status="<<world.matron_climb_status()<<'\n';
        require(world.matron_climb_status()==2,"routecraft: full tower ascent executes");
        // One additional held tick lets native floor sensing establish the ring.
        (void)world.advance_frame(Simulation::kFixedStepSeconds);
        require(std::abs(world.matron_state().floor_y-176.25)<0.1,"routecraft: upper level established");
        require(!world.request_matron_climb_goal({5,1000,-128.6},{0,0,1}) && world.matron_climb_status()==2,"routecraft: rejected replacement keeps current hold");
        world.cancel_matron_climb_goal();
        for(int i=0;i<30;++i) (void)world.advance_frame(Simulation::kFixedStepSeconds);
        require(std::abs(world.matron_state().position.y-(176.25+Matron::kStandCentre))<0.025,"routecraft: top transfer released onto actual ring");
        std::cout<<"routecraft full tower grounded top-out y="<<world.matron_state().position.y<<'\n';
        require(world.request_matron_climb_goal({5,155.2,-129.6},{0,0,1}),"routecraft: full tower plans descent");
        for(int tick=0;tick<9000 && world.matron_climb_status()==1;++tick) (void)world.advance_frame(Simulation::kFixedStepSeconds);
        require(world.matron_climb_status()==2,"routecraft: full tower descent executes");
        std::cout<<"routecraft full tower descent y="<<world.matron_state().position.y<<'\n';
        world.cancel_matron_climb_goal();
        for(int i=0;i<30;++i) (void)world.advance_frame(Simulation::kFixedStepSeconds);
        require(world.matron_state().alive && std::abs(world.matron_state().floor_y-154)<0.1,"routecraft: real upper-level descent recovers on lower deck");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,157); f.ladder(1.1F,156,162);
        require(f.go(RVec3(1.1F,161,-0.55)),"routecraft: shifted ladder lateral transfer");
        std::cout<<"routecraft sideways x="<<f.at().GetX()<<" y="<<f.at().GetY()<<" penetration="<<f.penetration<<'\n';
    }
    {
        Fixture f(RVec3(0,158,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,158); f.ladder(1.2F,154,162);
        f.box(RVec3(0.6F,160.75,-0.4),Vec3(0.1F,3.25F,1.5F));
        require(f.go(RVec3(1.2F,161,-0.55)),"routecraft: descent before ascent around obstruction");
        require(f.min_y<156.5F,"routecraft: observed physical backtracking");
        std::cout<<"routecraft backtrack min_y="<<f.min_y<<" final_y="<<f.at().GetY()<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        f.ladder(0,154,161);
        f.box(RVec3(0,158,-0.55),Vec3(3,0.1F,2));
        require(!f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: reachable holds beyond blocked body path refused");
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute,"routecraft: explicit no route");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: removal start");
        for(int i=0;i<90;++i) f.tick();
        for(std::size_t i=1;i<f.solids.size();++i) f.physics.GetBodyInterface().RemoveBody(f.solids[i]);
        f.tick();
        require(f.matron->climb_report().force==0,"routecraft: removed supports carry no load");
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute,"routecraft: missing holds end route");
        f.matron->cancel_climb_goal();
        for(int i=0;i<180;++i) f.tick();
        require(std::abs(f.at().GetY()-155.05)<0.04,"routecraft: released body physically lands");
        std::cout<<"routecraft removal/recovery y="<<f.at().GetY()<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,162); f.ladder(1.1F,154,162);
        require(f.matron->request_climb_goal(RVec3(0,161,-0.55),Vec3::sAxisZ()),"routecraft: changed geometry start");
        for(int i=0;i<90;++i) f.tick();
        f.box(RVec3(0,157.5,-0.55),Vec3(0.35F,0.1F,0.4F));
        for(int i=0;i<3000 && f.matron->climb_report().status==MatronRoutecraft::Status::Moving;++i) f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Holding,"routecraft: new obstruction replans to lateral detour");
        require(f.matron->climb_report().plans>1 && f.max_x>0.8F,"routecraft: detour was physically executed");
        std::cout<<"routecraft replan count="<<f.matron->climb_report().plans<<" detour_x="<<f.max_x<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: grip removal start");
        for(int i=0;i<90;++i) f.tick();
        const RVec3 hand=f.matron->climb_report().hand;
        for(std::size_t i=1;i<f.solids.size();++i) if(Vec3(f.physics.GetBodyInterface().GetPosition(f.solids[i])-hand).Length()<0.05F) f.physics.GetBodyInterface().RemoveBody(f.solids[i]);
        for(int i=0;i<1800 && f.matron->climb_report().status==MatronRoutecraft::Status::Moving;++i) f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Holding && f.matron->climb_report().plans>1,"routecraft: one missing grip locally replans");
        f.matron->cancel_climb_goal();
        require(f.matron->climb_report().force==0,"routecraft: explicit interruption clears contact");
        for(int i=0;i<270;++i) f.tick();
        require(std::abs(f.at().GetY()-155.05)<0.05,"routecraft: interrupted grounded recovery");
        std::cout<<"routecraft interrupted recovery peak_overlap="<<f.penetration<<" controlled_overlap="<<f.controlled_penetration<<'\n';
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: disturbed load start");
        f.physics.GetBodyInterface().AddImpulse(f.matron->body(),Vec3(0,-440,0));
        f.tick();
        require(f.measured_force>4400 && f.measured_power>8900,"routecraft: disturbed motion reaches power limit with finite force");
        std::cout<<"routecraft measured saturation N="<<f.measured_force<<" W="<<f.measured_power<<'\n';
        Matron::Saved saved; saved.state=Matron::State::Dormant;
        f.matron->restore(saved);
        require(f.matron->climb_report().force==0 && f.matron->climb_report().status==MatronRoutecraft::Status::Released,"routecraft: restore clears controller");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: capture interruption start");
        f.tick();
        f.matron->begin_grab(f.at()+Vec3(0,0,0.5F));
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Released && f.matron->climb_report().force==0,"routecraft: capture immediately owns the hands");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: moved support start");
        for(int i=0;i<45;++i) f.tick();
        for(std::size_t i=1;i<f.solids.size();++i) {
            const auto id=f.solids[i];
            f.physics.GetBodyInterface().SetPosition(id,f.physics.GetBodyInterface().GetPosition(id)+Vec3(4,0,0),EActivation::DontActivate);
        }
        f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute && f.matron->climb_report().force==0,"routecraft: displaced anchors invalidate before load");
    }
    {
        // Unfamiliar pitch, missing every third rung, shifted world origin.
        Fixture f(RVec3(2.3,157.07,-3.55));
        f.box(RVec3(2.3,155.9,-4),Vec3(3,0.1F,2));
        int rung=0;
        for(float y=156.27F;y<163;y+=0.37F,++rung) if(rung%3!=1) f.box(RVec3(2.3,y,-3),Vec3(0.28F,0.02F,0.02F));
        require(f.go(RVec3(2.3,162,-3.55)) && f.go(RVec3(2.3,157.2,-3.55)),"routecraft: unfamiliar shifted sparse pitch up/down");
        std::cout<<"routecraft unfamiliar sparse descent y="<<f.at().GetY()<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: selected support before occlusion");
        // Wall misses the capsule but separates its shoulders from every bar.
        f.box(RVec3(0,157.5,-0.15),Vec3(2,3.5F,0.025F));
        f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute && f.measured_force==0,"routecraft: selected occluded contact applies no actual force");
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        MutableCompoundShapeSettings settings;
        settings.AddShape(Vec3(10,158,0),Quat::sIdentity(),new BoxShape(Vec3(0.28F,0.02F,0.02F)));
        settings.AddShape(Vec3(0,158,0),Quat::sIdentity(),new BoxShape(Vec3(0.28F,0.02F,0.02F)));
        Ref<MutableCompoundShape> shape=static_cast<MutableCompoundShape *>(settings.Create().Get().GetPtr());
        BodyCreationSettings body(shape,RVec3::sZero(),Quat::sIdentity(),EMotionType::Static,object_layers::kStatic);
        const BodyID id=f.physics.GetBodyInterface().CreateAndAddBody(body,EActivation::DontActivate); f.solids.push_back(id);
        require(f.matron->request_climb_goal(RVec3(0,157.2,-0.55),Vec3::sAxisZ()),"routecraft: compound contact selected");
        f.tick();
        const Vec3 old_com=shape->GetCenterOfMass();
        shape->RemoveShape(1);
        f.physics.GetBodyInterface().NotifyShapeChanged(id,old_com,false,EActivation::DontActivate);
        f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute && f.matron->climb_report().force==0,"routecraft: stale removed compound child safely releases");
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        BodyCreationSettings bar(new BoxShape(Vec3(0.28F,0.02F,0.02F)),RVec3(0,158,0),Quat::sIdentity(),EMotionType::Kinematic,object_layers::kMoving);
        f.solids.push_back(f.physics.GetBodyInterface().CreateAndAddBody(bar,EActivation::Activate));
        require(!f.matron->request_climb_goal(RVec3(0,157.2,-0.55),Vec3::sAxisZ()),"routecraft: moving support family explicitly rejected");
    }
    {
        Simulation world(InitialSpawn::Deck154);
        world.launch_matron_for_test({11,199.32,-131.41});
        const auto started=std::chrono::steady_clock::now();
        const bool accepted=world.request_matron_climb_goal({11,221.37,-130.31},{0,0,1});
        std::cout<<"held-out panel accepted="<<accepted<<" status="<<world.matron_climb_status()
            <<" plan_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
            <<" native_queries="<<world.matron_climb_queries()<<std::endl;
        require(!accepted && world.matron_climb_status()==int(MatronRoutecraft::Status::BudgetExceeded),
            "routecraft: dense held-out panel explicitly exceeds grip budget");
    }
    std::cout<<"PASS: matron-routecraft\n";
}
