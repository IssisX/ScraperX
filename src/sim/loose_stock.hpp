#pragma once

// Loose stock: the things lying about the tower that are not holding it up
// (the owner, 2026-10-17: "if SOME objects are moveable, MOST objects should
// be moveable"). Crates, drums, pallets, timber, steel beams, plate, pipe and
// shipping containers: each a real body with no rule of its own. The player
// carries the light ones (the carry's grip holds up to about 70 kg), shoves
// the middling ones by walking into them and stacks them to reach a ledge;
// the heaviest wait for a machine big enough to move them. What they do
// comes from that. The tower's frame, decks, stairs and machines stay put.
//
// They sleep once at rest, so a yard full of them costs nothing while it is
// left alone; anything that touches one wakes it. They are saved and
// restored with the machines like every kit body.
//
// Up the tower (the owner, 2026-10-17: little lived-in spots all over the
// structure, not only the yard): dress_tower() finds, on every deck ring of
// the stack and the upper rings, places against the shaft's railed edge with
// room to stand the work there and a walkway past it (back from the edge
// where it has no rail), and leaves a little
// spot of stock in each -- a step of skid crates, gas bottles and a toolbox,
// a reel of cable on its pallet and planks, a pallet of drums. A
// place is taken only where the world says it fits: a flat, level floor under
// every corner, nothing in the way above it, no machine near it and the next
// spot well along the ring.
//
// Every heavy piece (more than the hands carry) sits on a pallet or on skid
// runners, so a forklift's forks fit under it wherever it is. And nothing a
// forklift or a crane had to put in place is set where neither could have
// (the owner, 2026-10-17): the steel, the plate and the crane's cages stand
// only on the decks the yard crane lands loads on (yard_crane.hpp: 11, 22 and
// 44 m), which also have the most spots.
//
// A sudden blow to a piece is a hit in the impact ledger (impacts.hpp),
// heard by its sound (sound()) and by the Matron.

#include "sim/mechanism_kit.hpp"
#include "sim/simulation.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <vector>

namespace scraperx::sim {

class LooseStock final {
public:
    // Kit entities: piece i of the table is kFirstEntity + i.
    static constexpr std::uint64_t kFirstEntity = kit::kStockEntityBase;

    enum class Kind : std::uint8_t {
        Crate = 0,          // 0.96 x 0.88 x 0.96 m timber crate, 160 kg: shoved
        IngotDolly = 1,     // a box of ingots on a rolling dolly, 420 kg
        TimberBeam = 2,     // 4 m baulk of timber, 70 kg
        Drum = 3,           // an empty oil drum on its end, 55 kg: carried
        Pallet = 4,         // 1.2 x 1.0 m, 22 kg: carried
        Container = 5,      // a 40 ft shipping container, empty, 3,750 kg
        SteelBeam = 6,      // a 10 m steel beam, 600 kg
        Sleeper = 7,        // a 3 m hardwood sleeper, 90 kg
        PipeBearer = 8,     // a 2.4 m steel cradle with a post at each end, 250 kg
        PlateStack = 9,     // a 3 x 1.5 m stack of steel plate, 8 t
        PlateSlab = 10,     // a 2.4 x 1.2 m slab of plate, 2 t
        Pipe = 11,          // an 8 m steel pipe, 0.6 m across, 700 kg
        BilletStillage = 12,  // a stillage of steel billets on runners, 5 t: more than a forklift's load
        CoilSkid = 13,        // a coil of steel strip cradled on a skid, 2.8 t: a forklift's hard lift
        GasBottle = 14,       // a welding gas bottle stood on end, 60 kg: carried
        CableReel = 15,       // a wooden reel of cable on its rims, 180 kg: rolls
        Toolbox = 16,         // a steel toolbox, 18 kg: carried
        PlankBundle = 17,     // 1.8 m planks strapped in a bundle, 60 kg
        // Up the tower (the owner, 2026-10-17: every heavy piece on a pallet,
        // a skid or dunnage, so a forklift's forks fit under it anywhere):
        SkidCrate = 18,       // a timber crate built on its own skids, 160 kg: shoved
        PalletReel = 19,      // a reel of cable lying strapped to a pallet, 202 kg
        PlateSkid = 20,       // a 2 t slab of plate on two skid runners, lugged for a crane
        BottleCage = 21,      // a crane's lifting cage of nine gas bottles on runners, 690 kg
        // A machine's weight (lay_machine_weights):
        Pig = 22,             // a pig of iron with a lifting lug, 50 kg: carried
        // J-042 (Cory, 11-05: everything loose she can lift is hers to throw):
        Extinguisher = 23,    // a fire extinguisher stood on end, 9 kg: carried
    };
    static constexpr int kKinds = 24;
    // Heavier than the hands carry (the carry's grip holds about 70 kg): a
    // forklift's or a crane's to move, set where one of them could have put it.
    [[nodiscard]] static bool heavy(Kind kind) noexcept;
    // What a piece of this kind sounds like when it is struck.
    [[nodiscard]] static ImpactSound sound(Kind kind) noexcept;
    // A forklift's forks fit under it: a pallet, a skid's runners, dunnage.
    [[nodiscard]] static bool forkable(Kind kind) noexcept;

