#pragma once
#include "sim/line_material_source.hpp"
#include <Jolt/Physics/Constraints/Constraint.h>
#include <array>

namespace scraperx::sim {
// Three Cartesian spring rows, with actual weighted cable endpoint Jacobians.
// Refresh only at the serialized pre-island boundary. No retained source pointer.
class LineMaterialHandConstraint final : public JPH::Constraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    LineMaterialHandConstraint();
    bool set_geometry(JPH::Body &player,const LineMaterialSample &,JPH::Vec3 rest_offset);
    void invalidate();
    JPH::Vec3 player_force() const;
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User3; }
    void NotifyShapeChanged(const JPH::BodyID &,JPH::Vec3Arg) override { invalidate(); }
    bool IsActive() const override;
    void ResetWarmStart() override;
    void SetupVelocityConstraint(float) override;
    void WarmStartVelocityConstraint(float) override {}
    bool SolveVelocityConstraint(float) override;
    bool SolvePositionConstraint(float,float) override { return false; }
    void BuildIslands(JPH::uint32,JPH::IslandBuilder &,JPH::BodyManager &) override;
    JPH::uint BuildIslandSplits(JPH::LargeIslandSplitter &) const override;
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override { return nullptr; }
    void SaveState(JPH::StateRecorder &) const override;
    void RestoreState(JPH::StateRecorder &) override;
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *) const override {}
#endif
private:
    struct Participant {
        JPH::Body *body=nullptr;
        float weight=0; // signed derivative of player - material
        JPH::Vec3 arm=JPH::Vec3::sZero(); // signed weighted COM moment arm
    };
    std::array<Participant,3> parts_{};
    std::array<double,3> lambda_{},mass_{},bias_{};
    JPH::Vec3 error_=JPH::Vec3::sZero();
    unsigned count_=0;
    double gamma_=0,cap_=0;
    float dt_=0;
    bool valid_=false,solving_=false;
};
}
