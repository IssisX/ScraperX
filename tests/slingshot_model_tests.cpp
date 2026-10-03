#include "sim/slingshot_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace sling = scraperx::sim::slingshot;
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kGravity = 9.81;
constexpr double kLoadedMass = 100.0; // CHOSEN 85 kg rider + 15 kg leather/carriage.
constexpr double kStep = 1.0 / 90.0;
constexpr double kForkRadius = 10.0;
constexpr double kHalfSpan = 3.0;
constexpr double kMaximumDraw = 12.0;
constexpr double kDefaultElevation = 82.0;

void require(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void close(double actual, double expected, double tolerance, const std::string &message) {
    require(std::abs(actual - expected) <= tolerance,
            message + " actual=" + std::to_string(actual) +
            " expected=" + std::to_string(expected));
}

sling::DrawPath ground_path(double elevation_degrees = kDefaultElevation, double yaw_degrees = 0.0) {
    const double yaw = yaw_degrees * kPi / 180.0;
    const auto axis = sling::aim_axis(yaw, elevation_degrees * kPi / 180.0);
    sling::DrawPath path;
    path.undrawn_pouch_position = {6.0, 0.35, -55.0};
    path.pull_direction = {0.0, 0.0, 1.0};
    path.anchors = sling::symmetric_anchors(path.undrawn_pouch_position + axis * kForkRadius,
                                           kHalfSpan, yaw);
    return path;
}

void band_force_and_energy() {
    const sling::BandParameters bands;
    const auto path = ground_path();
    const auto idle = sling::evaluate_bands(path.undrawn_pouch_position, {}, path.anchors, bands);
    require(idle.valid, "neutral evaluation valid");
    close(idle.elastic_energy_j, 0.0, 1e-20, "no draw stores no energy");
    close(sling::length(idle.force_n), 0.0, 1e-9, "no draw exerts no spring force");

    double previous_energy = 0.0;
    for (double draw = 0.25; draw <= kMaximumDraw; draw += 0.25) {
        const auto evaluation = sling::evaluate_bands(sling::pouch_at_draw(path, draw), {}, path.anchors, bands);
        require(evaluation.elastic_energy_j > previous_energy, "greater draw stores more elastic energy");
        previous_energy = evaluation.elastic_energy_j;
        for (const auto &band : evaluation.bands) {
            require(band.tension_n >= 0.0, "bands never push");
            close(sling::length(band.force_on_pouch_n + band.reaction_on_anchor_n), 0.0, 1e-9,
                  "frame sees equal opposite anchor reaction");
        }
    }

    // Independent central difference of the scalar potential, at an
    // asymmetric real position, checks all three components of -grad(U).
    const auto point = path.undrawn_pouch_position + sling::Vec3{0.63, 1.47, 6.79};
    const auto evaluation = sling::evaluate_bands(point, {}, path.anchors, bands);
    constexpr double epsilon = 1e-5;
    for (int component = 0; component < 3; ++component) {
        sling::Vec3 offset{};
        if (component == 0) offset.x = epsilon;
        if (component == 1) offset.y = epsilon;
        if (component == 2) offset.z = epsilon;
        const double plus = sling::evaluate_bands(point + offset, {}, path.anchors, bands).elastic_energy_j;
        const double minus = sling::evaluate_bands(point - offset, {}, path.anchors, bands).elastic_energy_j;
        const double analytic = component == 0 ? evaluation.elastic_force_n.x :
                                component == 1 ? evaluation.elastic_force_n.y : evaluation.elastic_force_n.z;
        close(analytic, -(plus - minus) / (2.0 * epsilon), 2e-4, "elastic force is energy gradient");
    }

    const auto near_midpoint = path.undrawn_pouch_position +
                              sling::aim_axis(0.0, kDefaultElevation * kPi / 180.0) * kForkRadius;
    const auto slack = sling::evaluate_bands(near_midpoint, {100.0, -100.0, 100.0}, path.anchors, bands);
    close(slack.elastic_energy_j, 0.0, 0.0, "slack bands store zero energy");
    close(sling::length(slack.force_n), 0.0, 0.0, "slack band dashpot does not create tension");

    for (const sling::Vec3 velocity : {sling::Vec3{3.0, 12.0, -9.0}, sling::Vec3{-3.0, -12.0, 9.0},
                                      sling::Vec3{0.0, 10000.0, -10000.0}}) {
        const auto moving = sling::evaluate_bands(point, velocity, path.anchors, bands);
        const double damping_power = sling::dot(moving.force_n - moving.elastic_force_n, velocity);
        require(damping_power <= 1e-8, "dashpot is passive even when tension clamps to zero");
        close(damping_power, -moving.dissipated_power_w, 1e-6, "loss telemetry closes damping work");
    }
    auto moving_anchors = path.anchors;
    for (auto &v : moving_anchors.velocity) v = {1.0, 2.0, 3.0};
    const auto comoving = sling::evaluate_bands(point, {1.0, 2.0, 3.0}, moving_anchors, bands);
    close(comoving.dissipated_power_w, 0.0, 0.0, "rigid translation does not create damping loss");
}

struct ChargeResult {
    sling::WinchState state{};
    double time_s = 0.0;
};

ChargeResult charge(double target_draw, double step, double effort = 1.0,
                    sling::DrawPath path = ground_path(), sling::WinchParameters parameters = {}) {
    const sling::BandParameters bands;
    parameters.maximum_draw_m = target_draw;
    sling::DrawWinch winch(parameters);
    for (int i = 0; i < static_cast<int>(600.0 / step); ++i) {
        const auto result = winch.crank(path, bands, effort, step);
        require(result.valid, "valid winch charge");
        require(result.source_work_j <= parameters.maximum_source_power_w * effort * step + 1e-7,
                "source power budget per tick");
        require(result.average_source_force_n <= parameters.maximum_source_force_n * effort + 1e-6,
                "source force budget per tick");
        require(result.source_travel_m <= parameters.maximum_source_speed_mps * effort * step + 1e-9,
                "source travel budget per tick");
        if (result.at_draw_stop) return {winch.state(), (i + 1) * step};
        require(!result.force_stalled, "full chosen draw lies within winch force capacity");
    }
    require(false, "winch reached target draw within evaluation duration");
    return {};
}

void bounded_winch_work() {
    const auto path = ground_path();
    const sling::BandParameters bands;
    sling::DrawWinch idle;
    for (int i = 0; i < 90 * 30; ++i) (void)idle.crank(path, bands, 0.0, kStep);
    close(idle.state().held_draw_m, 0.0, 0.0, "idle time creates no draw");
    close(idle.state().source_work_j, 0.0, 0.0, "idle time creates no work");

    const auto full = charge(kMaximumDraw, kStep);
    const auto quarter_step = charge(kMaximumDraw, kStep * 0.25);
    close(full.state.source_work_j * sling::WinchParameters{}.efficiency,
          sling::spring_energy_at_draw(path, kMaximumDraw, bands) + full.state.band_loss_j, 1e-7,
          "source work funds exact spring U and passive draw loss");
    close(full.state.source_work_j, full.state.spring_work_j + full.state.opposing_load_work_j +
          full.state.band_loss_j + full.state.transmission_loss_j, 1e-7, "complete charge ledger closes");
    close(full.state.spring_work_j, quarter_step.state.spring_work_j, 1e-7,
          "exact elastic charge work agrees at h/4");
    close(full.state.band_loss_j, quarter_step.state.band_loss_j, 0.02 * full.state.band_loss_j,
          "passive draw loss converges at h/4 within its energy budget");
    close(full.time_s, quarter_step.time_s, kStep, "charge time converges at h/4");
    require(full.time_s >= full.state.source_work_j / sling::WinchParameters{}.maximum_source_power_w,
            "finite source work imposes charge duration");

    sling::DrawWinch ratchet;
    const auto early = ratchet.crank(path, bands, 1.0, kStep);
    for (int i = 0; i < 400; ++i) (void)ratchet.crank(path, bands, 1.0, kStep);
    const auto held = ratchet.state();
    (void)ratchet.crank(path, bands, -1.0, 2.0);
    (void)ratchet.crank(path, bands, 0.0, 30.0);
    close(ratchet.state().held_draw_m, held.held_draw_m, 0.0, "one-way ratchet retains draw with no input");
    close(ratchet.state().source_work_j, held.source_work_j, 0.0, "latch cannot add energy");
    const double previous_draw = ratchet.state().held_draw_m;
    const auto late = ratchet.crank(path, bands, 1.0, kStep);
    require(late.held_draw_m - previous_draw < early.held_draw_m * 0.7,
            "draw speed falls under high elastic load");
    ratchet.release();
    const auto released = ratchet.state();
    (void)ratchet.crank(path, bands, 1.0, 1.0);
    close(ratchet.state().held_draw_m, released.held_draw_m, 0.0, "open ratchet cannot author draw");
    close(ratchet.state().source_work_j, released.source_work_j, 0.0, "release does not add launch energy");

    auto loaded_path = path;
    loaded_path.opposing_load_n = 2500.0;
    const auto loaded = charge(6.0, kStep, 1.0, loaded_path);
    close(loaded.state.opposing_load_work_j, 2500.0 * 6.0, 1e-8, "extra opposition work is funded");
    auto insufficient = sling::WinchParameters{};
    insufficient.maximum_source_force_n = 100.0;
    sling::DrawWinch stalled(insufficient);
    require(stalled.restore({5.0, 0.0, 0.0, 0.0, 0.0, true}), "restore known physical draw");
    const auto stall = stalled.crank(path, bands, 1.0, kStep);
    require(stall.force_stalled, "insufficient force stalls even when power exists");
    close(stall.source_work_j, 0.0, 0.0, "force stall cannot inject source work");
    auto invalid = path;
    invalid.pull_direction = {};
    require(!idle.crank(invalid, bands, 1.0, kStep).valid, "zero draw axis is rejected");
    require(!idle.crank(path, bands, std::numeric_limits<double>::quiet_NaN(), kStep).valid,
            "nonfinite input is rejected");
    std::cout << "CHARGE draw=" << kMaximumDraw << "m work=" << full.state.source_work_j
              << "J spring=" << full.state.spring_work_j << "J time=" << full.time_s
              << "s draw_loss=" << full.state.band_loss_j
              << "J h/4_loss_delta=" << std::abs(full.state.band_loss_j - quarter_step.state.band_loss_j)
              << "J h/4_time_delta=" << std::abs(full.time_s - quarter_step.time_s) << "s\n";
}

struct ShotResult {
    sling::Vec3 position{};
    sling::Vec3 velocity{};
    double release_time_s = 0.0;
    double apex_y_m = 0.0;
    double loss_j = 0.0;
    double ledger_residual_j = 0.0;
    double retained_elastic_energy_j = 0.0;
    double constraint_work_j = 0.0;
    double front_y_m = 0.0;
    double ground_range_m = 0.0;
    bool released = false;
    bool crossed_front = false;
};

struct TrackPhase {
    double travel_m = 0.0;
    double speed_mps = 0.0;
    double band_loss_j = 0.0;
    double constraint_work_j = 0.0;
};

TrackPhase increment(TrackPhase state, TrackPhase rate, double dt) {
    return {state.travel_m + rate.travel_m * dt,
            state.speed_mps + rate.speed_mps * dt,
            state.band_loss_j + rate.band_loss_j * dt,
            state.constraint_work_j + rate.constraint_work_j * dt};
}

// Independent evaluator of the real force along the passive aimed track.
// RK4 uses the native world's four collision substeps per external tick.
// The ideal normal reaction performs zero work. Track exit is the closest
// geometric point to the fork; residual elastic energy stays on the pouch.
// The rider leaves there with the velocity produced by force integration.
// This is model evidence, not a replacement for the authoritative Jolt body.
ShotResult release(double draw, double elevation, double yaw, double step) {
    const auto path = ground_path(elevation, yaw);
    const sling::BandParameters bands;
    const auto start = sling::pouch_at_draw(path, draw);
    const auto axis = sling::aim_axis(yaw * kPi / 180.0, elevation * kPi / 180.0);
    const auto midpoint = (path.anchors.position[0] + path.anchors.position[1]) * 0.5;
    const double track_end = sling::dot(midpoint - start, axis);
    const double initial_energy = sling::spring_energy_at_draw(path, draw, bands) +
                                  kLoadedMass * kGravity * start.y;
    TrackPhase phase;
    double elapsed = 0.0;
    bool released = draw == 0.0;
    const double substep = step * 0.25;
    const auto derivative = [&](const TrackPhase &state) {
        const auto position = start + axis * state.travel_m;
        const auto velocity = axis * state.speed_mps;
        const auto evaluation = sling::evaluate_bands(position, velocity, path.anchors, bands);
        require(evaluation.valid, "shot force evaluation valid");
        require(evaluation.bands[0].tension_n >= 0.0 && evaluation.bands[1].tension_n >= 0.0,
                "shot band reactions remain tensile");
        const auto total_force = evaluation.force_n + sling::Vec3{0.0, -kLoadedMass * kGravity, 0.0};
        const double along_force = sling::dot(total_force, axis);
        const auto normal_reaction = axis * along_force - total_force;
        const double normal_power = sling::dot(normal_reaction, velocity);
        close(normal_power, 0.0, 1e-7, "ideal launch guide reaction performs zero work");
        return TrackPhase{state.speed_mps, along_force / kLoadedMass,
                          evaluation.dissipated_power_w, normal_power};
    };
    for (int i = 0; !released && i < static_cast<int>(5.0 / substep); ++i) {
        const auto previous = phase;
        const auto k1 = derivative(phase);
        const auto k2 = derivative(increment(phase, k1, substep * 0.5));
        const auto k3 = derivative(increment(phase, k2, substep * 0.5));
        const auto k4 = derivative(increment(phase, k3, substep));
        phase = increment(increment(increment(increment(phase, k1, substep / 6.0),
                                     k2, substep / 3.0), k3, substep / 3.0), k4, substep / 6.0);
        elapsed += substep;
        if (phase.travel_m >= track_end) {
            const double fraction = (track_end - previous.travel_m) / (phase.travel_m - previous.travel_m);
            phase = increment(previous, {phase.travel_m - previous.travel_m,
                              phase.speed_mps - previous.speed_mps,
                              phase.band_loss_j - previous.band_loss_j,
                              phase.constraint_work_j - previous.constraint_work_j}, fraction);
            phase.travel_m = track_end;
            elapsed -= (1.0 - fraction) * substep;
            released = true;
        }
    }
    require(released, "drawn elastic pouch reaches geometric track end");
    auto position = start + axis * phase.travel_m;
    auto velocity = axis * phase.speed_mps;
    const double remaining_elastic_energy = sling::evaluate_bands(position, velocity, path.anchors, bands).elastic_energy_j;
    const double released_energy = 0.5 * kLoadedMass * sling::dot(velocity, velocity) +
                                   kLoadedMass * kGravity * position.y + remaining_elastic_energy;
    const auto release_position = position;
    const auto release_velocity = velocity;
    double apex_y = position.y;
    double front_y = 0.0;
    bool crossed_front = false;
    // Integrate post-release flight; no analytic velocity assignment.
    for (int i = 0; position.y > path.undrawn_pouch_position.y && i < static_cast<int>(60.0 / step); ++i) {
        const auto previous = position;
        position = position + (velocity + sling::Vec3{0.0, -kGravity * step * 0.5, 0.0}) * step;
        velocity.y -= kGravity * step;
        apex_y = std::max(apex_y, position.y);
        if (!crossed_front && position.z <= -124.0) {
            const double fraction = (-124.0 - previous.z) / (position.z - previous.z);
            front_y = previous.y + fraction * (position.y - previous.y);
            crossed_front = true;
        }
    }
    return {release_position, release_velocity, elapsed, apex_y, phase.band_loss_j,
            released_energy + phase.band_loss_j - initial_energy, remaining_elastic_energy,
            phase.constraint_work_j, front_y,
            sling::length(sling::Vec3{position.x - start.x, 0.0, position.z - start.z}),
            released, crossed_front};
}

void release_determinism_and_convergence() {
    const auto no_draw = release(0.0, kDefaultElevation, 0.0, kStep);
    close(sling::length(no_draw.velocity), 0.0, 0.0, "no draw produces no launch momentum");
    // An undercharged shot cannot reach the elevated physical rail end:
    // even a perfectly lossless guide cannot create the missing work.
    const auto path = ground_path();
    const auto axis = sling::aim_axis(0.0, kDefaultElevation * kPi / 180.0);
    const auto weak_start = sling::pouch_at_draw(path, 3.0);
    const auto weak_end = weak_start + axis * (kForkRadius + 3.0 * std::cos(kDefaultElevation * kPi / 180.0));
    require(sling::spring_energy_at_draw(path, 3.0) <
            kLoadedMass * kGravity * (weak_end.y - weak_start.y),
            "weak draw has insufficient work to clear the launch track");
    double previous_apex = 0.0;
    for (const double draw : {6.0, 9.0, kMaximumDraw}) {
        const auto shot = release(draw, kDefaultElevation, 0.0, kStep);
        const auto replay = release(draw, kDefaultElevation, 0.0, kStep);
        const auto fine = release(draw, kDefaultElevation, 0.0, kStep * 0.25);
        close(sling::length(shot.velocity - replay.velocity), 0.0, 0.0, "identical input replay is exact");
        close(shot.apex_y_m, replay.apex_y_m, 0.0, "identical replay apex is exact");
        close(sling::length(shot.velocity), sling::length(fine.velocity), 0.08, "release speed converges at h/4");
        close(shot.apex_y_m, fine.apex_y_m, 0.30, "apex converges at h/4");
        const double available_energy = sling::spring_energy_at_draw(ground_path(), draw);
        require(std::abs(shot.ledger_residual_j) < 0.02 * available_energy,
                "release energy residual below 2 percent");
        require(std::abs(fine.ledger_residual_j) < std::abs(shot.ledger_residual_j),
                "energy residual decreases with smaller timestep");
        require(shot.apex_y_m > previous_apex, "greater draw produces a higher physical apex");
        previous_apex = shot.apex_y_m;
        std::cout << "SHOT draw=" << draw << "m speed=" << sling::length(shot.velocity)
                  << "m/s apex=" << shot.apex_y_m << "m release=" << shot.release_time_s
                  << "s loss=" << shot.loss_j << "J residual=" << shot.ledger_residual_j
                  << "J remaining_U=" << shot.retained_elastic_energy_j
                  << "J front_y=" << shot.front_y_m
                  << "m h/4_apex_delta=" << std::abs(shot.apex_y_m - fine.apex_y_m) << "m\n";
    }

    const auto left = release(8.0, kDefaultElevation, -20.0, kStep);
    const auto right = release(8.0, kDefaultElevation, 20.0, kStep);
    require(left.velocity.x < -1.0 && right.velocity.x > 1.0, "yaw changes actual force-driven lateral launch");
    close(left.apex_y_m, right.apex_y_m, 1e-10, "symmetric yaw trajectories have symmetric energy");
    for (const double yaw : {-20.0, 20.0}) {
        const auto shot = release(8.0, kDefaultElevation, yaw, kStep);
        const auto fine = release(8.0, kDefaultElevation, yaw, kStep * 0.25);
        close(sling::length(shot.velocity), sling::length(fine.velocity), 0.08,
              "yaw band release speed converges at h/4");
        close(shot.apex_y_m, fine.apex_y_m, 0.30, "yaw band apex converges at h/4");
    }
    for (const double elevation : {78.0, 80.0, kDefaultElevation, 84.0}) {
        const auto charging = charge(kMaximumDraw, kStep, 1.0, ground_path(elevation));
        const auto shot = release(kMaximumDraw, elevation, 0.0, kStep);
        const auto fine = release(kMaximumDraw, elevation, 0.0, kStep * 0.25);
        close(shot.apex_y_m, fine.apex_y_m, 0.40, "aim band apex converges at h/4");
        require(shot.crossed_front && fine.crossed_front, "full aimed shot reaches tower-front plane");
        close(shot.front_y_m, fine.front_y_m, 0.10, "tower-front intersection converges at h/4");
        require(shot.retained_elastic_energy_j > 0.0, "residual band U is retained on pouch at track exit");
        close(shot.constraint_work_j, 0.0, 1e-8, "passive launch track supplies no work");
        require(shot.velocity.y > 0.0 && shot.velocity.z < 0.0, "allowed aim launches upward toward tower");
        const double energy = sling::spring_energy_at_draw(ground_path(elevation), kMaximumDraw);
        require(std::abs(shot.ledger_residual_j) < 0.02 * energy, "aim band energy ledger closes");
        std::cout << "AIM elevation=" << elevation << "deg work=" << charging.state.source_work_j
                  << "J charge=" << charging.time_s << "s vx=" << shot.velocity.x
                  << "m/s vy=" << shot.velocity.y << "m/s vz=" << shot.velocity.z
                  << "m/s apex=" << shot.apex_y_m << "m front_y=" << shot.front_y_m
                  << "m h/4_front_delta=" << std::abs(shot.front_y_m - fine.front_y_m) << "m\n";
    }
    const auto full = release(kMaximumDraw, kDefaultElevation, 0.0, kStep);
    require(full.apex_y_m > 350.0 && full.apex_y_m < 410.0, "chosen full draw has several-hundred-metre model apex");
    require(full.front_y_m > 352.0 && full.front_y_m < 375.0, "chosen aim clears nominal 352 m roof front in model");
}
} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(6);
    band_force_and_energy();
    bounded_winch_work();
    release_determinism_and_convergence();
    std::cout << "PASS slingshot constitutive force, bounded work, ratchet, aim, replay, and h/4 convergence\n";
    return 0;
}
