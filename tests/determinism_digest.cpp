// Device determinism digest: the same build, on the host and on the phone's
// arm64, must compute the same bits. It prints her scan graph of the whole
// tower and then one state digest per second of a fixed 10 s touch script
// from ExteriorGrade. CI runs it on the host and, built by the NDK, under
// qemu-aarch64, and requires the two outputs to be identical.
#include "sim/matron_traversal.hpp"
#include "sim/simulation.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

class Digest final {
public:
    void add(const double value) noexcept {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        mix(bits);
    }
    void add(const float value) noexcept {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        mix(bits);
    }
    void add(const Vector3 &v) noexcept {
        add(v.x);
        add(v.y);
        add(v.z);
    }
    void add(const JPH::Vec3 &v) noexcept {
        add(v.GetX());
        add(v.GetY());
        add(v.GetZ());
    }
    void mix(const std::uint64_t bits) noexcept {
        for (int i = 0; i < 8; ++i) {
            hash_ = (hash_ ^ ((bits >> (8 * i)) & 0xFFU)) * 1099511628211ULL;
        }
    }
    [[nodiscard]] std::uint64_t value() const noexcept { return hash_; }

private:
    std::uint64_t hash_ = 1469598103934665603ULL;
};

void tick(Simulation &s) {
    if (!s.advance_frame(Simulation::kFixedStepSeconds).accepted) {
        std::printf("FAIL tick refused\n");
        std::exit(EXIT_FAILURE);
    }
}

void scan_graph() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    tick(s);
    const auto &map = s.matron_affordances();
    Digest d;
    std::uint32_t usable = 0;
    for (const auto &e : map.edges()) {
        d.mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(e.from)));
        d.mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(e.to)));
        d.mix(static_cast<std::uint64_t>(e.kind));
        d.mix(e.usable ? 1U : 0U);
        d.add(e.cost);
        d.add(e.spec.start);
        d.add(e.spec.goal);
        usable += e.usable ? 1U : 0U;
    }
    for (const auto &r : map.regions()) {
        d.add(r.top);
        d.add(r.area);
    }
    std::printf("graph regions=%zu patches=%zu edges=%zu usable=%u digest=%016llx\n", map.regions().size(),
                map.patches().size(), map.edges().size(), usable, static_cast<unsigned long long>(d.value()));
}

void touch_replay() {
    Simulation s(InitialSpawn::ExteriorGrade);
    const int per_second = static_cast<int>(Simulation::kTickRateHz);
    for (int t = 0; t < 10 * per_second; ++t) {
        // A fixed touch script: walk, turn, jump, crouch, as a thumb would.
        const double phase = static_cast<double>(t) / per_second;
        const int leg = static_cast<int>(phase / 2.5);
        const double dir[4][2] = {{0.0, -1.0}, {1.0, -0.4}, {0.0, 1.0}, {-1.0, 0.3}};
        (void)s.set_move_input(dir[leg % 4][0], dir[leg % 4][1]);
        (void)s.set_facing(dir[leg % 4][0], dir[leg % 4][1]);
        if (t % per_second == per_second / 2) {
            (void)s.request_jump();
        }
        tick(s);
        if ((t + 1) % per_second == 0) {
            Digest d;
            const auto snap = s.snapshot();
            d.add(snap.player_position);
            const auto her = s.matron_state();
            d.add(her.position);
            for (std::uint32_t i = 0; i < s.kit_body_count(); ++i) {
                d.add(s.kit_body_position(i));
            }
            std::printf("replay t=%ds player=(%.9g,%.9g,%.9g) digest=%016llx\n", (t + 1) / per_second,
                        snap.player_position.x, snap.player_position.y, snap.player_position.z,
                        static_cast<unsigned long long>(d.value()));
        }
    }
}

} // namespace

int main() {
    scan_graph();
    touch_replay();
    std::fflush(stdout);
    return EXIT_SUCCESS;
}
