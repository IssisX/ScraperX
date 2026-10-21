// Her map of the tower: regions of real support and the directed ways between
// them (matron_routes.hpp). The footprints are the steel's own: the stack's
// decks are a ring of bands 9 m deep round the 34 m shaft (build_stack,
// physics_world.cpp; physics_world_tuning.hpp kStack*), the well's rings bands
// 4 m wide (world_solids.inc "UpperDeck"), narrowing 0.91 m a ring.

#include "sim/matron_routes.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace scraperx::sim::routes {
namespace {

constexpr float kShaftZ = -150.0F;
constexpr float kStackHalf = 26.0F;      // physics_world_tuning.hpp kStackHalfExtent
constexpr float kStackBand = 9.0F;       // kStackDeckBandDepth
constexpr float kRingBand = 4.0F;
constexpr float kRing176Mid = 23.09F;    // the 176.25 ring's band middle; 0.91 m in a ring above
constexpr float kRingStep = 0.91F;
constexpr float kUnreachable = std::numeric_limits<float>::max();

// The four bands of a square ring whose outer edge is `outer` and band `band`
// deep, round the shaft at (0, kShaftZ).
std::uint8_t ring_patches(std::array<Patch, 6> &patches, const float outer, const float band) {
    const float inner = outer - band;
    patches[0] = {-outer, outer, kShaftZ + inner, kShaftZ + outer};   // north
    patches[1] = {-outer, outer, kShaftZ - outer, kShaftZ - inner};   // south
    patches[2] = {inner, outer, kShaftZ - inner, kShaftZ + inner};    // east
    patches[3] = {-outer, -inner, kShaftZ - inner, kShaftZ + inner};  // west
    return 4;
}

RegionInfo stack_deck(const Region id, const std::string_view label, const float top, const int level) {
    RegionInfo r;
    r.id = id;
    r.name = label;
    r.top_y = top;
    r.level = level;
    r.patch_count = ring_patches(r.patches, kStackHalf, kStackBand);
    return r;
}

RegionInfo ring(const Region id, const std::string_view label, const int k, const int level) {
    RegionInfo r;
    r.id = id;
    r.name = label;
    r.top_y = 176.25F + 22.0F * float(k);
    r.level = level;
    const float mid = kRing176Mid - kRingStep * float(k);
    r.patch_count = ring_patches(r.patches, mid + 0.5F * kRingBand, kRingBand);
    return r;
}

// Along the square loop of half-size `half` round the shaft: the walk from one
// point to the other the short way round, plus each one's step off the loop.
float loop_walk(const float half, const float ax, const float az, const float bx, const float bz) {
    const auto along = [half](const float x, const float z, float &off) {
        const float u = std::clamp(x, -half, half);
        const float w = std::clamp(z - kShaftZ, -half, half);
        // The nearest side of the square, and the distance along the loop to it.
        const float dn = std::abs(w - half), ds = std::abs(w + half), de = std::abs(u - half), dw = std::abs(u + half);
        const float best = std::min({dn, ds, de, dw});
        float t = 0.0F;
        float px = u, pz = w;
        if (best == dn) {
            pz = half;
            t = half - u;                      // north side, east to west
        } else if (best == dw) {
            px = -half;
            t = 2.0F * half + (half - w);      // west side, north to south
        } else if (best == ds) {
            pz = -half;
            t = 4.0F * half + (u + half);      // south side, west to east
        } else {
            px = half;
            t = 6.0F * half + (w + half);      // east side, south to north
        }
        off = std::hypot(x - px, (z - kShaftZ) - pz);
        return t;
    };
    float off_a = 0.0F, off_b = 0.0F;
    const float ta = along(ax, az, off_a);
    const float tb = along(bx, bz, off_b);
    const float perimeter = 8.0F * half;
    const float d = std::abs(ta - tb);
    return std::min(d, perimeter - d) + off_a + off_b;
}

float loop_half(const RegionInfo &r) {
    if (r.patch_count < 4) {
        return 0.0F;
    }
    // The band's middle line: half way between the north band's two edges.
    return 0.5F * (r.patches[0].min_z + r.patches[0].max_z) - kShaftZ;
}

float walk_m(const RegionInfo &r, const float ax, const float az, const float bx, const float bz) {
    const float half = loop_half(r);
    return half > 0.0F ? loop_walk(half, ax, az, bx, bz) : std::hypot(ax - bx, az - bz);
}

} // namespace

