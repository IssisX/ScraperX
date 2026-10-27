// The Matron's map of the tower (sim/matron_routes.hpp): regions of real
// support told apart by where they are, not by a height; ways between them
// one direction each, planned only once qualified, blocked when they fail;
// and the map's tops checked against the steel itself.

#include "sim_tests.hpp"

#include "sim/matron_routes.hpp"

namespace {
namespace routes = scraperx::sim::routes;
using routes::Kind;
using routes::Reason;
using routes::Region;
using routes::Status;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

// A map with no way yet shown to carry her: the planning rules alone.
routes::Map bare_map() {
    routes::Map map;
    for (std::size_t i = 0; i < map.edge_count(); ++i) {
        map.set_status(i, Status::Unknown, Reason::Untested);
    }
    return map;
}

void qualify(routes::Map &map, const Region from, const Region to, const Kind kind) {
    const std::size_t i = map.find(from, to, kind);
    require(i < routes::kMaxEdges, "matron routes: the way is on the map");
    map.set_status(i, Status::Qualified, Reason::None);
}

void run_regions() {
    const routes::Map map;
    // The same x,z on two decks is two places.
    require(map.region_at(10.0F, 154.0F, -128.5F) == Region::Deck154, "matron routes: the 154 band");
    require(map.region_at(10.0F, 143.0F, -128.5F) == Region::Deck143, "matron routes: the 143 band, same x,z");
    require(map.region_at(10.0F, 132.02F, -128.5F) == Region::Deck132, "matron routes: the 132 band");
    // Between floors, over the shaft, in the bay's cut: nowhere.
    require(map.region_at(10.0F, 148.5F, -128.5F) == Region::Count, "matron routes: between two decks is no deck");
    require(map.region_at(0.0F, 154.0F, -150.0F + 5.0F) == Region::Count, "matron routes: over the shaft is no deck");
    require(map.region_at(-2.0F, 154.0F, -128.5F) == Region::Count, "matron routes: the bay's cut is no deck");
    // The catwalk across the shaft is the home deck's; the plate under C3's
    // riser is 143's; the bracket is its own.
    require(map.region_at(0.0F, 154.0F, -150.0F) == Region::Deck154, "matron routes: the catwalk");
    require(map.region_at(-6.0F, 143.0F, -134.2F) == Region::Deck143, "matron routes: the riser's foot plate");
    require(map.region_at(-3.4F, 152.22F, -130.2F) == Region::Bracket152, "matron routes: the bay bracket");
    // The rings: on the band, and not in the shaft beside it.
    require(map.region_at(5.0F, 176.25F, -127.5F) == Region::Ring176, "matron routes: the 176 ring");
    require(map.region_at(5.0F, 176.25F, -129.5F) == Region::Count, "matron routes: inside the 176 ring's edge");
    require(map.region_at(7.0F, 198.25F, -127.0F) == Region::Ring198, "matron routes: the 198 ring");
    require(map.region_at(7.0F, 220.25F, -129.0F) == Region::Ring220, "matron routes: the 220 ring");
    std::cout << "PASS scraperx_sim matron routes regions\n";
}

void run_directed() {
    routes::Map map = bare_map();
    routes::Path path;
    // Nothing is planned over before it is qualified.
    require(!map.plan(Region::Deck154, 5.0F, -129.0F, Region::Ring176, 0.0F, 3.0F, path),
            "matron routes: an unqualified way is not a way");
    qualify(map, Region::Deck154, Region::Ring176, Kind::LadderUp);
    require(map.plan(Region::Deck154, 5.0F, -129.0F, Region::Ring176, 0.0F, 3.0F, path) && path.count == 1,
            "matron routes: up the home ladder");
    // Up is not down.
    require(!map.plan(Region::Ring176, 5.0F, -127.0F, Region::Deck154, 0.0F, 3.0F, path),
            "matron routes: a way up is not a way down");
    qualify(map, Region::Ring176, Region::Deck154, Kind::LadderDown);
    require(map.plan(Region::Ring176, 5.0F, -127.0F, Region::Deck154, 0.0F, 3.0F, path) && path.count == 1 &&
                map.edge(path.edges[0]).kind == Kind::LadderDown,
            "matron routes: down the home ladder");
    // Two hops, in order: 143 up the riser to 154, then up the ladder.
    qualify(map, Region::Deck143, Region::Deck154, Kind::LadderUp);
    require(map.plan(Region::Deck143, -6.0F, -133.9F, Region::Ring176, 0.0F, 3.0F, path) && path.count == 2 &&
                map.edge(path.edges[0]).connector == routes::Connector::RiserLadder &&
                map.edge(path.edges[1]).connector == routes::Connector::HomeLadder,
            "matron routes: two decks up, the riser then the ladder");
    // Where she stands chooses: down by the pipe from beside it, by the riser
    // from beside that.
    qualify(map, Region::Deck154, Region::Deck143, Kind::PipeDown);
    qualify(map, Region::Deck154, Region::Deck143, Kind::LadderDown);
    require(map.plan(Region::Deck154, 15.0F, -128.5F, Region::Deck143, 0.0F, 1.0F, path) &&
                map.edge(path.edges[0]).connector == routes::Connector::CornerPipe,
            "matron routes: down by the pipe beside it");
    // From the far side of the deck the pipe is the long way round the shaft.
    require(map.plan(Region::Deck154, -21.5F, -140.0F, Region::Deck143, 0.0F, 1.0F, path) &&
                map.edge(path.edges[0]).connector == routes::Connector::RiserLadder,
            "matron routes: down by the riser from the west side");
    std::cout << "PASS scraperx_sim matron routes directed\n";
}

void run_blocked() {
    routes::Map map = bare_map();
    routes::Path path;
    qualify(map, Region::Deck154, Region::Ring176, Kind::LadderUp);
    const std::size_t up = map.find(Region::Deck154, Region::Ring176, Kind::LadderUp);
    map.fail(up, Reason::Failed, 10.0F);
    require(!map.plan(Region::Deck154, 5.0F, -129.0F, Region::Ring176, 20.0F, 3.0F, path),
            "matron routes: a failed way is left alone a while");
    require(map.plan(Region::Deck154, 5.0F, -129.0F, Region::Ring176, 10.0F + routes::kFailedBlockS, 3.0F, path),
            "matron routes: and tried again after it");
    // Past its retries it stays blocked whatever she hears, until the world
    // changes (a restore).
    for (int i = 0; i < 3; ++i) {
        map.fail(up, Reason::Failed, 100.0F);
    }
    require(!map.plan(Region::Deck154, 5.0F, -129.0F, Region::Ring176, 1.0e6F, 3.0F, path),
            "matron routes: a way that kept failing stays blocked");
    require(map.edge(up).reason == Reason::Failed, "matron routes: with its reason");
    map.unblock_all();
    require(map.plan(Region::Deck154, 5.0F, -129.0F, Region::Ring176, 0.0F, 3.0F, path),
            "matron routes: a restore clears it");
    std::cout << "PASS scraperx_sim matron routes blocked\n";
}

// The map's tops are the steel's: straight down onto every patch's middle
// (and its corners, 0.3 m in) the first thing met is at the region's top.
void run_tops_are_steel() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    const routes::Map map;
    int checked = 0;
    for (std::size_t r = 0; r < routes::kRegionCount; ++r) {
        const routes::RegionInfo &info = map.region(static_cast<Region>(r));
        for (std::uint8_t p = 0; p < info.patch_count; ++p) {
            const routes::Patch &patch = info.patches[p];
            if (patch.moving) {
                continue;   // a machine's floor: steel there only while it is
            }
            const double cx = 0.5 * (patch.min_x + patch.max_x);
            const double cz = 0.5 * (patch.min_z + patch.max_z);
            const bool along_x = patch.max_x - patch.min_x >= patch.max_z - patch.min_z;
            // Machines and posts stand on the bands; a sample that lands on one
            // reads its top, so try points along the patch for the bare deck.
            bool found = false;
            double nearest = 1.0e9;
            for (int k = 0; k < 9 && !found; ++k) {
                const double t = 0.1 + 0.1 * k;
                const double x = along_x ? patch.min_x + (patch.max_x - patch.min_x) * t : cx;
                const double z = along_x ? cz : patch.min_z + (patch.max_z - patch.min_z) * t;
                const double y = s.debug_support_y(Vector3{x, info.top_y + 0.05, z}, 0.5);
                if (!std::isnan(y)) {
                    nearest = std::min(nearest, std::abs(y - info.top_y));
                    found = std::abs(y - info.top_y) < 0.03;
                }
            }
            if (!found) {
                std::cerr << "  " << info.name << " patch " << int(p) << ": nearest top off by " << nearest << '\n';
            }
            require(found, "matron routes: a region's top is the steel under it");
            ++checked;
        }
    }
    std::cout << "PASS scraperx_sim matron routes tops: patches=" << checked << '\n';
}

} // namespace

void run_matron_routes() {
    run_regions();
    run_directed();
    run_blocked();
    run_tops_are_steel();
}
