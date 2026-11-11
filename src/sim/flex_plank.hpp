#pragma once

// J-096 slice 1 (#414): a plank that bends under what stands on it, springs
// back when the load leaves, and snaps past its strength.
//
// The plank is a cantilever: clamped at its root, free at its end. It is
// kSegments rigid boxes of its real timber, joined end to end by Jolt hinges
// about the plank's cross axis. Each hinge is a position motor whose spring
// is the beam's own bending stiffness, so whatever stands on it (him, her, a
// drum) sags it by its weight, through its contacts, and nothing reads who
// it is.
//
// The beam (Euler-Bernoulli, small deflection), length L, section b x h:
//   I = b h^3 / 12, Z = b h^2 / 6 (section modulus).
//   A load P at the free end deflects it d = P L^3 / (3 E I), so the tip's
//   stiffness is K = 3 E I / L^3.
// The chain: n segments of s = L / n, a hinge at x_i = i s (i = 0 the root,
// held to the world), each with the same rotational spring k. Under P at the
// end each hinge carries the beam's moment P (L - x_i) and turns
// theta_i = P (L - x_i) / k; the end drops sum theta_i (L - x_i). Setting
// that equal to the beam's P / K gives the conversion:
//   k = K * sum_i (L - x_i)^2 = 3 E I sum_i (L - x_i)^2 / L^3
//     = E I (n + 1)(2n + 1) / (2 n L)            [N m / rad]
// (n = 4: k = 4.6875 E I / L). Each hinge's moment is then exactly the
// beam's at that point (statically, for an end load), so its stress
// M / Z is the board's.
// Damping: the tip's mode critically damped (zeta = 1) for one man at the
// end, c_tip = 2 sqrt(K (m_design + 33/140 m_plank)) [N s/m] (33/140: a
// cantilever's own mass at its tip, Rayleigh), carried to the hinges by the
// same sum: c = c_tip * sum_i (L - x_i)^2 [N m s / rad]. Lighter loads
// settle overdamped, heavier ones with a little overshoot; none rings.
// Inertia floor (numerical, stated): Jolt's iterative solver cannot hold an
// 85 kg man on a 1.1 kg segment (a heavy load on a light body; the contact,
// off the segment's centre, turns it faster than its hinges can answer in
// the step's iterations, and the board went many times softer than the beam
// while he walked). Each segment keeps its real mass and weight, but its
// rotational inertia is raised to m_design (s / 2)^2, the load's own about
// the segment's centre from its end (x233 for this board), and the island
// takes kVelocitySteps iterations. Statics are untouched (deflection and
// moment depend on k and the loads alone); the loaded mode carries about
// 4 (m_design (s/2)^2)(1.5 / L)^2 more at the tip, +14% on his 85 kg, its
// frequency -6%. A snapped piece gets its real inertia back.
// Snap: a hinge whose elastic moment k |theta| passes the rupture moment
// M_r = f_m Z breaks, and every hinge outboard of it with it: the segments
// past the break fall as loose pieces (J-038's breaks make them loose stock;
// not yet in this slice).
//
// Determinism: segments, hinges and entity ids are made in a fixed order;
// post_step reads and breaks hinges root to end. Tests and the eye check
// only: no plank is built in the shipped world (Simulation::debug_add_flex_plank).

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstdint>

namespace scraperx::sim {

struct Timber final {
    float youngs_modulus_pa = 0.0F;   // E: bending, mean
    float bending_strength_pa = 0.0F; // f_m: the bending stress it breaks at
    float density_kg_m3 = 0.0F;
};

// EN 338:2016 table 1, strength class C24, the common structural softwood
// (spruce, pine, fir; scaffold boards are graded to it): E_0,mean 11.0 kN/mm2,
// f_m,k 24 N/mm2 (the characteristic bending strength: 95% of C24 boards
// carry it), rho_mean 420 kg/m3.
inline constexpr Timber kTimberC24{11.0e9F, 24.0e6F, 420.0F};

class FlexPlank final {
public:
    static constexpr int kSegments = 4;
    // The hinges' island solved this many velocity iterations (Jolt's default
    // 10 leaves a man's weight on the board soft while he walks; see
    // "Inertia floor" above).
    static constexpr int kVelocitySteps = 50;
    // Entity ids: a range of their own, above fabricated objects' (0x20000).
    static constexpr std::uint64_t kEntityBase = 0x30000;
    static constexpr std::uint32_t kMaxPlanks = 16;
    static constexpr std::uint64_t kEntityLimit = kEntityBase + kMaxPlanks * kSegments;
    [[nodiscard]] static constexpr bool is_entity(const std::uint64_t entity) noexcept {
        return entity >= kEntityBase && entity < kEntityLimit;
    }

