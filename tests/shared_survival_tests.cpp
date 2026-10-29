#include "sim/shared_survival.hpp"
#include "sim/physics_world_support.hpp"
#include <iostream>
#include <stdexcept>

using namespace JPH;
using namespace scraperx::sim;
namespace {
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
struct World {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{8*1024*1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    float dt = 1.0F/120;
    std::vector<BodyID> bodies;
    std::vector<Ref<HingeConstraint>> hinges;
    World() { physics.Init(4096, 0, 2048, 1024, layers, broad, pairs); }
    ~World() {
        for (auto &h : hinges) physics.RemoveConstraint(h);
        for (auto id : bodies) { physics.GetBodyInterface().RemoveBody(id); physics.GetBodyInterface().DestroyBody(id); }
    }
    BodyID box(Vec3 half, RVec3 at, float mass, bool upright=false) {
        BodyCreationSettings settings(new BoxShape(half), at, Quat::sIdentity(),
            mass > 0 ? EMotionType::Dynamic : EMotionType::Static,
            mass > 0 ? detail::object_layers::kMoving : detail::object_layers::kStatic);
        settings.mFriction = 0.8F;
        settings.mLinearDamping = 0;
        settings.mAngularDamping = 0;
        settings.mAllowSleeping = false;
        if (mass > 0) {
            settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = mass;
        }
        if (upright) settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        const auto id = physics.GetBodyInterface().CreateAndAddBody(settings,
            mass > 0 ? EActivation::Activate : EActivation::DontActivate);
        bodies.push_back(id);
        return id;
    }
    HingeConstraint *hinge(BodyID id, RVec3 pivot, Vec3 axis=Vec3::sAxisZ()) {
        HingeConstraintSettings s;
        s.mPoint1 = s.mPoint2 = pivot;
        s.mHingeAxis1 = s.mHingeAxis2 = axis;
        s.mNormalAxis1 = s.mNormalAxis2 = std::abs(axis.GetY()) > 0.9F ? Vec3::sAxisX() : axis.Cross(Vec3::sAxisY()).Normalized();
        BodyLockWrite lock(physics.GetBodyLockInterface(), id);
        auto *h = static_cast<HingeConstraint *>(s.Create(Body::sFixedToWorld, lock.GetBody()));
        physics.AddConstraint(h);
        hinges.push_back(h);
        return h;
    }
    void tick() { physics.Update(dt, 1, &temp, &jobs); }
    void advance(SharedSurvival &s, BodyID actor, int ticks, bool eligible=true) {
        for (int i=0;i<ticks;++i) { s.pre_step(actor, eligible); tick(); }
    }
    float y(BodyID id) { return float(physics.GetBodyInterface().GetPosition(id).GetY()); }
    void move(BodyID id, RVec3 to) {
        physics.GetBodyInterface().SetPosition(id, to, EActivation::Activate);
    }
};
struct Fixture : World {
    BodyID floor = box({4,0.2F,4}, {0,-0.2F,0}, 0);
    BodyID actor = box({0.3F,0.7F,0.3F}, {0,0.7F,0}, 220, true);
    BodyID beam = box({1,0.08F,0.4F}, {1.45F,1.46F,0}, 100);
    HingeConstraint *beam_hinge = hinge(beam, {2.45F,1.46F,0});
    SharedSurvival encounter{physics, {beam, {0.65F,0.65F,0}, 0.7F}};
    Fixture() { tick(); }
};
void finite_support() {
    Fixture w;
    w.advance(w.encounter, w.actor, 600);
    const auto v=w.encounter.snapshot();
    std::cout << "  support y=" << w.y(w.beam) << " N=" << v.applied_n << " cap=" << v.capacity_n << '\n';
    check(v.hands_busy && v.applied_n>500 && v.applied_n<=v.capacity_n+0.01F, "finite support not sustained");
    check(v.capacity_n<=1600 && w.y(w.beam)>1.3F, "support exceeds grip or structure fell");
    w.encounter.pre_step(w.actor,true);
    const BodyLockRead beam(w.physics.GetBodyLockInterface(),w.beam);
    const BodyLockRead actor(w.physics.GetBodyLockInterface(),w.actor);
    const auto a=beam.GetBody().GetAccumulatedForce();
    const auto b=actor.GetBody().GetAccumulatedForce();
    check((a+b).Length()<0.1F && a.Length()>500, "hand effort has no equal reaction");
    std::cout << "PASS shared survival finite load and equal reaction\n";
}
void facing_and_torque() {
    Fixture w;
    w.advance(w.encounter,w.actor,120);
    w.encounter.pre_step(w.actor,true,Quat::sRotation(Vec3::sAxisY(),JPH_PI));
    check(!w.encounter.snapshot().hands_busy,"hand remains behind rotated facing");
    w.physics.GetBodyInterface().SetRotation(w.beam,Quat::sRotation(Vec3::sAxisZ(),0.1F),EActivation::Activate);
    w.move(w.beam,{1.45F,1.54F,0});
    w.encounter.pre_step(w.actor,true);
    check(w.encounter.snapshot().hands_busy,"tilted surface not contacted");
    const BodyLockRead a(w.physics.GetBodyLockInterface(),w.beam);
    const BodyLockRead b(w.physics.GetBodyLockInterface(),w.actor);
    auto torque=[](const Body &body) {
        return body.GetAccumulatedTorque()+Vec3(body.GetCenterOfMassPosition()).Cross(body.GetAccumulatedForce());
    };
    check((torque(a.GetBody())+torque(b.GetBody())).Length()<0.01F,"hand contact creates net world torque");
    std::cout << "PASS shared survival facing and inclined-contact torque conservation\n";
}
void sensor_support() {
    for (int surface=0;surface<2;++surface) {
        Fixture w;
        w.advance(w.encounter,w.actor,120);
        {
            BodyLockWrite lock(w.physics.GetBodyLockInterface(),surface==0 ? w.floor : w.beam);
            if (!lock.Succeeded()) throw std::runtime_error("sensor body missing");
            lock.GetBody().SetIsSensor(true);
        }
        w.encounter.pre_step(w.actor,true);
        check(!w.encounter.snapshot().hands_busy,"sensor surface carries hand support");
    }
    std::cout << "PASS shared survival sensors carry no load\n";
}
void contact_and_overload() {
    {
        Fixture w;
        w.move(w.actor, {0,3,0});
        w.encounter.pre_step(w.actor,true);
        check(!w.encounter.snapshot().hands_busy, "support without palm contact");
    }
    {
        Fixture w;
        w.advance(w.encounter,w.actor,120);
        w.move(w.floor,{0,-2,0});
        w.encounter.pre_step(w.actor,true);
        check(w.encounter.snapshot().applied_n==0, "support without footing");
        w.advance(w.encounter,w.actor,120);
        check(w.y(w.beam)<1.0F, "lost footing does not release load");
    }
    {
        Fixture w;
        w.advance(w.encounter,w.actor,120);
        for(int i=0;i<240;++i) {
            w.encounter.pre_step(w.actor,true);
            check(w.encounter.snapshot().applied_n<=1600, "unbounded overload support");
            w.physics.GetBodyInterface().AddForce(w.beam,{0,-5000,0});
            w.tick();
        }
        check(w.y(w.beam)<1.0F, "overload held indefinitely");
    }
    std::cout << "PASS shared survival contact, footing loss and overload collapse\n";
}
void prop_takes_load() {
    Fixture w;
    w.advance(w.encounter,w.actor,240);
    // A player-deliverable loose body, no latch flag or constraint. The test
    // places it under the free end; production uses the existing carry owner.
    w.box({0.18F,0.69F,0.35F},{0.65F,0.69F,0},40);
    w.advance(w.encounter,w.actor,120);
    w.move(w.actor,{-2,0.7F,0});
    w.advance(w.encounter,w.actor,360);
    check(!w.encounter.snapshot().hands_busy && w.y(w.beam)>1.3F, "physical brace cannot replace her support");
    std::cout << "PASS shared survival loose prop carries route after release\n";
}
void district() {
    Fixture w;
    auto rotor=w.box({0.6F,0.2F,0.2F},{0,4,3},80);
    auto h=w.hinge(rotor,{0,4,3},Vec3::sAxisY());
    auto guard=w.box({0.4F,0.4F,0.1F},{0,4,5},10);
    auto g=w.hinge(guard,{0,4,5},Vec3::sAxisY());
    auto second=w.box({0.6F,0.2F,0.2F},{3,4,3},80);
    auto h2=w.hinge(second,{3,4,3},Vec3::sAxisY());
    auto channel=w.encounter.add_drive({h,g,2,80,0});
    auto channel2=w.encounter.add_drive({h2,g,2,80,0});
    check(w.encounter.start(channel)==SharedSurvival::Restart::Isolated,"unpowered start");
    w.encounter.restore_supply();
    check(w.encounter.start(channel)==SharedSurvival::Restart::Started,"safe start refused");
    w.advance(w.encounter,w.actor,120,false);
    check(std::abs(w.encounter.drive_view(channel).speed_rad_s)>1.5F,"drive has no actual motion");
    check(std::abs(w.encounter.drive_view(channel2).speed_rad_s)<0.01F,"unselected channel started");
    w.encounter.isolate();
    const auto before=w.encounter.drive_view(channel);
    w.advance(w.encounter,w.actor,60,false);
    const auto coast=w.encounter.drive_view(channel);
    check(std::abs(coast.speed_rad_s)>1.5F && std::abs(coast.angle_rad-before.angle_rad)>0.5F,"shutdown freezes motion/geometry");
    w.encounter.restore_supply();
    check(!w.encounter.drive_view(channel).running,"restoring supply auto starts");
    check(w.encounter.start(channel)==SharedSurvival::Restart::Moving,"restart while coasting");
    w.encounter.set_brake(channel,20);
    float last=std::abs(coast.speed_rad_s);
    for(int i=0;i<240;++i) {
        w.advance(w.encounter,w.actor,1,false);
        float now=std::abs(w.encounter.drive_view(channel).speed_rad_s);
        check(now<=last+0.001F,"passive brake adds energy"); last=now;
    }
    check(last<0.01F,"brake did not stop rotor");
    w.physics.GetBodyInterface().SetRotation(guard,Quat::sRotation(Vec3::sAxisY(),0.5F),EActivation::Activate);
    check(w.encounter.start(channel)==SharedSurvival::Restart::GuardOpen,"open guard restart");
    w.physics.GetBodyInterface().SetRotation(guard,Quat::sIdentity(),EActivation::Activate);
    check(w.encounter.start(channel)==SharedSurvival::Restart::Started,"safe selective restart failed");
    w.physics.GetBodyInterface().SetRotation(guard,Quat::sRotation(Vec3::sAxisY(),0.5F),EActivation::Activate);
    w.encounter.pre_step(w.actor,false);
    check(!w.encounter.drive_view(channel).running,"opening guard does not trip drive");
    {
        Fixture loaded;
        auto r=loaded.box({0.6F,0.2F,0.2F},{0,4,3},80);
        auto rh=loaded.hinge(r,{0,4,3},Vec3::sAxisY());
        auto gd=loaded.box({0.4F,0.4F,0.1F},{0,4,5},10);
        auto gh=loaded.hinge(gd,{0,4,5},Vec3::sAxisY());
        const auto c=loaded.encounter.add_drive({rh,gh,2,80,0});
        loaded.encounter.restore_supply();
        check(loaded.encounter.start(c)==SharedSurvival::Restart::Started,"initial start failed");
        loaded.advance(loaded.encounter,loaded.actor,120);
        check(loaded.encounter.snapshot().hands_busy && !loaded.encounter.drive_view(c).running,
              "bracing does not trip district drive");
        check(loaded.encounter.start(c)==SharedSurvival::Restart::HandsOccupied,"restart with hands in mechanism");
    }
    w.encounter.reset();
    check(!w.encounter.supplied() && !w.encounter.snapshot().hands_busy,"restore keeps stale effort/power");
    std::cout << "PASS shared survival district coasting, finite brakes, selective restart and interlocks\n";
}
void power_support_loop() {
    Fixture w;
    auto guard=w.box({0.4F,0.4F,0.1F},{0,4,5},10);
    auto g=w.hinge(guard,{0,4,5},Vec3::sAxisY());
    const auto motor=w.encounter.add_drive({w.beam_hinge,g,-0.2F,2000,2000});
    w.advance(w.encounter,w.actor,30,false);
    w.encounter.restore_supply();
    check(w.encounter.start(motor)==SharedSurvival::Restart::Started,"flap drive did not start");
    w.advance(w.encounter,w.actor,6,false);
    w.encounter.set_brake(motor,0);
    w.encounter.isolate();
    w.advance(w.encounter,w.actor,360);
    check(w.encounter.snapshot().hands_busy && w.y(w.beam)>1.3F,"power loss did not transfer load to hands");
    auto prop=w.box({0.18F,0.69F,0.35F},{0.65F,0.69F,0},40);
    w.advance(w.encounter,w.actor,120);
    w.move(w.actor,{-2,0.7F,0});
    w.advance(w.encounter,w.actor,120);
    check(w.y(w.beam)>1.3F && !w.encounter.snapshot().hands_busy,"prop did not free actor");
    w.encounter.restore_supply();
    check(w.encounter.start(motor)==SharedSurvival::Restart::Started,"stabilised route cannot restart");
    const float before=w.y(w.beam);
    w.advance(w.encounter,w.actor,120);
    check(w.y(w.beam)>before+0.1F,"restart did not change route geometry");
    check(w.y(prop)<0.8F,"motor teleported the loose prop");
    std::cout << "PASS shared survival power loss -> hand load -> loose brace -> released actor -> powered route\n";
}
void lifecycle_and_replay() {
    {
        Fixture w;
        w.physics.GetBodyInterface().SetFriction(w.actor,0);
        w.advance(w.encounter,w.actor,120);
        check(w.encounter.snapshot().hands_busy,"actor did not plant feet");
        w.encounter.pre_step(w.actor,false);
        check(!w.encounter.snapshot().hands_busy && w.physics.GetBodyInterface().GetFriction(w.actor)==0,
              "interruption retains effort or planted friction");
        w.encounter.pre_step(w.actor,true);
        check(w.encounter.snapshot().hands_busy,"valid contact cannot be reacquired");
        w.physics.GetBodyInterface().SetFriction(w.actor,0.6F); // a knockdown owns friction now
        w.encounter.pre_step(w.actor,false);
        check(w.physics.GetBodyInterface().GetFriction(w.actor)==0.6F,"release overwrote knockdown friction");
        w.physics.GetBodyInterface().SetFriction(w.actor,0);
        w.encounter.pre_step(w.actor,true);
        w.encounter.reset();
        check(w.physics.GetBodyInterface().GetFriction(w.actor)==0,"reset retains planted friction");
        w.encounter.pre_step(BodyID(),true);
        check(!w.encounter.snapshot().hands_busy,"invalid actor produces support");
        // Remove the constraint before its body, as required by Jolt ownership.
        w.physics.RemoveConstraint(w.beam_hinge);
        w.hinges.clear();
        w.physics.GetBodyInterface().RemoveBody(w.beam);
        w.physics.GetBodyInterface().DestroyBody(w.beam);
        w.bodies.erase(std::remove(w.bodies.begin(),w.bodies.end(),w.beam),w.bodies.end());
        w.box({1,0.08F,0.4F},{1.45F,1.46F,0},100); // reused index, different sequence
        w.encounter.pre_step(w.actor,true);
        check(!w.encounter.snapshot().hands_busy,"stale structure aliases replacement");
        w.physics.GetBodyInterface().RemoveBody(w.actor);
        w.physics.GetBodyInterface().DestroyBody(w.actor);
        w.bodies.erase(std::remove(w.bodies.begin(),w.bodies.end(),w.actor),w.bodies.end());
        w.encounter.pre_step(w.actor,true);
        check(!w.encounter.snapshot().hands_busy,"destroyed actor produces support");
    }
    {
        Fixture w;
        auto r=w.box({0.5F,0.2F,0.2F},{0,4,3},40);
        auto h=w.hinge(r,{0,4,3},Vec3::sAxisY());
        auto gbody=w.box({0.4F,0.4F,0.1F},{0,4,5},10);
        auto g=w.hinge(gbody,{0,4,5},Vec3::sAxisY());
        const auto c=w.encounter.add_drive({h,g,1,80,80});
        for(int cycle=0;cycle<8;++cycle) {
            w.encounter.restore_supply();
            check(!w.encounter.drive_view(c).running,"power cycle starts motor automatically");
            check(w.encounter.start(c)==SharedSurvival::Restart::Started,"repeated safe start refused");
            w.advance(w.encounter,w.actor,60,false);
            w.encounter.isolate();
            w.advance(w.encounter,w.actor,60,false);
            check(std::abs(w.encounter.drive_view(c).speed_rad_s)<0.01F,"cycle retained unexpected kinetic energy");
        }
        w.physics.RemoveConstraint(h);
        check(w.encounter.start(c)==SharedSurvival::Restart::Unavailable,"removed hinge remains available");
        w.physics.GetBodyInterface().RemoveBody(r);
        w.physics.GetBodyInterface().DestroyBody(r);
        w.bodies.erase(std::remove(w.bodies.begin(),w.bodies.end(),r),w.bodies.end());
        w.hinges.erase(std::remove_if(w.hinges.begin(),w.hinges.end(),[h](const auto &v){return v.GetPtr()==h;}),w.hinges.end());
        check(w.encounter.start(c)==SharedSurvival::Restart::Unavailable,"destroyed rotor can restart");
        check(w.encounter.drive_view(c).speed_rad_s==0,"stale drive dereferences destroyed body");
        w.encounter.reset();
    }
    auto replay=[](float dt,int count) {
        Fixture w; w.dt=dt;
        w.advance(w.encounter,w.actor,count);
        return std::array<float,3>{w.y(w.beam),w.encounter.snapshot().applied_n,w.encounter.snapshot().capacity_n};
    };
    const auto first=replay(1.0F/120,600), repeated=replay(1.0F/120,600), coarse=replay(1.0F/60,300);
    check(first==repeated,"same native inputs do not replay deterministically");
    check(std::abs(first[0]-coarse[0])<0.01F && std::abs(first[1]-coarse[1])<5,
          "support depends materially on 60/120 Hz step");
    std::cout << "PASS shared survival interruption, reacquisition, stale handles, cycling, deterministic replay and step sensitivity\n";
}
void matron_transition() {
    World w;
    const float x=Matron::kLockerX, z=Matron::kSpawnZ;
    w.box({4,0.2F,4},{x,Matron::kDeckTop-0.2F,z},0);
    auto player=w.box({0.1F,0.1F,0.1F},{x+3,Matron::kDeckTop+1,z},80);
    Matron matron(w.physics,detail::object_layers::kStatic,detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel,player);
    Matron::Saved saved; saved.state=Matron::State::Hunting;
    matron.restore(saved);
    for(int i=0;i<300;++i) {
        matron.pre_step(1.0F/120,Stimulus(RVec3(x,Matron::kDeckTop+1,z),0.0F));
        w.tick(); matron.post_step(1.0F/120);
    }
    const auto at=w.physics.GetBodyInterface().GetPosition(matron.body());
    const auto facing=Quat::sRotation(Vec3::sAxisY(),matron.view().yaw);
    auto beam=w.box({1,0.08F,0.4F},at+facing*Vec3(1.45F,0.76F,0),100);
    w.physics.GetBodyInterface().SetRotation(beam,facing,EActivation::Activate);
    w.hinge(beam,at+facing*Vec3(2.45F,0.76F,0),facing*Vec3::sAxisZ());
    SharedSurvival encounter(w.physics,{beam});
    matron.set_shared_survival(&encounter);
    // Her hands go up only in her trap at the bay (matron.hpp, bay.hpp): a
    // falling beam beside her while she hunts is not hers to hold. The bay's
    // own test (sim_tests_bay.cpp) holds the bracing itself: no catch while
    // bracing, her rage still fed, friction back and hostile after release.
    int braced=0;
    for(int i=0;i<120;++i) {
        matron.pre_step(1.0F/120,Stimulus(RVec3(at),1.0F));
        if(matron.bracing_structure()) ++braced;
        w.tick(); matron.post_step(1.0F/120);
    }
    std::cout << "  Matron bracing ticks=" << braced << " of 120, out of her trap\n";
    check(braced==0,"Matron braced a structure out of her trap");
    check(matron.caught(w.physics.GetBodyInterface().GetPosition(matron.body())),"Matron not hostile beside the beam");
    matron.set_shared_survival(nullptr);
    std::cout << "PASS shared survival actual Matron: hands up only in her trap, hostile beside a falling beam\n";
}
}
int main() {
    try { finite_support(); facing_and_torque(); sensor_support(); contact_and_overload(); prop_takes_load(); district(); power_support_loop(); lifecycle_and_replay(); matron_transition(); }
    catch(const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
