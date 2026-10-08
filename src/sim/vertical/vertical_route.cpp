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
    place("sx.slab_incline.v1", {RVec3(-10.0, 0.1, -103.08), 0.5F * JPH_PI},
          {kInclineStaticFirst, 3}, {kInclineDynamicFirst, 2});
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
