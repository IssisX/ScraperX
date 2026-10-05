#include "sim/simulation.hpp"
#include "lift_fixture.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char**argv){
 using namespace scraperx::sim;using namespace scraperx::sim::insertable;
 int mode=argc>1?std::atoi(argv[1]):1;
 fixture_initial_energy=argc>2?std::atof(argv[2]):1200000;
 int driveSeconds=argc>3?std::atoi(argv[3]):20;
 Simulation sim;
 Vector3 start=mode==0?Vector3{-22,121.9,-162}:Vector3{-31,121.9,mode==2?-169.0:-162};
 if(!sim.debug_restart_at(start)){std::puts("FAIL stage");return 1;}
 for(int i=0;i<180;++i)(void)sim.advance_frame(Simulation::kFixedStepSeconds);
 auto state=sim.snapshot();std::printf("START grounded=%d support=%llu player_y=%.6f\n",state.player_grounded,(unsigned long long)state.support_entity_id,state.player_position.y);
 if(mode&&(!state.player_grounded||state.support_entity_id!=2973)){std::puts("FAIL support");return 2;}
 double maxGap=0;int unsupported=0;fixture_direction=-1;
 for(int i=0;i<driveSeconds*90;++i){(void)sim.advance_frame(Simulation::kFixedStepSeconds);state=sim.snapshot();auto deck=sim.kit_body_position(sim.kit_body_index(2973));if(mode){maxGap=std::max(maxGap,std::abs(state.player_position.y-deck.y-1.2));if(!state.player_grounded||state.support_entity_id!=2973)++unsupported;}}
 if(mode==4){
 const auto before=fixture_lift->capture();
 if(!sim.restart_checkpoint()){std::puts("FAIL restart");return 5;}
 const auto after=fixture_lift->capture();
 const double error=std::abs(before.energy-after.energy);
 std::printf("CHECKPOINT energy_error_j=%.9f cutoff_before=%d cutoff_after=%d target_error=%.9f\n",error,before.cutoff,after.cutoff,std::abs(before.target-after.target));
 if(error>.001||before.cutoff!=after.cutoff||std::abs(before.target-after.target)>1e-6)return 6;
 }
 double initialVy=sim.snapshot().player_linear_velocity.y;
 if(mode==3){(void)sim.set_move_input(-1,0);(void)sim.request_jump();}
 double forceSum=0;int forceCount=0;
 for(int i=0;i<180;++i){(void)sim.advance_frame(Simulation::kFixedStepSeconds);if(i>=135){forceSum+=fixture_lift->drive->GetTotalLambdaMotor()/(Simulation::kFixedStepSeconds/4);++forceCount;}}
 std::printf("DEPARTURE mode=%d initial_vy=%.6f final_vy=%.6f mean_force_n=%.3f\n",mode,initialVy,sim.snapshot().player_linear_velocity.y,forceSum/forceCount);
 fixture_direction=0;
 const double stopY=sim.kit_body_position(sim.kit_body_index(2973)).y;
 for(int i=0;i<180;++i)(void)sim.advance_frame(Simulation::kFixedStepSeconds);
 const double stopDrop=stopY-sim.kit_body_position(sim.kit_body_index(2973)).y;
 std::printf("ENERGY capacity_j=%.3f remaining_j=%.6f peak_electrical_w=%.3f overdraft_j=%.9f cutoff=%d stop_drop_m=%.6f\n",fixture_initial_energy,fixture_lift->energy,fixture_lift->peak_electrical,fixture_lift->overdraft,fixture_lift->cutoff,stopDrop);
 state=sim.snapshot();auto deck=sim.kit_body_position(sim.kit_body_index(2973));
 std::printf("PLAYER_PROBE mode=%d player=(%.5f,%.5f,%.5f) support=%llu grounded=%d deck_y=%.6f work_j=%.3f peak_force_n=%.3f max_gap_m=%.6f unsupported_ticks=%d\n",mode,state.player_position.x,state.player_position.y,state.player_position.z,(unsigned long long)state.support_entity_id,state.player_grounded,deck.y,fixture_lift->work,fixture_lift->peak_force,maxGap,unsupported);
 if(fixture_lift->energy<0||fixture_lift->overdraft>0||fixture_lift->peak_electrical>45000||std::abs(stopDrop)>.02)return 4;
 return (mode&&(unsupported>2||maxGap>.05))?3:0;
}
