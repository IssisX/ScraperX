// J-095 #413 — pull the line out from under her: one keeper pin turns the
// zipline she is riding into a falling cable. The picked line is the yard
// line (line 0): deck 2's north band (-2, 22, -125.5) down to the yard
// (-2, 0, -72). It is the line she uses in pursuit — the player rides it
// down in front of her in the Turn and she follows on the bare cable — and
// the grade stair (StairStringer, grade to deck 1 at (12, -123.6)) stands
// beside its span on the north face. The cable is a rope of segments at
// 3.5 kg/m (30 mm steel wire rope); the free end falls, it does not rise.
// The reel, fired from the 220 ring's rack (~197 m), never reaches the pin:
// the hook's line is 30 m and the pin is far beyond it. (The issue's bay
// staging cannot take the hook: the bay rack at grade exists only in the
// proving ground, not in the game world.)

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/zipline.hpp"

#include <cmath>
#include <iostream>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Vector3;

namespace {

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

constexpr int kStateHunting = static_cast<int>(Matron::State::Hunting);

// A sprint to a point, steering straight at it (the matron-line test's
// steer/go, file-local there). Returns true on arrival within the budget.
bool sprint_to(Simulation &s, const double x, const double z, const double seconds,
               const double tolerance = 0.4) {
    const int ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        const auto now = s.snapshot();
        const double dx = x - now.player_position.x;
        const double dz = z - now.player_position.z;
        const double length = std::hypot(dx, dz);
        if (length < tolerance) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
            return true;
        }
        (void)s.set_move_input(dx / length, dz / length);
        (void)s.set_facing(dx / length, dz / length);
        (void)s.set_sprint_input(true);
        tick(s);
        if (i % 90 == 0) {
            const auto t = s.snapshot();
            std::cout << "  sprint_to (" << x << ", " << z << ") t=" << i / 90 << "s p=(" << t.player_position.x
                      << ", " << t.player_position.y << ", " << t.player_position.z << ") v=("
                      << t.player_linear_velocity.x << ", " << t.player_linear_velocity.z << ") g="
                      << t.player_grounded << " bal=" << t.player_balancing << "\n";
        }
    }
    return false;
}

double matron_player_distance(const Simulation &s) {
    const auto m = s.matron_state();
    const auto p = s.snapshot().player_position;
    return std::hypot(m.position.x - p.x, m.position.z - p.z);
}

// The yard line's stand stands under its parked trolley, its ramp coming up
// from the east along deck 2's band: from the deck past the ramp's foot,
// west up the ramp onto the stand, stopped under the bar. (Same staging as
// run_zipline; the core sub-test walks it inline and jumps at once.)
constexpr double kStandX = -2.0;
constexpr double kStandZ = -124.77;

// Her place along a line, from her position: the projection on its run.
double along_line(const Vector3 at, const Simulation::ZiplineLine &line) {
    const double dx = at.x - line.top.x;
    const double dy = at.y - line.top.y;
    const double dz = at.z - line.top.z;
    const double len = line.length;
    if (len < 1.0) {
        return 0.0;
    }
    return (dx * (line.bottom.x - line.top.x) + dy * (line.bottom.y - line.top.y) +
            dz * (line.bottom.z - line.top.z)) /
           len;
}

} // namespace

