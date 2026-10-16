#include "sim/simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace scraperx::sim;
namespace {
const char *phase = "START";
Simulation *active = nullptr;
void observe(Simulation &s, const char *label, std::uint64_t = 0) {
    const auto p = s.snapshot();
    const auto c = s.yard_crane_state();
    const auto f = s.forklift_state();
    const auto t = s.kit_body_position(s.kit_body_index(2440));
    std::cout << label << " phase=" << phase << " truck=" << t.x << ',' << t.y << ',' << t.z
              << " player=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z
              << " support=" << p.support_entity_id << " grounded=" << p.player_grounded
              << " truck_controls=" << f.controls_reachable << " cab=" << c.controls_reachable
              << " leg=" << c.rigging_leg << " mask=" << unsigned(c.attached_mask)
              << " failure=" << unsigned(c.failure) << " hoist=" << c.hoist_length_m
              << " yaw=" << c.slew_angle_rad << " trolley=" << c.trolley_position_m
              << " slew_rate=" << c.slew_target_rad_s << " trolley_rate=" << c.trolley_target_mps
              << " transport_energy=" << c.transport_energy_j
              << " lift=" << f.lift_position_m << " tilt=" << f.tilt_angle_rad << std::endl;
}
void require(bool ok, const char *why) {
    if (ok) return;
    if (active) observe(*active, "FIRST_FAILURE");
    std::cerr << "FAIL public crane freight route: " << why << std::endl;
    std::exit(1);
}
void tick(Simulation &s, unsigned count = 1) {
    while (count--) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick accepted");
        require(s.snapshot().death_count == 0, "route remains recoverable without death");
        require(s.yard_crane_state().failure == 0 && s.yard_crane_state().hoist_connected,
                "actual crane and all physical cables remain intact");
    }
}
void begin(Simulation &s, const char *name) { phase = name; observe(s, "PHASE"); }
#include "forklift_public_driver.hpp"
// Ordinary walking controller copied unchanged from forklift_route_walk.hpp;
// the cargo-specific ascent helpers in that header are outside this route.
void walk_to(Simulation &s, double x, double z, double speed = .35) {
    for (unsigned i = 0; i < 900; ++i) {
        const auto p = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        if (s.snapshot().player_grounded && std::hypot(x - p.x, z - p.z) < .07 &&
            std::hypot(v.x, v.z) < .15) {
            require(s.set_move_input(0, 0), "stop walking through public intent");
            tick(s, 20);
            return;
        }
        const double dx = x - p.x, dz = z - p.z;
        const double ix = std::clamp(dx - .45 * v.x, -speed, speed);
        const double iz = std::clamp(dz - .45 * v.z, -speed, speed);
        require(s.set_move_input(ix, iz), "route walking intent accepted");
        tick(s);
    }
    observe(s, "WALK_FAILED");
    const auto end = s.snapshot();
    const auto q = s.kit_body_rotation(s.kit_body_index(2440));
    std::cout << "WALK_TARGET x=" << x << " z=" << z
              << " velocity=" << end.player_linear_velocity.x << ',' << end.player_linear_velocity.y << ',' << end.player_linear_velocity.z
              << " foot=" << end.foot_push_active << " foot_load=" << end.foot_push_last_load_n
              << " truck_rotation=" << q.x << ',' << q.y << ',' << q.z << ',' << q.w << std::endl;
    require(false, "ordinary movement reaches next route approach");
}

