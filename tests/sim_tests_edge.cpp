// Standing still near an edge: the stick let go, the body stays where it is.

#include "sim_tests.hpp"

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

// What a body let go of may move, over the 3 s watched: a millimetre.
constexpr double kStillDriftM = 0.001;
constexpr double kPlayerRadiusM = 0.35;

// One tick of a careful creep onto (x, z) relative to `frame` (a moving
// support's position, or zero): the stick in proportion to what is left,
// never more than `most`.
void creep_tick(Simulation &sim, const double x, const double z, const Vector3 frame, const double most) {
    const auto s = sim.snapshot();
    const double dx = frame.x + x - s.player_position.x;
    const double dz = frame.z + z - s.player_position.z;
    const double d = std::hypot(dx, dz);
    const double stick = std::min(most, 4.0 * d);
    (void)sim.set_move_input(d > 1.0e-4 ? stick * dx / d : 0.0, d > 1.0e-4 ? stick * dz / d : 0.0);
    (void)sim.advance_frame(Simulation::kFixedStepSeconds);
}

Vector3 still_frame(const Simulation &) { return {}; }

// Creeps onto (x, z), relative to `frame`, and holds it there half a second,
// so the body is at rest on it when the stick is let go. True within 1 cm.
template <typename Frame>
bool creep_to(Simulation &sim, const double x, const double z, Frame frame, const double most = 0.12) {
    for (int i = 0; i < 90 * 8 + 45; ++i) {
        creep_tick(sim, x, z, frame(sim), most);
    }
    const auto s = sim.snapshot();
    const auto f = frame(sim);
    return std::hypot(s.player_position.x - f.x - x, s.player_position.z - f.z - z) < 0.01;
}

// From where the body stands, creeps along (dir_x, dir_z) until the support's
// contact point stays behind the centre: the centre has passed the edge and
// the capsule's rim rests on it. That contact's coordinate along the creep.
double find_edge(Simulation &sim, const double dir_x, const double dir_z) {
    for (int i = 0; i < 90 * 20; ++i) {
        (void)sim.set_move_input(0.1 * dir_x, 0.1 * dir_z);
        (void)sim.set_facing(dir_x, dir_z);
        (void)sim.advance_frame(Simulation::kFixedStepSeconds);
        const auto s = sim.snapshot();
        const double lead = (s.player_position.x - s.support_contact_point.x) * dir_x +
                            (s.player_position.z - s.support_contact_point.z) * dir_z;
        if (s.player_grounded && lead > 0.01) {
            (void)sim.set_move_input(0.0, 0.0);
            return s.support_contact_point.x * dir_x + s.support_contact_point.z * dir_z;
        }
    }
    require(false, "edge stand: the creep must find the edge");
    return 0.0;
}

struct Stand {
    double drift = 0.0;     // horizontal travel over the 3 s, relative to the frame
    double toward = 0.0;    // of it, along (dir_x, dir_z)
    bool fell = false;
};

// Lets go of the stick and watches 3 s; the drift relative to `frame`.
template <typename Frame>
Stand let_go(Simulation &sim, const double dir_x, const double dir_z, const double floor_y, Frame frame,
             const char *where, const double offset) {
    (void)sim.set_move_input(0.0, 0.0);
    const auto start = sim.snapshot();
    const auto f0 = frame(sim);
    (void)sim.advance_frame(3.0);
    const auto end = sim.snapshot();
    const auto f1 = frame(sim);
    const double mx = (end.player_position.x - f1.x) - (start.player_position.x - f0.x);
    const double mz = (end.player_position.z - f1.z) - (start.player_position.z - f0.z);
    Stand out;
    out.drift = std::hypot(mx, mz);
    out.toward = mx * dir_x + mz * dir_z;
    out.fell = end.player_position.y < floor_y || end.death_count > start.death_count;
    if (std::getenv("SCRAPERX_EDGE_DETAIL") != nullptr) {
        std::cout << "  " << where << " offset=" << offset << " drift_m=" << out.drift << " toward_m=" << out.toward
                  << " fell=" << out.fell << '\n';
    }
    return out;
}
}  // namespace

