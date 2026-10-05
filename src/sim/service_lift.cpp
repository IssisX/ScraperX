#include "sim/service_lift.hpp"
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Part;
using kit::Material;
using kit::BodyIndex;
// Chosen material-model densities. Each visible primitive carries its own
// mass; Kit derives compound COM/inertia from precisely these same solids.
constexpr float kSteelDensity = 7850;
constexpr float kTimberDensity = 550;
constexpr float kArmLength = 18;
constexpr float kStartAngle = 15 * JPH_PI / 180;
constexpr float kEndAngle = 65 * JPH_PI / 180;
constexpr float kCarriageSpeed = .18F;
constexpr float kCarriageAcceleration = .11F;
constexpr double kEfficiency = .8;
constexpr double kElectronicsWatts = 250;
const RVec3 kOrigin(-31,108.732514,-153);
const Quat kYaw = Quat::sRotation(Vec3::sAxisY(),JPH_PI/2);
Part solid(Vec3 half, Vec3 p = Vec3::sZero(), Material material = Material::Steel) {
    Part result;result.half=half;result.offset=p;result.material=material;
    result.convex_radius=std::min(.01F,.25F*std::min({half.GetX(),half.GetY(),half.GetZ()}));
    result.mass_kg=8*half.GetX()*half.GetY()*half.GetZ()*(material==Material::Timber?kTimberDensity:kSteelDensity);
    return result;
}
Part shaft(float radius,float half_length,Vec3 p) {
    auto result=solid(Vec3(radius,half_length,radius),p,Material::Galvanised);
    result.shape=Part::Shape::Cylinder;result.rotation=Quat::sRotation(Vec3::sAxisX(),JPH_PI/2);
    result.mass_kg=JPH_PI*radius*radius*2*half_length*kSteelDensity;
    return result;
}
void ibeam(std::vector<Part>&p,float half_length,Vec3 center,float half_height=.18F,float half_width=.07F) {
    // Separate web/flanges have no overlapping volume; open sections remain
    // visible and collidable. No solid-box inertia with a fictional low mass.
    p.push_back(solid(Vec3(half_length,half_height-.008F,.003F),center));
    for(float sign:{-1.F,1.F})p.push_back(solid(Vec3(half_length,.004F,half_width),center+Vec3(0,sign*(half_height-.004F),0)));
}
void crossbeam(std::vector<Part>&p,float half_length,Vec3 center,float half_height=.10F,float half_width=.04F){
    std::vector<Part> sections;ibeam(sections,half_length,Vec3::sZero(),half_height,half_width);
    const auto rotation=Quat::sRotation(Vec3::sAxisY(),JPH_PI/2);
    for(auto part:sections){part.offset=center+rotation*part.offset;part.rotation=rotation; p.push_back(part);}
}
std::vector<Part> arms(float spacing,bool center_pin) {
    std::vector<Part> p;
    for(float sign:{-1.F,1.F})ibeam(p,9,Vec3(0,0,sign*spacing));
    // Transverse ties make each paired frame one connected assembly.
    for(float x:{-6.F,-3.F,3.F,6.F})crossbeam(p,spacing,Vec3(x,0,0),.06F,.04F);
    if(center_pin)p.push_back(shaft(.065F,1.24F,Vec3::sZero()));
    return p;
}
std::vector<Part> carriage() {
    std::vector<Part> p;
    for(float z:{-.65F,.65F})p.push_back(solid(Vec3(.5F,.06F,.08F),Vec3(0,-.12F,z)));
    for(float x:{-.42F,.42F})p.push_back(solid(Vec3(.08F,.13F,.57F),Vec3(x,-.05F,0)));
    p.push_back(shaft(.065F,1.78F,Vec3::sZero()));
    for(float z:{-1.6F,1.6F})p.push_back(solid(Vec3(.18F,.17F,.06F),Vec3(0,.17F,z)));
    return p;
}
std::vector<Part> carrier(bool deck) {
    std::vector<Part> p;
    const float z=deck?1.55F:1.6F;
    for(float sign:{-1.F,1.F})ibeam(p,9.2F,Vec3(0,-.12F,sign*z),.18F,.09F);
    if(deck){
        for(float x:{-9.F,-6.F,-3.F,0.F,3.F,6.F,9.F})crossbeam(p,z,Vec3(x,-.1F,0));
    }else{
        // Both scissor stages sweep through the open center of this frame.
        // Cross ties belong only at the left pivot and beyond maximum span.
        for(float x:{-9.F,9.2F})crossbeam(p,z,Vec3(x,-.1F,0));
    }
    // Pivot cheeks connect the fixed left pin to the carrier girders. The
    // right pin travels in the visible longitudinal pair of channels.
    for(float sign:{-1.F,1.F})p.push_back(solid(Vec3(.18F,.28F,.06F),Vec3(-9,-.39F,sign*.65F)));
    p.push_back(shaft(.065F,1.24F,Vec3(-9,-.65F,0)));
    // Three-sided guide shoes leave the back open for mast brackets, with20mm
    // running clearance. Their transverse yoke is welded to both girders.
    crossbeam(p,2.35F,Vec3(-9.2F,-.12F,0),.08F,.20F);
    for(float z:{-2.15F,2.15F}){
        p.push_back(solid(Vec3(.10F,.08F,.20F),Vec3(-9.5F,-.12F,z)));
        for(float dx:{.18F})p.push_back(solid(Vec3(.02F,.15F,.20F),Vec3(-9.8F+dx,-.12F,z)));
        for(float dz:{-.18F,.18F})p.push_back(solid(Vec3(.16F,.15F,.02F),Vec3(-9.8F,-.12F,z+dz)));
    }
    if(deck){
        for(float x:{-9.F,-6.F,-3.F,0.F,3.F,6.F,9.F})p.push_back(solid(Vec3(.04F,.102F,1.8F),Vec3(x,.162F,0),Material::Timber));
        p.push_back(solid(Vec3(9.2F,.018F,1.8F),Vec3(0,.282F,0),Material::Timber));
        // Open tower-side boarding lane; low edge rails elsewhere.
        p.push_back(solid(Vec3(9.2F,.025F,.025F),Vec3(0,1.15F,-1.76F),Material::Yellow));
        for(float x:{-9.F,-6.F,-3.F,0.F,3.F,6.F,9.F})p.push_back(solid(Vec3(.025F,.43F,.025F),Vec3(x,.72F,-1.76F),Material::Yellow));
        p.push_back(solid(Vec3(.06F,.48F,.06F),Vec3(.75F,.78F,1.3F),Material::Yellow));
        p.push_back(solid(Vec3(.15F,.12F,.10F),Vec3(.75F,1.24F,1.3F),Material::Hazard));
    }
    return p;
}
}

