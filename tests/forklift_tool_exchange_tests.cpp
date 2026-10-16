#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *why) {
    if (!value) {
        std::cerr << "FAIL public fork exchange: " << why << std::endl;
        std::exit(1);
    }
}
void tick(Simulation &s, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native public tick accepted");
        require(s.snapshot().death_count == 0, "exchange retains living operator");
    }
}
void observe(Simulation &s, const char *label, std::uint64_t entity = 2443) {
    const auto p = s.kit_body_position(s.kit_body_index(2440));
    const auto t = s.kit_body_position(s.kit_body_index(entity));
    const auto v = s.kit_body_velocity(s.kit_body_index(entity));
    const auto f = s.forklift_state();
    std::cout << label << " chassis=" << p.x << ',' << p.y << ',' << p.z
              << " tool=" << t.x << ',' << t.y << ',' << t.z
              << " tool_velocity=" << v.x << ',' << v.y << ',' << v.z
              << " coupled=" << f.coupled << " generation=" << f.coupling_generation
              << " identity=" << f.tool_entity << " reachable=" << f.controls_reachable
              << " detach=" << f.can_detach << " couple=" << f.can_couple
              << " refusal=" << unsigned(f.tool_refusal) << " lift_force=" << f.lift_force_n
              << " lift=" << f.lift_position_m << " tilt=" << f.tilt_angle_rad
              << " speed=" << f.forward_speed_mps << std::endl;
}
#include "forklift_public_driver.hpp"

double distance(Vector3 a, Vector3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
}
double angle(Quaternion a, Quaternion b) {
    return 2 * std::acos(std::clamp(std::abs(a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w), 0., 1.));
}
void parked(Simulation &s, unsigned count = 45) {
    require(s.set_forklift_input(0, 0, 0, 0, 1), "neutral demands and full actual parking brake");
    tick(s, count);
    require(s.forklift_state().controls_reachable, "actual rear operator footing retained");
}
void supported_truck(Simulation &s) {
    const auto f = s.forklift_state();
    require(f.controls_reachable && s.snapshot().support_entity_id == 2440,
            "operator remains supported by the actual truck");
    for (const auto &wheel : f.wheels)
        require(wheel.contact && wheel.normal_force_n > 1, "all three actual tires retain loaded contact");
}
void same_tool(Simulation &s, bool coupled, std::uint64_t generation) {
    const auto f = s.forklift_state();
    require(f.tool_entity == 2443 && f.coupled == coupled && f.coupling_generation == generation,
            "same tool identity and exact expected coupling generation");
}
void lift_to(Simulation &s, double height) {
    for (unsigned i = 0; i < 1800; ++i) {
        const auto f = s.forklift_state();
        require(f.controls_reachable, "finite hydraulics require actual cab footing");
        const double error = height - f.lift_position_m;
        if (std::abs(error) < .002) { parked(s); return; }
        const double demand = std::clamp(5 * error, -.35, .35);
        require(s.set_forklift_input(0, 0, demand, 0, 1), "finite public lift command");
        tick(s);
    }
    observe(s, "LIFT_FAILED");
    require(false, "finite hydraulic lift reaches requested clearance");
}

double blade_axis_y(Simulation &s, std::uint64_t entity = 2443) {
    const auto q = s.kit_body_rotation(s.kit_body_index(entity));
    return 2*(q.y*q.z-q.w*q.x);
}
void level_blades(Simulation &s, std::uint64_t entity = 2443) {
    // The mast angle is chassis-relative; tire compression pitches the truck.
    // Read the real blade/carriage orientation and use the ordinary tilt lever.
    for (unsigned i = 0; i < 900; ++i) {
        const double y = blade_axis_y(s, entity);
        require(s.set_forklift_input(0, 0, 0, std::clamp(-20*y, -.3, .3), 1),
                "finite tilt levels actual world-space blades before contact");
        tick(s);
        if (i % 180 == 179) parked(s, 45);
        if (std::abs(y) < .0002 && i > 90) break;
    }
    parked(s, 90);
    require(std::abs(blade_axis_y(s, entity)) < .001,
            "actual blade plane is level before loading the parking steel");
}

