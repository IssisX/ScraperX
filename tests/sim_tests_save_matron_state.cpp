// J-116, protected: the Matron's state section ("MAT1") of the save.
// Her state, stage and meters sit between her damage ("MAD1") and her
// per-region injury ("INJ1"), 26 bytes: the mark, her state, her strain,
// her rage, her stage, her exposure, her handled count. A relaunch puts her
// back as a death restore would; a save made before MAT1 loads with her
// defaults (Dormant, a pest, unhurt); a MAT1 no build wrote is refused and
// leaves the world alone. The feed's roundtrip file stays tolerant of MAT1
// and proves the legs without it; this file proves MAT1 itself.
#include "sim_tests.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;

using Bytes = std::vector<std::uint8_t>;

namespace {

// The MAT1 mark, "MAT1" little-endian (0x3154414D), and the MAD1 mark.
constexpr std::uint32_t kMatronDamageMark = 0x314D414DU;
constexpr std::uint32_t kMatronStateMark = 0x3154414DU;
// The damage block is 13 bytes after its mark (damage, player damage, dead);
// MAT1 is 26 bytes after its mark (state, strain, rage, stage, exposure,
// handled). The marks are found by scanning, not by a position in the file:
// the tail is a prefix chain, so no section knows its own offset.
constexpr std::size_t kDamageBytes = 13;
constexpr std::size_t kStateBytes = 26;

std::size_t find_damage_mark(const Bytes &bytes) {
    for (std::size_t i = 0; i + kDamageBytes <= bytes.size(); ++i) {
        std::uint32_t word = 0;
        std::memcpy(&word, bytes.data() + i, 4);
        if (word != kMatronDamageMark) {
            continue;
        }
        float damage = 0.0F, player = 0.0F;
        std::memcpy(&damage, bytes.data() + i + 4, 4);
        std::memcpy(&player, bytes.data() + i + 8, 4);
        if (std::isfinite(damage) && damage >= 0.0F && std::isfinite(player) && player >= 0.0F &&
            bytes[i + 12] <= 1) {
            return i;
        }
    }
    return bytes.size();
}

std::size_t find_state_mark(const Bytes &bytes) {
    for (std::size_t i = 0; i + kStateBytes <= bytes.size(); ++i) {
        std::uint32_t word = 0;
        std::memcpy(&word, bytes.data() + i, 4);
        if (word != kMatronStateMark) {
            continue;
        }
        float strain = 0.0F, rage = 0.0F, exposure = 0.0F;
        std::memcpy(&strain, bytes.data() + i + 5, 4);
        std::memcpy(&rage, bytes.data() + i + 9, 4);
        std::memcpy(&exposure, bytes.data() + i + 14, 4);
        if (bytes[i + 4] <= 9 && std::isfinite(strain) && strain >= 0.0F && std::isfinite(rage) &&
            rage >= 0.0F && rage <= 1.0F && bytes[i + 13] <= 4 && std::isfinite(exposure) &&
            exposure >= 0.0F) {
            return i;
        }
    }
    return bytes.size();
}

// A plain save, one frame in, so the tail is the yard save: 74309 bytes
// before MAT1, 74335 after it.
Bytes plain_save() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const Bytes saved = s.save_game();
    std::cout << "  save-matron-state plain: save_bytes=" << saved.size() << '\n';
    return saved;
}

// She is Dangerous, so a relaunch that drops her state would show it: she
// would come back a pest, unhurt, and the stage and rage checks would fail.
void run_matron_dangerous_relaunch() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const std::uint64_t commits_before = s.snapshot().checkpoint_commit_count;
    s.debug_set_matron_stage(1);
    bool committed = false;
    for (std::uint32_t tick = 0; tick < 3 * Simulation::kTickRateHz; ++tick) {
        s.debug_set_matron_stage(1);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        if (s.snapshot().checkpoint_commit_count > commits_before) {
            committed = true;
            break;
        }
    }
    require(committed, "dangerous: the checkpoint must commit at Dangerous");
    const auto before = s.matron_state();
    require(before.stage == 1 && before.rage > 0.4, "dangerous: she is Dangerous before the save");
    const Bytes saved = s.save_game();
    std::cout << "  save-matron-state dangerous: save_bytes=" << saved.size() << '\n';
    Simulation b(InitialSpawn::ExteriorGrade);
    require(b.load_game(saved.data(), saved.size()), "dangerous: the save must load");
    const auto after = b.matron_state();
    require(after.stage == 1, "dangerous: she is still Dangerous after relaunch");
    require(after.rage > 0.4, "dangerous: her rage survives the relaunch");
    require(after.state == before.state, "dangerous: her state survives the relaunch");
    std::cout << "  save-matron-state dangerous: stage=" << int(after.stage) << " rage=" << after.rage
              << '\n';
}

// A save made before MAT1 ends at her damage and loads with her defaults.
void run_matron_state_old_format() {
    const Bytes saved = plain_save();
    const std::size_t damage = find_damage_mark(saved);
    require(damage != saved.size(), "old format: the damage mark must be found");
    const Bytes cut(saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(damage + kDamageBytes));
    Simulation b(InitialSpawn::ExteriorGrade);
    require(b.load_game(cut.data(), cut.size()), "old format: a save ending at her damage must load");
    const auto m = b.matron_state();
    require(m.stage == 0 && m.rage == 0.0, "old format: she loads as a pest, unhurt");
    std::cout << "  save-matron-state old format: cut_bytes=" << cut.size() << " stage=" << int(m.stage)
              << " rage=" << m.rage << '\n';
}

// A MAT1 no build wrote is refused, and the world is left alone.
void run_matron_state_corrupt() {
    const Bytes saved = plain_save();
    const std::size_t state = find_state_mark(saved);
    require(state != saved.size(), "corrupt: the state mark must be found");
    Simulation w(InitialSpawn::ExteriorGrade);
    (void)w.advance_frame(1.0);
    const Bytes before = w.save_game();

    Bytes bad = saved;
    bad[state + 4] = 19;  // her state, past Unrigging
    require(!w.load_game(bad.data(), bad.size()), "matron state 19: the load must refuse");
    require(w.save_game() == before, "matron state 19: the world is unchanged");
    std::cout << "  save-matron-state corrupt: state 19 refused\n";

    bad = saved;
    const float rage = 2.0F;  // past the 0..1 clamp
    std::memcpy(bad.data() + state + 9, &rage, 4);
    require(!w.load_game(bad.data(), bad.size()), "matron rage 2: the load must refuse");
    require(w.save_game() == before, "matron rage 2: the world is unchanged");
    std::cout << "  save-matron-state corrupt: rage 2 refused\n";
}

void run_save_matron_state() {
    run_matron_dangerous_relaunch();
    run_matron_state_old_format();
    run_matron_state_corrupt();
    std::cout << "save-matron-state: dangerous=1 old_format=1 corrupt=2 PASS\n";
}

}  // namespace

SCRAPERX_SLICE("save-matron-state", run_save_matron_state);
