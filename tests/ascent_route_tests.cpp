#include "sim/simulation.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <utility>
// AS-017: public walking, facing and hand input only; no body pose setters.
using namespace scraperx::sim;
bool walk(Simulation &s, double x, double z, double seconds = 20) {
  for (int i = 0; i < int(seconds * 90); ++i) {
    auto p = s.snapshot().player_position;
    double dx = x - p.x, dz = z - p.z, d = std::hypot(dx, dz);
    if (d < .07) {
      (void)s.set_move_input(0, 0);
      return true;
    }
    double a = std::min(1., d / .6);
    (void)s.set_move_input(dx / d * a, dz / d * a);
    (void)s.set_facing(dx / d, dz / d);
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
  }
  (void)s.set_move_input(0, 0);
  return false;
}
void report(Simulation &s, const char *label) {
  auto p = s.snapshot();
  std::cout << label << " pos " << p.player_position.x << ","
            << p.player_position.y << "," << p.player_position.z << " target "
            << p.carry_target_entity_id << " carrying " << p.carrying_entity_id
            << " tip " << s.pipe_bridge_tip_height() << " pipes "
            << s.pipe_bridge_retained_pipes() << " support "
            << p.support_entity_id << " deaths " << p.death_count << std::endl;
}
void wait(Simulation &s, double secs) {
  for (int i = 0; i < int(secs * 90); ++i)
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
}

// All progress is through public player inputs, with no spawned upper checkpoint.
bool walk_to(Simulation &s, double x, double z, double seconds, double tolerance=.15) {
  for (int i=0; i<int(seconds*90); ++i) {
    const auto p=s.snapshot().player_position;
    const double dx=x-p.x, dz=z-p.z, distance=std::hypot(dx,dz);
    if (distance<tolerance) { (void)s.set_move_input(0,0); return true; }
    const double strength=std::min(1.,distance/.6);
    (void)s.set_move_input(dx/distance*strength,dz/distance*strength);
    (void)s.set_facing(dx,dz);
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
  }
  (void)s.set_move_input(0,0); return false;
}
template<class Predicate> bool wait_for(Simulation &s, double seconds, Predicate reached) {
  for(int i=0;i<int(seconds*90);++i) {
    if(reached(s.snapshot())) return true;
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
  }
  return reached(s.snapshot());
}
template<class Predicate> bool hold_stick(Simulation &s, double x, double z, double fx, double fz, double seconds, Predicate reached) {
  (void)s.set_move_input(x,z); (void)s.set_facing(fx,fz);
  const bool result=wait_for(s,seconds,reached);
  (void)s.set_move_input(0,0); return result;
}
bool standing_above(const Snapshot &s, double y) {
  return s.player_grounded && s.traversal_state==TraversalState::None && s.player_position.y>y;
}
bool is_climbing(const Snapshot &s) { return s.traversal_state==TraversalState::Climbing; }
bool climb_facade(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    // Out onto the landing, round the cabinet to its south side.
    if (!(walk_to(simulation, 21.2, -124.4, 20.0) && walk_to(simulation, 21.2, -121.3, 6.0) &&
          walk_to(simulation, 20.0, -121.3, 6.0, 0.08))) {
        report(simulation, "to the cabinet");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        report(simulation, "cabinet offered");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 13.5); })) {
        report(simulation, "mantle onto the cabinet");
        return false;
    }
    // On the cabinet, clear of the duct's underside: jump, stick to the duct.
    (void)walk_to(simulation, 20.0, -122.10, 2.0, 0.05);
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.4, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        report(simulation, "hang on the duct's lip");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    const double hang_x = simulation.snapshot().player_position.x;
    (void)hold_stick(simulation, 1, 0, 0, -1, .5, [](const Snapshot &) { return false; });
    const auto shimmy = simulation.snapshot();
    if (shimmy.traversal_state != TraversalState::Hanging ||
        shimmy.player_position.x - hang_x < .27 || shimmy.player_position.x - hang_x > .33) {
        report(simulation, "duct shimmy"); return false;
    }
    (void)hold_stick(simulation, -1, 0, 0, -1, .5, [](const Snapshot &) { return false; });
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 17.0); })) {
        report(simulation, "up onto the duct");
        return false;
    }
    // Back toward the duct's exposed lip: lower deliberately, then top out.
    if (!walk_to(simulation, 20, -123.15, 3, .03)) return false;
    (void)simulation.set_facing(0, -1);
    wait(simulation, .4);
    report(simulation, "DUCT_EDGE_READY");
    if (!simulation.snapshot().edge_drop_available) return false;
    (void)simulation.request_release();
    if (!wait_for(simulation, 2, [](const Snapshot &v) { return v.traversal_state == TraversalState::Hanging; })) return false;
    report(simulation, "DUCT_LOWERED");
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2, [](const Snapshot &v) { return standing_above(v, 17); })) return false;
    // Along the duct to the vent stack, and up it over deck 3's edge.
    if (!(walk_to(simulation, 22.5, -123.05, 6.0, 0.1) && walk_to(simulation, 24.0, -123.00, 6.0, 0.06))) {
        report(simulation, "along the duct");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 22.5); })) {
        report(simulation, "up the vent onto deck 3");
        return false;
    }
    // Along deck 3 to the monorail, out along it under the ladder to its end.
    if (!(walk_to(simulation, 22.0, -125.2, 6.0) && walk_to(simulation, 12.5, -125.2, 12.0, 0.08) &&
          walk_to(simulation, 12.5, -119.65, 12.0, 0.06))) {
        report(simulation, "out along the monorail");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    const auto at_end = simulation.snapshot();
    if (!at_end.player_grounded || !at_end.player_balancing || at_end.player_position.y < 23.0) {
        report(simulation, "standing at the monorail's end");
        return false;
    }
    // Turn to the ladder and leap for it: standing, it is out of reach.
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().grip_available) {
        report(simulation, "ladder must require a leap"); return false;
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.3, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return is_climbing(state); })) {
        report(simulation, "catch the ladder");
        return false;
    }
    // Up the ladder onto the davit's arm, and back along it onto deck 4.
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 33.5); })) {
        report(simulation, "up the ladder onto the arm");
        return false;
    }
    if (!walk_to(simulation, 12.5, -125.0, 10.0, 0.1)) {
        report(simulation, "back along the arm");
        return false;
    }
    if (!walk_to(simulation, 11.2, -125.3, 4.0, 0.1)) {
        report(simulation, "off the arm onto deck 4");
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto on_deck4 = simulation.snapshot();
    return on_deck4.player_grounded && on_deck4.player_position.y > 33.5 &&
           on_deck4.support_entity_id == Simulation::kTowerEntityId;
}

