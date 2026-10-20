#pragma once
#include "sim/grapple_rope_constraint.hpp"
#include <array>
#include <cstddef>

namespace scraperx::sim {
// Geometry owner supplies derivatives of ONE cable length, in world axes.
// Body lifetimes and refresh before island construction remain the owner's job.
class FiniteCableConstraint final : public JPH::Constraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    static constexpr std::size_t capacity = 9;
    struct Participant {
        JPH::Body *body = nullptr;
        JPH::Vec3 linear = JPH::Vec3::sZero(); // dL/d COM translation, unitless
        JPH::Vec3 angular = JPH::Vec3::sZero(); // dL/d rotation, metres
    };
    struct Reaction {
        JPH::BodyID body;
        JPH::Vec3 force_n = JPH::Vec3::sZero();
        JPH::Vec3 torque_nm = JPH::Vec3::sZero();
    };
    enum class Admission { Accepted, InvalidGeometry, CapacityExceeded, NoDynamicBody };
    explicit FiniteCableConstraint(float rest_length_m);
    // Repeated bodies are combined before inverse mass/inertia. Any rejection
    // clears geometry, preventing a stale row; it never marks the line broken.
    // Call at the serialized collision OnStep boundary, before islands. Cold
    // setup discards old impulse; geometry changes do not change rest length.
    Admission set_geometry(const Participant *participants,std::size_t count,double length_m);
    bool set_rest(float rest_length_m);
    float rest() const { return rest_; }
    void set_drive_limited(bool value) { drive_limited_=value; }
    double prospective_tension(float rest_length_m,float collision_dt) const;
    float tension() const;
    bool broken() const { return broken_; }
    bool geometry_valid() const { return geometry_valid_; }
    double length() const { return length_; }
    std::size_t participant_count() const { return count_; }
    const Reaction &reaction(std::size_t index) const { return reactions_.at(index); }
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User3; }
    void NotifyShapeChanged(const JPH::BodyID &,JPH::Vec3Arg) override;
    bool IsActive() const override;
    void ResetWarmStart() override;
    void SetupVelocityConstraint(float collision_dt) override;
    void WarmStartVelocityConstraint(float) override {} // explicitly cold per collision
    bool SolveVelocityConstraint(float) override;
    bool SolvePositionConstraint(float,float) override { return false; }
    void BuildIslands(JPH::uint32,JPH::IslandBuilder &,JPH::BodyManager &) override;
    JPH::uint BuildIslandSplits(JPH::LargeIslandSplitter &) const override;
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override { return nullptr; }
    // Raw participants and impulses are never checkpointed. Remove this row
    // before snapshots, then rebuild cold from the owning cable's saved state.
    void SaveState(JPH::StateRecorder &) const override;
    void RestoreState(JPH::StateRecorder &) override;
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *) const override {}
#endif
private:
    double inverse_mass() const;
    double speed() const;
    void update_reactions();
    std::array<Participant,capacity> parts_{};
    std::array<Reaction,capacity> reactions_{};
    std::size_t count_ = 0;
    double length_ = 0, lambda_ = 0, gamma_ = 0, bias_ = 0, effective_mass_ = 0, cap_ = 0;
    float rest_, dt_ = 0;
    bool geometry_valid_ = false, solving_ = false, broken_ = false, drive_limited_ = false;
};
} // namespace scraperx::sim
