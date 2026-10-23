// Minimal real-body falsifiers: an authored route is insufficient without
// steel, sensors are not steel, and removing real held steel releases load.
#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"

#include <random>

namespace {
using namespace JPH;
using namespace scraperx::sim;
struct ClimbWorld {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{8 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    std::vector<BodyID> bodies, rungs;
    static constexpr float dt = 1.0F / 90.0F;
    ClimbWorld() {
        physics.Init(4096, 0, 2048, 1024, layers, broad, pairs);
        box(Vec3(40, 0.2F, 40), RVec3(0, Matron::kDeckTop - 0.2F, -150));
    }
    ~ClimbWorld() {
        for (BodyID id : bodies) {
            if (physics.GetBodyInterface().IsAdded(id)) physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    BodyID box(Vec3 half, RVec3 at, bool sensor = false) {
        BodyCreationSettings settings(new BoxShape(half), at, Quat::sIdentity(),
                                      EMotionType::Static, detail::object_layers::kStatic);
        settings.mIsSensor = sensor;
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        bodies.push_back(id);
        return id;
    }
    void steel(bool sensor, float height = 5.0F) {
        for (float y = Matron::kDeckTop + 0.3F; y < Matron::kDeckTop + height; y += 0.3F) {
            rungs.push_back(box(Vec3(0.28F, 0.02F, 0.02F), RVec3(5, y, -129.05), sensor));
        }
    }
    void tick(Matron &matron) {
        // A visible noisy climber on the known ladder yields a head-of-ladder
        // lead through the normal perception and Turn initiation paths.
        matron.pre_step(dt, RVec3(5, 160, -129.55), 1.0F);
        physics.Update(dt, 1, &temp, &jobs);
        matron.post_step(dt);
    }
    void awake(Matron &matron) {
        Matron::Saved saved;
        saved.state = Matron::State::Hunting;
        matron.restore(saved);
        // Fixture placement only; production climbing never sets a pose.
        physics.GetBodyInterface().SetPosition(matron.body(),
            RVec3(5, Matron::kDeckTop + Matron::kStandCentre, -129.55), EActivation::Activate);
    }
};
}

// A narrow board her scan read as open, walled off at its far end after the
// scan (a crate come down on it): her feet find no way along it, at the
// coarse grain and at the fine, so she leaves that way out and plans again,
// up the far deck's own standpipe from the floor.
void test_board_ends_at_wall() {
    ClimbWorld world;
    const float g = Matron::kDeckTop, top = g + 4.0F;
    world.box(Vec3(1.5F, 2.0F, 1.5F), RVec3(0, g + 2.0F, -138.5));                  // A: z -140..-137
    // Steps up to A from the south, 0.3 m each (her feet's own floor).
    for (int k = 1; k <= 13; ++k) {
        const float h = std::min(4.0F, 0.3F * float(k));
        world.box(Vec3(0.6F, 0.5F * h, 0.15F), RVec3(0, g + 0.5F * h, -137.0 + 0.3 * double(14 - k) - 0.15));
    }
    world.box(Vec3(0.15F, 0.1F, 2.5F), RVec3(0, top - 0.1F, -142.5));                // the board: z -145..-140
    world.box(Vec3(1.5F, 1.6F, 1.5F), RVec3(0, g + 1.6F, -146.5));                  // B, 0.8 m down: z -148..-145
    // B's own way up from the floor: a 0.10 m standpipe 0.45 m off its west face.
    world.box(Vec3(0.05F, 0.5F * 3.15F, 0.05F), RVec3(-1.95, g + 0.5F * 3.15F, -146.5));
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    world.awake(matron);
    world.physics.GetBodyInterface().SetPosition(matron.body(), RVec3(0, g + Matron::kStandCentre, -131.5), EActivation::Activate);
    const auto tick = [&] {
        matron.pre_step(ClimbWorld::dt, RVec3(40, g, -100), 0.0F);
        world.physics.Update(ClimbWorld::dt, 1, &world.temp, &world.jobs);
        matron.post_step(ClimbWorld::dt);
    };
    for (int i = 0; i < 90 && matron.scanned_routes() == nullptr; ++i) tick();
    require(matron.scanned_routes() != nullptr, "matron board wall: she reads the world");
    const auto *map = matron.scanned_routes();
    const int goal = map->region_at(Vec3(0, g + 3.2F, -146.5F));
    const std::vector<int> plan0 = map->plan(map->region_at(Vec3(0, g, -131.5F)), goal);
    bool by_board = false;
    for (const int k : plan0) by_board = by_board || map->edges()[std::size_t(k)].kind == traversal::MoveKind::StepDown ||
                                         map->edges()[std::size_t(k)].kind == traversal::MoveKind::HangDrop;
    require(goal >= 0 && by_board, "matron board wall: her first plan is up A and along the board");
    if (std::getenv("SCRAPERX_BW_DEBUG")) {
        for (const int k : plan0) {
            const auto &e = map->edges()[std::size_t(k)];
            const auto path = traversal::grid_walk(world.physics, {matron.body(), BodyID()}, traversal::Limits{}, Vec3(0, g, -131.5F), e.spec.start);
            const auto fine = traversal::grid_walk(world.physics, {matron.body(), BodyID()}, traversal::Limits{}, Vec3(0, g, -131.5F), e.spec.start, 3.0F, 0.1F, 60000);
            std::cout << "    edge " << traversal::move_name(e.kind) << " start " << e.spec.start.GetX() << "," << e.spec.start.GetY() << "," << e.spec.start.GetZ() << " walk " << path.size() << " fine " << fine.size() << "\n";
        }
    }
    // The crate: a wall across the board's far end, 1.5 m tall.
    world.box(Vec3(0.5F, 0.75F, 0.2F), RVec3(0, top + 0.75F, -144.4));
    require(matron.travel_to(goal), "matron board wall: a journey to the far deck");
    RVec3 last = world.physics.GetBodyInterface().GetPosition(matron.body());
    float worst = 0.0F;
    int t = 0;
    for (; t < 90 * 240 && matron.journey().active; ++t) {
        tick();
        const RVec3 now = world.physics.GetBodyInterface().GetPosition(matron.body());
        worst = std::max(worst, float((now - last).Length()));
        last = now;
    }
    const auto &j = matron.journey();
    std::cout << "  matron board ends at a wall: arrived " << j.arrivals << " replans " << j.replans << " moves "
              << j.moves_done << " in " << float(t) / 90.0F << " s, worst step " << worst << " m, at " << last.GetX() << ","
              << last.GetY() << "," << last.GetZ() << "\n";
    require(j.replans >= 1, "matron board wall: her feet find the board walled off and she plans again");
    require(!j.active && j.arrivals == 1, "matron board wall: by the far deck's own standpipe she gets there");
    require(worst <= 0.2F, "matron board wall: no tick moves her more than 0.2 m");
}

// Seeded: a board of random width and length between two decks of random
// height, walled off at a random point after her scan; up (from the floor to
// the far, lower deck) and down (from atop the near deck to it), with a shove off the
// board's side partway along it on alternate seeds. Each way she must notice,
// plan again and arrive, and no tick may move her more than 0.2 m.
namespace {
struct BoardWallRun {
    bool arrived = false, knocked = false;
    int replans = 0;
    float worst = 0.0F, secs = 0.0F;
};
BoardWallRun board_wall_run(const unsigned seed, const bool up) {
    std::mt19937 rng(seed * 7919U + 17U);
    const auto uni = [&](const float a, const float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    const int steps = std::uniform_int_distribution<int>(12, 15)(rng);
    const float g = Matron::kDeckTop, h = 0.3F * float(steps);
    const float width = uni(0.24F, 0.40F), length = uni(3.0F, 6.0F), drop = uni(0.5F, 0.9F);
    const float wall_at = uni(0.45F, 0.85F), wall_thick = uni(0.2F, 0.4F);
    const bool shove = (seed % 2U) == 1U;
    const float side = (seed % 4U) < 2U ? -1.0F : 1.0F;
    ClimbWorld world;
    const double a_z = -138.5;
    world.box(Vec3(1.5F, 0.5F * h, 1.5F), RVec3(0, g + 0.5F * h, a_z));
    for (int k = 1; k < steps; ++k) {
        const float sh = 0.3F * float(k);
        world.box(Vec3(0.6F, 0.5F * sh, 0.15F), RVec3(0, g + 0.5F * sh, a_z + 1.5 + 0.3 * double(steps - k) - 0.15));
    }
    const double b0 = a_z - 1.5 - double(length);                  // board's far end, B's near face
    world.box(Vec3(0.5F * width, 0.1F, 0.5F * length), RVec3(0, g + h - 0.1F, a_z - 1.5 - 0.5 * double(length)));
    const float b_top = h - drop;
    world.box(Vec3(1.5F, 0.5F * b_top, 1.5F), RVec3(0, g + 0.5F * b_top, b0 - 1.5));
    world.box(Vec3(0.05F, 0.5F * (b_top - 0.05F), 0.05F), RVec3(side * 1.95F, g + 0.5F * (b_top - 0.05F), b0 - 1.5));
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    world.awake(matron);
    const RVec3 start = up ? RVec3(0, g + Matron::kStandCentre, a_z + 1.5 + 0.3 * steps + 3.0)
                           : RVec3(0, g + h + Matron::kStandCentre, a_z);   // down: from atop A
    world.physics.GetBodyInterface().SetPosition(matron.body(), start, EActivation::Activate);
    const auto tick = [&] {
        matron.pre_step(ClimbWorld::dt, RVec3(40, g, -100), 0.0F);
        world.physics.Update(ClimbWorld::dt, 1, &world.temp, &world.jobs);
        matron.post_step(ClimbWorld::dt);
    };
    for (int i = 0; i < 90 && matron.scanned_routes() == nullptr; ++i) tick();
    BoardWallRun r;
    const auto *map = matron.scanned_routes();
    if (map == nullptr) return r;
    const int goal = map->region_at(Vec3(0, g + b_top, float(b0) - 1.5F));
    if (goal < 0) return r;
    if (std::getenv("SCRAPERX_BW_DEBUG")) {
        const int from = map->region_at(Vec3(float(start.GetX()), float(start.GetY()) - Matron::kStandCentre, float(start.GetZ())));
        std::cout << "    from " << from << " goal " << goal << " plan";
        for (const int k : map->plan(from, goal)) std::cout << " " << traversal::move_name(map->edges()[std::size_t(k)].kind);
        std::cout << "\n";
    }
    // The crate, after her scan.
    world.box(Vec3(0.5F, 0.75F, 0.5F * wall_thick), RVec3(0, g + h + 0.75F, a_z - 1.5 - double(wall_at * length)));
    if (!matron.travel_to(goal)) return r;
    RVec3 last = world.physics.GetBodyInterface().GetPosition(matron.body());
    int t = 0;
    for (; t < 90 * 300 && matron.journey().active; ++t) {
        tick();
        const RVec3 now = world.physics.GetBodyInterface().GetPosition(matron.body());
        r.worst = std::max(r.worst, float((now - last).Length()));
        last = now;
        // The knock-off: partway along the board, a shove off its side.
        const float feet_y = float(now.GetY()) - Matron::kStandCentre;
        if (shove && !r.knocked && std::abs(feet_y - (g + h)) < 0.15F && now.GetZ() < a_z - 2.0 &&
            now.GetZ() > a_z - 1.5 - double(wall_at * length) + 0.6) {
            world.physics.GetBodyInterface().AddImpulse(matron.body(), Vec3(1.0F, 0, 0) * (Matron::kMassKg * 2.0F));
            r.knocked = true;
        }
    }
    r.secs = float(t) / 90.0F;
    if (std::getenv("SCRAPERX_BW_DEBUG")) std::cout << "    end at " << last.GetX() << "," << float(last.GetY()) - g << "," << last.GetZ() << " step " << matron.journey().step << "/" << matron.journey().steps << " move " << traversal::move_name(matron.journey().move) << " region " << matron.journey().region << " failed " << matron.journey().moves_failed << "\n";
    r.replans = matron.journey().replans;
    r.arrived = !matron.journey().active && matron.journey().arrivals == 1;
    return r;
}
}

void run_matron_board_walls() {
    int passed = 0, runs = 0;
    std::cout << "  seed | way  | knocked | replans | arrived | worst step m | s\n";
    for (unsigned seed = std::getenv("SCRAPERX_BW_SEED") ? unsigned(std::atoi(std::getenv("SCRAPERX_BW_SEED"))) : 1U; seed <= (std::getenv("SCRAPERX_BW_SEED") ? unsigned(std::atoi(std::getenv("SCRAPERX_BW_SEED"))) : 10U); ++seed) {
        int seed_pass = 0;
        for (const bool up : {true, false}) {
            const BoardWallRun r = board_wall_run(seed, up);
            const bool ok = r.arrived && r.replans >= 1 && r.worst <= 0.2F;
            ++runs;
            if (ok) { ++passed; ++seed_pass; }
            std::cout << "  " << seed << " | " << (up ? "up  " : "down") << " | " << r.knocked << " | " << r.replans << " | "
                      << r.arrived << " | " << r.worst << " | " << r.secs << (ok ? "" : "  FAILED") << "\n";
        }
        std::cout << "  seed " << seed << ": " << seed_pass << "/2\n";
    }
    std::cout << "  board walls: " << passed << "/" << runs << "\n";
    require(passed == runs, "matron board walls: every seeded board walled off, both ways, she replans and arrives");
    std::cout << "PASS matron board walls: " << passed << "/" << runs << "\n";
}

void test_matron_climb_support() {
    std::array<std::array<float, 6>, 6> recorded{};
    for (int replay = 0; replay < 2; ++replay) {
    const float legal_force_cap = std::max({Matron::kClimbGripMaxN, MatronRoutecraft::kForce,
                                             traversal::Limits{}.total_force_n});
    for (int scenario = 0; scenario < 6; ++scenario) {
        ClimbWorld world;
        if (scenario > 0) world.steel(scenario == 1, scenario == 5 ? 2.8F : 5.0F);
        // Where the rungs lead: a landing at the climber's feet, past the
        // column's head (no table names this climb; her scan reads it).
        if (scenario >= 2) world.box(Vec3(0.8F, 0.1F, 0.8F), RVec3(5, Matron::kDeckTop + 4.9F, -127.9));
        if (scenario == 4) world.box(Vec3(0.3F, 0.5F, 0.02F), RVec3(5, 156.2, -129.2));
        if (scenario == 3) world.box(Vec3(1, 0.1F, 1), RVec3(5, 157, -129.55));
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        world.awake(matron);
        bool attached = false, removed = false, released = false, falling = false, landed = false;
        float peak_force = 0.0F;
        RVec3 previous = world.physics.GetBodyInterface().GetPosition(matron.body());
        for (int i = 0; i < 40 * 90; ++i) {
            world.tick(matron);
            const auto &view = matron.view();
            const RVec3 position = world.physics.GetBodyInterface().GetPosition(matron.body());
            require(Vec3(position - previous).Length() < 0.15F,
                    "matron support: attachment and release preserve continuous physical motion");
            previous = position;
            peak_force = std::max(peak_force, view.climb_force_n);
            require(view.climb_force_n <= legal_force_cap + 0.1F,
                    "matron support: applied grip force is bounded by the active controller cap");
            if (view.climbing) attached = true;
            if (scenario == 2 && !removed && view.climbing && view.y > 155.6F) {
                for (const BodyID rung : world.rungs) world.physics.GetBodyInterface().RemoveBody(rung);
                removed = true;
            } else if (removed || ((scenario == 3 || scenario == 5) && attached)) {
                if (!view.climbing && view.climb_force_n == 0.0F && view.climb_releases > 0) released = true;
                if (released && world.physics.GetBodyInterface().GetLinearVelocity(matron.body()).GetY() < -0.3F) falling = true;
                if (falling && std::abs(view.y - (Matron::kDeckTop + Matron::kStandCentre)) < 0.04F) {
                    landed = true;
                    break;
                }
            }
        }
        if (scenario < 2 || scenario == 4) {
            // No authored table names a ladder here any more: her scan reads
            // no rung edge from missing, sensor or obscured steel, so she
            // never takes hold (or tries a hold that refuses her) and puts no
            // force on anything.
            require(!attached && peak_force == 0,
                    "matron support: missing/sensor/obscured rungs refuse real attempted attachment");
            require(std::abs(matron.view().y - (Matron::kDeckTop + Matron::kStandCentre)) < 0.05F,
                    "matron support: nothing lifts her through empty space");
        } else if (scenario == 2) {
            require(attached && removed && released && falling && landed && peak_force > 2000,
                    "matron support: removed held steel releases force, falls under gravity, recovers floor");
        } else {
            // Her scan may already refuse the blocked or incomplete column (no
            // usable rung edge: she never takes hold); if she does take hold,
            // she stalls, lets go and falls to floor she recovers on.
            const bool refused = !attached && peak_force == 0 &&
                                 std::abs(matron.view().y - (Matron::kDeckTop + Matron::kStandCentre)) < 0.05F;
            require(refused || (attached && released && falling && landed),
                    "matron support: blocked or incomplete ascent stalls then releases to recoverable floor");
        }
        const auto &end = matron.view();
        const std::array<float, 6> result{end.x, end.y, end.z, peak_force,
            float(end.climb_grips), float(end.climb_releases)};
        if (replay == 0) recorded[scenario] = result;
        else require(result == recorded[scenario], "matron support: identical physics inputs replay identically");
    }
    }
    test_board_ends_at_wall();
    std::cout << "PASS matron climb support: missing steel, sensor exclusion, actual support removal, blocked ascent and floor recovery\n";
}
