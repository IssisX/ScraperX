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
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
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
  void OnContactAdded(const Body &, const Body &, const ContactManifold &,
                      ContactSettings &) override {
    ++adds;
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
  Fixture() {
    sys.Init(256, 0, 512, 2048, bp, bv, lp);
    sys.SetGravity(Vec3(0, -9.81, 0));
    sys.SetContactListener(&cl);
    auto s = sys.GetPhysicsSettings();
    s.mNumVelocitySteps = 20;
    s.mNumPositionSteps = 4;
    sys.SetPhysicsSettings(s);
  }
  Body *body(RefConst<Shape> s, RVec3 p, Quat q, bool moving = true) {
    BodyCreationSettings c(s, p, q,
                           moving ? EMotionType::Dynamic : EMotionType::Static,
                           moving ? 1 : 0);
    c.mCollisionGroup = CollisionGroup(groups, 1, bs.size());
    c.mLinearDamping = 0;
    c.mAngularDamping = 0;
    c.mAllowSleeping = false;
    c.mRestitution = 0;
    c.mFriction = .8;
    Body *b = sys.GetBodyInterface().CreateBody(c);
    sys.GetBodyInterface().AddBody(
        b->GetID(), moving ? EActivation::Activate : EActivation::DontActivate);
    bs.push_back(b);
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
        auto mp = b->GetShape()->GetMassProperties();
        Vec3 w = b->GetRotation().Conjugated() * b->GetAngularVelocity();
        e += .5 * mp.mMass * b->GetLinearVelocity().LengthSq() +
             .5 * w.Dot(mp.mInertia.Multiply3x3(w));
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
int main(int argc, char **argv) {
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory();
  RegisterTypes();
  {
    const double hz = argc > 1 ? atof(argv[1]) : 90,
                 e = argc > 2 ? atof(argv[2]) : .04,
                 force = argc > 3 ? atof(argv[3]) : 150, dt = 1 / hz, L = 2.4,
                 arm = 2.3, phi = std::asin(e / L), pi = std::acos(-1.);
    Fixture f;
    RVec3 base(0, .4, 0);
    StaticCompoundShapeSettings ps;
    ps.AddShape(Vec3(0, L / 2 - .15, 0), Quat::sIdentity(),
                box(Vec3(.15, L / 2 - .15, .15), 150));
    Body *prop =
        f.body(ps.Create().Get(), base, Quat::sRotation(Vec3::sAxisZ(), -phi));
    f.hinge(nullptr, prop, base, 15, Vec3::sAxisZ());
    RVec3 headAt = wp(*prop, Vec3(0, L, 0));
    CylinderShapeSettings cs(.3, .3, .01);
    cs.mDensity = 50 / (pi * .3 * .3 * .6);
    Body *head = f.body(cs.Create().Get(), headAt,
                        Quat::sRotation(Vec3::sAxisX(), pi / 2));
    f.hinge(prop, head, headAt, 2, Vec3::sAxisZ());
    RVec3 stopAt = wp(*prop, Vec3(0, .3, 0));
    stopAt += RVec3(.3, 0, 0);
    f.body(box(Vec3(.15, .15, .4), 100), stopAt, Quat::sIdentity(), false);
    // Isolated laboratory load: a 500 kg guided shoe supplies exactly 4.905 kN.
    // It does not reproduce the full bridge's reflected inertia.
    const RVec3 shoeAt(0, headAt.GetY() + .45, 0);
    Body *shoe =
        f.body(box(Vec3(.15, .15, .6), 500), shoeAt, Quat::sIdentity());
    SliderConstraintSettings ss;
    ss.mPoint1 = ss.mPoint2 = shoeAt;
    ss.mSliderAxis1 = ss.mSliderAxis2 = Vec3::sAxisY();
    ss.mNormalAxis1 = ss.mNormalAxis2 = Vec3::sAxisX();
    ss.mLimitsMin = -3.3;
    ss.mLimitsMax = .1;
    Ref<SliderConstraint> slider =
        static_cast<SliderConstraint *>(ss.Create(Body::sFixedToWorld, *shoe));
    f.sys.AddConstraint(slider);
    f.body(box(Vec3(4, .15, 3), 1000), RVec3(0, -.15, 0), Quat::sIdentity(),
           false);
    Body *handle = f.body(box(Vec3(.15, .15, .15), 3), RVec3(-3, 1.25, .5),
                          Quat::sIdentity());
    f.sys.GetBodyInterface().SetFriction(handle->GetID(), .1f);
    f.body(box(Vec3(.4, .15, .7), 100), RVec3(-3, .95, .5), Quat::sIdentity(),
           false);
    f.body(box(Vec3(.4, .5, .15), 100), RVec3(-3, 1, -.09), Quat::sIdentity(),
           false);
    PulleyConstraintSettings rs;
    rs.mSpace = EConstraintSpace::WorldSpace;
    rs.mBodyPoint1 = wp(*prop, Vec3(0, arm, 0));
    rs.mBodyPoint2 = handle->GetPosition();
    rs.mFixedPoint1 = RVec3(-3, 2.7, 0);
    rs.mFixedPoint2 = RVec3(-3, 1.25, 1);
    rs.mMinLength = 0;
    rs.mMaxLength = -1;
    Ref<PulleyConstraint> rope =
        static_cast<PulleyConstraint *>(rs.Create(*prop, *handle));
    f.sys.AddConstraint(rope);
    double releaseDrift = 0;
    double work = 0, maxStroke = 0, maxDrift = 0, armedMotion = 0, dead = -1,
           deadStroke = -1, deadWork = -1, maxRise = 0, minShoe = shoeAt.GetY();
    for (int i = 0; i < int(8 * hz); ++i) {
      double t = i * dt;
      auto old = handle->GetPosition();
      double stroke = .5 - old.GetZ();
      if (t >= 3 && stroke < .3)
        f.sys.GetBodyInterface().AddForce(handle->GetID(), Vec3(0, 0, -force));
      f.sys.Update(dt, 1, &f.temp, &f.jobs);
      auto now = handle->GetPosition();
      if (t >= 3 && stroke < .3)
        work += force * (old.GetZ() - now.GetZ());
      Vec3 py = prop->GetRotation() * Vec3::sAxisY();
      double p = std::atan2(py.GetX(), py.GetY());
      if (t < 3)
        armedMotion = std::max(armedMotion, std::abs(p - phi));
      if (dead < 0)
        releaseDrift = std::max(releaseDrift, f.drift());
      if (p < 0 && dead < 0) {
        dead = t + dt;
        deadStroke = .5 - now.GetZ();
        deadWork = work;
      }
      maxStroke = std::max(maxStroke, .5 - double(now.GetZ()));
      maxDrift = std::max(maxDrift, f.drift());
      maxRise =
          std::max(maxRise, double(shoe->GetPosition().GetY() - shoeAt.GetY()));
      minShoe = std::min(minShoe, double(shoe->GetPosition().GetY()));
    }
    printf("{\"hz\":%.0f,\"offset_m\":%.3f,\"force_N\":%.1f,\"armed_prop_"
           "motion_rad\":%.9f,\"deadcentre_s\":%.6f,\"deadcentre_stroke_m\":%."
           "9f,\"deadcentre_work_J\":%.6f,\"hand_work_J\":%.6f,\"max_stroke_"
           "m\":%.9f,\"shoe_max_rise_m\":%.9f,\"shoe_min_y_m\":%.6f,\"hinge_"
           "drift_m\":%.9f}\n",
           hz, e, force, armedMotion, dead, deadStroke, deadWork, work,
           maxStroke, maxRise, minShoe, maxDrift);
    printf("{\"release_hinge_drift_m\":%.9f}\n", releaseDrift);
    f.sys.RemoveConstraint(rope);
    f.sys.RemoveConstraint(slider);
  }
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
}
