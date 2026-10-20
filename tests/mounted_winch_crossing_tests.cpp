#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
namespace {
void report(const char *stage, const Simulation &sim) {
    const auto s = sim.snapshot(); const auto r = sim.rigging_state();
    const auto p = sim.kit_body_position(sim.kit_body_index(2499));
    const auto q = sim.kit_body_rotation(sim.kit_body_index(2499));
    std::cout << "MOUNTED_WINCH " << stage << " tick=" << s.tick_index
        << " player=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
        << " support=" << s.support_entity_id << " grounded=" << s.player_grounded
        << " traversal=" << unsigned(s.traversal_state) << " hands=" << s.traversal_hand_constraint_count
        << " beam_q=" << q.x << "," << q.y << "," << q.z << "," << q.w
        << " beam=" << p.x << ',' << p.y << ',' << p.z << " installed=" << r.installed_count
        << " target=" << r.target << " kind=" << r.target_kind << " failure=" << r.failure
        << " operable=" << r.power_operable << " brake=" << r.braking
        << " speed=" << r.target_speed_mps << " length=" << r.rest_length_m
        << " tension=" << r.tension_n << " energy=" << r.energy_j
        << " used=" << r.source_energy_used_j << '\n';
}
void require(bool value, const char *why, const Simulation &sim) {
    if (value) return;
    report(why, sim); std::cerr << "FAIL MOUNTED_WINCH_PUBLIC " << why << '\n'; std::exit(1);
}
void tick(Simulation &sim, unsigned count = 1) {
    while (count--) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "fixed tick accepted", sim);
        require(sim.snapshot().death_count == 0 && sim.snapshot().player_gravity_factor == 1,
            "ordinary same-life operation retains gravity", sim);
    }
}
void walk(Simulation &sim, double x, double z) {
    for (unsigned i = 0; i < 2400; ++i) {
        const auto s = sim.snapshot(); const auto p = s.player_position, v = s.player_linear_velocity;
        const double dx = x - p.x, dz = z - p.z;
        if (std::hypot(dx, dz) < .08 && s.player_grounded && std::hypot(v.x, v.z) < .17) {
            require(sim.set_move_input(0, 0), "neutral walking", sim); tick(sim, 8); return;
        }
        require(sim.set_move_input(std::clamp(1.3*dx-.45*v.x, -.55, .55),
            std::clamp(1.3*dz-.45*v.z, -.55, .55)), "ordinary movement input", sim);
        tick(sim);
    }
    require(false, "public walking reaches workstation", sim);
}
void command(Simulation &sim, RiggingCommandKind kind, std::uint64_t target = 0) {
    require(sim.request_rigging(kind, target), "ordered rigging command accepted", sim); tick(sim, 2);
}
void aim(Simulation &sim, Vector3 target) {
    const auto p = sim.snapshot().player_position;
    require(sim.set_rigging_aim(target.x-p.x, target.y-p.y-.62, target.z-p.z), "finite aim accepted", sim);
    tick(sim, 3);
}
Vector3 beam_eye(const Simulation &sim, double x = -1.8) {
    const auto body = sim.kit_body_index(2499);
    const auto p = sim.kit_body_position(body); const auto q = sim.kit_body_rotation(body);
    // Shape-origin eye, transformed by this ordinary body's actual rotation.
    constexpr double y = .17;
    return {p.x+(1-2*(q.y*q.y+q.z*q.z))*x+2*(q.x*q.y-q.z*q.w)*y,
        p.y+2*(q.x*q.y+q.z*q.w)*x+(1-2*(q.x*q.x+q.z*q.z))*y,
        p.z+2*(q.x*q.z-q.y*q.w)*x+2*(q.y*q.z+q.x*q.w)*y};
}
}

