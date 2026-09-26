#include "sim/pipe_bridge.hpp"
#include <Jolt/Physics/Body/BodyLock.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using Part = kit::Part;
Part box(Vec3 half, float mass) {
  Part p;
  p.half = half;
  p.mass_kg = mass;
  p.convex_radius = .01F;
  return p;
}
Part cylinder(float half_length, float radius, float mass, float margin) {
  Part p = box(Vec3(radius, half_length, radius), mass);
  p.shape = Part::Shape::Cylinder;
  p.convex_radius = margin;
  return p;
}
struct Compound {
  std::vector<Part> parts;
  void AddShape(Vec3 offset, Quat rotation, Part part) {
    part.offset = offset;
    part.rotation = rotation;
    parts.push_back(part);
  }
};
RVec3 wp(const Body &body, Vec3 point) {
  return body.GetWorldTransform() * point;
}
struct Build {
  PhysicsSystem &sys;
  kit::Kit &kit;
  std::unordered_map<const Body *, kit::BodyIndex> indices;
  std::uint64_t next_dynamic = 2500, next_static = 1500;
  Body *body(const std::vector<Part> &parts, RVec3 at, Quat rotation,
             bool moving, const std::string &name) {
    float mass = 0;
    if (moving)
      for (const auto &part : parts)
        mass += part.mass_kg;
    auto display = parts;
    for (auto &part : display) {
      part.material = kit::Material::Steel;
      if (!moving &&
          (name.find("bearing") != std::string::npos ||
           name.find("column") != std::string::npos ||
           name == "lower_deck_seat" || name == "receiver_ultimate_floor"))
        part.material = kit::Material::Concrete;
      if (name == "route_deck" || name == "landing")
        part.material = kit::Material::Galvanised;
      if (name == "bridge" || name == "aux_arm")
        part.material = part.mass_kg > 1000 ? kit::Material::Galvanised
                                            : kit::Material::Rust;
      if (name == "pan" || name == "rack_floor")
        part.material = kit::Material::Galvanised;
      if (name == "gate_frame" || name == "rack_underframe" ||
          name == "rack_leg")
        part.material = kit::Material::Rust;
      if (name == "rack_gate")
        part.material = part.mass_kg >= 500 ? kit::Material::Concrete
                                            : kit::Material::Yellow;
      if (name == "bridge_prop" || name == "rack_prop")
        part.material = kit::Material::Yellow;
      if (name == "crush_pack")
        part.material = kit::Material::Timber;
      if (name.find("pipe_") == 0)
        part.material = kit::Material::Rust;
      if (name.find("handle") != std::string::npos || name == "rack_grip")
        part.material = kit::Material::Yellow;
    }
    const auto id = kit.add_body(moving ? next_dynamic++ : next_static++,
                                 display, at, rotation, mass, .8F);
    if (moving) {
      kit.set_damping(id, 0, 0);
      kit.set_continuous_collision(id);
    }
    Body *raw = sys.GetBodyLockInterfaceNoLock().TryGetBody(kit.body_id(id));
    indices.emplace(raw, id);
    return raw;
  }
  Body *body(Part part, RVec3 at, Quat rotation, bool moving,
             const std::string &name) {
    return body(std::vector<Part>{part}, at, rotation, moving, name);
  }
  kit::BodyIndex index(const Body *raw) const { return indices.at(raw); }
  void hinge(Body *first, Body *second, RVec3 pivot, double friction,
             Vec3 axis) {
    kit.add_hinge(first ? index(first) : kit::BodyIndex{}, index(second), pivot,
                  axis, static_cast<float>(friction));
  }
};
Body *make_input(Build &f, Body *prop, double prop_x) {
  Body *handle =
      f.body(box(Vec3(.15, .15, .15), 3), RVec3(prop_x - 3, 1.25, -85),
             Quat::sIdentity(), true, "bridge_handle");
  f.sys.GetBodyInterface().SetFriction(handle->GetID(), .1F);
  f.body(box(Vec3(.7, .15, .15), 100), RVec3(prop_x - 3, .95, -85),
         Quat::sIdentity(), false, "bridge_handle_table");
  f.body(box(Vec3(.15, .5, .15), 100), RVec3(prop_x - 3.59, 1, -85),
         Quat::sIdentity(), false, "bridge_handle_stop");
  f.kit.add_guide(f.index(handle), Vec3(-1, 0, 0), 0, .28F, 0, 0, 0);
  f.kit.add_tie(f.index(prop), Vec3(0, 2.3F, 0), f.index(handle),
                Vec3::sZero());
  f.kit.set_carry(f.index(handle), kit::CarryKind::Handle, Vec3::sZero());
  return handle;
}
} // namespace

