#pragma once
// Every hit a tracked body takes, read from its own motion. After each physics
// step a body whose velocity changed by more than kMinChangeMps in the tick
// (about three g held for a tick; gravity's own change is a ninth of a m/s)
// was struck, and half its mass times that change squared is the hit's
// energy. It is the detection the loose stock used for the Matron's crash
// hearing, widened to the forklifts and the yard crane's hook block and kept
// as a list: what the presentation plays and what the Matron hears are the
// same hits.
//
// Deterministic: the bodies are read in the order they were tracked, after
// the step; nothing comes from the contact callbacks, whose order is the job
// system's. A body in the player's hands is not read: the carry's own pull
// would be heard as hits.
#include "sim/simulation.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace scraperx::sim {

class ImpactLedger final {
public:
    static constexpr float kMinChangeMps = 0.35F;
    static constexpr float kMinEnergyJ = 1.0F;
    // A body heard again within this many ticks only for a hit at least twice
    // as hard: a bounce's first strike, not the chatter of its contacts.
    static constexpr int kQuietTicks = 6;
    // The hardest hits of one tick are kept; the rest are dropped.
    static constexpr std::size_t kMaxPerTick = 8;
    // The list read: when full, the oldest goes.
    static constexpr std::size_t kKept = 32;
    // How loud a hit is to the Matron, 0..1 (matron.hpp hear: a sound of 1
    // carries 55 m): nothing at kSilentJ, full at kFullJ, by the log between. A 55 kg
    // drum dropped 1 m (~540 J) is 0.68, an 18 kg toolbox set down hard
    // (~36 J) 0.29, a walk's own noise.
    static constexpr float kSilentJ = 5.0F;
    static constexpr float kFullJ = 5000.0F;

    explicit ImpactLedger(JPH::PhysicsSystem &physics) : physics_(physics) {}

    void track(JPH::BodyID body, std::uint64_t entity, ImpactSound sound, float mass_kg);
    // A tower machine's moving part (a cage, cab, platform, counterweight,
    // boom, door leaf): its hits are read like any tracked body's. With `runs`
    // it is also the machine's running source: while it moves at least
    // kRunMinMps its guides, ropes and wheels are heard from its measured
    // kinetic energy (run_loudness). Only dynamic bodies are taken; a static
    // or kinematic one is skipped and false comes back.
    bool track_machine(JPH::BodyID body, std::uint64_t entity, ImpactSound sound, float mass_kg, bool runs);
    // A running machine this tick: where, and how loud (0..1).
    struct Running {
        std::uint64_t entity = 0;
        JPH::RVec3 at = JPH::RVec3::sZero();
        float speed_mps = 0.0F;
        float loudness = 0.0F;
    };
    // Running noise: below kRunMinMps a machine is quiet (creep, settling).
    // Above, loudness(half m v^2) times kRunShare, held to kRunMaxLoudness
    // (the yard crane's unloaded hoist, 0.35): a machine running is quieter
    // than the same energy spent in one strike.
    static constexpr float kRunMinMps = 0.1F;
    static constexpr float kRunShare = 0.5F;
    static constexpr float kRunMaxLoudness = 0.35F;
    [[nodiscard]] static float run_loudness(float mass_kg, float speed_mps) noexcept;
    // The running machines of the last step, in the order they were tracked.
    [[nodiscard]] const std::vector<Running> &running() const noexcept { return running_; }
    // The machine parts taken by track_machine, in that order: entity, and
    // whether it is its stage's running source.
    struct Machine {
        std::uint64_t entity = 0;
        bool runs = false;
    };
    [[nodiscard]] const std::vector<Machine> &machines() const noexcept { return machines_; }
    // The body of a part taken by track_machine; invalid for anything else.
    [[nodiscard]] JPH::BodyID machine_body(std::uint64_t entity) const noexcept;
    // After the physics step: the hits of this tick. `carried` is the entity in
    // the player's hands (0 for none).
    void post_step(std::uint64_t carried);
    // After a restore puts the bodies back: their velocities are taken as they
    // now are, so the jump is not heard as a hit.
    void resync();
    // One body's velocity taken as it now is (set by hand: a throw, a launch),
    // so the change is not heard as a hit.
    void resync_body(std::uint64_t entity);

    // The last step's hits, hardest first (what the Matron hears).
    [[nodiscard]] const std::vector<ImpactSnapshot> &tick() const noexcept { return tick_; }
    // The kept hits, oldest first.
    [[nodiscard]] std::vector<ImpactSnapshot> kept() const;
    // The newest hit's sequence, 0 before the first.
    [[nodiscard]] std::uint64_t last_sequence() const noexcept { return sequence_; }
    [[nodiscard]] static float loudness(float energy_j) noexcept;
    [[nodiscard]] std::size_t tracked() const noexcept { return tracked_.size(); }

private:
    struct Tracked {
        JPH::BodyID body;
        std::uint64_t entity = 0;
        ImpactSound sound = ImpactSound::Timber;
        float mass_kg = 0.0F;
        JPH::Vec3 last_velocity = JPH::Vec3::sZero();
        int quiet_ticks = 0;
        float last_energy_j = 0.0F;
        bool runs = false;
    };

    JPH::PhysicsSystem &physics_;
    std::vector<Tracked> tracked_;
    std::vector<ImpactSnapshot> tick_;
    std::vector<Running> running_;
    std::vector<Machine> machines_;
    std::array<ImpactSnapshot, kKept> ring_{};
    std::size_t ring_next_ = 0;
    std::size_t ring_count_ = 0;
    std::uint64_t sequence_ = 0;
};

} // namespace scraperx::sim
