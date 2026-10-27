#pragma once

// Her decks' steel as the tower's builders lay it, for the tests to say where
// she stands: an answer key, never read by the game. These are the footprints
// her authored map held (src/sim/matron_routes.cpp, which Board v5 step 2
// takes out of the game), number for number: the stack's decks a ring of
// bands 9 m deep round the 34 m shaft (physics_world_tuning.hpp kStack*), the
// well's rings bands 4 m wide narrowing 0.91 m a ring (world_solids.inc
// "UpperDeck"), with the walkway plate at 143 under C3's riser, the bay's cut
// and the catwalk at 154, the bay's bracket at 152.22, and the well's cages
// at their stops. Two additions the map never held, from
// band_counterweight_well.cpp: the timber boards out from the 176 ring to the
// standpipe's foot, where her pole climb down from the 198 ring stands her,
// and the catwalk flush with the 198 ring, where the panel climb down from
// the 220 ring stands her.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace scraperx_test_decks {

enum class Deck : std::uint8_t { Deck132, Deck143, Deck154, Bracket152, Ring176, Ring198, Ring220, None };

struct Patch {
    float min_x = 0.0F, max_x = 0.0F, min_z = 0.0F, max_z = 0.0F;
    float rise = 0.0F;     // its top over the deck's (within kOnTopM)
    bool moving = false;   // a machine's floor: steel there only while it is
    [[nodiscard]] bool holds(const float x, const float z) const noexcept {
        return x >= min_x && x <= max_x && z >= min_z && z <= max_z;
    }
};

struct DeckInfo {
    Deck id = Deck::None;
    const char *name = "";
    float top_y = 0.0F;
    std::array<Patch, 8> patches{};
    std::uint8_t count = 0;
};

inline constexpr float kShaftZ = -150.0F;
inline constexpr float kStackHalf = 26.0F;
inline constexpr float kStackBand = 9.0F;
inline constexpr float kRingBand = 4.0F;
inline constexpr float kRing176Mid = 23.09F;
inline constexpr float kRingStep = 0.91F;
inline constexpr float kOnTopM = 0.35F;   // feet within this of a deck's top: on it
// The ladder from her deck up to the 176 ring, as the world builds it: the
// line its rungs stand on, and where a climber stands at its foot.
inline constexpr float kHerLadderX = 5.0F;
inline constexpr float kHerLadderFootZ = -129.55F;

// The four bands of a square ring whose outer edge is `outer`, `band` deep,
// round the shaft at (0, kShaftZ).
inline std::uint8_t ring_patches(std::array<Patch, 8> &patches, const float outer, const float band) {
    const float inner = outer - band;
    patches[0] = {-outer, outer, kShaftZ + inner, kShaftZ + outer};   // north
    patches[1] = {-outer, outer, kShaftZ - outer, kShaftZ - inner};   // south
    patches[2] = {inner, outer, kShaftZ - inner, kShaftZ + inner};    // east
    patches[3] = {-outer, -inner, kShaftZ - inner, kShaftZ + inner};  // west
    return 4;
}

inline DeckInfo stack_deck(const Deck id, const char *name, const float top) {
    DeckInfo d;
    d.id = id;
    d.name = name;
    d.top_y = top;
    d.count = ring_patches(d.patches, kStackHalf, kStackBand);
    return d;
}

inline DeckInfo ring(const Deck id, const char *name, const int k) {
    DeckInfo d;
    d.id = id;
    d.name = name;
    d.top_y = 176.25F + 22.0F * float(k);
    const float mid = kRing176Mid - kRingStep * float(k);
    d.count = ring_patches(d.patches, mid + 0.5F * kRingBand, kRingBand);
    return d;
}

inline std::array<DeckInfo, 7> make_decks() {
    std::array<DeckInfo, 7> d{};
    d[0] = stack_deck(Deck::Deck132, "deck 132", 132.0F);
    d[1] = stack_deck(Deck::Deck143, "deck 143", 143.0F);
    d[1].patches[d[1].count++] = {-11.5F, -5.0F, -135.35F, -133.0F};   // the walkway plate under C3's riser
    d[2] = stack_deck(Deck::Deck154, "deck 154", 154.0F);
    {
        // The bay cuts the north band at 154; the catwalk crosses the shaft at z -150.
        const Patch north = d[2].patches[0];
        d[2].patches[0] = {north.min_x, -3.9F, north.min_z, north.max_z};
        d[2].patches[d[2].count++] = {-0.4F, north.max_x, north.min_z, north.max_z};
        d[2].patches[d[2].count++] = {-16.94F, 16.94F, -150.8F, -149.2F};
    }
    d[3].id = Deck::Bracket152;
    d[3].name = "bay bracket 152.22";
    d[3].top_y = 152.22F;
    d[3].patches[0] = {-3.9F, -2.9F, -131.25F, -129.35F};
    d[3].count = 1;
    d[4] = ring(Deck::Ring176, "ring 176.25", 0);
    d[5] = ring(Deck::Ring198, "ring 198.25", 1);
    {
        // The Counterweight Well's cages at their stops: stage A's up at the
        // 176 ring, stage B's at the 176 ring below and the 198 ring above.
        constexpr Patch kCageA{-12.0F, -9.0F, -132.8F, -130.0F, 0.0F, true};
        constexpr Patch kCageB{-8.7F, -5.7F, -132.8F, -130.0F, 0.0F, true};
        d[4].patches[d[4].count++] = kCageA;
        d[4].patches[d[4].count++] = kCageB;
        d[5].patches[d[5].count++] = kCageB;
        // The boards at 176.45 (0.2 m over the ring, within kOnTopM): out
        // south from the ring, then east past the standpipe.
        d[4].patches[d[4].count++] = {6.90F, 7.20F, -131.05F, -128.60F, 0.20F};
        d[4].patches[d[4].count++] = {6.90F, 8.50F, -131.05F, -130.75F, 0.20F};
        // The catwalk flush with the 198 ring, under the scaffold panel.
        d[5].patches[d[5].count++] = {10.50F, 11.50F, -132.00F, -129.72F};
    }
    d[6] = ring(Deck::Ring220, "ring 220.25", 2);
    return d;
}

inline const std::array<DeckInfo, 7> &decks() {
    static const std::array<DeckInfo, 7> all = make_decks();
    return all;
}

// The deck she stands on: of those whose steel is under (x, z), the one whose
// top is nearest her feet, within kOnTopM; Deck::None for none.
inline Deck deck_at(const float x, const float feet, const float z) {
    Deck best = Deck::None;
    float best_dy = kOnTopM;
    for (const DeckInfo &d : decks()) {
        const float dy = std::abs(feet - d.top_y);
        if (dy > best_dy) {
            continue;
        }
        for (std::uint8_t i = 0; i < d.count; ++i) {
            if (d.patches[i].holds(x, z)) {
                best = d.id;
                best_dy = dy;
                break;
            }
        }
    }
    return best;
}

inline const DeckInfo &deck(const Deck id) { return decks()[static_cast<std::size_t>(id)]; }

} // namespace scraperx_test_decks