    struct Placement {
        Kind kind;
        std::uint8_t variant;  // its paint, for the presentation (no physics)
        float x, y, z;         // where its underside sits, its centre over (x, z)
        float yaw;             // about +y
        // J-038 slice 0: its stated load in kg, carried on top of the kind's
        // own mass. Only containers state one today: a 40 ft box's load,
        // centred, so its body is 3,750 kg plus this. Default: empty.
        float load_kg = 0.0F;
    };

    LooseStock(JPH::PhysicsSystem &physics, kit::Kit &kit);

    // Finds the tower's spots and leaves the stock in them (see above): after
    // every other body of the world is built, so the places it takes are
    // tested against all of them. Its pieces follow the table's.
    // Nothing is set within keep_clear_radius of a point of keep_clear (the
    // places the player is put down).
    // The pre-world-b spots are built here; the new east/west spots are
    // only recorded here (their places, no entities and no bodies yet), and
    // their table places are taken by append_reserved() after the Matron's
    // throwables, so every pre-existing entity keeps its id and every
    // pre-existing body keeps its build order. Their bodies are built by
    // realize_reserved once the bay, the twin tower and the summit stand.
    // Until append_reserved() runs the east/west pieces have no table
    // place, so east_west_begin() is count(). The bands' rings above
    // 396.25 m and the top decks are recorded the same way, and their
    // table places are taken by append_upper() after the east/west
    // pieces; until it runs, upper_begin() is count(). A ring above
    // 396.25 m left with no carryable piece gets one toolbox, recorded in
    // its own list; its table places are taken by append_hightower()
    // after the Matron's kinds, so no pre-existing piece or body moves at
    // all, and until it runs, hightower_begin() is count().
    void dress_tower(const std::vector<JPH::RVec3> &keep_clear, float keep_clear_radius);
    // Takes the east/west places dress_tower recorded, at the end of the
    // table (after the Matron's throwables), so the crane steel, the
    // machine weights and the throwables keep the entities they had before
    // the east/west stock existed and saves made before it still load.
    // Their bodies stay unbuilt (invalid BodyIndex) until realize_reserved.
    void append_reserved();
    // Builds the bodies dress_tower reserved for the east/west pieces,
    // next in the kit's build order, and returns their places in the
    // table. The upper pieces are built separately (realize_upper),
    // after the deck-154 forklift, so the forklift keeps its place right
    // after the east/west pieces.
    [[nodiscard]] std::vector<int> realize_reserved();
    // Builds the bodies dress_tower reserved for the upper pieces, after
    // the deck-154 forklift, and returns their places in the table.
    [[nodiscard]] std::vector<int> realize_upper();
    // Takes the upper places dress_tower recorded, at the end of the
    // table (after the east/west pieces), so the crane steel, the
    // machine weights, the throwables and the east/west pieces keep the
    // entities they had before the upper stock existed and saves made
    // before it still load. Their bodies stay unbuilt (invalid
    // BodyIndex) until realize_upper.
    void append_upper();
    // Takes the hightower places dress_tower recorded, at the end of the
    // table (after the Matron's kinds), so every piece laid before them
    // keeps the entity it had and saves made before them still load.
    // Their bodies stay unbuilt (invalid BodyIndex) until realize_hightower.
    void append_hightower();
    // Builds the bodies dress_tower reserved for the hightower pieces,
    // last of all, and returns their places in the table.
    [[nodiscard]] std::vector<int> realize_hightower();
    // The steel for the yard crane's magnet (yard_crane.hpp), laid in its
    // hook's reach after the tower's stock, so saves made before it line up:
    // two slabs of plate one on the other, and two beams side by side, the
    // kCraneSteelPieces pieces starting at crane_steel_begin().
    void lay_crane_steel();
    static constexpr int kCraneSteelPieces = 4;
    // The weights machines are started with, laid after the crane's devices
    // (yard_crane.hpp), so saves made before them line up: the pig of iron
    // on deck 8 by S3's cage, for the pan on its brake's trip line
    // (band_stack.cpp). The kMachineWeights pieces starting at
    // machine_weights_begin().
    void lay_machine_weights();
    static constexpr int kMachineWeights = 1;
    // What the Matron takes and throws as a pest (matron.hpp, her mischief):
    // two toolboxes, and an oil drum too heavy for her (the player's, to drop
    // on her), by the outer rail of her deck's north band, near the way out. Laid after the machine weights, so saves made
    // before them line up. The kMatronThrowables pieces starting at
    // matron_throwables_begin().
    void lay_matron_throwables();
    static constexpr int kMatronThrowables = 3;
    // J-042: two more kinds by her deck's north band, west of the drum,
    // so what she takes and throws is no one kind: a 9 kg extinguisher and
    // a 22 kg pallet. Laid after the deck's forklift and the upper stock
    // (the hightower stock comes after them), so
    // every earlier piece and body keeps its place. The kMatronKinds pieces
    // starting at matron_kinds_begin().
    void lay_matron_kinds();
    static constexpr int kMatronKinds = 2;
    // Where a hand takes a carried piece of this kind, in its own frame.
    [[nodiscard]] static JPH::Vec3 carry_handle(Kind kind) noexcept;

