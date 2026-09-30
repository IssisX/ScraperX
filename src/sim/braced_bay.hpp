#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {
// AS-021: fixed structural parkour from the +77 m service deck to +88 m.
// The native part list also supplies the rendered geometry.
void build_braced_bay(kit::Kit &kit);
} // namespace scraperx::sim
