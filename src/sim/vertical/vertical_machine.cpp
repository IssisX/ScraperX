#include "sim/vertical/vertical_machine.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim::vertical {
Machine::Machine(BuildContext c, std::string key, uint32 ns, uint32 nd) : id(std::move(key)), context(c) {
    if (!std::isfinite(c.placement.yaw) || (!std::isfinite(c.placement.origin.GetX()) || !std::isfinite(c.placement.origin.GetY()) || !std::isfinite(c.placement.origin.GetZ()))) throw std::invalid_argument("nonfinite placement");
    if (c.kit.body_count()+ns+nd>2048 || c.world.GetNumBodies()+ns+nd>c.world.GetMaxBodies()) throw std::invalid_argument("insufficient Kit/world body capacity");
    const auto validate = [&](EntityRange r, uint32 need, uint64 low, uint64 high) {
        if (r.count < need || r.first < low || r.first >= high || r.count > high-r.first)
            throw std::invalid_argument("entity range outside available Kit band");
        for (uint32 i=0; i<need; ++i) if (c.kit.body_for_entity(r.first+i).valid())
            throw std::invalid_argument("entity range collides with existing Kit body");
    };
    validate(c.statics, ns, kit::kStaticEntityBase, kit::kDynamicEntityBase);
    validate(c.dynamics, nd, kit::kDynamicEntityBase, kit::kEntityLimit);
}
Machine::~Machine() { for (auto &constraint: constraints_) context.world.RemoveConstraint(constraint); }
void Machine::command(Command c) {
    if (!std::isfinite(c.travel)) throw std::invalid_argument("nonfinite travel");
    c.travel = std::clamp(c.travel, 0.f, 1.f); command_ = c;
}
void Machine::pre_step(float dt) {
    if (!(dt>0) || !std::isfinite(dt)) throw std::invalid_argument("invalid dt");
    for (auto &d: drives) {
        const auto mode = command_.powered ? EMotorState::Velocity : EMotorState::Off;
        if (d.hinge) {
            d.hinge->SetMotorState(mode);
            float target = d.feedback_body.valid() ? std::clamp(2.f*d.radians_per_metre*(d.extent*command_.travel-Vec3(context.kit.body_position(d.feedback_body)-point(d.feedback_origin)).Dot(direction(d.feedback_axis))),-d.speed,d.speed) : d.translation_feedback ? std::clamp(2.f*d.radians_per_metre*(d.extent*command_.travel-d.translation_feedback->GetCurrentPosition()),-d.speed,d.speed) : d.continuous ? d.speed*command_.travel : std::clamp(2.f*(d.extent*command_.travel-d.hinge->GetCurrentAngle()),-d.speed,d.speed);
            if (command_.powered) target=std::clamp(target,d.target_speed-d.acceleration*dt,d.target_speed+d.acceleration*dt);
            else target=0;
            d.target_speed=target;
            d.hinge->SetTargetAngularVelocity(target);
        } else if (d.slider) {
            d.slider->SetMotorState(mode);
            d.slider->SetTargetVelocity(std::clamp(2.f*(d.extent*command_.travel-d.slider->GetCurrentPosition()),-d.speed,d.speed));
        }
    }
    for (auto &step: stepping) step(dt, command_);
}
ControlState Machine::capture_control() const {
    ControlState state;state.command=command_;
    for(const auto &d:drives) state.target_speeds.push_back(d.target_speed);
    return state;
}
void Machine::restore_control(ControlState state) {
    if(state.target_speeds.size()!=drives.size()) throw std::invalid_argument("checkpoint drive count mismatch");
    for(float v:state.target_speeds) if(!std::isfinite(v)) throw std::invalid_argument("nonfinite checkpoint drive speed");
    command(state.command);
    for(auto &c:constraints_) c->ResetWarmStart();
    for(size_t i=0;i<drives.size();++i) {
        auto &d=drives[i];d.target_speed=state.target_speeds[i];
        if(d.hinge){d.hinge->SetMotorState(command_.powered?EMotorState::Velocity:EMotorState::Off);d.hinge->SetTargetAngularVelocity(d.target_speed);}
        if(d.slider){d.slider->SetMotorState(command_.powered?EMotorState::Velocity:EMotorState::Off);d.slider->SetTargetVelocity(std::clamp(2.f*(d.extent*command_.travel-d.slider->GetCurrentPosition()),-d.speed,d.speed));}
    }
}
bool Machine::owns_traversal_entity(uint64 entity) const {
    for(const auto &p: ports) if(p.walkable && context.kit.body_entity(p.body)==entity) return true;
    return false;
}
RVec3 Machine::port_position(const Port &p) const {
    return context.kit.body_position(p.body)+context.kit.body_rotation(p.body)*p.local_point;
}
RVec3 Machine::point(Vec3 p) const { return context.placement.origin + direction(p); }
Vec3 Machine::direction(Vec3 p) const { return Quat::sRotation(Vec3::sAxisY(),context.placement.yaw)*p; }
BodyIndex Machine::body(std::string name, std::vector<Part> parts, Vec3 p, float mass, float roll) {
    const uint64 entity = mass>0 ? context.dynamics.first+dynamic_used_++ : context.statics.first+static_used_++;
    if ((mass>0 && dynamic_used_>context.dynamics.count) || (mass<=0 && static_used_>context.statics.count)) throw std::logic_error("factory exceeded reserved IDs");
    auto b=context.kit.add_body(entity,parts,point(p),Quat::sRotation(Vec3::sAxisY(),context.placement.yaw)*Quat::sRotation(Vec3::sAxisZ(),roll),mass,.85f);
    if(mass>0) { context.kit.set_damping(b,0.f,0.f); context.kit.set_continuous_collision(b); }
    bodies.push_back({std::move(name),b}); return b;
}
Body &Machine::native(BodyIndex b) {
    if(!b.valid()) return Body::sFixedToWorld;
    auto *body=context.world.GetBodyLockInterfaceNoLock().TryGetBody(context.kit.body_id(b));
    if(!body) throw std::logic_error("invalid module body lifetime");
    return *body;
}
Ref<HingeConstraint> Machine::hinge(BodyIndex a, BodyIndex b, Vec3 p, float lo, float hi, float k, float damping) {
    HingeConstraintSettings s;
    s.mPoint1=s.mPoint2=point(p); s.mHingeAxis1=s.mHingeAxis2=direction(Vec3::sAxisZ());
    s.mNormalAxis1=s.mNormalAxis2=direction(Vec3::sAxisX());
    s.mLimitsMin=lo; s.mLimitsMax=hi;
    if(k>0) s.mLimitsSpringSettings={ESpringMode::StiffnessAndDamping,k,damping};
    s.mNumVelocityStepsOverride=40; s.mNumPositionStepsOverride=8;
    Ref<HingeConstraint> c=static_cast<HingeConstraint*>(s.Create(native(a),native(b)));
    context.world.AddConstraint(c); constraints_.push_back(c.GetPtr()); return c;
}
Ref<SliderConstraint> Machine::slider(BodyIndex a, BodyIndex b, Vec3 axis, float lo, float hi) {
    SliderConstraintSettings s; s.mAutoDetectPoint=true; s.SetSliderAxis(direction(axis));
    s.mLimitsMin=lo; s.mLimitsMax=hi; s.mNumVelocityStepsOverride=40; s.mNumPositionStepsOverride=8;
    Ref<SliderConstraint> c=static_cast<SliderConstraint*>(s.Create(native(a),native(b)));
    context.world.AddConstraint(c); constraints_.push_back(c.GetPtr()); return c;
}
void Machine::walkable(std::string name,BodyIndex b,Vec3 p) { ports.push_back({std::move(name),b,p,true}); }
void Machine::own(Ref<Constraint> c) { context.world.AddConstraint(c); constraints_.push_back(c); }
Ref<HingeConstraint> Machine::axis_hinge(BodyIndex a,BodyIndex b,Vec3 pivot,Vec3 axis,float lo,float hi) {
    HingeConstraintSettings s;
    s.mPoint1=s.mPoint2=point(pivot);
    s.mHingeAxis1=s.mHingeAxis2=direction(axis.Normalized());
    s.mNormalAxis1=s.mNormalAxis2=s.mHingeAxis1.GetNormalizedPerpendicular();
    s.mLimitsMin=lo;s.mLimitsMax=hi;
    s.mNumVelocityStepsOverride=40;s.mNumPositionStepsOverride=8;
    Ref<HingeConstraint> c=static_cast<HingeConstraint*>(s.Create(native(a),native(b)));
    own(c.GetPtr());return c;
}
void Machine::no_collision(BodyIndex a,BodyIndex b) { context.kit.disable_collision(a,b); }
Part box(Vec3 half,Material material,Vec3 offset) { Part p;p.half=half;p.material=material;p.offset=offset;return p; }
Part wheel(float r,float depth,Vec3 offset) { Part p=box(Vec3(r,depth,r),Material::Yellow,offset);p.shape=Part::Shape::Cylinder;p.rotation=Quat::sRotation(Vec3::sAxisX(),JPH_PI/2);return p; }
std::unique_ptr<Machine> create(const std::string &id,BuildContext c) {
    if(id=="sx.gravity_balance.v1") return gravity_balance(c);
    if(id=="sx.traction_tram.v1") return traction_tram(c);
    if(id=="sx.rack_climber.v1") return rack_climber(c);
    if(id=="sx.crown_gondola.v1") return crown_gondola(c);
    if(id=="sx.barrel_helix.v1") return barrel_helix(c);
    if(id=="sx.cascade_mast.v1") return cascade_mast(c);
    if(id=="sx.luffing_derrick.v1") return luffing_derrick(c);
    if(id=="sx.pitman_lift.v1") return pitman_lift(c);
    if(id=="sx.slab_incline.v1") return slab_incline(c);
    if(id=="sx.stone_wheel.v1") return stone_wheel(c);
    throw std::invalid_argument("unknown vertical module: "+id);
}
}