struct ServiceLift::Impl {
    PhysicsSystem& world;kit::Kit& kit;
    std::vector<Ref<Constraint>> joints;
    Ref<SliderConstraint> drive;
    BodyIndex foot,mid,deck,top,center;
    State state;Design design;
    float effort=0,step_dt=0;
    double last_coordinate=0;
    bool pending=false,energized=false;
    Impl(PhysicsSystem&w,kit::Kit&k):world(w),kit(k){}
    ~Impl(){for(auto &joint:joints)world.RemoveConstraint(joint);}
    Body& native(BodyIndex b){return *world.GetBodyLockInterfaceNoLock().TryGetBody(kit.body_id(b));}
    RVec3 point(Vec3 p)const{return kOrigin+kYaw*p;}
    BodyIndex body(std::uint64_t id,std::vector<Part> p,Vec3 pos,bool dynamic,float roll=0){
        if(kit.body_for_entity(id).valid())throw std::logic_error("AS-027 entity collision");
        float mass=0;for(const auto&part:p)mass+=part.mass_kg;
        auto b=kit.add_body(id,p,point(pos),kYaw*Quat::sRotation(Vec3::sAxisZ(),roll),dynamic?mass:0,.85F);
        if(dynamic){kit.set_damping(b,0,0);kit.set_continuous_collision(b);design.moving_mass_kg+=mass;}
        return b;
    }
    Ref<SliderConstraint> slider(BodyIndex a,BodyIndex b,Vec3 axis,float low,float high){
        SliderConstraintSettings s;s.mAutoDetectPoint=true;s.SetSliderAxis(kYaw*axis);
        s.mLimitsMin=low;s.mLimitsMax=high;s.mNumVelocityStepsOverride=40;s.mNumPositionStepsOverride=8;
        Ref<SliderConstraint> c=static_cast<SliderConstraint*>(s.Create(native(a),native(b)));
        world.AddConstraint(c);joints.push_back(c.GetPtr());return c;
    }
    void hinge(BodyIndex a,BodyIndex b,Vec3 p){
        HingeConstraintSettings s;s.mPoint1=s.mPoint2=point(p);s.mHingeAxis1=s.mHingeAxis2=kYaw*Vec3::sAxisZ();
        s.mNormalAxis1=s.mNormalAxis2=kYaw*Vec3::sAxisX();s.mNumVelocityStepsOverride=40;s.mNumPositionStepsOverride=8;
        Ref<Constraint> c=s.Create(native(a),native(b));world.AddConstraint(c);joints.push_back(c);kit.disable_collision(a,b);
    }
    double mass(BodyIndex b){return 1/native(b).GetMotionProperties()->GetInverseMass();}
    void account(){
        if(!pending)return;
        const double force=drive->GetTotalLambdaMotor()/step_dt;
        const double work=force*(drive->GetCurrentPosition()-last_coordinate);
        state.actuator_force_n=force;state.electrical_power_w=0;
        if(energized){
            const double positive=std::max(0.,work);
            const double debit=positive/kEfficiency+kElectronicsWatts*step_dt;
            state.positive_work_j+=positive;
            state.heat_j+=std::max(0.,-work)+debit-positive;
            state.energy_overdraft_j=std::max(state.energy_overdraft_j,debit-state.energy_j);
            state.energy_j=std::max(0.,state.energy_j-debit);
            state.electrical_power_w=debit/step_dt;
            state.peak_electrical_w=std::max(state.peak_electrical_w,state.electrical_power_w);
            if(state.energy_j<=0)state.energy_cutoff=true;
        }else state.heat_j+=std::max(0.,-work);
        pending=false;
    }
};