Map::Map() {
    // Her walking lines are Matron::levels_ (matron.cpp): 0 the home deck,
    // 1..3 the 176/198/220 rings, 12 + k the stack deck at 11k m, 25 the bracket.
    regions_[std::size_t(Region::Deck132)] = stack_deck(Region::Deck132, "deck 132", 132.0F, 23);
    regions_[std::size_t(Region::Deck143)] = stack_deck(Region::Deck143, "deck 143", 143.0F, 24);
    // At 143 a walkway plate runs out from the north band under C3's riser
    // (band_stack.cpp): the riser's foot.
    {
        RegionInfo &r = regions_[std::size_t(Region::Deck143)];
        r.patches[r.patch_count++] = {-11.5F, -5.0F, -135.35F, -133.0F};
    }
    regions_[std::size_t(Region::Deck154)] = stack_deck(Region::Deck154, "deck 154", 154.0F, 0);
    {
        // The bay cuts the north band at 154 (bay.hpp kWestX..kEastX), and the
        // catwalk crosses the shaft at z -150 (matron.cpp, its planks).
        RegionInfo &r = regions_[std::size_t(Region::Deck154)];
        const Patch north = r.patches[0];
        r.patches[0] = {north.min_x, -3.9F, north.min_z, north.max_z};
        r.patches[r.patch_count++] = {-0.4F, north.max_x, north.min_z, north.max_z};
        r.patches[r.patch_count++] = {-16.94F, 16.94F, -150.8F, -149.2F, true};
    }
    {
        RegionInfo &r = regions_[std::size_t(Region::Bracket152)];
        r.id = Region::Bracket152;
        r.name = "bay bracket 152.22";
        r.top_y = 152.22F;
        r.level = 25;
        r.patches[0] = {-3.9F, -2.9F, -131.25F, -129.35F};
        r.patch_count = 1;
    }
    regions_[std::size_t(Region::Ring176)] = ring(Region::Ring176, "ring 176.25", 0, 1);
    regions_[std::size_t(Region::Ring198)] = ring(Region::Ring198, "ring 198.25", 1, 2);
    {
        // The Counterweight Well's cages at their stops (band_counterweight_well.cpp:
        // 3.0 x 2.8 m floors on z -131.4): stage A's up at the 176 ring, stage B's
        // at the 176 ring below and the 198 ring above. Her walking lines on the
        // 176 ring run into both (Matron::levels_).
        constexpr Patch kCageA{-12.0F, -9.0F, -132.8F, -130.0F, true};
        constexpr Patch kCageB{-8.7F, -5.7F, -132.8F, -130.0F, true};
        RegionInfo &r176 = regions_[std::size_t(Region::Ring176)];
        r176.patches[r176.patch_count++] = kCageA;
        r176.patches[r176.patch_count++] = kCageB;
        RegionInfo &r198 = regions_[std::size_t(Region::Ring198)];
        r198.patches[r198.patch_count++] = kCageB;
    }
    regions_[std::size_t(Region::Ring220)] = ring(Region::Ring220, "ring 220.25", 2, 3);

    // The ladders, from their builders. The home ladder stands on the 154
    // deck against the 176 ring's inner face (band_counterweight_well.cpp
    // build_climbing_route); C3's riser rises from the 143 walkway plate over
    // the 154 rail (band_stack.cpp). She climbs both on their shaft side
    // (south) and tops out north.
    ladders_[std::size_t(Connector::HomeLadder)] =
        LadderInfo{5.00F, -129.05F, 0.28F, 154.30F, 176.20F, 0.30F, -1.0F, 1.0F, 154.00F, 176.25F};
    ladders_[std::size_t(Connector::RiserLadder)] =
        LadderInfo{-6.00F, -133.50F, 0.28F, 143.40F, 155.15F, 0.30F, -1.0F, 1.0F, 143.00F, 154.00F};

    // Every way starts Unknown: none is planned over until its executor has
    // taken her along it, this direction, in a test (set_status).
    using Do = Step::Do;
    const auto walk = [](const float x, const float z) { return Step{Do::Walk, Connector::Count, x, z}; };
    const auto up = [](const Connector c) { return Step{Do::ClimbUp, c, 0.0F, 0.0F}; };
    const auto down = [](const Connector c) { return Step{Do::ClimbDown, c, 0.0F, 0.0F}; };
    // The home ladder: from the north band round its west rail to its foot,
    // up, and onto the ring; down from the ring's edge over its head to the
    // foot, and back round the rail.
    add({Region::Deck154, Region::Ring176, Kind::LadderUp, Connector::HomeLadder, 3.90F, -128.50F, 5.00F, -127.50F,
         40.0F},
        {walk(3.90F, -129.55F), walk(5.00F, -129.55F), up(Connector::HomeLadder), walk(5.00F, -127.50F)});
    add({Region::Ring176, Region::Deck154, Kind::LadderDown, Connector::HomeLadder, 5.00F, -127.50F, 3.90F, -128.50F,
         40.0F},
        {walk(5.00F, -128.55F), down(Connector::HomeLadder), walk(3.90F, -129.55F), walk(3.90F, -128.50F)});
    // C3's riser: from the 143 band out onto its plate to the foot, up, over
    // the bridge and down its steps to 154's runway; down, from the runway up
    // the steps onto the bridge, over its lip, and off the plate onto 143.
    add({Region::Deck143, Region::Deck154, Kind::LadderUp, Connector::RiserLadder, -6.00F, -131.00F, -6.00F, -129.50F,
         25.0F},
        {walk(-6.00F, -133.85F), up(Connector::RiserLadder), walk(-6.00F, -131.20F), walk(-6.00F, -129.50F)});
    add({Region::Deck154, Region::Deck143, Kind::LadderDown, Connector::RiserLadder, -6.00F, -129.50F, -6.00F,
         -131.00F, 25.0F},
        {walk(-6.00F, -131.20F), walk(-6.00F, -132.85F), down(Connector::RiserLadder), walk(-6.00F, -131.00F)});
    // C3 below the riser, 132 to 143: from the 132 south band up the stepped
    // walkway across the shaft to its landing, up the duct's face, along the
    // duct to its wall ladder, up onto the 143 plate; and all of it back down.
    add({Region::Deck132, Region::Deck143, Kind::LadderUp, Connector::C3WallLadder, -10.50F, -171.00F, -10.50F,
         -131.00F, 45.0F},
        {walk(-10.50F, -145.00F), walk(-10.50F, -144.40F), up(Connector::C3Duct), walk(-10.50F, -135.85F),
         up(Connector::C3WallLadder), walk(-10.50F, -131.00F)});
    add({Region::Deck143, Region::Deck132, Kind::LadderDown, Connector::C3WallLadder, -10.50F, -131.00F, -10.50F,
         -171.00F, 45.0F},
        {walk(-10.50F, -134.85F), down(Connector::C3WallLadder), walk(-10.50F, -143.40F), down(Connector::C3Duct),
         walk(-10.50F, -145.00F), walk(-10.50F, -171.00F)});
    // The corner pipe, both ways between 132, 143 and 154 (no executor that
    // is physical yet: Unknown); the bay's rungs and lip (her trap's own).
    add({Region::Deck154, Region::Deck143, Kind::PipeDown, Connector::CornerPipe, 15.95F, -132.25F, 15.95F, -132.25F,
         15.0F});
    add({Region::Deck143, Region::Deck154, Kind::PipeUp, Connector::CornerPipe, 15.95F, -132.25F, 15.95F, -132.25F,
         30.0F});
    add({Region::Deck143, Region::Deck132, Kind::PipeDown, Connector::CornerPipe, 15.95F, -132.25F, 15.95F, -132.25F,
         15.0F});
    add({Region::Deck132, Region::Deck143, Kind::PipeUp, Connector::CornerPipe, 15.95F, -132.25F, 15.95F, -132.25F,
         30.0F});
    add({Region::Bracket152, Region::Deck154, Kind::RungsUp, Connector::BayRungs, -3.60F, -130.15F, -4.35F, -130.15F,
         8.0F});
    add({Region::Deck154, Region::Bracket152, Kind::DropDown, Connector::BayLip, -4.35F, -130.15F, -3.45F, -129.85F,
         4.0F});

    // Qualified: her hands have taken her along these, these ways, in tests
    // (tests/sim_tests_matron_traverse.cpp each climb both ways; the routes
    // themselves end to end).
    for (const auto &[from, to, kind] : {std::tuple{Region::Deck154, Region::Ring176, Kind::LadderUp},
                                         std::tuple{Region::Ring176, Region::Deck154, Kind::LadderDown},
                                         std::tuple{Region::Deck143, Region::Deck154, Kind::LadderUp},
                                         std::tuple{Region::Deck154, Region::Deck143, Kind::LadderDown},
                                         std::tuple{Region::Deck132, Region::Deck143, Kind::LadderUp},
                                         std::tuple{Region::Deck143, Region::Deck132, Kind::LadderDown}}) {
        set_status(find(from, to, kind), Status::Qualified, Reason::None);
    }
}

