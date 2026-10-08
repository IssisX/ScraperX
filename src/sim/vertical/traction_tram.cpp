#include "sim/vertical/vertical_machine.hpp"
#include <cmath>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> traction_tram(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.traction_tram.v1",5,3);
    const float a=DegreesToRadians(35.f),height=26,travel=height/std::sin(a),dx=travel*std::cos(a);
    auto track=m->body("track",{box(Vec3(34,.3f,1.4f),Material::Steel)},Vec3(27*std::cos(a),27*std::sin(a),0),0,a);
    auto entry=m->body("entry",{box(Vec3(1.5f,.3f,2.4f),Material::Concrete)},Vec3(-3.3f,10.55f,0),0);
    auto exit=m->body("exit",{box(Vec3(1.5f,.3f,2.4f),Material::Concrete)},Vec3(10.3f+dx,36.55f,0),0);
    const float low=2.3f/std::cos(a),high=low+7*std::tan(a);
    auto deck=m->body("deck",{
        box(Vec3(5,.25f,2.2f),Material::Timber),
        box(Vec3(.2f,(10.6f-low)/2,.2f),Material::Steel,Vec3(-3.5f,-(10.6f-low)/2,-1.2f)),
        box(Vec3(.2f,(10.6f-high)/2,.2f),Material::Steel,Vec3(3.5f,-(10.6f-high)/2,-1.2f))
    },Vec3(3.5f,10.6f,0),1600);
    // Planar bearing: XY translation remains free. The rail contacts must
    // carry normal weight or there is no usable traction.
    SixDOFConstraintSettings planar;
    planar.mPosition1=planar.mPosition2=m->point(Vec3(3.5f,10.6f,0));
    planar.mAxisX1=planar.mAxisX2=m->direction(Vec3::sAxisX());
    planar.mAxisY1=planar.mAxisY2=Vec3::sAxisY();
    planar.MakeFreeAxis(SixDOFConstraintSettings::EAxis::TranslationX);
    planar.MakeFreeAxis(SixDOFConstraintSettings::EAxis::TranslationY);
    for(int axis=2;axis<6;++axis)planar.MakeFixedAxis(static_cast<SixDOFConstraintSettings::EAxis>(axis));
    planar.mNumVelocityStepsOverride=40;planar.mNumPositionStepsOverride=8;
    m->own(planar.Create(m->native({}),m->native(deck)));
    const Vec3 along(std::cos(a),std::sin(a),0),normal(-std::sin(a),std::cos(a),0);
    const float first=low*std::sin(a),last=7*std::cos(a)+high*std::sin(a);
    // Axle-height wheel bumpers fit between the rear bearing posts. A tall
    // full-width barrier would hit the horizontal deck before docking.
    m->body("lower_stop",{box(Vec3(.25f,.65f,.6f),Material::Concrete)},along*(first-2.25f)+normal*2.3f,0,a);
    m->body("upper_stop",{box(Vec3(.25f,.65f,.6f),Material::Concrete)},along*(last+travel+2.25f)+normal*2.3f,0,a);
    for(int i=0;i<2;++i){
        Vec3 axle(i?7:0,i?high:low,0);
        auto roller=m->body("wheel_"+std::to_string(i),{wheel(2,.8f)},axle,180);
        // Uphill weight transfer loads the lower wheel more heavily. A motor
        // sized from half the total drawbar force stalls that loaded wheel.
        auto h=m->hinge(deck,roller,axle);h->GetMotorSettings().SetTorqueLimit(24000);
        Drive drive;drive.hinge=h;drive.feedback_body=deck;drive.feedback_origin=Vec3(3.5f,10.6f,0);drive.feedback_axis=Vec3(std::cos(a),std::sin(a),0);drive.radians_per_metre=-.5f;drive.extent=travel;drive.speed=.65f;m->drives.push_back(drive);
        m->no_collision(deck,roller);
    }
    // The planar bearing only prevents sideways motion and roll; uphill work
    // requires wheel/track frictional contact.
    (void)track;
    m->walkable("deck",deck,Vec3(0,.25f,0));m->walkable("entry",entry,Vec3(0,.3f,0));m->walkable("exit",exit,Vec3(0,.3f,0));
    return m;
}
}