    [[nodiscard]] int count() const noexcept { return static_cast<int>(pieces_.size()); }
    // Every piece as placed, the table's and then the tower's.
    [[nodiscard]] const std::vector<Placement> &placed() const noexcept { return placed_; }
    [[nodiscard]] kit::BodyIndex body(int index) const noexcept {
        return pieces_[static_cast<std::size_t>(index)];
    }
    // Where each group begins in the table: static, then the tower's
    // spots, then the crane steel, the machine weights, the Matron's
    // throwables, then the east/west stock, then the upper stock, then the
    // Matron's kinds, and last the hightower stock, so saves made before a
    // group line up. Until append_reserved() runs, the east/west begin is
    // count(); until append_upper() runs, the upper begin is count();
    // until append_hightower() runs, the hightower begin is count().
    [[nodiscard]] int crane_steel_begin() const noexcept { return crane_steel_begin_; }
    [[nodiscard]] int machine_weights_begin() const noexcept { return machine_weights_begin_; }
    [[nodiscard]] int matron_throwables_begin() const noexcept { return matron_throwables_begin_; }
    [[nodiscard]] int east_west_begin() const noexcept { return east_west_begin_; }
    [[nodiscard]] int upper_begin() const noexcept { return upper_begin_; }
    [[nodiscard]] int matron_kinds_begin() const noexcept { return matron_kinds_begin_; }
    [[nodiscard]] int hightower_begin() const noexcept { return hightower_begin_; }

    // The pieces as placed, in order (also the order of their entities).
    [[nodiscard]] static const std::vector<Placement> &placements();
    // The Matron's deck's own seven, first in the table.
    static constexpr int kDeckStackLow = 0;      // two crates side by side and one on top
    static constexpr int kDeckCrateNorth = 3;    // a loose crate north of the catwalk's east end
    static constexpr int kDeckCrateSouth = 4;
    static constexpr int kDeckIngotDolly = 5;
    static constexpr int kDeckBeam = 6;

private:
    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    std::vector<kit::BodyIndex> pieces_;
    std::vector<Placement> placed_;
    // The east/west places dress_tower recorded, waiting for
    // append_reserved() to take their table places at the end.
    std::vector<Placement> pending_east_west_;
    // The bands' rings above 396.25 m and the top decks dress_tower
    // recorded, waiting for append_upper() to take their table places
    // after the east/west pieces.
    std::vector<Placement> pending_upper_;
    // The hightower toolboxes dress_tower recorded, one on each upper ring
    // left with no carryable piece, waiting for append_hightower() to take
    // their table places after the Matron's kinds.
    std::vector<Placement> pending_hightower_;
    // Where each group begins in the table (see the accessors above).
    int crane_steel_begin_ = 0;
    int machine_weights_begin_ = 0;
    int matron_throwables_begin_ = 0;
    int east_west_begin_ = 0;
    int upper_begin_ = 0;
    int matron_kinds_begin_ = 0;
    int hightower_begin_ = 0;
    // Puts one piece in the world, as placed.
    void add_piece(const Placement &placement);
    // Builds one piece's body for an entity already chosen, as placed.
    [[nodiscard]] kit::BodyIndex create_body(const Placement &placement, std::uint64_t entity);
    // Builds the bodies of the table places in [begin, end) that have
    // none yet, in table order, and returns their places.
    [[nodiscard]] std::vector<int> realize_range(int begin, int end);
};

} // namespace scraperx::sim
