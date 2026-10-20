#include "sim/portable_traversal_line.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float h=1.F/360;
void require(bool value,const char *why) { if(!value) throw std::runtime_error(why); }
struct BP final:BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "material-crossing"; }
};
struct BV final:ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
struct LP final:ObjectLayerPairFilter { bool ShouldCollide(ObjectLayer a,ObjectLayer b) const override { return a || b; } };
struct Contacts final:ContactListener {
    BodyID player;bool grounded=false;
    void observe(const Body &a,const Body &b,const ContactManifold &m) {
        if(a.GetID()==player && -m.mWorldSpaceNormal.GetY()>.7F) grounded=true;
        if(b.GetID()==player && m.mWorldSpaceNormal.GetY()>.7F) grounded=true;
    }
    void OnContactAdded(const Body &a,const Body &b,const ContactManifold &m,ContactSettings &) override { observe(a,b,m); }
    void OnContactPersisted(const Body &a,const Body &b,const ContactManifold &m,ContactSettings &) override { observe(a,b,m); }
};
struct Fixture final:PhysicsStepListener {
    BP bp;BV bv;LP lp;PhysicsSystem world;Contacts contacts;
    TempAllocatorImpl allocator{8*1024*1024};JobSystemSingleThreaded jobs{1024};
    BodyID player,first_beam,second_beam;
    std::unique_ptr<PhysicalHandClimb> hands;
    std::unique_ptr<PortableTraversalLine> line;
    unsigned ticks=0;
    Fixture() {
        world.Init(64,0,128,128,bp,bv,lp);
        BodyCreationSettings p(new CapsuleShape(.55F,.35F),RVec3(0,.9F,0),Quat::sIdentity(),EMotionType::Dynamic,1);
        p.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;p.mMassPropertiesOverride.mMass=85;
        p.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
        p.mAllowSleeping=false;p.mLinearDamping=0;
        player=world.GetBodyInterface().CreateAndAddBody(p,EActivation::Activate);
        contacts.player=player;world.SetContactListener(&contacts);
        first_beam=box(RVec3(0,2.9F,0),Vec3(.1F,.1F,1));
        second_beam=box(RVec3(4,2.9F,0),Vec3(.1F,.1F,1));
        box(RVec3(0,-.15F,0),Vec3(.65F,.15F,1));
        box(RVec3(4,-.15F,0),Vec3(.65F,.15F,1));
        box(RVec3(2,-.15F,-1.8F),Vec3(2.65F,.15F,.8F));
        box(RVec3(2,-3.15F,0),Vec3(8,.15F,8));
        hands=std::make_unique<PhysicalHandClimb>(world,player);
        line=std::make_unique<PortableTraversalLine>(world,player,1,*hands);
        world.AddStepListener(this);line->equip(true);step(30);aim();
    }
    ~Fixture() { world.RemoveStepListener(this);line.reset();hands.reset();world.SetContactListener(nullptr); }
    BodyID box(RVec3 p,Vec3 half) {
        BodyCreationSettings s(new BoxShape(half),p,Quat::sIdentity(),EMotionType::Static,0);
        s.mFriction=.35F;return world.GetBodyInterface().CreateAndAddBody(s,EActivation::DontActivate);
    }
    void aim(Vec3 direction=Vec3::sAxisY()) { line->aim(world.GetBodyInterface().GetPosition(player)+Vec3(0,.6F,0),direction,true); }
    void OnStep(const PhysicsStepListenerContext &context) override {
        line->collision_step(context.mDeltaTime);hands->pre_step(context.mDeltaTime,true);
    }
    void step(unsigned count=1) {
        for(unsigned i=0;i<count;++i) {
            contacts.grounded=false;
            require(world.Update(h,1,&allocator,&jobs)==EPhysicsUpdateError::None,"actual combined Jolt update");
            hands->post_step(h);line->post_step();++ticks;
        }
    }
    void trace(const char *phase) {
        const auto p=world.GetBodyInterface().GetPosition(player);const auto s=line->readback();
        std::cerr<<"TRACE "<<phase<<" tick="<<ticks<<" player="<<p.GetX()<<','<<p.GetY()<<','<<p.GetZ()
            <<" grounded="<<contacts.grounded<<" failure="<<unsigned(s.failure)<<" broken="<<s.broken
            <<" installed="<<s.installed<<" two_ended="<<s.two_ended<<" path="<<s.path_valid<<" path_reason="<<s.path_reason
            <<" tension_n="<<s.tension_n<<" fitting="<<s.grip_point.GetX()<<','<<s.grip_point.GetY()<<','<<s.grip_point.GetZ()
            <<" hands="<<hands->attached_count()<<" hand_n="<<(hands->hand_force(0)+hands->hand_force(1)).Length()
            <<" regrips="<<s.regrip_count<<'\n';
    }
    // Fixture approach driver: a bounded grounded force, never a pose/velocity
    // assignment. The actual contacts, rope, carried fitting and hands solve.
    void walk_to(Vec3 target) {
        auto &bi=world.GetBodyInterface();
        unsigned settled=0;
        for(unsigned i=0;i<10000 && settled<60;++i) {
            aim();line->set_reel_input(-1);
            const auto at=bi.GetPosition(player);const Vec3 error(target.GetX()-at.GetX(),0,target.GetZ()-at.GetZ());
            const Vec3 velocity=bi.GetLinearVelocity(player);const Vec3 horizontal(velocity.GetX(),0,velocity.GetZ());
            const Vec3 desired=error.Length()>.04F ? error.Normalized()*.45F : Vec3::sZero();
            Vec3 force=85*12*(desired-horizontal);if(force.Length()>450)force=force.Normalized()*450;
            if(contacts.grounded)bi.AddForce(player,force);
            step();
            require(!line->readback().broken,"supported fitting carry does not break line");
            settled=error.Length()<.05F && horizontal.Length()<.05F ? settled+1 : 0;
        }
        if(settled<60) { trace("approach_failed");require(false,"bounded ground approach reaches next beam"); }
    }
};
void combined_crossing() {
    Fixture f;auto &bi=f.world.GetBodyInterface();const auto body_count=f.world.GetNumBodies();
    require(f.contacts.grounded,"entry begins on actual deck contact");
    auto shown=f.line->readback();require(shown.target_valid,"first ordinary beam shown");
    require(f.line->install(shown.target),"install first end from shown target");
    require(f.line->grab(),"grab real carried fitting");
    f.walk_to(Vec3(0,0,-1.8F));f.walk_to(Vec3(4,0,-1.8F));f.walk_to(Vec3(4,0,0));
    f.line->set_reel_input(0);f.step(360);f.aim();shown=f.line->readback();f.trace("second_pin_ready");
    require(f.contacts.grounded && shown.target_valid && shown.can_pin_end,"unloaded fitting reaches shown second mount");
    require(f.line->pin_second_end(shown.target),"pin second end through shown target");
    require(f.line->two_ended() && f.line->readback().path_valid && !f.hands->active(),"two-ended installation replaces carried grip cold");
    require(f.world.GetNumBodies()==body_count,"bounded line hardware count");
    f.step(180);f.aim();require(f.line->grab(),"grab reachable actual material");
    require(f.hands->attached_count()==2 && f.hands->grip_readback(0).distributed && f.hands->grip_readback(1).distributed,"both real distributed hand rows");
    unsigned loaded=0;float peak=0;unsigned changes[2]{};
    auto left=f.hands->grip_readback(0),right=f.hands->grip_readback(1);
    const float start_x=bi.GetPosition(f.player).GetX();
    for(unsigned i=0;i<7200;++i) {
        f.aim(-Vec3::sAxisX());f.line->traverse(-Vec3::sAxisX(),h);f.step();const auto state=f.line->readback();
        peak=std::max(peak,state.tension_n);
        if(i==300 || i==600) {
            const auto c=f.hands->commanded_position();
            std::cerr<<"POSTURE_DIAGNOSTIC i="<<i<<" target="<<c.GetX()<<','<<c.GetY()<<','<<c.GetZ()
                <<" last_work="<<f.hands->last_actuator_positive_work_j()<<" last_bound="<<f.hands->last_command_work_bound_j()<<'\n';
        }
        if(!f.contacts.grounded && state.tension_n>100 &&
            f.hands->hand_force(0).Length()+f.hands->hand_force(1).Length()>300) {
            if(loaded==0) { f.trace("first_airborne_load");
                require(!f.line->recover() && f.line->readback().failure==PortableTraversalLine::Failure::Loaded,"loaded line refuses packing"); }
            ++loaded;
        }
        const auto new_left=f.hands->grip_readback(0),new_right=f.hands->grip_readback(1);
        if(new_left.generation!=left.generation)++changes[0];
        if(new_right.generation!=right.generation)++changes[1];
        left=new_left;right=new_right;
        if(state.broken || !state.path_valid || f.hands->attached_count()!=2) {
            f.trace("crossing_invalid");require(false,"loaded crossing retains real admitted material hands");
        }
        if(bi.GetPosition(f.player).GetX()<1.0F && loaded>30 && changes[0]>0 && changes[1]>0)break;
    }
    f.trace("crossing_finish");
    require(loaded>30,"actual airborne load persists across solved intervals");
    require(start_x-bi.GetPosition(f.player).GetX()>2,"movement stick drives real crossing displacement");
    require(changes[0]>0 && changes[1]>0 && f.line->readback().regrip_count>=2,"alternating material regrips");
    const auto saved=f.line->capture();const auto departure=bi.GetLinearVelocity(f.player);
    f.line->release();require((bi.GetLinearVelocity(f.player)-departure).LengthSq()==0,"release preserves exact solved player velocity");
    const auto released=f.line->capture();
    for(unsigned i=0;i<MaterialTraversalLine::node_count;++i)
        require((released.crossing.nodes[i].velocity-saved.crossing.nodes[i].velocity).LengthSq()==0,"release preserves exact line-node velocities");
    require(f.line->readback().installed && f.line->two_ended() && !f.hands->active(),"installation survives release");
    f.line->detach_for_restore();require(f.line->restore(saved),"combined cold checkpoint restores installation");
    const auto restored=f.line->capture();
    require(!f.hands->active() && f.line->two_ended() && f.world.GetNumBodies()==body_count,"cold restore has one stock item and no resurrected grips");
    for(unsigned i=0;i<MaterialTraversalLine::node_count;++i) {
        require((restored.crossing.nodes[i].position-saved.crossing.nodes[i].position).LengthSq()==0,"cold checkpoint preserves material positions");
        require((restored.crossing.nodes[i].velocity-saved.crossing.nodes[i].velocity).LengthSq()==0,"cold checkpoint preserves material velocities");
    }
    std::cout<<"PASS MATERIAL_LINE_CROSSING loaded_intervals="<<loaded<<" regrips="<<changes[0]<<','<<changes[1]
        <<" displacement_m="<<start_x-bi.GetPosition(f.player).GetX()<<" peak_line_n="<<peak
        <<" release_exact=1 installed_persistence=1 cold_restore=1 loaded_pack_rejected=1\n";
}
}
int main() {
    RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();int code=0;
    try { combined_crossing(); }catch(const std::exception &e) { std::cerr<<"FAIL MATERIAL_LINE_CROSSING "<<e.what()<<'\n';code=1; }
    UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return code;
}
