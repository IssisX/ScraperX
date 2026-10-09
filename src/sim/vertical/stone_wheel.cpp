#include "sim/vertical/vertical_machine.hpp"
#include <algorithm>
#include <cmath>
#include <string>

// The owner's drawing (2026-10-08): an overshot stone wheel. Stone from the
// hopper over the top pours into the bucket just past the top, and that
// bucket's weight, falling, turns the 26 m wheel and carries a rider in the
// bucket opposite it from the bottom to the top.
//
// The encounter (goal phase 7). Nothing is pressed.
// - The feed plank: a timber plank runs out east from the lower receiver over
//   the 20 m drop, pivoted 1.5 m out, a counterweight under its inner end.
//   Walking out past about 2 m beyond the pivot tips it onto its stop. Its tip
//   pulls a rope up over the gantry to the hopper gate's counterweight arm, so
//   the gate opens and stone pours while the player stands out there.
// - The brake: the plank's rod sets the wheel's band brake. While the plank
//   is tipped the brake holds the wheel; when the player walks back in and
//   the plank drops, it lets the wheel go for one half turn. The brake only
//   ever resists: it governs the turn to 0.08 rad/s, holds the wheel against
//   turning back, and stops it at the next rest, half a turn on. Stone alone
//   drives it.
// - The ride: eight open buckets, 45 degrees apart, so at every rest one
//   stands at the lower receiver and its opposite stands at the top, under the
//   spout. Only that top bucket is filled. The opposite bucket starts with the
//   same lever arm as the stone, so the wheel moves only if the stone
//   outweighs the rider (85 kg). If it moves at all it carries the rider all
//   the way: both arms stay equal for the whole half turn. Too little stone
//   and nothing happens. A bucket left unboarded rides up empty and brings
//   the next bucket down to the receiver.
// - Discharge: at the bottom on the falling side a striker stands each
//   bucket's gate open as it passes, and the spent stone falls onto the pile
//   at the foot, which stays.
// - Supply: the hopper holds 16 t and is not refilled. A ride needs more than
//   85 kg; a bucket takes 400 kg. Stone poured past that spills to the pile.
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> stone_wheel(BuildContext c) {
    constexpr float kRadius = 13.0f;      // the pins' circle
    constexpr float kDrop = 2.5f;         // pin to bucket floor
    constexpr float kRimZ = 2.2f;         // the rim's plane, behind the buckets
    constexpr int kBuckets = 8;           // 45 degrees apart: a half turn is four
    // The wheel turns clockwise seen from the front (-z), down on the east
    // side: its upper receiver and the ramp from it to deck 4 stand east of
    // C1's last ladder. At rest it stands 8 degrees past the bottom and top,
    // so the stone over the top already has a lever arm: R sin(8 deg) = 1.81 m.
    const float rest_offset = DegreesToRadians(-8.0f);
    constexpr float kTurn = -1.0f;        // the wheel's forward sense about +z
    constexpr float kSpeed = .08f;        // the brake's governed turn, rad/s
    constexpr float kGroundY = -17.2f;    // as placed, the yard is 17.2 m under the origin
    const float centre_y = 3.43f + kDrop + kRadius * std::cos(rest_offset);   // bottom floor top 3.43 m
    const Vec3 centre(0, centre_y, 0);
    const float step = 2.0f * JPH_PI / kBuckets;
    const auto pin_at = [&](float angle) { return centre + kRadius * Vec3(std::cos(angle), std::sin(angle), 0); };
    const float bottom_angle = -.5f * JPH_PI + rest_offset;
    const float top_angle = .5f * JPH_PI + rest_offset;
    const Vec3 bottom_pin = pin_at(bottom_angle), top_pin = pin_at(top_angle);
    const float bottom_floor = bottom_pin.GetY() - kDrop, top_floor = top_pin.GetY() - kDrop;
    // The spout, over the top bucket at rest.
    const Vec3 mouth(top_pin.GetX(), top_pin.GetY() + 2.7f, 0);

    auto m = std::make_unique<Machine>(c, "sx.stone_wheel.v1", 4, kBuckets + 4);

    // The frame: a bearing A-frame behind the rim; a gantry carrying the
    // hopper, the chute's foot and the feed rope's two sheaves; and a mast at
    // the plank's tip.
    const float back_z = kRimZ + 1.6f;
    const Vec3 plank_pivot(bottom_pin.GetX() + 1.5f + 1.5f, bottom_floor - .075f, -3.3f);
    const float plank_out = 4.5f;                    // pivot to the plank's tip
    const Vec3 plank_tip(plank_pivot.GetX() + plank_out, bottom_floor, -3.3f);
    const Vec3 gate_pivot(mouth.GetX() - .35f, mouth.GetY() + .05f, 0);
    const Vec3 gate_weight(gate_pivot.GetX() - .5f, gate_pivot.GetY() + .3f, 0);
    const Vec3 sheave_high(gate_weight.GetX(), mouth.GetY() + 2.2f, 0);
    const Vec3 sheave_tip(plank_tip.GetX(), sheave_high.GetY(), -3.3f);
    std::vector<Part> frame;
    const auto span = [](Vec3 from, Vec3 to, float half, Material material) {
        const Vec3 run = to - from;
        auto p = box(Vec3(.5f * run.Length(), half, half), material, .5f * (from + to));
        p.rotation = Quat::sFromTo(Vec3::sAxisX(), run.Normalized());
        return p;
    };
    for (const float side : {-1.0f, 1.0f}) {
        frame.push_back(span(Vec3(side * 9.0f, kGroundY, back_z), Vec3(0, centre_y, back_z), .35f, Material::Timber));
    }
    frame.push_back(box(Vec3(.6f, .6f, .9f), Material::Steel, Vec3(0, centre_y, back_z - .5f)));   // bearing
    frame.push_back(span(Vec3(mouth.GetX(), kGroundY, back_z + .6f), Vec3(mouth.GetX(), sheave_high.GetY() + .4f, back_z + .6f),
                         .3f, Material::Timber));                                                     // hopper post
    frame.push_back(span(Vec3(mouth.GetX(), mouth.GetY() + 1.9f, back_z + .6f), Vec3(mouth.GetX(), mouth.GetY() + 1.9f, 0),
                         .25f, Material::Timber));                                                    // hopper arm
    // The tip's mast stands beside the plank's tip, off its line, so the
    // player out on the plank looks past it; an arm carries the sheave over
    // the tip.
    const float mast_z = -4.4f;
    frame.push_back(span(Vec3(sheave_tip.GetX(), kGroundY, mast_z), Vec3(sheave_tip.GetX(), sheave_high.GetY() + .4f, mast_z),
                         .3f, Material::Timber));                                                     // the tip's mast
    frame.push_back(span(Vec3(sheave_tip.GetX(), sheave_high.GetY() + .4f, mast_z),
                         Vec3(sheave_tip.GetX(), sheave_high.GetY() + .4f, -3.3f), .2f, Material::Timber));   // sheave arm
    frame.push_back(span(Vec3(sheave_tip.GetX(), sheave_high.GetY() + .4f, mast_z),
                         Vec3(mouth.GetX(), sheave_high.GetY() + .4f, back_z + .6f), .2f, Material::Timber));   // gantry
    frame.push_back(wheel(.45f, .1f, sheave_tip + Vec3(0, .1f, 0)));
    frame.push_back(wheel(.45f, .1f, sheave_high + Vec3(0, .1f, 0)));
    {
        // The chute, from the stone's yard up to the hopper (drawing: "stone pours in"),
        // its foot's post standing clear of the buckets' circle (14.3 m).
        const Vec3 low(mouth.GetX() - 17.0f, mouth.GetY() + 7.0f, 0), high(mouth.GetX() - .6f, mouth.GetY() + 2.4f, 0);
        frame.push_back(span(low, high, .1f, Material::Timber));
        frame.push_back(span(Vec3(low.GetX(), kGroundY, 0), Vec3(low.GetX(), low.GetY(), 0), .25f, Material::Timber));
    }
    auto structure = m->body("frame", frame, Vec3::sZero(), 0);

    auto entry = m->body("entry", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                         Vec3(bottom_pin.GetX(), bottom_floor - .3f, -3.3f), 0);
    auto exit = m->body("exit", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                        Vec3(top_pin.GetX(), top_floor - .3f, -3.3f), 0);

    // The hopper over the spout, full of stone: a bin with a gated mouth.
    auto hopper = m->body("hopper", {
        box(Vec3(1.2f, .1f, 1.2f), Material::Rust, Vec3(0, 1.0f, 0)),
        box(Vec3(1.4f, .9f, .1f), Material::Rust, Vec3(0, 1.9f, 1.3f)),
        box(Vec3(1.4f, .9f, .1f), Material::Rust, Vec3(0, 1.9f, -1.3f)),
        box(Vec3(.1f, .9f, 1.2f), Material::Rust, Vec3(1.3f, 1.9f, 0)),
        box(Vec3(.1f, .9f, 1.2f), Material::Rust, Vec3(-1.3f, 1.9f, 0)),
        box(Vec3(.3f, .5f, .3f), Material::Rust, Vec3(0, .4f, 0))
    }, mouth, 0);

    // The rotor: the rim, four diameters, the hub, and an arm out to each pin.
    std::vector<Part> rotor;
    for (int i = 0; i < 72; ++i) {
        const float a = 2.0f * JPH_PI * i / 72.0f;
        auto p = box(Vec3(.6f, .25f, .3f), Material::Timber,
                     Vec3(kRadius * std::cos(a), kRadius * std::sin(a), kRimZ));
        p.rotation = Quat::sRotation(Vec3::sAxisZ(), a + .5f * JPH_PI);
        rotor.push_back(p);
    }
    for (int i = 0; i < 4; ++i) {
        auto p = box(Vec3(kRadius, .22f, .2f), Material::Timber, Vec3(0, 0, kRimZ));
        p.rotation = Quat::sRotation(Vec3::sAxisZ(), rest_offset + i * step);
        rotor.push_back(p);
    }
    rotor.push_back(wheel(1.1f, .5f, Vec3(0, 0, kRimZ)));
    // The band brake's drum on the hub, in front of the bearing.
    rotor.push_back(wheel(1.6f, .15f, Vec3(0, 0, kRimZ + .9f)));
    for (int k = 0; k < kBuckets; ++k) {
        const float a = bottom_angle + k * step;
        rotor.push_back(box(Vec3(.15f, .15f, .5f * kRimZ), Material::Steel,
                            Vec3(kRadius * std::cos(a), kRadius * std::sin(a), .5f * kRimZ)));
    }
    auto wheel_body = m->body("wheel", rotor, centre, 6000);
    // No limits: the wheel goes round and round, half a turn a ride.
    auto hinge = m->hinge({}, wheel_body, centre);

    // The striker under the bottom, just past it: each bucket's gate stands
    // open on it 2 degrees past the bottom, when the rider opposite is past
    // the top.
    const Vec3 strike(-1.2f, bottom_floor - 1.5f, 0);
    auto striker = m->body("striker", {box(Vec3(.6f, .12f, .12f), Material::Yellow, Vec3(-.5f, 0, 0))}, strike, 30);
    const auto striker_lever = c.kit.add_lever(striker, m->point(strike), m->direction(Vec3::sAxisZ()),
                                               m->direction(Vec3::sAxisX()), 0.0f, .01f);

    // The hopper's gate, a plate under its mouth hinged at its west edge, its
    // counterweight beyond the hinge shutting it on its stop. Lifting the
    // counterweight swings the plate down open (positive about -z).
    auto gate = m->body("gate", {
        box(Vec3(.35f, .05f, .4f), Material::Yellow, Vec3(.35f, 0, 0)),
        box(Vec3(.25f, .25f, .25f), Material::Steel, Vec3(-.5f, .3f, 0))
    }, gate_pivot, 40);
    const Vec3 gate_axis = m->direction(-Vec3::sAxisZ());
    const auto gate_lever = c.kit.add_lever(gate, m->point(gate_pivot), gate_axis,
                                            m->direction(Vec3::sAxisX()), 0.0f, .6f);
    c.kit.add_bin(hopper, 16000.0f, 16000.0f, Vec3(0, -.1f, 0), gate_lever, .3f, 2.0f, 40.0f);

    // The feed plank: 6 m of timber, 0.6 m wide, its deck flush with the
    // receiver; 330 kg of iron under its inner end. With the player 2 m out
    // past the pivot it tips (outer end down, positive about -z) onto its
    // stop at 0.12 rad.
    auto plank = m->body("plank", {
        [] { auto p = box(Vec3(3.0f, .075f, .3f), Material::Timber, Vec3(1.5f, 0, 0)); p.mass_kg = 150; return p; }(),
        [] { auto p = box(Vec3(.3f, .3f, .3f), Material::Rust, Vec3(-1.2f, -.375f, 0)); p.mass_kg = 330; return p; }()
    }, plank_pivot, 480);
    const auto plank_lever = c.kit.add_lever(plank, m->point(plank_pivot), m->direction(-Vec3::sAxisZ()),
                                             m->direction(Vec3::sAxisX()), 0.0f, .12f);
    m->levers.push_back(plank_lever);
    // The feed rope: from the plank's tip up to the mast's sheave, along the
    // gantry and down to the gate's counterweight. The tip drops 0.54 m on
    // its stop; the purchase on the gate side (ratio 2.2) lifts the
    // counterweight 0.25 m, about 0.5 rad of the gate's 0.6 (open past 0.3).
    {
        const Vec3 tip_local(plank_out, .075f, 0);
        const Vec3 weight_local(-.5f, .3f, 0);
        constexpr float kRatio = 2.2f;
        const float length = (plank_tip - sheave_tip).Length() +
                             kRatio * (gate_weight - sheave_high).Length();
        m->ropes.push_back(c.kit.add_rope(plank, tip_local, m->point(sheave_tip), gate, weight_local,
                                          m->point(sheave_high), kRatio, length, 0));
    }

    // The buckets, level on their pins, all alike and open to the front: any
    // one at the bottom can be boarded, and each holds stone.
    for (int k = 0; k < kBuckets; ++k) {
        const float a = bottom_angle + k * step;
        const Vec3 pin = pin_at(a);
        std::vector<Part> parts = {
            box(Vec3(1.2f, .1f, 1.5f), Material::Steel, Vec3(0, -kDrop - .1f, 0)),        // floor first
            box(Vec3(1.2f, .5f, .05f), Material::Rust, Vec3(0, -kDrop + .5f, 1.45f)),
            box(Vec3(.05f, .5f, 1.5f), Material::Rust, Vec3(1.15f, -kDrop + .5f, 0)),
            box(Vec3(.05f, .5f, 1.5f), Material::Rust, Vec3(-1.15f, -kDrop + .5f, 0)),
            box(Vec3(.04f, .5f * kDrop, .04f), Material::Steel, Vec3(1.25f, -.5f * kDrop, 0)),
            box(Vec3(.04f, .5f * kDrop, .04f), Material::Steel, Vec3(-1.25f, -.5f * kDrop, 0)),
            box(Vec3(1.3f, .04f, .04f), Material::Steel, Vec3(0, 0, 0))
        };
        auto bucket = m->body(k == 0 ? "rider_bucket" : "bucket_" + std::to_string(k), parts, pin, 250);
        auto pin_hinge = m->hinge(wheel_body, bucket, pin);
        pin_hinge->SetMaxFrictionTorque(200);
        c.kit.set_damping(bucket, 0, .8f);
        m->no_collision(wheel_body, bucket);
        m->no_collision(bucket, striker);
        c.kit.add_bin(bucket, 0.0f, 400.0f, Vec3(0, -kDrop - .2f, 0), striker_lever, -.5f, 1.6f, 300.0f);
        if (k == 0) {
            m->walkable("deck", bucket, Vec3(0, -kDrop, 0));
        }
    }
    m->no_collision(wheel_body, striker);
    m->no_collision(wheel_body, gate);
    m->no_collision(structure, wheel_body);
    m->no_collision(hopper, gate);
    m->no_collision(structure, plank);
    m->no_collision(entry, plank);

    // The band brake. command.travel is 0 or 1: the rest the wheel is to stop
    // at, the build pose or half a turn on (the route flips it each time the
    // feed plank drops). It never drives:
    // turning forward, it brakes only above the governed speed or past the
    // rest; turning back, it holds.
    PhysicsSystem &world = c.world;
    const BodyID wheel_id = c.kit.body_id(wheel_body);
    const Vec3 wheel_axis = m->direction(Vec3::sAxisZ());
    hinge->SetMotorState(EMotorState::Velocity);
    m->stepping.push_back([&world, wheel_id, wheel_axis, hinge, speed = kSpeed](float, const Command &command) {
        // Forward is kTurn about +z; angles and speeds below are forward.
        const float target = command.travel > .5f ? JPH_PI : 0.0f;
        float error = std::fmod(target - kTurn * hinge->GetCurrentAngle() + 4.0f * JPH_PI, 2.0f * JPH_PI);
        if (error > 2.0f * JPH_PI - .3f) {
            error = 0.0f;   // just past the rest: held, not sent round again
        }
        const float omega = kTurn * world.GetBodyInterface().GetAngularVelocity(wheel_id).Dot(wheel_axis);
        auto &motor = hinge->GetMotorSettings();
        hinge->SetMotorState(EMotorState::Velocity);
        if (omega < 0.0f) {
            motor.SetTorqueLimits(-300000.0f, 300000.0f);
            hinge->SetTargetAngularVelocity(0.0f);
        } else {
            // Only against forward: the brake's torque is backward.
            motor.SetTorqueLimits(kTurn > 0 ? -300000.0f : 0.0f, kTurn > 0 ? 0.0f : 300000.0f);
            hinge->SetTargetAngularVelocity(kTurn * std::min(speed, 2.0f * error));
        }
    });

    m->walkable("entry", entry, Vec3(0, .3f, 0));
    m->walkable("exit", exit, Vec3(0, .3f, 0));
    return m;
}
}  // namespace scraperx::sim::vertical
