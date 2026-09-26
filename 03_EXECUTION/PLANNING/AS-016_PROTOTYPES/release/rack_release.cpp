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
                 handForce = argc > 2 ? atof(argv[2]) : 150, dt = 1 / hz;
    Fixture f;
    const double pi = std::acos(-1.), alpha = 5 * pi / 180, L = 5, e = .04,
                 pa = -std::asin(e / L), arm = 4.8;
    const RVec3 pivot(6, 4.4, -82.3),
        propBase(9.5, 2.4, -82.75 - L * std::cos(pa));
    StaticCompoundShapeSettings gs;
    gs.AddShape(Vec3(0, -1.6, 0), Quat::sIdentity(),
                box(Vec3(2.2, .15, .15), 200));
    gs.AddShape(Vec3(0, 0, 0), Quat::sIdentity(),
                box(Vec3(3.65, .15, .15), 100));
    gs.AddShape(Vec3(-2.05, -.8, 0), Quat::sIdentity(),
                box(Vec3(.15, .8, .15), 50));
    gs.AddShape(Vec3(2.05, -.8, 0), Quat::sIdentity(),
                box(Vec3(.15, .8, .15), 50));
    gs.AddShape(Vec3(3.5, -1, 0), Quat::sIdentity(),
                box(Vec3(.15, 1, .15), 50));
    gs.AddShape(Vec3(3.5, -2, 0), Quat::sIdentity(),
                box(Vec3(.2, .15, .15), 40));
    gs.AddShape(Vec3(0, .6, 0), Quat::sIdentity(), box(Vec3(.15, .6, .15), 20));
    gs.AddShape(Vec3(0, 1.2, 0), Quat::sIdentity(), box(Vec3(.5, .5, .5), 500));
    Body *gate = f.body(gs.Create().Get(), pivot, Quat::sIdentity());
    f.hinge(nullptr, gate, pivot, 30, Vec3::sAxisX());
    f.body(box(Vec3(.3, .15, .3), 1000), RVec3(9.5, 5.08, -83.1),
           Quat::sIdentity(), false);
    StaticCompoundShapeSettings ps;
    ps.AddShape(Vec3(0, 0, L / 2 - .15), Quat::sIdentity(),
                box(Vec3(.15, .15, L / 2 - .15), 200));
    ps.AddShape(Vec3(0, 0, arm), Quat::sIdentity(), box(Vec3(.3, .3, .3), 40));
    Body *prop = f.body(ps.Create().Get(), propBase,
                        Quat::sRotation(Vec3::sAxisY(), pa));
    f.hinge(nullptr, prop, propBase, 30, Vec3::sAxisY());
    RVec3 headAt = wp(*prop, Vec3(0, 0, L));
    CylinderShapeSettings cs(.2, .3, .01);
    cs.mDensity = 50 / (pi * .3 * .3 * .4);
    Body *head = f.body(cs.Create().Get(), headAt, Quat::sIdentity());
    f.hinge(prop, head, headAt, 5, Vec3::sAxisY());
    // Broad visible stop holds the strut just beyond its dead centre.
    RVec3 stopAt = wp(*prop, Vec3(0, 0, 2.5));
    stopAt += RVec3(-.3, 0, 0);
    f.body(box(Vec3(.15, .3, .4), 100), stopAt, Quat::sIdentity(), false);
    // Five-degree rack. All twenty individual pipe sections begin upstream.
    const double rackZ = -79.6,
                 rackTop = 2.4 + (rackZ + 82.7) * std::tan(alpha);
    f.body(box(Vec3(2.35, .15, 3.1), 1000),
           RVec3(6, rackTop - .15 / std::cos(alpha), rackZ),
           Quat::sRotation(Vec3::sAxisX(), -alpha), false);
    for (double x : {3.5, 8.5})
      f.body(box(Vec3(.15, .7, 2.75), 100), RVec3(x, 3.1, -79.2),
             Quat::sIdentity(), false);
    std::vector<Body *> pipes;
    for (int lane = 0; lane < 4; ++lane)
      for (int row = 0; row < 5; ++row) {
        const double z = -81.73 + row * .805 / std::cos(alpha),
                     y = 2.4 + (z + 82.7) * std::tan(alpha) +
                         .4 / std::cos(alpha);
        CylinderShapeSettings pc(.5, .4, .005);
        pc.mDensity = 800 / (pi * .4 * .4);
        Body *p = f.body(pc.Create().Get(), RVec3(4.35 + lane * 1.1, y, z),
                         Quat::sRotation(Vec3::sAxisZ(), pi / 2));
        pipes.push_back(p);
        // Hollow-pipe inertia: OD .8 m, ID derived from 800 kg steel and 1 m
        // length.
        const double ri2 = .16 - 800 / (7850 * pi), ia = .5 * 800 * (.16 + ri2),
                     it = 800 * (3 * (.16 + ri2) + 1) / 12;
        MassProperties mp;
        mp.mMass = 800;
        mp.mInertia = Mat44::sScale(Vec3(it, ia, it));
        p->GetMotionProperties()->SetMassProperties(EAllowedDOFs::All, mp);
      }
    // Fixed receiver exists only to check gate clearance and collection, not
    // bridge dynamics.
    f.body(box(Vec3(2.3, .05, 2.25), 1000), RVec3(6, 2.35, -85),
           Quat::sIdentity(), false);
    f.body(box(Vec3(2.3, .65, .15), 100), RVec3(6, 3.05, -87.4),
           Quat::sIdentity(), false);
    for (double x : {3.55, 8.45})
      f.body(box(Vec3(.15, .65, 2.25), 100), RVec3(x, 3.05, -85),
             Quat::sIdentity(), false);
    // A short accessible handle loads the retainer through a real tension-only
    // trip rope.
    Body *handle = f.body(box(Vec3(.15, .15, .15), 3), RVec3(12, 1.25, -82),
                          Quat::sIdentity());
    f.sys.GetBodyInterface().SetFriction(handle->GetID(), .1f);
    f.body(box(Vec3(.4, .15, .7), 100), RVec3(12, .95, -82), Quat::sIdentity(),
           false);
    f.body(box(Vec3(.4, .5, .15), 100), RVec3(12, 1, -82.59), Quat::sIdentity(),
           false);
    PulleyConstraintSettings rs;
    rs.mSpace = EConstraintSpace::WorldSpace;
    rs.mBodyPoint1 = wp(*prop, Vec3(0, 0, arm));
    rs.mBodyPoint2 = handle->GetPosition();
    rs.mFixedPoint1 = RVec3(15, 2.4, -82.95);
    rs.mFixedPoint2 = RVec3(12, 1.25, -81);
    rs.mMinLength = 0;
    rs.mMaxLength = -1;
    rs.mRatio = 1.0f / 3.0f;
    Ref<PulleyConstraint> rope =
        static_cast<PulleyConstraint *>(rs.Create(*prop, *handle));
    f.sys.AddConstraint(rope);
    const double duration = 12;
    double work = 0, maxStroke = 0, maxDrift = 0, gateBefore = 0,
           propBefore = 0, breakTime = -1, releaseAt = 3, propMax = pa,
           minPipeZ = 1e9, maxHandleForce = 0;
    std::ofstream csv(argc > 3 ? argv[3] : "rack_release.csv");
    csv << "t,gate_rad,prop_rad,handle_stroke,hand_work,hinge_drift,pipe_"
           "crossed\n";
    for (int i = 0; i < int(duration * hz); ++i) {
      const double t = i * dt;
      auto old = handle->GetCenterOfMassPosition();
      double stroke = -(old.GetZ() + 82);
      if (t >= releaseAt && stroke < .3) {
        f.sys.GetBodyInterface().AddForce(handle->GetID(),
                                          Vec3(0, 0, -handForce));
        maxHandleForce = handForce;
      }
      f.sys.Update(dt, 1, &f.temp, &f.jobs);
      auto now = handle->GetCenterOfMassPosition();
      if (t >= releaseAt && stroke < .3)
        work += -handForce * (now.GetZ() - old.GetZ());
      Vec3 gy = gate->GetRotation() * Vec3::sAxisY(),
           pz = prop->GetRotation() * Vec3::sAxisZ();
      double gq = std::atan2(gy.GetZ(), gy.GetY()),
             pq = std::atan2(pz.GetX(), pz.GetZ());
      if (t < releaseAt) {
        gateBefore = std::max(gateBefore, std::abs(gq));
        propBefore = std::max(propBefore, std::abs(pq - pa));
      }
      if (pq > 0 && breakTime < 0)
        breakTime = t;
      propMax = std::max(propMax, pq);
      maxStroke = std::max(maxStroke, -double(now.GetZ() + 82));
      maxDrift = std::max(maxDrift, f.drift());
      int crossed = 0;
      for (auto *p : pipes) {
        minPipeZ = std::min(minPipeZ, double(p->GetPosition().GetZ()));
        if (p->GetPosition().GetZ() < -82.7)
          ++crossed;
      }
      if (i % std::max(1, int(hz / 90)) == 0)
        csv << t + dt << ',' << gq << ',' << pq << ',' << -(now.GetZ() + 82)
            << ',' << work << ',' << f.drift() << ',' << crossed << '\n';
    }
    int crossed = 0, inside = 0;
    double fastest = 0;
    for (auto *p : pipes) {
      auto p0 = p->GetPosition();
      if (p0.GetZ() < -82.7)
        ++crossed;
      if (p0.GetZ() < -82.75 && p0.GetZ() > -87.25 && p0.GetX() > 3.7 &&
          p0.GetX() < 8.3 && p0.GetY() > 2.35 && p0.GetY() < 4)
        ++inside;
      fastest = std::max(fastest, double(p->GetLinearVelocity().Length()));
    }
    printf("{\"hz\":%.0f,\"force_N\":%.1f,\"armed_gate_motion_rad\":%.9f,"
           "\"armed_prop_motion_rad\":%.9f,\"crossed_deadcentre_s\":%.6f,"
           "\"prop_max_rad\":%.9f,\"hand_work_J\":%.6f,\"handle_stroke_max_m\":"
           "%.6f,\"hinge_drift_max_m\":%.9f,\"pipe_crossed\":%d,\"pipe_in_"
           "receiver\":%d,\"pipe_fastest_end_mps\":%.6f}\n",
           hz, maxHandleForce, gateBefore, propBefore, breakTime, propMax, work,
           maxStroke, maxDrift, crossed, inside, fastest);
    for (auto *p : pipes) {
      auto p0 = p->GetPosition();
      if (p0.GetZ() >= -82.75)
        printf("remaining pipe xyz %.6f %.6f %.6f\n", p0.GetX(), p0.GetY(),
               p0.GetZ());
    }
    f.sys.RemoveConstraint(rope);
  }
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
}
