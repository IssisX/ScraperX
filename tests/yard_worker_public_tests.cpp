#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <functional>

using namespace scraperx::sim;
namespace {
void require(bool condition, const char *reason) {
    if (condition) return;
    std::cerr << "FAIL YARD_WORKER_PUBLIC " << reason << std::endl;
    std::exit(EXIT_FAILURE);
}
void step(Simulation &sim, int count = 1) {
    while (count--) require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "90Hz tick accepted");
}
double distance(Vector3 a, Vector3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
void walk_to(Simulation &sim, Vector3 target, const std::function<void()> &observe = {}) {
    for (int i=0; i<600; ++i) {
        if (observe) observe();
        const auto p = sim.snapshot().player_position;
        const double x = target.x-p.x, z = target.z-p.z, d = std::hypot(x,z);
        if (d < .13) {
            require(sim.set_move_input(0,0), "walking input released");
            step(sim,12);
            return;
        }
        const double speed = std::min(.28,d*.6);
        require(sim.set_move_input(x/d*speed,z/d*speed), "ordinary walking input accepted");
        step(sim);
    }
    const auto p=sim.snapshot().player_position;
    std::cerr << "WALK_STOP " << p.x << ',' << p.y << ',' << p.z << std::endl;
    require(false,"ordinary arrival approach reaches target without staging");
}
void normal_arrival() {
    Simulation sim(InitialSpawn::FreightYardArrival,WorldContent::Slingshot);
    require(sim.entity_body_count(Simulation::kYardWorkerEntityId)==1,"one real worker capsule exists");
    require(sim.kit_body_index(Simulation::kYardWorkerBenchEntityId)<sim.kit_body_count(),"bench has a real Kit body");
    step(sim,90);
    auto actor=sim.yard_worker_state();
    std::cout << "YARD_WORKER start=" << actor.position.x << ',' << actor.position.y << ',' << actor.position.z
              << " support=" << actor.support_entity_id << " activity=" << int(actor.activity) << std::endl;
    require(actor.present && actor.supported && actor.activity==YardWorkerActivity::Working,
            "worker settles and welds beside ordinary arrival");
    require(std::abs(actor.position.y-.9)<.04,"worker soles stand on real yard grade");
    const auto start=sim.snapshot().player_position;
    bool attended=false,pointed=false;
    // The longer walk may include the greeting; observe it throughout rather
    // than requiring the player to arrive during one exact activity frame.
    const auto observe_greeting=[&] {
        actor=sim.yard_worker_state();
        attended|=actor.activity==YardWorkerActivity::Attending;
        pointed|=actor.activity==YardWorkerActivity::Pointing;
        require(actor.supported,"greeting retains real ground contact");
        require(std::hypot(actor.drive_force_n.x,actor.drive_force_n.z)<=220.001,
                "greeting retains finite native walk force");
        if (actor.activity==YardWorkerActivity::Pointing)
            require(distance(actor.look_target,actor.guide_point)<.00001,
                    "point targets the authored crane-yard direction");
    };
    walk_to(sim,{-93.15,0,-45.6},observe_greeting);
    walk_to(sim,{-89.15,0,-48.6},observe_greeting);
    for (int i=0;i<260;++i) {
        observe_greeting();
        if (attended && pointed) break;
        step(sim);
    }
    require(attended && pointed,"normal player approach makes worker notice then point");
    const auto before=sim.snapshot().player_position;
    // Retrace the clear approach, then pass the cargo beam at its west end.
    walk_to(sim,{-93.15,0,-45.6});
    walk_to(sim,{-99,0,-45.6});
    walk_to(sim,{-99,0,-43.5});
    const auto after=sim.snapshot().player_position;
    require(std::hypot(after.x-before.x,after.z-before.z)>4.5 &&
            std::hypot(after.x-start.x,after.z-start.z)>4.5,"player walks onward freely through the complete interaction");
    step(sim,450);
    actor=sim.yard_worker_state();
    require(actor.supported && actor.activity==YardWorkerActivity::Working,
            "worker resumes actual supported work after player passes");

    // Existing public debug placement creates a retry point. Only this
    // checkpoint subcase is staged; the approach above starts normally.
    // Keep the player airborne during this subcase so ordinary new firm
    // footing cannot legitimately replace the checkpoint being compared.
    require(sim.debug_restart_at({0,20,0}),"create existing retry point during player flight");
    const auto saved=sim.yard_worker_state();
    step(sim,100);
    require(sim.restart_checkpoint(),"existing whole-world retry succeeds");
    const auto restored=sim.yard_worker_state();
    require(distance(saved.position,restored.position)<.00001 && distance(saved.velocity,restored.velocity)<.00001,
            "retry restores actual worker position and momentum");
    require(saved.activity==restored.activity && saved.activity_seconds==restored.activity_seconds &&
            restored.episode==saved.episode+1,"retry restores activity and begins a new presentation episode");
    require(!restored.supported && distance(restored.drive_force_n,{})<.00001,
            "retry invalidates old support and walking force");
    step(sim,90);
    require(sim.yard_worker_state().supported,"fresh post-retry solve reacquires actual ground");

    const auto moving=sim.moving_body_count();
    for (int i=0;i<4;++i) {
        require(sim.restart_checkpoint(),"repeated retry succeeds");
        require(sim.entity_body_count(Simulation::kYardWorkerEntityId)==1 && sim.moving_body_count()==moving,
                "retry leaks no bodies and preserves actor identity");
    }
    require(sim.debug_restart_at({-87.9,.9,-50}),"stage only collision subcase beside worker");
    step(sim,30);
    const auto before_shove=sim.yard_worker_state().position;
    require(sim.set_move_input(-.8,0),"ordinary player walking drives collision");
    double peak_speed=0;
    for (int i=0;i<40;++i) {
        step(sim);
        const auto pushed=sim.yard_worker_state();
        peak_speed=std::max(peak_speed,std::hypot(pushed.velocity.x,pushed.velocity.z));
        require(std::hypot(pushed.drive_force_n.x,pushed.drive_force_n.z)<=220.001,
                "collision never raises worker drive authority");
    }
    require(sim.set_move_input(0,0),"release collision input");
    const auto pushed=sim.yard_worker_state();
    std::cout << "YARD_WORKER shove_dx=" << pushed.position.x-before_shove.x << " peak_speed=" << peak_speed << std::endl;
    require(pushed.position.x<before_shove.x-.1 && peak_speed>1.5,
            "actual player contact pushes worker beyond its own walking speed");
    std::cout << "YARD_WORKER_PUBLIC PASS arrival notice point onward return checkpoint collision" << std::endl;
}
}
int main() { normal_arrival(); }
