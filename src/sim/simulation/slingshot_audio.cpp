#include "sim/simulation/world.hpp"

namespace scraperx::sim {
double Simulation::slingshot_ratchet_draw_m() const noexcept {
    const auto *launcher = physics_world_->slingshot();
    return launcher ? launcher->state().ratchet_draw_m : 0.0;
}
} // namespace scraperx::sim
