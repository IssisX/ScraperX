# Frozen 9280 input fixtures

23 scenarios, 21 passed, 2 failed. Each executed once in a private imported project with isolated userdata. ARM Godot4.7/bridge proof; no x86, rendering, Actions, APK or phone claim.

| Scenario | Result / exit | Native receipt or actual error | Log |
| --- | --- | --- | --- |
| touch_jump | PASS / 0 | vy_peak=5.39 | [touch_jump.log](touch_jump.log) |
| touch_move_look | FAIL / 31 | touch overshoot did not produce native sprint | [touch_move_look.log](touch_move_look.log) |
| touch_gyro_aim | PASS / 0 | turned_rad=0.500 raised_rad=0.200 walk_alignment=1.0000 | [touch_gyro_aim.log](touch_gyro_aim.log) |
| touch_climb | PASS / 0 | state=2 rise_m=1.58 | [touch_climb.log](touch_climb.log) |
| touch_vault | PASS / 0 | x_after=6.08 | [touch_vault.log](touch_vault.log) |
| touch_double_tap_vault | PASS / 0 | x_after=6.08 | [touch_double_tap_vault.log](touch_double_tap_vault.log) |
| touch_crouch | PASS / 0 | blocked_z=-12.38 passed_z=-14.25 eye_crouched=0.95 eye_standing=1.52 | [touch_crouch.log](touch_crouch.log) |
| touch_hang_drop | PASS / 0 | vy_after_release=-0.08 grip_error_m=0.0000 head_grip_error_m=0.0000 | [touch_hang_drop.log](touch_hang_drop.log) |
| touch_hang_climb | PASS / 0 | top_y=4.52 grip_error_m=0.0000 head_grip_error_m=0.0000 | [touch_hang_climb.log](touch_hang_climb.log) |
| touch_chute | PASS / 0 | offered_at_mps=6.54 impact_mps=8.89 | [touch_chute.log](touch_chute.log) |
| touch_lethal_feedback | PASS / 0 | toast='FELL AT 34.7 M/S  /  RESTORED TO CHECKPOINT +0.9 M' lethal_mps=20.0 | [touch_lethal_feedback.log](touch_lethal_feedback.log) |
| touch_sump | PASS / 0 | isolated false->true | [touch_sump.log](touch_sump.log) |
| touch_pendant | PASS / 0 | boom_delta_rad=-0.081 hook_delta_m=0.721 | [touch_pendant.log](touch_pendant.log) |
| touch_carry | FAIL / 31 | PICK UP did not put the hook block on the native carry point | [touch_carry.log](touch_carry.log) |
| touch_water_screw | PASS / 0 | rpm=2.2 torque_nm=1500 tank_m3=0.000 | [touch_water_screw.log](touch_water_screw.log) |
| touch_water_lift | PASS / 0 | valve_open=1 bucket_m3=0.000 cage_m=0.000 | [touch_water_lift.log](touch_water_lift.log) |
| touch_pause | PASS / 0 | frozen_tick=25 advanced_to=39 | [touch_pause.log](touch_pause.log) |
| pad_core | PASS / 0 | forward_m=3.71 turned_rad=1.848 | [pad_core.log](pad_core.log) |
| pad_pendant | PASS / 0 | boom_delta_rad=-0.069 hook_delta_m=0.921 | [pad_pendant.log](pad_pendant.log) |
| keyboard_core | PASS / 0 | forward_m=3.71 rejected=0->1 | [keyboard_core.log](keyboard_core.log) |
| touch_rig | PASS / 0 | top_y=177.13 worst_wrist_step_m=0.074 | [touch_rig.log](touch_rig.log) |
| pad_rig | PASS / 0 | top_y=177.13 worst_wrist_step_m=0.074 | [pad_rig.log](pad_rig.log) |
| keyboard_rig | PASS / 0 | top_y=177.13 worst_wrist_step_m=0.075 | [keyboard_rig.log](keyboard_rig.log) |

Source/bridge hashes and command details: source-manifest.json, completion.json, results.json. Both failures have unresolved causes at baseline; no fixture or native repair was performed.
