#include "sim/simulation.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>
using namespace scraperx::sim;
int main(){
 Simulation s(InitialSpawn::FreightYardArrival);
 for(unsigned i=0; !s.is_initial_world_settled() && i<1000; ++i)
  if(!s.advance_frame(Simulation::kFixedStepSeconds).accepted)return 1;
 if(!s.is_initial_world_settled())return 2;
 const auto ready=s.snapshot().tick_index;
 // Same fixed neutral interval after the actual readiness boundary.
 std::vector<double> ms; ms.reserve(360);
 const auto cpu=std::clock();
 for(unsigned i=0;i<360;++i){
  auto start=std::chrono::steady_clock::now();
  if(!s.advance_frame(Simulation::kFixedStepSeconds).accepted)return 3;
  ms.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
 }
 const double cpu_ms=1000.*(std::clock()-cpu)/CLOCKS_PER_SEC/360;
 double mean=0;for(auto t:ms)mean+=t/360;
 std::sort(ms.begin(),ms.end());
 std::cout<<"IDLE ready="<<ready<<" wall_mean_ms="<<mean<<" p50_ms="<<ms[180]<<" p95_ms="<<ms[342]<<" cpu_ms="<<cpu_ms<<" deaths="<<s.snapshot().death_count<<'\n';
 return s.snapshot().death_count?4:0;
}
