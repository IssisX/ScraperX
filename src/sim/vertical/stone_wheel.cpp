#include "sim/vertical/vertical_machine.hpp"
#include <cmath>
#include <string>

// The owner's drawing (2026-10-08): an overshot stone wheel. Stone pours from
// a hopper fed by an inclined chute into the bucket just past the top of a
// 26 m wheel; the loaded side's weight turns it, and the rider, standing in a
// bucket that started at the bottom, is carried round and up to the top. The
// buckets hang level from pins on arms off the rim, which turns in a plane
// behind them (+z), so the front (-z) is open for the rider to step in and
// out. Past the bottom on the falling side each stone bucket's mouth passes a
// striker and empties onto the pile at the foot.
//
// The wheel's own motor only ever holds it back (a brake, as the gravity
// balance's): it keeps the turn to 0.08 rad/s and holds the wheel at the top.
// Sent back down, the same motor turns it back, the powered reset.
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> stone_wheel(BuildContext c) {
    constexpr float kRadius = 13.0f;      // the pins' circle
    constexpr float kDrop = 2.5f;         // pin to bucket floor
    constexpr float kRimZ = 2.2f;         // the rim's plane, behind the buckets
    constexpr int kBuckets = 9;           // 40 degrees apart
    constexpr float kFloorTop = 3.3f;     // the rider's bucket floor at the bottom
    // Half a turn, bottom to top, less a hair: a hinge limit must lie inside +-pi.
    constexpr float kTurn = JPH_PI - .02f;
    const float centre_y = kFloorTop + kDrop + kRadius;   // 18.8 m
    const Vec3 centre(0, centre_y, 0);
    const float step = 2.0f * JPH_PI / kBuckets;
    const auto pin_at = [&](float angle) { return centre + kRadius * Vec3(std::cos(angle), std::sin(angle), 0); };
    // The spout: 20 degrees past the top on the falling (-x) side, over the
    // bucket that stands there as built.
    const float spout_angle = DegreesToRadians(110.0f);
    const Vec3 spout_pin = pin_at(spout_angle);
    const Vec3 mouth(spout_pin.GetX(), spout_pin.GetY() + 2.7f, 0);

    auto m = std::make_unique<Machine>(c, "sx.stone_wheel.v1", 4, kBuckets + 3);

    // The frame: a bearing A-frame behind the rim, and a gantry from it over
    // the top carrying the hopper and the chute's foot.
    std::vector<Part> frame;
    const float back_z = kRimZ + 1.6f;
    for (const float side : {-1.0f, 1.0f}) {
        const float foot_x = side * 9.0f;
        const Vec3 foot(foot_x, 0, back_z), head(0, centre_y, back_z);
        const Vec3 leg = head - foot;
        auto p = box(Vec3(.35f, .5f * leg.Length(), .35f), Material::Timber, .5f * (foot + head));
        p.rotation = Quat::sRotation(Vec3::sAxisZ(), std::atan2(-leg.GetX(), leg.GetY()));
        frame.push_back(p);
    }
    frame.push_back(box(Vec3(.6f, .6f, .9f), Material::Steel, Vec3(0, centre_y, back_z - .5f)));   // bearing
    frame.push_back(box(Vec3(.3f, .5f * (mouth.GetY() + 2.0f), .3f), Material::Timber,
                        Vec3(mouth.GetX(), .5f * (mouth.GetY() + 2.0f), back_z + .6f)));             // hopper post
    frame.push_back(box(Vec3(.25f, .25f, .5f * (back_z + .6f)), Material::Timber,
                        Vec3(mouth.GetX(), mouth.GetY() + 1.9f, .5f * (back_z + .6f))));             // gantry arm
    {
        // The chute, from the stone's yard up to the hopper (drawing: "stone pours in").
        const Vec3 low(mouth.GetX() - 12.0f, mouth.GetY() + 6.0f, 0), high(mouth.GetX() - .6f, mouth.GetY() + 2.4f, 0);
        const Vec3 run = high - low;
        auto chute = box(Vec3(.5f * run.Length(), .1f, .9f), Material::Timber, .5f * (low + high));
        chute.rotation = Quat::sRotation(Vec3::sAxisZ(), std::atan2(run.GetY(), run.GetX()));
        frame.push_back(chute);
        frame.push_back(box(Vec3(.25f, .5f * low.GetY(), .25f), Material::Timber, Vec3(low.GetX(), .5f * low.GetY(), 0)));
    }
    auto structure = m->body("frame", frame, Vec3::sZero(), 0);

    auto entry = m->body("entry", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                         Vec3(0, kFloorTop - .3f, -3.3f), 0);
    auto exit = m->body("exit", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                        Vec3(0, kFloorTop + 2.0f * kRadius - .3f, -3.3f), 0);

    // The hopper over the spout, full of stone: a bin with a gated mouth.
    auto hopper = m->body("hopper", {
        box(Vec3(1.2f, .1f, 1.2f), Material::Rust, Vec3(0, 1.0f, 0)),
        box(Vec3(1.4f, .9f, .1f), Material::Rust, Vec3(0, 1.9f, 1.3f)),
        box(Vec3(1.4f, .9f, .1f), Material::Rust, Vec3(0, 1.9f, -1.3f)),
        box(Vec3(.1f, .9f, 1.2f), Material::Rust, Vec3(1.3f, 1.9f, 0)),
        box(Vec3(.1f, .9f, 1.2f), Material::Rust, Vec3(-1.3f, 1.9f, 0)),
        box(Vec3(.3f, .5f, .3f), Material::Rust, Vec3(0, .4f, 0))
    }, mouth, 0);

    // The rotor: the rim, three diameters, the hub, and an arm out to each pin.
    std::vector<Part> rotor;
    for (int i = 0; i < 72; ++i) {
        const float a = 2.0f * JPH_PI * i / 72.0f;
        auto p = box(Vec3(.6f, .25f, .3f), Material::Timber,
                     Vec3(kRadius * std::cos(a), kRadius * std::sin(a), kRimZ));
        p.rotation = Quat::sRotation(Vec3::sAxisZ(), a + .5f * JPH_PI);
        rotor.push_back(p);
    }
    for (int i = 0; i < 3; ++i) {
        auto p = box(Vec3(kRadius, .22f, .2f), Material::Timber, Vec3(0, 0, kRimZ));
        p.rotation = Quat::sRotation(Vec3::sAxisZ(), i * JPH_PI / 3.0f);
        rotor.push_back(p);
    }
    rotor.push_back(wheel(1.1f, .5f, Vec3(0, 0, kRimZ)));
    for (int k = 0; k < kBuckets; ++k) {
        const float a = -.5f * JPH_PI + k * step;
        rotor.push_back(box(Vec3(.15f, .15f, .5f * kRimZ), Material::Steel,
                            Vec3(kRadius * std::cos(a), kRadius * std::sin(a), .5f * kRimZ)));
    }
    auto wheel_body = m->body("wheel", rotor, centre, 6000);
    auto hinge = m->hinge({}, wheel_body, centre, -.02f, kTurn + .015f);
    hinge->GetMotorSettings().SetTorqueLimits(-300000, 0);

    // The striker at the foot on the falling side: a bar every stone bucket's
    // mouth passes, which stands its gate open.
    const float strike_angle = DegreesToRadians(240.0f);
    const Vec3 strike_pin = pin_at(strike_angle);
    const Vec3 strike(strike_pin.GetX(), strike_pin.GetY() - kDrop - .2f, -2.2f);
    auto striker = m->body("striker", {box(Vec3(.6f, .12f, .12f), Material::Yellow, Vec3(.5f, 0, 0))}, strike, 30);
    const auto striker_lever = c.kit.add_lever(striker, m->point(strike), m->direction(Vec3::sAxisZ()),
                                               m->direction(Vec3::sAxisX()), 0.0f, .01f);

    // The hopper's gate, a plate under its mouth hinged at its west edge, its
    // counterweight beyond the hinge shutting it on its stop; the gate's ram
    // swings the plate down open (positive about -z).
    const Vec3 gate_pivot(mouth.GetX() - .35f, mouth.GetY() + .05f, 0);
    auto gate = m->body("gate", {
        box(Vec3(.35f, .05f, .4f), Material::Yellow, Vec3(.35f, 0, 0)),
        box(Vec3(.25f, .25f, .25f), Material::Steel, Vec3(-.5f, .3f, 0))
    }, gate_pivot, 40);
    const Vec3 gate_axis = m->direction(-Vec3::sAxisZ());
    const auto gate_lever = c.kit.add_lever(gate, m->point(gate_pivot), gate_axis,
                                            m->direction(Vec3::sAxisX()), 0.0f, .6f);
    c.kit.add_bin(hopper, 16000.0f, 16000.0f, Vec3(0, -.1f, 0), gate_lever, .3f, 2.0f, 40.0f);

    // The buckets, level on their pins. The rider's (k = 0) starts at the
    // bottom and is open at the front; the others hold stone.
    BodyIndex rider_bucket;
    for (int k = 0; k < kBuckets; ++k) {
        const float a = -.5f * JPH_PI + k * step;
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
        if (k != 0) {
            parts.push_back(box(Vec3(1.2f, .5f, .05f), Material::Rust, Vec3(0, -kDrop + .5f, -1.45f)));
        }
        auto bucket = m->body(k == 0 ? "rider_bucket" : "bucket_" + std::to_string(k), parts, pin, 250);
        auto pin_hinge = m->hinge(wheel_body, bucket, pin);
        pin_hinge->SetMaxFrictionTorque(200);
        c.kit.set_damping(bucket, 0, .8f);
        m->no_collision(wheel_body, bucket);
        m->no_collision(bucket, striker);
        if (k == 0) {
            rider_bucket = bucket;
        } else {
            c.kit.add_bin(bucket, 0.0f, 400.0f, Vec3(0, -kDrop - .2f, 0), striker_lever, -.5f, 2.6f, 300.0f);
        }
    }
    m->no_collision(wheel_body, striker);
    m->no_collision(wheel_body, gate);
    m->no_collision(structure, wheel_body);
    m->no_collision(hopper, gate);

    Drive drive;
    drive.hinge = hinge;
    drive.extent = kTurn;
    drive.speed = .08f;
    drive.acceleration = .05f;
    m->drives.push_back(drive);

    // Sent up, the gate's ram holds the gate open until the wheel is at the
    // top; otherwise its weight shuts it.
    PhysicsSystem &world = c.world;
    const BodyID gate_id = c.kit.body_id(gate);
    m->stepping.push_back([&world, gate_id, gate_axis, hinge](float, const Command &command) {
        if (command.powered && command.travel > .5f && hinge->GetCurrentAngle() < kTurn - .03f) {
            world.GetBodyInterface().AddTorque(gate_id, 1500.0f * gate_axis);
        }
    });

    m->walkable("deck", rider_bucket, Vec3(0, -kDrop, 0));
    m->walkable("entry", entry, Vec3(0, .3f, 0));
    m->walkable("exit", exit, Vec3(0, .3f, 0));
    return m;
}
}  // namespace scraperx::sim::vertical
