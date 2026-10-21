#include "sim/grappling_gun.hpp"
#include "sim/finite_line_reel.hpp"
#include "sim/grappling_hook_geometry.hpp"
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
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace scraperx::sim {
static_assert(GrapplingGun::reel_power_w==FiniteLineReel::reel_power_w);
static_assert(GrapplingGun::reel_speed_m_per_s==FiniteLineReel::reel_speed_m_per_s);
namespace {
using namespace JPH;
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
// Shared launch law for the real impulse and read-only ballistic preview.
Vec3 launch_impulse(Vec3 aim, float energy_j, float inverse_mass) {
    float magnitude=std::sqrt(2*energy_j/(inverse_mass*aim.LengthSq()));
    Vec3 impulse=aim*magnitude;
    const auto paid=[&]() {
        return .5*inverse_mass*(double(impulse.GetX())*impulse.GetX()+
            double(impulse.GetY())*impulse.GetY()+double(impulse.GetZ())*impulse.GetZ());
    };
    while (paid()>energy_j) {
        magnitude=std::nextafter(magnitude,0.0F); impulse=aim*magnitude;
    }
    return impulse;
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

struct GrapplingGun::Impl {
    JPH::PhysicsSystem &system;
    JPH::BodyID player;
    JPH::ObjectLayer layer;
    std::array<JPH::BodyID,capacity> hooks{};
    JPH::Ref<GrappleRope> rope;
    JPH::RefConst<JPH::Shape> shape;
    unsigned cartridges=capacity;
    ShotProfile shot_profile=ShotProfile::Long;
    bool charging=false;
    float charge_seconds=0;
    double preview_clock=0;
    mutable double preview_at=-1;
    mutable AimPreview preview_cache{};
    float charge_fraction() const { return std::clamp(charge_seconds/full_charge_seconds,0.0F,1.0F); }
    float charged_energy() const { return precision_energy_j+(cartridge_energy_j-precision_energy_j)*charge_fraction(); }
    float shot_energy() const { return shot_profile==ShotProfile::Near ? precision_energy_j : cartridge_energy_j; }
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
    struct RetrievalGrasp {
        int index=-1;
        RVec3 from=RVec3::sZero(), target=RVec3::sZero();
        Vec3 relative_velocity=Vec3::sZero();
        RefConst<Shape> shape;
        RMat44 transform;
        Vec3 contact_local=Vec3::sZero(), hand_local=Vec3::sZero();
        AABox bounds;
    };
    // Shared read-only hand approach. Queries select intent, never support or
    // inventory; retrieve still checks the complete hand and withdrawal path.
    Failure find_retrieval_grasp(RetrievalGrasp &result) const {
        const auto &s=*this;
    RVec3 from;
    Vec3 player_velocity;
    {
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.player);
        if (!lock.Succeeded()) return Failure::BodyUnavailable;
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
        const auto bounds=hook.GetWorldSpaceBounds();
        const Vec3 outside=Vec3::sMax(bounds.mMin-Vec3(from),
            Vec3::sMax(Vec3(from)-bounds.mMax,Vec3::sZero()));
        if (outside.LengthSq()>1.14F*1.14F) continue;
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
    if (best<0) return Failure::OutOfReach;
    if (s.rope && best==s.selected && (selected_eye_distance>s.rope->rest()+.005F || s.rope->tension()>1.0F)) {
        return Failure::LineTaut;
    }
    if (relative_velocity.Length()>1.0F) return Failure::RetrievalTooFast;
        result={best,from,target,relative_velocity,retrieved_shape,retrieved_transform,
            contact_local,hand_local,retrieved_bounds};
        return Failure::None;
    }
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
bool GrapplingGun::begin_charge() {
    auto &s=*impl_;
    if (s.charging) return true;
    if (s.rope || s.pickup_active) { s.failure=Failure::Busy; return false; }
    if (!s.cartridges || std::none_of(s.hooks.begin(),s.hooks.end(),[](JPH::BodyID id){return id.IsInvalid();})) {
        s.failure=Failure::NoInventory; return false;
    }
    s.charging=true; s.charge_seconds=0; s.preview_at=-1; s.failure=Failure::None;
    return true;
}
void GrapplingGun::cancel_charge() noexcept {
    auto &s=*impl_; s.charging=false; s.charge_seconds=0; s.preview_at=-1;
}
bool GrapplingGun::fire_charged(JPH::Vec3 aim) {
    auto &s=*impl_;
    if (!s.charging) return false;
    const float energy=s.charged_energy();
    cancel_charge();
    return fire_with_energy(aim,energy);
}
GrapplingGun::AimPreview GrapplingGun::aim_preview(JPH::Vec3 aim) const {
    using namespace JPH;
    const auto &s=*impl_;
    AimPreview out;
    if (!s.charging || !finite(aim) || !aim.IsNormalized(1.0e-4F)) return out;
    out.charge_fraction=s.charge_fraction();
    // At most30 collision-query batches/sec, only while actively aiming.
    // The bounded guide is prospective geometry, never a grip/capture claim.
    if (s.preview_at>=0 && s.preview_clock-s.preview_at<1.0/30.0) {
        out=s.preview_cache; out.charge_fraction=s.charge_fraction(); return out;
    }
    RVec3 player;
    Vec3 inherited;
    float player_inverse_mass=0;
    {
        BodyLockRead lock(s.system.GetBodyLockInterface(),s.player);
        if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) return out;
        player=lock.GetBody().GetCenterOfMassPosition();
        inherited=lock.GetBody().GetLinearVelocity();
        player_inverse_mass=lock.GetBody().GetMotionProperties()->GetInverseMass();
    }
    aim=aim.Normalized();
    RVec3 point=launch_origin(player,aim);
    Vec3 velocity=inherited+launch_impulse(aim,s.charged_energy(),player_inverse_mass+1.0F/hook_mass_kg)/hook_mass_kg;
    const Vec3 gravity=s.system.GetGravity();
    const SphereShape envelope(.24F);
    const RopeBodyFilter filter(s.player,BodyID());
    ShapeCastSettings settings; settings.mReturnDeepestPoint=true;
    // The guide shares the real barrel-clearance envelope as well as flight.
    const RVec3 barrel=muzzle_start(player);
    ClosestHitCollisionCollector<CastShapeCollector> barrel_hits;
    const RShapeCast barrel_cast(&envelope,Vec3::sReplicate(1),RMat44::sTranslation(barrel),Vec3(point-barrel));
    s.system.GetNarrowPhaseQuery().CastShape(barrel_cast,settings,barrel,barrel_hits,{},{},filter);
    if (barrel_hits.HadHit()) {
        out.points[0]=barrel;
        out.points[1]=barrel+Vec3(point-barrel)*barrel_hits.mHit.mFraction;
        out.count=2; out.blocked=true;
        s.preview_cache=out; s.preview_at=s.preview_clock;
        return out;
    }
    out.points[out.count++]=point;
    constexpr float dt=1.0F/90.0F;
    for (unsigned segment=0;segment<32;++segment) {
        RVec3 next=point;
        for (unsigned tick=0;tick<18;++tick) { velocity+=gravity*dt; next+=velocity*dt; }
        const Vec3 relative(next-player);
        if (relative.LengthSq()>maximum_line_length_m*maximum_line_length_m) {
            // Bound the display by available spool. It never locks that point.
            const Vec3 direction(next-point), start(point-player);
            const float a=direction.LengthSq(), b=2*start.Dot(direction);
            const float c=start.LengthSq()-maximum_line_length_m*maximum_line_length_m;
            const float fraction=a>0 ? std::clamp((-b+std::sqrt(std::max(0.0F,b*b-4*a*c)))/(2*a),0.0F,1.0F) : 0;
            next=point+direction*fraction; out.line_limit=true;
        }
        ClosestHitCollisionCollector<CastShapeCollector> hits;
        const RShapeCast cast(&envelope,Vec3::sReplicate(1),RMat44::sTranslation(point),Vec3(next-point));
        s.system.GetNarrowPhaseQuery().CastShape(cast,settings,point,hits,{},{},filter);
        if (hits.HadHit()) { next=point+Vec3(next-point)*hits.mHit.mFraction; out.blocked=true; out.line_limit=false; }
        out.points[out.count++]=next;
        point=next;
        if (out.blocked || out.line_limit) break;
    }
    s.preview_cache=out; s.preview_at=s.preview_clock;
    return out;
}
bool GrapplingGun::set_shot_profile(ShotProfile profile) {
    auto &s=*impl_;
    if ((profile!=ShotProfile::Near && profile!=ShotProfile::Long) || s.rope || s.pickup_active || s.charging) return false;
    s.shot_profile=profile;
    return true;
}
bool GrapplingGun::fire(JPH::Vec3 aim) {
    const float energy=impl_->shot_energy();
    cancel_charge();
    return fire_with_energy(aim,energy);
}
bool GrapplingGun::fire_with_energy(JPH::Vec3 aim, float energy_j) {
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
            const Vec3 impulse=launch_impulse(aim,energy_j,inverse_mass);
            const auto paid_energy=[&]() {
                return .5*inverse_mass*(double(impulse.GetX())*impulse.GetX()+
                    double(impulse.GetY())*impulse.GetY()+double(impulse.GetZ())*impulse.GetZ());
            };
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
    cancel_charge();
    auto &s=*impl_;
    s.cancel_pickup();
    s.remove_line();
    s.state=LineState::Released; s.failure=Failure::None;
}
void GrapplingGun::pre_step(float dt) {
    using namespace JPH;
    auto &s=*impl_;
    if (std::isfinite(dt) && dt>0) {
        s.preview_clock+=dt;
        if (s.charging) s.charge_seconds=std::min(full_charge_seconds,s.charge_seconds+dt);
    }
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
        const auto result=FiniteLineReel::step(s.rope->rest(),radius,s.rope->tension(),dt,reel,
            minimum_line_length_m,maximum_line_length_m,
            [&](float rest,float collision_dt) { return s.rope->prospective_tension(rest,collision_dt); });
        s.spool_absorbed+=result.absorbed_j;
        s.reel_work+=result.work_j;
        s.rope->set_drive_limited(result.drive_limited);
        s.rope->set_rest(result.rest_length_m);
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
bool GrapplingGun::recovery_in_reach() const {
    Impl::RetrievalGrasp grasp;
    return impl_->find_retrieval_grasp(grasp)==Failure::None;
}
bool GrapplingGun::retrieve() {
    using namespace JPH;
    auto &s=*impl_;
    Impl::RetrievalGrasp grasp;
    const auto failure=s.find_retrieval_grasp(grasp);
    if (failure!=Failure::None) { s.failure=failure; return false; }
    const auto from=grasp.from, target=grasp.target;
    const auto best=grasp.index;
    const auto relative_velocity=grasp.relative_velocity;
    const auto &retrieved_shape=grasp.shape;
    const auto &retrieved_transform=grasp.transform;
    const auto contact_local=grasp.contact_local, hand_local=grasp.hand_local;
    const auto &retrieved_bounds=grasp.bounds;
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
    result.shot_profile=s.shot_profile; result.shot_energy_j=s.charging ? s.charged_energy() : s.shot_energy();
    result.charging=s.charging; result.charge_fraction=s.charge_fraction();
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
    cancel_charge();
    auto &s=*impl_;
    s.cancel_pickup(); s.pickup_work=0;
    s.remove_line();
    auto &bodies=s.system.GetBodyInterface();
    for (auto &hook:s.hooks) if (!hook.IsInvalid()) {
        bodies.RemoveBody(hook); bodies.DestroyBody(hook); hook=JPH::BodyID();
    }
    s.selected=-1; s.cartridges=capacity; s.shot_profile=ShotProfile::Long;
    s.state=LineState::Ready; s.failure=s.pending=Failure::None;
    s.reel_input=0;
    s.launch_work=s.last_launch_work=s.spool_absorbed=s.release_discarded=s.retrieval_absorbed=s.reel_work=0;
}
} // namespace scraperx::sim