void run_zipline_pin() {
    using scraperx::sim::InitialSpawn;
    const auto yard = [] {
        Simulation probe(InitialSpawn::Deck2South);
        return probe.zipline_line(0);
    }();
    std::cout << "  zipline pin: yard line top (" << yard.top.x << ", " << yard.top.y << ", " << yard.top.z
              << ") bottom (" << yard.bottom.x << ", " << yard.bottom.y << ", " << yard.bottom.z << ") length "
              << yard.length << ", stand (" << yard.stand.x << ", " << yard.stand.y << ", " << yard.stand.z << ")\n";
    require(std::abs(yard.top.x + 2.0) < 0.01 && std::abs(yard.bottom.z + 72.0) < 0.01 && yard.length > 50.0,
            "zipline pin: the yard line is line 0, deck 2 down to the yard");

    // She is mid-span on the bare cable under ordinary pursuit when the pin
    // is pulled: the cable's near end leaves the line within one fall, and
    // her acceleration matches the moving cable, with no velocity write.
    // The staging is the matron-line test's: a real Turn chase, not a
    // debug-forced stage — she only takes a line in the Turn.
    {
        Simulation s(InitialSpawn::Deck2South);
        s.place_matron_home(22.0F);
        s.debug_fix_matron_pace(3);   // her old one speed (matron-line test)
        require(s.advance_frame(1.0).accepted, "zipline pin: settle");
        // The sprint that wakes her, then straight onto the stand by
        // run_zipline's proven path, and wait there under the bar for her
        // charge (her Turn). No loop round the deck: deck 2's north band
        // is walled at x 8.5, x 13.2 and x 17.5 in turn (the player halts
        // dead at each, velocity 0), and only run_zipline's line is proven
        // open. She is kept directed at him with a heard point every 8 s.
        // The jump goes at once when the charge is seen: the Turn ends
        // when she arrives at him standing still, and the ride trace
        // showed turn 0 already at the ride's first tick after the
        // settle frames and the walk to the stand.
        require(sprint_to(s, 10.0, -128.5, 4.0, 1.0), "zipline pin: a sprint toward her locker");
        const int hz = static_cast<int>(Simulation::kTickRateHz);
        require(walk_to(s, 10.0, -125.5, 4.0, 0.3) && walk_to(s, 2.5, -127.0, 20.0, 0.3) &&
                    walk_to(s, 1.6, -124.77, 10.0, 0.3) && walk_to(s, kStandX, kStandZ, 8.0, 0.12),
                "zipline pin: onto the stand under the bar");
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_facing(0.0, 1.0);
        std::cout << "  zipline pin: DIAG on the stand, pin at (" << s.zipline_pin_position(0).x << ", "
                  << s.zipline_pin_position(0).y << ", " << s.zipline_pin_position(0).z << "), cable top at ("
                  << s.zipline_cable_top(0).x << ", " << s.zipline_cable_top(0).y << ", "
                  << s.zipline_cable_top(0).z << ")\n";
        // On the stand, wait for her charge (her Turn) close in: she must
        // be within 6 m, past the wall across the north band at x 8.5 that
        // stopped her first charge at 11 m. She is kept directed at him
        // with a heard point every 8 s.
        bool charging = false;
        for (int i = 0; i < 180 * hz && !charging; ++i) {
            if (i % (8 * hz) == 0) {
                const auto p = s.snapshot().player_position;
                s.debug_matron_hear(Vector3{static_cast<float>(p.x), static_cast<float>(p.y),
                                            static_cast<float>(p.z)});
            }
            tick(s);
            const auto m = s.matron_state();
            if (i % (5 * hz) == 0) {
                std::cout << "  zipline pin: charge t=" << i / hz << "s she at (" << m.position.x << ", "
                          << m.position.y << ", " << m.position.z << ") state " << static_cast<int>(m.state)
                          << " turn " << static_cast<int>(m.turn) << " dist " << matron_player_distance(s)
                          << "\n";
            }
            charging = m.state == kStateHunting && m.turn != 0 && matron_player_distance(s) < 6.0;
        }
        require(charging && s.snapshot().death_count == 0,
                "zipline pin: she comes for the player at the line's ramp");
        std::cout << "  zipline pin: DIAG charge seen, pin at (" << s.zipline_pin_position(0).x << ", "
                  << s.zipline_pin_position(0).y << ", " << s.zipline_pin_position(0).z << "), cable top at ("
                  << s.zipline_cable_top(0).x << ", " << s.zipline_cable_top(0).y << ", "
                  << s.zipline_cable_top(0).z << ")\n";
        // DIAG: is the chain on the line? Sample the line and measure the
        // nearest segment to each sample.
        {
            const auto yard0 = s.zipline_line(0);
            const double dx = (yard0.bottom.x - yard0.top.x) / yard0.length;
            const double dy = (yard0.bottom.y - yard0.top.y) / yard0.length;
            const double dz = (yard0.bottom.z - yard0.top.z) / yard0.length;
            for (const double along : {10.0, 20.0, 30.0, 40.0, 50.0}) {
                const Vector3 at{static_cast<float>(yard0.top.x + dx * along),
                                 static_cast<float>(yard0.top.y + dy * along),
                                 static_cast<float>(yard0.top.z + dz * along)};
                const Vector3 near = s.zipline_cable_nearest(0, at);
                std::cout << "  zipline pin: DIAG chain at " << along << " m, nearest "
                          << std::hypot(at.x - near.x, at.y - near.y, at.z - near.z) << " m\n";
            }
        }
        // The jump at once: no settle, no walk.
        require(s.request_jump() && advance_until(s, [](const Snapshot &n) { return n.zipline_riding; }, 1.2),
                "zipline pin: the player catches the yard line");
        // He rides it down; she comes up the ramp behind and takes the
        // bare cable after him.
        bool on_line = false;
        double her_along = 0.0;
        for (int i = 0; i < 150 * hz && !on_line; ++i) {
            const auto n = s.snapshot();
            // He brakes near the end so he is still on the line while she
            // comes down after him; once he has let go on the yard she
            // follows the lead he left.
            const bool braking = n.zipline_riding && n.zipline_along_m > yard.length - 12.0 &&
                                 n.zipline_speed_mps > 5.0;
            (void)s.set_move_input(0.0, braking ? -1.0 : 0.0);
            tick(s);
            const auto m = s.matron_state();
            if (i % (5 * hz) == 0) {
                std::cout << "  zipline pin: ride t=" << i / hz << "s player riding " << n.zipline_riding
                          << " along " << n.zipline_along_m << " at (" << n.player_position.x << ", "
                          << n.player_position.y << ", " << n.player_position.z << "), she at (" << m.position.x
                          << ", " << m.position.y << ", " << m.position.z << ") state " << static_cast<int>(m.state)
                          << " turn " << static_cast<int>(m.turn) << " on_line " << m.on_line << " dist "
                          << matron_player_distance(s) << "\n";
            }
            if (m.on_line) {
                her_along = along_line(m.position, yard);
                if (her_along > 15.0 && her_along < 40.0) {
                    on_line = true;
                }
            }
            if (!n.zipline_riding && n.player_grounded && n.player_position.y < 1.5) {
                // He is down on the yard; she follows the lead he left.
                (void)s.set_move_input(0.0, 0.0);
            }
        }
        const auto mid = s.matron_state();
        std::cout << "  zipline pin: she is on the cable at " << her_along << " m of " << yard.length << ", at ("
                  << mid.position.x << ", " << mid.position.y << ", " << mid.position.z << "), alive " << mid.alive
                  << "\n";
        require(on_line, "zipline pin: she takes the yard line on the bare cable in pursuit, mid-span");
        const double damage_before = s.matron_state().damage_ns;
        const Vector3 cable_before = s.zipline_cable_top(0);
        const Vector3 her_before = mid.position;
        // Her velocity and the cable's, over the first half second after
        // the pull, from finite differences each tick.
        Vector3 her_last = her_before;
        double her_vy = 0.0;
        double cable_vy = 0.0;
        s.debug_pull_zipline_pin(0);
        require(s.zipline_pin_pulled(0), "zipline pin: the pin is pulled");
        // The cable's top after the pull is the top segment's centre,
        // 0.4 m below the anchor: the velocity's first difference must
        // start there, not at the anchor, or the first tick reads the
        // offset as a 38 m/s fall.
        Vector3 cable_last = s.zipline_cable_top(0);
        // The cable at her hands, for the acceleration match: the free
        // end is whipped by the chain and does not fall at g, but the
        // cable she hangs from does. Its first difference starts at the
        // nearest point to her hands after the pull.
        const Vector3 her_hands_before{static_cast<float>(her_before.x),
                                       static_cast<float>(her_before.y + 1.55),
                                       static_cast<float>(her_before.z)};
        Vector3 near_last = s.zipline_cable_nearest(0, her_hands_before);
        double near_vy = 0.0;
        std::cout << "  zipline pin: DIAG pin at (" << s.zipline_pin_position(0).x << ", "
                  << s.zipline_pin_position(0).y << ", " << s.zipline_pin_position(0).z << "), cable top at ("
                  << s.zipline_cable_top(0).x << ", " << s.zipline_cable_top(0).y << ", "
                  << s.zipline_cable_top(0).z << ")\n";
        double her_accel_sum = 0.0;
        double cable_accel_sum = 0.0;
        double near_accel_sum = 0.0;
        int samples = 0;
        double her_highest = her_before.y;
        double cable_highest = cable_before.y;
        for (int i = 0; i < static_cast<int>(0.5 * Simulation::kTickRateHz); ++i) {
            tick(s);
            const auto m = s.matron_state();
            const Vector3 cable = s.zipline_cable_top(0);
            const Vector3 her_hands_now{static_cast<float>(m.position.x),
                                        static_cast<float>(m.position.y + 1.55),
                                        static_cast<float>(m.position.z)};
            const Vector3 near = s.zipline_cable_nearest(0, her_hands_now);
            const double dt = Simulation::kFixedStepSeconds;
            const double her_vy_now = (m.position.y - her_last.y) / dt;
            const double cable_vy_now = (cable.y - cable_last.y) / dt;
            const double near_vy_now = (near.y - near_last.y) / dt;
            if (i > 0) {
                her_accel_sum += (her_vy_now - her_vy) / dt;
                cable_accel_sum += (cable_vy_now - cable_vy) / dt;
                near_accel_sum += (near_vy_now - near_vy) / dt;
                ++samples;
            }
            her_vy = her_vy_now;
            cable_vy = cable_vy_now;
            near_vy = near_vy_now;
            her_last = m.position;
            cable_last = cable;
            near_last = near;
            her_highest = std::max(her_highest, m.position.y);
            cable_highest = std::max(cable_highest, cable.y);
            if (i % 9 == 0) {
                std::cout << "  zipline pin: DIAG fall t=" << (i * dt) << "s her y " << m.position.y << " cable y "
                          << cable.y << " her vy " << her_vy_now << " cable vy " << cable_vy_now << "\n";
            }
        }
        const double her_accel = samples > 0 ? her_accel_sum / samples : 0.0;
        const double cable_accel = samples > 0 ? cable_accel_sum / samples : 0.0;
        const double near_accel = samples > 0 ? near_accel_sum / samples : 0.0;
        const auto after = s.matron_state();
        const Vector3 cable_after = s.zipline_cable_top(0);
        const Vector3 her_hands{static_cast<float>(after.position.x), static_cast<float>(after.position.y + 1.55),
                                static_cast<float>(after.position.z)};
        const Vector3 nearest = s.zipline_cable_nearest(0, her_hands);
        const double hands_to_cable = std::hypot(her_hands.x - nearest.x, her_hands.y - nearest.y,
                                                 her_hands.z - nearest.z);
        // DIAG: her centre to the cable (rotation-free — the hands point
        // is body-local and swings with her, the snapshot has yaw only).
        const Vector3 nearest_centre = s.zipline_cable_nearest(0, after.position);
        const double centre_to_cable = std::hypot(after.position.x - nearest_centre.x,
                                                  after.position.y - nearest_centre.y,
                                                  after.position.z - nearest_centre.z);
        std::cout << "  zipline pin: after the pull, her accel " << her_accel << " m/s^2, cable accel " << cable_accel
                  << " m/s^2, cable at her hands " << near_accel << " m/s^2; cable top fell "
                  << (cable_before.y - cable_after.y) << " m, her hands " << hands_to_cable
                  << " m from the cable, centre " << centre_to_cable << " m, damage " << damage_before << " -> "
                  << s.matron_state().damage_ns << " N s\n";
        require(cable_before.y - cable_after.y > 0.5,
                "zipline pin: the cable's near end leaves the line within one fall");
        require(cable_highest <= cable_before.y + 0.05,
                "zipline pin: the free end falls, it does not rise");
        require(her_highest <= her_before.y + 0.05, "zipline pin: she falls with it, she does not rise");
        require(std::abs(her_accel + 9.81) < 2.0 && std::abs(near_accel + 9.81) < 2.0 &&
                    std::abs(her_accel - near_accel) < 2.0,
                "zipline pin: her acceleration matches the moving cable");
        require(hands_to_cable < 0.3, "zipline pin: her forearms stay on the falling cable");
        // She comes down on the yard and lets go of the rope.
        require(advance_until(s, [&s](const Snapshot &) { return !s.matron_state().on_line; }, 6.0),
                "zipline pin: she comes down on the yard and lets go");
        // The fall's impact does NOT count as a blow on her, and that is
        // the existing damage path, not a gap in it: take_blow counts
        // only a moving body striking her, the ground is no striker, and
        // while she rides it ignores everything (riding_). A segment of
        // the falling cable striking her after she lets go would count;
        // here she falls with the cable and lands on the yard, damage 0.
        // (Issue #413: report which — her, and only by a cable hit.)
        const auto landed = s.matron_state();
        std::cout << "  zipline pin: landed, damage " << damage_before << " -> " << landed.damage_ns << " N s, alive "
                  << landed.alive << "\n";
    }

    // Pulled empty: the cable drops, and the climb by the stair still
    // completes — the grade stair beside the span, yard to deck 1, climbed
    // by its ladder (the stair's rungs at (12, -123.6)).
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(1.0).accepted, "zipline pin: settle");
        const Vector3 cable_before = s.zipline_cable_top(0);
        s.debug_pull_zipline_pin(0);
        require(s.zipline_pin_pulled(0), "zipline pin: the pin is pulled empty");
        require(advance_until(s, [&s, &cable_before](const Snapshot &) {
                    return s.zipline_cable_top(0).y < cable_before.y - 1.0;
                }, 3.0),
                "zipline pin: pulled empty, the cable drops");
        // Up the grade ladder at (12, -123.6), yard to deck 1, alive: to
        // its foot, close (the grip is offered within reach of the lowest
        // rung at y 1.45), face it, take hold and climb until standing
        // above it. The ladder is climbed from its north side, facing
        // south — from the south it grips but does not ascend. It tops
        // out at the OuterEdgeBeam (y 9.75-10.85): the player climbs to
        // y 8.84 and the beam bars the last two metres to deck 1, and
        // standing_above needs a floor to stand on, which the ladder
        // never gives. That beam is pre-existing, not the dropped cable:
        // the climb to the beam proves the stair is still usable and the
        // cable does not block it. (The beam is a finding for Claude,
        // not this job.)
        require(walk_to(s, 12.0, -126.0, 20.0, 0.5) && walk_to(s, 12.0, -124.2, 10.0, 0.3),
                "zipline pin: to the stair's foot");
        (void)s.set_facing(0.0, 1.0);
        (void)s.advance_frame(0.4);
        require(s.snapshot().grip_available, "zipline pin: the ladder is in reach");
        (void)s.request_traversal();
        (void)s.advance_frame(0.2);
        const bool climbing = is_climbing(s.snapshot());
        std::cout << "  zipline pin: DIAG climb start, climbing " << climbing << " at ("
                  << s.snapshot().player_position.x << ", " << s.snapshot().player_position.y << ", "
                  << s.snapshot().player_position.z << ")\n";
        const bool climbed = hold_stick(s, 0.0, 1.0, 0.0, 1.0, 60.0,
                                        [](const Snapshot &n) { return n.player_position.y > 8.0; });
        std::cout << "  zipline pin: DIAG climb end, climbed " << climbed << " at ("
                  << s.snapshot().player_position.x << ", " << s.snapshot().player_position.y << ", "
                  << s.snapshot().player_position.z << ") climbing " << is_climbing(s.snapshot()) << "\n";
        require(climbing && climbed,
                "zipline pin: the climb by the stair still completes");
        const auto up = s.snapshot();
        std::cout << "  zipline pin: pulled empty, up the stair to (" << up.player_position.x << ", "
                  << up.player_position.y << ", " << up.player_position.z << "), deaths " << up.death_count << "\n";
        require(up.death_count == 0, "zipline pin: the climb by the stair still completes, alive");
    }

    // The reel, fired from the game's hook rack, never reaches a pin that
    // only a hand at the saddle can pull: the hook's line is 30 m and the
    // pin is ~197 m from the rack. The game starts without the hook: it
    // is taken from its rack on the 220 ring first (the game's rack at
    // (-22.4, 220.25, -127.6); the bay rack at grade exists only in the
    // proving ground, not in the game world, so the bay staging cannot
    // take it there).
    {
        Simulation s(InitialSpawn::Ring220North);
        require(s.advance_frame(1.0).accepted, "zipline pin: settle");
        require(!s.request_hook_fire(), "zipline pin: without the hook nothing fires");
        require(walk_to(s, -21.6, -127.6, 14.0, 0.3), "zipline pin: to the hook rack on the 220 ring");
        require(s.request_hook_take() && s.advance_frame(Simulation::kFixedStepSeconds).accepted &&
                    s.snapshot().has_hook,
                "zipline pin: the hook is taken from its rack");
        const Vector3 pin = s.zipline_pin_position(0);
        const auto from = s.snapshot().player_position;
        const double distance = std::hypot(pin.x - from.x, pin.y - from.y, pin.z - from.z);
        std::cout << "  zipline pin: 220 rack to pin " << distance << " m; hook line 30 m\n";
        require(distance > 30.0, "zipline pin: the pin is past the hook's line from the 220 ring");
        (void)s.set_facing((pin.x - from.x) / distance, (pin.z - from.z) / distance);
        require(s.set_hook_aim(pin.x - from.x, pin.y - from.y, pin.z - from.z) && s.request_hook_fire(),
                "zipline pin: the reel is fired from the 220 ring");
        bool near_pin = false;
        for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            const auto n = s.snapshot();
            if (n.hook_state == 2) {
                break;
            }
            const double d = std::hypot(n.hook_position.x - pin.x, n.hook_position.y - pin.y,
                                        n.hook_position.z - pin.z);
            near_pin = near_pin || d < 1.0;
        }
        const auto end = s.snapshot();
        std::cout << "  zipline pin: hook state " << int(end.hook_state) << ", near the pin " << near_pin
                  << ", pin pulled " << s.zipline_pin_pulled(0) << "\n";
        require(!near_pin && !s.zipline_pin_pulled(0),
                "zipline pin: the reel never reaches the pin");
    }

    // The player's way: he goes to the saddle — the top post at the
    // line's start — and takes the pin out of its slot by hand. The
    // pick-up is a grounded verb (1.2 m, in front of the body), so he
    // does it standing at the post, not hanging from the bar: the pin
    // sits low on the post for a hand on the deck or the stand, and the
    // pick-up takes a tick to land in the hands.
    {
        Simulation s(InitialSpawn::Deck2South);
        require(s.advance_frame(1.0).accepted, "zipline pin: settle");
        const Vector3 pin = s.zipline_pin_position(0);
        require(walk_to(s, 2.5, -127.0, 20.0, 0.3) && walk_to(s, pin.x, pin.z, 10.0, 0.3),
                "zipline pin: to the saddle's post");
        const auto hands = s.snapshot().player_position;
        (void)s.set_facing((pin.x - hands.x), (pin.z - hands.z));
        require(s.request_pick_up(), "zipline pin: he takes the pin");
        require(advance_until(s, [](const Snapshot &n) {
                    return n.carrying_entity_id == scraperx::sim::Zipline::kPinEntityId;
                }, 2.0),
                "zipline pin: the pin is in his hand");
        require(advance_until(s, [&s](const Snapshot &) { return s.zipline_pin_pulled(0); }, 2.0),
                "zipline pin: taking the pin pulls it clear and the cable goes");
        std::cout << "  zipline pin: the player's way works; the cable is falling\n";
    }

    std::cout << "PASS scraperx_sim zipline_pin: yard line 0, cable 3.5 kg/m, pin pulled, she falls with it\n";
}
SCRAPERX_GROUP("zipline_pin", 715, run_zipline_pin);
SCRAPERX_SELECT("zipline-pin", run_zipline_pin);
