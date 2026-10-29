// Minimal real-body falsifiers: an authored route is insufficient without
// steel, sensors are not steel, and removing real held steel releases load.
#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"
#include "matron_contact_audit.hpp"

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
        // A fixture body may be made kinematic or dynamic later (SetMotionType).
        settings.mAllowDynamicOrKinematic = true;
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
        matron.pre_step(dt, Stimulus(RVec3(5, 160, -129.55), 1.0F));
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
        matron.pre_step(ClimbWorld::dt, Stimulus(RVec3(40, g, -100), 0.0F));
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
        matron.pre_step(ClimbWorld::dt, Stimulus(RVec3(40, g, -100), 0.0F));
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
            require(view.climb_force_n <= law::kForceN + 0.1F,
                    "matron support: applied grip force is bounded by her law's force cap");
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
    run_matron_state_exits();
    std::cout << "PASS matron climb support: missing steel, sensor exclusion, actual support removal, blocked ascent and floor recovery\n";
}

// ---- a riser that is not the shaft's corner pipe (matron_pipe.cpp) -----------
// A square post, 0.4 m across, standing on the floor 0.3 m off a solid deck's
// east face and up to its top 8 m over the floor: no rail, no flange, its foot
// on the floor she starts on. Nothing else joins the floor and the deck. Her
// journey (travel_to) takes her up it and, from the deck, down it: her scan's
// riser, her own decks and line beside it, both hands on its face, her feet
// braced on it, her hips close to it, by forces alone with gravity on.
namespace {
scraperx::sim::MatronSnapshot riser_snapshot(const Matron::View &view) {
    scraperx::sim::MatronSnapshot m;
    m.alive = view.alive;
    m.position = {view.x, view.y, view.z};
    m.on_line = view.on_line;
    m.climbing = view.climbing;
    m.climb_hand = {view.climb_hand.GetX(), view.climb_hand.GetY(), view.climb_hand.GetZ()};
    m.climb_hand2_known = view.climb_hand2_known;
    m.climb_feet_support_known = view.climb_feet_support_known;
    m.contact_supported = view.contact_supported;
    m.contact_feet = {view.contact_feet.GetX(), view.contact_feet.GetY(), view.contact_feet.GetZ()};
    m.contact_hand_held = view.contact_hand_held;
    m.contact_hand = {view.contact_hand.GetX(), view.contact_hand.GetY(), view.contact_hand.GetZ()};
    m.contact_hand2_held = view.contact_hand2_held;
    m.contact_hand2 = {view.contact_hand2.GetX(), view.contact_hand2.GetY(), view.contact_hand2.GetZ()};
    m.contact_line = view.contact_line;
    m.contact_braced = view.contact_braced;
    m.contact_brace = {view.contact_brace.GetX(), view.contact_brace.GetY(), view.contact_brace.GetZ()};
    m.climb_move = view.climb_move;
    m.climb_move_active = view.climb_move_active;
    m.on_pipe = view.on_pipe;
    m.pipe_sliding = view.pipe_sliding;
    m.pipe_stunned = view.pipe_stunned;
    m.pipe_head_down = view.pipe_head_down;
    m.pipe_on_face = view.pipe_on_face;
    m.pipe_axis = {view.pipe_axis.GetX(), 0.0, view.pipe_axis.GetZ()};
    m.pipe_half_x = view.pipe_half_x;
    m.pipe_half_z = view.pipe_half_z;
    return m;
}
struct RiserLeg {
    bool planned = false, arrived = false, on_riser = false;
    float worst_step = 0.0F, seconds = 0.0F;
    float worst_hips_m = 0.0F;      // her middle off the face she climbs, on the face
    float worst_shoulder_m = 0.0F;  // her leading hand off her shoulder, on the face
    float end_feet_y = 0.0F;
    MatronContactAudit contacts;
};
}

