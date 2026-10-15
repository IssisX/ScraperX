#include "sim/grappling_gun.hpp"
#include "sim/grappling_hook_geometry.hpp"
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace scraperx::sim {
namespace {
using namespace JPH;
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
double potential(float radius, float rest) {
    const double extension = std::max(0.0, double(radius) - double(rest));
    return .5 * GrapplingGun::stiffness_n_per_m * extension * extension;
}
class RopeBodyFilter final : public BodyFilter {
public:
    RopeBodyFilter(BodyID player, BodyID hook) : player_(player), hook_(hook) {}
    bool ShouldCollide(const BodyID &id) const override { return id != player_ && id != hook_; }
    bool ShouldCollideLocked(const Body &body) const override { return !body.IsSensor(); }
private:
    BodyID player_, hook_;
};
class HookBodyFilter final : public BodyFilter {
public:
    explicit HookBodyFilter(BodyID hook):hook_(hook) {}
    bool ShouldCollide(const BodyID &id) const override { return id==hook_; }
private:
    BodyID hook_;
};
// A zero-depth initial cylinder/floor cast can report its motion direction as
// an axis. Use the actual receiver face and complete starting bounds to admit
// only exact touching contact that the whole hook is moving OUT of. All other
// hits, including penetration, closing contact and later obstacles, still block.
class WithdrawalCollector final : public ClosestHitCollisionCollector<CastShapeCollector> {
public:
    WithdrawalCollector(RVec3 base,Vec3 direction,AABox bounds)
        : base_(base),direction_(direction),bounds_(bounds) {}
    void AddHit(const ShapeCastResult &hit) override {
        if (hit.mFraction==0 && hit.mPenetrationDepth<=1.0e-6F) {
            const auto *body=GetContext();
            if (body) {
                const auto point=base_+hit.mContactPointOn2;
                const Vec3 normal=body->GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,point);
                const float minimum=Vec3(RVec3(bounds_.GetCenter())-point).Dot(normal)-
                    bounds_.GetExtent().Dot(normal.Abs());
                if (finite(normal) && normal.LengthSq()>.99F && minimum>=-1.0e-5F &&
                    direction_.Dot(normal)>1.0e-5F) return;
            }
        }
        ClosestHitCollisionCollector<CastShapeCollector>::AddHit(hit);
    }
private:
    RVec3 base_; Vec3 direction_; AABox bounds_;
};
// A swept .01m sphere is the straight line's finite collision footprint.
// Exclude only the exact endpoint bodies, never an entire receiver/support.
// The steel crook has a rear eye; the receiver needs a clear cable corridor. There is no wrapping solver or hidden through-wall force.
bool blocked(const PhysicsSystem &system, RVec3 from, RVec3 to, float radius,
             BodyID player, BodyID hook, bool no_lock) {
    const SphereShape sphere(radius);
    const RShapeCast cast(&sphere, Vec3::sReplicate(1), RMat44::sTranslation(from), Vec3(to-from));
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    ClosestHitCollisionCollector<CastShapeCollector> collector;
    const RopeBodyFilter filter(player,hook);
    const auto &query = no_lock ? system.GetNarrowPhaseQueryNoLock() : system.GetNarrowPhaseQuery();
    query.CastShape(cast,settings,from,collector,{},{},filter);
    if (collector.HadHit() && no_lock) {
        const auto &hit = collector.mHit;
        const auto *body = system.GetBodyLockInterfaceNoLock().TryGetBody(hit.mBodyID2);
        if (body) {
            const auto inverse = body->GetWorldTransform().Inversed();
            const Vec3 local_from(inverse * from), local_to(inverse * to);
            const auto position = body->GetPosition();
            const auto rotation = body->GetRotation();
            std::fprintf(stderr, "[DEBUG-grapple-hit] body=%u entity=%llu subshape=%u fraction=%.9f radius=%.6f local_from=%.6f,%.6f,%.6f local_to=%.6f,%.6f,%.6f\n",
                hit.mBodyID2.GetIndexAndSequenceNumber(), static_cast<unsigned long long>(body->GetUserData()),
                hit.mSubShapeID2.GetValue(), hit.mFraction, radius,
                local_from.GetX(), local_from.GetY(), local_from.GetZ(), local_to.GetX(), local_to.GetY(), local_to.GetZ());
            std::fprintf(stderr, "[DEBUG-grapple-hit] pose=%.6f,%.6f,%.6f rotation=%.6f,%.6f,%.6f,%.6f\n",
                position.GetX(), position.GetY(), position.GetZ(), rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
            if (body->GetShape()->GetSubType() == EShapeSubType::StaticCompound) {
                const auto *compound = static_cast<const StaticCompoundShape *>(body->GetShape());
                SubShapeID remainder;
                const auto index = compound->GetSubShapeIndexFromID(hit.mSubShapeID2, remainder);
                const auto &part = compound->GetSubShape(index);
                const auto center = part.GetPositionCOM() + compound->GetCenterOfMass();
                const auto bounds = part.mShape->GetLocalBounds();
                std::fprintf(stderr, "[DEBUG-grapple-hit] part=%u center=%.6f,%.6f,%.6f half=%.6f,%.6f,%.6f\n",
                    index, center.GetX(), center.GetY(), center.GetZ(),
                    bounds.GetExtent().GetX(), bounds.GetExtent().GetY(), bounds.GetExtent().GetZ());
            }
        }
    }
    return collector.HadHit();
}
}

class GrappleRopeSettings final : public JPH::TwoBodyConstraintSettings {
    JPH_DECLARE_SERIALIZABLE_VIRTUAL(, GrappleRopeSettings)
public:
    JPH::Vec3 mLocalEye = JPH::Vec3::sZero();
    float mRestLength = 0;
    JPH::TwoBodyConstraint *Create(JPH::Body &player, JPH::Body &hook) const override;
    void SaveBinaryState(JPH::StreamOut &stream) const override {
        ConstraintSettings::SaveBinaryState(stream);
        stream.Write(mLocalEye); stream.Write(mRestLength);
    }
protected:
    void RestoreBinaryState(JPH::StreamIn &stream) override {
        ConstraintSettings::RestoreBinaryState(stream);
        stream.Read(mLocalEye); stream.Read(mRestLength);
    }
};
JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Winvalid-offsetof")
JPH_GCC_SUPPRESS_WARNING("-Winvalid-offsetof")
JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(GrappleRopeSettings) {
    JPH_ADD_BASE_CLASS(GrappleRopeSettings, JPH::TwoBodyConstraintSettings)
    JPH_ADD_ATTRIBUTE(GrappleRopeSettings, mLocalEye)
    JPH_ADD_ATTRIBUTE(GrappleRopeSettings, mRestLength)
}
JPH_SUPPRESS_WARNING_POP

class GrappleRope final : public JPH::TwoBodyConstraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    GrappleRope(JPH::Body &player, JPH::Body &hook, const GrappleRopeSettings &settings)
        : TwoBodyConstraint(player,hook,settings), local_eye_(settings.mLocalEye), rest_(settings.mRestLength) {}
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User2; }
    JPH::Mat44 GetConstraintToBody1Matrix() const override { return JPH::Mat44::sIdentity(); }
    JPH::Mat44 GetConstraintToBody2Matrix() const override { return JPH::Mat44::sTranslation(local_eye_); }
    void NotifyShapeChanged(const JPH::BodyID &id, JPH::Vec3Arg delta) override {
        if (id == mBody2->GetID()) local_eye_ -= delta;
        ResetWarmStart();
    }
    void set_rest(float rest) { rest_=rest; }
    void set_drive_limited(bool limited) { drive_limited_=limited; }
    float rest() const { return rest_; }
    // Same implicit spring/damper demand used by Setup, including the rotating
    // hook eye's point velocity and reciprocal angular effective mass.
    float prospective_tension(float rest, float dt) const {
        using namespace JPH;
        const RVec3 eye=mBody2->GetCenterOfMassTransform()*local_eye_;
        const Vec3 separation(eye-mBody1->GetCenterOfMassPosition());
        const float radius=separation.Length();
        if (radius<rest || radius<1.0e-6F) return 0;
        const Vec3 axis=separation/radius;
        const Vec3 torque_axis=Vec3(eye-mBody2->GetCenterOfMassPosition()).Cross(axis);
        const auto *motion=mBody2->GetMotionProperties();
        const float inverse_mass=mBody1->GetMotionProperties()->GetInverseMass()+motion->GetInverseMass()+
            torque_axis.Dot(motion->MultiplyWorldSpaceInverseInertiaByVector(mBody2->GetRotation(),torque_axis));
        const float radial_speed=(mBody2->GetPointVelocity(eye)-mBody1->GetLinearVelocity()).Dot(axis);
        const float implicit_damping=GrapplingGun::damping_ns_per_m+dt*GrapplingGun::stiffness_n_per_m;
        const float demand=(GrapplingGun::stiffness_n_per_m*(radius-rest)+implicit_damping*radial_speed)/
            (1+dt*implicit_damping*inverse_mass);
        return std::isfinite(demand) ? std::max(0.0F,demand) : demand;
    }
    void inhibit() { permitted_=false; ResetWarmStart(); }
    bool broken() const { return broken_; }
    float tension() const {
        return solved_ && row_.IsActive() && dt_>0 ? std::max(0.0F,-row_.GetTotalLambda()/dt_) : 0;
    }
    void SetupVelocityConstraint(float dt) override {
        using namespace JPH;
        solved_=false;
        dt_=dt;
        const RVec3 eye=mBody2->GetCenterOfMassTransform()*local_eye_;
        const Vec3 separation(eye-mBody1->GetCenterOfMassPosition());
        const float radius=separation.Length();
        if (!permitted_ || broken_ || !std::isfinite(dt) || dt<=0 ||
            !finite(separation) || !std::isfinite(rest_) || rest_<=0 || radius<rest_ || radius<1.0e-6F) {
            ResetWarmStart(); return;
        }
        axis_=separation/radius;
        const float force_limit=drive_limited_ ? GrapplingGun::reel_force_n : GrapplingGun::strength_n;
        cap_=force_limit*dt;
        if (double(cap_)/dt>force_limit) cap_=std::nextafter(cap_,0.0F);
        const Vec3 arm(eye-mBody2->GetCenterOfMassPosition());
        const float extension=radius-rest_;
        const float demand=prospective_tension(rest_,dt);
        // The cap is numerical protection, not unlimited extension assistance.
        // Either excess elastic stretch or excess constitutive demand parts the
        // line before warmstart/solve; post_step removes the registered row.
        if (!std::isfinite(demand) || extension>GrapplingGun::strength_n/GrapplingGun::stiffness_n_per_m ||
            demand>GrapplingGun::strength_n) {
            broken_=true; ResetWarmStart(); return;
        }
        row_.CalculateConstraintPropertiesWithStiffnessAndDamping(dt,*mBody1,separation,*mBody2,arm,
            axis_,0,extension,GrapplingGun::stiffness_n_per_m,GrapplingGun::damping_ns_per_m);
    }
    void ResetWarmStart() override {
        row_.Deactivate(); axis_=JPH::Vec3::sZero(); dt_=cap_=0; solved_=false;
    }
    void WarmStartVelocityConstraint(float ratio) override {
        if (!row_.IsActive()) return;
        const float lambda=row_.GetTotalLambda();
        if (!std::isfinite(lambda) || lambda>0) {
            const float dt=dt_; ResetWarmStart(); SetupVelocityConstraint(dt); ratio=0;
        }
        if (!std::isfinite(ratio) || ratio<0) ratio=0;
        if (lambda<0) {
            ratio=std::min(ratio,cap_/(-lambda));
            if ((-lambda)*ratio>cap_) ratio=std::nextafter(ratio,0.0F);
        }
        if (row_.IsActive()) row_.WarmStart(*mBody1,*mBody2,axis_,ratio);
    }
    bool SolveVelocityConstraint(float) override {
        if (!row_.IsActive()) return false;
        solved_=true;
        return row_.SolveVelocityConstraint(*mBody1,*mBody2,axis_,-cap_,0);
    }
    bool SolvePositionConstraint(float,float) override { return false; }
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override {
        auto *settings=new GrappleRopeSettings;
        ToConstraintSettings(*settings); settings->mLocalEye=local_eye_; settings->mRestLength=rest_;
        return settings;
    }
    void SaveState(JPH::StateRecorder &stream) const override {
        TwoBodyConstraint::SaveState(stream); row_.SaveState(stream);
        stream.Write(local_eye_); stream.Write(rest_); stream.Write(axis_);
        stream.Write(dt_); stream.Write(cap_); stream.Write(solved_); stream.Write(permitted_); stream.Write(broken_);
        stream.Write(drive_limited_);
    }
    void RestoreState(JPH::StateRecorder &stream) override {
        TwoBodyConstraint::RestoreState(stream); row_.RestoreState(stream);
        stream.Read(local_eye_); stream.Read(rest_); stream.Read(axis_);
        stream.Read(dt_); stream.Read(cap_); stream.Read(solved_); stream.Read(permitted_); stream.Read(broken_);
        stream.Read(drive_limited_);
    }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *) const override {}