namespace {
void choose(Simulation &sim, double x, std::uint64_t expected) {
    aim(sim, {x, 1.08, -37.33});
    require(sim.rigging_state().target_line == expected, "aim identifies exact installed source", sim);
    command(sim, RiggingCommandKind::Select, sim.rigging_state().target_line);
    require(sim.rigging_state().selected == expected && sim.rigging_state().power_operable,
        "explicit source selection acknowledges correct cable", sim);
}
std::uint64_t install(Simulation &sim, double eye_x) {
    aim(sim, {-108+eye_x, 1.08, -37.33});
    require(sim.rigging_state().target_kind == 2 && sim.rigging_state().target != 0,
        "source offered before installation", sim);
    command(sim, RiggingCommandKind::Action, sim.rigging_state().target);
    aim(sim, beam_eye(sim,eye_x));
    command(sim, RiggingCommandKind::Action, sim.rigging_state().target);
    require(sim.rigging_state().failure == 0 && sim.rigging_state().power_operable,
        "two real clips install selected source", sim);
    return sim.rigging_state().selected;
}
void run_winch(Simulation &sim, double effort, unsigned ticks) {
    require(sim.set_rigging_power_input(effort), "held winch effort accepted", sim); tick(sim,ticks);
    require(sim.set_rigging_power_input(0), "release winch effort", sim); tick(sim,90);
    require(sim.rigging_state().braking && sim.rigging_state().target_speed_mps == 0,
        "neutral command engages finite brake", sim);
    report(effort > 0 ? "raised-end" : "lowered-end",sim);
}
void jump_to(Simulation &sim, double x, std::uint64_t receiver) {
    require(sim.request_jump(), "ordinary departure jump", sim);
    for (unsigned i = 0; i < 180; ++i) {
        const auto s = sim.snapshot(); const auto p=s.player_position,v=s.player_linear_velocity;
        require(sim.set_move_input(std::clamp(1.5*(x-p.x)-.35*v.x,-.8,.8),
            std::clamp(1.5*(-36-p.z)-.35*v.z,-.5,.5)), "ordinary airborne steering", sim);
        tick(sim);
        if (i > 8 && sim.snapshot().player_grounded && sim.snapshot().support_entity_id == receiver) {
            require(sim.set_move_input(0,0), "stop on actual receiver", sim);tick(sim,20);
            if(receiver==Simulation::kWorldSolidEntityId) {
                const auto at=sim.snapshot().player_position;
                require(at.x>-104.7 && at.x<-103.3 && at.z>-38.55 && at.z<-35.45 &&
                    std::abs(at.y-2.48)<.04 && sim.snapshot().player_grounded &&
                    sim.snapshot().support_entity_id==Simulation::kWorldSolidEntityId,
                    "actual footing on the specific existing receiving steel stack",sim);
            }
            return;
        }
    }
    require(false, "jump reaches actual requested receiver", sim);
}
Vector3 side_eye(const Simulation &sim,int side) {
    const auto body=sim.kit_body_index(2499);
    const auto p=sim.kit_body_position(body);const auto q=sim.kit_body_rotation(body);
    const double z=.5*side;
    return {p.x+2*(q.x*q.z+q.y*q.w)*z,p.y+2*(q.y*q.z-q.x*q.w)*z,
        p.z+(1-2*(q.x*q.x+q.y*q.y))*z};
}
void to_side(Simulation &sim,int side) {
    if(side<0) {walk(sim,-108,-40.6);walk(sim,-108,-37.9);return;}
    walk(sim,-108,-40.6);walk(sim,-101,-40.6);walk(sim,-101,-34.1);walk(sim,-108,-34.1);
}
void clip(Simulation &sim,Vector3 point,unsigned kind) {
    aim(sim,point);const auto r=sim.rigging_state();
    require(r.target && r.target_kind==kind,"actual marked eye/guide is offered",sim);
    command(sim,RiggingCommandKind::Action,r.target);
    require(sim.rigging_state().failure==0,"physical stay installation step succeeds",sim);
}
std::uint64_t install_stay(Simulation &sim,int side) {
    const double z=-36+1.9*side;
    walk(sim,-108+2.5*side,z);
    clip(sim,{-108+3.1*side,1.3,-36+1.2*side},0);
    walk(sim,-108,z);
    clip(sim,{-108,2.4,-36+1.2*side},1);
    clip(sim,side_eye(sim,side),0);
    require(sim.rigging_state().installed_count==(side<0?3U:4U),
        "opposed real passive stays augment the two powered lines",sim);
    return sim.rigging_state().selected;
}
void remove_stay(Simulation &sim,int side,std::uint64_t line) {
    aim(sim,side_eye(sim,side));
    require(sim.rigging_state().target_line==line,"side lug offers exact installed stay",sim);
    command(sim,RiggingCommandKind::Select,line);
    for(unsigned i=0;i<180 && sim.rigging_state().tension_n>60;++i)tick(sim);
    require(sim.rigging_state().tension_n<=60,"real powered lift unloads stay before removal",sim);
    report(side<0?"front-stay-unloaded":"rear-stay-unloaded",sim);
    const auto before=sim.rigging_state().installed_count;
    command(sim,RiggingCommandKind::Unclip,line);
    require(sim.rigging_state().installed_count+1==before,"unloaded passive stay removes safely",sim);
}
void walk_beam(Simulation &sim) {
    const auto body=sim.kit_body_index(2499);
    Vector3 previous{};
    for(unsigned i=0;i<1200;++i) {
        const auto p=sim.kit_body_position(body);const auto q=sim.kit_body_rotation(body);
        const Vector3 along{1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w)};
        const Vector3 target{p.x+3*along.x,p.y+3*along.y,p.z+3*along.z};
        const auto s=sim.snapshot();const auto at=s.player_position,v=s.player_linear_velocity;
        const double dx=target.x-at.x,dz=target.z-at.z;
        const double progress=(at.x-p.x)*along.x+(at.y-p.y)*along.y+(at.z-p.z)*along.z;
        // Depart before the visible end-eye pad, preserving ordinary walking
        // momentum into the jump instead of asking for a stop on the hardware.
        if(progress>1.35 && s.player_grounded && s.support_entity_id==2499) return;
        // The player steers along the visibly moving beam, rather than a
        // fixed world-Z path that stops matching this free suspension.
        const double vx=i?(target.x-previous.x)/Simulation::kFixedStepSeconds:0;
        const double vz=i?(target.z-previous.z)/Simulation::kFixedStepSeconds:0;
        require(sim.set_facing(along.x,along.z),"look along actual crossing",sim);
        require(sim.set_move_input(std::clamp(1.3*dx-.45*(v.x-vx),-.55,.55),
            std::clamp(1.3*dz-.45*(v.z-vz),-.55,.55)),"steer along real beam centreline",sim);
        previous=target;tick(sim);
    }
    require(false,"walking reaches the moving beam end",sim);
}
}
int main() {
    std::cout << std::unitbuf;
    Simulation sim(InitialSpawn::FreightYardArrival); tick(sim,90);
    walk(sim,-80,-43.5);walk(sim,-100,-43.5);walk(sim,-108,-40.5);
    walk(sim,-109.1,-38.2);walk(sim,-109.1,-36.85);
    command(sim,RiggingCommandKind::Equip);
    auto left=install(sim,-1.8);
    walk(sim,-109.1,-38.2);walk(sim,-106.9,-38.2);walk(sim,-106.9,-36.85);
    auto right=install(sim,1.8);
    require(left != right && sim.rigging_state().installed_count == 2,
        "two independent real cable installations cooperate",sim);
    run_winch(sim,1,333);
    walk(sim,-106.9,-38.2);walk(sim,-109.1,-38.2);walk(sim,-109.1,-36.85);
    choose(sim,-109.8,left);run_winch(sim,1,333);
    const double left_source_used=sim.rigging_state().source_energy_used_j;
    const auto l=beam_eye(sim),r=beam_eye(sim,1.8);
    require(std::min(l.y,r.y)>1.05 && std::abs(l.y-r.y)<.1,
        "both beam ends lift clear of grade without hidden support",sim);
    walk(sim,-109.1,-38.2);to_side(sim,-1);
    auto front=install_stay(sim,-1);
    to_side(sim,1);auto rear=install_stay(sim,1);
    command(sim,RiggingCommandKind::Unequip);
    walk(sim,-101,-34.1);walk(sim,-101,-40.6);walk(sim,-112.5,-40.6);walk(sim,-112.5,-36);
    require(sim.snapshot().support_entity_id == 1498 && sim.snapshot().player_position.y>2,
        "ordinary stair approach reaches separate entry landing",sim);
    const auto checkpoint=sim.snapshot();
    const auto held=sim.kit_body_position(sim.kit_body_index(2499));
    require(checkpoint.checkpoint_footing_valid && sim.restart_checkpoint(),
        "actual firm entry footing creates usable rig checkpoint",sim);
    tick(sim,30);
    require(sim.rigging_state().installed_count==4 &&
        std::abs(sim.kit_body_position(sim.kit_body_index(2499)).y-held.y)<.03 &&
        // Cold restore selects the first occupied line, not the last passive stay.
        std::abs(sim.rigging_state().source_energy_used_j-left_source_used)<1.e-6 &&
        !sim.rigging_state().equipped && sim.rigging_state().target_speed_mps==0,
        "ordinary checkpoint restores both installed cables, held load and finite source",sim);
    left=sim.rigging_state().lines[0];right=sim.rigging_state().lines[1];
    front=sim.rigging_state().lines[2];rear=sim.rigging_state().lines[3];
    walk(sim,-111.65,-36);jump_to(sim,-109.7,2499);
    unsigned supported=0;
    for (unsigned i=0;i<180;++i) {
        tick(sim);
        if(sim.snapshot().player_grounded && sim.snapshot().support_entity_id==2499)++supported;
    }
    require(supported>150 && sim.kit_body_position(sim.kit_body_index(2499)).y>.9,
        "two brake-held lines carry actual player mass on suspended beam",sim);
    report("before-beam-walk",sim);
    walk_beam(sim);
    require(sim.snapshot().support_entity_id==2499,"walkable free beam carries onward movement",sim);
    jump_to(sim,-104.0,Simulation::kWorldSolidEntityId);walk(sim,-103.5,-36);walk(sim,-103.5,-40.6);
    require(sim.rigging_state().installed_count==4,"dismount preserves all four real support lines",sim);
    walk(sim,-106.9,-38.2);walk(sim,-106.9,-36.85);
    command(sim,RiggingCommandKind::Equip);choose(sim,-106.2,right);run_winch(sim,1,36);
    walk(sim,-106.9,-38.2);walk(sim,-109.1,-38.2);walk(sim,-109.1,-36.85);
    choose(sim,-109.8,left);run_winch(sim,1,36);
    walk(sim,-109.1,-38.2);to_side(sim,-1);remove_stay(sim,-1,front);
    to_side(sim,1);remove_stay(sim,1,rear);
    walk(sim,-101,-34.1);walk(sim,-101,-40.6);walk(sim,-106.9,-40.6);walk(sim,-106.9,-38.2);walk(sim,-106.9,-36.85);
    choose(sim,-106.2,right);run_winch(sim,-1,369);
    walk(sim,-106.9,-38.2);walk(sim,-109.1,-38.2);walk(sim,-109.1,-36.85);
    choose(sim,-109.8,left);run_winch(sim,-1,369);
    require(sim.kit_body_position(sim.kit_body_index(2499)).y<.23,
        "unloaded crossing lowers to real grade for reconfiguration",sim);
    const double recovered_left_used=sim.rigging_state().source_energy_used_j;
    command(sim,RiggingCommandKind::Unclip,left);
    require(sim.rigging_state().installed_count==1,"unloaded left cable safely removed",sim);
    walk(sim,-109.1,-38.2);walk(sim,-106.9,-38.2);walk(sim,-106.9,-36.85);
    choose(sim,-106.2,right);
    const double recovered_right_used=sim.rigging_state().source_energy_used_j;
    command(sim,RiggingCommandKind::Unclip,right);
    require(sim.rigging_state().installed_count==0,"unloaded right cable safely removed",sim);
    command(sim,RiggingCommandKind::Unequip);walk(sim,-106.9,-40.6);
    // Reuse the recovered inventory and the same paid devices for a real
    // inclined crossing. Unequal commanded lengths change the structure;
    // no body pose, exit velocity or force limit is supplied by this route.
    walk(sim,-109.1,-38.2);walk(sim,-109.1,-36.85);
    command(sim,RiggingCommandKind::Equip);
    const auto reused_left=install(sim,-1.8);
    require(reused_left!=left && std::abs(sim.rigging_state().source_energy_used_j-recovered_left_used)<1.e-6,
        "recovered left kit gets a fresh line token without refilling its device",sim);
    walk(sim,-109.1,-38.2);walk(sim,-106.9,-38.2);walk(sim,-106.9,-36.85);
    const auto reused_right=install(sim,1.8);
    require(reused_right!=right && std::abs(sim.rigging_state().source_energy_used_j-recovered_right_used)<1.e-6,
        "recovered right kit retains its independently paid source",sim);
    run_winch(sim,1,405);
    walk(sim,-106.9,-38.2);walk(sim,-109.1,-38.2);walk(sim,-109.1,-36.85);
    choose(sim,-109.8,reused_left);run_winch(sim,1,261);
    const auto inclined_left=beam_eye(sim),inclined_right=beam_eye(sim,1.8);
    require(inclined_right.y-inclined_left.y>.25 && inclined_left.y>.8,
        "different finite end lengths build a suspended incline rather than the old level layout",sim);
    walk(sim,-109.1,-38.2);to_side(sim,-1);install_stay(sim,-1);
    to_side(sim,1);install_stay(sim,1);
    command(sim,RiggingCommandKind::Unequip);
    walk(sim,-101,-34.1);walk(sim,-101,-40.6);walk(sim,-112.5,-40.6);walk(sim,-112.5,-36);
    require(sim.snapshot().support_entity_id==1498,"ordinary stairs reach the reconfigured route",sim);
    walk(sim,-111.65,-36);jump_to(sim,-109.7,2499);
    unsigned inclined_supported=0;
    for(unsigned i=0;i<90;++i) {
        tick(sim);
        if(sim.snapshot().player_grounded && sim.snapshot().support_entity_id==2499)++inclined_supported;
    }
    require(inclined_supported>60,"reused kit supports the real player on the inclined structure",sim);
    walk_beam(sim);jump_to(sim,-104,Simulation::kWorldSolidEntityId);
    walk(sim,-103.5,-36);walk(sim,-103.5,-40.6);
    require(sim.rigging_state().installed_count==4 && !sim.rigging_state().equipped,
        "reconfigured world crossing remains installed after onward movement",sim);
    report("reused-inclined-crossing",sim);
    std::cout<<"PASS MOUNTED_WINCH_CROSSING arrival=1 powered=2 side_stays=2 suspended_player_ticks="<<supported
        <<" receiver=1 checkpoint_recovery=1 actual_stay_unload=2 lower=1 unclip=4 reconfigured_crossings=2 source_reuse=2 onward=1 deaths=0\n";
}
