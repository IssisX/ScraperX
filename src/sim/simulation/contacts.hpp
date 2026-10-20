#pragma once

#include "sim/simulation/constants.hpp"
#include "sim/slingshot.hpp"
#include "sim/traversal_affordance.hpp"
#include "sim/traversal_plan.hpp"
#include "sim/physical_foot_push.hpp"
#include "sim/physical_story_actor.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyManifold.h>
#include <atomic>
#include <mutex>
#include <vector>

namespace scraperx::sim::detail {

struct SupportSample final {
    bool grounded = false;
    std::uint64_t entity_id = 0;
    scraperx::sim::Vector3 contact_point{};
    scraperx::sim::Vector3 point_velocity{};
    float normal_y = 0.0F;
    scraperx::sim::Vector3 normal{0, 1, 0};
    scraperx::sim::Vector3 player_point_velocity{};
    scraperx::sim::Vector3 player_contact_point{};
    float approach_normal_speed = 0.0F;
    float approach_tangent_speed = 0.0F;
    JPH::BodyID body_id;
    JPH::Vec3 soft_material = JPH::Vec3::sZero();
    JPH::SubShapeID subshape{};
    JPH::Vec3 material_point = JPH::Vec3::sZero();
    JPH::Vec3 material_normal = JPH::Vec3::sAxisY();
    const JPH::Shape *shape_identity = nullptr;
    float friction = 0;
    bool bottom_contact = false;
};

[[nodiscard]] bool entity_is_causal_section_support(std::uint64_t entity) noexcept;

// Supports that actually move. These outrank static ground when the player is
// in contact with both, so support-relative locomotion picks the machine.
[[nodiscard]] bool entity_is_moving_support(const std::uint64_t entity_id) noexcept;

struct ReclaimImpactSample {float speed=0;JPH::RVec3 position=JPH::RVec3::sZero();};

class PlayerContactListener final : public JPH::ContactListener {
public:
    void set_reclaim_fragments(std::pair<std::uint64_t,std::uint64_t> range)noexcept {reclaim_first_=range.first;reclaim_last_=range.second;}
    ReclaimImpactSample reclaim_impact()const noexcept {lock();const auto result=reclaim_sample_;unlock();return result;}
    void set_landing_feedback(bool enabled) noexcept { landing_feedback_ = enabled; }
    void set_slingshot(scraperx::sim::Slingshot *launcher) noexcept {
        slingshot_ = launcher;
    }
    // Owner-thread selection before Update. This records a separate actual
    // contact receipt; it never changes Jolt contacts or general ground rank.
    void set_transfer_receiver(JPH::BodyID body, const ReceiverSurfacePatch &patch) noexcept {
        lock();
        transfer_receiver_ = body;
        transfer_patch_ = patch;
        unlock();
    }
    // One canonical actual bottom manifold, selected without retaining an
    // unbounded callback list. Body identity/tick are immutable during Update.
    void set_story_actor(JPH::BodyID body, std::uint64_t tick) noexcept {
        lock();
        story_actor_body_ = body;
        story_actor_tick_ = tick;
        story_actor_contact_ = {};
        story_actor_contact_valid_ = false;
        unlock();
    }
    bool story_actor_contact(PhysicalStoryActor::Contact &out) const noexcept {
        lock();
        const bool valid = story_actor_contact_valid_;
        if (valid) out = story_actor_contact_;
        unlock();
        return valid;
    }
    void begin_tick() noexcept {
        lock();
        story_actor_contact_ = {};
        story_actor_contact_valid_ = false;
        sample_ = {};
        transfer_sample_ = {};
        wall_sample_ = {};
        transfer_receiver_ = JPH::BodyID();
        transfer_patch_ = {};
        impact_sample_ = {};
        reclaim_sample_={};
        inspection_seat_contact_ = false;
        unlock();
    }

    void begin_collision_step() noexcept {
        lock();
        // Stance belongs to the current substep; impact_sample_ retains the
        // strongest rigid strike across the entire tick for consequences.
        story_actor_contact_ = {};
        story_actor_contact_valid_ = false;
        sample_ = {};
        transfer_sample_ = {};
        wall_sample_ = {};
        inspection_seat_contact_ = false;
        unlock();
    }

    [[nodiscard]] bool inspection_span_seat_contact() const noexcept {
        lock();
        const bool result = inspection_seat_contact_;
        unlock();
        return result;
    }

