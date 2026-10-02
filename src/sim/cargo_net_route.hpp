#pragma once
#include "sim/mechanism_kit.hpp"
namespace scraperx::sim {
// AS-024: one native static assembly; the same parts supply collision and drawing.
void build_cargo_net_route(kit::Kit &kit);
}