#endif
private:
    JPH::Vec3 local_eye_, axis_=JPH::Vec3::sZero();
    JPH::AxisConstraintPart row_;
    float rest_, dt_=0, cap_=0;
    bool permitted_=true, broken_=false, solved_=false, drive_limited_=false;
};
JPH::TwoBodyConstraint *GrappleRopeSettings::Create(JPH::Body &player, JPH::Body &hook) const {
    return new GrappleRope(player,hook,*this);
}

struct GrapplingGun::Impl {
    JPH::PhysicsSystem &system;
    JPH::BodyID player;
    JPH::ObjectLayer layer;
    std::array<JPH::BodyID,capacity> hooks{};
    JPH::Ref<GrappleRope> rope;
    JPH::RefConst<JPH::Shape> shape;
    unsigned cartridges=capacity;
    int selected=-1;
    LineState state=LineState::Ready;
    Failure failure=Failure::None, pending=Failure::None;
    float reel_input=0;
    bool auto_reel=false;
    bool pickup_active=false, inspecting_pickup=false;
    BodyID pickup_body;
    Vec3 pickup_contact=Vec3::sZero(), pickup_hand=Vec3::sZero();
    float pickup_time=0;
    double pickup_work=0;
    void cancel_pickup() { pickup_active=false; pickup_body=BodyID(); pickup_time=0; }
    void pickup_step(float dt) {
        if (!pickup_active || !std::isfinite(dt) || dt<=0) return;
        const auto &access=system.GetBodyLockInterfaceNoLock();
        Body *p=access.TryGetBody(player), *h=access.TryGetBody(pickup_body);
        if (!p || !h) { cancel_pickup(); return; }
        // Leave the final request alive until post_step checks the completed
        // collision update; no extra impulse is admitted after its expiry.
        if (pickup_time<=0) return;
        const float action_dt=std::min(dt,pickup_time);
        const RVec3 contact=h->GetCenterOfMassTransform()*pickup_contact;
        const RVec3 hand=h->GetCenterOfMassTransform()*pickup_hand;
        const RVec3 from=p->GetCenterOfMassPosition();
        const Vec3 travel(from-hand);
        if (travel.LengthSq()>1.10F*1.10F || travel.LengthSq()<1.e-8F ||
            (rope && pickup_body==selected_id() && (rope->tension()>1 || Vec3(eye(*h)-from).Length()>rope->rest()+.005F)) ||
            blocked(system,from,hand,.04F,player,pickup_body,true)) { cancel_pickup(); return; }
        const Vec3 relative=h->GetPointVelocity(contact)-p->GetPointVelocity(contact);
        if (!finite(relative) || relative.LengthSq()>1.0F) { cancel_pickup(); return; }
        const Vec3 n=travel.Normalized();
        const Vec3 rh(contact-h->GetCenterOfMassPosition()), rp(contact-p->GetCenterOfMassPosition());
        const auto *mh=h->GetMotionProperties(), *mp=p->GetMotionProperties();
        const float inverse_mass=mh->GetInverseMass()+mp->GetInverseMass();
        const auto response=[&](Vec3 impulse) {
            return inverse_mass*impulse+
                mh->MultiplyWorldSpaceInverseInertiaByVector(h->GetRotation(),rh.Cross(impulse)).Cross(rh)+
                mp->MultiplyWorldSpaceInverseInertiaByVector(p->GetRotation(),rp.Cross(impulse)).Cross(rp);
        };
        const Vec3 unit_response=response(n);
        const double k=n.Dot(unit_response), u=relative.Dot(n);
        if (!(k>0) || !std::isfinite(k)) { cancel_pickup(); return; }
        // CHOSEN one short hand pull: existing human1500N/750W envelope,
        // at most1m/s actual relative grasp speed and0.2s duration. No pose edit.
        const double budget=750.0*action_dt;
        const double energy_cap=u<0 ? -u/k+std::sqrt(2*budget/k) :
            2*budget/(u+std::sqrt(u*u+2*k*budget));
        const double a=unit_response.LengthSq(), b=relative.Dot(unit_response);
        const double speed_cap=std::max(0.0,(-b+std::sqrt(std::max(0.0,b*b+a*(1-relative.LengthSq()))))/a);
        float j=float(std::max(0.0,std::min({1500.0*action_dt,energy_cap,speed_cap,(1.0-u)/k})));
        Vec3 impulse=n*j;
        const auto charge=[&](Vec3 value) {
            const double magnitude=value.Length(); if (!(magnitude>0)) return 0.0;
            const double effective=value.Dot(response(value))/(magnitude*magnitude);
            const double velocity=relative.Dot(value)/magnitude;
            const double positive=velocity<0 ? std::max(0.0,magnitude+velocity/effective) : magnitude;
            return velocity<0 ? .5*effective*positive*positive :
                velocity*magnitude+.5*effective*magnitude*magnitude;
        };
        while (j>0 && (impulse.Length()>1500.0*action_dt || charge(impulse)>budget ||
                       (relative+response(impulse)).LengthSq()>1.0F)) {
            j=std::nextafter(j,0.0F); impulse=n*j;
        }
        p->AddImpulse(-impulse,contact);
        h->AddImpulse(impulse,contact);
        pickup_work+=charge(impulse);
        pickup_time-=action_dt;
    }
    double launch_work=0, last_launch_work=0, spool_absorbed=0, release_discarded=0, retrieval_absorbed=0, reel_work=0;
    Impl(JPH::PhysicsSystem &s, JPH::BodyID p, JPH::ObjectLayer l) : system(s), player(p), layer(l) {
        JPH::StaticCompoundShapeSettings compound;
        for (const auto &part:grappling_hook_layout().parts) {
            JPH::Array<JPH::Vec3> points;
            for (float z:{part.z_low,part.z_high}) for (const auto &vertex:part.polygon)
                points.push_back(JPH::Vec3(vertex.GetX(),vertex.GetY(),z));
            auto *prism=new JPH::ConvexHullShapeSettings(points,0);
            prism->mDensity=7850.0F; prism->mHullTolerance=1.0e-6F;
            compound.AddShape(JPH::Vec3::sZero(),JPH::Quat::sIdentity(),prism);
        }
        const auto result=compound.Create();
        if (!result.HasError()) shape=result.Get();
    }
    JPH::BodyID selected_id() const { return selected>=0 ? hooks[unsigned(selected)] : JPH::BodyID(); }
    JPH::RVec3 eye(const JPH::Body &hook) const { return hook.GetWorldTransform()*hook_eye_local(); }
    void remove_line() {
        auto_reel=false;
        if (!rope) return;
        const auto &bodies=system.GetBodyLockInterface();
        const JPH::BodyID ids[]{player,selected_id()};
        {
            JPH::BodyLockMultiRead lock(bodies,ids,2);
            const auto *p=lock.GetBody(0), *h=lock.GetBody(1);
            if (p && h) release_discarded+=potential(JPH::Vec3(eye(*h)-p->GetCenterOfMassPosition()).Length(),rope->rest());
        }
        rope->inhibit();
        system.RemoveConstraint(rope);
        rope=nullptr;
        // A released tool needs its independent stem/eye support points for
        // floor contact and physical pickup. Invalidate cached reduced contacts
        // outside Update; retain the established live-line contact policy.
        const auto loose=selected_id();
        if (!loose.IsInvalid()) system.GetBodyInterface().SetUseManifoldReduction(loose,false);
        pending=Failure::None;
    }
};

