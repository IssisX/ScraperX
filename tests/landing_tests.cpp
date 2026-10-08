#include "sim/simulation.hpp"
#include "sim/slingshot.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *message) {
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(EXIT_FAILURE); }
}
void tick(Simulation &simulation, int count = 1) {
    for (int i = 0; i < count; ++i)
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted, "fixed physics step rejected");
}
Snapshot drop(Simulation &simulation, Vector3 at) {
    require(simulation.debug_restart_at(at), "clear fall fixture rejected");
    const auto before = simulation.snapshot().landing_count;
    for (int i = 0; i < 90 * 5; ++i) {
        tick(simulation);
        if (simulation.snapshot().landing_count > before) return simulation.snapshot();
    }
    require(false, "real fall never recorded landing");
    return {};
}
}
int main() {
    Simulation light_sim, medium_sim, heavy_sim, lethal_sim;
    const auto light = drop(light_sim, {40, 1.3, -50});
    const auto medium = drop(medium_sim, {40, 4.9, -50});
    const auto heavy = drop(heavy_sim, {40, 10.9, -50});
    const auto lethal = drop(lethal_sim, {40, 25.9, -50});
    for (const auto &receipt : {light, medium, heavy, lethal}) {
        std::cout << "landing normal=" << receipt.landing_normal_speed_mps << " energy="
                  << receipt.landing_approach_energy_j << " observed_impulse="
                  << receipt.landing_observed_normal_impulse_ns << " balance=" << receipt.landing_balance
                  << " recovery=" << receipt.landing_recovery_seconds << " deaths=" << receipt.death_count << '\n';
        require(std::abs(receipt.landing_approach_energy_j - .5 * 85 *
                    receipt.landing_normal_speed_mps * receipt.landing_normal_speed_mps) < .01,
                "approach energy differs from actual relative contact speed");
        require(receipt.landing_observed_normal_impulse_ns > 0,
                "Jolt landing failed to show actual upward momentum change");
    }
    require(light.landing_approach_energy_j < medium.landing_approach_energy_j &&
            medium.landing_approach_energy_j < heavy.landing_approach_energy_j,
            "fall height does not determine landing energy");
    require(light.landing_recovery_seconds < heavy.landing_recovery_seconds &&
            light.landing_balance > heavy.landing_balance,
            "heavy contact does not affect native balance/recovery");
    require(heavy.death_count == 0 && heavy.landing_recovering && lethal.death_count == 1,
            "light/heavy/lethal physical outcomes were not distinguished");
    const auto jump_vx = medium_sim.snapshot().player_linear_velocity.x;
    require(medium_sim.set_move_input(1, 0), "combined move/jump input rejected");
    require(medium_sim.request_jump(), "follow-up jump rejected");
    tick(medium_sim);
    require(!medium_sim.snapshot().player_grounded && medium_sim.snapshot().player_linear_velocity.y > 5 &&
            std::abs(medium_sim.snapshot().player_linear_velocity.x - jump_vx) < .15 &&
            medium_sim.snapshot().landing_jump_work_j > 1000,
            "landing recovery consumed jump or replaced arrival momentum");
    require(heavy_sim.set_move_input(1, 0), "walking intent rejected");
    const double initial_vx = heavy_sim.snapshot().player_linear_velocity.x;
    tick(heavy_sim);
    require(std::abs(heavy_sim.snapshot().player_linear_velocity.x - initial_vx) < .15,
            "landing recovery replaced momentum with a walking velocity kick");
    tick(heavy_sim, 90 * 3);
    require(!heavy_sim.snapshot().landing_recovering && heavy_sim.snapshot().landing_balance == 1,
            "physical recovery never restored balance");
    require(heavy_sim.snapshot().landing_recovery_work_j > 0, "active recovery footwork supplied no measured work");

    // Ordinary approach to the facade's broad switchgear cabinet.
    // A blocked movement request is not another landing: it must not keep
    // refreshing impact recovery and suppress the normal step-up path.
    Simulation blocked_sim;
    const auto blocked_landing = drop(blocked_sim, {20, 12.3, -121.15});
    require(blocked_landing.landing_recovering, "cabinet approach recorded no recovery");
    require(blocked_sim.set_move_input(0, -1), "cabinet approach input rejected");
    tick(blocked_sim, 90 * 4);
    const auto blocked = blocked_sim.snapshot();
    std::cout << "cabinet approach at=" << blocked.player_position.x << ','
              << blocked.player_position.y << ',' << blocked.player_position.z
              << " recovery=" << blocked.landing_recovery_seconds
              << " step_ups=" << blocked.step_up_count << '\n';
    require(blocked.player_grounded && blocked.death_count == 0 &&
            blocked.support_entity_id == 1600 &&
            blocked.player_position.z > -121.55 && blocked.player_position.z < -121.30,
            "cabinet did not physically block the grounded walking request");
    require(!blocked.landing_recovering && blocked.landing_balance == 1,
            "blocked walking request renewed impact recovery indefinitely");
    const auto stood = heavy_sim.snapshot().player_position;
    require(heavy_sim.debug_restart_at(stood), "actual settled grade pose failed solver-slop round trip");
    require(!heavy_sim.snapshot().landing_recovering && heavy_sim.snapshot().landing_balance == 1,
            "explicit restart retained landing recovery");

    Simulation dynamic_sim;
    const auto pouch = Slingshot::neutral_position();
    const auto dynamic = drop(dynamic_sim, {pouch.GetX(), pouch.GetY() + 1.73, pouch.GetZ()});
    require(dynamic.landing_support_entity_id == Slingshot::kPouchEntity,
            "dynamic pouch landing failed the generic support witness");
    Simulation rocker_sim;
    const auto rocker = drop(rocker_sim, {31, 69.2, -138.45});
    std::cout << "rocker support=" << rocker.landing_support_entity_id << " at=" << rocker.player_position.x << ','
              << rocker.player_position.y << ',' << rocker.player_position.z << '\n';
    require(rocker.landing_support_entity_id == 2800,
            "compound dynamic rocker landing failed the generic support witness");
    require(rocker.death_count == 0 && rocker.landing_approach_energy_j > 0,
            "dynamic support landing lacked actual energy receipt");
    require(rocker_sim.set_move_input(1, 0), "dynamic-support walking input rejected");
    double maximum_recovery_power = 0;
    for (int i = 0; i < 60; ++i) {
        const auto before = rocker_sim.snapshot().landing_recovery_work_j;
        tick(rocker_sim);
        maximum_recovery_power = std::max(maximum_recovery_power,
            (rocker_sim.snapshot().landing_recovery_work_j - before) / Simulation::kFixedStepSeconds);
    }
    require(maximum_recovery_power <= 3100,
            "moving-support footwork exceeded finite source power");
    require(rocker_sim.restart_checkpoint() && !rocker_sim.snapshot().landing_recovering,
            "checkpoint restart retained recovery source/state");

    require(heavy.landing_response == 1 && light.landing_response == 0,
            "impact-conditioned brace failed to distinguish real landings");
    Simulation roll_sim;
    require(roll_sim.debug_restart_at({40,16.9,-50}),"roll fall staging rejected");
    (void)roll_sim.set_crouch_input(true);
    (void)roll_sim.set_facing(1,0);(void)roll_sim.set_move_input(1,0);
    const auto before_roll=roll_sim.snapshot().landing_count;
    for(int i=0;i<450&&roll_sim.snapshot().landing_count==before_roll;++i)tick(roll_sim);
    const auto roll=roll_sim.snapshot();
    require(roll.landing_response==2&&roll.death_count==0,"prepared forward fall failed to choose compact recovery");
    (void)roll_sim.set_move_input(0,0);tick(roll_sim,6);
    require(roll_sim.snapshot().player_crouched&&roll_sim.snapshot().player_linear_velocity.x>roll.player_linear_velocity.x*.95,
            "roll lost compact posture or replaced incoming momentum");
    require(roll_sim.request_jump(),"roll recovery follow-up jump rejected");tick(roll_sim);
    require(!roll_sim.snapshot().player_grounded&&roll_sim.snapshot().player_linear_velocity.y>5,
            "roll consumed the player's escape jump");
    std::cout<<"roll normal="<<roll.landing_normal_speed_mps<<" tangent="<<roll.landing_tangent_speed_mps<<" compact=1 retained_momentum=1\n";

    Simulation unprepared;
    require(unprepared.debug_restart_at({40,16.9,-50}),"unprepared forward fall staging rejected");
    (void)unprepared.set_facing(1,0);(void)unprepared.set_move_input(1,0);
    for(int i=0;i<450&&unprepared.snapshot().landing_count==0;++i)tick(unprepared);
    require(unprepared.snapshot().landing_count>0&&unprepared.snapshot().landing_response==1&&
            unprepared.snapshot().death_count==0,"unprepared forward fall must brace rather than select roll");

    Simulation legacy(InitialSpawn::SurvivableDrop, WorldContent::RegressionFixtures);
    tick(legacy, 90 * 3);
    require(legacy.snapshot().landing_count == 0 && legacy.snapshot().death_count == 0,
            "production feedback changed historical fixture contract");
    std::cout << "PASS physical landing energy, finite recovery, dynamic supports and restart\n";
    return EXIT_SUCCESS;
}
