#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {
// AS-022's fixed north-face frame. One Kit part list owns collision and drawing.
void build_north_service_frame(kit::Kit &kit);
}