PipeBridge::PipeBridge(JPH::PhysicsSystem &system, kit::Kit &kit)
    : system_(system), kit_(kit) {
  using namespace JPH;
  Build f{system, kit, {}};
  const double pi = std::acos(-1.), alpha = 5 * pi / 180,
               tailL = std::hypot(5, 2.8), rackRaise = .5;
  const int pipeCount = 20;
  // Actual open pan and outboard four-bar; no preloaded ballast body.
  const RVec3 pivot(6, .6, -90), mainPivot(3.2, .6, -90),
      auxPivot(8.8, 1.8, -90), panPin(3.2, 3.4, -85), auxPin(8.8, 4.6, -85),
      panOrigin(6, 2.4, -85);
  const auto pedestal = [&](RVec3 top, Vec3 half_width, const char *name) {
    return f.body(
        box(Vec3(half_width.GetX(), top.GetY() / 2, half_width.GetZ()), 1000),
        RVec3(top.GetX(), top.GetY() / 2, top.GetZ()), Quat::sIdentity(), false,
        name);
  };
  Body *mainBearing = pedestal(mainPivot, Vec3(.2, 0, .3), "main_bearing");
  Body *auxBearing = pedestal(auxPivot, Vec3(.3, 0, .3), "aux_bearing");
  const Quat tailRot = Quat::sRotation(Vec3::sAxisX(), -std::atan2(2.8, 5));
  Compound bs;
  bs.AddShape(Vec3(0, -.2, -10), Quat::sIdentity(),
              box(Vec3(1.5, .2, 10), 8500));
  bs.AddShape(Vec3(-2.8, 1.4, 2.5), tailRot,
              box(Vec3(.15, .15, tailL / 2), 400));
  bs.AddShape(Vec3(-1.4, 0, 0), Quat::sIdentity(),
              box(Vec3(1.55, .15, .15), 50));
  // A cross-foot connects the outboard arm to the prop's roller-bearing shoe.
  const double propX = 2.5, propOffset = .04, propL = 2.4,
               propPhi = std::asin(propOffset / propL),
               shoeBottom = .4 + propL * std::cos(propPhi) + .3;
  bs.AddShape(Vec3(propX + .04 - 6, shoeBottom + .15 - .6, 5),
              Quat::sIdentity(), box(Vec3(.15, .15, .6), 40));
  bs.AddShape(Vec3((propX + .04 + 3.2) / 2 - 6, shoeBottom + .15 - .6, 5),
              Quat::sIdentity(), box(Vec3(.38, .15, .15), 10));
  Body *beam = f.body(bs.parts, pivot, Quat::sIdentity(), true, "bridge");
  Body *aux = f.body(box(Vec3(.15, .15, tailL / 2), 500),
                     RVec3(8.8, 3.2, -87.5), tailRot, true, "aux_arm");
  Compound ps;
  ps.AddShape(Vec3(0, -.05, 0), Quat::sRotation(Vec3::sAxisX(), -3 * pi / 180),
              box(Vec3(2.3, .05, 2.25), 385));
  ps.AddShape(Vec3(0, .65, -2.4), Quat::sIdentity(),
              box(Vec3(2.3, .65, .15), 100));
  for (double x : {-2.45, 2.45}) {
    // Open retaining sides expose the actual cargo from the operator's
    // ground-level view. Gaps remain narrower than the 0.8 m pipe diameter.
    ps.AddShape(Vec3(x, .1, 0), Quat::sIdentity(),
                box(Vec3(.15, .1, 2.25), 30));
    ps.AddShape(Vec3(x, .65, 0), Quat::sIdentity(),
                box(Vec3(.15, .1, 2.25), 50));
    for (double z : {-2., 2.})
      ps.AddShape(Vec3(x, .4, z), Quat::sIdentity(),
                  box(Vec3(.15, .3, .1), 10));
  }
  ps.AddShape(Vec3(0, 2.3, 0), Quat::sIdentity(),
              box(Vec3(2.95, .15, .15), 100));
  ps.AddShape(Vec3(-2.8, 1.25, 0), Quat::sIdentity(),
              box(Vec3(.15, 1.2, .15), 25));
  ps.AddShape(Vec3(2.8, 1.25, 0), Quat::sIdentity(),
              box(Vec3(.15, 1.2, .15), 40));
  ps.AddShape(Vec3(0, .27, 2.3), Quat::sIdentity(),
              box(Vec3(2.3, .15, .05), 100));
  ps.AddShape(Vec3(0, -.125, 0), Quat::sIdentity(),
              box(Vec3(.75, .025, .75), 50));
  Body *pan = f.body(ps.parts, panOrigin, Quat::sIdentity(), true, "pan");
  f.hinge(mainBearing, beam, mainPivot, 3000, Vec3::sAxisX());
  f.hinge(auxBearing, aux, auxPivot, 0, Vec3::sAxisX());
  f.hinge(beam, pan, panPin, 0, Vec3::sAxisX());
  f.hinge(aux, pan, auxPin, 0, Vec3::sAxisX());
  f.body(box(Vec3(1.6, .15, .5), 1000), RVec3(6, .03, -109), Quat::sIdentity(),
         false, "lower_deck_seat");
  // Physical bridge holding prop and independent reachable trip handle.
  const RVec3 bp(propX, .4, -85);
  Compound bps;
  bps.AddShape(Vec3(0, 1.05, 0), Quat::sIdentity(),
               box(Vec3(.15, 1.05, .15), 150));
  Body *bridgeProp =
      f.body(bps.parts, bp, Quat::sRotation(Vec3::sAxisZ(), -propPhi), true,
             "bridge_prop");
  Body *bridgeBearing = pedestal(bp, Vec3(.3, 0, .3), "bridge_prop_bearing");
  f.hinge(bridgeBearing, bridgeProp, bp, 15, Vec3::sAxisZ());
  RVec3 bh = wp(*bridgeProp, Vec3(0, propL, 0));
  Body *bridgeRoll =
      f.body(cylinder(.3, .3, 50, .01), bh,
             Quat::sRotation(Vec3::sAxisX(), pi / 2), true, "bridge_roller");
  f.hinge(bridgeProp, bridgeRoll, bh, 2, Vec3::sAxisZ());
  f.body(box(Vec3(.15, .15, .4), 100),
         wp(*bridgeProp, Vec3(0, .3, 0)) + RVec3(.3, 0, 0), Quat::sIdentity(),
         false, "bridge_prop_stop");
  Body *bridgeHandle = make_input(f, bridgeProp, propX);
  // Rack and retained top-hinged gate, from the release-only contact fixture.
  const double rackL = 5, e = .04, pa = -std::asin(e / rackL), arm = 4.8;
  const RVec3 gatePivot(6, 4.4 + rackRaise, -82.3),
      rackPropBase(9.5, 2.4 + rackRaise, -82.75 - rackL * std::cos(pa));
  Compound gs;
  gs.AddShape(Vec3(0, -1.6, 0), Quat::sIdentity(),
              box(Vec3(2.2, .15, .15), 200));
  gs.AddShape(Vec3(0, 0, 0), Quat::sIdentity(), box(Vec3(3.65, .15, .15), 100));
  for (double x : {-2.05, 2.05})
    gs.AddShape(Vec3(x, -.8, 0), Quat::sIdentity(),
                box(Vec3(.15, .8, .15), 50));
  gs.AddShape(Vec3(3.5, -1, 0), Quat::sIdentity(), box(Vec3(.15, 1, .15), 50));
  gs.AddShape(Vec3(3.5, -2, 0), Quat::sIdentity(), box(Vec3(.2, .15, .15), 40));
  gs.AddShape(Vec3(0, .6, 0), Quat::sIdentity(), box(Vec3(.15, .6, .15), 20));
  gs.AddShape(Vec3(0, 1.2, 0), Quat::sIdentity(), box(Vec3(.5, .5, .5), 500));
  Body *gate =
      f.body(gs.parts, gatePivot, Quat::sIdentity(), true, "rack_gate");
  Compound frame;
  const double postTop = gatePivot.GetY() - .35;
  for (double x : {-3.45, 3.45}) {
    frame.AddShape(Vec3(x, postTop / 2 - gatePivot.GetY(), .5),
                   Quat::sIdentity(), box(Vec3(.2, postTop / 2, .2), 1000));
    frame.AddShape(Vec3(x, -.45, .2), Quat::sIdentity(),
                   box(Vec3(.2, .1, .5), 1000));
    Part bearing = cylinder(.2, .35, 100, .01);
    bearing.inner_radius = .23F;
    frame.AddShape(Vec3(x, 0, 0), Quat::sRotation(Vec3::sAxisZ(), pi / 2),
                   bearing);
  }
  Body *gateFrame =
      f.body(frame.parts, gatePivot, Quat::sIdentity(), false, "gate_frame");
  f.hinge(gateFrame, gate, gatePivot, 30, Vec3::sAxisX());
  f.body(box(Vec3(1.05, .15, .3), 1000), RVec3(10.25, 5.08 + rackRaise, -83.1),
         Quat::sIdentity(), false, "rack_gate_open_stop");
  pedestal(RVec3(11, 4.93 + rackRaise, -83.1), Vec3(.2, 0, .2),
           "gate_stop_post");
  Compound rps;
  rps.AddShape(Vec3(0, 0, rackL / 2 - .15), Quat::sIdentity(),
               box(Vec3(.15, .15, rackL / 2 - .15), 200));
  rps.AddShape(Vec3(0, .2, 4.4), Quat::sIdentity(),
               box(Vec3(.15, .35, .15), 12));
  rps.AddShape(Vec3(0, .4, 4.65), Quat::sIdentity(),
               box(Vec3(.15, .15, .35), 12));
  rps.AddShape(Vec3(.275, .4, 4.8), Quat::sIdentity(),
               box(Vec3(.425, .15, .15), 16));
  Body *rackProp =
      f.body(rps.parts, rackPropBase, Quat::sRotation(Vec3::sAxisY(), pa), true,
             "rack_prop");
  Body *rackBearing =
      pedestal(rackPropBase, Vec3(.3, 0, .3), "rack_prop_bearing");
  f.hinge(rackBearing, rackProp, rackPropBase, 30, Vec3::sAxisY());
  RVec3 rh = wp(*rackProp, Vec3(0, 0, rackL));
  Body *rackRoll = f.body(cylinder(.2, .3, 50, .01), rh, Quat::sIdentity(),
                          true, "rack_roller");
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
         RVec3(6, rackTop + .2 * std::tan(alpha) - .175 / std::cos(alpha),
               rackZ + .2),
         Quat::sRotation(Vec3::sAxisX(), -alpha), false, "rack_underframe");
  for (double x : {3.5, 8.5}) {
    Compound rail;
    const double centerTop = 2.4 + rackRaise + 3.5 * std::tan(alpha);
    for (double height : {.1, .55})
      rail.AddShape(Vec3(0, height, 0), Quat::sRotation(Vec3::sAxisX(), -alpha),
                    box(Vec3(.15, .08, 2.75), 50));
    for (double z : {-2.5, 2.5})
      rail.AddShape(Vec3(0, .35 + z * std::tan(alpha), z), Quat::sIdentity(),
                    box(Vec3(.15, .35, .1), 50));
    f.body(rail.parts, RVec3(x, centerTop, -79.2), Quat::sIdentity(), false,
           "rack_rail");
  }
  for (double x : {3.9, 8.1})
    for (double z : {-79.5, -77.2}) {
      const double top =
          2.4 + rackRaise + (z + 82.7) * std::tan(alpha) - .3 / std::cos(alpha);
      pedestal(RVec3(x, top, z), Vec3(.2, 0, .2), "rack_leg");
    }
  std::vector<Body *> pipes;
  for (int index = 0; index < pipeCount; ++index) {
    int lane = index / 5, row = index % 5;
    double z = -81.73 + row * .805 / std::cos(alpha),
           y = 2.4 + rackRaise + (z + 82.7) * std::tan(alpha) +
               .4 / std::cos(alpha);
    Part pipe = cylinder(.5, .4, 800, .005);
    pipe.inner_radius = static_cast<float>(std::sqrt(.16 - 800 / (7850 * pi)));
    Body *p = f.body(pipe, RVec3(4.35 + lane * 1.1, y, z),
                     Quat::sRotation(Vec3::sAxisZ(), pi / 2), true,
                     "pipe_" + std::to_string(index));
    pipes.push_back(p);
    double ri2 = .16 - 800 / (7850 * pi), ia = .5 * 800 * (.16 + ri2),
           it = 800 * (3 * (.16 + ri2) + 1) / 12;
    MassProperties mp;
    mp.mMass = 800;
    mp.mInertia = Mat44::sScale(Vec3(it, ia, it));
    kit_.set_mass_properties(f.index(p), mp);
  }
  // A visible 3:1 lever supplies the rack release advantage. Its mass and
  // inertia are solved directly; the output cord is tension-only.
  const RVec3 releasePivot(12, 2.4 + rackRaise, -81.95);
  Compound releaseParts;
  releaseParts.AddShape(Vec3(0, 0, 1), Quat::sIdentity(),
                        box(Vec3(.15, .15, 2.15), 100));
  Body *releaseLever = f.body(releaseParts.parts, releasePivot,
                              Quat::sIdentity(), true, "rack_release_lever");
  Body *rackGrip = f.body(box(Vec3(.15, .575 + rackRaise / 2, .15), 20),
                          releasePivot + RVec3(0, -.575 - rackRaise / 2, 3),
                          Quat::sIdentity(), true, "rack_grip");
  kit_.add_fixed_joint(f.index(releaseLever), f.index(rackGrip));
  kit_.set_carry(f.index(rackGrip), kit::CarryKind::Handle,
                 Vec3(0, static_cast<float>(-.575 - rackRaise / 2), 0));
  Body *leverBearing =
      pedestal(releasePivot, Vec3(.3, 0, .3), "rack_lever_bearing");
  f.hinge(leverBearing, releaseLever, releasePivot, 15, Vec3::sAxisY());
  f.body(box(Vec3(.15, .65, .4), 100), RVec3(11.41, 1.825, -78.95),
         Quat::sIdentity(), false, "rack_lever_stop");
  kit_.add_tie(f.index(rackProp), Vec3(.55, .4, arm), f.index(releaseLever),
               Vec3(0, 0, -1));
  beam_ = f.index(beam);
  pan_ = f.index(pan);
  bridge_handle_ = f.index(bridgeHandle);
  rack_handle_ = f.index(rackGrip);
  for (std::size_t i = 0; i < pipes.size(); ++i)
    pipes_[i] = f.index(pipes[i]);
  f.body(box(Vec3(2.3, .15, 2.25), 1000),
         RVec3(6, kBedTop - kStroke - .15, -84.5), Quat::sIdentity(), false,
         "receiver_ultimate_floor");
  const double nominal = std::asin(7.4 / 20),
               tip_z = -90 - 20 * std::cos(nominal);
  const auto slab = [&](double x, double width, double start_z, double end_z,
                        double start_y, double end_y, double thickness) {
    const double run = start_z - end_z, rise = end_y - start_y;
    const double pitch = std::atan2(rise, run), h = thickness / 2;
    f.body(box(Vec3(width / 2, h, std::hypot(run, rise) / 2), 1000),
           RVec3(x, (start_y + end_y) / 2 - h * std::cos(pitch),
                 (start_z + end_z) / 2 - h * std::sin(pitch)),
           Quat::sRotation(Vec3::sAxisX(), pitch), false, "route_deck");
  };
  // Stay west of the bearing, then join north of its crosshead sweep.
  slab(1.69, 2.5, -86, -90.5, 0, .8, .3);
  slab(2.44, 4, -90.5, -90.5 - 2 * std::cos(nominal), .8, 1.54, .3);
  slab(9.06, 3, tip_z + 3, tip_z, 7.75 - 3 * std::tan(nominal), 7.75, .3);
  f.body(box(Vec3(1.5, .2, 2), 1000), RVec3(9.06, 7.8, tip_z - 2),
         Quat::sIdentity(), false, "landing");
  slab(9.06, 3, tip_z - 4, -124, 8, 11, .3);
  // Visible ground reactions: route columns stay beneath their decks.
  for (double z : {tip_z - 1, tip_z - 3})
    for (double x : {7.86, 10.26})
      f.body(box(Vec3(.2, 3.8, .2), 1000), RVec3(x, 3.8, z), Quat::sIdentity(),
             false, "landing_column");
  for (double z : {-117., -122.}) {
    const double top = 8 + 3 * ((tip_z - 4) - z) / ((tip_z - 4) + 124);
    for (double x : {7.86, 10.26})
      f.body(box(Vec3(.2, (top - .3) / 2, .2), 1000),
             RVec3(x, (top - .3) / 2, z), Quat::sIdentity(), false,
             "connector_column");
  }
  // The finite material's occupied surface also collides with the player and
  // loose cargo. Its kinematic boundary follows irreversible compression;
  // pan contact is supplied once by the constitutive force below.
  Body *pack = f.body(box(Vec3(1.25, kStroke / 2, 1.25), 1000),
                      RVec3(6, kBedTop - kStroke / 2, -84.5), Quat::sIdentity(),
                      true, "crush_pack");
  crush_pack_ = f.index(pack);
  system_.GetBodyInterface().SetMotionType(
      pack->GetID(), EMotionType::Kinematic, EActivation::Activate);
  kit_.disable_collision(crush_pack_, pan_);
}

