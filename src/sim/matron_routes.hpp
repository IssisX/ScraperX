#pragma once

// Her map of the tower (the owner, 2026-10-21: "I really want her to be able
// to climb through more decks, up and down"; the multi-deck proposal on
// grok/matron-multi-deck, 03_EXECUTION/MATRON_REDESIGN/MULTI_DECK_PROPOSAL.md).
//
// Regions are patches of real support: a deck's bands, a ring, a bracket. A
// region is known by where it is, its footprint and the top of its steel,
// never by a height alone: two decks share every x,z of their bands.
//
// Edges are the ways between them, one direction each: up a ladder and down
// it are two edges, either of which can be missing or blocked. An edge names
// the steel it goes by (a Connector) and how her body does it (a Kind); the
// traversal (matron_traversal.hpp) is what does it, with her hands on that
// steel and her own weight. This map only chooses: nothing here moves a body.
//
// Planning is bounded: fixed storage, at most kMaxExpansions edges looked at
// a plan. An edge that failed under her is blocked for a while with the
// reason; the map does not forget it because she heard something new.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace scraperx::sim::routes {

// Stable authored identities: saved, compared and logged by value.
enum class Region : std::uint8_t {
    Deck132,
    Deck143,
    Deck154,
    Bracket152,   // the bay's bracket under the 154 deck's leaf (bay.hpp)
    Ring176,
    Ring198,
    Ring220,
    Count,
};

enum class Kind : std::uint8_t {
    LadderUp,
    LadderDown,
    PipeUp,
    PipeDown,
    RungsUp,      // the bay's wall rungs, out over the lip
    DropDown,     // off an edge onto support close under it
};

enum class Status : std::uint8_t {
    Unknown,      // authored steel, not yet shown to carry her this way
    Qualified,    // a test has taken her along it this way
    Blocked,      // shown not to: see Reason
};

enum class Reason : std::uint8_t {
    None,
    Untested,     // no executor for this way yet
    Clearance,    // her body does not fit through it
    Capability,   // it needs a hold she has no way to take (a smooth pole, a jump)
    Failed,       // it failed under her just now (a lost hold, a blocked mantle)
    Stage,        // her stage does not take it (the frail Matron outside the Turn)
};

enum class Connector : std::uint8_t {
    HomeLadder,   // 154 -> 176.25, x 5.0 (band_counterweight_well.cpp)
    RiserLadder,  // C3's high riser, 143 -> 154, x -6.0 (band_stack.cpp)
    CornerPipe,   // the north-east riser and its flanges (world_solids.inc)
    BayRungs,     // the bay's west wall (bay.cpp)
    BayLip,       // the bay cut's west lip down to the bracket
    C3WallLadder, // C3's wall ladder, the duct's north end up to 143 (band_stack.cpp)
    C3Duct,       // C3's duct, its 1.7 m face up from the catwalk landing
    Count,
};

// An axis-aligned patch of a region's footprint, in the horizontal plane;
// `moving` when it is a machine's floor (a cage at a stop, a catwalk plate),
// steel there only while the machine is.
struct Patch final {
    float min_x = 0.0F, max_x = 0.0F, min_z = 0.0F, max_z = 0.0F;
    bool moving = false;
    [[nodiscard]] bool holds(const float x, const float z) const noexcept {
        return x >= min_x && x <= max_x && z >= min_z && z <= max_z;
    }
};

struct RegionInfo final {
    Region id = Region::Count;
    std::string_view name;
    float top_y = 0.0F;    // the top of its steel
    int level = -1;        // her walking lines on it (Matron::levels_)
    std::array<Patch, 6> patches{};
    std::uint8_t patch_count = 0;
};

// A ladder as she meets it: rungs across x (half_width either side of x) in
// the plane z, from bottom_y to top_y every pitch; she climbs on its side
// (side_z, -1 south or +1 north of the rung plane) and tops out toward out_z.
struct LadderInfo final {
    float x = 0.0F, z = 0.0F;
    float half_width = 0.0F;
    float bottom_y = 0.0F, top_y = 0.0F, pitch = 0.0F;
    float side_z = 0.0F, out_z = 0.0F;
    float foot_y = 0.0F, head_y = 0.0F;   // the support at its foot and over its head
};

struct Edge final {
    Region from = Region::Count;
    Region to = Region::Count;
    Kind kind = Kind::LadderUp;
    Connector connector = Connector::Count;
    float entry_x = 0.0F, entry_z = 0.0F;   // where she stands to start it, on `from`
    float exit_x = 0.0F, exit_z = 0.0F;     // where it leaves her, on `to`
    float cost_s = 0.0F;                    // the time it takes, at her climbing pace
    Status status = Status::Unknown;
    Reason reason = Reason::Untested;
    float blocked_until_s = 0.0F;
    std::uint8_t failures = 0;
};

constexpr std::size_t kRegionCount = static_cast<std::size_t>(Region::Count);
constexpr std::size_t kMaxEdges = 64;
constexpr std::size_t kMaxPath = 16;
constexpr std::size_t kMaxExpansions = 128;
constexpr float kOnTopM = 0.35F;          // feet within this of a region's top: on it (with support under them)
constexpr float kFailedBlockS = 30.0F;    // a failed way, left alone this long
constexpr std::uint8_t kMaxRetries = 2;   // then it stays blocked until the world changes

struct Path final {
    std::array<std::uint8_t, kMaxPath> edges{};
    std::uint8_t count = 0;
    float cost_s = 0.0F;
};

class Map final {
public:
    Map();

    [[nodiscard]] const RegionInfo &region(Region id) const noexcept {
        return regions_[static_cast<std::size_t>(id)];
    }
    [[nodiscard]] const Edge &edge(const std::size_t index) const noexcept { return edges_[index]; }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }
    [[nodiscard]] const LadderInfo &ladder(Connector c) const noexcept {
        return ladders_[static_cast<std::size_t>(c)];
    }

    // The region whose footprint holds (x, z) and whose top is within kOnTopM
    // of feet_y; Region::Count when none does (between floors, on a machine,
    // off the map). Support under the feet is the caller's to establish.
    [[nodiscard]] Region region_at(float x, float feet_y, float z) const noexcept;

    // The cheapest way from `from` (standing at x, z) to `to`, over edges not
    // blocked at now_s, walking at walk_mps between them. False if none.
    bool plan(Region from, float x, float z, Region to, float now_s, float walk_mps, Path &out) const noexcept;

    // An attempt along edge `index` failed under her: blocked for a while,
    // and after kMaxRetries it stays blocked until unblock().
    void fail(std::size_t index, Reason reason, float now_s) noexcept;
    void succeed(std::size_t index) noexcept;
    void unblock_all() noexcept;
    [[nodiscard]] bool usable(std::size_t index, float now_s) const noexcept;

    // The edge from `from` to `to` by `kind`; kMaxEdges when there is none.
    [[nodiscard]] std::size_t find(Region from, Region to, Kind kind) const noexcept;
    // Qualifying a way (its executor and test exist) or ruling it out.
    void set_status(std::size_t index, Status status, Reason reason) noexcept;

private:
    std::array<RegionInfo, kRegionCount> regions_{};
    std::array<Edge, kMaxEdges> edges_{};
    std::size_t edge_count_ = 0;
    std::array<LadderInfo, static_cast<std::size_t>(Connector::Count)> ladders_{};
    void add(const Edge &edge) noexcept;
};

[[nodiscard]] std::string_view name(Region id) noexcept;
[[nodiscard]] std::string_view name(Reason reason) noexcept;

} // namespace scraperx::sim::routes
