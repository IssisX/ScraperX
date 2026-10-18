cmake_minimum_required(VERSION 3.20)

project(ScraperX VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SCRAPERX_BUILD_BRIDGE "Build the Godot GDExtension bridge" ON)
option(SCRAPERX_WITH_JOLT "Compile the pinned Jolt substrate into the native graph" ON)
option(SCRAPERX_TESTS_ON_ANDROID "Build host-style native tests in Termux" OFF)

if(ANDROID)
    include(cmake/ScraperXAndroidAbi.cmake)
    scraperx_configure_android_cpu("${ANDROID_ABI}")
endif()

set(SCRAPERX_GODOT_CPP_COMMIT "507ed9d840c01a3c5b2a39af8bb4000bfac30bf5" CACHE STRING "Pinned godot-cpp commit")
set(SCRAPERX_JOLT_COMMIT "e77f175595e64cb44218cc9d9d56fc365ad0e36a" CACHE STRING "Pinned Jolt commit")

add_library(scraperx_sim STATIC
    src/sim/simulation.cpp
    src/sim/simulation/affordance.cpp
    src/sim/simulation/carry.cpp
    src/sim/simulation/checkpoint.cpp
    src/sim/simulation/climb.cpp
    src/sim/simulation/fixture_build.cpp
    src/sim/simulation/geometry.cpp
    src/sim/simulation/grapple.cpp
    src/sim/simulation/forklift.cpp
    src/sim/simulation/yard_crane_control.cpp
    src/sim/simulation/hand_transfer.cpp
    src/sim/simulation/legacy_traversal.cpp
    src/sim/simulation/locomotion.cpp
    src/sim/simulation/ground_slide.cpp
    src/sim/simulation/parkour_intent.cpp
    src/sim/simulation/machine_build.cpp
    src/sim/simulation/machine_update.cpp
    src/sim/simulation/queries.cpp
    src/sim/simulation/runtime.cpp
    src/sim/simulation/snapshot.cpp
    src/sim/simulation/support.cpp
    src/sim/simulation/tick.cpp
    src/sim/simulation/world.cpp
    src/sim/simulation/world_geometry.cpp
    src/sim/physical_hand_climb.cpp
    src/sim/physical_foot_push.cpp
    src/sim/traversal_affordance.cpp
    src/sim/traversal_plan.cpp
    src/sim/traversal_receiver.cpp
    src/sim/parkour_route.cpp
    src/sim/west_brace_bay_route.cpp
    src/sim/inspection_junction_route.cpp
    src/sim/north_transfer_route.cpp
    src/sim/pipe_service_route.cpp
    src/sim/split_inspection_route.cpp
    src/sim/grappling_gun.cpp
    src/sim/grapple_practice_route.cpp
    src/sim/deformable_plank.cpp
    src/sim/steam_plant.cpp
    src/sim/water_screw.cpp
    src/sim/mechanism_kit.cpp
    src/sim/facade_route.cpp
    src/sim/cargo_net_route.cpp
    src/sim/cargo_net.cpp
    src/sim/catch_net_route.cpp
    src/sim/suspended_ladder.cpp
    src/sim/service_lift.cpp
    src/sim/warehouse_forklift.cpp
    src/sim/warehouse_forklift_yard.cpp
    src/sim/powered_cable.cpp
    src/sim/yard_crane_geometry.cpp
    src/sim/yard_crane.cpp
    src/sim/gravity_wheel_geometry.cpp
    src/sim/gravity_cart_geometry.cpp
    src/sim/supplied_ascent.cpp
    src/sim/supplied/vertical_machine.cpp
    src/sim/supplied/gravity_balance.cpp
    src/sim/supplied/crown_gondola.cpp
    src/sim/supplied/traction_tram.cpp
    src/sim/supplied/barrel_helix.cpp
    src/sim/supplied/cascade_mast.cpp
    src/sim/supplied/pitman_lift.cpp
    src/sim/slingshot.cpp
    src/sim/slingshot_model.cpp
    src/sim/pipe_bridge.cpp
    src/sim/swing_stair.cpp
    src/sim/upper_ascent.cpp
    src/sim/teeter_rise.cpp
    src/sim/braced_bay.cpp
    src/sim/north_service_frame.cpp
    src/sim/band_counterweight_well.cpp
    src/sim/band_ground_water_lift.cpp
)
target_include_directories(scraperx_sim PUBLIC src)
set_source_files_properties(src/sim/deformable_plank.cpp src/sim/physical_foot_push.cpp src/sim/physical_hand_climb.cpp src/sim/grappling_gun.cpp src/sim/warehouse_forklift.cpp src/sim/powered_cable.cpp src/sim/yard_crane.cpp PROPERTIES COMPILE_OPTIONS
    "$<$<CXX_COMPILER_ID:GNU,Clang>:-fno-rtti>;$<$<CXX_COMPILER_ID:MSVC>:/GR->")
