#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace scraperx::sim;
namespace {
constexpr std::uint64_t platform = 2493, weight = 2494, lever = 2495, receiver = 1495;
unsigned platform_ticks = 0, receiver_ticks = 0;
void report(const char *stage, const Simulation &sim) {
    const auto s = sim.snapshot(); const auto r = sim.rigging_state();
    const auto p = sim.kit_body_position(sim.kit_body_index(platform));
    const auto w = sim.kit_body_position(sim.kit_body_index(weight));
    const auto l = sim.kit_body_rotation(sim.kit_body_index(lever));
    if (std::string_view(stage) == "occupied-lift") {
        for (unsigned i = 0; i < 3; ++i) {
            const auto body = sim.kit_body_index(2496 + i);
            const auto at = sim.kit_body_position(body);
            std::cout << "RIGGING_LOADED entity=" << 2496+i << " position="
                << at.x << ',' << at.y << ',' << at.z << " tray_dy=" << at.y-w.y << '\n';
        }
    }
    if (s.carrying_entity_id >= 2496 && s.carrying_entity_id <= 2498) {
        const auto body = sim.kit_body_index(s.carrying_entity_id);
        const auto at = sim.kit_body_position(body), velocity = sim.kit_body_velocity(body);
        std::cout << "RIGGING_BALLAST entity=" << s.carrying_entity_id
            << " position=" << at.x << ',' << at.y << ',' << at.z
            << " velocity=" << velocity.x << ',' << velocity.y << ',' << velocity.z << '\n';
    }
    std::cout << "RIGGING_PUBLIC " << stage << " tick=" << s.tick_index
        << " player=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
        << " velocity=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
        << " support=" << s.support_entity_id << " grounded=" << s.player_grounded
        << " carrying=" << s.carrying_entity_id << " pickup_target=" << s.carry_target_entity_id
        << " equipped=" << r.equipped << " suspended=" << r.suspended << " draft=" << r.draft
        << " guide=" << r.guide_threaded << " target=" << r.target << " failure=" << r.failure
        << " installed=" << r.installed_count << " tension_n=" << r.tension_n
        << " platform=" << p.x << ',' << p.y << ',' << p.z
        << " weight=" << w.x << ',' << w.y << ',' << w.z
        << " lever_q=" << l.x << ',' << l.y << ',' << l.z << ',' << l.w << '\n';
}
void require(bool condition, const char *why, const Simulation &sim) {
    if (condition) return;
    report(why, sim); std::cerr << "FAIL RIGGING_PUBLIC " << why << '\n'; std::exit(1);
}
void tick(Simulation &sim, unsigned n = 1) {
    while (n--) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick", sim);
        const auto s = sim.snapshot();
        require(s.death_count == 0, "ordinary route survives without restart", sim);
        if (s.player_grounded && s.support_entity_id == platform) ++platform_ticks;
        if (s.player_grounded && s.support_entity_id == receiver) ++receiver_ticks;
    }
}
void walk(Simulation &sim, double x, double z, double cap = .55, std::uint64_t carried = 0, bool stair_jumps = false) {
    auto last_progress = sim.snapshot().player_position;
    for (unsigned i = 0; i < 2400; ++i) {
        const auto s = sim.snapshot();
        require(s.carrying_entity_id == carried, "walking retains requested carried body", sim);
        const auto p = s.player_position, v = s.player_linear_velocity;
        const double dx = x - p.x, dz = z - p.z;
        if (std::hypot(dx,dz) < .07 && s.player_grounded && std::hypot(v.x,v.z) < .16) {
            require(sim.set_move_input(0,0), "stop walking", sim); tick(sim,8); report("waypoint",sim); return;
        }
        require(sim.set_move_input(std::clamp(1.3*dx-.45*v.x,-cap,cap), std::clamp(1.3*dz-.45*v.z,-cap,cap)), "ordinary walking input", sim);
        if (stair_jumps && i && i % 90 == 0) {
            if (std::hypot(p.x-last_progress.x,p.z-last_progress.z) < .04 && s.player_grounded) {
                report("carried-stair-jump",sim);
                require(sim.request_jump(),"ordinary loaded jump clears service riser",sim);
            }
            last_progress = p;
        }
        tick(sim);
        if (i && i % 600 == 0) report("walking-progress",sim);
    }
    require(false,"bounded walking reaches waypoint",sim);
}
void command(Simulation &sim, RiggingCommandKind kind, std::uint64_t target = 0) {
    require(sim.request_rigging(kind,target),"public rigging command accepted",sim); tick(sim,2);
}
void aim_action(Simulation &sim, unsigned eye, Vector3 point) {
    const auto p = sim.snapshot().player_position;
    require(sim.set_rigging_aim(point.x-p.x,point.y-p.y-.62,point.z-p.z),"public aim accepts finite direction",sim);
    tick(sim,3); const auto target = sim.rigging_state().target;
    require(target && unsigned(target & 0xffffffffU) == eye,"actual eye reachable through public aim",sim);
    command(sim,RiggingCommandKind::Action,target);
    require(sim.rigging_state().failure == 0,"authored eye action completes",sim);
}
void service(Simulation &sim, std::uint64_t carried = 0) {
    walk(sim,-114.8,-28.2,.45,carried); walk(sim,-117.5,-28.2,.4,carried);
    walk(sim,-117.5,-34.2,.3,carried,carried != 0);
}
void install(Simulation &sim) {
    command(sim,RiggingCommandKind::Equip);
    walk(sim,-123.5,-34.2);
    const auto p = sim.kit_body_position(sim.kit_body_index(platform));
    aim_action(sim,1,{p.x-1,p.y+.48,p.z+.9});
    walk(sim,-123.5,-32.8); walk(sim,-123.5,-28.2); service(sim);
    aim_action(sim,3,{-116.56,4.3,-35.1});
    const auto w = sim.kit_body_position(sim.kit_body_index(weight));
    aim_action(sim,2,{w.x+.94,w.y+.7,w.z+.9});
    require(sim.rigging_state().installed_count == 1,"three ordinary eye actions install one line",sim);
    command(sim,RiggingCommandKind::Unequip);
    require(sim.rigging_state().installed_count == 1,"stowing preserves installed line",sim);
}
void board(Simulation &sim) {
    walk(sim,-117.5,-28.2); walk(sim,-125.8,-28.2);
    walk(sim,-125.8,-39); walk(sim,-122.5,-39); walk(sim,-123.4,-37.8,.25);
    require(sim.request_jump(),"ordinary jump onto low platform",sim); walk(sim,-123.4,-36.5,.25);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == platform,"ordinary boarding has actual platform footing",sim);
}
void pull_lever(Simulation &sim, bool release) {
    const auto grip = sim.kit_carry_grip_position(sim.kit_body_index(lever));
    const auto at = sim.snapshot().player_position;
    require(sim.set_facing(grip.x-at.x,grip.z-at.z),"face actual lever grip",sim); tick(sim,8);
    require(sim.snapshot().carry_target_entity_id == lever && sim.request_pick_up(),"actual lever grip within ordinary reach",sim);
    tick(sim,8);
    require(sim.snapshot().carrying_entity_id == lever,"actual carry constraint grips lever",sim);
    if (!release) {
        require(sim.set_crouch_input(true),"ordinary crouch frees vertical lever-return movement",sim);
        tick(sim,2);
    }
    require(sim.set_move_input(release ? .3 : -.3,0),"pull real hinged lever",sim);
    bool reached = false;
    for(unsigned i=0;i<180;++i) {
        tick(sim);
        require(sim.snapshot().carrying_entity_id == lever,"real lever grip persists throughout deliberate pull",sim);
        const auto q=sim.kit_body_rotation(sim.kit_body_index(lever));
        const double angle=2*std::atan2(q.z,q.w);
        if(release ? angle>.7 : angle<.025) { reached=true; break; }
    }
    require(reached,"ordinary grip moves lever through catch threshold",sim);
    require(sim.set_move_input(0,0),"stop lever pull",sim);
    require(sim.request_set_down(),"let go of lever",sim); tick(sim,30);
    if (!release) {
        require(sim.set_crouch_input(false),"restore ordinary standing after lever return",sim);
        tick(sim,15);
    }
    report(release ? "lever-released" : "lever-returned",sim);
}
void place_ballast(Simulation &sim, unsigned i) {
    const auto entity = std::uint64_t(2496+i);
    const auto at = sim.kit_body_position(sim.kit_body_index(entity));
    walk(sim,-114.8,-28.2); walk(sim,at.x,at.z+.85,.35);
    require(sim.set_facing(0,-1),"face actual ballast",sim); tick(sim,10);
    require(sim.snapshot().carry_target_entity_id == entity && sim.request_pick_up(),"pick up authored25kg ballast",sim);
    tick(sim,30); require(sim.snapshot().carrying_entity_id == entity,"actual ballast carry acquired",sim);
    service(sim,entity);
    const double x = -117.5 + (i == 0 ? -.52 : i == 1 ? .52 : 0);
    walk(sim,x,-34.2,.18,entity);
    require(sim.set_facing(0,-1),"face open tray front",sim); tick(sim,45);
    walk(sim,x,-34.95,.12,entity);
    require(sim.request_set_down(),"release ballast into actual tray",sim); tick(sim,180);
    const auto b = sim.kit_body_position(sim.kit_body_index(entity));
    const auto w = sim.kit_body_position(sim.kit_body_index(weight));
    require(std::abs(b.x-w.x)<.8 && std::abs(b.z-w.z)<.65 && b.y>w.y+.2 && b.y<w.y+.4,
        "released authored ballast rests in counterweight tray",sim);
    walk(sim,-117.5,-34.2,.2); walk(sim,-117.5,-28.2,.3);
}
}
int main() {
    std::cout << std::unitbuf;
    Simulation sim(InitialSpawn::FreightYardArrival); tick(sim,90); report("arrival",sim);
    walk(sim,-80,-43.5); walk(sim,-100,-43.5); walk(sim,-120,-43.5);
    walk(sim,-125.8,-43.5); walk(sim,-125.8,-32.8); walk(sim,-123.5,-32.8);
    install(sim); board(sim); pull_lever(sim,true); tick(sim,360);
    require(sim.kit_body_position(sim.kit_body_index(platform)).y < .35 && sim.snapshot().support_entity_id == platform,
        "base counterweight cannot lift actual occupied platform",sim);
    walk(sim,-123.1,-36.5,.2);
    pull_lever(sim,false);
    walk(sim,-123.3,-35.5,.2);
    command(sim,RiggingCommandKind::Equip);
    const auto line = sim.rigging_state().selected;
    require(line != 0,"installed line retained for retry",sim);
    command(sim,RiggingCommandKind::Unclip,line);
    require(sim.rigging_state().installed_count == 0,"unloaded reseated line unclips without restart",sim);
    command(sim,RiggingCommandKind::Unequip);
    walk(sim,-122.5,-39); walk(sim,-125.8,-39); walk(sim,-125.8,-28.2); walk(sim,-114.8,-28.2);
    for(unsigned i=0;i<3;++i) place_ballast(sim,i);
    walk(sim,-125.8,-28.2); walk(sim,-123.5,-32.8); install(sim); board(sim); pull_lever(sim,true);
    tick(sim,900); report("occupied-lift",sim);
    require(sim.kit_body_position(sim.kit_body_index(platform)).y > 2.25 && sim.rigging_state().tension_n > 60 &&
        sim.snapshot().support_entity_id == platform,"actual ballasted rig lifts occupied platform to receiver",sim);
    for (unsigned i = 0; i < 3; ++i) {
        const auto ballast = sim.kit_body_position(sim.kit_body_index(2496+i));
        const auto tray = sim.kit_body_position(sim.kit_body_index(weight));
        require(std::abs(ballast.x-tray.x)<.8 && std::abs(ballast.z-tray.z)<.65 &&
                    ballast.y>tray.y+.2 && ballast.y<tray.y+.5,
                "all three actual ballast bodies descend with tray during occupied lift",sim);
    }
    tick(sim,180);
    require(sim.kit_body_position(sim.kit_body_index(platform)).y > 2.25 &&
                sim.snapshot().support_entity_id == platform,
            "ballasted occupied platform holds receiving height before onward movement",sim);
    walk(sim,-121.6,-36,.2); walk(sim,-120.5,-36,.2); walk(sim,-120.5,-38,.2);
    require(receiver_ticks >= 30 && sim.snapshot().support_entity_id == receiver,"receiving deck supplies actual onward footing",sim);
    walk(sim,-120.5,-39.5,.3); tick(sim,90);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,"ordinary grade recovery completes route",sim);
    std::cout<<"PASS RIGGING_PUBLIC arrival=1 ballast=3 actual_platform_ticks="<<platform_ticks<<" receiver_ticks="<<receiver_ticks<<" retries=1 resets=0 deaths=0\n";
}