void Map::add(const Edge &edge, const std::initializer_list<Step> steps) noexcept {
    if (edge_count_ >= kMaxEdges) {
        return;
    }
    Edge &e = edges_[edge_count_++];
    e = edge;
    for (const Step &step : steps) {
        if (e.step_count < kMaxSteps) {
            e.steps[e.step_count++] = step;
        }
    }
}

Region Map::region_at(const float x, const float feet_y, const float z) const noexcept {
    Region best = Region::Count;
    float best_dy = kOnTopM;
    for (const RegionInfo &r : regions_) {
        const float dy = std::abs(feet_y - r.top_y);
        if (r.id == Region::Count || dy > best_dy) {
            continue;
        }
        for (std::uint8_t i = 0; i < r.patch_count; ++i) {
            if (r.patches[i].holds(x, z)) {
                best = r.id;
                best_dy = dy;
                break;
            }
        }
    }
    return best;
}

std::size_t Map::find(const Region from, const Region to, const Kind kind) const noexcept {
    for (std::size_t i = 0; i < edge_count_; ++i) {
        if (edges_[i].from == from && edges_[i].to == to && edges_[i].kind == kind) {
            return i;
        }
    }
    return kMaxEdges;
}

void Map::set_status(const std::size_t index, const Status status, const Reason reason) noexcept {
    if (index < edge_count_) {
        edges_[index].status = status;
        edges_[index].reason = reason;
    }
}