// An ordinary analog driver tracks a straight parking line. A virtual point
// ahead in the direction of travel gives rear-steer pursuit; no body state is
// written. The long approach removes heading error before the final crawl.
void parking_line(Simulation &s, double x, double z, double direction,
                  double speed_limit = .45, const Vector3 *loose_tool = nullptr) {
    const auto chassis = s.kit_body_index(2440);
    for (unsigned i = 0; i < 9000; ++i) {
        const auto p = s.kit_body_position(chassis);
        const auto v = s.kit_body_velocity(chassis);
        const auto f = forward(s);
        require(s.forklift_state().controls_reachable, "parking driver keeps physical operator footing");
        if (loose_tool) {
            require(distance(s.kit_body_position(s.kit_body_index(2443)), *loose_tool) < .025,
                    "loose fork frame rests independently throughout withdrawal and return");
        }
        require(f.z < -.96, "parking approach keeps actual forks facing the cradle");
        const double error = direction * (z - p.z) / f.z;
        const double speed_now = direction * (v.x*f.x + v.z*f.z);
        if (std::abs(z-p.z) < .012 && std::abs(speed_now) < .025) {
            parked(s, 60);
            observe(s, "PARKING_LINE_FINISHED");
            require(std::abs(s.kit_body_position(chassis).x-x) < .025,
                    "finite rear steering centers actual chassis on parking line");
            require(std::abs(forward(s).x) < .015, "actual return heading fits coupler angular gate");
            return;
        }
        require(error > -.08, "finite braking stops without driving beyond parking target");
        const double lookahead = 1.1;
        const double tx = x - p.x;
        const double tz = direction * (f.z < 0 ? -lookahead : lookahead);
        const double lateral = f.z*tx - f.x*tz;
        const double steer = std::clamp(-std::atan2(2*1.65*lateral, tx*tx+tz*tz)/.9, -.45, .45);
        const double speed = std::min(speed_limit, .55 * std::max(0., error));
        const double brake = speed_now > speed + .025 || error < .006 ? 1 : 0;
        const double drive = direction * std::clamp(.07 + .8*(speed-speed_now), 0., .32);
        require(s.set_forklift_input(brake ? 0 : drive, steer, 0, 0, brake),
                "finite public crawl, steer and brake inputs");
        tick(s);
        if (i % 900 == 0) observe(s, "PARKING_LINE");
    }
    observe(s, "PARKING_LINE_FAILED");
    require(false, "finite public driving reaches precise exchange parking pose");
}
void no_action_snap(Simulation &s, Vector3 before, Quaternion rotation, Vector3 velocity) {
    const auto tool = s.kit_body_index(2443);
    const auto after = s.kit_body_position(tool);
    const Vector3 expected{before.x+velocity.x*Simulation::kFixedStepSeconds,
                           before.y+velocity.y*Simulation::kFixedStepSeconds,
                           before.z+velocity.z*Simulation::kFixedStepSeconds};
    require(distance(after, expected) < .01 && angle(rotation, s.kit_body_rotation(tool)) < .01,
            "exchange action retains natural tool pose without capture or release snap");
    require(distance(s.kit_body_velocity(tool), velocity) < .15,
            "exchange action preserves natural tool momentum within one solved support tick");
}
} // namespace

