#pragma once
#include "sim/mechanism_kit.hpp"
#include <array>
#include <string>
#include <vector>
namespace scraperx::sim {
enum class CargoNetClips { EndRows, Perimeter };
struct CargoNetConfig final {
    std::uint64_t entity = 1953;
    JPH::RVec3 origin = JPH::RVec3(18.0,0.45,-117.9);
    JPH::Quat rotation = JPH::Quat::sIdentity();
    int columns = 9, rows = 23;
    float dx = 0.5F, dy = 10.3F/22.0F, knot_half_width = 0.04F;
    float vertex_mass = 0.06F;
    float stretch_compliance = 5.0e-6F, shear_compliance = 5.0e-5F, bend_compliance = 2.0e-3F;
    float damping = 1.2F;
    // Passive axial strain-rate loss, as a fraction of each native edge pair's
    // critical damping. Zero preserves legacy cloth; requires internal clips
    // and positive native edge compliance (no infinite-stiffness dashpot).
    float axial_damping_ratio = 0.0F;
    unsigned iterations = 8;
    CargoNetClips clips = CargoNetClips::EndRows;
    float clip_stiffness = 5000.0F, clip_damping = 25.0F;
    bool paired_sleep = false;
    bool internally_integrated_clips = false;
};
// One actual Jolt soft body. Read-back is the same deforming mesh used for
// collision, grip material coordinates, rider load and Godot presentation.
class CargoNet final {
public:
    static constexpr std::uint64_t kEntity = 1953;
    CargoNet(JPH::PhysicsSystem &system, JPH::ObjectLayer layer, JPH::BodyID frame);
    CargoNet(JPH::PhysicsSystem &system, JPH::ObjectLayer layer, JPH::BodyID frame, const CargoNetConfig &config);
    [[nodiscard]] const CargoNetConfig &config() const noexcept { return config_; }
    [[nodiscard]] int columns() const noexcept { return config_.columns; }
    [[nodiscard]] int rows() const noexcept { return config_.rows; }
    [[nodiscard]] std::size_t attachment_count() const noexcept { return attachment_vertices_.size(); }
    // Native immutable contact selector, installed only outside PhysicsSystem::Update.
    // Contact point is rigid COM-local; normal is world-space toward the rigid body.
    // Caller context must outlive this net. Callback must not lock or mutate physics.
    using ContactFrictionCallback = float (*)(void *, JPH::BodyID, JPH::Vec3, JPH::Vec3, float) noexcept;
    void set_contact_friction_callback(ContactFrictionCallback callback, void *context);
    void pre_step(float dt);
    // Owner thread, after PhysicsSystem::Update, refresh and contact measurement.
    // Keeps the finite clip-connected net/frame in one native sleep lifecycle.
    void post_step(float dt, bool externally_touched = false);
    ~CargoNet();
    CargoNet(const CargoNet &) = delete;
    CargoNet &operator=(const CargoNet &) = delete;
    [[nodiscard]] JPH::BodyID body() const noexcept { return body_; }
    void refresh();
    [[nodiscard]] bool grip(JPH::RVec3 centre, JPH::RVec3 aim, JPH::Vec3 facing, JPH::RVec3 &point) const noexcept;
    [[nodiscard]] JPH::Vec3 material_point(JPH::RVec3 world) const noexcept;
    [[nodiscard]] JPH::Vec3 material_velocity(JPH::Vec3 material) const noexcept;
    [[nodiscard]] JPH::RVec3 world_point(JPH::Vec3 material) const noexcept;
    [[nodiscard]] float material_inverse_mass(JPH::Vec3 material) const noexcept;
    void impulse(JPH::Vec3 material, JPH::Vec3 world_impulse);
    // Owner thread, outside Update: one ongoing finite material load. Internal
    // clips integrate it at native cloth cadence; legacy nets use impulse(F*dt).
    // Invalid/nonfinite inputs throw invalid_argument; overlapping nonzero loads
    // throw logic_error without changing the pending load. Zero work is a no-op.
    // Capture at an owner boundary with no pending load; restore cancels it.
    void force(JPH::Vec3 material, JPH::Vec3 world_force, float duration_seconds);
    [[nodiscard]] const std::vector<JPH::RVec3> &vertices() const noexcept { return vertices_; }
    [[nodiscard]] std::vector<JPH::RVec3> render_vertices(float alpha) const;
    [[nodiscard]] JPH::RVec3 render_world_point(JPH::Vec3 material, float alpha) const noexcept;
    [[nodiscard]] const std::vector<std::uint32_t> &indices() const noexcept { return indices_; }
    [[nodiscard]] std::string capture() const;
    void restore(const std::string &state);
private:
    [[nodiscard]] JPH::RVec3 node(int c, int r) const noexcept;
    [[nodiscard]] JPH::RVec3 surface(float c, float r) const noexcept;
    [[nodiscard]] JPH::RVec3 surface(float c, float r, const std::vector<JPH::RVec3> &vertices) const noexcept;
    [[nodiscard]] std::uint32_t vertex(int c, int r, int corner) const noexcept { return std::uint32_t(4*(r*config_.columns+c)+corner); }
    CargoNetConfig config_;
    std::vector<std::uint32_t> attachment_vertices_;
    JPH::PhysicsSystem &system_;
    JPH::BodyID body_;
    JPH::BodyID frame_;
    std::vector<JPH::RVec3> attachment_local_;
    std::vector<JPH::RVec3> vertices_;
    std::vector<JPH::RVec3> previous_vertices_;
    std::vector<std::uint32_t> indices_;
    float equilibrium_time_ = 0.0F;
    bool sleeping_frame_valid_ = false;
    JPH::RVec3 sleeping_frame_position_ = JPH::RVec3::sZero();
    JPH::Quat sleeping_frame_rotation_ = JPH::Quat::sIdentity();
    bool clip_step_started_ = false;
    JPH::RMat44 clip_frame_transform_ = JPH::RMat44::sIdentity();
    JPH::Mat44 clip_frame_inverse_inertia_ = JPH::Mat44::sZero();
    float clip_frame_inverse_mass_ = 0.0F;
    JPH::Vec3 clip_frame_linear_ = JPH::Vec3::sZero();
    JPH::Vec3 clip_frame_angular_ = JPH::Vec3::sZero();
    JPH::Vec3 clip_reaction_ = JPH::Vec3::sZero();
    JPH::Vec3 clip_torque_ = JPH::Vec3::sZero();
    void internal_clip_step(float dt);
    void finish_clip_step(JPH::BodyInterface &bodies);
    void wake_pair();
    [[nodiscard]] bool sleeping_frame_changed() const;
    // Transient owner force, deliberately outside serialized native state.
    std::array<std::uint32_t,16> force_vertices_{};
    std::array<float,16> force_weights_{};
    JPH::Vec3 pending_world_force_ = JPH::Vec3::sZero();
    double force_remaining_seconds_ = 0.0;
};
}