void test_matron_riser() {
    ClimbWorld world;
    const float g = Matron::kDeckTop;
    const float deck = g + 8.0F;
    world.box(Vec3(2.0F, 4.0F, 2.0F), RVec3(0, g + 4.0, -140));                // the deck: x -2..2, z -142..-138, top g + 8
    world.box(Vec3(0.2F, 4.0F, 0.2F), RVec3(2.5, g + 4.0, -140));              // the riser: x 2.3..2.7, z -140.2..-139.8
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    world.awake(matron);
    matron.set_stage_for_test(Matron::Stage::Dangerous);
    world.physics.GetBodyInterface().SetPosition(matron.body(), RVec3(6.5, g + Matron::kStandCentre, -137.0),
                                                 EActivation::Activate);
    const auto tick = [&] {
        matron.pre_step(ClimbWorld::dt, Stimulus(RVec3(40, g, -100), 0.0F));
        world.physics.Update(ClimbWorld::dt, 1, &world.temp, &world.jobs);
        matron.post_step(ClimbWorld::dt);
    };
    for (int i = 0; i < 90 && matron.scanned_routes() == nullptr; ++i) tick();
    require(matron.scanned_routes() != nullptr, "matron riser: she reads the world");
    const auto *map = matron.scanned_routes();
    bool found = false;
    for (const auto &r : map->risers()) found = found || (std::abs(r.x - 2.5F) < 0.01F && std::abs(r.z + 140.0F) < 0.01F);
    require(found, "matron riser: her scan finds the post as a riser");
    const int floor_region = map->region_at(Vec3(6.5F, g, -137.0F));
    const int deck_region = map->region_at(Vec3(0.0F, deck, -140.0F));
    require(floor_region >= 0 && deck_region >= 0 && map->plan(floor_region, deck_region).empty() &&
                map->plan(deck_region, floor_region).empty(),
            "matron riser: her map has no other way between the floor and the deck");
    const auto leg = [&](const int goal) {
        RiserLeg r;
        r.planned = matron.travel_to(goal);
        if (!r.planned) return r;
        RVec3 last = world.physics.GetBodyInterface().GetPosition(matron.body());
        int t = 0;
        for (; t < 90 * 120 && matron.journey().active; ++t) {
            tick();
            const RVec3 now = world.physics.GetBodyInterface().GetPosition(matron.body());
            r.worst_step = std::max(r.worst_step, float((now - last).Length()));
            last = now;
            const Matron::View &v = matron.view();
            const auto m = riser_snapshot(v);
            r.contacts.tick(m);
            r.on_riser = r.on_riser || v.on_pipe;
            if (v.on_pipe && v.pipe_on_face && v.contact_hand_held && !v.pipe_sliding && !v.pipe_stunned) {
                // Her middle off the face she climbs: how far out from the
                // post's square her middle is (the nearest face, level).
                const float dx = std::max(0.0F, std::abs(float(now.GetX()) - 2.5F) - 0.2F);
                const float dz = std::max(0.0F, std::abs(float(now.GetZ()) + 140.0F) - 0.2F);
                r.worst_hips_m = std::max(r.worst_hips_m, std::hypot(dx, dz));
                const RVec3 shoulder = now + RVec3(0.0, Matron::kHalfHeight - 0.2, 0.0);
                r.worst_shoulder_m = std::max(r.worst_shoulder_m, float((v.contact_hand - shoulder).Length()));
            }
        }
        r.seconds = float(t) / 90.0F;
        r.arrived = !matron.journey().active && matron.journey().arrivals > 0;
        r.end_feet_y = float(last.GetY()) - Matron::kStandCentre;
        return r;
    };
    const auto report = [&](const char *name, const RiserLeg &r) {
        std::cout << "  matron riser " << name << ": planned " << r.planned << " arrived " << r.arrived << " on it "
                  << r.on_riser << " in " << r.seconds << " s, feet at " << r.end_feet_y - g << " m, worst step "
                  << r.worst_step << " m, hips off its face at most " << r.worst_hips_m << " m, hand off her shoulder at most "
                  << r.worst_shoulder_m << " m, peak force " << matron.view().pipe_peak_force_n << " N, power "
                  << matron.view().pipe_peak_power_w << " W\n";
        r.contacts.report(name);
    };
    const RiserLeg up = leg(deck_region);
    report("up", up);
    const std::uint32_t arrivals_up = matron.journey().arrivals;
    const RiserLeg down = leg(floor_region);
    report("down", down);
    for (const RiserLeg *r : {&up, &down}) {
        require(r->planned && r->on_riser && r->arrived, "matron riser: her journey takes the post up and down");
        require(r->worst_step <= 0.2F, "matron riser: no tick moves her more than 0.2 m");
        require(r->contacts.clean(), "matron riser: her exported contacts are hers on it (S6)");
        require(r->contacts.pipe_face > 0 && r->contacts.braced == r->contacts.pipe_face &&
                    r->contacts.two_hands == r->contacts.pipe_face,
                "matron riser: both hands on its face and her feet braced on it every tick she climbs it");
        require(r->worst_hips_m <= Matron::kRadius + 0.13F, "matron riser: her hips close to its face");
        require(r->worst_shoulder_m <= 0.75F, "matron riser: her arms bent, her grip near her shoulder");
    }
    require(std::abs(up.end_feet_y - deck) < 0.1F && arrivals_up == 1, "matron riser: up, on the deck");
    require(std::abs(down.end_feet_y - g) < 0.1F, "matron riser: down, on the floor");
    require(matron.view().pipe_peak_force_n <= law::kForceN + 0.5F && matron.view().pipe_peak_power_w <= law::kPowerW + 0.5F,
            "matron riser: her grip within her one force and power budget");
    std::cout << "PASS scraperx_sim matron riser: up_s=" << up.seconds << " down_s=" << down.seconds
              << " hips_m=" << std::max(up.worst_hips_m, down.worst_hips_m) << "\n";
}

