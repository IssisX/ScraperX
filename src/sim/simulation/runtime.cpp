#include "sim/simulation/runtime.hpp"

namespace scraperx::sim::detail {
std::mutex JoltRuntimeLease::mutex_;
std::uint32_t JoltRuntimeLease::lease_count_ = 0;
} // namespace scraperx::sim::detail
