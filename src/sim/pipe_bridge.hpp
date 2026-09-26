#pragma once
#include "sim/mechanism_kit.hpp"
#include <array>

namespace scraperx::sim {
// AS-016. Kit owns bodies/constraints; this object owns only the finite
// receiver material and observes the mechanism. No stage/progress controller.
class PipeBridge final {
public:
  struct State {
    double plastic_front_m = 0;
    double plastic_work_j = 0;
  };
  PipeBridge(JPH::PhysicsSystem &system, kit::Kit &kit);
  void pre_step(float delta_seconds);
  [[nodiscard]] const State &state() const noexcept { return state_; }
  void restore(const State &state) noexcept { state_ = state; }
  [[nodiscard]] double tip_height() const;
  [[nodiscard]] std::uint32_t retained_pipes() const;
  [[nodiscard]] kit::BodyIndex rack_handle() const { return rack_handle_; }
  [[nodiscard]] kit::BodyIndex bridge_handle() const { return bridge_handle_; }
  static constexpr double kBedTop = .6553712630033334;
  static constexpr double kStroke = .65;

private:
  JPH::PhysicsSystem &system_;
  kit::Kit &kit_;
  kit::BodyIndex beam_, pan_, rack_handle_, bridge_handle_, crush_pack_;
  std::array<kit::BodyIndex, 20> pipes_{};
  State state_{};
};
} // namespace scraperx::sim