bool Map::usable(const std::size_t index, const float now_s) const noexcept {
    const Edge &e = edges_[index];
    return e.status == Status::Qualified && now_s >= e.blocked_until_s;
}

void Map::fail(const std::size_t index, const Reason reason, const float now_s) noexcept {
    Edge &e = edges_[index];
    e.reason = reason;
    if (e.failures < 255) {
        ++e.failures;
    }
    e.blocked_until_s = e.failures > kMaxRetries ? std::numeric_limits<float>::infinity() : now_s + kFailedBlockS;
}

void Map::succeed(const std::size_t index) noexcept {
    Edge &e = edges_[index];
    e.failures = 0;
    e.blocked_until_s = 0.0F;
    e.reason = Reason::None;
}

void Map::unblock_all() noexcept {
    for (std::size_t i = 0; i < edge_count_; ++i) {
        if (edges_[i].status == Status::Qualified) {
            succeed(i);
        }
    }
}

bool Map::plan(const Region from, const float x, const float z, const Region to, const float now_s,
               const float walk_mps, Path &out) const noexcept {
    out = Path{};
    if (from == to) {
        return true;
    }
    if (from == Region::Count || to == Region::Count || walk_mps <= 0.0F) {
        return false;
    }
    // Dijkstra over the edges: dist[e] is the time to stand at e's exit.
    std::array<float, kMaxEdges> dist{};
    std::array<std::int16_t, kMaxEdges> prev{};
    std::array<bool, kMaxEdges> done{};
    dist.fill(kUnreachable);
    prev.fill(-1);
    for (std::size_t i = 0; i < edge_count_; ++i) {
        const Edge &e = edges_[i];
        if (e.from == from && usable(i, now_s)) {
            dist[i] = walk_m(region(from), x, z, e.entry_x, e.entry_z) / walk_mps + e.cost_s;
        }
    }
    std::size_t expansions = 0;
    int goal = -1;
    while (expansions < kMaxExpansions) {
        int best = -1;
        for (std::size_t i = 0; i < edge_count_; ++i) {
            if (!done[i] && dist[i] < kUnreachable && (best < 0 || dist[i] < dist[std::size_t(best)])) {
                best = int(i);
            }
        }
        if (best < 0) {
            break;
        }
        ++expansions;
        const Edge &e = edges_[std::size_t(best)];
        done[std::size_t(best)] = true;
        if (e.to == to) {
            goal = best;
            break;
        }
        for (std::size_t j = 0; j < edge_count_; ++j) {
            const Edge &n = edges_[j];
            if (done[j] || n.from != e.to || !usable(j, now_s)) {
                continue;
            }
            const float d = dist[std::size_t(best)] +
                            walk_m(region(e.to), e.exit_x, e.exit_z, n.entry_x, n.entry_z) / walk_mps + n.cost_s;
            if (d < dist[j]) {
                dist[j] = d;
                prev[j] = std::int16_t(best);
            }
        }
    }
    if (goal < 0) {
        return false;
    }
    std::array<std::uint8_t, kMaxPath> reversed{};
    std::uint8_t n = 0;
    for (int i = goal; i >= 0 && n < kMaxPath; i = prev[std::size_t(i)]) {
        reversed[n++] = std::uint8_t(i);
    }
    for (std::uint8_t i = 0; i < n; ++i) {
        out.edges[i] = reversed[n - 1 - i];
    }
    out.count = n;
    out.cost_s = dist[std::size_t(goal)];
    return true;
}

std::string_view name(const Region id) noexcept {
    switch (id) {
    case Region::Deck132: return "deck 132";
    case Region::Deck143: return "deck 143";
    case Region::Deck154: return "deck 154";
    case Region::Bracket152: return "bay bracket 152.22";
    case Region::Ring176: return "ring 176.25";
    case Region::Ring198: return "ring 198.25";
    case Region::Ring220: return "ring 220.25";
    case Region::Count: break;
    }
    return "none";
}

std::string_view name(const Reason reason) noexcept {
    switch (reason) {
    case Reason::None: return "none";
    case Reason::Untested: return "no executor yet";
    case Reason::Clearance: return "her body does not fit";
    case Reason::Capability: return "needs a hold she cannot take";
    case Reason::Failed: return "failed under her";
    case Reason::Stage: return "not at her stage";
    }
    return "?";
}

} // namespace scraperx::sim::routes
