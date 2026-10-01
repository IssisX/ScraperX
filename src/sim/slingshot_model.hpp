#pragma once

#include <array>
#include <cstddef>

// Standalone SI-unit constitutive model. Jolt remains the authority for the
// moving pouch, rider, contacts, and integration; this module never sets a
// launch velocity or integrates a second runtime body.
namespace scraperx::sim::slingshot {

struct Vec3 final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] Vec3 operator+(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 operator-(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 operator*(Vec3 a, double scale) noexcept;
[[nodiscard]] Vec3 operator/(Vec3 a, double scale) noexcept;
[[nodiscard]] double dot(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] double length(Vec3 a) noexcept;
[[nodiscard]] Vec3 normalized(Vec3 a) noexcept;

struct BandParameters final {
    // CHOSEN ground loading geometry: fork midpoint 10 m from the neutral
    // pouch, anchors +/-3 m across it. DERIVED L0=sqrt(10^2+3^2).
    double natural_length_m = 10.44030650891055;
    double stiffness_n_per_m = 10000.0;
    // CHOSEN small passive material loss, not a launch-speed governor.
    double damping_ns_per_m = 15.0;
};

struct AnchorPoints final {
    std::array<Vec3, 2> position{};
    std::array<Vec3, 2> velocity{};
};

struct BandState final {
    double length_m = 0.0;
    double extension_m = 0.0;
    double extension_rate_mps = 0.0;
    double elastic_tension_n = 0.0;
    double tension_n = 0.0;
    double elastic_energy_j = 0.0;
    double dissipated_power_w = 0.0;
    Vec3 force_on_pouch_n{};
    Vec3 reaction_on_anchor_n{};
};

struct BandEvaluation final {
    std::array<BandState, 2> bands{};
    Vec3 elastic_force_n{};
    Vec3 force_n{};
    double elastic_energy_j = 0.0;
    double dissipated_power_w = 0.0;
    bool valid = false;
};

// Each bundle is tension-only: x=max(0, |p-a|-L0), U=k*x*x/2,
// F=-grad(U). The dashpot only acts in a taut band and cannot turn its
// tension negative. Anchor reactions are equal and opposite. Anchor
// velocity is included so a moving fork's mechanical work remains visible.
[[nodiscard]] BandEvaluation evaluate_bands(
    Vec3 pouch_position, Vec3 pouch_velocity, const AnchorPoints &anchors,
    const BandParameters &parameters = {}) noexcept;

// Convenience geometry for a fixed symmetric fork. yaw=0 aims toward -Z;
// positive elevation aims up. This does not move any runtime body.
[[nodiscard]] Vec3 aim_axis(double yaw_radians, double elevation_radians) noexcept;
[[nodiscard]] AnchorPoints symmetric_anchors(Vec3 midpoint, double half_span_m,
                                             double yaw_radians) noexcept;

struct DrawPath final {
    Vec3 undrawn_pouch_position{};
    // Any nonzero vector; it is normalized internally. Positive draw moves
    // the pouch along this vector, normally away from the fork midpoint.
    Vec3 pull_direction{0.0, 0.0, 1.0};
    AnchorPoints anchors{};
    // Positive conservative/passive opposition along the draw path. Add
    // m*g*dy/dq when the winch lifts a load; don't omit gravity work. When
    // gravity assists draw, this model conservatively credits none of it.
    double opposing_load_n = 0.0;
};

[[nodiscard]] Vec3 pouch_at_draw(const DrawPath &path, double draw_m) noexcept;
[[nodiscard]] double spring_energy_at_draw(const DrawPath &path, double draw_m,
                                          const BandParameters &bands = {}) noexcept;
[[nodiscard]] double draw_resistance_n(const DrawPath &path, double draw_m,
                                     const BandParameters &bands = {}) noexcept;

struct WinchParameters final {
    double maximum_draw_m = 12.0;
    // CHOSEN explicitly exaggerated manual player strength, requested by
    // the user. These are not claims about ordinary human performance.
    // The API also accepts physically human-scale or funded source values.
    double maximum_source_power_w = 200000.0;
    // DERIVED 160 kN pouch capacity / (4 * 0.88).
    double maximum_source_force_n = 45454.545454545456;
    double maximum_source_speed_mps = 16.0;
    double mechanical_advantage = 4.0;
    double efficiency = 0.88;
};

struct WinchState final {
    double held_draw_m = 0.0;
    double source_work_j = 0.0;
    double spring_work_j = 0.0;
    double opposing_load_work_j = 0.0;
    double transmission_loss_j = 0.0;
    bool latch_closed = true;
    double band_loss_j = 0.0;
};

struct DrawStep final {
    double held_draw_m = 0.0;
    double elastic_energy_j = 0.0;
    double source_work_j = 0.0;
    double spring_work_j = 0.0;
    double opposing_load_work_j = 0.0;
    double band_loss_j = 0.0;
    double source_travel_m = 0.0;
    double average_source_force_n = 0.0;
    double draw_resistance_n = 0.0;
    bool at_draw_stop = false;
    bool force_stalled = false;
    bool valid = false;
};

// Ideal one-way ratchet with bounded work, travel, and force. Its held
// coordinate is the ONLY kinematic output; the integration owner must bind
// that coordinate to its real draw restraint. Opening the latch merely
// removes that restraint. evaluate_bands then supplies actual forces.
//
// The source pays [U(q_new)-U(q_old)+opposition*dq+band loss]/efficiency.
// Elastic work uses the exact potential increment; passive band loss over
// each imposed draw step uses fixed five-point Gaussian quadrature.
// No unused power is accumulated and no source work is charged while idle.
// Keep the path fixed while charged, or separately account for work done
// moving the fork/aim frame. Kinematically rotating a charged assembly can
// otherwise inject gravitational work even if its spring U is unchanged.
class DrawWinch final {
public:
    explicit DrawWinch(WinchParameters parameters = {}) noexcept;
    [[nodiscard]] const WinchState &state() const noexcept { return state_; }
    [[nodiscard]] const WinchParameters &parameters() const noexcept { return parameters_; }
    [[nodiscard]] DrawStep crank(const DrawPath &path, const BandParameters &bands,
                                 double effort, double delta_seconds) noexcept;
    void release() noexcept { state_.latch_closed = false; }
    // Relatch only when the actual pouch meets a physical ratchet/seat;
    // this accepts that body's present draw and performs no motion/work.
    [[nodiscard]] bool relatch(double actual_draw_m) noexcept;
    [[nodiscard]] bool restore(const WinchState &state) noexcept;

private:
    WinchParameters parameters_{};
    WinchState state_{};
};

} // namespace scraperx::sim::slingshot
