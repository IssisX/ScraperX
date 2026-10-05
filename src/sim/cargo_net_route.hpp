#pragma once
#include "sim/mechanism_kit.hpp"
namespace scraperx::sim {
// AS-024: fixed concrete footings and a finite-mass, elastically anchored
// gantry/receiver. The same native parts supply collision and drawing.
constexpr std::uint64_t kCargoGantryEntity=2952;
kit::BodyIndex build_cargo_net_route(kit::Kit &kit);
}