set_target_properties(scraperx_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(scraperx_sim PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
)

if(SCRAPERX_WITH_JOLT)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)

    find_package(Git REQUIRED)
    set(scraperx_jolt_patch_command
        ${CMAKE_COMMAND}
        "-DJOLT_SOURCE=<SOURCE_DIR>"
        "-DJOLT_PATCH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-passive-soft-attachments.patch"
        "-DJOLT_PATCH_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-passive-soft-attachments.hashes.cmake"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyJoltPatch.cmake")
    FetchContent_Declare(
        jolt
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG ${SCRAPERX_JOLT_COMMIT}
        GIT_SHALLOW FALSE
        SOURCE_SUBDIR Build
        PATCH_COMMAND ${scraperx_jolt_patch_command}
    )
    FetchContent_MakeAvailable(jolt)
    # Source overrides bypass population/patch commands. Verify those and
    # every reconfigure as well, before any native compilation can start.
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DJOLT_SOURCE=${jolt_SOURCE_DIR}"
        "-DJOLT_PATCH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-passive-soft-attachments.patch"
        "-DJOLT_PATCH_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-passive-soft-attachments.hashes.cmake"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyJoltPatch.cmake"
        RESULT_VARIABLE scraperx_jolt_patch_result)
    if(NOT scraperx_jolt_patch_result EQUAL 0)
        message(FATAL_ERROR "Pinned Jolt attachment patch verification failed")
    endif()
    # Separate default-off rigid integration extension; the passive soft-body
    # extension keeps its independent pinned receipt and unchanged source.
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DJOLT_SOURCE=${jolt_SOURCE_DIR}"
        "-DJOLT_PATCH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-accurate-rigid-integration.patch"
        "-DJOLT_PATCH_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-accurate-rigid-integration.hashes.cmake"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyJoltPatch.cmake"
        RESULT_VARIABLE scraperx_jolt_rigid_patch_result)
    if(NOT scraperx_jolt_rigid_patch_result EQUAL 0)
        message(FATAL_ERROR "Pinned Jolt accurate rigid integration patch failed")
    endif()
    # Default-off finite slider friction stabilization. The current-substep
    # target is captured after integration/CCD; only the forklift lift opts in.
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DJOLT_SOURCE=${jolt_SOURCE_DIR}"
        "-DJOLT_PATCH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-finite-slider-friction.patch"
        "-DJOLT_PATCH_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/jolt-finite-slider-friction.hashes.cmake"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyJoltPatch.cmake"
        RESULT_VARIABLE scraperx_jolt_slider_patch_result)
    if(NOT scraperx_jolt_slider_patch_result EQUAL 0)
        message(FATAL_ERROR "Pinned Jolt finite slider friction patch failed")
    endif()
    set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(scraperx_sim PUBLIC Jolt)
    target_compile_definitions(scraperx_sim PUBLIC SCRAPERX_HAS_JOLT=1)
endif()