// The owner, playtesting: standing anywhere close to an edge, the body slid
// off toward it as if on ice. The capsule's rounded foot rests on the edge
// once the centre is past it; that contact's normal tilts out over the drop,
// and the frictionless body was pushed along it by the solve every tick, the
// feet holding it only through the velocity locomotion sets before the solve.
// Let go at a set distance in from an edge (or past it, on the rim), the body
// stays put: on deck 4's south edge 44 m up, on and below the yard's kerb (a
// step down, a step up), on the scaffold boards over the well, near the edge
// of a moving support, and on a 15-degree ramp. The stick still moves it at
// once: a gentle push walks it off the rim, a gentle pull brings it back.
void run_edge_stand() {
    using scraperx::sim::InitialSpawn;
    double worst = 0.0;
    const auto still = [&worst](const Stand &stand, const char *message) {
        worst = std::max(worst, stand.drift);
        require(stand.drift < kStillDriftM && !stand.fell, message);
    };

    // Deck 4's south edge at x = 11; positive offsets are in from the edge.
    Simulation deck(InitialSpawn::Deck4South);
    (void)deck.advance_frame(0.5);
    require(walk_to(deck, 11.0, -125.0, 6.0, 0.05), "edge stand: the walk to deck 4's south band");
    const double deck_edge = find_edge(deck, 0.0, 1.0);
    for (const double offset : {0.35, 0.25, 0.15, 0.05, -0.05, -0.15, -0.25}) {
        require(creep_to(deck, 11.0, deck_edge - offset, still_frame), "edge stand: the creep onto the deck's spot");
        still(let_go(deck, 0.0, 1.0, 44.5, still_frame, "deck4", offset),
              "edge stand: let go near deck 4's edge, or on the rim past it, the body must stay put");
    }
    // A minute correction off the rim: a gentle pull, and the body is back
    // over the deck at once.
    (void)hold_stick(deck, 0.0, -0.2, 0.0, 1.0, 0.5, [](const scraperx::sim::Snapshot &) { return false; });
    const double pulled = deck.snapshot().player_position.z;
    require(deck_edge - pulled > 0.15, "edge stand: a gentle pull must bring the body back off the rim");
    // Walking off on purpose: from the rim, a gentle push takes it over.
    require(creep_to(deck, 11.0, deck_edge + 0.25, still_frame), "edge stand: the creep back onto the rim");
    (void)deck.advance_frame(0.5);
    int off_tick = -1;
    for (int i = 0; i < 90 && off_tick < 0; ++i) {
        (void)deck.set_move_input(0.0, 0.2);
        (void)deck.advance_frame(Simulation::kFixedStepSeconds);
        if (!deck.snapshot().player_grounded) {
            off_tick = i;
        }
    }
    (void)deck.set_move_input(0.0, 0.0);
    require(off_tick >= 0 && off_tick < 30, "edge stand: a gentle push from the rim must walk the body off the edge");

    // The scaffold boards (0.3 m wide) over the well off the 176 ring.
    {
        Simulation boards(InitialSpawn::Ring176East);
        (void)boards.advance_frame(1.0);
        require(walk_to(boards, 7.05, -128.4, 8.0, 0.1), "edge stand: the walk to the boards");
        (void)boards.advance_frame(0.5);
        require(creep_to(boards, 7.05, -129.8, still_frame) && boards.snapshot().player_balancing,
                "edge stand: the creep out along the boards");
        still(let_go(boards, 1.0, 0.0, 177.0, still_frame, "boards", 0.0),
              "edge stand: let go on the boards, the body must stay put");
    }

    // The yard's kerb, a 0.28 m step: its north edge, a step down off it.
    Simulation kerb(InitialSpawn::ExteriorGrade);
    (void)kerb.advance_frame(0.5);
    require(walk_to(kerb, 20.0, -117.0, 40.0) && walk_to(kerb, 20.0, -118.0, 6.0, 0.08),
            "edge stand: the walk onto the yard's kerb");
    const double kerb_edge = find_edge(kerb, 0.0, 1.0);
    for (const double offset : {0.05, -0.05, -0.15, -0.25}) {
        require(creep_to(kerb, 20.0, kerb_edge - offset, still_frame), "edge stand: the creep onto the kerb's spot");
        still(let_go(kerb, 0.0, 1.0, 1.0, still_frame, "kerb top", offset),
              "edge stand: let go at the kerb's edge, or on the rim past it, the body must stay put");
    }
    // At its foot, the step's face in front: 0.1 m off it, then against it,
    // walked up to slower than a step is taken (kStepMinimumSpeed), so the
    // body stands there rather than stepping up.
    require(walk_to(kerb, 20.0, -116.5, 6.0), "edge stand: down off the kerb to its foot");
    require(creep_to(kerb, 20.0, kerb_edge + kPlayerRadiusM + 0.1, still_frame, 0.03),
            "edge stand: the creep up to near the kerb's face");
    still(let_go(kerb, 0.0, -1.0, 0.0, still_frame, "kerb foot", 0.1),
          "edge stand: let go near the foot of a step, the body must stay put");
    const auto steps = kerb.snapshot().step_up_count;
    (void)hold_stick(kerb, 0.0, -0.03, 0.0, -1.0, 2.0, [](const scraperx::sim::Snapshot &) { return false; });
    (void)kerb.advance_frame(0.5);
    require(kerb.snapshot().step_up_count == steps && kerb.snapshot().player_position.y < 1.0,
            "edge stand: walked up to the step's face slowly, the body must stand at its foot");
    still(let_go(kerb, 0.0, -1.0, 0.0, still_frame, "kerb foot", 0.0),
          "edge stand: let go against the foot of a step, the body must stay put");

    // The proving ground's translating support (2 m either way, 2.75 m half):
    // near its south edge, z = 10.75, the drift measured against it.
    {
        Simulation moving(InitialSpawn::TranslatingSupport);
        (void)moving.advance_frame(1.0);
        const auto platform = [](const Simulation &sim) {
            return Vector3{sim.snapshot().translating_support_position.x, 0.0, 0.0};
        };
        for (const double offset : {0.15, -0.1}) {
            require(creep_to(moving, 0.0, 10.75 - offset, platform) &&
                        moving.snapshot().support_entity_id == Simulation::kTranslatingSupportEntityId,
                    "edge stand: the creep onto the moving support's spot");
            still(let_go(moving, 0.0, 1.0, 0.8, platform, "moving support", offset),
                  "edge stand: let go near a moving support's edge, the body must stay put on it");
        }
    }

    // The trapeze's launch ramp, 15 degrees: let go on it, the body stays.
    {
        Simulation ramp(InitialSpawn::ExteriorGrade);
        (void)ramp.advance_frame(0.5);
        require(walk_to(ramp, 14.0, -28.0, 12.0) && walk_to(ramp, 23.0, -28.0, 6.0, 0.1),
                "edge stand: the walk up onto the trapeze's ramp");
        require(creep_to(ramp, 23.0, -28.0, still_frame) && ramp.snapshot().player_position.y > 1.5,
                "edge stand: the creep onto the ramp's middle");
        still(let_go(ramp, -1.0, 0.0, 1.0, still_frame, "ramp", 0.0),
              "edge stand: let go on a walkable ramp, the body must stay put");
    }
    std::cout << "PASS scraperx_sim edge stand: deck_edge_z=" << deck_edge << " kerb_edge_z=" << kerb_edge
              << " worst_drift_m=" << worst << " pulled_back_m=" << deck_edge - pulled
              << " walk_off_tick=" << off_tick << '\n';
}
