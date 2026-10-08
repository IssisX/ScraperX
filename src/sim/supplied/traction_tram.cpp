#include "vertical_machine.hpp"
#include <cmath>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> traction_tram(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.traction_tram.v1",5,3);
    const float a=DegreesToRadians(35.f),height=33,travel=height/std::sin(a),dx=travel*std::cos(a);
    // Rail and its bolted receiver brackets share the fixed tower foundation.
    // Brackets are authored in the rotated rail body's local coordinates.
    // Two real rail heads under the wide rollers, with an open centre.
    // The archive's broad solid slab otherwise reads as a pedestrian ramp.
    std::vector<Part> rail={box(Vec3(39.5f,.3f,.12f),Material::Steel,Vec3(0,0,-.6f)),box(Vec3(39.5f,.3f,.12f),Material::Steel,Vec3(0,0,.6f))};
    const Vec3 centre(32.5f*std::cos(a),32.5f*std::sin(a),0);
    const Quat unroll=Quat::sRotation(Vec3::sAxisZ(),-a);
    const auto bracket=[&](Vec3 half,Vec3 p){auto b=box(half,Material::Steel,unroll*(p-centre));b.rotation=unroll;rail.push_back(b);};
    for(int end=0;end<2;++end){const float x=3.5f+(end?dx:0),y=2.f+(end?height:0);
        bracket(Vec3(.25f,.25f,3.1f),Vec3(x,y,-3.1f));
        bracket(Vec3(.25f,4.3f,.25f),Vec3(x,y+4.3f,-5.9f));
    }
    auto track=m->body("track",rail,centre,0,a);
    auto entry=m->body("entry",{box(Vec3(1.5f,.3f,2.6f),Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},Vec3(3.5f,10.55f,-4.9f),0);
    auto exit=m->body("exit",{box(Vec3(1.5f,.3f,2.6f),Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},Vec3(3.5f+dx,43.55f,-4.9f),0);
    const float low=2.3f/std::cos(a),high=low+7*std::tan(a);
    auto deck=m->body("deck",{
        box(Vec3(5,.25f,2.2f),Material::Timber),
        box(Vec3(.2f,(10.6f-low)/2,.2f),Material::Steel,Vec3(-3.5f,-(10.6f-low)/2,-1.2f)),
        box(Vec3(.2f,(10.6f-high)/2,.2f),Material::Steel,Vec3(3.5f,-(10.6f-high)/2,-1.2f)),
        box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,-1.2f)),
        box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.15f,-1.2f))
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
        Drive drive;drive.hinge=h;drive.feedback_body=deck;drive.feedback_origin=Vec3(3.5f,10.6f,0);drive.feedback_axis=Vec3(std::cos(a),std::sin(a),0);drive.radians_per_metre=-.5f;drive.extent=travel;drive.speed=.65f;drive.acceleration=.2f;m->drives.push_back(drive);
        m->no_collision(deck,roller);
    }
    // The planar bearing only prevents sideways motion and roll; uphill work
    // requires wheel/track frictional contact.
    (void)track;
    m->walkable("deck",deck,Vec3(0,.25f,0));m->walkable("entry",entry,Vec3(0,.3f,0));m->walkable("exit",exit,Vec3(0,.3f,0));
    return m;
}
}