    [[nodiscard]] SupportSample sample() const noexcept {
        lock();
        const SupportSample result = sample_;
        unlock();
        return result;
    }
    [[nodiscard]] SupportSample impact_sample() const noexcept {
        lock();
        const auto result = impact_sample_;
        unlock();
        return result;
    }
    [[nodiscard]] SupportSample transfer_sample() const noexcept {
        lock();
        const auto result = transfer_sample_;
        unlock();
        return result;
    }

    [[nodiscard]] scraperx::sim::FootContactSample wall_sample() const noexcept {
        lock();
        const auto result = wall_sample_;
        unlock();
        return result;
    }

    // AS-003: the entity on the player's carry point, 0 for none. The player
    // and what it holds share the carry point; contact between them is never
    // meaningful, only the constraint relates them.
    void set_carried_entity(const std::uint64_t entity) noexcept {
        carried_entity_.store(entity, std::memory_order_relaxed);
    }

    // AS-002: the deployed swing flight's own foot lands flush on top of the
    // (frozen, AS-001) handoff deck by design -- deployed foot at
    // (-6.000, 24.1872), exactly the deck's own centre and tread-surface
    // height (see kLegalFortyDeployedThetaRadians's comment). But the
    // flight's length and its 8.000 m rise together force theta_deployed to
    // 60 deg (cos 60 = rise / L = 8/16 = 0.5), so the final third of the
    // sweep -- theta in [48.8 deg, 60 deg], confirmed by a rotated-box vs
    // AABB separating-axis check -- drags the flight's own solid body
    // through the deck's near corner: up to 0.36 m of overlap, 0.156 m of
    // it still present at the final hinge-limit rest pose itself. No
    // AS-002-owned knob (hinge position, flight length, deployed angle) can
    // be retuned to clear this without unpicking the rope-crossing,
    // mid-landing-gap, or T_crit statics numbers the plan derives from this
    // exact geometry, and the deck itself is out of scope to touch. The
    // plan's own design note already settles what the deck's role should
    // be: "deployed foot must not bear: the stop is the hinge limit, not
    // the deck" -- it was never meant to be a physical obstacle to the
    // flight, so excluding this one body pair from collision is fidelity to
    // that intent, not a workaround for it. Every other pair keeps ordinary
    // collision, flight vs the player included, so a player caught in the
    // sweep is still struck and shoved exactly as the plan's own "standing
    // in the sweep when it deploys" note requires.
    [[nodiscard]] JPH::ValidateResult OnContactValidate(
        const JPH::Body &first, const JPH::Body &second, JPH::RVec3Arg,
        const JPH::CollideShapeResult &) override {
        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        using Sim = scraperx::sim::Simulation;
        const bool is_swing_flight_vs_handoff_deck =
            (first_entity == Sim::kIntakeSwingFlightEntityId &&
             second_entity == Sim::kIntakeHandoffEntityId) ||
            (first_entity == Sim::kIntakeHandoffEntityId &&
             second_entity == Sim::kIntakeSwingFlightEntityId);
        // Same reasoning, second body pair: the flight's own cross-section
        // is coincident with its hinge anchor at every sweep angle by
        // construction, so contact between them is never meaningful -- see
        // kIntakeSwingAnchorEntityId's own comment at the anchor's build
        // site for the diagnostic trace that found this.
        const bool is_swing_flight_vs_hinge_anchor =
            (first_entity == Sim::kIntakeSwingFlightEntityId &&
             second_entity == Sim::kIntakeSwingAnchorEntityId) ||
            (first_entity == Sim::kIntakeSwingAnchorEntityId &&
             second_entity == Sim::kIntakeSwingFlightEntityId);
        const std::uint64_t carried = carried_entity_.load(std::memory_order_relaxed);
        const bool is_player_vs_carried =
            carried != 0 &&
            ((first_entity == Sim::kPlayerEntityId && second_entity == carried) ||
             (first_entity == carried && second_entity == Sim::kPlayerEntityId));
        const bool is_facade_embedded_attachment =
            (first_entity==Sim::kTowerEntityId && scraperx::sim::is_facade_tower_attachment(second_entity)) ||
            (second_entity==Sim::kTowerEntityId && scraperx::sim::is_facade_tower_attachment(first_entity));
        return (is_facade_embedded_attachment || is_swing_flight_vs_handoff_deck || is_swing_flight_vs_hinge_anchor ||
                is_player_vs_carried)
                   ? JPH::ValidateResult::RejectAllContactsForThisBodyPair
                   : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body &first,
                        const JPH::Body &second,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &settings) override {
        if (!settings.mIsSensor) observe_story_actor(first, second, manifold);
        observe_slingshot(first, second);
        observe_inspection_seat(first, second, manifold);
        observe_support(first, second, manifold);
        observe_wall(first, second, manifold);
        observe_reclaim(first,second,manifold);
    }