    struct Spec final {
        // The top face's centre line at the root, where the plank leaves its
        // clamp, and the horizontal way it runs out.
        JPH::RVec3 root = JPH::RVec3::sZero();
        JPH::Vec3 along = JPH::Vec3::sAxisX();
        // A BS 2482 scaffold board, 225 x 38 mm, out 1.2 m.
        float length_m = 1.2F;
        float width_m = 0.225F;
        float thickness_m = 0.038F;
        Timber timber = kTimberC24;
        // The load its end is critically damped for (one man: kPlayerMassKg).
        float design_load_kg = 85.0F;
        // Its bodies' object layer (the world's moving bodies').
        JPH::ObjectLayer layer = 1;
    };

    FlexPlank(JPH::PhysicsSystem &physics, const Spec &spec, std::uint64_t entity_base);
    ~FlexPlank();
    FlexPlank(const FlexPlank &) = delete;
    FlexPlank &operator=(const FlexPlank &) = delete;

    // After each physics step: every whole hinge's moment read, and a hinge
    // past the rupture moment broken with every one outboard of it.
    void post_step();

    [[nodiscard]] const Spec &spec() const noexcept { return spec_; }
    [[nodiscard]] float second_moment_m4() const noexcept;   // I
    [[nodiscard]] float section_modulus_m3() const noexcept; // Z
    [[nodiscard]] float tip_stiffness_n_per_m() const noexcept { return tip_stiffness_; }
    [[nodiscard]] float joint_stiffness_nm_per_rad() const noexcept { return joint_stiffness_; }
    [[nodiscard]] float joint_damping_nms_per_rad() const noexcept { return joint_damping_; }
    [[nodiscard]] float rupture_moment_nm() const noexcept;
    [[nodiscard]] float segment_mass_kg() const noexcept;

    [[nodiscard]] JPH::BodyID segment(const int i) const noexcept { return segments_[static_cast<std::size_t>(i)]; }
    [[nodiscard]] bool joint_whole(const int i) const noexcept { return joints_[static_cast<std::size_t>(i)] != nullptr; }
    [[nodiscard]] float moment_nm(const int i) const noexcept { return moments_[static_cast<std::size_t>(i)]; }
    [[nodiscard]] float peak_moment_nm() const noexcept { return peak_moment_; }
    [[nodiscard]] bool snapped() const noexcept { return snapped_joint_ >= 0; }
    [[nodiscard]] int snapped_joint() const noexcept { return snapped_joint_; }
    [[nodiscard]] float snap_moment_nm() const noexcept { return snap_moment_; }
    // The top face's centre at the free end, where the last segment is now.
    [[nodiscard]] JPH::RVec3 tip() const;

private:
    void break_from(int joint);
    [[nodiscard]] JPH::MassProperties solver_mass_properties(JPH::Vec3 half) const;

    JPH::PhysicsSystem &physics_;
    Spec spec_;
    float tip_stiffness_ = 0.0F;
    float joint_stiffness_ = 0.0F;
    float joint_damping_ = 0.0F;
    JPH::Ref<JPH::GroupFilterTable> group_;
    std::array<JPH::BodyID, kSegments> segments_{};
    // Hinge 0 holds segment 0 to the world; hinge i (i > 0) segment i - 1 to i.
    std::array<JPH::Ref<JPH::HingeConstraint>, kSegments> joints_{};
    std::array<float, kSegments> moments_{};
    float peak_moment_ = 0.0F;
    int snapped_joint_ = -1;
    float snap_moment_ = 0.0F;
};

} // namespace scraperx::sim