Vector3 rotate(Quaternion q, Vector3 v) {
    const double x = v.x, y = v.y, z = v.z;
    const double tx = 2 * (q.y * z - q.z * y);
    const double ty = 2 * (q.z * x - q.x * z);
    const double tz = 2 * (q.x * y - q.y * x);
    return {x + q.w * tx + q.y * tz - q.z * ty,
            y + q.w * ty + q.z * tx - q.x * tz,
            z + q.w * tz + q.x * ty - q.y * tx};
}
Vector3 truck_point(Simulation &s, double x, double y, double z) {
    const auto body = s.kit_body_index(2440);
    const auto p = s.kit_body_position(body);
    const auto v = rotate(s.kit_body_rotation(body), {x, y, z});
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
std::array<double, 2> body_vertical_extent(Simulation &s, std::uint64_t entity) {
    const auto body = s.kit_body_index(entity);
    const auto origin = s.kit_body_position(body);
    const auto rotation = s.kit_body_rotation(body);
    std::array<double, 2> extent{1e9, -1e9};
    for (unsigned i = 0; i < s.kit_body_part_count(body); ++i) {
        const auto part = s.kit_body_part(body, i);
        for (double x : {-part.half.x, part.half.x})
            for (double y : {-part.half.y, part.half.y})
                for (double z : {-part.half.z, part.half.z}) {
                    auto v = rotate(part.rotation, {x, y, z});
                    v = rotate(rotation, {v.x + part.offset.x, v.y + part.offset.y, v.z + part.offset.z});
                    extent[0] = std::min(extent[0], origin.y + v.y);
                    extent[1] = std::max(extent[1], origin.y + v.y);
                }
    }
    return extent;
}
double truck_bottom(Simulation &s) {
    double low = 1e9;
    for (const auto entity : {2440, 2441, 2442, 2443})
        low = std::min(low, body_vertical_extent(s, entity)[0]);
    for (const auto &wheel : s.forklift_state().wheels)
        low = std::min(low, wheel.position.y - wheel.radius_m - wheel.width_m * .5);
    return low;
}
Vector3 eye(Simulation &s, unsigned leg) {
    return truck_point(s, leg % 2 ? .52 : -.52, leg < 2 ? .315 : .565, leg < 2 ? .75 : -.75);
}
void walk_local(Simulation &s, double x, double z) {
    const auto p = truck_point(s, x, 0, z); walk_to(s, p.x, p.z);
}
void parked(Simulation &s) {
    require(s.set_forklift_input(0, 0, 0, 0, 1), "park actual truck"); tick(s, 120);
    for (const auto &w : s.forklift_state().wheels)
        require(w.contact && w.normal_force_n > 0 && w.contact_normal.y > .7,
                "all three actual tires have positive supported contact");
}
void drive_x_to(Simulation &s, double x, double z, double max_speed = .7) {
    for (unsigned i = 0; i < 6000; ++i) {
        require(s.forklift_state().controls_reachable, "actual truck floor retains operator");
        const auto p = s.kit_body_position(s.kit_body_index(2440));
        const auto v = s.kit_body_velocity(s.kit_body_index(2440));
        const auto f = forward(s);
        const double dx = x - p.x;
        if (std::abs(dx) < .015 && std::hypot(v.x, v.z) < .035) {
            parked(s);
            require(std::abs(p.z - z) < .035 && f.x > .9995,
                    "pickup settles inside real rig capture and heading tolerance");
            return;
        }
        require(dx > -.10 && f.x > .8, "finite eastbound approach keeps forward heading");
        const double speed = std::clamp(.55 * dx, 0., max_speed);
        const double steer = std::clamp(1.5 * (std::atan(.6 * (z - p.z)) - std::atan2(f.z, f.x)), -.3, .3);
        const double current = v.x * f.x + v.z * f.z;
        const bool brake = current > speed + .035 || dx < 0;
        const double drive = std::clamp(.08 + .7 * (speed - current), 0., .4);
        require(s.set_forklift_input(brake ? 0 : drive, steer, 0, 0, brake), "finite pickup alignment drive");
        tick(s);
        if (i % 900 == 0) observe(s, "PRECISE_DRIVE");
    }
    require(false, "finite pickup drive reaches target");
}
void prepare_truck(Simulation &s) {
    begin(s, "ORIGINAL_TRUCK_DRIVE_TO_PICKUP");
    drive_straight_to(s, -105, 1.5);
    drive_straight_to(s, -160, 1.5);
    std::vector<Point> curve{{-100, -160}};
    for (unsigned i = 1; i <= 24; ++i) {
        const double a = 1.5707963267948966 * i / 24;
        curve.push_back({-90 - 10 * std::cos(a), -160 + 10 * std::sin(a)});
    }
    curve.push_back({-84, -150}); follow(s, curve, 1);
    drive_x_to(s, -80, -150);
    begin(s, "FINITE_HYDRAULIC_STOW");
    require(s.set_forklift_input(0, 0, 1, 0, 1), "lift forks before forward tilt");
    for (unsigned i = 0; i < 900 && s.forklift_state().lift_position_m < .35; ++i) tick(s);
    require(s.forklift_state().lift_position_m >= .35, "actual hydraulic lift clears grade");
    require(s.set_forklift_input(0, 0, 0, -1, 1), "finite forward mast stop");
    for (unsigned i = 0; i < 900 && s.forklift_state().tilt_angle_rad > -.138; ++i) tick(s);
    require(s.forklift_state().tilt_angle_rad < -.138, "actual mast reaches forward transport stop");
    parked(s);
    walk_local(s, -2, -1.65);
    require(!s.forklift_state().controls_reachable, "ordinary departure revokes truck controls");
}
void cab(Simulation &s) {
    // All transit stays west of the actual foundation before entering its open western door.
    walk_to(s, -69, s.snapshot().player_position.z);
    walk_to(s, -69, -150);
    walk_to(s, -68.5, -150);
    require(s.set_facing(1, 0), "face actual booth door");
    walk_to(s, -66.5, -150, .3);
    require(s.yard_crane_state().controls_reachable && s.snapshot().support_entity_id == 1450,
            "actual cab floor authorizes crane controls");
}
void leave_cab(Simulation &s) {
    require(s.set_yard_crane_input(0, 0, 0), "neutral crane before leaving cab");
    walk_to(s, -69, -150);
    require(!s.yard_crane_state().controls_reachable, "walking off cab floor revokes crane controls");
}
void align_empty(Simulation &s) {
    begin(s, "CAB_EMPTY_RIG_ALIGNMENT"); cab(s);
    auto c = s.yard_crane_state();
    double ex = 0, ez = 0;
    for (unsigned leg = 0; leg < 4; ++leg) {
        const auto e = eye(s, leg);
        ex += (e.x - c.lines[leg + 1].to.x) / 4;
        ez += (e.z - c.lines[leg + 1].to.z) / 4;
    }
    // The trolley target is measured once from actual endpoints. Chasing the
    // swinging spreader would inject energy into its physical pendulum.
    const double yaw = c.slew_angle_rad;
    const double target_trolley = c.trolley_position_m + ex * std::sin(yaw) + ez * std::cos(yaw);
    const double target_yaw = yaw + (ex * std::cos(yaw) - ez * std::sin(yaw)) / c.trolley_position_m;
    unsigned ready = 0;
    for (unsigned i = 0; i < 3600; ++i) {
        c = s.yard_crane_state();
        double excess = -100, worst = 0;
        for (unsigned leg = 0; leg < 4; ++leg) {
            const auto e = eye(s, leg); const auto &l = c.lines[leg + 1];
            worst = std::max(worst, std::sqrt((e.x-l.to.x)*(e.x-l.to.x)+(e.y-l.to.y)*(e.y-l.to.y)+(e.z-l.to.z)*(e.z-l.to.z)));
            excess = std::max(excess, std::sqrt((e.x-l.from.x)*(e.x-l.from.x)+(e.y-l.from.y)*(e.y-l.from.y)+(e.z-l.from.z)*(e.z-l.from.z)) - l.rest_length_m);
        }
        // The physical owner accepts a reached hook within60mm, then checks
        // stock, relative speed and actual obstruction again at each eye.
        // Do not require a stricter fictional centering fixture beforehand.
        const bool aligned = worst < .06 && excess < -.0005;
        ready = aligned ? ready + 1 : 0;
        if (ready >= 90) { require(s.set_yard_crane_input(0, 0, 0), "hold actual aligned rig"); tick(s, 90); return; }
        const double trolley_error = target_trolley - c.trolley_position_m;
        const double yaw_error = std::remainder(target_yaw - c.slew_angle_rad, 6.283185307179586);
        const double trolley = std::abs(trolley_error) > .001 ? std::clamp(4 * trolley_error - 4 * c.trolley_target_mps, -.2, .2) : 0;
        const double slew = std::abs(yaw_error) > .0001 ? std::clamp(10 * yaw_error - 4 * c.slew_target_rad_s, -.1, .1) : 0;
        require(s.set_yard_crane_input(slew, trolley, excess > -.001 ? -.02 : 0), "finite empty rig correction");
        tick(s);
        if (i % 450 == 0) { observe(s, "ALIGN_RIG"); std::cout << "ENDPOINT worst=" << worst << " excess=" << excess << std::endl; }
    }
    require(false, "cab controls align four real fixed-stock sling endpoints");
}
void rig_legs(Simulation &s, bool attach) {
    begin(s, attach ? "WALK_AND_ATTACH_FOUR_REAL_EYES" : "SUPPORTED_LOCAL_DETACH_FOUR_LEGS");
    // Both sides approached around the open rear, never through truck steel.
    for (unsigned side = 0; side < 2; ++side) {
        const double x = side ? 1.15 : -1.15;
        // Walk a clear perimeter outside the actual rear counterweight and
        // low side treads; a diagonal across their capsule envelope is blocked.
        if (side == 0) {
            walk_local(s, 2, -3);
            walk_local(s, -2, -3);
        } else {
            walk_local(s, -1.65, -.75);
            walk_local(s, -2, -3);
            walk_local(s, 2, -3);
        }
        walk_local(s, side ? 1.65 : -1.65, -.75);
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned leg = (row ? 0 : 2) + side;
            walk_local(s, x, row ? .75 : -.75);
            tick(s, 60);
            require(s.yard_crane_state().rigging_reachable && s.yard_crane_state().rigging_leg == int(leg),
                    "actual shoulder reach and clear ray select intended steel eye");
            const auto before = s.yard_crane_state().attached_mask;
            require(attach ? s.request_yard_crane_attach() : s.request_yard_crane_detach(), "local physical leg request queued");
            tick(s, 90);
            const unsigned expected = attach ? before | (1U << leg) : before & ~(1U << leg);
            if (s.yard_crane_state().attached_mask != expected) {
                const auto c = s.yard_crane_state();
                const auto e = eye(s, leg);
                const auto &line = c.lines[leg + 1];
                std::cout << "RIG_RECEIPT leg=" << leg
                          << " eye=" << e.x << ',' << e.y << ',' << e.z
                          << " loose=" << line.to.x << ',' << line.to.y << ',' << line.to.z
                          << " top=" << line.from.x << ',' << line.from.y << ',' << line.from.z
                          << " stock=" << line.rest_length_m << " force=" << line.force_n << std::endl;
                for (const auto &wheel : s.forklift_state().wheels)
                    std::cout << "RIG_WHEEL contact=" << wheel.contact << " force=" << wheel.normal_force_n
                              << " normal=" << wheel.contact_normal.x << ',' << wheel.contact_normal.y << ',' << wheel.contact_normal.z << std::endl;
            }
            require(s.yard_crane_state().attached_mask == expected, "only reached individual leg changes physical state");
            observe(s, attach ? "INDIVIDUAL_LEG_ATTACHED" : "INDIVIDUAL_LEG_DETACHED");
        }
        walk_local(s, side ? 1.65 : -1.65, -.75);
    }
    walk_local(s, 2, -3);
    require(s.yard_crane_state().attached_mask == (attach ? 15 : 0), "four-leg transaction complete");
}
void hoist_truck_to(Simulation &s, double height) {
    unsigned settled = 0;
    for (unsigned i = 0; i < 12000; ++i) {
        const auto p = s.kit_body_position(s.kit_body_index(2440));
        const auto v = s.kit_body_velocity(s.kit_body_index(2440));
        const double error = height - p.y;
        settled = std::abs(error) < .025 && std::abs(v.y) < .04 ? settled + 1 : 0;
        if (settled >= 90) { require(s.set_yard_crane_input(0, 0, 0), "neutral loaded hoist"); tick(s, 90); return; }
        const double input = std::abs(error) < .015 ? 0 : std::clamp(2 * error - 2 * v.y, -.65, .65);
        require(s.set_yard_crane_input(0, 0, input), "finite loaded hoist motion"); tick(s);
        if (i % 900 == 0) observe(s, "LOADED_HOIST");
    }
    require(false, "loaded hoist reaches actual transfer height");
}
void transfer_dock(Simulation &s) {
    begin(s, "RETURN_CAB_AND_LIFT_SAME_TRUCK");
    // Return west of the freight, then walk the real grade into the booth.
    walk_to(s, -76, -154); cab(s);
    const auto source = s.yard_crane_state().hoist_source_used_j;
    // Carry the entire real assembly above the side guardrails before slewing.
    // Derive height from actual dock/truck geometry, including tire envelopes;
    // .35m is a conservative operating clearance for the freely hanging load.
    const double dock_top = body_vertical_extent(s, 1451)[1];
    const double below_chassis = s.kit_body_position(s.kit_body_index(2440)).y - truck_bottom(s);
    const double height = dock_top + below_chassis + .35;
    std::cout << "TRANSFER_CLEARANCE rail_top=" << dock_top << " below_chassis=" << below_chassis
              << " hoist_chassis_target=" << height << std::endl;
    hoist_truck_to(s, height);
    require(truck_bottom(s) > dock_top + .15, "whole suspended truck clears actual dock guardrails before slew");
    require(s.yard_crane_state().hoist_source_used_j > source, "loaded elevation consumes actual finite source work");
    begin(s, "LOADED_SLEW_AND_TROLLEY_TO_11M_DOCK");
    const double target_yaw = std::atan2(29., 10.);
    // Hook projection includes the actual rearward suspension offset of the
    // initially +X truck. Correct final truck position from its measured pose.
    double target_radius = std::hypot(29. - .419814, 10.);
    unsigned ready = 0;
    for (unsigned i = 0; i < 18000; ++i) {
        const auto c = s.yard_crane_state();
        const double da = std::remainder(target_yaw - c.slew_angle_rad, 6.283185307179586);
        const double dr = target_radius - c.trolley_position_m;
        ready = std::abs(da) < .0005 && std::abs(dr) < .01 ? ready + 1 : 0;
        if (ready >= 180) break;
        require(i + 1 < 18000, "finite loaded slew and trolley arrive over receiving dock");
        const double slew = std::abs(da) < .0003 ? 0 : std::clamp(3 * da - 3 * c.slew_target_rad_s, -.5, .5);
        const double trolley = std::abs(dr) < .004 ? 0 : std::clamp(2 * dr - 3 * c.trolley_target_mps, -.5, .5);
        require(s.set_yard_crane_input(slew, trolley, 0), "finite physical loaded slew/trolley"); tick(s);
        if (i % 900 == 0) observe(s, "LOADED_TRANSFER");
    }
    require(s.set_yard_crane_input(0, 0, 0), "stop over actual receiving dock"); tick(s, 450);
    const auto p = s.kit_body_position(s.kit_body_index(2440));
    require(p.x > -34 && p.x < -29 && p.z > -142 && p.z < -138, "actual truck is inside dock receiving footprint");
    begin(s, "ACTUAL_THREE_TIRE_DOCK_LANDING");
    require(s.set_yard_crane_input(0, 0, -.25), "lower truck onto real11m dock");
    unsigned supported = 0;
    for (unsigned i = 0; i < 3600; ++i) {
        tick(s); bool all = true;
        for (const auto &w : s.forklift_state().wheels)
            all = all && w.contact && w.normal_force_n > 0 && w.contact_normal.y > .7 && std::abs(w.contact_position.y - 11) < .05;
        supported = all ? supported + 1 : 0;
        if (supported >= 90) break;
        if (i + 1 == 3600) {
            std::cout << "LANDING_GEOMETRY fork_bottom=" << body_vertical_extent(s, 2443)[0]
                      << " truck_bottom=" << truck_bottom(s) << std::endl;
            for (const auto &wheel : s.forklift_state().wheels)
                std::cout << "LANDING_WHEEL contact=" << wheel.contact << " load=" << wheel.normal_force_n
                          << " point=" << wheel.contact_position.x << ',' << wheel.contact_position.y << ',' << wheel.contact_position.z
                          << " center_y=" << wheel.position.y << std::endl;
        }
        require(i + 1 < 3600, "all three real tires land on actual dock surface");
    }
    // Pay out a measured additional small length until each line is slack.
    for (unsigned i = 0; i < 900; ++i) {
        const auto c = s.yard_crane_state(); bool slack = true;
        for (unsigned leg = 1; leg < 5; ++leg) slack = slack && c.lines[leg].force_n <= 150;
        if (slack && i > 90) break;
        tick(s); require(i + 1 < 900, "actual dock unloads sling forces");
    }
    require(s.set_yard_crane_input(0, 0, 0), "hold actual supported load"); tick(s, 120);
}
void board_again(Simulation &s) {
    walk_local(s, -2, -1.65);
    for (unsigned i = 0; i < 900; ++i) {
        const auto p = truck_point(s, 0, 0, -1.65);
        const auto state = s.snapshot();
        if (s.forklift_state().controls_reachable) { require(s.set_move_input(0, 0), "stand in truck cab"); tick(s, 60); return; }
        require(s.set_move_input(std::clamp(1.2*(p.x-state.player_position.x)-.45*state.player_linear_velocity.x,-.3,.3),
                                 std::clamp(1.2*(p.z-state.player_position.z)-.45*state.player_linear_velocity.z,-.3,.3)), "ordinary boarding on transferred truck");
        tick(s);
    }
    require(false, "ordinary footsteps reboard same original transferred truck");
}
void travel(Simulation &s, double x, double z) {
    // Long approaches retain the accepted walking controller's finite bound.
    const auto start = s.snapshot().player_position;
    const unsigned pieces = unsigned(std::ceil(std::hypot(x-start.x,z-start.z)/6));
    for (unsigned i = 1; i <= pieces; ++i)
        walk_to(s, start.x+(x-start.x)*i/pieces, start.z+(z-start.z)*i/pieces);
}
void reach_dock_on_foot(Simulation &s) {
    begin(s, "PLAYER_EXISTING_CARGO_NET_ACCESS_TO_11M_RING");
    leave_cab(s);
    // Walk around the southern building face to the existing AS-024 cargo
    // net1953. The same Simulation retains the parked crane/truck throughout.
    // Pass north of the real west buttress foot: x[-42,-36], z[-114,-108].
    // The full standing capsule sweeps clear at z=-106.5 before turning east.
    travel(s, -69, -106.5); travel(s, 20, -106.5); travel(s, 20, -114);
    require(s.set_facing(0,-1) && s.set_move_input(0,-.25), "ordinary cargo net approach");
    for (unsigned i=0; i<360 && !(s.snapshot().grip_available && s.snapshot().grip_entity_id==1953); ++i) tick(s);
    require(s.set_move_input(0,0), "stop at real cargo net grip");
    require(s.snapshot().player_grounded && s.snapshot().grip_available && s.snapshot().grip_entity_id==1953,
            "actual grade approach earns existing material net grip");
    require(s.request_traversal(), "Action takes existing material net"); tick(s);
    require(s.snapshot().traversal_state==TraversalState::Climbing && s.snapshot().traversal_hand_constraint_count==2,
            "two physical hands carry actual player on cargo net");
    require(s.set_move_input(0,-1), "ordinary held ascent on existing cargo mesh");
    bool transfer=false, received=false;
    for(unsigned i=0;i<1800;++i) {
        const auto before=s.snapshot();
        if(!transfer && before.traversal_support_entity_id==2952 && before.traversal_state==TraversalState::Climbing) {
            require(s.request_traversal(), "fresh Action at actual cargo net receiving transfer"); transfer=true;
        }
        tick(s);
        require(s.snapshot().player_gravity_factor==1, "material net ascent retains actual gravity");
        if(s.snapshot().player_grounded && s.snapshot().player_position.y>11.7) {received=true;break;}
        if(i%300==0) observe(s,"PERSONNEL_CARGO_NET_ASCENT");
    }
    require(s.set_move_input(0,0), "stand on actual cargo receiving support");tick(s,45);
    require(transfer && received && s.snapshot().player_grounded && s.snapshot().traversal_state==TraversalState::None,
            "existing material net provides actual11m personnel footing");
    travel(s,18.5,-120.5);travel(s,18.5,-126);
    require(s.snapshot().support_entity_id==Simulation::kTowerEntityId,
            "existing cargo receiver connects to actual first ring");
    begin(s,"WALK_EXISTING_RING_TO_FREIGHT_DOCK");
    travel(s,-20,-126);travel(s,-24,-130);travel(s,-24,-139);
    travel(s,-27,-139);travel(s,-27,-137.3);travel(s,-34,-137.3);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id==1451,
            "ordinary ring movement reaches actual freight receiving dock");
}
void onward_and_retry(Simulation &s) {
    begin(s, "REBOARD_AND_DRIVE_INTO_EXISTING_RING"); board_again(s);
    const auto start = s.kit_body_position(s.kit_body_index(2440));
    // Same rear-steer driver follows the actual dock center through its mouth.
    follow(s, {{start.x,start.z},{-23,-140}}, 1);
    const auto t = s.kit_body_position(s.kit_body_index(2440));
    require(t.x > -25.5 && t.y > 11.4, "same original truck passes onto existing ring at11m");
    parked(s);
    for (const auto &w : s.forklift_state().wheels)
        require(w.contact_position.x > -25.5 && std::abs(w.contact_position.y - 11) < .05,
                "all actual tires cross the dock overlap onto the existing ring");
    walk_local(s, -2, -1.65);
    walk_to(s, -24.65, -137);
    walk_to(s, -22, -137);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == Simulation::kTowerEntityId,
            "actual existing ring supports onward player footing");
    walk_to(s, -22, -135);
    require(s.snapshot().support_entity_id == Simulation::kTowerEntityId, "ordinary onward ring movement stays supported");
    begin(s, "PHYSICAL_DOCK_RETRY_WITHOUT_RESET");
    walk_to(s, -24.65, -137); board_again(s);
    const auto now = s.kit_body_position(s.kit_body_index(2440));
    follow(s, {{now.x,now.z},{-31,-140}}, -1);
    parked(s);
    require(s.kit_body_position(s.kit_body_index(2440)).x < -29 && s.yard_crane_state().attached_mask == 0,
            "same intact truck returns to actual dock pickup for feasible next lift");
}
} // namespace
int main() {
    Simulation s(InitialSpawn::ExteriorGrade, WorldContent::Slingshot); active = &s;
    begin(s, "SINGLE_INITIAL_GRADE_STAGE_AND_BOARD"); board(s);
    require(s.forklift_state().chassis_entity == 2440, "route uses original yard truck identity");
    prepare_truck(s); align_empty(s); leave_cab(s);
    walk_to(s, -82, -154); rig_legs(s, true);
    begin(s, "RIGGED_TRUCK_NATIVE_DRIVE_INTERLOCK"); board_again(s);
    require(s.set_forklift_input(1, .5, 1, 1, 0), "attempt real rigged truck demands"); tick(s, 18);
    const auto locked = s.forklift_state();
    require(locked.applied_drive == 0 && locked.applied_brake == 1 &&
            locked.lift_target_mps == 0 && locked.tilt_target_rad_s == 0,
            "native crane interlock parks every truck actuator while rigged");
    parked(s); walk_local(s, -2, -1.65);
    transfer_dock(s);
    reach_dock_on_foot(s); rig_legs(s, false); onward_and_retry(s);
    std::cout << "PASS public same-truck freight route: original yard drive, actual stow, cab alignment, four local legs, loaded11m landing, local release, existing ring drive/onward footing and physical retry\n";
}