    void OnContactPersisted(const JPH::Body &first,
                            const JPH::Body &second,
                            const JPH::ContactManifold &manifold,
                            JPH::ContactSettings &settings) override {
        if (!settings.mIsSensor) observe_story_actor(first, second, manifold);
        observe_slingshot(first, second);
        observe_inspection_seat(first, second, manifold);
        observe_support(first, second, manifold);
        observe_wall(first, second, manifold);
    }

private:
    void observe_story_actor(const JPH::Body &first, const JPH::Body &second,
                             const JPH::ContactManifold &manifold) noexcept {
        if (story_actor_body_.IsInvalid() ||
            (first.GetID() != story_actor_body_ && second.GetID() != story_actor_body_)) return;
        PhysicalStoryActor::Contact candidate;
        if (!PhysicalStoryActor::capture_contact(story_actor_body_, first, second,
            manifold, story_actor_tick_, candidate)) return;
        const auto key = [](const PhysicalStoryActor::Contact &c) {
            return std::make_tuple(c.support.GetIndexAndSequenceNumber(), c.subshape.GetValue(),
                c.local_point.GetX(), c.local_point.GetY(), c.local_point.GetZ());
        };
        lock();
        if (!story_actor_contact_valid_ || key(candidate) < key(story_actor_contact_)) {
            story_actor_contact_ = candidate;
            story_actor_contact_valid_ = true;
        }
        unlock();
    }
    void observe_inspection_seat(const JPH::Body &first, const JPH::Body &second,
                                 const JPH::ContactManifold &manifold) noexcept {
        using namespace scraperx::sim;
        const bool span_first = first.GetUserData() == kSplitInspectionGirderEntity &&
                                second.GetUserData() == kSplitInspectionSpanSeatEntity;
        const bool span_second = second.GetUserData() == kSplitInspectionGirderEntity &&
                                 first.GetUserData() == kSplitInspectionSpanSeatEntity;
        if ((!span_first && !span_second) || first.IsSensor() || second.IsSensor() ||
            manifold.mRelativeContactPointsOn1.empty()) return;
        const auto &span = span_first ? first : second;
        const auto &seat = span_first ? second : first;
        if (span.GetShape()->GetType() != JPH::EShapeType::Compound ||
            seat.GetShape()->GetType() != JPH::EShapeType::Compound) return;
        JPH::SubShapeID remainder;
        const auto span_part = static_cast<const JPH::CompoundShape *>(span.GetShape())
            ->GetSubShapeIndexFromID(span_first ? manifold.mSubShapeID1 : manifold.mSubShapeID2, remainder);
        const auto seat_part = static_cast<const JPH::CompoundShape *>(seat.GetShape())
            ->GetSubShapeIndexFromID(span_first ? manifold.mSubShapeID2 : manifold.mSubShapeID1, remainder);
        const auto normal = span_first ? -manifold.mWorldSpaceNormal : manifold.mWorldSpaceNormal;
        // Part0 is the beveled tip; part1 is the full-depth walking box.
        // The receiving plate is seat part0; bracket/side contact cannot pass.
        if (span_part != 1 || seat_part != 0 || normal.GetY() < kSupportNormalThreshold) return;
        lock();
        inspection_seat_contact_ = true;
        unlock();
    }

