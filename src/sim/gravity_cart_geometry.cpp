#include "sim/gravity_cart_geometry.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;
const Vec3 kAlong(.5F, std::sqrt(3.0F) * .5F, 0);
const Vec3 kNormal(-std::sqrt(3.0F) * .5F, .5F, 0);
const Vec3 kDeck(-.30F, 2.75F, 0);
const RVec3 kOrigin(-41.5, 327.25, -142.0);

// Dynamic kilograms are explicit chosen masses of these actual parts. Only
// timber below derives its chosen 600kg/m3 mass from its remaining solid.
Part part(const Vec3 half, const Vec3 offset, const float mass = 0,
          const Quat rotation = Quat::sIdentity(), const Material material = Material::Steel) {
    Part p;
    p.half = half; p.offset = offset; p.mass_kg = mass;
    p.rotation = rotation; p.material = material; p.convex_radius = .002F;
    return p;
}
Part between(const Vec3 first, const Vec3 last, const float half_section, const float mass = 0) {
    const Vec3 delta = last - first;
    return part(Vec3(delta.Length() * .5F, half_section, half_section),
                (first + last) * .5F, mass,
                Quat::sFromTo(Vec3::sAxisX(), delta.Normalized()));
}
Vec3 local(const float x, const float y, const float z) {
    return Vec3(RVec3(x, y, z) - kOrigin);
}
void bounds(std::vector<Part> &parts, const Vec3 low, const Vec3 high,
            const Material material = Material::Steel) {
    parts.push_back(part((high-low)*.5F, (low+high)*.5F, 0, Quat::sIdentity(), material));
}
void control(std::vector<Part> &parts, const Vec3 head) {
    parts.push_back(part(Vec3(.035F,.55F,.035F),head-Vec3(0,.55F,0),2,
                         Quat::sIdentity(),Material::Yellow));
    parts.push_back(part(Vec3(.16F,.035F,.06F),head,1,
                         Quat::sIdentity(),Material::Hazard));
}
void receiver(std::vector<Part> &parts, const float xmin, const float y,
              const float zmin, const float zmax, const std::array<float,2> support_z,
              const float brace_x, const float brace_y) {
    bounds(parts,local(xmin,y-.18F,zmin),local(-25.6F,y,zmax),Material::Galvanised);
    for(const float z:support_z) {
        bounds(parts,local(xmin,y-.54F,z-.15F),local(-25.4F,y-.18F,z+.15F));
        parts.push_back(between(local(brace_x,y-.36F,z),local(-25.7F,brace_y,z),.15F));
        bounds(parts,local(-26.2F,brace_y-.25F,z-.4F),local(-25.2F,brace_y+.25F,z+.4F));
    }
    for(const float x:{xmin+.25F,-25.9F})
        bounds(parts,local(x-.15F,y-.54F,zmin),local(x+.15F,y-.18F,zmax));
}
float mass_of(const std::vector<Part> &parts) {
    float total = 0;
    for(const auto &p:parts) total += p.mass_kg;
    return total;
}
void timber(std::vector<Part> &parts,const Vec3 half,const Vec3 offset) {
    parts.push_back(part(half,offset,8*half.GetX()*half.GetY()*half.GetZ()*600,
                         Quat::sIdentity(),Material::Timber));
}
std::vector<Part> cart_parts(const Vec3 eye) {
    const Quat incline = Quat::sRotation(Vec3::sAxisZ(),JPH_PI/3);
    std::vector<Part> parts;
    const float width=(1.6F-3*.006F)/4;
    for(int i=0;i<4;++i) {
        const float z=-.8F+width*.5F+i*(width+.006F);
        if(i==0||i==3) timber(parts,Vec3(1.6F,.02F,width*.5F),kDeck+Vec3(0,0,z));
        else {
            // Actual front rope well: X[1.25,1.6], Z[-.06,.06].
            timber(parts,Vec3(1.425F,.02F,width*.5F),kDeck+Vec3(-.175F,0,z));
            const float zlow=i==1?-.3985F:.06F,zhigh=i==1?-.06F:.3985F;
            timber(parts,Vec3(.175F,.02F,(zhigh-zlow)*.5F),
                   kDeck+Vec3(1.425F,0,(zlow+zhigh)*.5F));
        }
    }
    for(const float z:{-.52F,.52F}) {
        parts.push_back(part(Vec3(2.4F,.055F,.055F),kNormal*.28F+Vec3(0,0,z),12,incline));
        const Vec3 low=kAlong*-2.4F+kNormal*.28F+Vec3(0,0,z);
        const Vec3 high=kAlong*2.4F+kNormal*.28F+Vec3(0,0,z);
        parts.push_back(between(low,Vec3(low.GetX(),2.705F,z),.045F,7));
        parts.push_back(between(high,Vec3(high.GetX(),2.705F,z),.045F,2));
        parts.push_back(between(low,Vec3(high.GetX(),2.705F,z),.035F,5.7512F));
        parts.push_back(part(Vec3(1.6F,.045F,.045F),Vec3(kDeck.GetX(),2.705F,z<0?-.70F:.70F),4));
    }
    for(const float s:{-2.4F,2.4F}) {
        const bool rear=s<0;
        // Retain the authored axle's chosen6kg/1.3m linear allowance while
        // extending only the rear end into the outboard retainer corridor.
        const float axle_mass=rear?6.0F*(1.8F/1.3F):6.0F;
        parts.push_back(part(Vec3(.035F,.035F,rear?.90F:.65F),
            kAlong*s+kNormal*.28F+Vec3(0,0,rear?.25F:0),axle_mass,incline));
    }
    control(parts,kDeck+Vec3(-.85F,1.10F,-.45F));
    Part eye_part=part(Vec3(.09F,.035F,.09F),eye,2,
        Quat::sRotation(Vec3::sAxisX(),JPH_PI*.5F));
    eye_part.shape=Part::Shape::Cylinder;
    parts.push_back(eye_part);
    // The carryable block's three real retaining lips leave the central
    // standing lane open. They are chassis mass, not hidden extra cargo.
    parts.push_back(part(Vec3(.025F,.045F,.155F),kDeck+Vec3(-1.45F,.045F,.58F),.4F));
    for(const float z:{.425F,.735F})
        parts.push_back(part(Vec3(.255F,.045F,.025F),kDeck+Vec3(-1.195F,.045F,z),.4F));
    const float low_body_mass=275-mass_of(parts);
    if(!(low_body_mass>0)) throw std::logic_error("gravity cart mass allowance exhausted");
    parts.push_back(part(Vec3(1.7F,.08F,.40F),kNormal*.47F,low_body_mass,incline));
    return parts;
}
} // namespace

