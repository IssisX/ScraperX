#include "sim/vertical/vertical_machine.hpp"
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> gravity_balance(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.gravity_balance.v1",3,2);
    m->body("frame",{
        box(Vec3(.3f,18,.3f),Material::Concrete,Vec3(4,17,-2.8f)),
        box(Vec3(.3f,18,.3f),Material::Concrete,Vec3(-7,17,-2.8f)),
        box(Vec3(6,.35f,.5f),Material::Steel,Vec3(-1.5f,34,-2.8f)),
        wheel(2.75f,.3f,Vec3(-2.75f,34,-1.8f))
    },Vec3::sZero(),0);
    auto entry=m->body("entry",{box(Vec3(1.5f,.3f,2),Material::Concrete)},Vec3(5.3f,3,0),0);
    auto exit=m->body("exit",{box(Vec3(1.5f,.3f,2),Material::Concrete)},Vec3(5.3f,28,0),0);
    auto deck=m->body("deck",{
        box(Vec3(3.5f,.3f,2),Material::Timber),
        box(Vec3(.12f,1,.12f),Material::Steel,Vec3(-3.2f,1,-1.8f)),
        box(Vec3(.12f,1,.12f),Material::Steel,Vec3(3.2f,1,-1.8f)),
        box(Vec3(3.3f,.12f,.12f),Material::Steel,Vec3(0,2,-1.8f))
    },Vec3(0,3,0),1000);
    auto ballast=m->body("ballast",{box(Vec3(1,2.5f,1),Material::Concrete)},Vec3(-5.5f,28,-1.8f),2500);
    auto slide=m->slider({},deck,Vec3::sAxisY(),0,25);
    m->slider({},ballast,Vec3::sAxisY(),-25,0);
    auto rope=c.kit.add_rope(deck,Vec3(0,2,-1.8f),m->point(Vec3(0,34,-1.8f)),ballast,Vec3(0,2.5f,0),m->point(Vec3(-5.5f,34,-1.8f)),1,32.5f,150000);
    m->ropes.push_back(rope);
    // Ascending, only negative force is allowed: the heavier ballast supplies
    // lifting work. Descending under command, this motor is the powered reset.
    slide->GetMotorSettings().SetForceLimits(-80000,0);
    Drive drive;drive.slider=slide;drive.extent=25;drive.speed=1.2f;
    m->drives.push_back(drive);
    m->walkable("deck",deck,Vec3(0,.3f,0));
    m->walkable("entry",entry,Vec3(0,.3f,0));
    m->walkable("exit",exit,Vec3(0,.3f,0));
    return m;
}
}