    void observe_reclaim(const JPH::Body&first,const JPH::Body&second,const JPH::ContactManifold&manifold)noexcept {
        const auto is_fragment=[this](std::uint64_t e){return reclaim_first_!=0&&e>=reclaim_first_&&e<=reclaim_last_;};
        if((!is_fragment(first.GetUserData())&&!is_fragment(second.GetUserData()))||manifold.mRelativeContactPointsOn1.empty())return;
        const auto p1=manifold.GetWorldSpaceContactPointOn1(0),p2=manifold.GetWorldSpaceContactPointOn2(0);
        // Accepted native contact, actual pre-solve relative closing speed.
        // Audio uses speed; this is deliberately not a solver impulse claim.
        const float speed=(first.GetPointVelocity(p1)-second.GetPointVelocity(p2)).Dot(manifold.mWorldSpaceNormal);
        if(speed<1.F)return;
        lock();if(speed>reclaim_sample_.speed)reclaim_sample_={speed,p1};unlock();
    }
    void observe_slingshot(const JPH::Body &first, const JPH::Body &second) noexcept {
        if (slingshot_ && (first.GetUserData() == scraperx::sim::Slingshot::kPouchEntity ||
                          second.GetUserData() == scraperx::sim::Slingshot::kPouchEntity))
            slingshot_->note_pouch_contact();
        if (slingshot_ && (first.GetUserData() == scraperx::sim::Slingshot::kLaunchRailEntity ||
                          second.GetUserData() == scraperx::sim::Slingshot::kLaunchRailEntity))
            slingshot_->note_carrier_contact();
        if (slingshot_ && (first.GetUserData() == scraperx::sim::Simulation::kPlayerEntityId ||
                          second.GetUserData() == scraperx::sim::Simulation::kPlayerEntityId))
            slingshot_->note_external_influence();
    }
    [[nodiscard]] static int support_rank(const std::uint64_t entity_id) noexcept {
        if (entity_id == 0 || entity_id == scraperx::sim::Simulation::kPlayerEntityId) {
            return 0;
        }
        return entity_is_moving_support(entity_id) ? 2 : 1;
    }

    void observe_wall(const JPH::Body &first, const JPH::Body &second,
                      const JPH::ContactManifold &manifold) noexcept {
        const bool player_first = first.GetUserData() == scraperx::sim::Simulation::kPlayerEntityId;
        const bool player_second = second.GetUserData() == scraperx::sim::Simulation::kPlayerEntityId;
        if ((!player_first && !player_second) || first.IsSensor() || second.IsSensor() ||
            !first.IsRigidBody() || !second.IsRigidBody()) return;
        const auto &player = player_first ? first : second;
        const auto &wall = player_first ? second : first;
        const auto normal = player_first ? -manifold.mWorldSpaceNormal : manifold.mWorldSpaceNormal;
        const auto count = player_first ? manifold.mRelativeContactPointsOn2.size() :
                                         manifold.mRelativeContactPointsOn1.size();
        if (count == 0 || std::abs(normal.GetY()) > .2F || wall.GetFriction() <= 0) return;
        const auto point = [&](JPH::uint i) {
            return player_first ? manifold.GetWorldSpaceContactPointOn2(i) :
                                  manifold.GetWorldSpaceContactPointOn1(i);
        };
        const auto centre = player.GetCenterOfMassPosition();
        const float depth = JPH::Vec3(centre-point(0)).Dot(normal);
        if (!(depth > 0)) return;
        // Select within the actual convex manifold patch. A vertical capsule
        // contributes a line: interpolate its real endpoints below the COM,
        // leaving friction-cone margin as the existing leg starts extending.
        const float drop = std::min(.8F*scraperx::sim::PhysicalFootPush::lateral_reach_m(),
                                    .8F*wall.GetFriction()*depth);
        const double target_y = centre.GetY()-drop;
        scraperx::sim::FootContactSample candidate;
        float best_error = std::numeric_limits<float>::max();
        const auto consider = [&](JPH::RVec3 anchor) {
            const auto delta = JPH::Vec3(centre-anchor);
            const float height = delta.Dot(normal);
            const float tangent = (delta-normal*height).Length();
            if (delta.GetY() < .05F || height <= 0 ||
                tangent > scraperx::sim::PhysicalFootPush::lateral_reach_m() ||
                tangent > wall.GetFriction()*height) return;
            const float error = float(std::abs(anchor.GetY()-target_y));
            if (error >= best_error) return;
            best_error = error;
            candidate.valid = true;
            candidate.body = wall.GetID();
            candidate.subshape = player_first ? manifold.mSubShapeID2 : manifold.mSubShapeID1;
            candidate.entity_id = wall.GetUserData();
            candidate.world_point = anchor;
            candidate.local_point = JPH::Vec3(wall.GetInverseCenterOfMassTransform()*anchor);
            candidate.world_normal = normal;
            candidate.local_normal = wall.GetRotation().Conjugated()*normal;
            candidate.point_velocity = wall.GetPointVelocity(anchor);
            candidate.friction = wall.GetFriction();
            candidate.shape_identity = wall.GetShape();
        };
        for (JPH::uint i=0;i<count;++i) {
            const auto a = point(i);
            consider(a);
            for (JPH::uint j=i+1;j<count;++j) {
                const auto b = point(j);
                const double span = b.GetY()-a.GetY();
                if (std::abs(span) < 1.0e-6) continue;
                const double fraction = (target_y-a.GetY())/span;
                if (fraction >= 0 && fraction <= 1) consider(a+(b-a)*float(fraction));
            }
        }
        if (!candidate.valid) return;
        lock();
        const auto key = [](const scraperx::sim::FootContactSample &sample) {
            return std::make_tuple(sample.entity_id,sample.body.GetIndexAndSequenceNumber(),
                sample.subshape.GetValue(),sample.local_point.GetX(),sample.local_point.GetY(),
                sample.local_point.GetZ(),sample.local_normal.GetX(),sample.local_normal.GetY(),
                sample.local_normal.GetZ());
        };
        // Callback order cannot choose a different face of a compound wall.
        if (!wall_sample_.valid || key(candidate) < key(wall_sample_)) wall_sample_ = candidate;
        unlock();
    }