int main() {
  Simulation s;
  wait(s, .5);
  report(s, "SPAWN");
  std::set<std::uint64_t> ids;
  for (std::uint32_t body = 0; body < s.kit_body_count(); ++body)
    if (!ids.insert(s.kit_body_entity(body)).second)
      return 27;
  for (const auto entity : {1016, 2016, 2017, 2018, 2019})
    if (s.entity_body_count(entity) != 0)
      return 28;
  for (auto pair :
       {std::pair<int, double>{2500, 9000}, {2502, 1000}, {2509, 800}})
    if (std::abs(s.kit_body_mass(s.kit_body_index(pair.first)) - pair.second) >
        .01)
      return 20;

  {
    for (auto point : {Vector3{13, 0, -75}, Vector3{12.7, 0, -78.95}}) {
      if (!walk(s, point.x, point.z)) {
        report(s, "APPROACH_FAIL");
        return 1;
      }
    }
    (void)s.set_facing(-1, 0);
    wait(s, .1);
    report(s, "RACK_READY");
    if (s.snapshot().carry_target_kind != 2)
      return 29;
    (void)s.request_pick_up();
    wait(s, .15);
    report(s, "RACK_PICK");
    if (s.snapshot().carrying_entity_id != 2530)
      return 2;

    (void)s.set_move_input(-.35, 0);
    wait(s, .6);
    (void)s.set_move_input(0, 0);
    (void)s.request_set_down();
    wait(s, 10);
    report(s, "RACK_LOADED");
    if (s.pipe_bridge_retained_pipes() != 20)
      return 3;

  }
  for (auto point : {Vector3{12.7, 0, -75}, Vector3{-1.4, 0, -75},
                     Vector3{-1.4, 0, -85.6}, Vector3{-.5, 0, -85.6}}) {
    if (!walk(s, point.x, point.z)) {
      report(s, "BRIDGE_APPROACH_FAIL");
      return 4;
    }
  }
  (void)s.set_facing(0, 1);
  wait(s, .1);
  report(s, "BRIDGE_READY");
  (void)s.request_pick_up();
  wait(s, .15);
  report(s, "BRIDGE_PICK");
  if (!s.snapshot().carrying_entity_id)
    return 5;
  (void)s.set_move_input(-.35, 0);
  wait(s, .6);
  (void)s.set_move_input(0, 0);
  (void)s.request_set_down();
  wait(s, 15);
  report(s, "BRIDGE_LIFTED");

  if ((s.pipe_bridge_tip_height() < 7.53 || s.pipe_bridge_tip_height() > 8.2))
    return 6;
  const double tz = -90 - 20 * std::cos(std::asin(7.4 / 20));
  for (auto point : {Vector3{-.6, 0, -86}, Vector3{1.94, 0, -86},
                     Vector3{1.94, 0, -91.1}, Vector3{6, 0, -91.1},
                     Vector3{6, 0, tz + 1.5}, Vector3{9.06, 0, tz + 1.5},
                     Vector3{9.06, 0, tz - 2}, Vector3{9.06, 0, -125.2}}) {
    if (!walk(s, point.x, point.z)) {
      report(s, "CROSS_FAIL");
      return 7;
    }
    wait(s, .2);
    report(s, "CROSS");

  }
  report(s, "ARRIVED");
  if (s.snapshot().player_position.y < 11.6 || s.snapshot().death_count != 0)
    return 8;
  if (!climb_facade(s)) return 40;
  report(s,"FACADE_33M");
  if (s.snapshot().death_count != 0) return 41;
  // Walk into a real fall after committing the upper ring; the failed
  // timeline must restore both the player and the spent pipe mechanism.
  wait(s, 1);
  const double spent = s.pipe_bridge_crush_front();
  if (!walk_to(s, 24, -125.3, 10, .1)) return 42;
  (void)s.set_move_input(1, 0);
  wait(s, 1.5);
  (void)s.set_move_input(0, 0);
  wait(s, 5);
  report(s, "FACADE_RESTORED");
  if (s.snapshot().death_count != 1 || !standing_above(s.snapshot(), 33.5) ||
      s.pipe_bridge_retained_pipes() != 20 ||
      std::abs(s.pipe_bridge_crush_front() - spent) > .002) return 43;
  if (!walk_to(s, 11.2, -125.3, 10, .1) ||
      !wait_for(s, 2, [](const Snapshot &v) { return standing_above(v, 33.5); }) ||
      s.snapshot().death_count != 1) { report(s, "CONTINUE_FAIL"); return 44; }
  std::cout << "PASS grade -> pipe -> facade: supported +33m, checkpoint restored" << std::endl;
  return 0;
}