int main() {
    Simulation s(InitialSpawn::ExteriorGrade, WorldContent::Slingshot);
    const auto original = s.forklift_state();
    require(original.available && original.coupled && original.tool_entity == 2443 &&
            s.kit_body_index(2443) != Simulation::kKitNone && s.kit_body_index(2447) != Simulation::kKitNone,
            "normal production world contains original coupled forks and actual cradle");
    // Queue acceptance cannot authorize remote mechanical operation.
    require(s.request_forklift_detach(), "remote detach intent may be queued");
    same_tool(s, true, original.coupling_generation);
    tick(s, 2);
    same_tool(s, true, original.coupling_generation);
    require(!s.forklift_state().can_detach && !s.forklift_state().controls_reachable,
            "remote normal-world request cannot detach tool");

    board(s); // The one allowed player-only initial grade staging operation.
    const auto tool = s.kit_body_index(2443);
    const auto body_count = s.kit_body_count();
    const auto generation = s.forklift_state().coupling_generation;
    lift_to(s, .35);
    require(!s.forklift_state().can_detach && s.forklift_state().tool_refusal == 10,
            "raised tool has no qualifying static support");
    require(s.request_forklift_detach(), "unsupported detach intent queued once");
    tick(s);
    same_tool(s, true, generation);
    parked(s);
    same_tool(s, true, generation);

    // Forward semicircle: initial +Z heading becomes -Z, with an aisle east
    // of the retained cargo lane. Radius6 avoids the actual nearby crate.
    std::vector<Point> arc{{-100, -56}};
    constexpr double pi = 3.14159265358979323846;
    for (unsigned i = 1; i <= 36; ++i) {
        const double t = pi*i/36;
        arc.push_back({-94-6*std::cos(t), -56+6*std::sin(t)});
    }
    arc.push_back({-88, -62});
    follow(s, arc, 1);
    parking_line(s, -88, -67.37, 1);
    supported_truck(s);
    same_tool(s, true, generation); // Old remote/unsupported requests expired.
    level_blades(s);
    lift_to(s, .15);
    level_blades(s);

    // Lower gently with actual world-space tilt feedback. Coarse lowering
    // before leveling can press a blade edge down while its center stays high.
    // Each neutral interval asks the unchanged physical owner's readiness.
    bool supported = false;
    for (unsigned i = 0; i < 240; ++i) {
        parked(s, 18);
        const auto readiness = s.forklift_state();
        if (readiness.can_detach) { supported = true; break; }
        if (i % 15 == 0) observe(s, "PARKING_SUPPORT_REFUSAL");
        require(readiness.tool_refusal == 10 || readiness.tool_refusal == 12,
                "neutral parking exposes actual support or coupler-load refusal");
        require(readiness.lift_position_m > .07, "support reached before carriage bottom stop");
        const double lift = readiness.tool_refusal == 12 ? .01 : -.01;
        for (unsigned j = 0; j < 9; ++j) {
            require(s.set_forklift_input(0, 0, lift,
                        std::clamp(-20*blade_axis_y(s), -.15, .15), 1),
                    "finite fine lift and tilt settle real fork material points on steel");
            tick(s);
        }
    }
    parked(s, 60);
    supported = supported || s.forklift_state().can_detach;
    observe(s, "ON_REAL_PARKING_STEEL");
    require(supported, "actual parked fork support grants derived detach readiness");
    same_tool(s, true, generation);

    // A rejected request must not survive and fire after commands become legal.
    require(s.set_forklift_input(0, 0, 0, 0, .5), "partial brake is distinct from full parking brake");
    require(s.request_forklift_detach(), "unparked detach intent queued");
    tick(s);
    same_tool(s, true, generation);
    require(s.forklift_state().tool_refusal == 5 && !s.forklift_state().can_detach,
            "readiness and action both reject partial parking brake");
    parked(s, 60);
    require(s.forklift_state().can_detach, "full brake restores true supported readiness");
    same_tool(s, true, generation);

    require(s.request_forklift_detach(), "ready detach can be cancelled before solve");
    s.cancel_forklift_tool_request();
    tick(s);
    same_tool(s, true, generation);
    require(s.forklift_state().can_detach, "cancellation clears intent without changing physical readiness");

    const auto parking = s.kit_body_position(s.kit_body_index(2440));
    const double parking_lift = s.forklift_state().lift_position_m;
    auto before = s.kit_body_position(tool);
    auto rotation = s.kit_body_rotation(tool);
    auto velocity = s.kit_body_velocity(tool);
    require(s.request_forklift_detach(), "supported local detach queued");
    same_tool(s, true, generation);
    tick(s);
    same_tool(s, false, generation+1);
    no_action_snap(s, before, rotation, velocity);
    parked(s, 60);
    const auto resting_tool = s.kit_body_position(tool);
    const auto resting_rotation = s.kit_body_rotation(tool);
    require(std::abs(resting_tool.y-.25) < .02, "released fork blades rest at actual steel top height");
    parking_line(s, parking.x, parking.z+2.4, -1, .35, &resting_tool);
    same_tool(s, false, generation+1);
    supported_truck(s);
    require(s.kit_body_position(s.kit_body_index(2440)).z-parking.z > 2,
            "truck physically withdraws over two meters from independent tool");
    require(!s.forklift_state().can_couple, "withdrawn carriage cannot remotely capture forks");
    require(s.request_forklift_couple(), "distant couple intent queued once");
    tick(s);
    same_tool(s, false, generation+1);
    parked(s, 90);
    require(distance(s.kit_body_position(tool), resting_tool) < .025 &&
            angle(s.kit_body_rotation(tool), resting_rotation) < .015 &&
            distance(s.kit_body_velocity(tool), {}) < .03,
            "same loose tool remains independently stable on static parking steel");

    lift_to(s, parking_lift);
    level_blades(s, 2442);
    parking_line(s, parking.x, parking.z, 1, .25, &resting_tool);
    parked(s, 90);
    observe(s, "REALIGNED_WITH_ORIGINAL_TOOL");
    same_tool(s, false, generation+1); // Distant request expired before return.
    require(s.forklift_state().can_couple, "actual slow return fits unchanged native hardware gates");
    supported_truck(s);
    before = s.kit_body_position(tool);
    rotation = s.kit_body_rotation(tool);
    velocity = s.kit_body_velocity(tool);
    require(s.request_forklift_couple(), "local aligned supported coupling queued");
    same_tool(s, false, generation+1);
    tick(s);
    same_tool(s, true, generation+2);
    no_action_snap(s, before, rotation, velocity);
    lift_to(s, parking_lift+.30);
    require(s.kit_body_position(tool).y > resting_tool.y+.24, "recoupled actual forks lift clear of cradle");
    const auto raised_tool = s.kit_body_position(tool);
    const auto raised_truck = s.kit_body_position(s.kit_body_index(2440));
    parking_line(s, parking.x, parking.z+2.4, -1, .35);
    same_tool(s, true, generation+2);
    require(s.kit_body_count() == body_count && s.kit_body_index(2443) == tool,
            "complete exchange retains one existing tool body without spawning duplicates");
    supported_truck(s);
    const auto transported = s.kit_body_position(tool);
    const auto truck = s.kit_body_position(s.kit_body_index(2440));
    require(truck.z-raised_truck.z > 2 && transported.z-raised_tool.z > 2 &&
            std::abs((transported.z-truck.z)-(raised_tool.z-raised_truck.z)) < .04 &&
            transported.y > resting_tool.y+.24,
            "same real coupled tool follows chassis during raised reverse transport");
    observe(s, "PUBLIC_EXCHANGE_COMPLETE");
    std::cout << "PASS normal-world public fork detach, independent rest, real return and recoupled transport\n";
}