GravityCartGeometry build_gravity_cart(PhysicsSystem &world,kit::Kit &kit) {
    // 1950..1953 belong to slingshot/cargo;1960 starts the ladder. Machine
    // checks every actually allocated entity for collision before authoring.
    vertical::BuildContext context{kit,world,{kOrigin,0},{1954,6},{2320,8}};
    GravityCartGeometry result;
    result.machine=std::make_unique<vertical::Machine>(context,"gravity_cart",6,8);
    auto &m=*result.machine;
    const float stroke=static_cast<float>(result.stroke_m);
    const Quat incline=Quat::sRotation(Vec3::sAxisZ(),JPH_PI/3);
    result.lower_deck=kOrigin+kDeck;
    result.upper_deck=result.lower_deck+kAlong*stroke;
    const RVec3 S(-27.198294,352.721281,-142),H(-44.5,352.721281,-142);
    result.tow_local=Vec3(RVec3(-41.8,327.430384,-142)-kOrigin);

    std::vector<Part> track;
    const float rail_half=(stroke+5.8F)*.5F;
    for(const float z:{-.65F,.65F})
        track.push_back(part(Vec3(rail_half,.1F,.045F),kAlong*(stroke*.5F)-kNormal*.1F+Vec3(0,0,z),0,incline));
    // Real rail crossheads and side reaction beams enter visible Tower plates.
    for(const float s:{0.0F,stroke*.5F,stroke}) {
        const Vec3 cross=kAlong*s-kNormal*.35F;
        track.push_back(part(Vec3(.15F,.15F,.85F),cross,0,incline));
        for(const float z:{-.65F,.65F}) {
            const Vec3 first=cross+Vec3(0,0,z);
            const Vec3 last=local(-25.7F,static_cast<float>(kOrigin.GetY())+first.GetY(),-142+z);
            track.push_back(between(first,last,.15F));
            bounds(track,last+Vec3(-.5F,-.25F,-.4F),last+Vec3(.5F,.25F,.4F));
        }
    }
    const auto rails=m.body("track",track,Vec3::sZero(),0);
    (void)rails;

    std::vector<Part> loading;
    receiver(loading,-43.4F,330,-140.8F,-139.2F,{-140.5F,-139.5F},-42.9F,318.9F);
    control(loading,local(-25.9F,331.1F,-139.5F));
    const auto lower=m.body("loading_receiver",loading,Vec3::sZero(),0);
    std::vector<Part> middle;
    receiver(middle,-37.1F,341,-140.8F,-139.2F,{-140.5F,-139.5F},-36.6F,329.64F);
    receiver(middle,-30.5F,341,-142.65F,-140.8F,{-142.5F,-140.95F},-30.25F,329.64F);
    control(middle,local(-25.9F,342.1F,-139.5F));
    const auto intermediate=m.body("intermediate_receiver",middle,Vec3::sZero(),0);
    std::vector<Part> upper;
    bounds(upper,local(-26.3F,351.82F,-140.8F),local(-24.5F,352,-139.2F),Material::Galvanised);
    bounds(upper,local(-26.2F,351.3F,-140.3F),local(-25.2F,351.8F,-138.7F));
    control(upper,local(-25.1F,353.1F,-139.5F));
    const auto upper_receiver=m.body("upper_receiver_control",upper,Vec3::sZero(),0);

    std::vector<Part> shaft;
    const float cage_bottom=325.5F,cage_top=353.3F;
    for(const float x:{-45.3F,-43.7F}) for(const float z:{-142.7F,-141.3F})
        bounds(shaft,local(x-.10F,cage_bottom,z-.10F),local(x+.10F,cage_top,z+.10F));
    for(const float y:{cage_bottom,cage_top}) {
        bounds(shaft,local(-45.4F,y-.10F,-142.8F),local(-43.6F,y+.10F,-142.6F));
        bounds(shaft,local(-45.4F,y-.10F,-141.4F),local(-43.6F,y+.10F,-141.2F));
    }
    // Main redirect reaction beam is behind the complete cart/rider sweep,
    // with short end cantilevers reaching the actual S/H rope redirects.
    shaft.push_back(between(local(-44.5F,353.3F,-143.2F),local(-25.7F,353.3F,-143.2F),.15F));
    shaft.push_back(between(local(-44.5F,353.3F,-143.2F),local(-44.5F,353.3F,-142),.15F));
    shaft.push_back(between(local(-25.7F,353.3F,-143.2F),local(-25.7F,341,-143.2F),.15F));
    bounds(shaft,local(-26.2F,340.75F,-143.6F),local(-25.2F,341.25F,-142.8F));
    const auto shaft_frame=m.body("slab_cage_H_support",shaft,Vec3::sZero(),0);

    const Vec3 pivot=kAlong*(stroke-2.65F)+kNormal*.70F+Vec3(0,0,1.05F);
    result.latch_pivot=kOrigin+pivot;
    std::vector<Part> stop_support;
    stop_support.push_back(part(Vec3(.1F,.32F,.77F),kAlong*(-2.4F-.28F-.1F)+kNormal*.28F,0,incline));
    stop_support.push_back(part(Vec3(.1F,.10F,.77F),kAlong*(stroke+2.768328F)+kNormal*.10F,0,incline));
    stop_support.push_back(part(Vec3(.04F,.08F,.08F),pivot-kAlong*.105F-kNormal*.365F+Vec3(0,0,.21F),0,incline));
    // The rotating blade and its fixed reaction structure remain outside
    // deckZ+.8. A real journal reaches a bored bearing beyond the blade;
    // no solid bearing collider or collision mask blocks its rotation.
    const Vec3 bearing=pivot+Vec3(0,0,.40F);
    const Vec3 tower=local(-25.7F,349.9F,-140.55F);
    const Vec3 brace_axis=(tower-bearing).Normalized();
    stop_support.push_back(between(bearing+brace_axis*.20F,tower,.15F));
    for(const float sign:{-1.0F,1.0F}) {
        stop_support.push_back(part(Vec3(.0425F,.15F,.06F),
            bearing+Vec3(sign*.1075F,0,0)));
        stop_support.push_back(part(Vec3(.065F,.0425F,.06F),
            bearing+Vec3(0,sign*.1075F,0)));
    }
    // This outboard standoff joins the actual low arm-stop bracket to the
    // bearing/brace, behind the blade'sZ+1.23 outer face.
    stop_support.push_back(part(Vec3(.04F,.34F,.08F),
        pivot-kAlong*.105F-kNormal*.34F+Vec3(0,0,.29F),0,incline));
    stop_support.push_back(between(local(-27.198294F,353.3F,-143.2F),local(-27.198294F,353.3F,-142),.15F));
    stop_support.push_back(between(local(-27.198294F,353.3F,-143.2F),local(-25.7F,353.3F,-143.2F),.15F));
    // Visible redirect journals connect each mathematical redirect point to
    // its fixed support, with no extra body or animated sheave state.
    for(const RVec3 redirect:{S,H}) {
        Part journal=part(Vec3(.16F,.09F,.16F),Vec3(redirect-kOrigin),0,
            Quat::sRotation(Vec3::sAxisX(),JPH_PI*.5F),Material::Yellow);
        journal.shape=Part::Shape::Cylinder;
        stop_support.push_back(journal);
        stop_support.push_back(between(Vec3(redirect-kOrigin),Vec3(redirect-kOrigin)+Vec3(0,.58F,0),.06F));
    }
    const auto upper_support=m.body("S_support_upper_bumper_bracket",stop_support,Vec3::sZero(),0);
    (void)shaft_frame;

    const auto parts=cart_parts(result.tow_local);
    result.deck=m.body("deck",parts,Vec3::sZero(),275);
    SixDOFConstraintSettings lateral;
    lateral.mPosition1=lateral.mPosition2=kit.body_center_of_mass_position(result.deck);
    lateral.MakeFixedAxis(SixDOFConstraintSettings::TranslationZ);
    lateral.MakeFixedAxis(SixDOFConstraintSettings::RotationX);
    lateral.MakeFixedAxis(SixDOFConstraintSettings::RotationY);
    lateral.MakeFreeAxis(SixDOFConstraintSettings::TranslationX);
    lateral.MakeFreeAxis(SixDOFConstraintSettings::TranslationY);
    lateral.MakeFreeAxis(SixDOFConstraintSettings::RotationZ);
    lateral.mNumVelocityStepsOverride=40;lateral.mNumPositionStepsOverride=8;
    result.lateral_guide=static_cast<SixDOFConstraint*>(lateral.Create(Body::sFixedToWorld,m.native(result.deck)));
    m.own(result.lateral_guide.GetPtr());
    for(unsigned i=0;i<4;++i) {
        const Vec3 centre=kAlong*(i<2?-2.4F:2.4F)+kNormal*.28F+Vec3(0,0,i%2?.65F:-.65F);
        Part roller=part(Vec3(.28F,.04F,.28F),Vec3::sZero(),35,
            Quat::sRotation(Vec3::sAxisX(),JPH_PI*.5F),Material::Yellow);
        roller.shape=Part::Shape::Cylinder;
        result.rollers[i]=m.body("roller_"+std::to_string(i),{roller},centre,35);
        auto hinge=m.hinge(result.deck,result.rollers[i],centre);
        hinge->SetMotorState(EMotorState::Off);
        m.no_collision(result.deck,result.rollers[i]);
    }
    const Vec3 slab_origin=Vec3(H-kOrigin)-Vec3(0,1,0);
    std::vector<Part> slab_parts{part(Vec3(.55F,.45F,.48F),Vec3::sZero(),566,
        Quat::sIdentity(),Material::Concrete),part(Vec3(.055F,.10F,.055F),Vec3(0,.50F,0),4)};
    result.slab=m.body("gravity_slab",slab_parts,slab_origin,570);
    result.slab_axis=m.slider({},result.slab,Vec3::sAxisY(),-stroke,0);
    result.slab_axis->SetMaxFrictionForce(static_cast<float>(result.brake_force_n));
    result.slab_axis->GetMotorSettings().SetForceLimits(0,static_cast<float>(result.reset_force_n));
    result.slab_axis->SetMotorState(EMotorState::Off);
    result.slab_axis->SetTargetVelocity(0);
    // Fixed S-H span is rendered by Kit but contributes no variable length.
    const float length=Vec3(m.point(result.tow_local)-S).Length()+.5F;
    result.tow=kit.add_rope(result.deck,result.tow_local,S,result.slab,Vec3(0,.50F,0),H,1,length,0);

    // Gravity closes this visible arm against a real rear bracket. The
    // passing cart axle opens it through contact. No proximity FixedConstraint
    // catches the cart; loss of tow/brake before seating remains a real fall.
    Part retainer_journal=part(Vec3(.04F,.33F,.04F),Vec3(0,0,.25F),2,
        Quat::sRotation(Vec3::sAxisX(),JPH_PI*.5F));
    retainer_journal.shape=Part::Shape::Cylinder;
    result.upper_latch=m.body("upper_gravity_retainer",{
        part(Vec3(.055F,.2075F,.18F),-kNormal*.2075F,5.1875F,incline),retainer_journal},pivot,7.1875F);
    result.upper_latch_hinge=m.hinge({},result.upper_latch,pivot,-.35F,1.9F);
    result.upper_latch_hinge->GetMotorSettings().SetTorqueLimits(0,
        static_cast<float>(result.retainer_release_torque_nm));
    result.upper_latch_hinge->SetMotorState(EMotorState::Off);
    result.upper_latch_hinge->SetTargetAngularVelocity(0);
    // Bumper friction is zero: the lower seat cannot carry rail-normal
    // weight through friction. Its normal alone restrains along-track travel.
    world.GetBodyInterface().SetFriction(kit.body_id(upper_support),0);
    result.optional_load=m.body("maintenance_block",{
        part(Vec3(.13F,.13F,.13F),Vec3::sZero(),25,Quat::sIdentity(),Material::Rust)},
        kDeck+Vec3(-1.18F,.15F,.58F),25);
    kit.set_carry(result.optional_load,kit::CarryKind::Load,Vec3(0,.13F,0));
    m.walkable("deck",result.deck,kDeck+Vec3(0,.02F,0));
    m.walkable("lower",lower,local(-41.8F,330,-140));
    m.walkable("upper",upper_receiver,local(-25.1F,352,-139.5F));
    m.walkable("intermediate",intermediate,local(-35.45F,341,-140));
    m.ports.push_back({"deck_control",result.deck,kDeck+Vec3(-.85F,1.10F,-.45F),false});
    m.ports.push_back({"lower_control",lower,local(-25.9F,331.1F,-139.5F),false});
    m.ports.push_back({"intermediate_control",intermediate,local(-25.9F,342.1F,-139.5F),false});
    m.ports.push_back({"upper_control",upper_receiver,local(-25.1F,353.1F,-139.5F),false});
    m.command({0,false});
    return result;
}
} // namespace scraperx::sim