namespace scraperx::sim {
struct MatronStateExitAccess {
    static bool check_end_hold_cleans_all(ClimbWorld &world) {
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, BodyID());
        world.awake(m);
        m.state_ = Matron::State::Claimed;
        m.grip_on_ = true;
        m.wrist_on_ = true;
        m.wrist_reseat_ = true;
        m.arm_throw_ = true;
        m.leg_toss_ = true;
        m.retake_ = true;
        m.reel_to_floor_ = true;
        m.spare_walk_ = true;
        m.spare_side_set_ = true;
        m.spare_tries_ = 2;
        m.fatal_hold_ = true;
        m.fatal_path_ = Matron::FatalPath::Floor;
        m.fatal_dropped_ = true;
        m.fatal_edge_ = RVec3(1.0, 2.0, 3.0);
        m.fatal_s_ = 4.0F;
        m.fatal_carry_s_ = 3.0F;
        m.fatal_window_s_ = 2.0F;
        m.fatal_check_ticks_ = 7;
        m.fatal_lethal_ = true;
        m.fatal_toss_ = true;
        m.fatal_beat_ = 4;
        m.fatal_sub_ = 1;
        m.watch_still_s_ = 1.5F;
        m.pull2_n_ = 400.0F;
        m.end_hold(Matron::State::Searching);
        return m.state_ == Matron::State::Searching &&
               m.let_go_ &&
               !m.grip_on_ &&
               !m.wrist_on_ &&
               !m.wrist_reseat_ &&
               !m.arm_throw_ &&
               !m.leg_toss_ &&
               !m.retake_ &&
               !m.reel_to_floor_ &&
               !m.spare_walk_ &&
               !m.spare_side_set_ &&
               m.spare_tries_ == 0 &&
               !m.fatal_hold_ &&
               m.fatal_path_ == Matron::FatalPath::None &&
               !m.fatal_dropped_ &&
               m.fatal_edge_ == RVec3::sZero() &&
               m.fatal_s_ == 0.0F &&
               m.fatal_carry_s_ == 0.0F &&
               m.fatal_window_s_ == 0.0F &&
               m.fatal_check_ticks_ == 0 &&
               !m.fatal_lethal_ &&
               !m.fatal_toss_ &&
               m.watch_still_s_ == 0.0F &&
               m.pull2_n_ == Matron::kHandReachPullN;
    }

