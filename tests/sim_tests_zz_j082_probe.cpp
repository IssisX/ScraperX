// UNCOMMITTED probe for J-082 sizing. Never pushed.
#include "sim_tests.hpp"

#include "sim/matron_traversal.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
using scraperx::sim::Vector3;

namespace {

void her_map_484() {
    Simulation s(InitialSpawn::Ring484North);
    require(s.advance_frame(0.5).accepted, "probe: settle");
    const auto &map = s.matron_affordances();
    const auto &st = map.stats();
    std::cout << "PROBE stats: patches=" << st.patches << " usable=" << st.usable_patches << " bars=" << st.bars
              << " ladders=" << st.ladders << " poles=" << st.poles << " risers=" << st.risers
              << " regions=" << st.regions << " edges=" << st.edges << " usable_edges=" << st.usable_edges << "\n";
    for (const auto &l : map.ladders()) {
        if (l.bottom > 480.0F && l.top < 645.0F) {
            std::cout << "PROBE ladder x=" << l.x << " z=" << l.z << " along_x=" << l.along_x << " bottom=" << l.bottom
                      << " top=" << l.top << " rungs=" << l.rungs << "\n";
        }
    }
    const char *pairs[][2] = {{"484e", "506e"}, {"616e", "640"}};
    (void)pairs;
    const auto plan_between = [&](const char *name, JPH::Vec3 a, JPH::Vec3 b) {
        const int ra = map.region_at(a);
        const int rb = map.region_at(b);
        std::cout << "PROBE plan " << name << ": from region " << ra << " to " << rb;
        if (ra >= 0 && rb >= 0) {
            const auto path = map.plan(ra, rb);
            std::cout << " edges=" << path.size() << ":";
            for (const int e : path) {
                const auto &edge = map.edges()[static_cast<std::size_t>(e)];
                std::cout << " " << scraperx::sim::traversal::move_name(edge.kind) << "(" << edge.spec.line.GetX() << ","
                          << edge.spec.line.GetZ() << " " << edge.spec.bottom_y << "-" << edge.spec.top_y << ")";
            }
        }
        std::cout << "\n";
    };
    plan_between("484->506", JPH::Vec3(10.3F, 484.25F, -149.0F), JPH::Vec3(10.0F, 506.25F, -149.0F));
    plan_between("616->640", JPH::Vec3(4.9F, 616.25F, -149.0F), JPH::Vec3(8.0F, 640.25F, -149.0F));
    plan_between("440->462", JPH::Vec3(0.0F, 440.25F, -140.0F), JPH::Vec3(0.0F, 462.25F, -140.0F));
}

struct Try {
    bool caught = false;
    bool on_block = false;
    bool hung_block = false;
    double catch_speed = 0.0;
    double release_vx = 0.0, release_vy = 0.0;
    double peak_y = 0.0;
    double min_y = 1e9;
    double end_x = 0.0, end_y = 0.0;
    int state_at_end = 0;
    std::uint64_t launches = 0;
    std::uint64_t deaths = 0;
};

// One run east down the lane from start_x, jump at jump_x; at the front of
// the swing (vx>0, vy>0, x past the bar) wait `delay` ticks then jump (or let go).
Try one(const double start_x, const bool sprint, const double jump_x, const int delay, const bool thrown) {
    Try t;
    const char *bxs = std::getenv("J082_BARX");
    const double bar_x = bxs ? std::atof(bxs) : -1.0;
    const char *fs = std::getenv("J082_FACE");
    const double face_x = fs ? std::atof(fs) : 4.0;
    const char *ls = std::getenv("J082_LIPH");
    const double top_y = 494.0 + (ls ? std::atof(ls) : 1.5);
    Simulation s(InitialSpawn::Ring484North);
    require(s.advance_frame(0.3).accepted, "probe: settle");
    s.debug_launch_player(Vector3{start_x, 494.0 + 0.95, -154.5}, Vector3{0.0, 0.0, 0.0});
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.6);
    (void)s.set_move_input(1.0, 0.0);
    (void)s.set_sprint_input(sprint);
    bool jumped = false;
    int front_ticks = -1;
    bool released = false;
    for (int tick = 0; tick < 8 * 90; ++tick) {
        const Snapshot n = s.snapshot();
        if (!jumped && n.player_position.x > jump_x) {
            (void)s.request_jump();
            jumped = true;
        }
        if (n.traversal_state == TraversalState::Swinging && !t.caught) {
            t.caught = true;
            t.catch_speed = std::hypot(n.player_linear_velocity.x, n.player_linear_velocity.y);
        }
        if (t.caught && !released && n.traversal_state == TraversalState::Swinging) {
            if (front_ticks < 0 && n.player_linear_velocity.x > 0.5 && n.player_linear_velocity.y > 0.0 &&
                n.player_position.x > bar_x) {
                front_ticks = 0;
            }
            if (front_ticks >= 0 && front_ticks++ == delay) {
                t.release_vx = n.player_linear_velocity.x;
                t.release_vy = n.player_linear_velocity.y;
                if (thrown) {
                    (void)s.request_jump();
                } else {
                    (void)s.request_release();
                }
                released = true;
            }
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Snapshot m = s.snapshot();
        t.peak_y = std::max(t.peak_y, m.player_position.y);
        t.min_y = std::min(t.min_y, m.player_position.y);
        if (m.traversal_state == TraversalState::Hanging && m.player_position.x > face_x - 1.5) {
            t.hung_block = true;
        }
        if (m.player_grounded && m.player_position.x > face_x && m.player_position.y > top_y + 0.8) {
            t.on_block = true;
        }
        if (released && m.player_grounded && tick > 30) {
            break;
        }
        if (m.player_position.y < 486.0) {
            break;
        }
    }
    const Snapshot e = s.snapshot();
    t.end_x = e.player_position.x;
    t.end_y = e.player_position.y;
    t.state_at_end = static_cast<int>(e.traversal_state);
    t.launches = e.swing_launch_count;
    t.deaths = e.death_count;
    return t;
}

void sweep() {
    const char *mode = std::getenv("J082_SWEEP");
    if (mode == nullptr) {
        return;
    }
    const char *jxs = std::getenv("J082_JX");
    const double jx = jxs ? std::atof(jxs) : -2.8;
    const char *ss = std::getenv("J082_STEP");
    const int step = ss ? std::atoi(ss) : 3;
    for (const bool sprint : {true}) {
        for (const double jump_x : {-2.6, -2.3}) {
            const Try t = one(-8.0, sprint, jump_x, 999, true);
            std::cout << "PROBE direct sprint jump_x=" << jump_x << " caught=" << t.caught << " hung_block=" << t.hung_block
                      << " on_block=" << t.on_block << " end=(" << t.end_x << "," << t.end_y << ")\n";
        }
    }
    for (const bool sprint : {false}) {
        for (const double jump_x : {jx}) {
            for (const bool thrown : {true, false}) {
                for (int delay = 0; delay <= (std::getenv("J082_DMAX") ? std::atoi(std::getenv("J082_DMAX")) : 45); delay += step) {
                    const Try t = one(-8.0, sprint, jump_x, delay, thrown);
                    std::cout << "PROBE sprint=" << sprint << " jump_x=" << jump_x << " thrown=" << thrown
                              << " delay=" << delay << " caught=" << t.caught << " catch_v=" << t.catch_speed
                              << " rel=(" << t.release_vx << "," << t.release_vy << ") peak=" << t.peak_y
                              << " hung_block=" << t.hung_block << " on_block=" << t.on_block << " end=(" << t.end_x
                              << "," << t.end_y << ") state=" << t.state_at_end << " launches=" << t.launches
                              << " deaths=" << t.deaths << "\n";
                    if (!t.caught) {
                        break;
                    }
                    if (std::getenv("J082_TERSE") && !(t.hung_block || t.on_block)) {
                        continue;
                    }
                }
            }
        }
    }
}

void run_j082_probe() {
    if (std::getenv("J082_SWEEP") == nullptr) her_map_484();
    sweep();
}

}  // namespace

SCRAPERX_GROUP("j082_probe", 9990, run_j082_probe);
SCRAPERX_SELECT("J082-PROBE", run_j082_probe);
