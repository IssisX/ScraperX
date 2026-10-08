#include "sim/vertical/vertical_machine.hpp"
#include <cmath>
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> crown_gondola(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.crown_gondola.v1",3,2);
    const float target=DegreesToRadians(178.f);
    m->body("bearing_frame",{
        box(Vec3(.5f,19,.5f),Material::Concrete,Vec3(-3,18,-3.5f)),
        box(Vec3(3,.4f,.4f),Material::Steel,Vec3(-1,18,-3.5f))
    },Vec3::sZero(),0);
    auto entry=m->body("entry",{box(Vec3(3.5f,.3f,1.5f),Material::Concrete)},Vec3(0,1.95f,4.8f),0);
    const float endx=14*std::sin(target),endy=18-14*std::cos(target)-2;
    auto exit=m->body("exit",{box(Vec3(3.5f,.3f,1.5f),Material::Concrete)},Vec3(endx,endy-.05f,4.8f),0);
    std::vector<Part> rotor;
    for(int i=0;i<64;++i){const float a=2*JPH_PI*i/64;auto p=box(Vec3(.75f,.22f,.35f),Material::Steel,Vec3(14*std::cos(a),14*std::sin(a),0));p.rotation=Quat::sRotation(Vec3::sAxisZ(),a+JPH_PI/2);rotor.push_back(p);}
    // Full diametral spokes, not four half-length arms.
    for(int i=0;i<2;++i){auto p=box(Vec3(14,.22f,.28f),Material::Rust);p.rotation=Quat::sRotation(Vec3::sAxisZ(),i*JPH_PI/2);rotor.push_back(p);}
    auto wheel=m->body("crown",rotor,Vec3(0,18,-1.8f),7000);
    auto h=m->hinge({},wheel,Vec3(0,18,-1.8f),DegreesToRadians(-2.f),DegreesToRadians(179.f));
    h->GetMotorSettings().SetTorqueLimit(400000);
    auto deck=m->body("deck",{
        box(Vec3(3,.25f,2),Material::Timber),
        box(Vec3(.12f,1,.12f),Material::Steel,Vec3(-2.8f,1,-1.8f)),
        box(Vec3(.12f,1,.12f),Material::Steel,Vec3(2.8f,1,-1.8f)),
        box(Vec3(2.8f,.12f,.12f),Material::Steel,Vec3(0,2,-1.8f)),
        box(Vec3(1,.3f,1),Material::Concrete,Vec3(0,-.6f,0))
    },Vec3(0,2,1),1200);
    auto hanger=m->hinge(wheel,deck,Vec3(0,4,-.8f));hanger->SetMaxFrictionTorque(100);c.kit.set_damping(deck,0,.3f);
    m->no_collision(wheel,deck);
    Drive drive;drive.hinge=h;drive.extent=target;drive.speed=.13f;drive.acceleration=.03f;m->drives.push_back(drive);
    m->walkable("deck",deck,Vec3(0,.25f,0));m->walkable("entry",entry,Vec3(0,.3f,0));m->walkable("exit",exit,Vec3(0,.3f,0));
    return m;
}
}