    void observe_support(const JPH::Body &first,
                         const JPH::Body &second,
                         const JPH::ContactManifold &manifold) noexcept {
        if (manifold.mRelativeContactPointsOn1.size() == 0 ||
            manifold.mRelativeContactPointsOn2.size() == 0) {
            return;
        }

        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        std::uint64_t support_entity = 0;
        float support_normal_y = 0.0F;
        JPH::RVec3 support_contact_point{};
        JPH::RVec3 player_contact_point{};
        JPH::Vec3 normal = JPH::Vec3::sZero();
        const JPH::Body *player_body = nullptr;
        const JPH::Body *support_body = nullptr;

        if (first_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = second_entity;
            support_normal_y = -manifold.mWorldSpaceNormal.GetY();
            support_contact_point = manifold.GetWorldSpaceContactPointOn2(0);
            support_body = &second;
            player_body = &first;
            player_contact_point = manifold.GetWorldSpaceContactPointOn1(0);
            normal = -manifold.mWorldSpaceNormal;
        } else if (second_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = first_entity;
            support_normal_y = manifold.mWorldSpaceNormal.GetY();
            support_contact_point = manifold.GetWorldSpaceContactPointOn1(0);
            support_body = &first;
            player_body = &second;
            player_contact_point = manifold.GetWorldSpaceContactPointOn2(0);
            normal = manifold.mWorldSpaceNormal;
        }

        // WO-013: a sensor body (the wet grate) still produces a full contact
        // manifold -- Jolt's own doc comment is explicit that sensors "will
        // receive collision callbacks, but will not cause any collision
        // responses" -- so without this check a wet grate would read as
        // real support from geometry alone, even though no physical force
        // is actually holding the player up (they are in freefall through
        // it). A sensor is never a valid support.
        if (support_body == nullptr || support_body->IsSensor() ||
            support_normal_y < kSupportNormalThreshold || support_rank(support_entity) == 0) {
            return;
        }

        const JPH::Vec3 point_velocity = support_body->GetPointVelocity(support_contact_point);
        const JPH::Vec3 player_velocity = player_body->GetPointVelocity(player_contact_point);
        const JPH::Vec3 relative = player_velocity - point_velocity;
        const float normal_velocity = relative.Dot(normal);
        SupportSample candidate{
            true,
            support_entity,
            {support_contact_point.GetX(), support_contact_point.GetY(), support_contact_point.GetZ()},
            {point_velocity.GetX(), point_velocity.GetY(), point_velocity.GetZ()},
            support_normal_y,
            {normal.GetX(), normal.GetY(), normal.GetZ()},
            {player_velocity.GetX(), player_velocity.GetY(), player_velocity.GetZ()},
            {player_contact_point.GetX(), player_contact_point.GetY(), player_contact_point.GetZ()},
            std::max(0.0F, -normal_velocity),
            (relative - normal * normal_velocity).Length(),
            support_body->GetID(),
        };
        candidate.subshape = support_body == &first ? manifold.mSubShapeID1 : manifold.mSubShapeID2;
        candidate.material_point = JPH::Vec3(support_body->GetInverseCenterOfMassTransform() * support_contact_point);
        candidate.material_normal = support_body->GetRotation().Conjugated() * normal;
        candidate.shape_identity = support_body->GetShape();
        candidate.friction = support_body->GetFriction();
        const auto foot_delta = JPH::Vec3(player_contact_point - player_body->GetPosition());
        const auto half = player_body->GetShape()->GetLocalBounds().GetExtent().GetY();
        candidate.bottom_contact = foot_delta.GetY() <= -(half - kPlayerRadius) &&
            foot_delta.GetY() >= -half - .20F &&
            foot_delta.GetX()*foot_delta.GetX() + foot_delta.GetZ()*foot_delta.GetZ() <=
                (kPlayerRadius+.15F)*(kPlayerRadius+.15F);

        lock();
        // A compound may expose several coplanar children under the foot.
        // General stance ranking can select a different child even while
        // the accepted receiver also has a real solved bottom contact.
        if (candidate.bottom_contact && candidate.body_id == transfer_receiver_ &&
            transfer_patch_.contains(candidate.subshape) &&
            (!transfer_sample_.grounded || candidate.normal_y > transfer_sample_.normal_y))
            transfer_sample_ = candidate;
        if (!impact_sample_.grounded || candidate.approach_normal_speed > impact_sample_.approach_normal_speed ||
            (candidate.approach_normal_speed == impact_sample_.approach_normal_speed &&
             candidate.entity_id < impact_sample_.entity_id))
            impact_sample_ = candidate;
        const int current_rank = support_rank(sample_.entity_id);
        const int candidate_rank = support_rank(candidate.entity_id);
        if (!sample_.grounded || candidate_rank > current_rank ||
            (candidate_rank == current_rank &&
             ((!landing_feedback_ && candidate.normal_y > sample_.normal_y) ||
              (landing_feedback_ && (candidate.approach_normal_speed > sample_.approach_normal_speed ||
               (candidate.approach_normal_speed == sample_.approach_normal_speed && candidate.normal_y > sample_.normal_y)))))) {
            sample_ = candidate;
        }
        unlock();
    }

