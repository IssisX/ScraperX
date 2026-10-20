#include "sim/rigging_system.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float dt = 1.F / 360;
void require(bool value, const char *why) {
    if (!value) { std::cerr << "FAIL RIGGING_SYSTEM " << why << '\n'; std::exit(1); }
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "rigging"; }
};
struct BF final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LF final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Fixture final : PhysicsStepListener {
    BP bp; BF bf; LF lf; PhysicsSystem world;
    TempAllocatorImpl temp{8 * 1024 * 1024}; JobSystemSingleThreaded jobs{1024};
    std::unique_ptr<kit::Kit> parts;
    std::unique_ptr<RiggingSystem> rig;
    kit::BodyIndex support, load;
    explicit Fixture(float support_mass = 0, float rating = 6000, double capacity = 250000, bool separate_controls = false) {
        world.Init(64, 0, 128, 128, bp, bf, lf);
        parts = std::make_unique<kit::Kit>(world, 0, 1);
        support = box(1000, RVec3(0, 4, 0), support_mass);
        load = box(2000, RVec3(0, 0, 0), 100);
        rig = std::make_unique<RiggingSystem>(world, *parts);
        PoweredCable::Config profile;
        profile.max_speed_mps = .25F; profile.pull_force_n = profile.brake_force_n = 6000;
        profile.electrical_power_w = 2000; profile.efficiency = .8; profile.capacity_j = capacity;
        profile.min_length_m = .1F; profile.max_length_m = 12;
        if (separate_controls)
            rig->add_powered_mount(support, Vec3::sZero(), Vec3(.6F, -3, -1), profile, rating);
        else rig->add_powered_mount(support, Vec3::sZero(), profile, rating);
        rig->add_eye(load, Vec3::sZero(), 6000);
        rig->equip(true); world.AddStepListener(this);
    }
    ~Fixture() { world.RemoveStepListener(this); rig.reset(); parts.reset(); }
    void OnStep(const PhysicsStepListenerContext &context) override { rig->collision_step(context.mDeltaTime); }
    kit::BodyIndex box(std::uint64_t entity, RVec3 at, float mass) {
        kit::Part p; p.half = Vec3(.2F, .2F, .2F);
        const auto b = parts->add_body(entity, {p}, at, Quat::sIdentity(), mass, .5F);
        if (mass > 0) parts->set_damping(b, 0, 0);
        return b;
    }
    RVec3 eye(unsigned id) const { return parts->body_position(id == 1 ? support : load); }
    std::uint64_t aim(unsigned id) {
        rig->aim(eye(id) + Vec3(0, 0, 1), -Vec3::sAxisZ(), BodyID(), true);
        const auto token = rig->readback().target;
        require(token != 0 && unsigned(token & 0xffffffffU) == id, "actual aim identifies authored eye");
        return token;
    }
    void action(unsigned id) { require(rig->action(aim(id)), "real shown tool action succeeds"); }
    void install() { action(1); action(2); }
    void operate(float effort) {
        // Watch the load: the retained control context is mount reach, not aim.
        rig->aim(eye(1) + Vec3(0, 0, 1), -Vec3::sAxisY(), BodyID(), true);
        rig->set_power_input(effort);
    }
    void tick(unsigned count) {
        while (count--) {
            parts->pre_step(dt);
            require(world.Update(dt, 1, &temp, &jobs) == EPhysicsUpdateError::None, "Jolt update");
            parts->post_step(dt); rig->post_step(dt);

        }
    }
};
void lift_payout_brake_and_context() {
    Fixture f; f.install();
    require(f.world.GetConstraints().size() == 1 && f.rig->readback().installed_count == 1,
            "direct powered install creates exactly one actual row");
    require(!f.rig->unclip(f.rig->line_handle(0)), "unsolved line cannot unclip");
    std::array<RVec3, 4> points;
    require(f.rig->polyline(0, points) == 2 && (points[0] - f.eye(1)).Length() < 1.e-6F &&
            (points[1] - f.eye(2)).Length() < 1.e-6F, "powered path uses real two endpoints");
    f.tick(180); const auto settled = f.parts->body_position(f.load).GetY();
    f.operate(1);
    require(f.rig->readback().power_operable && !f.rig->readback().target,
                         "operator may watch load without aiming mount");
    f.tick(720); const auto lifted = f.parts->body_position(f.load).GetY();
    auto r = f.rig->readback();
    require(lifted > settled + .3F && r.rest_length_m < 3.7F && r.motor_work_j > 0 &&
            r.source_energy_used_j > 0 && r.energy_j < r.capacity_j && r.tension_n > 60,
            "finite source performs actual loaded lift");
    const auto handle = f.rig->line_handle(0);
    require(!f.rig->unclip(handle) && f.rig->readback().failure == RiggingSystem::Failure::Loaded,
            "measured loaded powered line cannot unclip");
    const auto paid = r.source_energy_used_j;
    f.operate(-1); f.tick(360); const auto down = f.parts->body_position(f.load).GetY();
    require(down < lifted - .1F, "negative command pays out loaded beam");
    f.operate(0); f.tick(360); const auto held = f.parts->body_position(f.load).GetY();
    f.tick(360); require(std::abs(f.parts->body_position(f.load).GetY() - held) < .02F &&
                        f.rig->readback().braking, "neutral real brake holds beam");
    require(f.rig->readback().source_energy_used_j == paid, "payout and neutral do not refill battery");
    f.operate(1); f.tick(1); f.rig->equip(false); f.tick(1);
    require(f.rig->readback().braking && f.rig->readback().target_speed_mps == 0 &&
            f.world.GetConstraints().size() == 1, "stow stops motor and retains installed brake");
    f.rig->equip(true); f.operate(1); f.tick(1);
    f.rig->aim(RVec3(20, 4, 0), -Vec3::sAxisZ(), BodyID(), true); f.tick(1);
    require(!f.rig->readback().power_operable && f.rig->readback().braking,
            "remote operator loses command without removing line");
    f.operate(1); f.tick(1);
    f.rig->aim(f.eye(1) + Vec3(0, 0, 1), -Vec3::sAxisZ(), BodyID(), false); f.tick(1);
    require(f.rig->readback().braking, "unsafe context stops command");
    f.operate(std::numeric_limits<float>::quiet_NaN()); f.tick(1);
    require(f.rig->readback().braking, "nonfinite power input stops safely");
    std::cout << "POWERED_RIG lift_m=" << lifted - settled << " payout_m=" << lifted - down
              << " source_j=" << paid << " actual_brake=1\n";
}
void source_restore_cut_reclip() {
    Fixture f; f.install(); f.operate(1); f.tick(360);
    kit::Kit::Checkpoint bodies; f.parts->capture(bodies);
    const auto graph = f.rig->capture(); const auto old = f.rig->line_handle(0);
    const auto old_target = f.aim(1); const auto before = f.rig->readback();
    f.rig->detach_for_restore(); require(f.world.GetConstraints().empty(), "detach removes backend before body restore");
    f.parts->restore(bodies); f.rig->restore(graph);
    auto r = f.rig->readback();
    require(f.world.GetConstraints().size() == 1 && r.energy_j == before.energy_j &&
            r.source_energy_used_j == before.source_energy_used_j && r.rest_length_m == before.rest_length_m &&
            r.target_speed_mps == 0 && r.braking && r.tension_n == 0,
            "cold restore retains topology paid rest and source ledger without warm impulse");
    f.rig->equip(true); f.aim(1);
    f.rig->post_step(dt);
    require(!f.rig->unclip(f.rig->line_handle(0)) && f.rig->readback().failure == RiggingSystem::Failure::Loaded,
            "historical collision interval cannot authorize cold-restored unclip");
    require(!f.rig->action(old_target) && !f.rig->select(old), "restore rejects stale eye and line identities");
    require(f.rig->cut(f.rig->line_handle(0)), "deliberate cut removes powered row");
    require(f.world.GetConstraints().empty(), "cut removes only backend");
    require(f.rig->unclip(f.rig->line_handle(0)), "broken unloaded line can clear slot");
    f.world.SetGravity(Vec3::sZero()); f.install(); r = f.rig->readback();
    require(r.energy_j == before.energy_j && r.source_energy_used_j == before.source_energy_used_j,
            "reclipping same mounted device cannot mint source energy");
    require(!f.rig->select(old), "replacement rejects generation-stale line");
    f.rig->action(f.aim(1));
    require(f.rig->readback().failure == RiggingSystem::Failure::Capacity,
            "registered winch cannot own second simultaneous cable");
    f.tick(1); const auto current = f.rig->line_handle(0);
    f.aim(1); require(f.rig->unclip(current), "actual solved slack line unclips");
    f.install(); require(f.rig->readback().energy_j == before.energy_j, "ordinary unclip retains source");
    auto disabled = bodies;
    // Restore body ownership independently after removing the referencing backend.
    f.rig->detach_for_restore();
    disabled.bodies[f.load.value].enabled = false;
    f.parts->restore(disabled);
    f.rig->restore(graph);
    require(f.world.GetConstraints().empty() && f.rig->readback().failure == RiggingSystem::Failure::StaleTarget,
            "missing body identity rejects restoration without fake install");
}
void rejected_topology_and_selection() {
    Fixture blocked; blocked.box(1001, RVec3(0, 2, 0), 0);
    blocked.action(1);
    require(!blocked.rig->action(blocked.aim(2)) && blocked.world.GetConstraints().empty() &&
            blocked.rig->readback().failure == RiggingSystem::Failure::Obstructed,
            "solid direct span refuses powered install without fake success");
    Fixture f; f.world.SetGravity(Vec3::sZero());
    const auto support2 = f.box(1002, RVec3(3, 4, 0), 0);
    const auto load2 = f.box(2002, RVec3(3, 0, 0), 100);
    PoweredCable::Config profile; profile.capacity_j = 1000;
    f.rig->add_powered_mount(support2, Vec3::sZero(), profile);
    f.rig->add_eye(load2, Vec3::sZero());
    f.install(); f.operate(5); f.tick(1);
    require(std::abs(f.rig->readback().target_speed_mps) <= .25F,
            "finite command is clamped at actual device speed");
    const auto used = f.rig->readback().source_energy_used_j;
    for (const auto b : {support2, load2}) {
        f.rig->aim(f.parts->body_position(b) + Vec3(0, 0, 1), -Vec3::sAxisZ(), BodyID(), true);
        require(f.rig->action(f.rig->readback().target), "second registered device installs independently");
    }
    f.tick(1);
    auto graph = f.rig->capture();
    require(graph.powered_lines[0].target_speed_mps == 0 && graph.power_sources[0].source_energy_used_j == used &&
            graph.powered_lines[1].energy_j == profile.capacity_j,
            "selection change stops prior device and keeps independent source inventories");
    f.aim(1); require(f.rig->select(f.rig->line_handle(0)), "first installation remains selectable");
    f.operate(1); f.tick(1);
    require(f.rig->readback().source_energy_used_j > used, "returning selection resumes same finite device");
}
void offered_winch_selection_and_stale_receipts() {
    Fixture f; f.world.SetGravity(Vec3::sZero());
    const auto support_b = f.box(1002, RVec3(3, 4, 0), 0);
    const auto load_b = f.box(2002, RVec3(3, 0, 0), 100);
    PoweredCable::Config profile; profile.capacity_j = 1000;
    f.rig->add_powered_mount(support_b, Vec3::sZero(), profile);
    f.rig->add_eye(load_b, Vec3::sZero());
    f.aim(1);
    require(f.rig->readback().target_line == 0, "empty mount offers no cable");
    f.install();
    const auto line_a = f.rig->line_handle(0);
    f.aim(1);
    require(f.rig->readback().target_line == line_a, "mount A offers its exact installed cable");
    f.aim(2);
    require(f.rig->readback().target_line == line_a,
            "connected load endpoint offers its exact line for deliberate inspection and removal");
    for (const auto b : {support_b, load_b}) {
        f.rig->aim(f.parts->body_position(b) + Vec3(0, 0, 1), -Vec3::sAxisZ(), BodyID(), true);
        require(f.rig->action(f.rig->readback().target), "mount B installs its independent cable");
    }
    const auto line_b = f.rig->line_handle(1);
    // Both mounts remain reachable: selection, rather than lost reach, stops A.
    const RVec3 hand(1.5F, 4, .5F);
    const auto look = [&](kit::BodyIndex mount, unsigned id) {
        f.rig->aim(hand, Vec3(f.parts->body_position(mount) - hand).Normalized(), BodyID(), true);
        const auto r = f.rig->readback();
        require(unsigned(r.target & 0xffffffffU) == id, "look offers exact registered mount");
        return r.target_line;
    };
    require(look(f.support, 1) == line_a && f.rig->readback().selected == line_b,
            "looking at A offers A without automatically selecting it");
    require(f.rig->select(f.rig->readback().target_line), "explicit receipt selects A");
    f.rig->set_power_input(1); f.tick(1);
    const auto a_paid = f.rig->capture();
    require(a_paid.powered_lines[0].target_speed_mps < 0, "A command reaches A motor");
    require(look(support_b, 3) == line_b && f.rig->readback().selected == line_a &&
            f.rig->readback().target_speed_mps < 0,
            "looking at B retains A command while both mounts remain reachable");
    require(f.rig->select(f.rig->readback().target_line), "explicit receipt selects B");
    f.tick(1);
    require(f.rig->capture().powered_lines[0].target_speed_mps == 0 &&
            f.rig->capture().powered_lines[0].source_energy_used_j == a_paid.powered_lines[0].source_energy_used_j,
            "selecting B stops A at the next collision without another source debit");
    f.rig->set_power_input(1); f.tick(1);
    auto graph = f.rig->capture();
    require(graph.powered_lines[0].target_speed_mps == 0 && graph.powered_lines[1].target_speed_mps < 0 &&
            graph.powered_lines[0].source_energy_used_j == a_paid.powered_lines[0].source_energy_used_j &&
            graph.powered_lines[1].source_energy_used_j > a_paid.powered_lines[1].source_energy_used_j,
            "B command changes only B source ledger and motor");
    const auto b_paid = graph.powered_lines[1].source_energy_used_j;
    require(look(f.support, 1) == line_a && f.rig->select(f.rig->readback().target_line),
            "return to A uses same exact installation");
    require(f.rig->readback().source_energy_used_j == a_paid.powered_lines[0].source_energy_used_j,
            "returning to A preserves its paid ledger");
    f.rig->set_power_input(1); f.tick(1);
    graph = f.rig->capture();
    require(graph.powered_lines[0].source_energy_used_j > a_paid.powered_lines[0].source_energy_used_j &&
            graph.powered_lines[1].source_energy_used_j == b_paid && graph.powered_lines[1].target_speed_mps == 0,
            "returning command belongs only to A and preserves B ledger");
    f.rig->set_power_input(0);
    look(support_b, 3);
    require(f.rig->unclip(line_b) && f.rig->readback().target_line == 0 && !f.rig->select(line_b),
            "solved unloaded removal clears offered cable and rejects old token");
    for (const auto b : {support_b, load_b}) {
        f.rig->aim(f.parts->body_position(b) + Vec3(0, 0, 1), -Vec3::sAxisZ(), BodyID(), true);
        require(f.rig->action(f.rig->readback().target), "B replacement installs");
    }
    const auto replacement = f.rig->line_handle(1);
    require(replacement != line_b && look(support_b, 3) == replacement && !f.rig->select(line_b) &&
            f.rig->readback().target_line == replacement,
            "replacement offers new generation and never obsolete token");
    kit::Kit::Checkpoint bodies; f.parts->capture(bodies);
    graph = f.rig->capture();
    f.rig->detach_for_restore();
    require(f.rig->readback().target_line == 0, "detached graph offers no obsolete cable");
    f.parts->restore(bodies); f.rig->restore(graph);
    require(f.rig->readback().target_line == 0, "cold restore starts without offered cable");
    f.rig->equip(true);
    const auto restored = look(support_b, 3);
    require(restored != replacement && restored != line_b && restored == f.rig->line_handle(1) &&
            !f.rig->select(replacement) && f.rig->readback().target_line == restored,
            "restored mount offers current epoch and rejects stale selection");
    require(f.rig->cut(restored) && f.rig->readback().target_line == 0,
            "broken cable cannot remain an offered control target");
    f.rig->detach_for_restore(); bodies.bodies[load_b.value].enabled = false;
    f.parts->restore(bodies); f.rig->restore(graph); f.rig->equip(true);
    require(look(support_b, 3) == 0, "invalid restored load cannot expose a cable token");
    std::cout << "WINCH_SELECTION exact_mount_tokens=2 independent_ledgers=1 stale_receipts_rejected=1\n";
}
void separate_panel_and_physical_exit() {
    Fixture legacy;
    legacy.aim(1);
    require((legacy.rig->readback().target_point - legacy.eye(1)).Length() < 1.e-6F,
            "original overload retains cable exit as its control point");
    Fixture f(0, 6000, 250000, true);
    const Vec3 control(.6F, -3, -1);
    const auto panel = [&] { return f.parts->body_position(f.support) + control; };
    const auto hand = [&] { return panel() + Vec3(0, 0, -1); };
    const auto aim_panel = [&] {
        f.rig->aim(hand(), Vec3::sAxisZ(), BodyID(), true);
        const auto r = f.rig->readback();
        require(r.target != 0 && (r.target_point - panel()).Length() < 1.e-6F,
                "low panel offers registered mount and exact panel marker");
        return r.target;
    };
    require((f.eye(1) - hand()).Length() > RiggingSystem::kInstallReachM &&
            f.rig->available_eye(0)->local.IsNearZero(),
            "high physical exit stays in catalogue beyond control reach");
    const auto eye_count = f.rig->eye_count();
    bool rejected = false;
    try {
        f.rig->add_powered_mount(f.support, Vec3::sZero(),
            Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0), PoweredCable::Config{});
    } catch (const std::invalid_argument &) { rejected = true; }
    require(rejected && f.rig->eye_count() == eye_count, "nonfinite panel rejected without registering a mount");
    require(f.rig->action(aim_panel()), "reachable low panel starts high-exit installation");
    std::array<RVec3, 4> points;
    require(f.rig->preview(points) == 2 && (points[0] - f.eye(1)).Length() < 1.e-6F,
            "draft cable still begins at physical high exit");
    f.action(2);
    require(std::abs(f.rig->readback().rest_length_m - 4.025F) < 1.e-5F &&
            f.rig->polyline(0, points) == 2 && (points[0] - f.eye(1)).Length() < 1.e-6F,
            "paid cable length and installed polyline use high exit rather than panel");
    aim_panel();
    const auto line = f.rig->readback().target_line;
    require(line == f.rig->line_handle(0) && f.rig->select(line) && f.rig->readback().power_operable,
            "panel offers exact installed line and authorizes explicit control selection");
    f.rig->set_power_input(1); f.tick(180);
    const auto state = f.rig->capture().powered_lines[0];
    require((state.support_endpoint - f.eye(1)).Length() < 1.e-6F &&
            (state.load_endpoint - f.eye(2)).Length() < 1.e-6F &&
            std::abs(state.actual_length_m - float((f.eye(2) - f.eye(1)).Length())) < 1.e-5F &&
            state.load_force.GetY() > 60 && std::abs(state.load_force.GetX()) < 1.e-5F &&
            std::abs(state.load_force.GetZ()) < 1.e-5F && state.source_energy_used_j > 0,
            "real solved force and measured endpoints remain at high physical exit");
    f.rig->aim(RVec3(10, 1, -2), Vec3::sAxisZ(), BodyID(), true);
    require(!f.rig->readback().power_operable && !f.rig->select(line), "remote panel cannot authorize control selection");
    f.rig->set_power_input(1); f.tick(1);
    require(f.rig->readback().braking && f.rig->readback().source_energy_used_j == state.source_energy_used_j,
            "leaving panel range stops motor without further source debit");
    aim_panel(); f.rig->set_power_input(1); f.tick(1);
    f.box(1003, panel() + Vec3(0, 0, -.5F), 0);
    f.rig->aim(hand(), Vec3::sAxisZ(), BodyID(), true);
    require(f.rig->readback().target == 0 && !f.rig->readback().power_operable && !f.rig->select(line),
            "other-body obstruction hides panel and rejects reachable cable selection");
    const auto paid = f.rig->readback().source_energy_used_j;
    f.rig->set_power_input(1); f.tick(1);
    require(f.rig->readback().braking && f.rig->readback().source_energy_used_j == paid,
            "obstructed panel stops motor without granting through-body control");
    Fixture slack(0, 6000, 250000, true); slack.world.SetGravity(Vec3::sZero());
    const auto slack_panel = slack.eye(1) + control;
    slack.rig->aim(slack_panel + Vec3(0, 0, -1), Vec3::sAxisZ(), BodyID(), true);
    require(slack.rig->action(slack.rig->readback().target), "slack fixture starts at panel");
    slack.action(2); slack.tick(1);
    slack.rig->aim(slack_panel + Vec3(0, 0, -1), Vec3::sAxisZ(), BodyID(), true);
    require(slack.rig->unclip(slack.rig->readback().target_line),
            "actual low panel authorizes removal of solved unloaded high cable");
    std::cout << "WINCH_PANEL high_exit=1 low_control=1 physical_force=1 obstruction_stop=1 legacy_overload=1\n";
}
void dynamic_support_and_overload() {
    Fixture free(20); free.world.SetGravity(Vec3::sZero()); free.install();
    const auto a = free.parts->body_position(free.support), b = free.parts->body_position(free.load);
    free.operate(1); free.tick(360);
    const auto da = free.parts->body_position(free.support) - a, db = free.parts->body_position(free.load) - b;
    require(da.GetY() < -.1F && db.GetY() > .01F && std::abs(20 * da.GetY() + 100 * db.GetY()) < .01F,
            "free support recoils with reciprocal momentum instead of world pin");
    Fixture weak(0, 100); weak.install(); weak.tick(180);
    require(weak.world.GetConstraints().empty() && weak.rig->readback().failure == RiggingSystem::Failure::Overload,
            "weak registered eye breaks from actual solved overload");
    Fixture empty(0, 6000, 0); empty.install(); empty.operate(1); empty.tick(180);
    require(empty.rig->readback().energy_cutoff && empty.rig->readback().braking &&
            empty.rig->readback().source_energy_used_j == 0, "empty device retains finite brake without paid lift");
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    lift_payout_brake_and_context(); source_restore_cut_reclip(); rejected_topology_and_selection(); offered_winch_selection_and_stale_receipts(); separate_panel_and_physical_exit(); dynamic_support_and_overload();
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
    std::cout << "PASS RIGGING_POWERED real_jolt_kit=1 public_player_route=unverified\n";
}