void PipeBridge::pre_step(const float delta_seconds) {
  auto &bodies = system_.GetBodyInterface();
  const auto transform = bodies.GetWorldTransform(kit_.body_id(pan_));
  const JPH::RVec3 striker = transform * JPH::Vec3(0, -.15F, 0);
  // The finite pad only reacts beneath the complete central striker.
  if (std::abs(striker.GetX() - 6) <= .5 &&
      std::abs(striker.GetZ() + 84.5) <= .5) {
    const double penetration = std::max(0.0, kBedTop - striker.GetY());
    constexpr double yield = 90000, stiffness = 15e6;
    const double next =
        std::min(kStroke, std::max(state_.plastic_front_m,
                                   penetration - yield / stiffness));
    state_.plastic_work_j += yield * (next - state_.plastic_front_m);
    state_.plastic_front_m = next;
    const double normal = stiffness * std::max(0.0, penetration - next);
    if (normal > 0)
      bodies.AddForce(kit_.body_id(pan_),
                      JPH::Vec3(0, static_cast<float>(normal), 0), striker);
  }
  bodies.MoveKinematic(
      kit_.body_id(crush_pack_),
      JPH::RVec3(6, kBedTop - kStroke / 2 - state_.plastic_front_m, -84.5),
      JPH::Quat::sIdentity(), delta_seconds);
}

double PipeBridge::tip_height() const {
  return (system_.GetBodyInterface().GetWorldTransform(kit_.body_id(beam_)) *
          JPH::Vec3(0, 0, -20))
      .GetY();
}

std::uint32_t PipeBridge::retained_pipes() const {
  const auto pan_inverse = system_.GetBodyInterface()
                               .GetWorldTransform(kit_.body_id(pan_))
                               .InversedRotationTranslation();
  std::uint32_t count = 0;
  for (const auto pipe : pipes_) {
    const auto p = pan_inverse * kit_.body_center_of_mass_position(pipe);
    if (std::abs(p.GetX()) < 2.3 && std::abs(p.GetZ()) < 2.25 && p.GetY() > 0 &&
        p.GetY() < 1.6)
      ++count;
  }
  return count;
}
} // namespace scraperx::sim