    void lock() const noexcept {
        while (lock_.test_and_set(std::memory_order_acquire)) {
        }
    }

    void unlock() const noexcept {
        lock_.clear(std::memory_order_release);
    }

    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    bool landing_feedback_ = false;
    SupportSample sample_{};
    SupportSample transfer_sample_{};
    scraperx::sim::FootContactSample wall_sample_{};
    JPH::BodyID transfer_receiver_;
    ReceiverSurfacePatch transfer_patch_;
    SupportSample impact_sample_{};
    ReclaimImpactSample reclaim_sample_{};
    bool inspection_seat_contact_ = false;
    std::uint64_t reclaim_first_=0,reclaim_last_=0;
    std::atomic<std::uint64_t> carried_entity_{0};
    scraperx::sim::Slingshot *slingshot_ = nullptr;
    JPH::BodyID story_actor_body_;
    std::uint64_t story_actor_tick_ = 0;
    PhysicalStoryActor::Contact story_actor_contact_{};
    bool story_actor_contact_valid_ = false;
};

// Callback only copies supplied soft manifold data. All player reads, material
// sampling and support decisions happen on the simulation owner after Update.
class PlayerSoftContactListener final : public JPH::SoftBodyContactListener {
public:
    struct Contact { JPH::BodyID soft, other; std::uint64_t entity; std::uint32_t vertex;
        JPH::RVec3 point; JPH::Vec3 normal; };
    void set_player(JPH::BodyID player) noexcept { player_ = player; }
    void begin_tick() { std::lock_guard<std::mutex> guard(mutex_); contacts_.clear(); touched_ = false; }
    void begin_collision_step() { std::lock_guard<std::mutex> guard(mutex_); contacts_.clear(); }
    std::vector<Contact> contacts() const { std::lock_guard<std::mutex> guard(mutex_); return contacts_; }
    bool catch_touched() const { std::lock_guard<std::mutex> guard(mutex_); return touched_; }
    void OnSoftBodyContactAdded(const JPH::Body &soft, const JPH::SoftBodyManifold &manifold) override {
        const auto transform = soft.GetCenterOfMassTransform();
        std::lock_guard<std::mutex> guard(mutex_);
        std::uint32_t index = 0;
        for (const auto &vertex : manifold.GetVertices()) {
            if (manifold.HasContact(vertex) && manifold.GetContactBodyID(vertex) == player_) {
                contacts_.push_back({soft.GetID(), player_, soft.GetUserData(), index,
                    transform * manifold.GetLocalContactPoint(vertex),
                    transform.Multiply3x3(manifold.GetContactNormal(vertex))});
                if (soft.GetUserData() == scraperx::sim::kCatchNetEntity) touched_ = true;
            }
            ++index;
        }
    }
private:
    mutable std::mutex mutex_;
    JPH::BodyID player_;
    std::vector<Contact> contacts_;
    bool touched_ = false;
};


} // namespace scraperx::sim::detail