if(BUILD_TESTING AND (NOT ANDROID OR SCRAPERX_TESTS_ON_ANDROID))
    # Exercise the current integration blocker first; every gate still runs once.
    add_executable(scraperx_sim_tests
        tests/simulation_tests.cpp
        tests/simulation/athletic_traversal_tests.cpp
        tests/simulation/counterweight_well_tests.cpp
        tests/simulation/foundation_support_tests.cpp
        tests/simulation/freight_jib_tests.cpp
        tests/simulation/ground_water_tests.cpp
        tests/simulation/hook5_rack_tests.cpp
        tests/simulation/intake_rise_tests.cpp
        tests/simulation/legal_forty_tests.cpp
        tests/simulation/plant_recovery_tests.cpp
        tests/simulation/process_sump_tests.cpp
        tests/simulation/structural_needle_tests.cpp
        tests/simulation/test_utilities.cpp
        tests/simulation/world_crouch_tests.cpp
    )
    target_link_libraries(scraperx_sim_tests PRIVATE scraperx_sim)
    target_compile_options(scraperx_sim_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    )
    add_test(NAME scraperx_sim.athletic_traversal COMMAND scraperx_sim_tests)

    add_executable(scraperx_parkour_intent_flow_tests tests/parkour_intent_flow_tests.cpp)
    target_link_libraries(scraperx_parkour_intent_flow_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.parkour_intent_flow COMMAND scraperx_parkour_intent_flow_tests)

    add_executable(scraperx_parkour_intent_buffer_tests tests/parkour_intent_buffer_tests.cpp)
    target_link_libraries(scraperx_parkour_intent_buffer_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.parkour_intent_buffer COMMAND scraperx_parkour_intent_buffer_tests)

    add_executable(scraperx_ground_yard_tests tests/ground_yard_tests.cpp)
    target_link_libraries(scraperx_ground_yard_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.ground_yard COMMAND scraperx_ground_yard_tests)

    add_executable(scraperx_parkour_yard_route_tests tests/parkour_yard_route_tests.cpp)
    target_link_libraries(scraperx_parkour_yard_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.parkour_yard_route COMMAND scraperx_parkour_yard_route_tests)

    add_executable(scraperx_physical_hand_climb_tests tests/physical_hand_climb_tests.cpp)
    add_executable(scraperx_grappling_gun_tests tests/grappling_gun_tests.cpp)
    target_link_libraries(scraperx_grappling_gun_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.grappling_gun COMMAND scraperx_grappling_gun_tests)
    add_executable(scraperx_grapple_practice_route_tests tests/grapple_practice_route_tests.cpp)
    target_link_libraries(scraperx_grapple_practice_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.grapple_practice_route COMMAND scraperx_grapple_practice_route_tests primary)
    add_test(NAME scraperx_sim.grapple_portable_rail COMMAND scraperx_grapple_practice_route_tests portable-rail)
    add_test(NAME scraperx_sim.grapple_independent407 COMMAND scraperx_grapple_practice_route_tests independent407)
    add_test(NAME scraperx_sim.grapple_practice_recovery COMMAND scraperx_grapple_practice_route_tests miss-retry)
    add_test(NAME scraperx_sim.grapple_failed_retrieval COMMAND scraperx_grapple_practice_route_tests failed-retrieval)
    if(SCRAPERX_WITH_JOLT)
        add_executable(scraperx_second_hand_recovery_tests tests/second_hand_recovery_tests.cpp)
        target_link_libraries(scraperx_second_hand_recovery_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.second_hand_recovery COMMAND scraperx_second_hand_recovery_tests)
        add_executable(scraperx_alternating_shimmy_tests tests/alternating_shimmy_tests.cpp)
        target_link_libraries(scraperx_alternating_shimmy_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.alternating_shimmy COMMAND scraperx_alternating_shimmy_tests)
        add_executable(scraperx_grapple_bay_clearance_tests tests/grapple_bay_clearance_tests.cpp)
        target_link_libraries(scraperx_grapple_bay_clearance_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.grapple_bay_clearance COMMAND scraperx_grapple_bay_clearance_tests)
    endif()
    target_link_libraries(scraperx_physical_hand_climb_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.physical_hand_climb COMMAND scraperx_physical_hand_climb_tests)

    if(SCRAPERX_WITH_JOLT)
        add_executable(scraperx_traversal_affordance_tests tests/traversal_affordance_tests.cpp)
        target_link_libraries(scraperx_traversal_affordance_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.traversal_affordance COMMAND scraperx_traversal_affordance_tests)
        add_executable(scraperx_hand_foot_transfer_tests tests/hand_foot_transfer_tests.cpp)
        target_link_libraries(scraperx_hand_foot_transfer_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.hand_foot_transfer COMMAND scraperx_hand_foot_transfer_tests)
        add_executable(scraperx_physical_topout_tests tests/physical_topout_tests.cpp)
        target_link_libraries(scraperx_physical_topout_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.physical_topout COMMAND scraperx_physical_topout_tests)
        add_test(NAME scraperx_sim.physical_topout_single_step COMMAND scraperx_physical_topout_tests single-step)
    add_test(NAME scraperx_sim.physical_topout_onward_jump COMMAND scraperx_physical_topout_tests jump-buffer)
        add_test(NAME scraperx_sim.physical_topout_onward_run COMMAND scraperx_physical_topout_tests onward-run)
        add_test(NAME scraperx_sim.physical_topout_release COMMAND scraperx_physical_topout_tests release)
        add_test(NAME scraperx_sim.physical_topout_no_candidate COMMAND scraperx_physical_topout_tests no-candidate)
        add_test(NAME scraperx_sim.physical_topout_blocked COMMAND scraperx_physical_topout_tests blocked)
        add_test(NAME scraperx_sim.physical_topout_restart COMMAND scraperx_physical_topout_tests restart)
        add_test(NAME scraperx_sim.physical_topout_frames COMMAND scraperx_physical_topout_tests frames)
        add_executable(scraperx_physical_entry_overlap_tests tests/physical_entry_overlap_tests.cpp)
        target_link_libraries(scraperx_physical_entry_overlap_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.physical_entry_overlap COMMAND scraperx_physical_entry_overlap_tests entry)
        add_test(NAME scraperx_sim.physical_entry_natural COMMAND scraperx_physical_entry_overlap_tests natural)
        add_test(NAME scraperx_sim.physical_entry_release COMMAND scraperx_physical_entry_overlap_tests release)
        add_test(NAME scraperx_sim.physical_entry_expired COMMAND scraperx_physical_entry_overlap_tests expired)
        add_test(NAME scraperx_sim.physical_entry_radial COMMAND scraperx_physical_entry_overlap_tests radial)
        add_executable(scraperx_physical_foot_push_tests tests/physical_foot_push_tests.cpp)
        target_link_libraries(scraperx_physical_foot_push_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.physical_foot_push COMMAND scraperx_physical_foot_push_tests)
        add_executable(scraperx_warehouse_forklift_tests tests/warehouse_forklift_tests.cpp)
        target_link_libraries(scraperx_warehouse_forklift_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.warehouse_forklift COMMAND scraperx_warehouse_forklift_tests)
        add_executable(scraperx_warehouse_forklift_load_tests tests/warehouse_forklift_load_tests.cpp)
        target_link_libraries(scraperx_warehouse_forklift_load_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.warehouse_forklift_load COMMAND scraperx_warehouse_forklift_load_tests)
        add_executable(scraperx_physical_step_intent_tests tests/physical_step_intent_tests.cpp)
        target_link_libraries(scraperx_physical_step_intent_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.physical_step_intent COMMAND scraperx_physical_step_intent_tests)
        add_executable(scraperx_walking_contact_normal_tests tests/walking_contact_normal_tests.cpp)
        target_link_libraries(scraperx_walking_contact_normal_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.walking_contact_normal COMMAND scraperx_walking_contact_normal_tests)
        add_executable(scraperx_physical_walking_step_tests tests/physical_walking_step_tests.cpp)
        target_link_libraries(scraperx_physical_walking_step_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.physical_walking_step COMMAND scraperx_physical_walking_step_tests)
        foreach(mode fast neutral cancel reverse restart)
            add_test(NAME scraperx_sim.physical_walking_step_${mode} COMMAND scraperx_physical_walking_step_tests ${mode})
        endforeach()
        add_executable(scraperx_physical_wall_kick_tests tests/physical_wall_kick_tests.cpp)
        add_executable(scraperx_ground_slide_tests tests/ground_slide_tests.cpp)
        target_link_libraries(scraperx_ground_slide_tests PRIVATE scraperx_sim)
        foreach(mode flow glide brake steer jump guards)
            add_test(NAME scraperx_sim.ground_slide_${mode} COMMAND scraperx_ground_slide_tests ${mode})
        endforeach()
        target_link_libraries(scraperx_physical_wall_kick_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.physical_wall_kick COMMAND scraperx_physical_wall_kick_tests)
        add_test(NAME scraperx_sim.physical_wall_kick_no_wall COMMAND scraperx_physical_wall_kick_tests no-wall)
        add_test(NAME scraperx_sim.physical_wall_kick_expired COMMAND scraperx_physical_wall_kick_tests expired)
        add_test(NAME scraperx_sim.physical_wall_kick_cancel COMMAND scraperx_physical_wall_kick_tests cancel)
        add_test(NAME scraperx_sim.physical_wall_kick_restart COMMAND scraperx_physical_wall_kick_tests restart)
        add_test(NAME scraperx_sim.physical_wall_kick_contacts COMMAND scraperx_physical_wall_kick_tests contacts)
        add_executable(scraperx_forklift_boarding_diagnostic_tests tests/forklift_boarding_diagnostic_tests.cpp)
        target_link_libraries(scraperx_forklift_boarding_diagnostic_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.forklift_boarding_diagnostic COMMAND scraperx_forklift_boarding_diagnostic_tests)
        add_executable(scraperx_freight_yard_arrival_tests tests/freight_yard_arrival_tests.cpp)
        target_link_libraries(scraperx_freight_yard_arrival_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.freight_yard_arrival COMMAND scraperx_freight_yard_arrival_tests)
        add_executable(scraperx_forklift_boarding_tests tests/forklift_boarding_tests.cpp)
        target_link_libraries(scraperx_forklift_boarding_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.forklift_boarding COMMAND scraperx_forklift_boarding_tests)
        add_executable(scraperx_warehouse_forklift_flow_tests tests/warehouse_forklift_flow_tests.cpp)
        target_link_libraries(scraperx_warehouse_forklift_flow_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.warehouse_forklift_flow COMMAND scraperx_warehouse_forklift_flow_tests)
        add_executable(scraperx_warehouse_forklift_route_tests tests/warehouse_forklift_route_tests.cpp)
        target_link_libraries(scraperx_warehouse_forklift_route_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.warehouse_forklift_route COMMAND scraperx_warehouse_forklift_route_tests)
        add_executable(scraperx_forklift_tool_exchange_tests tests/forklift_tool_exchange_tests.cpp)
        target_link_libraries(scraperx_forklift_tool_exchange_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.forklift_tool_exchange COMMAND scraperx_forklift_tool_exchange_tests)
        add_executable(scraperx_forklift_slider_projection_tests tests/forklift_slider_projection_tests.cpp)
        target_link_libraries(scraperx_forklift_slider_projection_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.forklift_slider_projection COMMAND scraperx_forklift_slider_projection_tests)
        add_executable(scraperx_powered_cable_tests tests/powered_cable_tests.cpp)
        target_link_libraries(scraperx_powered_cable_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.powered_cable COMMAND scraperx_powered_cable_tests)
        add_executable(scraperx_yard_crane_route_tests tests/yard_crane_route_tests.cpp)
        target_link_libraries(scraperx_yard_crane_route_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.yard_crane_route COMMAND scraperx_yard_crane_route_tests)
        add_executable(scraperx_yard_crane_rigging_reach_tests tests/yard_crane_rigging_reach_tests.cpp)
        target_link_libraries(scraperx_yard_crane_rigging_reach_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.yard_crane_rigging_reach COMMAND scraperx_yard_crane_rigging_reach_tests)
        add_executable(scraperx_yard_crane_world_tests tests/yard_crane_world_tests.cpp)
        target_link_libraries(scraperx_yard_crane_world_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.yard_crane_world COMMAND scraperx_yard_crane_world_tests)
        add_executable(scraperx_yard_crane_tests tests/yard_crane_tests.cpp)
        target_link_libraries(scraperx_yard_crane_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.yard_crane COMMAND scraperx_yard_crane_tests)
        add_executable(scraperx_wood_foot_push_player_tests tests/wood_foot_push_player_tests.cpp)
        target_link_libraries(scraperx_wood_foot_push_player_tests PRIVATE scraperx_sim)
        add_test(NAME scraperx_sim.wood_foot_push_player COMMAND scraperx_wood_foot_push_player_tests)
    endif()

    add_executable(scraperx_mechanism_hull_tests tests/mechanism_hull_tests.cpp)
    target_link_libraries(scraperx_mechanism_hull_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.mechanism_hull COMMAND scraperx_mechanism_hull_tests)

    add_executable(scraperx_physical_traversal_tests tests/physical_traversal_tests.cpp)
    target_link_libraries(scraperx_physical_traversal_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.physical_traversal COMMAND scraperx_physical_traversal_tests)
    add_test(NAME scraperx_sim.material_grip_approach
             COMMAND scraperx_physical_traversal_tests material_approach)
    add_test(NAME scraperx_sim.single_rigid_hand_catch
             COMMAND scraperx_physical_traversal_tests single_rigid_hand)

    add_executable(scraperx_service_lift_physics_tests tests/service_lift_physics_tests.cpp)
    target_link_libraries(scraperx_service_lift_physics_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.service_lift_physics COMMAND scraperx_service_lift_physics_tests)

    add_executable(scraperx_gravity_reclaim_route_tests tests/gravity_reclaim_route_tests.cpp)
    target_link_libraries(scraperx_gravity_reclaim_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.gravity_reclaim_route COMMAND scraperx_gravity_reclaim_route_tests)

    add_executable(scraperx_gravity_cart_integration_tests tests/gravity_cart_integration_tests.cpp)
    target_link_libraries(scraperx_gravity_cart_integration_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.gravity_cart_integration COMMAND scraperx_gravity_cart_integration_tests)

    add_executable(scraperx_supplied_machine_route_tests tests/supplied_machine_route_tests.cpp)
    target_link_libraries(scraperx_supplied_machine_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.supplied_machine_route COMMAND scraperx_supplied_machine_route_tests)
    add_test(NAME scraperx_sim.shared_hanging_posture COMMAND scraperx_supplied_machine_route_tests parkour_tuck)
    add_test(NAME scraperx_sim.shared_passive_hanging COMMAND scraperx_supplied_machine_route_tests parkour_free_arc)
    add_test(NAME scraperx_sim.shared_thin_edge_catch COMMAND scraperx_supplied_machine_route_tests parkour_thin_edge)
    add_test(NAME scraperx_sim.shared_ledge_hang COMMAND scraperx_supplied_machine_route_tests parkour_ledge_hang)
    add_test(NAME scraperx_sim.shared_rising_ledge_catch COMMAND scraperx_supplied_machine_route_tests parkour_rising_ledge)
    add_test(NAME scraperx_sim.shared_dynamic_grip COMMAND scraperx_supplied_machine_route_tests parkour_dynamic_grip)

    add_executable(scraperx_service_lift_route_tests tests/service_lift_route_tests.cpp)
    target_link_libraries(scraperx_service_lift_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.service_lift_route COMMAND scraperx_service_lift_route_tests)

    add_executable(scraperx_suspended_ladder_tests tests/suspended_ladder_tests.cpp)
    target_link_libraries(scraperx_suspended_ladder_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.suspended_ladder_route COMMAND scraperx_suspended_ladder_tests)
    add_executable(scraperx_cargo_net_tests tests/cargo_net_tests.cpp)
    target_link_libraries(scraperx_cargo_net_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.cargo_net_ground_route COMMAND scraperx_cargo_net_tests)
    add_test(NAME scraperx_sim.material_hanging_posture
             COMMAND scraperx_cargo_net_tests material_posture)
    add_executable(scraperx_catch_net_model_tests tests/catch_net_model_tests.cpp tests/soft_contact_regression.cpp)
    target_link_libraries(scraperx_catch_net_model_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.catch_net_model COMMAND scraperx_catch_net_model_tests)
    add_test(NAME scraperx_sim.catch_net_force_phase COMMAND scraperx_catch_net_model_tests force)
    add_test(NAME scraperx_sim.catch_net_contact_friction COMMAND scraperx_catch_net_model_tests friction)
    add_executable(scraperx_soft_velocity_tests tests/soft_velocity_tests.cpp)
    target_link_libraries(scraperx_soft_velocity_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.soft_velocity COMMAND scraperx_soft_velocity_tests)
    add_executable(scraperx_catch_net_player_tests tests/catch_net_player_tests.cpp)
    target_link_libraries(scraperx_catch_net_player_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.catch_net_player COMMAND scraperx_catch_net_player_tests)

    add_executable(scraperx_leather_pouch_tests tests/leather_pouch_tests.cpp)
    target_link_libraries(scraperx_leather_pouch_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.leather_pouch COMMAND scraperx_leather_pouch_tests)

    add_executable(scraperx_slingshot_model_tests tests/slingshot_model_tests.cpp)
    target_link_libraries(scraperx_slingshot_model_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.slingshot_model COMMAND scraperx_slingshot_model_tests)
    add_executable(scraperx_slingshot_machine_tests tests/slingshot_machine_tests.cpp)
    target_link_libraries(scraperx_slingshot_machine_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.slingshot_machine COMMAND scraperx_slingshot_machine_tests)
    add_executable(scraperx_launcher_tests tests/launcher_tests.cpp)
    target_link_libraries(scraperx_launcher_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.launcher_and_restart COMMAND scraperx_launcher_tests)
    add_executable(scraperx_landing_tests tests/landing_tests.cpp)
    target_link_libraries(scraperx_landing_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.landing_response COMMAND scraperx_landing_tests)
    add_executable(scraperx_landing_flow_tests tests/landing_flow_tests.cpp)
    target_link_libraries(scraperx_landing_flow_tests PRIVATE scraperx_sim)
    foreach(mode flow jump guards)
        add_test(NAME scraperx_sim.landing_flow_${mode} COMMAND scraperx_landing_flow_tests ${mode})
    endforeach()
    add_executable(scraperx_parkour_flow_tests tests/parkour_flow_tests.cpp)
    target_link_libraries(scraperx_parkour_flow_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.parkour_flow COMMAND scraperx_parkour_flow_tests)
    add_executable(scraperx_ascent_route_tests tests/ascent_route_tests.cpp)
    target_link_libraries(scraperx_ascent_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.ascent_route COMMAND scraperx_ascent_route_tests)
    add_test(NAME scraperx_sim.mantle_feel COMMAND scraperx_ascent_route_tests --mantle-feel)
    add_executable(scraperx_swing_stair_route_tests tests/swing_stair_route_tests.cpp)
    target_link_libraries(scraperx_swing_stair_route_tests PRIVATE scraperx_sim)
    foreach(mode RANGE 0 7)
        add_test(NAME scraperx_sim.swing_stair_route_${mode} COMMAND scraperx_swing_stair_route_tests ${mode})
    endforeach()
    add_executable(scraperx_pipe_bridge_tests tests/pipe_bridge_tests.cpp)
    target_link_libraries(scraperx_pipe_bridge_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.pipe_bridge_route COMMAND scraperx_pipe_bridge_tests)
    add_test(NAME scraperx_sim.pipe_bridge_no_input COMMAND scraperx_pipe_bridge_tests 1)
    add_test(NAME scraperx_sim.pipe_bridge_rack_only COMMAND scraperx_pipe_bridge_tests 2)
    add_test(NAME scraperx_sim.pipe_bridge_empty_release COMMAND scraperx_pipe_bridge_tests 3)
    add_test(NAME scraperx_sim.pipe_bridge_regrab COMMAND scraperx_pipe_bridge_tests 4)
    add_test(NAME scraperx_sim.pipe_bridge_passenger COMMAND scraperx_pipe_bridge_tests 5)
    add_executable(scraperx_pipe_bridge_refinement tests/pipe_bridge_refinement.cpp)
    target_link_libraries(scraperx_pipe_bridge_refinement PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.pipe_bridge_refinement COMMAND scraperx_pipe_bridge_refinement)
    add_executable(scraperx_taper_inspection_route_tests tests/taper_inspection_route_tests.cpp)
    target_link_libraries(scraperx_taper_inspection_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.taper_inspection_route COMMAND scraperx_taper_inspection_route_tests)
    add_executable(scraperx_west_brace_bay_route_tests tests/west_brace_bay_route_tests.cpp)
    target_link_libraries(scraperx_west_brace_bay_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.west_brace_bay_route COMMAND scraperx_west_brace_bay_route_tests)
    add_executable(scraperx_inspection_junction_route_tests tests/inspection_junction_route_tests.cpp)
    target_link_libraries(scraperx_inspection_junction_route_tests PRIVATE scraperx_sim)
    add_executable(scraperx_north_transfer_route_tests tests/north_transfer_route_tests.cpp)
    target_link_libraries(scraperx_north_transfer_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.north_transfer_route COMMAND scraperx_north_transfer_route_tests)
    add_test(NAME scraperx_sim.north_transfer_recovery
        COMMAND scraperx_north_transfer_route_tests miss-retry)
    add_executable(scraperx_split_inspection_route_tests tests/split_inspection_route_tests.cpp)
    target_link_libraries(scraperx_split_inspection_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.split_inspection_route COMMAND scraperx_split_inspection_route_tests primary)
    add_test(NAME scraperx_sim.split_inspection_recovery COMMAND scraperx_split_inspection_route_tests miss-retry)
    add_executable(scraperx_rigid_integration_precision_tests tests/rigid_integration_precision_tests.cpp)
    target_link_libraries(scraperx_rigid_integration_precision_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.rigid_integration_precision COMMAND scraperx_rigid_integration_precision_tests)
    add_executable(scraperx_pipe_service_route_tests tests/pipe_service_route_tests.cpp)
    target_link_libraries(scraperx_pipe_service_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.pipe_service_quick COMMAND scraperx_pipe_service_route_tests quick)
    add_test(NAME scraperx_sim.pipe_service_sheltered COMMAND scraperx_pipe_service_route_tests sheltered)
    add_test(NAME scraperx_sim.pipe_service_recovery COMMAND scraperx_pipe_service_route_tests miss-retry)
    add_test(NAME scraperx_sim.inspection_junction_route COMMAND scraperx_inspection_junction_route_tests)
    add_test(NAME scraperx_sim.inspection_junction_recovery
        COMMAND scraperx_inspection_junction_route_tests miss-onward)
    add_test(NAME scraperx_sim.inspection_junction_damage
        COMMAND scraperx_inspection_junction_route_tests damage-onward)
    add_executable(scraperx_deformable_plank_tests tests/deformable_plank_tests.cpp)
    target_link_libraries(scraperx_deformable_plank_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.deformable_plank COMMAND scraperx_deformable_plank_tests)
    add_executable(scraperx_deformable_plank_player_tests tests/deformable_plank_player_tests.cpp)
    target_link_libraries(scraperx_deformable_plank_player_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.deformable_plank_player COMMAND scraperx_deformable_plank_player_tests)
    add_executable(scraperx_teeter_route_tests tests/teeter_route_tests.cpp)
    target_link_libraries(scraperx_teeter_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.teeter_route COMMAND scraperx_teeter_route_tests)
    add_executable(scraperx_teeter_physics_tests tests/teeter_physics_tests.cpp)
    target_link_libraries(scraperx_teeter_physics_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.teeter_physics COMMAND scraperx_teeter_physics_tests)
    add_executable(scraperx_braced_bay_tests tests/braced_bay_tests.cpp)
    target_link_libraries(scraperx_braced_bay_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.braced_bay COMMAND scraperx_braced_bay_tests)
    add_executable(scraperx_north_service_frame_tests tests/north_service_frame_tests.cpp)
    target_link_libraries(scraperx_north_service_frame_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.north_service_frame COMMAND scraperx_north_service_frame_tests)
    add_test(NAME scraperx_sim.north_service_frame_60hz COMMAND scraperx_north_service_frame_tests 60)
    add_test(NAME scraperx_sim.north_service_frame_360hz COMMAND scraperx_north_service_frame_tests 360)