    static bool check_save_and_restore_clean_all(ClimbWorld &world) {
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, BodyID());
        world.awake(m);
        m.state_ = Matron::State::Staggering;
        const auto saved_stagger = m.save();
        m.state_ = Matron::State::Down;
        const auto saved_down = m.save();
        m.travel_.active = true;
        m.armplay_done_ = true;
        m.armplay_reposes_ = 2;
        m.hold_retakes_ = 1;
        m.hold_tears_at_ = 1;
        m.fatal_beat_ = 5;
        m.fatal_sub_ = 1;
        m.chase_way_held_ = true;
        m.clamber_hist_n_ = 3;
        m.clamber_refused_until_ = 50.0F;
        m.commit_s_ = 2.0F;
        m.allow_catwalk_ = true;
        m.search_s_ = 9.0F;
        m.emerge_s_ = 4.0F;
        m.sound_count_ = 2;
        m.pipe_swat_body_ = BodyID(1);
        m.restore(saved_stagger);
        return saved_stagger.state == Matron::State::Hunting &&
               saved_down.state == Matron::State::Hunting &&
               !m.travel_.active &&
               !m.armplay_done_ &&
               m.armplay_reposes_ == 0 &&
               m.hold_retakes_ == 0 &&
               m.hold_tears_at_ == 0 &&
               m.fatal_beat_ == 0 &&
               m.fatal_sub_ == 0 &&
               !m.chase_way_held_ &&
               m.clamber_hist_n_ == 0 &&
               m.clamber_refused_until_ < 0.0F &&
               m.commit_s_ == 0.0F &&
               !m.allow_catwalk_ &&
               m.search_s_ == 0.0F &&
               m.state_ == Matron::State::Emerging && m.emerge_s_ == 0.4F &&
               m.sound_count_ == 0 &&
               m.pipe_swat_body_.IsInvalid();
    }

    static bool check_grab_reach_interruptions(ClimbWorld &world) {
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, BodyID());
        world.awake(m);
        // (a) His blow landing while she is reaching in State::Grab (!grip_on_)
        m.state_ = Matron::State::Grab;
        m.grip_on_ = false;
        m.hold_s_ = 0.05F;
        m.blow_by_player_ = true;
        const bool blow_aborted = m.his_blow() &&
                                  m.state_ == Matron::State::Staggering &&
                                  m.resume_state_ == Matron::State::Searching &&
                                  m.grab_cooldown_s_ >= Matron::kRegrabAfterTearS;
        // (b) Machine moving steel under her (riding_) while reaching in State::Grab
        const BodyID plat = world.box(Vec3(4.0F, 0.2F, 4.0F), RVec3(5.0, Matron::kDeckTop + 2.0F, -129.55));
        world.physics.GetBodyInterface().SetMotionType(plat, EMotionType::Kinematic, EActivation::Activate);
        world.physics.GetBodyInterface().SetObjectLayer(plat, detail::object_layers::kMoving);
        world.physics.GetBodyInterface().SetLinearVelocity(plat, Vec3(0.0F, 1.5F, 0.0F));
        world.physics.GetBodyInterface().SetPosition(
            m.body(), RVec3(5.0, Matron::kDeckTop + 2.2F + Matron::kStandCentre, -129.55), EActivation::Activate);
        m.state_ = Matron::State::Grab;
        m.grip_on_ = false;
        m.grab_cooldown_s_ = 0.0F;
        m.feel_floor(ClimbWorld::dt);
        const bool ride_aborted = m.riding_ &&
                                  m.state_ != Matron::State::Grab &&
                                  m.grab_cooldown_s_ >= Matron::kGrabCooldownSeconds;
        return blow_aborted && ride_aborted;
    }

    static bool check_release_and_unrig_stagger_exits(ClimbWorld &world) {
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, BodyID());
        world.awake(m);
        // Knockdown from State::Release or State::Grab (!grip_on_) must clean hold fields
        m.state_ = Matron::State::Release;
        m.hold_phase_ = 1;
        m.hold_s_ = 0.25F;
        m.wrist_on_ = true;
        m.knock_down();
        const bool release_cleaned = m.state_ == Matron::State::Down &&
                                     m.hold_phase_ == 0 &&
                                     m.hold_s_ == 0.0F &&
                                     !m.wrist_on_;
        return release_cleaned;
    }

    static bool check_claimed_timeout_and_dangle_fatal_carry(ClimbWorld &world) {
        const BodyID player = world.box(Vec3(0.3F, 0.9F, 0.3F), RVec3(5.0, Matron::kDeckTop + 1.0F, -129.0));
        world.physics.GetBodyInterface().SetMotionType(player, EMotionType::Dynamic, EActivation::Activate);
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, player);
        world.awake(m);
        // (a) Claimed beat give-up (> kFatalGiveUpSeconds) must signal let_go_
        m.state_ = Matron::State::Claimed;
        m.fatal_beat_ = 1;
        m.fatal_sub_ = 0;
        m.grip_on_ = true;
        m.let_go_ = false;
        m.hold_s_ = Matron::kFatalGiveUpSeconds + 0.1F;
        m.step_claimed(ClimbWorld::dt, RVec3(5.0, Matron::kDeckTop + Matron::kStandCentre, -129.55), Matron::kDeckTop);
        const bool claimed_lets_go = m.fatal_beat_ == 8 && !m.grip_on_ && m.let_go_;

        // (b) Dangle 3rd brace loss in fatal_hold_ must advance fatal_carry_s_ so Carry goes to fatal_floor()
        m.state_ = Matron::State::Dangle;
        m.grip_on_ = true;
        m.fatal_hold_ = true;
        m.fatal_carry_s_ = 0.0F;
        m.brace_losses_ = 2;
        m.rim_x_ = 50.0F;
        m.rim_z_ = -129.55F;
        m.hold_s_ = 1.0F;
        m.step_hold(ClimbWorld::dt, RVec3(5.0, Matron::kDeckTop + 1.0F, -129.0));
        const bool dangle_exits_loop = m.state_ == Matron::State::Carry &&
                                       m.fatal_carry_s_ >= Matron::kFatalCarryS;
        return claimed_lets_go && dangle_exits_loop;
    }

    static bool check_spare_bounded_retries(ClimbWorld &world) {
        // Off at x -20, clear of the bodies the other checks left in this world.
        const RVec3 him(-20.0, Matron::kDeckTop + 1.0F, -129.0);
        const BodyID player = world.box(Vec3(0.3F, 0.9F, 0.3F), him);
        world.physics.GetBodyInterface().SetMotionType(player, EMotionType::Dynamic, EActivation::Activate);
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, player);
        world.awake(m);
        world.physics.GetBodyInterface().SetPosition(
            m.body(), RVec3(-20.0, Matron::kDeckTop + Matron::kStandCentre, -129.55), EActivation::Activate);
        m.state_ = Matron::State::Spare;
        m.grip_on_ = true;
        m.let_go_ = false;
        m.hold_phase_ = 1;
        m.spare_walk_ = false;
        // Two timeouts of the set-down tried and not done: the third counts, and she backs up and over
        // once more.
        m.spare_tries_ = 2;
        m.hold_s_ = 8.1F;
        m.step_hold(ClimbWorld::dt, him);
        const bool counted = m.spare_tries_ == 3 && m.hold_phase_ == 0 && m.grip_on_;
        // Her last pass down beside her, her deck close under his body: set down.
        m.hold_phase_ = 1;
        m.hold_s_ = 0.0F;
        m.feel_floor(ClimbWorld::dt);
        m.step_hold(ClimbWorld::dt, him);
        return counted && !m.spare_walk_ && m.hold_phase_ == 2 && !m.grip_on_ && m.let_go_;
    }

    // Past the third try with open air under him she never lets go: she keeps hold and walks him on,
    // inward, until her deck is close under his body, then sets him down (the hold ends in bounded time).
    static bool check_spare_never_lets_go_over_the_drop(ClimbWorld &world) {
        const BodyID player = world.box(Vec3(0.3F, 0.9F, 0.3F), RVec3(0.0, Matron::kDeckTop + 40.0F, -100.0));
        world.physics.GetBodyInterface().SetMotionType(player, EMotionType::Dynamic, EActivation::Activate);
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, player);
        world.awake(m);
        // Her deck ends at z -110; she stands 1 m in from the edge, facing out, him held 1.6 m out of her.
        const double edge_z = -110.0;
        world.physics.GetBodyInterface().SetPosition(
            m.body(), RVec3(0.0, Matron::kDeckTop + Matron::kStandCentre, edge_z - 1.0), EActivation::Activate);
        m.rim_x_ = 0.0F;
        m.rim_z_ = float(edge_z);
        m.rim_nx_ = 0.0F;
        m.rim_nz_ = 1.0F;
        m.state_ = Matron::State::Spare;
        m.grip_on_ = true;
        m.let_go_ = false;
        m.hold_phase_ = 1;
        m.spare_walk_ = false;
        m.spare_tries_ = 3;
        m.hold_s_ = 0.0F;
        bool released = false;
        bool released_over_air = false;
        int ticks = 0;
        for (; ticks < 30 * 90 && !released; ++ticks) {
            // Him out of her hand (fixture placement), as she holds him over the edge.
            const RVec3 her = world.physics.GetBodyInterface().GetPosition(m.body());
            const RVec3 him(her.GetX(), her.GetY() + 0.1, her.GetZ() + 1.6);
            world.physics.GetBodyInterface().SetPosition(player, him, EActivation::Activate);
            world.physics.GetBodyInterface().SetLinearVelocity(player, Vec3::sZero());
            const float feet = float(her.GetY()) - Matron::kStandCentre;
            const bool air_under = !m.deck_close_below(him, feet);
            m.pre_step(ClimbWorld::dt, Stimulus(him, 0.0F));
            released = m.let_go_ || !m.grip_on_ || m.state_ != Matron::State::Spare;
            released_over_air = released_over_air || (released && air_under);
            world.physics.Update(ClimbWorld::dt, 1, &world.temp, &world.jobs);
            m.post_step(ClimbWorld::dt);
        }
        const RVec3 end = world.physics.GetBodyInterface().GetPosition(m.body());
        std::cout << "  spare over the drop: released " << released << " after " << float(ticks) / 90.0F << " s, over air "
                  << released_over_air << ", she ended " << (edge_z - end.GetZ()) << " m in from the edge\n";
        return released && !released_over_air;
    }

    static bool check_reel_break_and_calm_backoff(ClimbWorld &world) {
        const BodyID player = world.box(Vec3(0.3F, 0.9F, 0.3F), RVec3(5.0, Matron::kDeckTop + 1.0F, -128.0));
        world.physics.GetBodyInterface().SetMotionType(player, EMotionType::Dynamic, EActivation::Activate);
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, player);
        world.awake(m);
        // (a) grip_lost during reel_ must end_reel with 30 s back-off, not retake()
        m.state_ = Matron::State::Carry;
        m.grip_on_ = true;
        m.reel_ = true;
        m.retake_ = false;
        m.calm_s_ = 0.0F;
        m.grab_cooldown_s_ = 0.0F;
        const float hold_off = m.hands_off_after_hold();
        m.grip_lost(true, false);
        const bool reel_backed_off = !m.reel_ &&
                                     !m.retake_ &&
                                     hold_off >= Matron::kRegrabAfterTearS &&
                                     m.state_ == Matron::State::Staggering &&
                                     m.resume_state_ == Matron::State::Searching &&
                                     m.grab_cooldown_s_ >= Matron::kRegrabAfterTearS;
        // (b) While calm_s_ > 0, pre_step in Hunting transitions to Shuffling (patrols away)
        m.state_ = Matron::State::Hunting;
        m.calm_s_ = 25.0F;
        m.pre_step(ClimbWorld::dt, Stimulus(RVec3(5.0, Matron::kDeckTop + 1.0F, -128.0), 1.0F));
        const bool patrols_away = m.state_ == Matron::State::Shuffling && m.calm_s_ > 20.0F;
        return reel_backed_off && patrols_away;
    }

    static bool check_cooldowns_tick_during_mover(ClimbWorld &world) {
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, BodyID());
        world.awake(m);
        m.state_ = Matron::State::Hunting;
        m.journey_active_ = true;
        m.grab_cooldown_s_ = 10.0F;
        m.shove_cooldown_s_ = 2.0F;
        m.calm_s_ = 10.0F;
        m.throw_cooldown_s_ = 5.0F;
        m.fetch_refused_s_ = 5.0F;
        m.climb_retry_s_ = 2.0F;
        m.pre_step(ClimbWorld::dt, Stimulus(RVec3(5.0, Matron::kDeckTop + 1.0F, -128.0), 0.0F));
        return m.grab_cooldown_s_ < 10.0F &&
               m.shove_cooldown_s_ < 2.0F &&
               m.calm_s_ < 10.0F &&
               m.throw_cooldown_s_ < 5.0F &&
               m.fetch_refused_s_ < 5.0F &&
               std::abs(m.climb_retry_s_ - (2.0F - ClimbWorld::dt)) < 1.0e-4F;
    }

    static bool check_subphase_interruptions_and_stalls(ClimbWorld &world) {
        Matron m(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kOnSteel, BodyID());
        world.awake(m);
        // (a) PipePhase::Slide stall exits pipe after kPipeStallS
        m.pipe_phase_ = Matron::PipePhase::Slide;
        m.pipe_from_ = Matron::kRingFloor;
        m.pipe_to_ = Matron::kDeckTop;
        m.pipe_swat_body_ = BodyID(2);
        m.pipe_slide_mps_ = 3.0F;
        m.pipe_stall_s_ = 0.0F;
        for (int i = 0; i < 90 * 4 && m.pipe_phase_ != Matron::PipePhase::Off; ++i) {
            m.ride_pipe(ClimbWorld::dt);
        }
        const bool pipe_slide_exited = m.pipe_phase_ == Matron::PipePhase::Off &&
                                       m.pipe_swat_body_.IsInvalid() &&
                                       m.pipe_slide_mps_ == 0.0F;
        // (b) Trap::Holding when level_ != bay_level_ and Trap::ClimbOut timeout exit to Trap::Off
        m.trap_ = Matron::Trap::Holding;
        m.level_ = 0;
        m.bay_level_ = 2;
        m.trap_step(ClimbWorld::dt, RVec3(0.0, Matron::kDeckTop, -150.0), RVec3(0.0, Matron::kDeckTop, -150.0), false, false);
        const bool trap_holding_exited = m.trap_ == Matron::Trap::Off;

        m.trap_ = Matron::Trap::ClimbOut;
        m.level_ = 2;
        m.bay_level_ = 2;
        m.trap_s_ = 21.0F;
        m.trap_step(ClimbWorld::dt, RVec3(0.0, Matron::kDeckTop, -150.0), RVec3(0.0, Matron::kDeckTop, -150.0), false, false);
        const bool trap_climbout_exited = m.trap_ == Matron::Trap::Off;

        // (c) RoutecraftWay::cancel clears found_.active and routecraft_recovering_
        m.found_.active = true;
        m.routecraft_recovering_ = true;
        m.routecraft_way_.cancel();
        const bool routecraft_cleaned = !m.found_.active && !m.routecraft_recovering_;

        return pipe_slide_exited && trap_holding_exited && trap_climbout_exited && routecraft_cleaned;
    }
};
} // namespace scraperx::sim

