#include "sim/simulation.hpp"
#include "sim/grapple_practice_route.hpp"
#include "sim/grappling_gun.hpp"
#include "grappling_retention_observation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using namespace scraperx::sim;
constexpr auto kDeparture = kGrapplePracticeDepartureEntity;
constexpr auto kReceiver = kGrapplePracticeReceiverEntity;
constexpr auto kRecovery = kGrapplePracticeRecoveryEntity;
const char *phase = "roof484";
const double departure_x = kGrapplePracticeDeparture.GetX();
const double receiver_x = kGrapplePracticeReceiver.GetX();

void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    const auto g = s.grapple_gun_state();
    const auto cup = s.kit_body_index(kGrapplePracticeCupEntity);
    const auto cp = s.kit_body_position(cup);
    const auto cq = s.kit_body_rotation(cup);
    std::cerr << "[DEBUG-grapple-pose] " << phase << ',' << event << " cup="
              << cp.x << ',' << cp.y << ',' << cp.z << " q="
              << cq.x << ',' << cq.y << ',' << cq.z << ',' << cq.w << '\n';
    for (const auto &h : g.hooks) if (h.present && h.body == g.active_hook) {
        const JPH::Quat qcup(float(cq.x), float(cq.y), float(cq.z), float(cq.w));
        const JPH::Quat qhook(float(h.rotation.x), float(h.rotation.y), float(h.rotation.z), float(h.rotation.w));
        const auto head = qhook * JPH::Vec3(0, .10F, 0);
        const auto local = qcup.Conjugated() * JPH::Vec3(
            float(h.position.x-cp.x)+head.GetX(), float(h.position.y-cp.y)+head.GetY(),
            float(h.position.z-cp.z)+head.GetZ());
        std::cerr << "[DEBUG-grapple-head] local=" << local.GetX() << ',' << local.GetY() << ',' << local.GetZ() << '\n';
    }
    std::cout << "GRAPPLE_ROUTE," << phase << ',' << event << ',' << v.tick_index << ','
              << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z << ','
              << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ','
              << v.player_linear_velocity.z << ',' << v.player_grounded << ',' << v.support_entity_id << ','
              << v.death_count << ',' << g.state << ',' << g.failure << ',' << g.cartridges << ','
              << g.hook_endpoint.x << ',' << g.hook_endpoint.y << ',' << g.hook_endpoint.z << ','
              << g.line_length_m << ',' << g.tension_n << ',' << g.reel_work_j << '\n';
}
void require(bool ok, const char *reason, const Simulation &s) {
    if (ok) return;
    sample(s, "failure");
    std::cerr << "FAIL GRAPPLE_ROUTE phase=" << phase << " reason=" << reason << '\n';
    std::exit(EXIT_FAILURE);
}
void begin(Simulation &s, const char *next) { phase = next; sample(s, "begin"); }
void tick(Simulation &s, int count = 1, bool allow_parachute = false) {
    while (count-- > 0) {
        const auto r = s.advance_frame(Simulation::kFixedStepSeconds);
        const auto v = s.snapshot();
        require(r.accepted && r.steps_advanced == 1, "one native90Hz step", s);
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                std::isfinite(v.player_position.z) && std::isfinite(v.player_linear_velocity.x) &&
                std::isfinite(v.player_linear_velocity.y) && std::isfinite(v.player_linear_velocity.z),
                "finite native player motion", s);
        require(v.death_count == 0 && v.player_gravity_factor == 1 && (allow_parachute || !v.parachute_deployed) &&
                v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
                "same life ordinary gravity and no hand-assisted crossing", s);
    }
}
bool footing(const Snapshot &v, std::uint64_t support, double y) {
    return v.player_grounded && v.support_entity_id == support && !v.player_crouched &&
           std::abs(v.player_position.y-y) < .15;
}
void steer(Simulation &s, double x, double z, double ex = 0, double ez = 0) {
    const auto v = s.snapshot();
    const double dx = x-v.player_position.x, dz = z-v.player_position.z;
    const double ix = 1.8*dx-.28*v.player_linear_velocity.x+ex;
    const double iz = 1.8*dz-.28*v.player_linear_velocity.z+ez;
    const double scale = std::max(1.0, std::hypot(ix, iz));
    require(s.set_move_input(ix/scale, iz/scale), "ordinary stick input", s);
    if (std::hypot(dx, dz) > .02) require(s.set_facing(dx, dz), "ordinary facing", s);
}
JPH::Quat quat(const Quaternion &q) { return {float(q.x), float(q.y), float(q.z), float(q.w)}; }
Vector3 body_point(const Simulation &s, std::uint64_t entity, JPH::Vec3 local) {
    const auto body=s.kit_body_index(entity);
    const auto p=s.kit_body_position(body);
    const auto offset=quat(s.kit_body_rotation(body))*local;
    return {p.x+offset.GetX(),p.y+offset.GetY(),p.z+offset.GetZ()};
}
JPH::Vec3 body_local_point(const Simulation &s, std::uint64_t entity, Vector3 world) {
    const auto body=s.kit_body_index(entity); const auto p=s.kit_body_position(body);
    return quat(s.kit_body_rotation(body)).Conjugated()*JPH::Vec3(
        float(world.x-p.x),float(world.y-p.y),float(world.z-p.z));
}
void rack_follows_departure(const Simulation &s) {
    const auto expected=body_point(s,kDeparture,{-.7F,.9F,0});
    const auto actual=s.grapple_gun_state().rack_position;
    require(std::sqrt(std::pow(actual.x-expected.x,2)+std::pow(actual.y-expected.y,2)+
                      std::pow(actual.z-expected.z,2))<.002,
            "native rack follows actual departure body pose",s);
}
void walk(Simulation &s, double x, double z, const char *next, int budget = 1800,
          std::uint64_t target_body = 0, JPH::Vec3 target_local = JPH::Vec3::sZero()) {
    begin(s, next);
    double ex = 0, ez = 0;
    for (int i = 0; i < budget; ++i) {
        const auto v = s.snapshot();
        if (target_body!=0) {
            const auto target=body_point(s,target_body,target_local); x=target.x; z=target.z;
        }
        const double dx = x-v.player_position.x, dz = z-v.player_position.z;
        // The final 300mm tread leaves 50mm around native 100mm footing probes.
        // Brake relative to that real moving support before the neutral dwell.
        const bool final_tread=std::string(next)=="ordinary_side_stair_ascent";
        const bool launch_alignment=(std::string(next)=="retry_launch_point" || std::string(next)=="portable_rail_launch");
        const bool precise=final_tread || launch_alignment;
        const double speed=precise ? std::hypot(
            v.player_linear_velocity.x-v.support_point_linear_velocity.x,
            v.player_linear_velocity.z-v.support_point_linear_velocity.z) :
            std::hypot(v.player_linear_velocity.x,v.player_linear_velocity.z);
        if (v.player_grounded && std::hypot(dx, dz) < (precise?.015:.10) &&
            speed < (precise?.03:.14)) {
            require(s.set_move_input(0, 0), "arrival releases stick", s);
            sample(s, "arrived"); return;
        }
        if (std::hypot(dx, dz) > .75) ex = ez = 0;
        else {
            ex += dx*Simulation::kFixedStepSeconds*.4;
            ez += dz*Simulation::kFixedStepSeconds*.4;
            const double scale = std::max(1.0, std::hypot(ex, ez)/.35);
            ex /= scale; ez /= scale;
        }
        steer(s, x, z, ex, ez); tick(s);
    }
    require(false, "bounded ordinary walk arrival", s);
}
void walk_body(Simulation &s, std::uint64_t body, JPH::Vec3 local, const char *next,
               int budget = 1800) {
    walk(s,0,0,next,budget,body,local);
}
void stable(Simulation &s, std::uint64_t support, double y) {
    require(s.set_move_input(0, 0), "neutral footing input", s);
    for (int i = 0; i < 90; ++i) {
        tick(s); require(footing(s.snapshot(), support, y), "actual stable support", s);
    }
    require(s.snapshot().checkpoint_footing_valid, "native resting support patch", s);
    sample(s, "stable");
}
void stable_actual_support(Simulation &s, std::uint64_t support) {
    require(s.set_move_input(0,0), "neutral actual support input", s);
    for (int i=0;i<90;++i) {
        tick(s);
        const auto v=s.snapshot();
        require(v.player_grounded && v.support_entity_id==support && !v.player_crouched &&
                v.checkpoint_footing_valid, "sustained actual support/resting patch", s);
    }
    sample(s,"actual_support_stable");
}
// BODY POSITION plus rotated crook aperture center, transformed through the actual cup pose.
// Neither overlap alone nor a semantic flag establishes collision retention.
bool seated_head(const Simulation &s, JPH::Vec3 &local) {
    const auto g = s.grapple_gun_state();
    const auto cup = s.kit_body_index(kGrapplePracticeCupEntity);
    const auto p = s.kit_body_position(cup);
    const auto q = quat(s.kit_body_rotation(cup));
    for (const auto &h : g.hooks) if (h.present && h.body == g.active_hook) {
        const auto head = quat(h.rotation)*JPH::Vec3(0, .10F, 0);
        local = q.Conjugated()*JPH::Vec3(float(h.position.x-p.x)+head.GetX(),
            float(h.position.y-p.y)+head.GetY(), float(h.position.z-p.z)+head.GetZ());
        const auto hq=quat(h.rotation);
        const auto bar_world=JPH::RVec3(p.x,p.y,p.z)+JPH::RVec3(q*kGrapplePracticeBarCenterLocal);
        const auto bar=hq.Conjugated()*JPH::Vec3(float(bar_world.GetX()-h.position.x),
            float(bar_world.GetY()-h.position.y),float(bar_world.GetZ()-h.position.z));
        const auto axis=hq.Conjugated()*(q*JPH::Vec3::sAxisZ());
        return scraperx::test::retained_bar_in_crook(bar,axis,kGrapplePracticeBarRadius,
            .115F,.103F,JPH::PhysicsSettings{}.mPenetrationSlop);
    }
    return false;
}
// Test observation only: differentiate actual relative orientation every native
// tick. Bounded labeled output captures contact spin without changing physics.
struct ImpactSpinTrace {
    JPH::Quat previous=JPH::Quat::sIdentity();
    bool initialized=false;
    float peak=0;
    void observe(const Simulation &s, const char *event, int i) {
        const auto g=s.grapple_gun_state();
        const auto cup=s.kit_body_index(kGrapplePracticeCupEntity);
        const auto cq=quat(s.kit_body_rotation(cup));
        for (const auto &h:g.hooks) if (h.present && h.body==g.active_hook) {
            const auto relative=cq.Conjugated()*quat(h.rotation);
            auto delta=relative*previous.Conjugated();
            if (delta.GetW()<0) delta=-delta;
            const float speed=initialized ? 2*std::atan2(delta.GetXYZ().Length(),delta.GetW())/
                float(Simulation::kFixedStepSeconds) : 0;
            peak=std::max(peak,speed); previous=relative; initialized=true;
            if (i>=180 && (i%10==0 || speed>10)) {
                const auto down=relative*JPH::Vec3(0,-1,0);
                std::cerr << "[DEBUG-grapple-spin] " << event << " tick=" << s.snapshot().tick_index
                          << " relative_rad_s=" << speed << " peak=" << peak
                          << " neck_axis=" << down.GetX() << ',' << down.GetY() << ',' << down.GetZ() << '\n';
            }
        }
    }
};
void fire_into_cup(Simulation &s) {
    begin(s, "ordinary_aim_fire");
    const auto before = s.grapple_gun_state();
    const auto v = s.snapshot();
    // Ideal high arc includes barrel offset and reciprocal85kg recoil.
    // This chooses ordinary aim only; real collision/contact decides retention.
    const double speed = std::sqrt(2*GrapplingGun::cartridge_energy_j /
        (1.0/85.0+1.0/GrapplingGun::hook_mass_kg))/GrapplingGun::hook_mass_kg;
    const auto p = s.kit_body_position(s.kit_body_index(kGrapplePracticeCupEntity));
    const double target_dx = p.x-v.player_position.x;
    const double target_dz = p.z-v.player_position.z;
    const double target_x = std::hypot(target_dx, target_dz);
    const double target_y = p.y-v.player_position.y-.35;
    double low = 1.1, high = 1.49;
    for (int i = 0; i < 48; ++i) {
        const double a = (low+high)*.5;
        const double t = (target_x-.95*std::cos(a))/(speed*std::cos(a));
        const double y = .95*std::sin(a)+speed*std::sin(a)*t-4.905*t*t;
        if (y > target_y) low = a; else high = a;
    }
    const double angle = (low+high)*.5;
    require(s.set_facing(target_dx/target_x, target_dz/target_x) &&
            s.set_grapple_aim(std::cos(angle)*target_dx/target_x, std::sin(angle),
                              std::cos(angle)*target_dz/target_x) &&
            s.request_grapple_action(), "ordinary high aim and Action", s);
    tick(s);
    auto g = s.grapple_gun_state();
    require(g.state == 1 && g.line_attached && g.cartridges+1 == before.cartridges &&
            g.last_launch_work_j > 179 && g.last_launch_work_j <= 180.001,
            "finite cartridge launches actual hook", s);
    int settled = 0; bool retained = false;
    ImpactSpinTrace impact;
    JPH::Vec3 previous = JPH::Vec3::sZero(), first = previous;
    for (int i = 0; i < 540; ++i) {
        tick(s); g = s.grapple_gun_state();
        impact.observe(s,"flight_contact",i);
        require(g.line_attached && g.state == 1, "paid line survives real flight/contact", s);
        JPH::Vec3 local = JPH::Vec3::sZero();
        if (seated_head(s, local) && (settled == 0 || (local-previous).Length() < .006F)) {
            if (settled == 0) first = local;
            ++settled;
        } else settled = 0;
        if (i >= 180 && i % 30 == 0) {
            std::cerr << "[DEBUG-grapple-seat] i=" << i << " settled=" << settled
                      << " head_local=" << local.GetX() << ',' << local.GetY() << ',' << local.GetZ() << '\n';
            sample(s, "seating_probe");
        }
        previous = local;
        if (settled >= 45) {
            if ((local-first).Length() < .08F) { retained = true; break; }
            // A slow roll can exceed the dwell envelope without one large
            // step. Restart that observation window rather than keep an old
            // first point forever after the real head eventually stops.
            settled = 1; first = local;
        }
    }
    require(retained, "head seated on actual cup floor for45ticks", s);
    begin(s, "capture_deliberate_pause");
    for (int i = 0; i < 180; ++i) {
        tick(s);
        impact.observe(s,"human_pause",180+i);
        JPH::Vec3 local = JPH::Vec3::sZero();
        require(s.grapple_gun_state().line_attached && seated_head(s, local),
                "collision-held head and clear eye survive two-second human pause", s);
    }
    require(s.request_grapple_action(), "second Action locks paid line", s); tick(s);
    require(s.grapple_gun_state().state == 2, "native paid line locked", s);
    sample(s, "head_seated_locked");
}
void cross(Simulation &s) {
    fire_into_cup(s);
    // Take up at the real departure lip after ordinary braking. Taking up
    // at center then running toward the anchor creates freefall slack impact.
    walk_body(s,kDeparture,{.95F,0,0},"braked_departure_lip");
    stable_actual_support(s,kDeparture);
    begin(s, "finite_slack_takeup");
    require(s.set_grapple_reel_input(1), "manual takeup input", s);
    bool slack_removed = false;
    for (int i = 0; i < 900; ++i) {
        const auto g = s.grapple_gun_state();
        const double span = std::sqrt(std::pow(g.hook_endpoint.x-g.player_endpoint.x, 2)+
            std::pow(g.hook_endpoint.y-g.player_endpoint.y, 2)+
            std::pow(g.hook_endpoint.z-g.player_endpoint.z, 2));
        if (g.line_length_m <= span && g.tension_n>=300 &&
            s.snapshot().player_grounded) { slack_removed = true; break; }
        tick(s);
        if (i%10==0) {
            const auto now=s.grapple_gun_state();
            JPH::Vec3 head;
            const bool retained=seated_head(s,head);
            std::cerr << "[TAKEUP-contact] i=" << i << " tick=" << s.snapshot().tick_index
                      << " seated=" << retained << " head=" << head.GetX() << ',' << head.GetY() << ',' << head.GetZ()
                      << " tension=" << now.tension_n << " rest=" << now.line_length_m << '\n';
        }
        require(s.grapple_gun_state().state == 2, "native takeup retains line", s);
    }
    require(slack_removed, "finite manual takeup removes ballistic payout slack", s);
    require(s.set_grapple_reel_input(0), "passive swing disables reel", s);
    const auto jump_work = s.snapshot().landing_jump_work_j;
    begin(s, "ordinary_stepoff_passive_swing");
    const auto step_direction=quat(s.kit_body_rotation(s.kit_body_index(kDeparture)))*JPH::Vec3::sAxisX();
    require(s.set_facing(step_direction.GetX(),step_direction.GetZ()) &&
            s.set_move_input(.10F*step_direction.GetX(),.10F*step_direction.GetZ()), "ordinary slow exposed step off", s);
    int passive_loaded = 0; bool airborne = false;
    for (int i = 0; i < 270; ++i) {
        tick(s);
        const auto v = s.snapshot(); const auto g = s.grapple_gun_state();
        require(g.state == 2 && g.line_attached, "rope survives exposed swing", s);
        JPH::Vec3 local = JPH::Vec3::sZero();
        require(seated_head(s, local), "real cup retains head under passive load", s);
        if (!v.player_grounded && v.support_entity_id == 0) {
            airborne = true; require(s.set_move_input(0, 0), "passive flight neutral stick", s);
            if (g.tension_n > 250) ++passive_loaded; else passive_loaded = 0;
        }
        if (passive_loaded >= 12) break;
    }
    require(airborne && passive_loaded >= 12 && s.snapshot().landing_jump_work_j == jump_work,
            "actual rope carries airborne player with no jump or reel", s);
    sample(s, "passive_loaded");
    begin(s, "loaded_take_up_and_timed_drop");
    const double work_before = s.grapple_gun_state().reel_work_j;
    const double steering_before=s.snapshot().air_control_command_positive_work_j;
    require(s.set_grapple_reel_input(1), "finite loaded human work input", s);
    bool released = false; int loaded_work_ticks = 0;
    for (int i = 0; i < 900; ++i) {
        const auto target_footing=body_point(s,kReceiver,JPH::Vec3::sZero());
        steer(s,target_footing.x,target_footing.z);
        const double air_before=s.snapshot().air_control_command_positive_work_j;
        const auto previous = s.grapple_gun_state(); tick(s);
        const auto v = s.snapshot(); const auto g = s.grapple_gun_state();
        require(g.state == 2 && g.line_attached, "loaded takeup preserves real line", s);
        JPH::Vec3 local = JPH::Vec3::sZero();
        require(seated_head(s, local), "real cup retains head under powered load", s);
        const double air_work=v.air_control_command_positive_work_j-air_before;
        require(air_work>=-.001 && air_work<=3000*Simulation::kFixedStepSeconds+.02,
                "existing ordinary steering retains native finite work budget",s);
        const double work = g.reel_work_j-previous.reel_work_j;
        require(work >= -.001 && work <= 750*Simulation::kFixedStepSeconds+.02 &&
                previous.line_length_m-g.line_length_m <= Simulation::kFixedStepSeconds+.001,
                "finite750W and1mps takeup", s);
        if (!v.player_grounded && g.tension_n > 250 && work > .01) ++loaded_work_ticks;
        // Ordinary timed release uses the actual ballistic landing corridor.
        // A fixed rest-length stop neither proves height nor predicts arrival.
        const auto target=body_point(s,kReceiver,{0,.90F,0});
        const double rise=v.player_position.y-target.y;
        const double vy=v.player_linear_velocity.y;
        const double discriminant=vy*vy+2*9.81*rise;
        if (rise>0 && discriminant>=0 && loaded_work_ticks>=12 &&
            g.reel_work_j-work_before>100 && !v.player_grounded) {
            const double flight=(vy+std::sqrt(discriminant))/9.81;
            const Vector3 predicted{v.player_position.x+v.player_linear_velocity.x*flight,
                target.y,v.player_position.z+v.player_linear_velocity.z*flight};
            const auto landing=body_local_point(s,kReceiver,predicted);
            if (flight>.05 && landing.GetX()>-1.0F && landing.GetX()<1.0F &&
                landing.GetZ()>-1.0F && landing.GetZ()<1.6F &&
                v.player_linear_velocity.x>.25F) {
                require(s.set_grapple_reel_input(0) && s.request_grapple_release(), "timed native Drop", s);
                tick(s); released = true; break;
            }
        }
    }
    require(released && loaded_work_ticks >= 12 && s.grapple_gun_state().reel_work_j-work_before > 100,
            "loaded finite work earns receiving height before Drop", s);
    std::cout << "GRAPPLE_COMMAND_WORK,reel_j=" << s.grapple_gun_state().reel_work_j-work_before
              << ",ordinary_steering_j=" << s.snapshot().air_control_command_positive_work_j-steering_before << '\n';
    require(!s.grapple_gun_state().line_attached, "Drop removes line", s);
    bool landed = false;
    for (int i = 0; i < 270; ++i) {
        const auto target=body_point(s,kReceiver,JPH::Vec3::sZero());
        steer(s,target.x,target.z); tick(s);
        const auto v=s.snapshot();
        if (v.player_grounded && v.support_entity_id==kReceiver && !v.player_crouched &&
            v.checkpoint_footing_valid) { landed = true; break; }
    }
    require(landed, "real receiver1968 contact after release", s);
    walk_body(s,kReceiver,JPH::Vec3::sZero(),"receiver_braking"); stable_actual_support(s,kReceiver);
    walk(s, receiver_x, -132.8, "original_roof_exit"); stable(s, Simulation::kWorldSolidEntityId, 485.15);
    const auto away=s.grapple_gun_state();
    require(away.acquired && !away.station_available,"acquired gun reaches real roof away from rack",s);
    require(s.set_grapple_equipped(false),"ordinary away-from-rack stow",s); tick(s);
    require(!s.grapple_gun_state().equipped && s.set_grapple_equipped(true),"ordinary portable re-equip request",s); tick(s);
    const auto equipped=s.grapple_gun_state();
    require(equipped.equipped && !equipped.station_available && equipped.cartridges==away.cartridges &&
            equipped.available_hooks==away.available_hooks,"portable re-equip preserves actual finite stock",s);
    std::cout << "GRAPPLE_PORTABLE away_from_rack=1 stow_re_equip=1 finite_stock=1\n";
}
void miss_retry(Simulation &s) {
    begin(s, "deliberate_lower_bay_miss");
    const auto before = s.grapple_gun_state();
    require(s.set_grapple_aim(.8, -.6, 0) && s.request_grapple_action(), "ordinary wrong aim fire", s);
    tick(s);
    require(s.grapple_gun_state().cartridges+1 == before.cartridges, "miss spends cartridge", s);
    tick(s, 30);
    require(s.request_grapple_release(), "release missed line", s); tick(s);
    walk_body(s,kRecovery,{-1,0,0},"same_life_lower_deck_fall"); stable_actual_support(s, kRecovery);
    auto g=s.grapple_gun_state();
    const auto hook=g.active_hook;
    Vector3 position{}; bool found=false;
    for (const auto &h:g.hooks) if (h.present && h.body==hook) { position=h.position; found=true; }
    require(found,"actual missed loose hook remains",s);
    require(s.set_crouch_input(true),"ordinary crouch before low-roof hook approach",s);
    walk(s,position.x,position.z,"walk_to_actual_loose_hook");
    require(s.set_crouch_input(true),"crouch to reach loose hook",s); tick(s,90);
    bool reachable=false;
    for (int i=0;i<900;++i) {
        g=s.grapple_gun_state(); const auto v=s.snapshot();
        const double dx=g.hook_endpoint.x-g.player_endpoint.x;
        const double dy=g.hook_endpoint.y-g.player_endpoint.y;
        const double dz=g.hook_endpoint.z-g.player_endpoint.z;
        if (std::sqrt(dx*dx+dy*dy+dz*dz)<.8 &&
            std::hypot(v.player_linear_velocity.x,v.player_linear_velocity.z)<.14) { reachable=true; break; }
        steer(s,g.hook_endpoint.x,g.hook_endpoint.z); tick(s);
    }
    require(reachable,"ordinary approach within actual hook reach",s);
    require(s.set_move_input(0,0),"neutral retrieval input",s); tick(s,45);
    bool retrieved=false;
    for (int i=0;i<900;++i) {
        g=s.grapple_gun_state();
        if (g.action_recovers_hook && i%30==0) {
            require(s.request_grapple_action(),"deliberate loose-hook recovery attempt",s);
        } else if (!g.action_recovers_hook) steer(s,g.hook_endpoint.x,g.hook_endpoint.z);
        else require(s.set_move_input(0,0),"wait for actual moving hook to settle",s);
        tick(s); g=s.grapple_gun_state();
        require(g.cartridges==before.cartridges-1,"recovery attempts never spend another cartridge",s);
        bool remains=false;
        for (const auto &h:g.hooks) if (h.present && h.body==hook) remains=true;
        if (!remains) { retrieved=true; break; }
    }
    require(retrieved && g.available_hooks==before.available_hooks && g.cartridges==before.cartridges-1,
            "real retrieval returns hardware and preserves spent cartridge",s);
    require(s.set_crouch_input(false),"stand after actual retrieval",s); tick(s,45);
    walk_body(s,kRecovery,{-1.5F,0,-1.85F},"recovery_stair_toe");
    const auto stair_work_before=s.snapshot().landing_jump_work_j;
    const auto steps_before=s.snapshot().step_up_count;
    const auto legacy_steps_before=s.snapshot().step_up_pose_count;
    walk_body(s,kRecovery,{-4.75F,2.5F,-1.85F},"ordinary_side_stair_ascent",2700);
    std::cout << "RECOVERY_STAIR_WORK,before=" << stair_work_before << ",after="
              << s.snapshot().landing_jump_work_j << ",legacy_before=" << legacy_steps_before
              << ",legacy_after=" << s.snapshot().step_up_pose_count
              << ",steps_before=" << steps_before << ",steps_after=" << s.snapshot().step_up_count << '\n';
    require(s.snapshot().landing_jump_work_j>stair_work_before+1,
            "recovery stairs use finite reciprocal leg work",s);
    require(s.snapshot().step_up_pose_count==legacy_steps_before,
            "recovery stairs never use legacy step pose writes",s);
    const auto top_local=body_local_point(s,kRecovery,s.snapshot().player_position);
    require(top_local.GetY()>3.25F && top_local.GetY()<3.55F,
            "actual recovery top tread standing height in body frame",s);
    stable_actual_support(s,kRecovery);
    // Authored departure originX10: stair exit authoredX9.25 is localX-.75.
    walk_body(s,kDeparture,{-.75F,0,0},"return_departure"); stable_actual_support(s,kDeparture);
    walk_body(s,kDeparture,JPH::Vec3::sZero(),"retry_launch_point"); stable_actual_support(s,kDeparture);
    rack_follows_departure(s);
    require(s.grapple_gun_state().cartridges == before.cartridges-1,
            "same life return preserves spent inventory", s);
    sample(s, "miss_recovered");
}
bool return_rail_retained(const Simulation&s) {
 const auto g=s.grapple_gun_state();const auto cup=s.kit_body_index(kGrapplePracticeCupEntity);
 const auto q=quat(s.kit_body_rotation(cup));const auto center=body_point(s,kGrapplePracticeCupEntity,{.6F,.6F,.19F});
 for(const auto&h:g.hooks)if(h.present&&h.body==g.active_hook){
  const auto inv=quat(h.rotation).Conjugated();
  return scraperx::test::retained_square_in_crook(inv*JPH::Vec3(float(center.x-h.position.x),float(center.y-h.position.y),float(center.z-h.position.z)),
   inv*(q*JPH::Vec3::sAxisZ()),inv*(q*JPH::Vec3::sAxisX()),inv*(q*JPH::Vec3::sAxisY()),.025F,.54F,JPH::PhysicsSettings{}.mPenetrationSlop);
 }return false;
}
void portable_return_rail(Simulation&s){
 begin(s,"portable_return_rail");
 walk_body(s,kReceiver,{-.4F,0,.19F},"portable_rail_launch");stable_actual_support(s,kReceiver);
 const auto before=s.grapple_gun_state();const auto v=s.snapshot();
 require(before.acquired&&before.equipped&&!before.station_available&&!before.line_attached,"portable second use without rack",s);
 const auto target=body_point(s,kGrapplePracticeCupEntity,{.6F,.6F,.19F});
 const double dx=target.x-v.player_position.x,dz=target.z-v.player_position.z,range=std::hypot(dx,dz),height=target.y-v.player_position.y-.35;
 const double speed=std::sqrt(2*GrapplingGun::cartridge_energy_j/(1./85+1./GrapplingGun::hook_mass_kg))/GrapplingGun::hook_mass_kg;
 double lo=1.1,hi=1.5707;
 for(int i=0;i<48;++i){const double a=(lo+hi)/2,t=(range-.95*std::cos(a))/(speed*std::cos(a));if(.95*std::sin(a)+speed*std::sin(a)*t-4.905*t*t>height)lo=a;else hi=a;}
 const double a=(lo+hi)/2;
 require(s.set_facing(dx,dz)&&s.set_grapple_aim(std::cos(a)*dx/range,std::sin(a),std::cos(a)*dz/range)&&s.request_grapple_action(),"ordinary second rail aim/action",s);tick(s);
 int dwell=0;bool captured=false;
 for(int i=0;i<540;++i){tick(s);require(s.grapple_gun_state().line_attached,"second shot actual rope survives",s);if(return_rail_retained(s)){if(++dwell>=45){captured=true;break;}}else dwell=0;}
 require(captured,"actual square return rail capture/dwell",s);
 for(int i=0;i<180;++i){tick(s);require(return_rail_retained(s)&&s.grapple_gun_state().line_attached,"second rail physical pause retention",s);}
 require(s.set_grapple_auto_reel(true),"portable AUTO REEL command",s);
 const auto work=s.grapple_gun_state().reel_work_j;int loaded=0,airborne_loaded=0;bool lifted=false;
 for(int i=0;i<720;++i){tick(s);const auto g=s.grapple_gun_state();require(g.line_attached&&return_rail_retained(s),"second rail remains retained under finite reel",s);
  if(g.tension_n>250)++loaded;
  if(g.tension_n>250&&!s.snapshot().player_grounded)++airborne_loaded;else airborne_loaded=0;
  if(g.reel_work_j-work>400&&airborne_loaded>=12){lifted=true;break;}
 }
 require(lifted,"real portable rail supports airborne loaded player and paid reel work",s);
 const auto loaded_state=s.grapple_gun_state();sample(s,"portable_rail_loaded");
 require(s.set_grapple_auto_reel(false)&&s.request_grapple_release(),"ordinary STOP and Drop",s);tick(s);
 require(!s.grapple_gun_state().line_attached&&s.grapple_gun_state().cartridges+1==before.cartridges,"release preserves spent second cartridge",s);
 bool landed=false;
 for(int i=0;i<360;++i){const auto p=body_point(s,kReceiver,{-.4F,0,.19F});steer(s,p.x,p.z);tick(s);if(s.snapshot().player_grounded&&s.snapshot().support_entity_id==kReceiver){landed=true;break;}}
 require(landed,"ordinary release returns player to real receiver",s);
 walk_body(s,kReceiver,{-.4F,0,.19F},"portable_rail_return");stable_actual_support(s,kReceiver);
 std::cout<<"PASS PORTABLE_RETURN_RAIL actual_square_rail=1 same_assembly_reverse_use=1 capture_dwell=1 airborne_loaded=1 reel_j="<<loaded_state.reel_work_j-work<<" loaded_ticks="<<loaded<<" consecutive_airborne_loaded_ticks="<<airborne_loaded<<" spent_cartridges=2 release_receiver=1 deaths=0\n";
}
void portable_fire_on_real_roof(Simulation &s) {
    begin(s,"portable_roof_fire");
    const auto before=s.grapple_gun_state();
    require(before.acquired && before.equipped && !before.station_available && !before.line_attached,
            "portable shot starts equipped away from acquisition rack",s);
    require(s.set_move_input(0,0) && s.set_grapple_aim(0,1,0) && s.request_grapple_action(),
            "ordinary away-rack upward aim and fire",s); tick(s);
    const auto launched=s.grapple_gun_state();
    require(launched.line_attached && launched.cartridges+1==before.cartridges &&
            launched.available_hooks+1==before.available_hooks && launched.last_launch_work_j>179 &&
            launched.last_launch_work_j<=180.001,"portable shot launches actual finite stock and energy",s);
    const auto hook=launched.active_hook;
    Vector3 start{}; bool present=false;
    for(const auto &h:launched.hooks) if(h.present && h.body==hook) {start=h.position;present=true;}
    require(present,"portable projectile is an actual native body",s);
    tick(s,45);
    const auto flight=s.grapple_gun_state(); present=false;
    for(const auto &h:flight.hooks) if(h.present && h.body==hook && h.position.y>start.y+2) present=true;
    require(present && flight.line_attached && flight.cartridges==launched.cartridges &&
            flight.available_hooks==launched.available_hooks,"actual away-rack projectile flies without another stock grant",s);
    require(s.request_grapple_release(),"portable ordinary line release",s); tick(s);
    const auto released=s.grapple_gun_state(); present=false;
    for(const auto &h:released.hooks) if(h.present && h.body==hook) present=true;
    require(present && !released.line_attached && released.cartridges==launched.cartridges &&
            released.available_hooks==launched.available_hooks,"portable release leaves real hardware and spent cartridge",s);
    require(s.snapshot().death_count==0,"portable firing retains same life",s);
    std::cout << "GRAPPLE_PORTABLE actual_away_rack_shot=1 finite_cartridge=1 real_flight=1 real_release=1 deaths=0\n";
}
void failed_retrieval_preserves_stock(Simulation &s) {
    require(s.set_grapple_aim(1,0,0) && s.request_grapple_action(), "ordinary short-range shot", s);
    tick(s);
    require(s.request_grapple_release(), "release nearby moving hook", s); tick(s);
    const auto before=s.grapple_gun_state();
    const double dx=before.hook_endpoint.x-before.player_endpoint.x;
    const double dy=before.hook_endpoint.y-before.player_endpoint.y;
    const double dz=before.hook_endpoint.z-before.player_endpoint.z;
    require(!before.line_attached && before.action_recovers_hook && dx*dx+dy*dy+dz*dz<=2.25,
            "actual released hook is in native Action recovery region",s);
    require(s.request_grapple_action(), "Action attempts nearby hook recovery", s); tick(s);
    const auto after=s.grapple_gun_state();
    sample(s,"failed_retrieval_feedback");
    require(!after.line_attached && (after.failure==8 || after.failure==11) && after.cartridges==before.cartridges &&
            after.available_hooks==before.available_hooks && after.active_hook==before.active_hook,
            "failed nearby retrieval retains feedback and never launches another cartridge", s);
    std::cout << "PASS GRAPPLE_ROUTE failed_retrieval_preserves_stock\n";
}
void older_hook_recovery_intent(Simulation &s) {
    // Returning Action intent to latest-hook-only lookup must fail this mode.
    // Both hooks come from finite ordinary shots; observations never move them.
    walk(s,10,-131.1,"older_hook_roof_launch");
    stable(s,Simulation::kWorldSolidEntityId,485.15);
    const auto initial=s.grapple_gun_state();
    require(s.set_grapple_aim(.8,-.6,0) && s.request_grapple_action(),
            "ordinary first hook shot onto actual roof",s); tick(s);
    auto first=s.grapple_gun_state();
    require(first.line_attached && first.cartridges+1==initial.cartridges,
            "first actual hook spends one cartridge",s);
    const auto older=first.active_hook;
    require(s.request_grapple_release(),"ordinary first hook release",s); tick(s,180);
    walk(s,6,-131.1,"walk_away_from_older_hook");
    stable(s,Simulation::kWorldSolidEntityId,485.15);
    require(!s.grapple_gun_state().action_recovers_hook,
            "ordinary walk leaves first actual hook beyond recovery intent",s);
    require(s.set_grapple_aim(-1,0,0) && s.request_grapple_action(),
            "ordinary second hook shot away from first",s); tick(s);
    const auto second=s.grapple_gun_state();
    const auto latest=second.active_hook;
    require(second.line_attached && latest!=older && second.cartridges+2==initial.cartridges &&
            second.available_hooks+2==initial.available_hooks,
            "two distinct actual hooks spend finite hardware and cartridges",s);
    require(s.request_grapple_release(),"ordinary latest hook release",s); tick(s,180);
    require(s.set_crouch_input(true),"ordinary crouch approaches older hook",s); tick(s,45);
    auto find_hook=[](const GrappleGunSnapshot &g,std::uint32_t body) {
        return std::find_if(g.hooks.begin(),g.hooks.end(),[body](const auto &h) {
            return h.present && h.body==body;
        });
    };
    auto eye=[](const GrappleGunSnapshot::Hook &h) {
        const auto offset=quat(h.rotation)*GrapplingGun::hook_eye_local();
        return Vector3{h.position.x+offset.GetX(),h.position.y+offset.GetY(),h.position.z+offset.GetZ()};
    };
    auto distance=[](Vector3 a,Vector3 b) {
        return std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));
    };
    bool reached=false;
    for (int i=0;i<1800;++i) {
        const auto g=s.grapple_gun_state();
        const auto old=find_hook(g,older),last=find_hook(g,latest);
        require(old!=g.hooks.end() && last!=g.hooks.end(),"both actual released hooks remain",s);
        const auto target=eye(*old);
        if (distance(target,g.player_endpoint)<1.2 && distance(eye(*last),g.player_endpoint)>2 &&
            s.snapshot().player_grounded) { reached=true; break; }
        steer(s,target.x,target.z); tick(s);
    }
    require(reached,"ordinary approach reaches older hook while latest is distant",s);
    require(s.set_move_input(0,0),"neutral older-hook recovery attempt",s);
    const auto before=s.grapple_gun_state();
    const auto old=find_hook(before,older),last=find_hook(before,latest);
    const double older_distance=distance(eye(*old),before.player_endpoint);
    const double latest_distance=distance(eye(*last),before.player_endpoint);
    require(!before.line_attached && before.active_hook==latest && older_distance<1.2 &&
            latest_distance>2 && before.action_recovers_hook,
            "native Action recovery intent includes older real hook beyond latest",s);
    require(s.request_grapple_action(),"ordinary Action attempts older hook recovery",s); tick(s);
    const auto after=s.grapple_gun_state();
    const bool recovered=find_hook(after,older)==after.hooks.end();
    const bool real_failure=after.failure==unsigned(GrapplingGun::Failure::OutOfReach) ||
        after.failure==unsigned(GrapplingGun::Failure::RetrievalBlocked) ||
        after.failure==unsigned(GrapplingGun::Failure::RetrievalTooFast);
    require(!after.line_attached && after.active_hook==latest && after.cartridges==before.cartridges &&
            after.launch_work_j==before.launch_work_j && find_hook(after,latest)!=after.hooks.end() &&
            ((recovered && after.failure==unsigned(GrapplingGun::Failure::None) &&
              after.available_hooks==before.available_hooks+1) ||
             (!recovered && real_failure && after.available_hooks==before.available_hooks)),
            "older-hook Action recovers or preserves actual failure without another shot",s);
    sample(s,"older_hook_recovery_feedback");
    std::cout << "PASS GRAPPLE_ROUTE older-hook older_distance=" << older_distance
              << " latest_distance=" << latest_distance << " retrieved=" << recovered
              << " failure=" << after.failure << " spent_cartridges=2\n";
}
// Same-life access uses only the existing roofs, ordinary walking/air input,
// and the carried parachute. A second debug restart would hide portability.
void independent_407_descent(Simulation &s) {
    walk(s, departure_x, -132.8, "return_roof");
    walk(s, 0, -132.8, "north_roof_edge");
    begin(s, "outside_descent");
    bool deployed = false, landed = false;
    for (int i = 0; i < 1800; ++i) {
        const auto v = s.snapshot();
        steer(s, 0, v.player_position.y > 442 ? -140 : -127.6);
        if (!v.player_grounded && !deployed) {
            require(s.request_parachute(), "ordinary first parachute deployment", s);
            deployed = true;
        }
        tick(s, 1, true);
        if (i % 45 == 0) sample(s, "descent");
        if (s.snapshot().player_grounded && s.snapshot().player_position.y < 420) {
            landed = true; break;
        }
        if (s.snapshot().player_position.y < 406) break;
    }
    require(deployed && landed, "actual controlled descent reaches existing418roof", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false), "ordinary stand after418impact", s);
    tick(s, 135);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == Simulation::kWorldSolidEntityId &&
            !s.snapshot().player_crouched, "firm existing418roof", s);
    walk(s, -5.1, -127.8, "418_receiver_alignment");
    begin(s, "418_to407_parachute");
    deployed = landed = false;
    for (int i = 0; i < 450; ++i) {
        const auto v = s.snapshot();
        steer(s, -5.1, v.player_position.y > 416.5 ? -125.8 : -128.2);
        if (!v.player_grounded && !deployed) {
            require(s.request_parachute(), "ordinary second parachute deployment", s);
            deployed = true;
        }
        tick(s, 1, true);
        if (i % 30 == 0) sample(s, "second_descent");
        if (s.snapshot().player_grounded && s.snapshot().support_entity_id == 1935) {
            landed = true; break;
        }
        if (s.snapshot().player_position.y < 406) break;
    }
    require(deployed && landed, "actual407receiving steel reached in same life", s);
    require(s.set_move_input(0, 0), "neutral407landing", s);
    tick(s, 135); sample(s, "407_receiver_landed");
}
bool independent_407_retained(const Simulation &s) {
    const auto g = s.grapple_gun_state();
    for (const auto &h : g.hooks) if (h.present && h.body == g.active_hook) {
        const auto inv = quat(h.rotation).Conjugated();
        // Existing receiver handrail: real50mm square section,900mm along X.
        // Observe every corner at every finger plane; no anchor flag/joint.
        return scraperx::test::retained_square_in_crook(
            inv * JPH::Vec3(float(-5.1-h.position.x), float(408.3-h.position.y), float(-128.645-h.position.z)),
            inv * JPH::Vec3::sAxisX(), inv * JPH::Vec3::sAxisY(), inv * JPH::Vec3::sAxisZ(),
            .025F, .45F, JPH::PhysicsSettings{}.mPenetrationSlop);
    }
    return false;
}
void independent_407(Simulation &s) {
    require(s.entity_body_count(1935) == 1 && s.entity_body_count(1936) == 1,
            "existing independent inspection receiver and recovery steel", s);
    independent_407_descent(s);
    // Go around the rail's left end on the real timber, then step onto its
    // existing lower recovery tray. The shot has a transverse +Z bearing.
    walk(s, -5.95, -128.2, "407_left_end_approach");
    walk(s, -5.95, -129.8, "407_to_recovery_tray");
    require(s.set_move_input(0, 0) && s.set_crouch_input(false), "ordinary stand after tray impact", s);
    tick(s, 135); stable_actual_support(s, 1936);
    walk(s, -5.1, -129.8, "independent_rail_launch"); stable_actual_support(s, 1936);
    begin(s, "independent407_ordinary_fire");
    const auto before = s.grapple_gun_state();
    require(before.acquired && before.equipped && !before.station_available && !before.line_attached &&
            before.cartridges == 4, "portable original finite stock at independent tray", s);
    const auto v = s.snapshot();
    const double dx = -5.1-v.player_position.x, dz = -128.645-v.player_position.z;
    const double range = std::hypot(dx, dz), height = 408.3-v.player_position.y-.35;
    const double speed = std::sqrt(2*GrapplingGun::cartridge_energy_j /
        (1./85+1./GrapplingGun::hook_mass_kg))/GrapplingGun::hook_mass_kg;
    double lo = 1.1, hi = 1.5707;
    for (int i = 0; i < 48; ++i) {
        const double a = (lo+hi)/2, t = (range-.95*std::cos(a))/(speed*std::cos(a));
        if (.95*std::sin(a)+speed*std::sin(a)*t-4.905*t*t > height) lo = a;
        else hi = a;
    }
    const double aim = (lo+hi)/2;
    std::cout << "INDEPENDENT407_AIM pitch=" << aim << " range=" << range << " height=" << height << '\n';
    require(s.set_facing(dx, dz) &&
            s.set_grapple_aim(std::cos(aim)*dx/range, std::sin(aim), std::cos(aim)*dz/range) &&
            s.request_grapple_action(), "ordinary near-vertical independent rail aim/fire", s);
    tick(s);
    require(s.grapple_gun_state().line_attached && s.grapple_gun_state().cartridges+1 == before.cartridges,
            "actual independent shot spends one cartridge", s);
    int dwell = 0; bool captured = false;
    for (int i = 0; i < 450; ++i) {
        tick(s);
        require(s.grapple_gun_state().line_attached, "physical independent line survives", s);
        if (independent_407_retained(s)) {
            if (++dwell >= 45) { captured = true; break; }
        } else dwell = 0;
    }
    require(captured, "actual independent square rail collision capture", s);
    for (int i = 0; i < 90; ++i) {
        tick(s); require(independent_407_retained(s), "independent passive rail retention", s);
    }
    sample(s, "rail407_captured");
    const double work_before = s.grapple_gun_state().reel_work_j, start_y = s.snapshot().player_position.y;
    require(s.set_grapple_auto_reel(true), "ordinary independent AUTO REEL", s);
    int loaded = 0; bool lifted = false;
    for (int i = 0; i < 720; ++i) {
        const auto old = s.grapple_gun_state(); tick(s);
        const auto g = s.grapple_gun_state();
        require(g.line_attached && independent_407_retained(s), "independent rail retains loaded hook", s);
        const double work = g.reel_work_j-old.reel_work_j;
        require(work >= -.001 && work <= 750*Simulation::kFixedStepSeconds+.02 &&
                old.line_length_m-g.line_length_m <= Simulation::kFixedStepSeconds+.001,
                "native750W and1mps independent reel limits", s);
        if (!s.snapshot().player_grounded && g.tension_n > 250 && work > 0) ++loaded;
        else loaded = 0;
        if (loaded >= 12 && g.reel_work_j-work_before > 400 && s.snapshot().player_position.y-start_y > .3) {
            lifted = true; break;
        }
    }
    require(lifted, "real independent airborne loaded reel travel", s);
    const double rise = s.snapshot().player_position.y-start_y;
    const double paid = s.grapple_gun_state().reel_work_j-work_before;
    sample(s, "rail407_loaded");
    require(s.set_grapple_auto_reel(false) && s.request_grapple_release(), "ordinary independent STOP and Drop", s);
    tick(s);
    bool landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, -5.1, -129.8); tick(s);
        if (s.snapshot().player_grounded && s.snapshot().support_entity_id == 1936) { landed = true; break; }
    }
    require(landed, "real tray landing after independent Drop", s);
    walk(s, -5.1, -129.8, "independent_tray_braking"); stable_actual_support(s, 1936);
    walk(s, -7.5, -129.8, "independent_firm_exit"); stable_actual_support(s, 1936);
    require(!s.grapple_gun_state().line_attached && s.grapple_gun_state().cartridges == 3 &&
            s.grapple_gun_state().available_hooks+1 == before.available_hooks,
            "real release preserves finite spent stock", s);
    std::cout << "PASS INDEPENDENT407 actual_square_rail=1 acquired_same_life=1 rise_m=" << rise
              << " reel_j=" << paid << " consecutive_airborne_loaded_ticks=" << loaded
              << " release_firm_tray=1 spent_cartridges=1 deaths=0\n";
}
} // namespace
int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "primary";
    if (mode != "primary" && mode != "miss-retry" && mode != "failed-retrieval" && mode != "recovery-only" && mode != "older-hook" && mode != "portable-fire" && mode != "portable-rail" && mode != "independent407") return EXIT_FAILURE;
    std::cout << std::fixed << std::setprecision(6);
    Simulation s(InitialSpawn::ExteriorGrade, WorldContent::Slingshot);
    require(s.entity_body_count(kDeparture) == 1 && s.entity_body_count(kReceiver) == 1 &&
            s.entity_body_count(kRecovery) == 1 && s.entity_body_count(kGrapplePracticeCupEntity) == 1,
            "native route bodies in ordinary world", s);
    require(s.debug_restart_at({10, 485.15, -132.8}), "single initial actual484roof staging", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary initial stance", s);
    tick(s, 45); stable(s, Simulation::kWorldSolidEntityId, 485.15);
    walk(s, departure_x, -132.8, "roof_to_launch_bay");
    walk_body(s,kDeparture,JPH::Vec3::sZero(),"real_departure_approach"); stable_actual_support(s,kDeparture);
    require(s.grapple_gun_state().station_available && s.set_grapple_equipped(true), "real gun station equip", s);
    tick(s); require(s.grapple_gun_state().acquired && s.grapple_gun_state().equipped, "gun acquired", s);
    if (mode == "independent407") { independent_407(s); return EXIT_SUCCESS; }
    if (mode == "failed-retrieval") { failed_retrieval_preserves_stock(s); return EXIT_SUCCESS; }
    if (mode == "older-hook") { older_hook_recovery_intent(s); return EXIT_SUCCESS; }
    if (mode == "miss-retry" || mode == "recovery-only") {
        miss_retry(s);
        if (mode == "recovery-only") {
            std::cout << "PASS GRAPPLE_ROUTE recovery-only initial_staging_only=1 actual_hook_retrieved=1"
                         " finite_leg_stairs=1 departure_return=1 spent_cartridge=1 deaths=0\n";
            return EXIT_SUCCESS;
        }
    }
    rack_follows_departure(s);
    const unsigned cartridges = s.grapple_gun_state().cartridges;
    cross(s);
    require(s.grapple_gun_state().cartridges+1 == cartridges, "retry consumes another finite cartridge", s);
    if(mode=="portable-fire") portable_fire_on_real_roof(s);
    if(mode=="portable-rail") portable_return_rail(s);
    std::cout << "PASS GRAPPLE_ROUTE mode=" << mode
              << " initial_staging_only=1 native_head_seated=1 passive_loaded=1 loaded_reel_work=1"
                 " receiver1968=1 original_roof_exit=1 deaths=0\n";
}
