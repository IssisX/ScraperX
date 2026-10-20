#pragma once

#include "sim/mechanism_kit.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace scraperx::sim {

// Installation topology is separate from Kit's fixed machine/body inventory.
// All local points use the shape origin, never the centre-of-mass frame.
class RiggingSystem final {
public:
    static constexpr std::size_t kCapacity = 8;
    static constexpr std::size_t kEyeCapacity = 32;
    static constexpr float kInstallReachM = 1.8F; // Chosen assisted tool reach.
    enum class Kind : std::uint8_t { End, FixedGuide };
    enum class Failure : std::uint8_t {
        None, StaleTarget, OutOfReach, Obstructed, WrongTarget, MissingGuide,
        Degenerate, Capacity, Loaded, UnsafeContext, Overload
    };
    struct Eye {
        kit::BodyIndex body;
        JPH::BodyID identity;
        JPH::Vec3 local = JPH::Vec3::sZero();
        JPH::Vec3 guide_in = JPH::Vec3::sZero(), guide_out = JPH::Vec3::sZero();
        Kind kind = Kind::End;
        float rating_n = 8000;
    };
    struct Record {
        std::uint16_t first = 0, guide = 0, second = 0;
        float paid_length_m = 0, rating_n = 0;
        std::uint32_t generation = 0;
        bool occupied = false, broken = false, reversed = false;
    };
    struct Checkpoint { std::array<Record, kCapacity> lines{}; };
    struct Readback {
        bool equipped = false, suspended = false, draft = false, guide_threaded = false;
        std::uint64_t target = 0, selected = 0, event_serial = 0;
        Kind target_kind = Kind::End;
        Failure failure = Failure::None;
        JPH::RVec3 target_point = JPH::RVec3::sZero();
        float tension_n = 0, slack_m = 0;
        std::uint32_t installed_count = 0;
    };

    RiggingSystem(JPH::PhysicsSystem &, kit::Kit &);
    ~RiggingSystem();
    RiggingSystem(const RiggingSystem &) = delete;
    RiggingSystem &operator=(const RiggingSystem &) = delete;
    // Build-time registration only. No body/constraint is created by an eye.
    std::uint16_t add_eye(kit::BodyIndex, JPH::Vec3 local, float rating_n = 8000);
    std::uint16_t add_fixed_guide(kit::BodyIndex, JPH::Vec3 reachable_threading_eye,
                                JPH::Vec3 incoming, JPH::Vec3 outgoing, float rating_n = 8000);
    void equip(bool);
    void aim(JPH::RVec3 hand, JPH::Vec3 direction, JPH::BodyID player, bool allowed);
    bool action(std::uint64_t target);
    void undo();
    bool select(std::uint64_t line);
    [[nodiscard]] std::uint64_t line_handle(std::size_t slot) const;
    bool unclip(std::uint64_t line);
    bool cut(std::uint64_t line);
    void post_step(float collision_dt);
    [[nodiscard]] Readback readback() const;
    [[nodiscard]] Checkpoint capture() const;
    void detach_for_restore();
    void restore(const Checkpoint &);
    // Installed paths use the same body interpolation history as Kit cables.
    std::size_t polyline(std::size_t slot, std::array<JPH::RVec3, 4> &) const;
    std::size_t preview(std::array<JPH::RVec3, 4> &) const;
private:
    struct Slot {
        Record record;
        JPH::Ref<JPH::PulleyConstraint> constraint;
        float tension_n = 0;
        unsigned overload_ticks = 0;
        bool solved = false;
    };
    JPH::PhysicsSystem &world_;
    kit::Kit &kit_;
    std::array<Eye, kEyeCapacity> eyes_{};
    std::array<Slot, kCapacity> slots_{};
    std::size_t eye_count_ = 0;
    std::uint32_t epoch_ = 1, serial_ = 0;
    std::uint16_t first_ = 0, guide_ = 0;
    int selected_ = -1;
    bool equipped_ = false, allowed_ = false;
    std::uint64_t target_ = 0;
    JPH::RVec3 hand_ = JPH::RVec3::sZero();
    JPH::Vec3 aim_ = -JPH::Vec3::sAxisZ();
    JPH::BodyID player_;
    Failure failure_ = Failure::None;
    JPH::Body *body(const Eye &) const;
    JPH::RVec3 point(std::uint16_t eye, JPH::Vec3 local) const;
    bool clear_span(JPH::RVec3, JPH::RVec3, JPH::BodyID, JPH::BodyID) const;
    bool valid_record(const Record &) const;
    bool reachable(const Record &) const;
    void connect(Slot &);
    void disconnect(Slot &);
    std::uint64_t eye_token(std::size_t) const;
    std::uint64_t line_token(std::size_t) const;
    int line_index(std::uint64_t) const;
    bool reject(Failure);
};

} // namespace scraperx::sim
