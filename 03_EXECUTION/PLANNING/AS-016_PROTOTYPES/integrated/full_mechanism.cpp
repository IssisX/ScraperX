#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
using namespace JPH;
struct BP : BroadPhaseLayerInterface {
  uint GetNumBroadPhaseLayers() const override { return 2; }
  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override {
    return BroadPhaseLayer(l);
  }
  const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override {
    return "probe";
  }
};
struct BV : ObjectVsBroadPhaseLayerFilter {
  bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
    return a != 0 || b.GetValue() != 0;
  }
};
struct LP : ObjectLayerPairFilter {
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
    return a != 0 || b != 0;
  }
};
struct CL : ContactListener {
  int adds = 0;
  std::set<std::pair<uint64, uint64>> pairs;
  void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &,
                          ContactSettings &) override {
    pairs.emplace(std::min(a.GetUserData(), b.GetUserData()),
                  std::max(a.GetUserData(), b.GetUserData()));
  }
  void OnContactAdded(const Body &a, const Body &b, const ContactManifold &,
                      ContactSettings &) override {
    ++adds;
    pairs.emplace(std::min(a.GetUserData(), b.GetUserData()),
                  std::max(a.GetUserData(), b.GetUserData()));
  }
};
RefConst<Shape> box(Vec3 h, float mass) {
  BoxShapeSettings s(h, 0.01f);
  s.mDensity = mass / (8 * h.GetX() * h.GetY() * h.GetZ());
  return s.Create().Get();
}
double ang(const Body &b) {
  Vec3 x = b.GetRotation() * Vec3::sAxisX();
  return std::atan2(x.GetY(), x.GetX());
}
RVec3 wp(const Body &b, Vec3 p) { return b.GetWorldTransform() * p; }
struct H {
  Ref<HingeConstraint> c;
  Body *a, *b;
  Vec3 la, lb;
  RVec3 fixed;
};
struct Fixture {
  BP bp;
  BV bv;
  LP lp;
  CL cl;
  Ref<GroupFilterTable> groups = new GroupFilterTable(256);
  PhysicsSystem sys;
  TempAllocatorImpl temp{32 * 1024 * 1024};
  JobSystemSingleThreaded jobs{1024};
  std::vector<Body *> bs;
  std::vector<H> hs;
  std::vector<std::string> names;
  Fixture(unsigned velocitySteps = 10, unsigned positionSteps = 2) {
    sys.Init(256, 0, 512, 2048, bp, bv, lp);
    sys.SetGravity(Vec3(0, -9.81, 0));
    sys.SetContactListener(&cl);
    auto s = sys.GetPhysicsSettings();
    s.mNumVelocitySteps = velocitySteps;
    s.mNumPositionSteps = positionSteps;
    sys.SetPhysicsSettings(s);
  }
  Body *body(RefConst<Shape> s, RVec3 p, Quat q, bool moving = true,
             std::string name = "unnamed") {
    BodyCreationSettings c(s, p, q,
                           moving ? EMotionType::Dynamic : EMotionType::Static,
                           moving ? 1 : 0);
    c.mCollisionGroup = CollisionGroup(groups, 1, bs.size());
    c.mUserData = bs.size() + 1;
    if (moving)
      c.mMotionQuality = EMotionQuality::LinearCast;
    c.mLinearDamping = 0;
    c.mAngularDamping = 0;
    c.mAllowSleeping = false;
    c.mRestitution = 0;
    c.mFriction = .8;
    Body *b = sys.GetBodyInterface().CreateBody(c);
    sys.GetBodyInterface().AddBody(
        b->GetID(), moving ? EActivation::Activate : EActivation::DontActivate);
    bs.push_back(b);
    names.push_back(name);
    return b;
  }
  H &hinge(Body *a, Body *b, RVec3 p, double f = 0,
           Vec3 axis = Vec3::sAxisZ()) {
    H h;
    h.a = a;
    h.b = b;
    h.fixed = p;
    h.la = a ? Vec3(a->GetWorldTransform().InversedRotationTranslation() * p)
             : Vec3::sZero();
    h.lb = Vec3(b->GetWorldTransform().InversedRotationTranslation() * p);
    HingeConstraintSettings s;
    s.mPoint1 = s.mPoint2 = p;
    s.mHingeAxis1 = s.mHingeAxis2 = axis;
    s.mNormalAxis1 = s.mNormalAxis2 =
        axis == Vec3::sAxisX() ? Vec3::sAxisY() : Vec3::sAxisX();
    if (a) {
      auto ai = std::find(bs.begin(), bs.end(), a) - bs.begin();
      auto bi = std::find(bs.begin(), bs.end(), b) - bs.begin();
      groups->DisableCollision(ai, bi);
    }
    s.mMaxFrictionTorque = f;
    h.c = static_cast<HingeConstraint *>(
        s.Create(a ? *a : Body::sFixedToWorld, *b));
    sys.AddConstraint(h.c);
    hs.push_back(h);
    return hs.back();
  }
  double drift() {
    double r = 0;
    for (auto &h : hs)
      r = std::max(
          r,
          double(((h.a ? wp(*h.a, h.la) : h.fixed) - wp(*h.b, h.lb)).Length()));
    return r;
  }
  double pe() {
    double e = 0;
    for (auto *b : bs)
      if (b->IsDynamic())
        e += b->GetShape()->GetMassProperties().mMass * 9.81 *
             b->GetCenterOfMassPosition().GetY();
    return e;
  }
  double ke() {
    double e = 0;
    for (auto *b : bs)
      if (b->IsDynamic()) {
        auto *mp = b->GetMotionProperties();
        Vec3 w = mp->GetInertiaRotation().Conjugated() *
                 (b->GetRotation().Conjugated() * b->GetAngularVelocity());
        Vec3 ii = mp->GetInverseInertiaDiagonal();
        e += .5 * b->GetLinearVelocity().LengthSq() / mp->GetInverseMass();
        e += .5 * (w.GetX() * w.GetX() / ii.GetX() +
                   w.GetY() * w.GetY() / ii.GetY() +
                   w.GetZ() * w.GetZ() / ii.GetZ());
      }
    return e;
  }
  ~Fixture() {
    for (auto &h : hs)
      sys.RemoveConstraint(h.c);
    hs.clear();
    for (auto *b : bs) {
      sys.GetBodyInterface().RemoveBody(b->GetID());
      sys.GetBodyInterface().DestroyBody(b->GetID());
    }
  }
};
struct Input {
  Body *handle;
  Ref<TwoBodyConstraint> rope;
  Vec3 direction = Vec3(0, 0, -1), point = Vec3::sZero();
  double startZ, begin, end, force, work = 0, stroke = 0;
};
Input make_input(Fixture &f, Body *prop, Vec3 attach, RVec3 handleAt,
                 RVec3 pulley1, RVec3 pulley2, double ratio, double begin,
                 double force, const std::string &name) {
  Input out;
  out.handle = f.body(box(Vec3(.15, .15, .15), 3), handleAt, Quat::sIdentity(),
                      true, name + "_handle");
  f.sys.GetBodyInterface().SetFriction(out.handle->GetID(), .1);
  f.body(box(Vec3(.4, .15, .7), 100), handleAt + RVec3(0, -.3, 0),
         Quat::sIdentity(), false, name + "_handle_table");
  f.body(box(Vec3(.4, .5, .15), 100), handleAt + RVec3(0, -.25, -.59),
         Quat::sIdentity(), false, name + "_handle_stop");
  PulleyConstraintSettings rs;
  rs.mBodyPoint1 = wp(*prop, attach);
  rs.mBodyPoint2 = handleAt;
  rs.mFixedPoint1 = pulley1;
  rs.mFixedPoint2 = pulley2;
  rs.mMinLength = 0;
  rs.mMaxLength = -1;
  rs.mRatio = ratio;
  out.rope = static_cast<PulleyConstraint *>(rs.Create(*prop, *out.handle));
  f.sys.AddConstraint(out.rope);
  out.startZ = Vec3(handleAt).Dot(out.direction);
  out.begin = begin;
  out.end = begin + 2;
  out.force = force;
  return out;
}
double pitchX(const Body &b) {
  Vec3 y = b.GetRotation() * Vec3::sAxisY();
  return std::atan2(y.GetZ(), y.GetY());
}
int main(int argc, char **argv) {
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory();
  RegisterTypes();
  {
    double hz = argc > 1 ? atof(argv[1]) : 90, dt = 1 / hz;
    int mode = argc > 2 ? atoi(argv[2]) : 0;
    const double rackRaise = argc > 4 ? atof(argv[4]) : .5;
    const int pipeCount = mode == 3 ? 0 : 20;
    Fixture f(argc > 5 ? atoi(argv[5]) : 10, argc > 6 ? atoi(argv[6]) : 2);
    const double pi = std::acos(-1.), alpha = 5 * pi / 180,
                 tailL = std::hypot(5, 2.8);
    f.body(box(Vec3(30, .2, 40), 1000), RVec3(6, -.2, -95), Quat::sIdentity(),
           false, "ground");
    // Actual open pan and outboard four-bar; no preloaded ballast body.
    const RVec3 pivot(6, .6, -90), mainPivot(3.4, .6, -90),
        auxPivot(8.6, 1.8, -90), panPin(3.4, 3.4, -85), auxPin(8.6, 4.6, -85),
        panOrigin(6, 2.4, -85);
    const Quat tailRot = Quat::sRotation(Vec3::sAxisX(), -std::atan2(2.8, 5));
    StaticCompoundShapeSettings bs;
    bs.AddShape(Vec3(0, -.2, -10), Quat::sIdentity(),
                box(Vec3(1.5, .2, 10), 8500));
    bs.AddShape(Vec3(-2.6, 1.4, 2.5), tailRot,
                box(Vec3(.15, .15, tailL / 2), 400));
    bs.AddShape(Vec3(-1.3, 0, 0), Quat::sIdentity(),
                box(Vec3(1.45, .15, .15), 50));
    // A cross-foot connects the outboard arm to the prop's roller-bearing shoe.
    const double propX = 2.7, propOffset = .04, propL = 2.4,
                 propPhi = std::asin(propOffset / propL),
                 shoeBottom = .4 + propL * std::cos(propPhi) + .3;
    bs.AddShape(Vec3(propX + .04 - 6, shoeBottom + .15 - .6, 5),
                Quat::sIdentity(), box(Vec3(.15, .15, .6), 40));
    bs.AddShape(Vec3((propX + .04 + 3.4) / 2 - 6, shoeBottom + .15 - .6, 5),
                Quat::sIdentity(), box(Vec3(.38, .15, .15), 10));
    Body *beam =
        f.body(bs.Create().Get(), pivot, Quat::sIdentity(), true, "bridge");
    Body *aux = f.body(box(Vec3(.15, .15, tailL / 2), 500),
                       RVec3(8.6, 3.2, -87.5), tailRot, true, "aux_arm");
    StaticCompoundShapeSettings ps;
    ps.AddShape(Vec3(0, -.05, 0), Quat::sRotation(Vec3::sAxisX(), -3 * pi / 180),
                box(Vec3(2.3, .05, 2.25), 385));
    ps.AddShape(Vec3(0, .65, -2.4), Quat::sIdentity(),
                box(Vec3(2.3, .65, .15), 100));
    for (double x : {-2.45, 2.45})
      ps.AddShape(Vec3(x, .65, 0), Quat::sIdentity(),
                  box(Vec3(.15, .65, 2.25), 100));
    ps.AddShape(Vec3(0, 2.3, 0), Quat::sIdentity(),
                box(Vec3(2.75, .15, .15), 100));
    ps.AddShape(Vec3(-2.6, 1.25, 0), Quat::sIdentity(),
                box(Vec3(.15, 1.2, .15), 25));
    ps.AddShape(Vec3(2.6, 1.25, 0), Quat::sIdentity(),
                box(Vec3(.15, 1.2, .15), 40));
    ps.AddShape(Vec3(0, .27, 2.3), Quat::sIdentity(),
                box(Vec3(2.3, .15, .05), 100));
    ps.AddShape(Vec3(0, -.125, 0), Quat::sIdentity(), box(Vec3(.75, .025, .75), 50));
    Body *pan =
        f.body(ps.Create().Get(), panOrigin, Quat::sIdentity(), true, "pan");
    f.hinge(nullptr, beam, mainPivot, 3000, Vec3::sAxisX());
    f.hinge(nullptr, aux, auxPivot, 0, Vec3::sAxisX());
    f.hinge(beam, pan, panPin, 0, Vec3::sAxisX());
    f.hinge(aux, pan, auxPin, 0, Vec3::sAxisX());
    f.body(box(Vec3(1.6, .15, .5), 1000), RVec3(6, .03, -109),
           Quat::sIdentity(), false, "lower_deck_seat");
    // Physical bridge holding prop and independent reachable trip handle.
    const RVec3 bp(propX, .4, -85);
    StaticCompoundShapeSettings bps;
    bps.AddShape(Vec3(0, 1.05, 0), Quat::sIdentity(),
                 box(Vec3(.15, 1.05, .15), 150));
    Body *bridgeProp =
        f.body(bps.Create().Get(), bp,
               Quat::sRotation(Vec3::sAxisZ(), -propPhi), true, "bridge_prop");
    f.hinge(nullptr, bridgeProp, bp, 15, Vec3::sAxisZ());
    RVec3 bh = wp(*bridgeProp, Vec3(0, propL, 0));
    CylinderShapeSettings bcs(.3, .3, .01);
    bcs.mDensity = 50 / (pi * .3 * .3 * .6);
    Body *bridgeRoll =
        f.body(bcs.Create().Get(), bh, Quat::sRotation(Vec3::sAxisX(), pi / 2),
               true, "bridge_roller");
    f.hinge(bridgeProp, bridgeRoll, bh, 2, Vec3::sAxisZ());
    f.body(box(Vec3(.15, .15, .4), 100),
           wp(*bridgeProp, Vec3(0, .3, 0)) + RVec3(.3, 0, 0), Quat::sIdentity(),
           false, "bridge_prop_stop");
    Input bridgeInput;
    bridgeInput.handle = f.body(box(Vec3(.15, .15, .15), 3),
        RVec3(propX - 3, 1.25, -85), Quat::sIdentity(), true, "bridge_handle");
    f.sys.GetBodyInterface().SetFriction(bridgeInput.handle->GetID(), .1);
    f.body(box(Vec3(.7, .15, .4), 100), RVec3(propX - 3, .95, -85),
           Quat::sIdentity(), false, "bridge_handle_table");
    f.body(box(Vec3(.15, .5, .4), 100), RVec3(propX - 3.59, 1, -85),
           Quat::sIdentity(), false, "bridge_handle_stop");
    SliderConstraintSettings bridgeGuideSettings;
    bridgeGuideSettings.mPoint1 = bridgeGuideSettings.mPoint2 = bridgeInput.handle->GetPosition();
    bridgeGuideSettings.mSliderAxis1 = bridgeGuideSettings.mSliderAxis2 = Vec3(-1, 0, 0);
    bridgeGuideSettings.mNormalAxis1 = bridgeGuideSettings.mNormalAxis2 = Vec3::sAxisY();
    bridgeGuideSettings.mLimitsMin = 0;
    bridgeGuideSettings.mLimitsMax = .28;
    Ref<TwoBodyConstraint> bridgeGuide = bridgeGuideSettings.Create(Body::sFixedToWorld, *bridgeInput.handle);
    f.sys.AddConstraint(bridgeGuide);
    DistanceConstraintSettings bridgeCord;
    bridgeCord.mPoint1 = wp(*bridgeProp, Vec3(0, 2.3, 0));
    bridgeCord.mPoint2 = bridgeInput.handle->GetPosition();
    bridgeCord.mMinDistance = 0;
    bridgeCord.mMaxDistance = (bridgeCord.mPoint2 - bridgeCord.mPoint1).Length();
    bridgeInput.rope = bridgeCord.Create(*bridgeProp, *bridgeInput.handle);
    f.sys.AddConstraint(bridgeInput.rope);
    bridgeInput.direction = Vec3(-1, 0, 0);
    bridgeInput.startZ = -bridgeInput.handle->GetPosition().GetX();
    bridgeInput.begin = 14;
    bridgeInput.end = 16;
    bridgeInput.force = (mode == 1 || mode == 2) ? 0 : 150;
    // Rack and retained top-hinged gate, from the release-only contact fixture.
    const double rackL = 5, e = .04, pa = -std::asin(e / rackL), arm = 4.8;
    const RVec3 gatePivot(6, 4.4 + rackRaise, -82.3),
        rackPropBase(9.5, 2.4 + rackRaise, -82.75 - rackL * std::cos(pa));
    StaticCompoundShapeSettings gs;
    gs.AddShape(Vec3(0, -1.6, 0), Quat::sIdentity(),
                box(Vec3(2.2, .15, .15), 200));
    gs.AddShape(Vec3(0, 0, 0), Quat::sIdentity(),
                box(Vec3(3.65, .15, .15), 100));
    for (double x : {-2.05, 2.05})
      gs.AddShape(Vec3(x, -.8, 0), Quat::sIdentity(),
                  box(Vec3(.15, .8, .15), 50));
    gs.AddShape(Vec3(3.5, -1, 0), Quat::sIdentity(),
                box(Vec3(.15, 1, .15), 50));
    gs.AddShape(Vec3(3.5, -2, 0), Quat::sIdentity(),
                box(Vec3(.2, .15, .15), 40));
    gs.AddShape(Vec3(0, .6, 0), Quat::sIdentity(), box(Vec3(.15, .6, .15), 20));
    gs.AddShape(Vec3(0, 1.2, 0), Quat::sIdentity(), box(Vec3(.5, .5, .5), 500));
    Body *gate = f.body(gs.Create().Get(), gatePivot, Quat::sIdentity(), true,
                        "rack_gate");
    f.hinge(nullptr, gate, gatePivot, 30, Vec3::sAxisX());
    f.body(box(Vec3(.3, .15, .3), 1000), RVec3(9.5, 5.08 + rackRaise, -83.1),
           Quat::sIdentity(), false, "rack_gate_open_stop");
    StaticCompoundShapeSettings rps;
    rps.AddShape(Vec3(0, 0, rackL / 2 - .15), Quat::sIdentity(),
                 box(Vec3(.15, .15, rackL / 2 - .15), 200));
    rps.AddShape(Vec3(0, .2, 4.4), Quat::sIdentity(), box(Vec3(.15, .35, .15), 12));
    rps.AddShape(Vec3(0, .4, 4.65), Quat::sIdentity(), box(Vec3(.15, .15, .35), 12));
    rps.AddShape(Vec3(.275, .4, 4.8), Quat::sIdentity(), box(Vec3(.425, .15, .15), 16));
    Body *rackProp =
        f.body(rps.Create().Get(), rackPropBase,
               Quat::sRotation(Vec3::sAxisY(), pa), true, "rack_prop");
    f.hinge(nullptr, rackProp, rackPropBase, 30, Vec3::sAxisY());
    RVec3 rh = wp(*rackProp, Vec3(0, 0, rackL));
    CylinderShapeSettings rcs(.2, .3, .01);
    rcs.mDensity = 50 / (pi * .3 * .3 * .4);
    Body *rackRoll =
        f.body(rcs.Create().Get(), rh, Quat::sIdentity(), true, "rack_roller");
    f.hinge(rackProp, rackRoll, rh, 5, Vec3::sAxisY());
    f.body(box(Vec3(.15, .3, .4), 100),
           wp(*rackProp, Vec3(0, 0, 2.5)) + RVec3(-.3, 0, 0), Quat::sIdentity(),
           false, "rack_prop_stop");
    const double rackZ = -79.6,
                 rackTop = 2.4 + rackRaise + (rackZ + 82.7) * std::tan(alpha);
    f.body(box(Vec3(2.25, .025, 3.1), 1000),
           RVec3(6, rackTop - .025 / std::cos(alpha), rackZ),
           Quat::sRotation(Vec3::sAxisX(), -alpha), false, "rack_floor");
    f.body(box(Vec3(2.25, .125, 2.9), 1000),
           RVec3(6, rackTop + .2 * std::tan(alpha) - .175 / std::cos(alpha), rackZ + .2),
           Quat::sRotation(Vec3::sAxisX(), -alpha), false, "rack_underframe");
    for (double x : {3.5, 8.5})
      f.body(box(Vec3(.15, .7, 2.75), 100), RVec3(x, 3.1 + rackRaise, -79.2),
             Quat::sIdentity(), false, "rack_wall");
    std::vector<Body *> pipes;
    for (int index = 0; index < pipeCount; ++index) {
      int lane = index / 5, row = index % 5;
      double z = -81.73 + row * .805 / std::cos(alpha),
             y = 2.4 + rackRaise + (z + 82.7) * std::tan(alpha) + .4 / std::cos(alpha);
      CylinderShapeSettings pc(.5, .4, .005);
      pc.mDensity = 800 / (pi * .4 * .4);
      Body *p = f.body(pc.Create().Get(), RVec3(4.35 + lane * 1.1, y, z),
                       Quat::sRotation(Vec3::sAxisZ(), pi / 2), true,
                       "pipe_" + std::to_string(index));
      pipes.push_back(p);
      double ri2 = .16 - 800 / (7850 * pi), ia = .5 * 800 * (.16 + ri2),
             it = 800 * (3 * (.16 + ri2) + 1) / 12;
      MassProperties mp;
      mp.mMass = 800;
      mp.mInertia = Mat44::sScale(Vec3(it, ia, it));
      p->GetMotionProperties()->SetMassProperties(EAllowedDOFs::All, mp);
    }
    // A visible 3:1 lever supplies the rack release advantage. Its mass and
    // inertia are solved directly; the output cord is tension-only.
    const RVec3 releasePivot(12, 2.4 + rackRaise, -81.95);
    StaticCompoundShapeSettings releaseParts;
    releaseParts.AddShape(Vec3(0, 0, 1), Quat::sIdentity(), box(Vec3(.15, .15, 2.15), 100));
    releaseParts.AddShape(Vec3(0, -.575 - rackRaise / 2, 3), Quat::sIdentity(),
                         box(Vec3(.15, .575 + rackRaise / 2, .15), 20));
    Body *releaseLever = f.body(releaseParts.Create().Get(), releasePivot,
                                Quat::sIdentity(), true, "rack_release_lever");
    f.hinge(nullptr, releaseLever, releasePivot, 15, Vec3::sAxisY());
    f.body(box(Vec3(.15, .65, .4), 100), RVec3(11.41, 1.825, -78.95),
           Quat::sIdentity(), false, "rack_lever_stop");
    DistanceConstraintSettings releaseCord;
    releaseCord.mPoint1 = wp(*rackProp, Vec3(.55, .4, arm));
    releaseCord.mPoint2 = wp(*releaseLever, Vec3(0, 0, -1));
    releaseCord.mMinDistance = 0;
    releaseCord.mMaxDistance = (releaseCord.mPoint2 - releaseCord.mPoint1).Length();
    Input rackInput;
    rackInput.handle = releaseLever;
    rackInput.rope = releaseCord.Create(*rackProp, *releaseLever);
    f.sys.AddConstraint(rackInput.rope);
    rackInput.direction = Vec3(-1, 0, 0);
    rackInput.point = Vec3(0, -1.15 - rackRaise, 3);
    rackInput.startZ = Vec3(wp(*releaseLever, rackInput.point)).Dot(rackInput.direction);
    rackInput.begin = 3;
    rackInput.end = 5;
    rackInput.force = mode == 1 ? 0 : 150;
    const double target = std::asin(7.4 / 20),
                 energy = 9.81 * (2500 * std::sin(target) +
                                  47300 * (1 - std::cos(target))),
                 bedTop = .6 - 5 * std::sin(target) + 2.8 * std::cos(target) -
                          1.15 + (energy - 3000 * target) / 90000,
                 k = 15e6, fy = 90000;
    f.body(box(Vec3(2.3, .15, 2.25), 1000), RVec3(6, bedTop - .65 - .15, -84.5),
           Quat::sIdentity(), false, "receiver_ultimate_floor");
    std::ofstream csv(argc > 3 ? argv[3] : "full_mechanism.csv");
    csv.precision(12);
    csv << "t,bridge_q,tip_y,pan_y,pan_pitch,hinge_drift,pipes_inside,pipes_"
           "crossed,gate_q,bridge_prop_angle,rack_hand_work,bridge_hand_work,"
           "plastic_work,penetration,plastic_front,ke,pe\n";
    double maxDrift = 0, maxPanPitch = 0, maxTip = .6, panAtRelease = 0,
           bridgeDead = -1, allLoaded = -1, plastic = 0, plasticWork = 0,
           maxPen = 0, hingeWork = 0, energyInitial = f.pe();
    int retainedMinAfter = 20;
    for (int i = 0; i < int(45 * hz); ++i) {
      double t = i * dt;
      Input *inputs[] = {&rackInput, &bridgeInput};
      RVec3 oldHandle[2];
      bool pulling[2];
      for (int n = 0; n < 2; ++n) {
        auto &in = *inputs[n];
        oldHandle[n] = wp(*in.handle, in.point);
        pulling[n] =
            t >= in.begin && t < in.end && Vec3(oldHandle[n]).Dot(in.direction) - in.startZ < .3;
        if (pulling[n])
          f.sys.GetBodyInterface().AddForce(in.handle->GetID(),
                                            in.direction * in.force, oldHandle[n]);
      }
      double pen = std::max(0., bedTop - wp(*pan, Vec3(0, -.15, 0)).GetY()),
             nf = std::max(plastic, pen - fy / k);
      plasticWork += fy * (nf - plastic);
      plastic = nf;
      double normal = k * std::max(0., pen - plastic);
      if (normal > 0)
        f.sys.GetBodyInterface().AddForce(pan->GetID(), Vec3(0, normal, 0),
                                          wp(*pan, Vec3(0, -.15, 0)));
      std::vector<double> oldAngles;
      for (auto &h : f.hs)
        oldAngles.push_back(h.c->GetCurrentAngle());
      f.cl.pairs.clear();
      f.sys.Update(dt, 1, &f.temp, &f.jobs);
      for (int n = 0; n < 2; ++n) {
        auto &in = *inputs[n];
        auto now = wp(*in.handle, in.point);
        if (pulling[n])
          in.work += in.force * Vec3(now - oldHandle[n]).Dot(in.direction);
        in.stroke = std::max(in.stroke, double(Vec3(now).Dot(in.direction)) - in.startZ);
      }
      for (size_t n = 0; n < f.hs.size(); ++n)
        hingeWork +=
            std::abs(f.hs[n].c->GetTotalLambdaMotor() / dt *
                     std::remainder(f.hs[n].c->GetCurrentAngle() - oldAngles[n],
                                    2 * pi));
      int inside = 0, crossed = 0;
      for (auto *p : pipes) {
        Vec3 local =
            Vec3(pan->GetWorldTransform().InversedRotationTranslation() *
                 p->GetCenterOfMassPosition());
        if (local.GetX() > -2.3 && local.GetX() < 2.3 && local.GetZ() > -2.25 &&
            local.GetZ() < 2.25 && local.GetY() > .0 && local.GetY() < 1.6)
          ++inside;
        if (p->GetCenterOfMassPosition().GetZ() < -82.7)
          ++crossed;
      }
      if (inside == 20 && allLoaded < 0)
        allLoaded = t + dt;
      if (t >= 14)
        retainedMinAfter = std::min(retainedMinAfter, inside);
      double bq = pitchX(*beam), pq = pitchX(*pan),
             propAngle = std::atan2(
                 (bridgeProp->GetRotation() * Vec3::sAxisY()).GetX(),
                 (bridgeProp->GetRotation() * Vec3::sAxisY()).GetY());
      if (propAngle < 0 && bridgeDead < 0)
        bridgeDead = t + dt;
      maxDrift = std::max(maxDrift, f.drift());
      maxPanPitch = std::max(maxPanPitch, std::abs(pq));
      maxTip = std::max(maxTip, double(wp(*beam, Vec3(0, 0, -20)).GetY()));
      maxPen = std::max(maxPen, pen);
      if (t < 14)
        panAtRelease = pan->GetPosition().GetY();
      if (i % std::max(1, int(hz / 90)) == 0)
        csv << t + dt << ',' << bq << ',' << wp(*beam, Vec3(0, 0, -20)).GetY()
            << ',' << pan->GetPosition().GetY() << ',' << pq << ',' << f.drift()
            << ',' << inside << ',' << crossed << ',' << pitchX(*gate) << ','
            << propAngle << ',' << rackInput.work << ',' << bridgeInput.work
            << ',' << plasticWork << ',' << pen << ',' << plastic << ','
            << f.ke() << ',' << f.pe() << '\n';
      if (i % int(hz) == 0) {
        printf("t=%.1f q=%.5f tip=%.3f pan=%.3f inside=%d crossed=%d gate=%.3f "
               "prop=%.4f work=%.2f/%.2f\n",
               t, bq, wp(*beam, Vec3(0, 0, -20)).GetY(),
               pan->GetPosition().GetY(), inside, crossed, pitchX(*gate),
               propAngle, rackInput.work, bridgeInput.work);
        for (auto pair : f.cl.pairs)
          if ((pair.first == pan->GetUserData() ||
               pair.second == pan->GetUserData()) &&
              (f.names[pair.first - 1].rfind("pipe", 0) != 0 &&
               f.names[pair.second - 1].rfind("pipe", 0) != 0))
            printf("  pan contact: %s / %s\n", f.names[pair.first - 1].c_str(),
                   f.names[pair.second - 1].c_str());
      }
    }
    printf("RESULT "
           "{\"hz\":%.0f,\"mode\":%d,\"all_loaded_s\":%.6f,\"bridge_deadcentre_"
           "s\":%.6f,\"pan_y_before_release\":%.6f,\"tip_end\":%.6f,\"tip_"
           "max\":%.6f,\"retained_min_after_release\":%d,\"hinge_drift_max\":%."
           "9f,\"pan_pitch_max\":%.9f,\"rack_work_J\":%.6f,\"bridge_work_J\":%."
           "6f,\"rack_stroke_m\":%.6f,\"bridge_stroke_m\":%.6f,\"plastic_work_"
           "J\":%.6f,\"hinge_friction_work_J\":%.6f,\"max_penetration\":%.6f,"
           "\"mechanical_release_J\":%.6f,\"kinetic_end_J\":%.6f}\n",
           hz, mode, allLoaded, bridgeDead, panAtRelease,
           wp(*beam, Vec3(0, 0, -20)).GetY(), maxTip, retainedMinAfter,
           maxDrift, maxPanPitch, rackInput.work, bridgeInput.work,
           rackInput.stroke, bridgeInput.stroke, plasticWork, hingeWork, maxPen,
           energyInitial - f.pe(), f.ke());
    for (auto *p : pipes) {
      auto v = p->GetPosition();
      printf("PIPE %s %.4f %.4f %.4f speed %.6f\n",
             f.names[p->GetUserData() - 1].c_str(), v.GetX(), v.GetY(),
             v.GetZ(), p->GetLinearVelocity().Length());
    }
    f.sys.RemoveConstraint(rackInput.rope);
    f.sys.RemoveConstraint(bridgeInput.rope);
    f.sys.RemoveConstraint(bridgeGuide);
  }
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
}
