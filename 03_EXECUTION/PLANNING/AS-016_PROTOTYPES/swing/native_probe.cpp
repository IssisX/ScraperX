// Isolated AS-016 dynamics fixture; this is not a game or pipe-loading proof.
// Coordinates: X along deck, Y up, hinge axis Z. Dynamic/dynamic collisions are
// intentionally disabled. The pan starts with its full 17 t load. Rider cases
// are attached point-load proxies, not the production character controller.
// The only dissipative laws are native hinge dry friction and the finite,
// unilateral elastic/plastic receiver. No damping, pose changes, or velocity
// resets are applied after initial construction.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
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
    return a != ObjectLayer(b.GetValue());
  }
};
struct LP : ObjectLayerPairFilter {
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
    return a != b;
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
  PhysicsSystem sys;
  TempAllocatorImpl temp{32 * 1024 * 1024};
  JobSystemSingleThreaded jobs{1024};
  std::vector<Body *> bs;
  std::vector<H> hs;
  Fixture(int vel = 10, int pos = 2) {
    sys.Init(64, 0, 64, 128, bp, bv, lp);
    sys.SetGravity(Vec3(0, -9.81, 0));
    sys.SetContactListener(&cl);
    auto s = sys.GetPhysicsSettings();
    s.mNumVelocitySteps = vel;
    s.mNumPositionSteps = pos;
    sys.SetPhysicsSettings(s);
  }
  Body *body(RefConst<Shape> s, RVec3 p, Quat q, bool moving = true) {
    BodyCreationSettings c(s, p, q,
                           moving ? EMotionType::Dynamic : EMotionType::Static,
                           moving ? 1 : 0);
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
  H &hinge(Body *a, Body *b, RVec3 p, double f = 0) {
    H h;
    h.a = a;
    h.b = b;
    h.fixed = p;
    h.la = a ? Vec3(a->GetWorldTransform().InversedRotationTranslation() * p)
             : Vec3::sZero();
    h.lb = Vec3(b->GetWorldTransform().InversedRotationTranslation() * p);
    HingeConstraintSettings s;
    s.mPoint1 = s.mPoint2 = p;
    s.mHingeAxis1 = s.mHingeAxis2 = Vec3::sAxisZ();
    s.mNormalAxis1 = s.mNormalAxis2 = Vec3::sAxisX();
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
    double hz = argc > 1 ? atof(argv[1]) : 90,
           fric = argc > 2 ? atof(argv[2]) : 3000,
           rm = argc > 3 ? atof(argv[3]) : 0, rr = argc > 4 ? atof(argv[4]) : 0,
           fy = argc > 5 ? atof(argv[5]) : 90000, dt = 1 / hz,
           k = argc > 7 ? atof(argv[7]) : 15000000, duration = 35,
           bedNominalYield = argc > 10 ? atof(argv[10]) : 90000;
    Fixture f(argc > 8 ? atoi(argv[8]) : 10, argc > 9 ? atoi(argv[9]) : 2);
    RVec3 P(0, .6, 0), P2(0, 1.8, 0), pin(-5, 3.4, 0), pin2(-5, 4.6, 0);
    double rodL = std::hypot(5, 2.8), rodA = std::atan2(2.8, -5);
    StaticCompoundShapeSettings main;
    main.AddShape(Vec3(10, -.2, 0), Quat::sIdentity(),
                  box(Vec3(10, .2, 1.5), 8500));
    main.AddShape(Vec3(-2.5, 1.4, 0), Quat::sRotation(Vec3::sAxisZ(), rodA),
                  box(Vec3(rodL / 2, .15, .15), 500));
    if (rm > 0)
      main.AddShape(Vec3(rr, 0, 0), Quat::sIdentity(),
                    box(Vec3(.05, .05, .05), rm));
    Body *beam = f.body(main.Create().Get(), P, Quat::sIdentity());
    Body *aux = f.body(box(Vec3(rodL / 2, .15, .15), 500), RVec3(-2.5, 3.2, 0),
                       Quat::sRotation(Vec3::sAxisZ(), rodA));
    Body *pan = f.body(box(Vec3(2, .3, 2), 17000), RVec3(-5, 2.7, 0),
                       Quat::sIdentity());
    f.hinge(nullptr, beam, P, fric);
    f.hinge(nullptr, aux, P2);
    f.hinge(beam, pan, pin);
    f.hinge(aux, pan, pin2);
    double target = std::asin(7.4 / 20),
           E = 9.81 *
               (2500 * std::sin(target) + 47300 * (1 - std::cos(target))),
           piny = .6 - 5 * std::sin(target) + 2.8 * std::cos(target),
           bedTop = piny - 1 + (E - 3000 * target) / bedNominalYield;
    Body *floor =
        f.body(box(Vec3(3, .15, 3), 1000), RVec3(-5, bedTop - .65 - .15, 0),
               Quat::sIdentity(), false);
    (void)floor;
    double e0 = f.pe(), front = 0, dissPlastic = 0, dissFric = 0, maxDrift = 0,
           maxTilt = 0, maxSpeed = 0, maxAccel = 0, maxTotalAccel = 0,
           maxPen = 0, maxForce = 0, qPrev = 0, wPrev = 0, firstContact = -1,
           firstTurn = -1, settled = -1, quiet = 0, lastForce = 0,
           maxResidual = 0, lastQMin = 1e9, lastQMax = -1e9;
    std::ofstream csv(argc > 6 ? argv[6] : "native_probe.csv");
    csv.precision(12);
    csv << "t,q,tip_y,omega,pan_tilt,hinge_drift,penetration,plastic_front,"
           "force,ke,pe,plastic_diss,friction_diss,spring,ledger_residual\n";
    for (int i = 0; i < int(duration * hz); ++i) {
      double t = i * dt;
      RVec3 striker = wp(*pan, Vec3(0, -.3, 0));
      double pen = std::max(0., bedTop - striker.GetY());
      if (pen > 0 && firstContact < 0)
        firstContact = t;
      double newFront = std::max(front, pen - fy / k);
      dissPlastic += fy * (newFront - front);
      front = newFront;
      double elast = std::max(0., pen - front), force = k * elast;
      f.sys.GetBodyInterface().AddForce(pan->GetID(), Vec3(0, force, 0),
                                        striker);
      double oldQ = ang(*beam);
      f.sys.Update(dt, 1, &f.temp, &f.jobs);
      double q = ang(*beam), w = beam->GetAngularVelocity().GetZ();
      dissFric += std::abs(f.hs[0].c->GetTotalLambdaMotor() / dt * (q - oldQ));
      // Account for the new material state at the same instant as body energy.
      pen = std::max(0., bedTop - wp(*pan, Vec3(0, -.3, 0)).GetY());
      newFront = std::max(front, pen - fy / k);
      dissPlastic += fy * (newFront - front);
      front = newFront;
      elast = std::max(0., pen - front);
      force = k * elast;
      double tilt = ang(*pan), dr = f.drift(), spring = .5 * k * elast * elast,
             ke = f.ke(), pe = f.pe(),
             res = e0 - pe - ke - dissPlastic - dissFric - spring;
      maxDrift = std::max(maxDrift, dr);
      maxTilt = std::max(maxTilt, std::abs(tilt));
      maxSpeed = std::max(maxSpeed, 20 * std::abs(w));
      if (i > 0) {
        maxAccel = std::max(maxAccel, 20 * std::abs(w - wPrev) / dt / 9.81);
        maxTotalAccel = std::max(
            maxTotalAccel, 20 * std::hypot((w - wPrev) / dt, w * w) / 9.81);
      }
      maxPen = std::max(maxPen, pen);
      maxForce = std::max(maxForce, force);
      if (firstContact >= 0 && wPrev > 0 && w <= 0 && firstTurn < 0)
        firstTurn = t + dt;
      if (firstContact >= 0 && std::abs(w) < .0001)
        quiet += dt;
      else
        quiet = 0;
      if (quiet > 1 && settled < 0)
        settled = t + dt;
      if (t > duration - 5) {
        lastQMin = std::min(lastQMin, q);
        lastQMax = std::max(lastQMax, q);
        maxResidual = std::max(maxResidual, std::abs(res));
      }
      if (i % std::max(1, int(hz / 90)) == 0)
        csv << t + dt << ',' << q << ',' << wp(*beam, Vec3(20, 0, 0)).GetY()
            << ',' << w << ',' << tilt << ',' << dr << ',' << pen << ','
            << front << ',' << force << ',' << ke << ',' << pe << ','
            << dissPlastic << ',' << dissFric << ',' << spring << ',' << res
            << '\n';
      qPrev = q;
      wPrev = w;
      lastForce = force;
    }
    printf(
        "{\"hz\":%.0f,\"friction\":%.0f,\"rider_mass\":%.0f,\"rider_r\":%."
        "0f,\"crush_yield\":%.0f,\"bed_top\":%.9f,\"q_end\":%.9f,\"tip_y\":%"
        ".9f,\"first_contact_s\":%.6f,\"first_turn_s\":%.6f,\"settled_s\":%."
        "6f,\"peak_tip_speed\":%.9f,\"peak_tangent_accel_g\":%.9f,\"pan_"
        "tilt_max_rad\":%.9f,\"hinge_drift_max_m\":%.9f,\"plastic_front_m\":"
        "%.9f,\"penetration_max_m\":%.9f,\"force_end_N\":%.4f,\"force_max_"
        "N\":%.4f,\"ke_end_J\":%.6f,\"energy_released_J\":%.6f,\"plastic_"
        "diss_J\":%.6f,\"friction_diss_J\":%.6f,\"late_ledger_residual_max_"
        "J\":%.6f,\"late_q_span\":%.9f,\"ultimate_floor_contacts\":%d,\"peak_"
        "total_accel_g\":%.9f,\"stiffness\":%.0f,\"bed_nominal_yield\":%.0f}\n",
        hz, fric, rm, rr, fy, bedTop, qPrev, wp(*beam, Vec3(20, 0, 0)).GetY(),
        firstContact, firstTurn, settled, maxSpeed, maxAccel, maxTilt, maxDrift,
        front, maxPen, lastForce, maxForce, f.ke(), e0 - f.pe(), dissPlastic,
        dissFric, maxResidual, lastQMax - lastQMin, f.cl.adds, maxTotalAccel, k,
        bedNominalYield);
  }
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
}
