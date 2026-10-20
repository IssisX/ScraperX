#include "sim/finite_line_reel.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using scraperx::sim::FiniteLineReel;
void check(bool value,const char *message) {
    if (!value) throw std::runtime_error(message);
}
}
int main() {
    try {
        constexpr float dt=1.0F/360.0F;
        const auto unloaded=[](float,float) { return 0.0F; };
        const auto slack=FiniteLineReel::step(2,1,0,dt,1,.6F,18,unloaded);
        check(slack.rest_length_m<2 && double(2)-slack.rest_length_m<=dt,"slack speed cap");
        check(slack.work_j==0 && slack.absorbed_j==0 && slack.drive_limited,"slack is free takeup");
        const auto load=[](float rest,float) { return 6000.0F*(2.1F-rest); };
        const auto paid=FiniteLineReel::step(2,2.1F,600,dt,1,.6F,18,load);
        check(paid.rest_length_m<2 && paid.drive_limited,"loaded takeup");
        check(paid.work_j>0 && paid.work_j<=750.0*dt,"paid work budget");
        check(double(2)-paid.rest_length_m<=dt && load(paid.rest_length_m,dt)<=1500,"speed and force caps");
        check(paid.work_j>=FiniteLineReel::potential(2.1F,paid.rest_length_m)-FiniteLineReel::potential(2.1F,2),"stored energy paid");
        const auto excessive=[](float,float) { return 1501.0F; };
        const auto stalled=FiniteLineReel::step(2,2,0,dt,1,.6F,18,excessive);
        check(stalled.rest_length_m==2 && stalled.work_j==0 && !stalled.drive_limited,"current demand stalls despite zero old tension");
        const auto invalid=[](float,float) { return std::numeric_limits<float>::quiet_NaN(); };
        const auto bad=FiniteLineReel::step(2,2,0,dt,1,.6F,18,invalid);
        check(bad.rest_length_m==2 && bad.work_j==0 && !bad.drive_limited,"invalid current demand stalls");
        const auto payout=FiniteLineReel::step(2,2.1F,600,dt,-1,.6F,18,load);
        check(payout.rest_length_m>2 && payout.work_j==0 && !payout.drive_limited,"payout cannot supply command work");
        check(payout.absorbed_j==FiniteLineReel::potential(2.1F,2)-FiniteLineReel::potential(2.1F,payout.rest_length_m),"payout dissipates removed spring storage");
        const auto neutral=FiniteLineReel::step(2,2.1F,600,dt,0,.6F,18,load);
        check(neutral.rest_length_m==2 && neutral.work_j==0 && neutral.absorbed_j==0 && !neutral.drive_limited,"neutral leaves rest and work unchanged");
        const auto stop=FiniteLineReel::step(.6F,.5F,0,dt,1,.6F,18,unloaded);
        check(stop.rest_length_m==.6F && stop.work_j==0 && !stop.drive_limited,"minimum stop");
        const auto short_payout=FiniteLineReel::step(.3F,.3F,0,dt,-1,.6F,18,unloaded);
        check(short_payout.rest_length_m>.3F,"short fired line can pay out below takeup stop");
        const auto full=FiniteLineReel::step(18,18,0,dt,-1,.6F,18,unloaded);
        check(full.rest_length_m==18 && full.absorbed_j==0,"maximum stop");
        std::cout << "PASS finite_line_reel slack loaded_work force_speed rounding current_demand payout neutral stops\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
