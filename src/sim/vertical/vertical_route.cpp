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

    // The pitman lift: turned so its crank's plane runs north-south at
    // x -21.0, its deck and receivers west of it (x -25.5 to -21.5). The
    // receivers sit at z -150.5 to -147.5, the deck north of them; the frame's
    // columns at z -168.3 and -140.3 stand clear of the north-west corner
    // column. Lower receiver 3.3 m over the origin: 291.25 m.
    place("sx.pitman_lift.v1", {RVec3(-23.5, 287.95, -154.3), -0.5F * JPH_PI},
          {kPitmanStaticFirst, 3}, {kPitmanDynamicFirst, 3});
    // A steel walkway west from the mast's upper receiver (x -17.45) out
    // through the open west face to the pitman's lower receiver (x -21.5),
    // flush with both at 291.25 m.
    kit_.add_body(kMastPitmanWalkway, {box(Vec3(2.025F, 0.15F, 1.25F), Material::Galvanised)},
                  RVec3(-19.475, 291.1, -149.25), Quat::sIdentity(), 0.0F, 0.8F);

    // The barrel helix, unturned: its barrel's axis at x -40.8, z -149, its
    // deck and receivers east of it, the lower receiver (x -28.5 to -25.5)
    // against the pitman's upper one. Lower receiver 4.55 m over the origin:
    // 314.89 m; upper receiver 338.89 m.
    place("sx.barrel_helix.v1", {RVec3(-40.8, 310.34, -149.0), 0.0F},
          {kHelixStaticFirst, 3}, {kHelixDynamicFirst, 3});
    // From its upper receiver east over the pitman's frame and through the
    // open west face to TP-340's west edge (x -14.6), 1.36 m under the plate's
    // top (340.25 m).
    kit_.add_body(kHelixPlateWalkway, {box(Vec3(5.45F, 0.15F, 1.5F), Material::Galvanised)},
                  RVec3(-20.05, 338.74, -149.0), Quat::sIdentity(), 0.0F, 0.8F);

    // The crown gondola, turned so its front faces west onto the 750 deck:
    // its crown turns in the plane x 20.1 about (765.85, z -152) and carries
    // the cabin south and up over its top; its receivers at x 12 to 15, the
    // lower one against the deck's east parapet, flush with the deck (750.1 m).
    place("sx.crown_gondola.v1", {RVec3(18.3, 747.85, -152.0), -0.5F * JPH_PI},
          {kCrownStaticFirst, 3}, {kCrownDynamicFirst, 2});
    // The luffing derrick, turned so its boom points south along x 16 from its
    // mast at z -193, north of the tower: its lower receiver (z -158 to -155)
    // against the crown's upper one at 778.09 m; its cradle starts at z -161
    // and is luffed up and north to its upper receiver at 802.64 m (z -184).
    // The crown's cabin never goes north of z -155, so the two never meet.
    place("sx.luffing_derrick.v1", {RVec3(13.5, 771.39, -193.0), -0.5F * JPH_PI},
          {kDerrickStaticFirst, 3}, {kDerrickDynamicFirst, 3});

    // The owner's slab incline, turned so the incline rises north toward the
    // south face at x -10 and its slab's tower stands east of it (x -5.5):
    // the trolley's deck at the foot 3.4 m over the yard (z -103), at the head
    // 33.4 m (z -120.4), its receivers west of it at x -14.9 to -11.9.
    incline_ = static_cast<int>(placed_.size());
    place("sx.slab_incline.v1", {RVec3(-10.0, 0.1, -103.08), 0.5F * JPH_PI},
          {kInclineStaticFirst, 3}, {kInclineDynamicFirst, 4});
    // A ramp from the yard (z -90) up to the boarding platform's south edge,
    // and a plate from the head's receiver over the face's edge beam to deck 3
    // (33.0 m), 0.4 m below it.
    {
        const RVec3 low(-13.4, 0.0, -90.0), high(-13.4, 3.4, -101.58);
        const Vec3 run(high - low);
        Part ramp = box(Vec3(1.5F, 0.15F, 0.5F * run.Length()), Material::Timber);
        ramp.rotation = Quat::sRotation(Vec3::sAxisX(), std::atan2(run.GetY(), -run.GetZ()));
        kit_.add_body(kInclineRamp, {ramp}, low + 0.5 * run - RVec3(0.0, 0.15, 0.0), Quat::sIdentity(), 0.0F, 0.8F);
        kit_.add_body(kInclineHeadPlate, {box(Vec3(1.5F, 0.15F, 0.85F), Material::Galvanised)},
                      RVec3(-13.4, 33.25, -122.75), Quat::sIdentity(), 0.0F, 0.8F);
    }

    // The owner's stone wheel, unturned: its plane east-west at z -110, its
    // axle 36 m up at x 16, its receivers on its north (front) side at
    // z -114.8 to -111.8; at rest a bucket stands at the lower receiver
    // (20.63 m, x 14.19) and its opposite at the upper one (46.37 m, x 17.81).
    // Its feed plank runs east from the lower receiver; it has no RAISE.
    place("sx.stone_wheel.v1", {RVec3(16.0, 17.2, -110.0), 0.0F}, {kWheelStaticFirst, 4}, {kWheelDynamicFirst, 12});
    wheel_ = static_cast<int>(placed_.size()) - 1;
    // Ramps north from the receivers to the face: down from deck 2's edge
    // (22.0 m) to the lower one, and from the upper one down to deck 4 (44.0 m).
    const auto ramp = [this](std::uint64_t entity, RVec3 south, RVec3 north) {
        const Vec3 run(north - south);
        Part plank = box(Vec3(1.5F, 0.15F, 0.5F * run.Length()), Material::Timber);
        plank.rotation = Quat::sRotation(Vec3::sAxisX(), std::atan2(run.GetY(), -run.GetZ()));
        kit_.add_body(entity, {plank}, south + 0.5 * run - RVec3(0.0, 0.15, 0.0), Quat::sIdentity(), 0.0F, 0.8F);
    };
    ramp(kWheelLowRamp, RVec3(14.19, 20.63, -114.8), RVec3(14.19, 22.0, -123.9));
    ramp(kWheelHighRamp, RVec3(17.81, 46.37, -114.8), RVec3(17.81, 44.0, -123.9));

    // The traction tram, turned half about so its track rises west from
    // beside the east bridge's end toward deck 8, along z -161, clear of the
    // east machine hall and the bridge's web: its lower receiver at x 79.1 to
    // 82.1 (64.0 m), its upper at x 28.4 to 31.4 (90.0 m).
    tram_ = static_cast<int>(placed_.size());
    place("sx.traction_tram.v1", {RVec3(77.3, 53.15, -161.0), JPH_PI}, {kTramStaticFirst, 5}, {kTramDynamicFirst, 3});
    // Off the bridge's east end, past its rails, onto a landing over its
    // pylon, and a gangway north down to the lower receiver's south edge.
    kit_.add_body(kTramLanding, {box(Vec3(1.3F, 0.15F, 2.2F), Material::Galvanised)},
                  RVec3(81.3, 64.55, -153.0), Quat::sIdentity(), 0.0F, 0.8F);
    ramp(kTramGangway, RVec3(81.1, 64.7, -155.2), RVec3(81.1, 64.0, -158.6));
    // From the upper receiver a plate on west to deck 8's east edge beam.
    kit_.add_body(kTramTopPlate, {box(Vec3(1.0F, 0.15F, 2.4F), Material::Galvanised)},
                  RVec3(27.4, 89.85, -161.0), Quat::sIdentity(), 0.0F, 0.8F);
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
    for (const auto &p : m.ports) {
        if (p.name.rfind("plate", 0) == 0) placed.deck_parts.push_back(p.body);
    }
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
        if (static_cast<int>(i) == wheel_) {
            continue;   // worked with the body, not Action
        }
        const Placed &p = placed_[i];
        Role role = Role::None;
        bool on_part = false;
        for (const BodyIndex part : p.deck_parts) on_part = on_part || kit_.body_entity(part) == support;
        if (kit_.body_entity(p.deck) == support || on_part) {
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
            here.target = static_cast<int>(i) == tram_ ? std::fmod(targets_[i], 2.0F) : targets_[i];
            here.moving = deck_speed(i) > 0.05F;
            return here;
        }
    }
    return here;
}