endif()

if(SCRAPERX_BUILD_BRIDGE)
    set(GODOTCPP_API_VERSION "4.7" CACHE STRING "" FORCE)
    set(GODOTCPP_TARGET "template_release" CACHE STRING "" FORCE)
    set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(GODOTCPP_USE_HOT_RELOAD OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        godot_cpp
        GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
        GIT_TAG ${SCRAPERX_GODOT_CPP_COMMIT}
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(godot_cpp)

    add_library(scraperx_native SHARED
        src/bridge/register_types.cpp
        src/bridge/scraperx_simulation.cpp
        src/bridge/yard_crane_bridge.cpp
    )
    target_link_libraries(scraperx_native PRIVATE scraperx_sim godot::cpp)
    target_include_directories(scraperx_native PRIVATE src)
    set_target_properties(scraperx_native PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "scraperx_native"
    )

    if(ANDROID)
        include(cmake/ScraperXAndroidAbi.cmake)
        scraperx_android_extension_directory("${ANDROID_ABI}" SCRAPERX_ANDROID_ARCH)
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_SOURCE_DIR}/godot/addons/scraperx_native/bin/android/${SCRAPERX_ANDROID_ARCH}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_SOURCE_DIR}/godot/addons/scraperx_native/bin/linux")
    else()
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_BINARY_DIR}/bin")
    endif()

    set_target_properties(scraperx_native PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${SCRAPERX_EXTENSION_OUTPUT}"
        RUNTIME_OUTPUT_DIRECTORY "${SCRAPERX_EXTENSION_OUTPUT}"
    )
endif()
