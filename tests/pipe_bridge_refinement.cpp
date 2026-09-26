#include <Jolt/Jolt.h>

#include "sim/pipe_bridge.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
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
// Step refinement of the exact production construction. No duplicated geometry.
// Finite 150 N hand forces; no player or pose/velocity setters in this fixture.
struct RefinementResult {
  double tip, front, work, input_work, loss, max_ke;
  double pan_pitch = 0, floor_gap = 100, post_min = 100, post_max = 0;
  unsigned pipes;
};
RefinementResult run(int hz) {
  BP bp;
  BV bv;
  LP lp;
  PhysicsSystem sys;
  sys.Init(256, 0, 512, 2048, bp, bv, lp);
  sys.SetGravity(Vec3(0, -9.81F, 0));
  TempAllocatorImpl temp(32 * 1024 * 1024);
  JobSystemSingleThreaded jobs(1024);
  BodyCreationSettings ground(new BoxShape(Vec3(200, .5F, 200)),
                              RVec3(0, -.5, -100), Quat::sIdentity(),
                              EMotionType::Static, 0);
  ground.mFriction = .8F;
  auto &bodies = sys.GetBodyInterface();
  const auto ground_id =
      bodies.CreateAndAddBody(ground, EActivation::DontActivate);
  RefinementResult result{};
  {
    scraperx::sim::kit::Kit kit(sys, 0, 1);
    scraperx::sim::PipeBridge bridge(sys, kit);
    const auto pan = kit.body_for_entity(2502);
    const auto energy = [&]() {
      double potential = 0, kinetic = 0;
      for (unsigned i = 0; i < kit.body_count(); ++i) {
        const scraperx::sim::kit::BodyIndex index{i};
        if (!kit.body_dynamic(index) || kit.body_entity(index) == 2531)
          continue;
        potential += kit.body_mass(index) * 9.81 *
                     kit.body_center_of_mass_position(index).GetY();
        kinetic += kit.body_kinetic_energy(index);
      }
      result.max_ke = std::max(result.max_ke, kinetic);
      const auto striker =
          bodies.GetWorldTransform(kit.body_id(pan)) * Vec3(0, -.15F, 0);
      const double elastic = std::max(0.0, bridge.kBedTop - striker.GetY() -
                                               bridge.state().plastic_front_m);
      return potential + kinetic + .5 * 15e6 * elastic * elastic;
    };
    const double initial = energy();
    const float dt = 1.0F / hz;
    double previous_tip = bridge.tip_height();
    bool turned = false;
    for (int tick = 0; tick < 55 * hz; ++tick) {
      const double t = double(tick) / hz;
      auto active = t >= 3 && t < 5     ? bridge.rack_handle()
                    : t >= 14 && t < 16 ? bridge.bridge_handle()
                                        : scraperx::sim::kit::BodyIndex{};
      auto before = active.valid() ? kit.body_position(active) : RVec3::sZero();
      kit.pre_step(dt);
      bridge.pre_step(dt);
      if (active.valid())
        bodies.AddForce(kit.body_id(active), Vec3(-150, 0, 0));
      sys.Update(dt, 1, &temp, &jobs);
      kit.post_step(dt);
      if (active.valid())
        result.input_work +=
            -150 * (kit.body_position(active).GetX() - before.GetX());
      energy();
      const auto transform = bodies.GetWorldTransform(kit.body_id(pan));
      const auto up = kit.body_rotation(pan) * Vec3::sAxisY();
      result.pan_pitch = std::max<double>(result.pan_pitch,
                                  std::abs(std::atan2(up.GetZ(), up.GetY())));
      const auto floor_rotation =
          Quat::sRotation(Vec3::sAxisX(), -3 * std::acos(-1.) / 180);
      for (float x : {-2.3F, 2.3F})
        for (float z : {-2.25F, 2.25F}) {
          const auto corner = transform * (Vec3(0, -.05F, 0) +
                                           floor_rotation * Vec3(x, -.05F, z));
          result.floor_gap = std::min(
              result.floor_gap,
              double(corner.GetY() - (bridge.kBedTop - bridge.kStroke)));
        }
      const double tip = bridge.tip_height();
      if (previous_tip > 7 && tip < previous_tip)
        turned = true;
      if (turned) {
        result.post_min = std::min(result.post_min, tip);
        result.post_max = std::max(result.post_max, tip);
      }
      previous_tip = tip;
    }
    result.tip = bridge.tip_height();
    result.front = bridge.state().plastic_front_m;
    result.work = bridge.state().plastic_work_j;
    result.pipes = bridge.retained_pipes();
    result.loss = initial + result.input_work - energy() - result.work;
  }
  bodies.RemoveBody(ground_id);
  bodies.DestroyBody(ground_id);
  std::cout << std::setprecision(10) << "hz=" << hz << " pipes=" << result.pipes
            << " tip=" << result.tip << " front=" << result.front
            << " crush_work=" << result.work
            << " input_work=" << result.input_work
            << " other_loss=" << result.loss << " peak_ke=" << result.max_ke
            << " pan_pitch=" << result.pan_pitch
            << " floor_gap=" << result.floor_gap
            << " post_min=" << result.post_min
            << " post_max=" << result.post_max << std::endl;
  return result;
}
int main() {
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory();
  RegisterTypes();
  const auto coarse = run(90), fine = run(360);
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
  // Receiving corridor and physical arrest are fixed design requirements.
  const auto valid = [](const RefinementResult &r) {
    return r.pipes == 20 && r.tip >= 7.53 && r.tip <= 8.2 && r.front > 0 &&
           r.front < .58 && r.loss >= -1000 && r.floor_gap > 0 &&
           r.post_min >= 7.53 && r.post_max <= 8.2;
  };
  if (!valid(coarse) || !valid(fine) || std::abs(coarse.tip - fine.tip) > .05 ||
      std::abs(coarse.front - fine.front) > .01)
    return 1;
  std::cout << "PASS production pipe bridge refinement 90/360 Hz\n";
}
