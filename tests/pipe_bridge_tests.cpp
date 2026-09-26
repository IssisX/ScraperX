#include "sim/simulation.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <utility>
// AS-016: public walking, facing and hand input only; no body pose setters.
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
int main(int argc, char **argv) {
  const int mode = argc > 1 ? std::stoi(argv[1]) : 0;
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
  if (mode == 1) {
    wait(s, 30);
    report(s, "NO_INPUT");
    return s.pipe_bridge_tip_height() < .63 &&
                   s.pipe_bridge_retained_pipes() == 0
               ? 0
               : 21;
  }
  if (mode != 3) {
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
    if (mode == 4) {
      (void)s.set_move_input(-.15, 0);
      wait(s, .05);
      (void)s.set_move_input(0, 0);
      (void)s.request_set_down();
      wait(s, 1);
      if (s.pipe_bridge_retained_pipes() != 0)
        return 30;
      (void)s.request_pick_up();
      wait(s, .15);
      if (s.snapshot().carrying_entity_id != 2530)
        return 31;
      report(s, "PARTIAL_PULL_REGRAB");
    }
    (void)s.set_move_input(-.35, 0);
    wait(s, .6);
    (void)s.set_move_input(0, 0);
    (void)s.request_set_down();
    wait(s, 10);
    report(s, "RACK_LOADED");
    if (s.pipe_bridge_retained_pipes() != 20)
      return 3;
    if (mode == 2) {
      wait(s, 20);
      report(s, "RACK_ONLY");
      return s.pipe_bridge_tip_height() < .63 ? 0 : 22;
    }
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
  wait(s, mode == 5 ? .1 : 15);
  report(s, "BRIDGE_LIFTED");
  if (mode == 3) {
    report(s, "EMPTY_RELEASE");
    return s.pipe_bridge_tip_height() < 1 ? 0 : 23;
  }
  if (mode != 5 &&
      (s.pipe_bridge_tip_height() < 7.53 || s.pipe_bridge_tip_height() > 8.2))
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
    if (mode == 5 && point.x == 6 && point.z == -91.1) {
      const auto boarded = s.snapshot();
      if (boarded.support_entity_id != 2500 ||
          s.kit_body_velocity(s.kit_body_index(2500)).y < .01)
        return 32;
      wait(s, 10);
      report(s, "PASSIVE_RIDER");
      if (s.snapshot().support_entity_id != 2500 ||
          s.snapshot().player_position.y < boarded.player_position.y + .05)
        return 33;
    }
  }
  report(s, "ARRIVED");
  if (s.snapshot().player_position.y < 11.6 || s.snapshot().death_count != 0)
    return 8;
  // Reach the ordinary +33m ring, then step beyond its edge. A real lethal
  // landing must restore the spent crusher and loaded bridge with the player.
  for (auto point : {Vector3{21.5, 0, -128.5}, Vector3{21.5, 0, -171.5},
                     Vector3{-21.5, 0, -171.5}, Vector3{-21.5, 0, -128.5},
                     Vector3{21, 0, -128.5}}) {
    if (!walk(s, point.x, point.z)) {
      report(s, "RESTORE_APPROACH_FAIL");
      return 24;
    }
  }
  wait(s, 1);
  report(s, "BEFORE_FALL");
  if (s.snapshot().player_position.y < 33.5)
    return 25;
  const double spent = s.pipe_bridge_crush_front();
  (void)s.set_move_input(1, 0);
  wait(s, 1.3);
  (void)s.set_move_input(0, 0);
  wait(s, 5);
  report(s, "RESTORED");
  if (s.snapshot().death_count != 1 || s.snapshot().player_position.y < 33.5 ||
      s.pipe_bridge_retained_pipes() != 20 ||
      std::abs(s.pipe_bridge_crush_front() - spent) > .002)
    return 26;
  std::cout << "PASS scraperx_sim AS-016 full route pipes=20 arrival_y=11.9 "
               "death_restore=1 spent_crusher="
            << spent << std::endl;
  return 0;
}