JPH::Vec3 GrapplingGun::hook_eye_local() noexcept { return grappling_hook_layout().eye; }

GrapplingGun::GrapplingGun(JPH::PhysicsSystem &system, JPH::BodyID player, JPH::ObjectLayer layer)
    : impl_(std::make_unique<Impl>(system,player,layer)) {}
GrapplingGun::~GrapplingGun() { clear_transients(); }
bool GrapplingGun::line_active() const noexcept { return impl_->rope.GetPtr()!=nullptr; }
bool GrapplingGun::fire(JPH::Vec3 aim) {
    using namespace JPH;
    auto &s=*impl_;
    s.last_launch_work=0;
    if (s.pickup_active) { s.failure=Failure::Busy; return false; }
    if (s.rope) { s.failure=Failure::Busy; return false; }
    if (!finite(aim) || !aim.IsNormalized(1.0e-4F)) { s.failure=Failure::InvalidAim; return false; }
    aim=aim.Normalized();
    auto slot=std::find_if(s.hooks.begin(),s.hooks.end(),[](BodyID id){return id.IsInvalid();});
    if (s.cartridges==0 || slot==s.hooks.end()) { s.failure=Failure::NoInventory; return false; }
    RVec3 player_com;
    Vec3 inherited;
    {
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.player);
        if (!lock.Succeeded() || !lock.GetBody().IsDynamic() || !s.shape) {
            s.failure=Failure::BodyUnavailable; return false;
        }
        player_com=lock.GetBody().GetCenterOfMassPosition();
        inherited=lock.GetBody().GetLinearVelocity();
    }
    const RVec3 muzzle_start=GrapplingGun::muzzle_start(player_com);
    const RVec3 muzzle=GrapplingGun::launch_origin(player_com,aim);
    // Conservative enclosing sphere rejects both a blocked barrel path and
    // initial overlap; no shot is charged or spawned on failure.
    if (blocked(s.system,muzzle_start,muzzle,.24F,s.player,BodyID(),false)) {
        s.failure=Failure::MuzzleBlocked; return false;
    }
    // Launch the rigid head in the gun's yaw/pitch frame. Shortest-arc Y->aim
    // alone leaves the finger row tied to world Z when turning around the tower.
    // This sets initial muzzle orientation only; contacts/torques own flight.
    Quat launch_rotation=Quat::sFromTo(Vec3::sAxisY(),aim);
    const float horizontal=std::hypot(aim.GetX(),aim.GetZ());
    if (horizontal>1.e-6F) {
        const auto yaw=Quat::sRotation(Vec3::sAxisY(),std::atan2(-aim.GetZ(),aim.GetX()));
        const auto pitch=Quat::sFromTo(Vec3::sAxisY(),Vec3(horizontal,aim.GetY(),0));
        launch_rotation=yaw*pitch;
    }
    BodyCreationSettings settings(s.shape,muzzle,launch_rotation,EMotionType::Dynamic,s.layer);
    settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass=hook_mass_kg;
    settings.mLinearVelocity=inherited;
    settings.mMotionQuality=EMotionQuality::LinearCast;
    settings.mLinearDamping=0;
    settings.mAngularDamping=0;
    settings.mFriction=.6F;
    settings.mRestitution=0;
    auto &bodies=s.system.GetBodyInterface();
    const BodyID hook=bodies.CreateAndAddBody(settings,EActivation::Activate);
    if (hook.IsInvalid()) { s.failure=Failure::BodyUnavailable; return false; }
    Ref<GrappleRope> rope;
    {
        const BodyID ids[]{s.player,hook};
        BodyLockMultiWrite lock(s.system.GetBodyLockInterface(),ids,2);
        auto *p=lock.GetBody(0), *h=lock.GetBody(1);
        if (p && h) {
            GrappleRopeSettings rs;
            rs.mLocalEye=hook_eye_local()-h->GetShape()->GetCenterOfMass();
            rs.mRestLength=Vec3(s.eye(*h)-p->GetCenterOfMassPosition()).Length();
            rope=new GrappleRope(*p,*h,rs);
            const float inverse_mass=p->GetMotionProperties()->GetInverseMass()+h->GetMotionProperties()->GetInverseMass();
            float magnitude=std::sqrt(2*cartridge_energy_j/(inverse_mass*aim.LengthSq()));
            Vec3 impulse=aim*magnitude;
            const auto paid_energy=[&]() {
                return .5*inverse_mass*(double(impulse.GetX())*impulse.GetX()+
                    double(impulse.GetY())*impulse.GetY()+double(impulse.GetZ())*impulse.GetZ());
            };
            while (paid_energy()>cartridge_energy_j) {
                magnitude=std::nextafter(magnitude,0.0F); impulse=aim*magnitude;
            }
            p->AddImpulse(-impulse);
            h->AddImpulse(impulse);
            s.last_launch_work=paid_energy();
        }
    }
    if (!rope) {
        bodies.RemoveBody(hook); bodies.DestroyBody(hook);
        s.failure=Failure::BodyUnavailable; return false;
    }
    *slot=hook; s.selected=int(slot-s.hooks.begin()); s.rope=rope;
    s.system.AddConstraint(rope);
    bodies.ActivateBody(s.player);
    --s.cartridges; s.launch_work+=s.last_launch_work;
    s.state=LineState::PayingOut; s.failure=s.pending=Failure::None;
    return true;
}
bool GrapplingGun::lock_line() {
    auto &s=*impl_;
    if (!s.rope || s.pending!=Failure::None || s.rope->broken()) { s.failure=Failure::Busy; return false; }
    s.state=LineState::Locked; s.failure=Failure::None;
    return true;
}
bool GrapplingGun::set_auto_reel(bool enabled) {
    auto &s=*impl_;
    if (!enabled) { s.auto_reel=false; return true; }
    if (!lock_line()) return false;
    s.reel_input=0;
    s.auto_reel=true;
    return true;
}
void GrapplingGun::set_reel_input(float input) {
    impl_->reel_input=std::isfinite(input) ? std::clamp(input,-1.0F,1.0F) : 0;
    if (impl_->reel_input!=0) impl_->auto_reel=false;
}
void GrapplingGun::release() {
    auto &s=*impl_;
    s.cancel_pickup();
    s.remove_line();
    s.state=LineState::Released; s.failure=Failure::None;
}
void GrapplingGun::pre_step(float dt) {
    using namespace JPH;
    auto &s=*impl_;
    s.pickup_step(dt);
    if (!s.rope || s.pending!=Failure::None || !std::isfinite(dt) || dt<=0) return;
    s.rope->set_drive_limited(false);
    if (s.auto_reel && s.rope->rest()<=minimum_line_length_m+1.e-6F) s.auto_reel=false;
    const float reel=s.auto_reel ? 1.0F : s.reel_input;
    const auto &access=s.system.GetBodyLockInterfaceNoLock();
    const Body *p=access.TryGetBody(s.player), *h=access.TryGetBody(s.selected_id());
    if (!p || !h) { s.pending=Failure::BodyUnavailable; s.rope->inhibit(); return; }
    const RVec3 from=p->GetCenterOfMassPosition(), to=s.eye(*h);
    const Vec3 separation(to-from);
    const float radius=separation.Length();
    if (!finite(separation) || !std::isfinite(radius)) {
        s.pending=Failure::BodyUnavailable; s.rope->inhibit(); return;
    }
    if (s.rope->broken()) { s.pending=Failure::StrengthExceeded; s.rope->inhibit(); return; }
    // Query the entire eye-to-COM segment. Endpoint-body exclusion is exact;
    // receivers, including dynamic receivers, still obstruct a blocked slot.
    if (blocked(s.system,from,to,.01F,s.player,s.selected_id(),true)) {
        s.pending=Failure::Obstructed; s.rope->inhibit(); return;
    }
    if (s.state==LineState::PayingOut) {
        const float radial_speed=radius>1.0e-6F ?
            (h->GetPointVelocity(to)-p->GetLinearVelocity()).Dot(separation/radius) : 0;
        const float old=s.rope->rest();
        // Free spool also admits the upcoming outward stroke; it never reels
        // in, and any existing spring storage removed by payout is dissipated.
        const float paid=std::min(maximum_line_length_m,std::max(old,radius+std::max(0.0F,radial_speed)*dt));
        s.spool_absorbed+=std::max(0.0,potential(radius,old)-potential(radius,paid));
        s.rope->set_rest(paid);
    } else if (s.state==LineState::Locked && reel!=0) {
        const float old=s.rope->rest();
        const float stroke=reel_speed_m_per_s*std::abs(reel)*dt;
        if (reel<0) {
            const float paid=std::min(maximum_line_length_m,old+stroke);
            s.spool_absorbed+=std::max(0.0,potential(radius,old)-potential(radius,paid));
            s.rope->set_rest(paid);
        } else if (s.rope->tension()<=reel_force_n) {
            const double extension=std::max(0.0,double(radius)-old);
            const double slack=std::max(0.0,double(old)-radius);
            const double free_stroke=std::min(double(stroke),slack);
            const double base_force=std::max({double(s.rope->tension()),stiffness_n_per_m*extension,
                double(s.rope->prospective_tension(old,dt))});
            const double budget=reel_power_w*double(dt)*reel;
            // Solve F*d + .5*k*d^2 <= P*h without cancellation near d=0.
            const double energy_stroke=2*budget/(base_force+
                std::sqrt(base_force*base_force+2*stiffness_n_per_m*budget));
            const double loaded_stroke=std::min({std::max(0.0,double(stroke)-free_stroke),energy_stroke,
                std::max(0.0,double(reel_force_n)/stiffness_n_per_m-extension)});
            float target=std::max(minimum_line_length_m,float(double(old)-free_stroke-loaded_stroke));
            target=std::min(target,old);
            const auto charge=[&](float length) {
                const double actual_loaded=std::max(0.0,double(old)-length-slack);
                const double conservative=base_force*actual_loaded+.5*stiffness_n_per_m*actual_loaded*actual_loaded;
                return std::max(conservative,std::max(0.0,potential(radius,length)-potential(radius,old)));
            };
            // Float rest-length representation must not exceed paid work,
            // speed, or finite actuator force through rounding downward.
            while (target<old && (charge(target)>budget || double(old)-target>stroke ||
                (target<radius && stiffness_n_per_m*(double(radius)-target)>reel_force_n)))
                target=std::nextafter(target,old);
            // Stall on CURRENT prospective damping load, even when the prior
            // solved impulse was zero. No paid target change on a stalled tick.
            const float demand=s.rope->prospective_tension(target,dt);
            if (!std::isfinite(demand) || demand>reel_force_n) target=old;
            // Other island contacts can change the load after this admission
            // check. While actually shortening, the drive is a finite traction
            // limiter: its row slips above1500N instead of adding extra force.
            // k/c and quadratic storage remain unchanged; no clipped damping
            // dissipation or complete energy closure is inferred from this cap.
            s.rope->set_drive_limited(target<old);
            s.reel_work+=charge(target);
            s.rope->set_rest(target);
        }
    }
}
void GrapplingGun::post_step() {
    auto &s=*impl_;
    if (s.pickup_active) {
        // Update has completed; only here may a newly clear extraction remove
        // the physical body. Rechecking never queues another pull implicitly.
        s.inspecting_pickup=true;
        (void)retrieve();
        s.inspecting_pickup=false;
        if (s.pickup_active && s.pickup_time<=0) s.cancel_pickup();
    }
    if (s.rope && s.rope->broken()) s.pending=Failure::StrengthExceeded;
    if (s.pending!=Failure::None) {
        const Failure failure=s.pending;
        s.remove_line(); s.failure=failure; s.state=LineState::Failed;
    }
}
bool GrapplingGun::retrieve() {
    using namespace JPH;
    auto &s=*impl_;
    RVec3 from;
    Vec3 player_velocity;
    {
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.player);
        if (!lock.Succeeded()) { s.failure=Failure::BodyUnavailable; return false; }
        from=lock.GetBody().GetCenterOfMassPosition(); player_velocity=lock.GetBody().GetLinearVelocity();
    }
    int best=-1;
    float nearest=1.10F;
    RVec3 target;
    float selected_eye_distance=0;
    Vec3 relative_velocity;
    RefConst<Shape> retrieved_shape;
    RMat44 retrieved_transform;
    Vec3 contact_local=Vec3::sZero(), hand_local=Vec3::sZero();
    AABox retrieved_bounds;
    for (unsigned i=0;i<capacity;++i) {
        if (s.hooks[i].IsInvalid()) continue;
        // Automatic completion belongs to the grasp requested by Action.
        // A different old hook becoming nearer does not transfer that request.
        if (s.inspecting_pickup && s.hooks[i]!=s.pickup_body) continue;
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.hooks[i]);
        if (!lock.Succeeded()) continue;
        const auto &hook=lock.GetBody();
        // Sweep the whole finite hand to the actual back bridge. First contact
        // supplies an exposed grasp even when a ray-normal offset would bury
        // the hand in another leaf of this concave compound.
        const RVec3 focus=hook.GetWorldTransform()*Vec3(-.091F,.10F,0);
        const Vec3 direction(focus-from);
        const SphereShape hand(.04F);
        const RShapeCast approach(&hand,Vec3::sReplicate(1),RMat44::sTranslation(from),direction);
        ShapeCastSettings settings; settings.mReturnDeepestPoint=true;
        ClosestHitCollisionCollector<CastShapeCollector> hits;
        const HookBodyFilter only_hook(s.hooks[i]);
        s.system.GetNarrowPhaseQueryNoLock().CastShape(approach,settings,from,hits,{},{},only_hook);
        if (!hits.HadHit()) continue;
        const RVec3 contact=from+hits.mHit.mContactPointOn2;
        const RVec3 grasp=from+direction*hits.mHit.mFraction;
        const float distance=Vec3(grasp-from).Length();
        if (distance<=nearest) {
            best=int(i); nearest=distance; target=grasp;
            selected_eye_distance=Vec3(s.eye(hook)-from).Length();
            relative_velocity=hook.GetPointVelocity(contact)-player_velocity;
            retrieved_shape=hook.GetShape(); retrieved_transform=hook.GetCenterOfMassTransform();
            contact_local=Vec3(retrieved_transform.Inversed()*contact);
            hand_local=Vec3(retrieved_transform.Inversed()*grasp);
            retrieved_bounds=hook.GetWorldSpaceBounds();
        }
    }
    if (best<0) { s.failure=Failure::OutOfReach; return false; }
    if (s.rope && best==s.selected && (selected_eye_distance>s.rope->rest()+.005F || s.rope->tension()>1.0F)) {
        s.failure=Failure::LineTaut; return false;
    }
    if (relative_velocity.Length()>1.0F) { s.failure=Failure::RetrievalTooFast; return false; }
    // A normal offset from one leaf can intersect another leaf of a
    // concave hook. Validate the full hand against the complete hook.
    const SphereShape hand(.04F);
    CollideShapeSettings hand_settings;
    AllHitCollisionCollector<CollideShapeCollector> hand_contacts;
    const HookBodyFilter only_hook(s.hooks[unsigned(best)]);
    s.system.GetNarrowPhaseQuery().CollideShape(&hand,Vec3::sReplicate(1),
        RMat44::sTranslation(target),hand_settings,target,hand_contacts,{},{},only_hook);
    const bool penetrates=std::any_of(hand_contacts.mHits.begin(),hand_contacts.mHits.end(),
        [&](const auto &contact_hit) { return contact_hit.mPenetrationDepth>hand_settings.mCollisionTolerance; });
    if (penetrates) { s.failure=Failure::RetrievalBlocked; return false; }
    if (blocked(s.system,from,target,.04F,s.player,s.hooks[unsigned(best)],false)) {
        std::fprintf(stderr,"[DEBUG-grapple-retrieve] reach blocked\n");
        s.failure=Failure::RetrievalBlocked; return false;
    }
    // The complete crook must fit through the actual withdrawal path.
    // Clear hand reach cannot permit pulling steel through a retaining bar.
    WithdrawalCollector collector(from,Vec3(from-target),retrieved_bounds);
    const RShapeCast withdrawal(retrieved_shape,Vec3::sReplicate(1),retrieved_transform,Vec3(from-target));
    ShapeCastSettings withdrawal_settings;
    withdrawal_settings.mReturnDeepestPoint=true;
    const RopeBodyFilter withdrawal_filter(s.player,s.hooks[unsigned(best)]);
    s.system.GetNarrowPhaseQuery().CastShape(withdrawal,withdrawal_settings,from,collector,{},{},withdrawal_filter);
    if (collector.HadHit()) {
        if (!s.inspecting_pickup) {
        const auto &hit=collector.mHit;
        const auto *support=s.system.GetBodyLockInterfaceNoLock().TryGetBody(hit.mBodyID2);
        if (support) {
            const auto point=from+hit.mContactPointOn2;
            const auto normal=support->GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,point);
            const auto sb=support->GetWorldSpaceBounds();
            std::fprintf(stderr,"[DEBUG-grapple-retrieve-geometry] entity=%llu subshape=%u hook_y=%.6f,%.6f support_y=%.6f,%.6f\n",
                static_cast<unsigned long long>(support->GetUserData()),hit.mSubShapeID2.GetValue(),
                retrieved_bounds.mMin.GetY(),retrieved_bounds.mMax.GetY(),sb.mMin.GetY(),sb.mMax.GetY());
            AllHitCollisionCollector<CollideShapeCollector> overlaps;
            CollideShapeSettings overlap_settings;
            s.system.GetNarrowPhaseQuery().CollideShape(retrieved_shape,Vec3::sReplicate(1),
                retrieved_transform,overlap_settings,from,overlaps,{},{},withdrawal_filter);
            unsigned reported=0;
            for (const auto &overlap:overlaps.mHits) if (overlap.mBodyID2==hit.mBodyID2 && reported++<4)
                std::fprintf(stderr,"[DEBUG-grapple-retrieve-overlap] subshape=%u depth=%.9f point2_y=%.6f axis_y=%.6f\n",
                    overlap.mSubShapeID2.GetValue(),overlap.mPenetrationDepth,
                    float(from.GetY()+overlap.mContactPointOn2.GetY()),overlap.mPenetrationAxis.GetY());
            std::fprintf(stderr,"[DEBUG-grapple-retrieve] support_point=%.6f,%.6f,%.6f normal=%.6f,%.6f,%.6f\n",
                float(point.GetX()),float(point.GetY()),float(point.GetZ()),normal.GetX(),normal.GetY(),normal.GetZ());
        }
        std::fprintf(stderr,"[DEBUG-grapple-retrieve] withdrawal body=%u fraction=%.9f depth=%.9f axis=%.6f,%.6f,%.6f direction=%.6f,%.6f,%.6f\n",
            hit.mBodyID2.GetIndexAndSequenceNumber(),hit.mFraction,hit.mPenetrationDepth,
            hit.mPenetrationAxis.GetX(),hit.mPenetrationAxis.GetY(),hit.mPenetrationAxis.GetZ(),
            float(from.GetX()-target.GetX()),float(from.GetY()-target.GetY()),float(from.GetZ()-target.GetZ()));
        }
        if (!s.inspecting_pickup && !s.pickup_active) {
            s.pickup_body=s.hooks[unsigned(best)];
            s.pickup_contact=contact_local; s.pickup_hand=hand_local;
            s.pickup_time=.20F; s.pickup_active=true;
            s.system.GetBodyInterface().ActivateBody(s.player);
            s.system.GetBodyInterface().ActivateBody(s.pickup_body);
        }
        s.failure=Failure::RetrievalBlocked; return false;
    }
    if (s.pickup_body==s.hooks[unsigned(best)]) s.cancel_pickup();
    if (best==s.selected) { s.remove_line(); s.selected=-1; s.state=LineState::Ready; }
    // Short physical retrieval is explicitly an inventory abstraction, not a
    // velocity reset of a remotely flying hook or a refill of finite cartridges.
    s.retrieval_absorbed+=.5*hook_mass_kg*relative_velocity.LengthSq();
    auto &bodies=s.system.GetBodyInterface();
    const BodyID hook=s.hooks[unsigned(best)];
    bodies.RemoveBody(hook); bodies.DestroyBody(hook); s.hooks[unsigned(best)]=BodyID();
    s.failure=Failure::None;
    return true;
}
GrapplingGun::Readback GrapplingGun::readback() const {
    using namespace JPH;
    const auto &s=*impl_;
    Readback result;
    result.state=s.state; result.failure=s.failure; result.cartridges=s.cartridges; result.available_hooks=0;
    result.hook=s.selected_id(); result.launch_work_j=s.launch_work; result.last_launch_work_j=s.last_launch_work;
    result.spool_absorbed_energy_j=s.spool_absorbed; result.release_discarded_elastic_energy_j=s.release_discarded;
    result.retrieval_absorbed_relative_energy_j=s.retrieval_absorbed;
    result.reel_work_j=s.reel_work;
    result.auto_reel=s.auto_reel;
    {
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.player);
        if (lock.Succeeded()) result.player_endpoint=lock.GetBody().GetCenterOfMassPosition();
    }
    for (unsigned i=0;i<capacity;++i) {
        if (s.hooks[i].IsInvalid()) { ++result.available_hooks; continue; }
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.hooks[i]);
        if (!lock.Succeeded()) continue;
        const auto &body=lock.GetBody();
        result.hooks[i]={true,s.hooks[i],body.GetPosition(),body.GetRotation()};
        if (int(i)==s.selected) result.hook_endpoint=s.eye(body);
    }
    if (s.rope) {
        result.line_length_m=s.rope->rest(); result.tension_n=s.rope->tension();
        const float radius=Vec3(result.hook_endpoint-result.player_endpoint).Length();
        result.extension_m=std::max(0.0F,radius-result.line_length_m);
        result.stored_elastic_energy_j=potential(radius,result.line_length_m);
    }
    return result;
}
bool GrapplingGun::pickup_active() const noexcept { return impl_->pickup_active; }
double GrapplingGun::pickup_work_j() const noexcept { return impl_->pickup_work; }
void GrapplingGun::clear_transients() {
    auto &s=*impl_;
    s.cancel_pickup(); s.pickup_work=0;
    s.remove_line();
    auto &bodies=s.system.GetBodyInterface();
    for (auto &hook:s.hooks) if (!hook.IsInvalid()) {
        bodies.RemoveBody(hook); bodies.DestroyBody(hook); hook=JPH::BodyID();
    }
    s.selected=-1; s.cartridges=capacity;
    s.state=LineState::Ready; s.failure=s.pending=Failure::None;
    s.reel_input=0;
    s.launch_work=s.last_launch_work=s.spool_absorbed=s.release_discarded=s.retrieval_absorbed=s.reel_work=0;
}
} // namespace scraperx::sim