float Route::tram_departs_s() const noexcept {
    if (tram_ < 0) return -1.0F;
    const float state = targets_[static_cast<std::size_t>(tram_)];
    const float end = std::fmod(state, 2.0F);
    const float waited = std::floor(state / 2.0F);
    const int wait = end < .5F ? kTramFootWaitTicks : kTramHeadWaitTicks;
    const float at = travel(static_cast<std::size_t>(tram_));
    if (std::abs(at - end) > .01F || deck_speed(static_cast<std::size_t>(tram_)) > .05F) return -1.0F;
    return std::max(0.0F, static_cast<float>(wait) - waited) / 90.0F;
}

Route::InclineState Route::incline_state() const noexcept {
    if (incline_ < 0) return InclineState::Armed;
    return static_cast<InclineState>(static_cast<int>(targets_[static_cast<std::size_t>(incline_)] + .5F));
}

void Route::pre_step(float dt, bool action, std::uint64_t support) {
    if (action && incline_ >= 0) {
        // The slab incline: from its deck the plates work it, not Action;
        // from a receiver Action calls it there. Called down, its motor winds
        // the slab back up; called up, a trip line from the head pulls the
        // pin and the slab sends the empty trolley up.
        const Here here = at(support);
        if (here.machine == incline_) {
            float &state = targets_[static_cast<std::size_t>(incline_)];
            if (here.role == Role::Entry && here.travel > .01F) {
                state = float(InclineState::Resetting);
            } else if (here.role == Role::Exit && here.travel < .99F && incline_state() == InclineState::Armed) {
                state = float(InclineState::Released);
            }
            action = false;
        }
    }
    if (incline_ >= 0) {
        Placed &p = placed_[static_cast<std::size_t>(incline_)];
        Machine &m = *p.machine;
        float &state = targets_[static_cast<std::size_t>(incline_)];
        const float trip = m.pedals[0]->GetCurrentAngle();
        const float brake = m.pedals[1]->GetCurrentAngle();
        const float travel_now = travel(static_cast<std::size_t>(incline_));
        if (incline_state() == InclineState::Armed && trip > kInclineTripAngle) {
            state = float(InclineState::Released);
        }
        if (incline_state() == InclineState::Resetting && travel_now < .005F) {
            state = float(InclineState::Armed);
        }
        // The catch: while armed the slab is held at its top. The brake: the
        // treadle's travel presses the shoe, friction on the trolley's slider.
        if (incline_state() == InclineState::Armed) {
            m.holds[0]->SetLimits(-.001F, 0.0F);
            m.command({0.0F, true});
        } else {
            m.holds[0]->SetLimits(-kInclineSlabTravel, 0.0F);
            m.command(incline_state() == InclineState::Resetting ? Command{0.0F, true} : Command{1.0F, false});
        }
        incline_brake_n_ = kInclineBrakeFullN *
                            std::clamp(brake / (kInclineBrakeTakeUp * m.pedals[1]->GetLimitsMax()), 0.0F, 1.0F);
        m.drives[0].slider->SetMaxFrictionForce(incline_brake_n_);
    }
    if (action) {
        const Here here = at(support);
        if (here.machine >= 0 && here.machine != tram_) {
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
    if (tram_ >= 0) {
        // The traction tram shuttles on its own: stood at an end for its
        // wait, it is sent to the other. Action does nothing to it.
        float &state = targets_[static_cast<std::size_t>(tram_)];
        float end = std::fmod(state, 2.0F);
        float waited = std::floor(state / 2.0F);
        const std::size_t t = static_cast<std::size_t>(tram_);
        if (std::abs(travel(t) - end) < .01F && deck_speed(t) < .05F) {
            waited += 1.0F;
            if (waited >= static_cast<float>(end < .5F ? kTramFootWaitTicks : kTramHeadWaitTicks)) {
                end = 1.0F - end;
                waited = 0.0F;
            }
        } else {
            waited = 0.0F;
        }
        state = end + 2.0F * waited;
        placed_[t].machine->command({end, true});
    }
    if (wheel_ >= 0) {
        // The stone wheel's feed plank: tipped, it holds the wheel (armed);
        // dropped again after that, it lets the wheel go half a turn. The
        // count of half turns and the armed flag (.5) live in targets_.
        Placed &w = placed_[static_cast<std::size_t>(wheel_)];
        float &state = targets_[static_cast<std::size_t>(wheel_)];
        float half_turns = std::floor(state);
        bool armed = state - half_turns > .25F;
        const float plank = kit_.lever_angle(w.machine->levers.front());
        if (plank > .06F) {
            armed = true;
        } else if (armed && plank < .02F) {
            armed = false;
            half_turns += 1.0F;
        }
        state = half_turns + (armed ? .5F : 0.0F);
        w.machine->command({std::fmod(half_turns, 2.0F), true});
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