void run_matron_state_exits() {
    ClimbWorld world;
    const bool c1 = MatronStateExitAccess::check_end_hold_cleans_all(world);
    const bool c2 = MatronStateExitAccess::check_save_and_restore_clean_all(world);
    const bool c3 = MatronStateExitAccess::check_grab_reach_interruptions(world);
    const bool c4 = MatronStateExitAccess::check_release_and_unrig_stagger_exits(world);
    const bool c5 = MatronStateExitAccess::check_claimed_timeout_and_dangle_fatal_carry(world);
    const bool c6 = MatronStateExitAccess::check_spare_bounded_retries(world);
    const bool c7 = MatronStateExitAccess::check_reel_break_and_calm_backoff(world);
    const bool c8 = MatronStateExitAccess::check_cooldowns_tick_during_mover(world);
    const bool c9 = MatronStateExitAccess::check_subphase_interruptions_and_stalls(world);
    const bool c10 = MatronStateExitAccess::check_spare_never_lets_go_over_the_drop(world);
    std::cout << "  matron state exits: end_hold=" << c1
              << " restore=" << c2
              << " grab_reach=" << c3
              << " release_down=" << c4
              << " claimed_dangle=" << c5
              << " spare_retry=" << c6
              << " reel_calm=" << c7
              << " mover_cooldowns=" << c8
              << " subphase_stalls=" << c9
              << " spare_over_drop=" << c10 << "\n";
    require(c1, "matron state exits: end_hold resets all hold, armplay, spare and fatality state and signals let_go");
    require(c2, "matron state exits: save and restore clear active travel, hold, fatality and perception state");
    require(c3, "matron state exits: reaching Grab aborts cleanly on player blow or moving machine steel");
    require(c4, "matron state exits: knockdown during Release cleans hold phase and wrist state");
    require(c5, "matron state exits: Claimed beat give-up releases Jolt grip and Dangle 3rd brace loss exits fatal Carry loop");
    require(c6, "matron state exits: Spare bounded retries release grip and set down after stalled walk");
    require(c7, "matron state exits: reel line break grants 30 s back-off and calm_s patrols away in Shuffling");
    require(c8, "matron state exits: cooldown timers tick once per step while a Mover owns her body");
    require(c9, "matron state exits: PipePhase::Slide stall, Trap mismatch/timeout and RoutecraftWay cancel clean up");
    require(c10, "matron state exits: a set-down blocked three times over open air never lets go over the drop; she walks him on and sets him down on her deck");
    std::cout << "PASS matron state exits: all 10 state-exit groups verified\n";
}