ServiceLift::ServiceLift(PhysicsSystem&world,kit::Kit&kit):impl_(std::make_unique<Impl>(world,kit)){
    auto&m=*impl_;
    if (kit.body_count() + 12 > 2048 || world.GetNumBodies() + 12 > world.GetMaxBodies())
        throw std::logic_error("AS-027 body capacity exceeded");
    for (std::uint64_t id = kFrameEntity; id <= kLandingEntity; ++id)
        if (kit.body_for_entity(id).valid()) throw std::logic_error("AS-027 static entity collision");
    for (std::uint64_t id = 2970; id <= 2978; ++id)
        if (kit.body_for_entity(id).valid()) throw std::logic_error("AS-027 dynamic entity collision");
    const float w=kArmLength*std::cos(kStartAngle),h=kArmLength*std::sin(kStartAngle);
    const float stroke=w-kArmLength*std::cos(kEndAngle);m.design.stroke_m=stroke;
    // Static welded cantilever ties visibly into the existing110m west ring.
    std::vector<Part> base_parts;
    for(float z:{-1.6F,1.6F})ibeam(base_parts,9.5F,Vec3(9,1.0F,z),.35F,.18F);
    for(float x:{0.F,4.5F,9.F,13.5F,18.F})base_parts.push_back(solid(Vec3(.16F,.30F,4.3F),Vec3(x,.85F,2.1F)));
    for(float z:{-.65F,.65F})base_parts.push_back(solid(Vec3(.24F,.475F,.12F),Vec3(0,1.625F,z)));
    base_parts.push_back(shaft(.065F,1.24F,Vec3(0,2,0)));
    // Exposed stator/guide beds carry the translating linear-motor carriage.
    for(float z:{-.8F,.8F})base_parts.push_back(solid(Vec3(9.2F,.07F,.08F),Vec3(9,1.66F,z)));
    const auto base=m.body(kFrameEntity,std::move(base_parts),Vec3::sZero(),false);
    std::vector<Part> guides;
    // Short, flush rail sections preserve the same37m envelope. A single
    //37m by280mm convex produced a spurious deep shoe contact at a clear
    // pose in the ordinary campaign; bounded sections condition that pair
    // without changing guide clearance, force ratings or collision masks.
    for(float z:{-2.15F,2.15F})for(int section=0;section<10;++section)
        guides.push_back(solid(Vec3(.14F,1.85F,.14F),Vec3(-.8F,3.35F+3.7F*section,z)));
    // Tie the guide mast to the tower at each existing ring; outside travel.
    for(float y:{1.1F,12.1F,23.1F,34.1F}){
        guides.push_back(solid(Vec3(.14F,.14F,4.4F),Vec3(-1.6F,y,2.1F)));
        for(float z:{-2.15F,2.15F})guides.push_back(solid(Vec3(.26F,.14F,.12F),Vec3(-1.2F,y,z)));
    }
    const auto guide=m.body(kGuideEntity,std::move(guides),Vec3::sZero(),false);
    m.foot=m.body(2970,carriage(),Vec3(w,2,0),true);
    m.mid=m.body(2971,carrier(false),Vec3(9,2+h+.65F,0),true);
    m.center=m.body(2972,carriage(),Vec3(w,2+h,0),true);
    m.deck=m.body(kDeckEntity,carrier(true),Vec3(9,2+2*h+.65F,0),true);
    m.top=m.body(2974,carriage(),Vec3(w,2+2*h,0),true);
    m.drive=m.slider(base,m.foot,Vec3::sAxisX(),-stroke,.01F);
    m.slider(guide,m.mid,Vec3::sAxisY(),-.02F,12);
    m.slider(guide,m.deck,Vec3::sAxisY(),-.02F,24);
    m.slider(m.mid,m.center,Vec3::sAxisX(),-stroke,.01F);
    m.slider(m.deck,m.top,Vec3::sAxisX(),-stroke,.01F);
    double arm_effective=0;
    for(int stage=0;stage<2;++stage){
        const float y=2+stage*h;
        auto up=m.body(2975+stage*2,arms(.65F,true),Vec3(w/2,y+h/2,0),true,kStartAngle);
        auto down=m.body(2976+stage*2,arms(1.1F,false),Vec3(w/2,y+h/2,0),true,-kStartAngle);
        arm_effective+=(m.mass(up)+m.mass(down))*(stage==0?.25:.75);
        m.hinge(stage?m.mid:base,up,Vec3(0,y,0));m.hinge(stage?m.center:m.foot,down,Vec3(w,y,0));
        m.hinge(up,down,Vec3(w/2,y+h/2,0));m.hinge(stage?m.deck:m.mid,down,Vec3(0,y+h,0));m.hinge(stage?m.top:m.center,up,Vec3(w,y+h,0));
    }
    std::vector<Part> landings;
    for(float surface:{12.267486F,34.267486F}){
        landings.push_back(solid(Vec3(.7F,.1F,1.625F),Vec3(9,surface-.1F,3.625F),Material::Galvanised));
        landings.push_back(solid(Vec3(.06F,.5F,.06F),Vec3(8.45F,surface+.5F,4),Material::Yellow));
        landings.push_back(solid(Vec3(.15F,.12F,.10F),Vec3(8.45F,surface+1.0F,4),Material::Hazard));
    }
    m.body(kLandingEntity,std::move(landings),Vec3::sZero(),false);
    m.design.effective_vertical_mass_kg=arm_effective+.5*m.mass(m.mid)+m.mass(m.deck)+.5*m.mass(m.center)+m.mass(m.top)+85;
    // Ratings are derived once from the actual member masses,85kg rider and
    // worst-angle load, with declared headroom; never adapted to obstruction.
    const double initial_force=m.design.effective_vertical_mass_kg*9.81*2/std::tan(kStartAngle);
    m.design.rated_force_n=50000*std::ceil(initial_force*1.2/50000);
    m.design.brake_force_n=50000*std::ceil(initial_force*1.4/50000);
    m.design.electrical_rating_w=10000*std::ceil((m.design.rated_force_n*kCarriageSpeed/kEfficiency+kElectronicsWatts)/10000);
    const double gravity_work=m.design.effective_vertical_mass_kg*9.81*2*kArmLength*(std::sin(kEndAngle)-std::sin(kStartAngle));
    m.design.capacity_j=250000*std::ceil((gravity_work/kEfficiency+kElectronicsWatts*stroke/kCarriageSpeed)*1.3/250000);
    m.state.energy_j=m.design.capacity_j;m.last_coordinate=m.drive->GetCurrentPosition();
}
ServiceLift::~ServiceLift()=default;
void ServiceLift::pre_step(float effort){auto&m=*impl_;m.effort=std::isfinite(effort)?std::clamp(effort,-1.F,1.F):0;if(m.effort!=0&&!m.state.energy_cutoff)m.world.GetBodyInterface().ActivateConstraint(m.drive);}
void ServiceLift::collision_step(float dt){
    auto&m=*impl_;m.account();m.energized=m.effort!=0&&!m.state.energy_cutoff&&m.state.energy_j>0;
    const float wanted=m.energized?-m.effort*kCarriageSpeed:0;
    m.state.target_speed_mps=m.energized?std::clamp(wanted,m.state.target_speed_mps-kCarriageAcceleration*dt,m.state.target_speed_mps+kCarriageAcceleration*dt):0;
    const double velocity=std::abs(m.native(m.foot).GetLinearVelocity().Dot(kYaw*Vec3::sAxisX()));
    const double cap=std::min(m.design.rated_force_n,(m.design.electrical_rating_w-kElectronicsWatts)*kEfficiency/std::max({velocity,double(std::abs(m.state.target_speed_mps)),.01}));
    const double reserve=(cap*(std::max(velocity,double(std::abs(m.state.target_speed_mps)))+.02)/kEfficiency+kElectronicsWatts)*dt;
    if(m.energized&&m.state.energy_j<reserve){m.state.energy_cutoff=true;m.energized=false;m.state.target_speed_mps=0;}
    m.drive->GetMotorSettings().SetForceLimit(cap);m.drive->SetMaxFrictionForce(m.energized?0:m.design.brake_force_n);
    m.drive->SetMotorState(m.energized?EMotorState::Velocity:EMotorState::Off);m.drive->SetTargetVelocity(m.state.target_speed_mps);
    m.state.braking=!m.energized;m.last_coordinate=m.drive->GetCurrentPosition();m.step_dt=dt;m.pending=true;
}
void ServiceLift::post_step(){impl_->account();}
ServiceLift::State ServiceLift::state()const{return impl_->state;}
void ServiceLift::restore(const State&s){auto&m=*impl_;m.state=s;m.state.energy_j=std::clamp(s.energy_j,0.,m.design.capacity_j);m.effort=0;m.pending=false;m.energized=false;m.state.braking=true;m.state.target_speed_mps=0;m.drive->SetMotorState(EMotorState::Off);m.drive->SetMaxFrictionForce(m.design.brake_force_n);for(auto&joint:m.joints)joint->ResetWarmStart();m.last_coordinate=m.drive->GetCurrentPosition();}
ServiceLift::Design ServiceLift::design()const{return impl_->design;}
JPH::RVec3 ServiceLift::station_position(Station station)const{
    auto&m=*impl_;
    if(station==Station::Deck)return m.kit.body_position(m.deck)+m.kit.body_rotation(m.deck)*Vec3(.75F,1.24F,1.3F);
    return m.point(Vec3(8.45F,station==Station::Upper?35.267486F:13.267486F,4));
}
double ServiceLift::walking_surface_y()const{return impl_->kit.body_position(impl_->deck).GetY()+.3;}
kit::BodyIndex ServiceLift::deck()const{return impl_->deck;}
bool ServiceLift::owns_support(std::uint64_t entity)noexcept{return (entity>=kFrameEntity&&entity<=kLandingEntity)||(entity>=2970&&entity<=2978);}
} // namespace scraperx::sim
