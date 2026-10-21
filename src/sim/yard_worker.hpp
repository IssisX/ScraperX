#pragma once

#include "sim/physical_story_actor.hpp"

#ifdef SCRAPERX_HAS_JOLT
namespace scraperx::sim {

// Intent and lifecycle for one yard worker. PhysicalStoryActor retains the
// complete force/contact law; this owner never sets body velocity or pose.
class YardWorker final {
public:
    enum class Activity : std::uint8_t {
        Working, Attending, Pointing, Returning, Yielding, Recovering
    };
    struct State {
        Activity activity = Activity::Working;
        double seconds = 0, heading = 0;
        bool greeted = false;
        JPH::Vec3 aside = JPH::Vec3::sZero();
    };
    struct View {
        PhysicalStoryActor::Readback body{};
        Activity activity = Activity::Working;
        double seconds = 0, heading = 0;
        std::uint64_t tick = 0, episode = 1;
        bool supported = false;
        std::uint64_t support_entity = 0;
        JPH::RVec3 work_point = JPH::RVec3::sZero();
        JPH::RVec3 guide_point = JPH::RVec3::sZero();
        JPH::RVec3 look_target = JPH::RVec3::sZero();
    };
    YardWorker(JPH::PhysicsSystem &, JPH::ObjectLayer, std::uint64_t entity);
    [[nodiscard]] bool spawn();
    [[nodiscard]] JPH::BodyID body_id() const noexcept { return actor_.body_id(); }
    void pre_step(float dt, JPH::RVec3 player, JPH::Vec3 player_velocity, bool visible);
    void finish_step(const PhysicalStoryActor::Contact *contact);
    [[nodiscard]] View view() const;
    [[nodiscard]] State capture() const noexcept { return state_; }
    void restore(const State &);

    // East of the loaded six-metre beam's swept operating lane. The bench
    // and seam derive from this single authored station, never from a pose.
    static JPH::RVec3 home() { return JPH::RVec3(-89, .9, -50); }
    static JPH::RVec3 work_point() { return home() + JPH::Vec3(0, -.06F, -.68F); }
    static JPH::RVec3 guide_point() { return JPH::RVec3(-94, 1.3, -75); }

private:
    void enter(Activity activity) noexcept;
    JPH::PhysicsSystem &physics_;
    PhysicalStoryActor actor_;
    State state_{};
    std::uint64_t tick_ = 0, episode_ = 1;
    bool supported_ = false;
    std::uint64_t support_entity_ = 0;
    JPH::RVec3 look_target_ = work_point();
};
} // namespace scraperx::sim
#endif
