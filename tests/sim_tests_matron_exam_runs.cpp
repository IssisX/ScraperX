// Executors through the climbing lab's exam (tests/matron_exam.hpp): seeded
// structures nobody tuned for, each way up and down, judged by the exam's
// judge alone (not by the executor's own success flag). Routecraft
// (src/sim/matron_routecraft.*) climbs bars from a structure's foot, so its
// runs start there: the walk to the foot is the macro layer's. What it does
// not climb (a pole, a ledge stack) it must refuse without a fall. The exam
// stands on y 154: the base Matron still dies more than kFallenBelowM under
// her authored deck list (matron.cpp, `levels_`), a hidden route assumption
// the lab is to replace with a physical landing.

#include "sim_tests.hpp"

#include "matron_exam.hpp"
#include "sim/matron.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/JobSystemSingleThreaded.h>

#include <iostream>
#include <memory>

namespace {
using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::detail;
namespace exam = scraperx::sim::exam;

constexpr float kDt = exam::Judge::kTickSeconds;
constexpr float kBaseY = 154.0F;

struct World {
    JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    TempAllocatorImpl temp{32 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    PhysicsSystem physics;
    std::unique_ptr<Matron> matron;
    exam::Structure s;

    explicit World(const std::uint64_t seed) {
        physics.Init(1024, 0, 4096, 2048, broad, vs, pair);
        physics.SetGravity(Vec3(0.0F, -9.81F, 0.0F));
        s = exam::build(physics, seed, object_layers::kStatic, kBaseY);
        matron = std::make_unique<Matron>(physics, object_layers::kStatic, object_layers::kMoving,
                                          object_layers::kOnSteel, BodyID());
        Matron::Saved saved;
        saved.state = Matron::State::Hunting;
        matron->restore(saved);
    }
    ~World() {
        matron.reset();
        for (const BodyID id : s.bodies) {
            physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    // The exam's initial condition only, before the first tick.
    void place(const RVec3 feet) {
        physics.GetBodyInterface().SetPosition(matron->body(), feet + Vec3(0.0F, Matron::kStandCentre, 0.0F),
                                               EActivation::Activate);
    }
    void tick() {
        matron->pre_step(kDt, RVec3(300.0, 0.0, 300.0), 0.0F);
        physics.Update(kDt, 1, &temp, &jobs);
        matron->post_step(kDt);
    }
};

struct Run {
    bool planned = false;
    MatronRoutecraft::Status status = MatronRoutecraft::Status::Idle;
    exam::Verdict verdict;
    float seconds = 0.0F;
};

// From `from` (feet) to a standing goal `to` (feet), judged on `support` and `feet_box`.
Run climb(World &w, const RVec3 from, const RVec3 to, const BodyID support, const AABox &feet_box) {
    Run run;
    w.place(from);
    for (int i = 0; i < 45; ++i) {   // settle on the start's steel
        w.tick();
    }
    exam::Judge judge(support, feet_box, MatronRoutecraft::kForce, Matron::kStandCentre);
    if (std::getenv("SCRAPERX_EXAM_DEBUG") != nullptr) {
        const RVec3 at = w.physics.GetBodyInterface().GetPosition(w.matron->body());
        std::cout << "    debug: state " << int(w.matron->view().state) << " added "
                  << w.physics.GetBodyInterface().IsAdded(w.matron->body()) << " at " << at << '\n';
    }
    run.planned = w.matron->request_climb_goal(to + Vec3(0.0F, Matron::kStandCentre + 0.02F, 0.0F), Vec3::sAxisZ());
    run.status = w.matron->climb_report().status;
    bool released = false;
    int after = 0;   // ticks since her hands came off
    const int limit = run.planned ? 120 * 90 : 90;   // a refusal: a second to see she stays put
    for (int i = 0; i < limit && !judge.done() && after < 6 * 90; ++i) {
        w.tick();
        const MatronRoutecraft::Report &r = w.matron->climb_report();
        if (!released && r.status == MatronRoutecraft::Status::Holding) {
            w.matron->cancel_climb_goal();   // at the goal: her hands off, onto her feet
            released = true;
        } else if (!released && run.planned && r.status != MatronRoutecraft::Status::Moving) {
            judge.note_failure();   // the route ended under her without reaching the goal
            run.status = r.status;
            released = true;
        }
        judge.tick(w.physics, w.matron->body(), r.force, w.matron->climb_report().status ==
                                                             MatronRoutecraft::Status::Moving, kDt);
        run.seconds = float(i + 1) * kDt;
        after += released ? 1 : 0;
    }
    if (run.planned && run.status == MatronRoutecraft::Status::Moving) {
        run.status = w.matron->climb_report().status;
    }
    run.verdict = judge.verdict();
    return run;
}

const char *status_name(const MatronRoutecraft::Status s) {
    switch (s) {
    case MatronRoutecraft::Status::Idle: return "idle";
    case MatronRoutecraft::Status::Moving: return "moving";
    case MatronRoutecraft::Status::Holding: return "holding";
    case MatronRoutecraft::Status::NoRoute: return "no route";
    case MatronRoutecraft::Status::Released: return "released";
    case MatronRoutecraft::Status::BudgetExceeded: return "budget exceeded";
    }
    return "?";
}

void report(const char *way, const Run &r) {
    std::cout << " " << way << ": " << (r.planned ? "planned" : "refused") << " (" << status_name(r.status) << "), "
              << r.verdict.reason << ", worst step " << r.verdict.worst_step_m << " m, peak " << r.verdict.peak_commanded_n
              << " N, landing " << r.verdict.worst_landing_mps << " m/s";
}

// The laws hold whatever the outcome: no snap, no force over the cap, gravity
// kept, no lethal landing, and a refusal is never an arrival.
void require_laws(const Run &r) {
    const std::string &why = r.verdict.reason;
    require(why.find("snapped") != 0 && why != "over the force cap" && why != "gravity changed on her body" &&
                why != "not the native tick",
            "matron exam routecraft: the laws hold on every run");
    require(r.verdict.alive, "matron exam routecraft: no run kills her");
    require(r.planned || !r.verdict.arrived || r.verdict.physical_failures == 0,
            "matron exam routecraft: a refusal is not counted as a climb");
}

} // namespace

void run_matron_exam_routecraft() {
    constexpr std::uint64_t kSeeds = 40;
    int tried[4] = {0, 0, 0, 0};
    int up_clean[4] = {0, 0, 0, 0};
    int down_clean[4] = {0, 0, 0, 0};
    int refused[4] = {0, 0, 0, 0};
    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
        Run up;
        Run down;
        exam::Params p;
        {
            World w(seed);
            p = w.s.params;
            const float h = p.height_m;
            up = climb(w, RVec3(0.0, kBaseY, -0.6), RVec3(0.0, kBaseY + h, p.standoff_m + 0.6F), w.s.goal, w.s.goal_feet);
        }
        {
            World w(seed);
            down = climb(w, RVec3(0.0, kBaseY + p.height_m, p.standoff_m + 0.6F), RVec3(0.0, kBaseY, -0.7), w.s.floor,
                         w.s.start_feet);
        }
        const int k = int(p.kind);
        ++tried[k];
        up_clean[k] += up.verdict.clean ? 1 : 0;
        down_clean[k] += down.verdict.clean ? 1 : 0;
        refused[k] += (!up.planned ? 1 : 0) + (!down.planned ? 1 : 0);
        std::cout << "  exam seed " << seed << " " << exam::kind_name(p.kind) << " " << p.height_m << " m pitch "
                  << p.pitch_m << " first " << p.first_hold_m << (p.missing >= 0 ? " missing a hold" : "") << ":";
        report("up", up);
        report(";  down", down);
        std::cout << '\n';
        require_laws(up);
        require_laws(down);
    }
    std::cout << "PASS scraperx_sim matron exam routecraft: laws held on " << 2 * kSeeds << " runs; clean up/down:";
    for (int k = 0; k < 4; ++k) {
        std::cout << " " << exam::kind_name(exam::Kind(k)) << " " << up_clean[k] << "/" << down_clean[k] << " of "
                  << tried[k] << " (refused " << refused[k] << ")";
    }
    std::cout << '\n';
}
