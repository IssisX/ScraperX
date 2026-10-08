#include "sim/vertical/vertical_route.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim::vertical {

namespace {

const Port &port(const Machine &machine, const char *name) {
    for (const auto &p : machine.ports) {
        if (p.name == name) {
            return p;
        }
    }
    throw std::logic_error(machine.id + " has no port " + name);
}

}  // namespace

Route::Route(kit::Kit &kit, PhysicsSystem &world) : kit_(kit), world_(world) {
    // The cascade mast: turned half about, so its receivers face west onto the
    // 264 ring's west band (inner edge x -17.45) and its frame stands in the
    // well (x -15.85 to -4.95, clear of the 286 band's inner edge at -16.54
    // and of the 308 band's at -15.63). Its lower receiver and deck tops are
    // 3.3 m over its origin, so 264.25 m: flush with the band; its upper
    // receiver is 27 m higher, at 291.25 m.
    place("sx.cascade_mast.v1", {RVec3(-10.65, 260.95, -150.0), JPH_PI},
          {kCascadeStaticFirst, 3}, {kCascadeDynamicFirst, 3});
}

Route::~Route() = default;

void Route::place(const char *id, Placement placement, EntityRange statics, EntityRange dynamics) {
    Placed placed;
    placed.machine = create(id, BuildContext{kit_, world_, placement, statics, dynamics});
    const Machine &m = *placed.machine;
    placed.deck = port(m, "deck").body;
    placed.entry = port(m, "entry").body;
    placed.exit = port(m, "exit").body;
    placed.entry_y = static_cast<float>(m.port_position(port(m, "entry")).GetY());
    placed.exit_y = static_cast<float>(m.port_position(port(m, "exit")).GetY());
    placed.machine->command({0.0F, true});
    placed_.push_back(std::move(placed));
    targets_.push_back(0.0F);
}

float Route::travel(std::size_t i) const {
    const Placed &p = placed_[i];
    const float deck_y = static_cast<float>(p.machine->port_position(port(*p.machine, "deck")).GetY());
    return (deck_y - p.entry_y) / (p.exit_y - p.entry_y);
}

float Route::deck_speed(std::size_t i) const {
    return kit_.body_velocity(placed_[i].deck).Length();
}

Route::Here Route::at(std::uint64_t support) const {
    Here here;
    if (support == 0) {
        return here;
    }
    for (std::size_t i = 0; i < placed_.size(); ++i) {
        const Placed &p = placed_[i];
        Role role = Role::None;
        if (kit_.body_entity(p.deck) == support) {
            role = Role::Deck;
        } else if (kit_.body_entity(p.entry) == support) {
            role = Role::Entry;
        } else if (kit_.body_entity(p.exit) == support) {
            role = Role::Exit;
        }
        if (role != Role::None) {
            here.machine = static_cast<int>(i);
            here.role = role;
            here.travel = travel(i);
            here.target = targets_[i];
            here.moving = deck_speed(i) > 0.05F;
            return here;
        }
    }
    return here;
}

void Route::pre_step(float dt, bool action, std::uint64_t support) {
    if (action) {
        const Here here = at(support);
        if (here.machine >= 0) {
            float &target = targets_[static_cast<std::size_t>(here.machine)];
            switch (here.role) {
                case Role::Deck: target = target < 0.5F ? 1.0F : 0.0F; break;
                case Role::Entry: target = 0.0F; break;
                case Role::Exit: target = 1.0F; break;
                case Role::None: break;
            }
            placed_[static_cast<std::size_t>(here.machine)].machine->command({target, true});
        }
    }
    for (auto &p : placed_) {
        p.machine->pre_step(dt);
    }
}

Route::State Route::state() const {
    State state;
    for (const auto &p : placed_) {
        state.controls.push_back(p.machine->capture_control());
    }
    state.targets = targets_;
    return state;
}

void Route::restore(const State &state) {
    if (state.controls.size() != placed_.size() || state.targets.size() != placed_.size()) {
        throw std::invalid_argument("vertical route checkpoint does not match the route");
    }
    for (std::size_t i = 0; i < placed_.size(); ++i) {
        placed_[i].machine->restore_control(state.controls[i]);
    }
    targets_ = state.targets;
}

}  // namespace scraperx::sim::vertical
