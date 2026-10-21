#include "sim_tests.hpp"

#include <iomanip>
#include <set>

namespace {
using namespace scraperx::sim;

// All worlds use Deck154's constructor seed and authored geometry. Only the
// NPC fixture differs. Player placement is setup only; every recorded tick
// is a real 90 Hz native step driven by this fixed, open-loop input tape.
struct Trial {
    const char *name;
    Vector3 start;
    int ticks;
    bool lip;
    bool top;
};
struct Frame {
    Snapshot player;
    Simulation::MatronFixtureSample physical;
};
using Tape = std::vector<Frame>;
constexpr const char *placements[] = {"far-parked", "distant", "nearby-no-contact",
                                      "same-static-structure", "waiting-ladder-foot"};

void check(bool ok, const Trial &trial, int placement, int tick, const char *what) {
    if (ok) return;
    std::cerr << "FAIL matron-player-independence trial=" << trial.name
              << " placement=" << placements[placement] << " tick=" << tick << " " << what << '\n';
    std::exit(EXIT_FAILURE);
}
bool same(Vector3 a, Vector3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
double distance(Vector3 a, Vector3 b) {
    return std::hypot(horizontal_distance(a, b), a.y - b.y);
}
Vector3 anchor(const Trial &trial, int placement) {
    if (placement == 0) return {-20.0, 155.05, -165.0};
    if (placement == 1) return {-15.0, 155.05, -128.2};
    if (trial.lip) {
        if (placement == 2) return {-8.0, 155.05, -128.2};
        if (placement == 3) return {-5.5, 156.23, -132.5};
        // The C3 riser starts at 143 m; its crossover lip is at 155.18 m.
        return {-7.5, 144.05, -133.5};
    }
    if (placement == 2) return {7.5, 155.05, -130.0};
    if (placement == 3) return {7.05, 177.50, -130.4};
    return {3.5, 155.05, -129.6};
}
void compare(const Frame &a, const Frame &b, const Trial &trial, int placement, int tick) {
    const auto &x = a.player;
    const auto &y = b.player;
    // No tolerance: this deterministic native world must produce
    // identical player scalars and vectors, including the queries before a grip.
    if (!same(x.player_position, y.player_position)) {
        std::cerr << std::setprecision(17) << "  baseline=(" << x.player_position.x << ','
                  << x.player_position.y << ',' << x.player_position.z << ") actual=("
                  << y.player_position.x << ',' << y.player_position.y << ',' << y.player_position.z
                  << ") contacts=" << b.physical.player_contact << '\n';
    }
#define EQ(field) check(x.field == y.field, trial, placement, tick, #field)
#define VEC(field) check(same(x.field, y.field), trial, placement, tick, #field)
    VEC(player_position); VEC(player_linear_velocity); EQ(player_grounded);
    EQ(traversal_state); EQ(traversal_support_entity_id); EQ(traversal_progress);
    VEC(traversal_left_hand); VEC(traversal_right_hand); VEC(traversal_normal);
    VEC(traversal_ledge_point); VEC(traversal_target_point);
    EQ(grip_available); EQ(grip_entity_id); VEC(grip_point);
    EQ(ledge_available); EQ(ledge_entity_id); VEC(ledge_point); EQ(ledge_rise_meters);
    EQ(up_move); EQ(edge_drop_available); VEC(edge_drop_point);
    EQ(climb_count); EQ(accepted_traversal_count); EQ(aborted_traversal_count);
    EQ(hang_slip_count); EQ(player_climb_sliding); EQ(climb_slide_count);
    EQ(support_entity_id); VEC(support_contact_point); VEC(support_point_linear_velocity);
    EQ(death_count); EQ(hit_count);
#undef EQ
#undef VEC
    check(a.physical.hang_arm_limit == b.physical.hang_arm_limit &&
          a.physical.hang_overload_impulse == b.physical.hang_overload_impulse,
          trial, placement, tick, "hang reach limit and grip overload");
}

Tape replay(const Trial &trial, int placement, const Tape *reference) {
    Simulation s(InitialSpawn::Deck154);
    // Held waiting fixtures retain the native Pest stage, fixed creep pace,
    // and constructor seed (Matron 0x9E3779B9); only her anchor varies.
    s.debug_matron_patrol();
    s.debug_hold_matron_hands(1000.0);
    s.debug_fix_matron_pace(0);
    s.debug_launch_player(trial.start, {});
    const auto parked = anchor(trial, placement);
    Tape tape;
    tape.reserve(trial.ticks);
    int climbing = 0, hanging = 0, mantling = 0, hand_changes = 0, static_contacts = 0;
    int shared_contacts = 0, grip_offers = 0;
    std::set<double> rung_heights;
    std::vector<KitPart> rungs;
    const auto route = s.kit_body_index(Simulation::kWellRouteEntityId);
    for (unsigned i = 0; i < s.kit_body_part_count(route); ++i) {
        const auto part = s.kit_body_part(route, i);
        if (part.shape == 0 && std::abs(part.half.y - 0.02) < 1e-5 &&
            std::abs(part.half.x - 0.28) < 1e-5 && std::abs(part.offset.x - 5.0) < 1e-5)
            rungs.push_back(part);
    }
    check(rungs.size() > 60 && !s.kit_body_dynamic(route), trial, placement, -1, "authored static ladder exists");
    double min_clearance = 1e9, max_y = trial.start.y;
    Vector3 first_hang{}, last_hang{}, previous_hand{};
    for (int tick = 0; tick < trial.ticks; ++tick) {
        check(s.debug_matron_fixture(&parked).present, trial, placement, tick, "Matron body exists");
        check(s.set_facing(0.0, 1.0), trial, placement, tick, "facing accepted");
        double x = 0.0, z = 0.0;
        if (trial.lip) {
            z = tick < 90 ? 1.0 : 0.0;
            if (!trial.top && tick >= 90 && tick < 135) x = 1.0;
            if (trial.top && tick == 120)
                check(s.request_jump(), trial, placement, tick, "mantle input accepted");
        } else {
            if (tick == 60) check(s.request_traversal(), trial, placement, tick, "grip command accepted");
            if (tick >= 78 && (!trial.top || tick < 2200)) z = 1.0;
        }
        check(s.set_move_input(x, z), trial, placement, tick, "move accepted");
        const auto advanced = s.advance_frame(Simulation::kFixedStepSeconds);
        check(advanced.accepted, trial, placement, tick, "native step accepted");
        Frame frame{s.snapshot(), s.debug_matron_fixture()};
        const auto &p = frame.player;
        const auto &m = frame.physical;
        check(p.tick_index == static_cast<std::uint64_t>(tick + 1), trial, placement, tick, "one native tick");
        const auto npc = s.matron_state();
        check(m.present && npc.alive && npc.released && !npc.kneeling && npc.state < 10 &&
              !npc.climbing && !npc.on_line && !npc.on_pipe && !npc.clinging &&
              npc.carrying == 0 && npc.takes == 0 && npc.throws == 0 && npc.catches == 0,
              trial, placement, tick, "live upright Matron without indirect constraint loads");
        check(distance(m.position, parked) < 0.10, trial, placement, tick, "valid parked body, no depenetration jump");
        check(!m.player_contact, trial, placement, tick, "measured player/Matron contact must be absent");
        check(m.moving_contacts.empty(), trial, placement, tick, "no indirect dynamic-support loading by Matron");
        if (!m.standing_on_static) {
            std::cerr << "  floor=" << m.floor_entity << " at=" << m.position.x << ',' << m.position.y << ',' << m.position.z
                      << " npc_state=" << int(npc.state) << " static_contacts=";
            for (const auto entity : m.static_contacts) std::cerr << entity << ',';
            std::cerr << '\n';
        }
        check(m.standing_on_static, trial, placement, tick, "Matron feet contact an upward static floor without penetration");
        static_contacts += !m.static_contacts.empty();
        if (tick == 0) {
            std::cout << "  fixture=" << trial.name << '/' << placements[placement] << " static_entities=";
            for (const auto entity : m.static_contacts) std::cout << entity << ',';
            std::cout << std::endl;
        }
        if (p.traversal_support_entity_id != 0 &&
            std::find(m.static_contacts.begin(), m.static_contacts.end(), p.traversal_support_entity_id) != m.static_contacts.end())
            ++shared_contacts;
        grip_offers += p.grip_available;
        // Surface separation of upright capsules: radius sum 0.65 m,
        // combined vertical center segments 1.30 m (0.55 + 0.75).
        const double clearance = std::hypot(horizontal_distance(p.player_position, m.position),
            std::max(0.0, std::abs(p.player_position.y - m.position.y) - 1.30)) - 0.65;
        min_clearance = std::min(min_clearance, clearance);
        check(clearance > 0.05, trial, placement, tick, "genuine capsule clearance");
        check(p.death_count == 0, trial, placement, tick, "player remains alive");
        if (p.player_grounded)
            check(m.player_support_static && same(p.support_point_linear_velocity, {}),
                  trial, placement, tick, "player floor is stationary");
        if (p.traversal_state != TraversalState::None)
            check(m.traversal_support_static, trial, placement, tick, "authored traversal support is static");
        if (p.traversal_state == TraversalState::Climbing) {
            ++climbing;
            check(p.traversal_support_entity_id == Simulation::kWellRouteEntityId,
                  trial, placement, tick, "holds belong to authored 154-176 ladder");
            if (!same(previous_hand, p.traversal_left_hand)) ++hand_changes;
            previous_hand = p.traversal_left_hand;
            for (const auto hand : {p.traversal_left_hand, p.traversal_right_hand}) {
                bool on_rung = false;
                for (const auto &rung : rungs) {
                    // 0.1 mm accommodates the float member-axis rounding;
                    // this is geometry validation, never replay tolerance.
                    if (std::abs(hand.x - rung.offset.x) <= rung.half.x + 1e-4 &&
                        std::abs(hand.y - rung.offset.y) < 1e-4 &&
                        std::abs(hand.z - rung.offset.z) <= rung.half.z + 1e-4) {
                        on_rung = true;
                        rung_heights.insert(rung.offset.y);
                    }
                }
                // Native grips close around member axes, including the two
                // continuous rails. Count distinct horizontal rungs separately.
                const bool on_rail = (std::abs(hand.x - 4.72) < 1e-4 || std::abs(hand.x - 5.28) < 1e-4) &&
                    std::abs(hand.z + 129.05) < 1e-4 && hand.y >= 154.0 && hand.y <= 176.25;
                if (!on_rung && !on_rail) {
                    std::cerr << std::setprecision(12) << "  hand=" << hand.x << ',' << hand.y << ',' << hand.z
                              << " first_rung=" << rungs.front().offset.x << ',' << rungs.front().offset.y << ',' << rungs.front().offset.z << '\n';
                }
                check(on_rung || on_rail, trial, placement, tick, "each hand closes around an authored ladder member");
            }
            check(distance(p.player_position, p.traversal_left_hand) < 1.5 &&
                  distance(p.player_position, p.traversal_right_hand) < 1.5,
                  trial, placement, tick, "actual hands remain physically reachable");
        }
        if (trial.lip && p.traversal_state != TraversalState::None) {
            check(p.traversal_support_entity_id == Simulation::kStackC3RouteEntityId,
                  trial, placement, tick, "hang and mantle use the authored C3 crossover bridge");
            // The native snapshot deliberately clears hand points during
            // Mantling; only Hanging publishes the bridge's actual hand pair.
            if (p.traversal_state == TraversalState::Hanging)
                check(distance(p.player_position, p.traversal_left_hand) < 1.5 &&
                      distance(p.player_position, p.traversal_right_hand) < 1.5,
                      trial, placement, tick, "bridge hands within physical reach");
        }
        if (p.traversal_state == TraversalState::Hanging) {
            if (hanging++ == 0) first_hang = p.traversal_ledge_point;
            last_hang = p.traversal_ledge_point;
        }
        mantling += p.traversal_state == TraversalState::Mantling;
        max_y = std::max(max_y, p.player_position.y);
        if (reference) compare((*reference)[tick], frame, trial, placement, tick);
        tape.push_back(std::move(frame));
    }
    const auto &end = tape.back().player;
    std::cout << "  trial=" << trial.name << " placement=" << placements[placement]
              << " ticks=" << trial.ticks << " climb=" << climbing << " hang=" << hanging
              << " mantle=" << mantling << " hand_changes=" << hand_changes
              << " max_y=" << max_y << " shimmy_m=" << horizontal_distance(first_hang, last_hang)
              << " player_contacts=0 dynamic_contacts=0 static_contact_ticks=" << static_contacts
              << " shared_contact_ticks=" << shared_contacts << " distinct_rungs=" << rung_heights.size()
              << " end_y=" << end.player_position.y << " grounded=" << end.player_grounded
              << " clearance_m=" << min_clearance << std::endl;
    check(static_contacts > trial.ticks / 2, trial, placement, trial.ticks, "Matron rests on real static geometry");
    if (placement == 3) check(shared_contacts > 45, trial, placement, trial.ticks, "Matron and player share the same static body");
    if (trial.lip) {
        check(hanging > 45, trial, placement, trial.ticks, "fixture actually hangs");
        if (trial.top) check(mantling > 0 && end.player_grounded && end.player_position.y > 156.0,
                            trial, placement, trial.ticks, "ledge mantle completes on authored bridge");
        else check(horizontal_distance(first_hang, last_hang) > 0.15,
                   trial, placement, trial.ticks, "fixture actually shimmies");
    } else {
        check(climbing > 150 && hand_changes > 5 && rung_heights.size() > 5 && grip_offers > 0 && max_y > trial.start.y + 2.0,
              trial, placement, trial.ticks, "rung acquisition and upward progress");
        if (trial.top) check(end.player_grounded && end.player_position.y > 177.0 && mantling > 0,
                            trial, placement, trial.ticks, "154 to 176.25 ladder tops out");
    }
    return tape;
}
} // namespace

void run_matron_player_independence() {
    const Trial trials[] = {
        {"ledge-mantle-154", {-6.25, 154.5, -134.05}, 360, true, true},
        {"hang-shimmy-154", {-6.25, 154.5, -134.05}, 240, true, false},
        {"rung-to-rung", {5.0, 155.0, -129.6}, 450, false, false},
        {"ladder-154-176", {5.0, 155.0, -129.6}, 2700, false, true},
    };
    for (const auto &trial : trials) {
        const auto baseline = replay(trial, 0, nullptr);
        for (int placement = 1; placement < 5; ++placement) replay(trial, placement, &baseline);
        std::cout << "PASS scraperx_sim matron-player-independence " << trial.name
                  << ": five placements, exact per-tick equality\n";
    }
}
