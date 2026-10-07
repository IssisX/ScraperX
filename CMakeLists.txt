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

set(SCRAPERX_GODOT_CPP_COMMIT "507ed9d840c01a3c5b2a39af8bb4000bfac30bf5" CACHE STRING "Pinned godot-cpp commit")
set(SCRAPERX_JOLT_COMMIT "e77f175595e64cb44218cc9d9d56fc365ad0e36a" CACHE STRING "Pinned Jolt commit")

add_library(scraperx_sim STATIC
    src/sim/simulation.cpp
    src/sim/physical_hand_climb.cpp
    src/sim/steam_plant.cpp
    src/sim/water_screw.cpp
    src/sim/mechanism_kit.cpp
    src/sim/facade_route.cpp
    src/sim/cargo_net_route.cpp
    src/sim/cargo_net.cpp
    src/sim/suspended_ladder.cpp
    src/sim/service_lift.cpp
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

    FetchContent_Declare(
        jolt
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG ${SCRAPERX_JOLT_COMMIT}
        GIT_SHALLOW FALSE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(jolt)
    set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(scraperx_sim PUBLIC Jolt)
    target_compile_definitions(scraperx_sim PUBLIC SCRAPERX_HAS_JOLT=1)
endif()

if(BUILD_TESTING AND (NOT ANDROID OR SCRAPERX_TESTS_ON_ANDROID))
    add_executable(scraperx_physical_hand_climb_tests tests/physical_hand_climb_tests.cpp)
    target_link_libraries(scraperx_physical_hand_climb_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.physical_hand_climb COMMAND scraperx_physical_hand_climb_tests)

    add_executable(scraperx_physical_traversal_tests tests/physical_traversal_tests.cpp)
    target_link_libraries(scraperx_physical_traversal_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.physical_traversal COMMAND scraperx_physical_traversal_tests)

    add_executable(scraperx_service_lift_physics_tests tests/service_lift_physics_tests.cpp)
    target_link_libraries(scraperx_service_lift_physics_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.service_lift_physics COMMAND scraperx_service_lift_physics_tests)

    add_executable(scraperx_service_lift_route_tests tests/service_lift_route_tests.cpp)
    target_link_libraries(scraperx_service_lift_route_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.service_lift_route COMMAND scraperx_service_lift_route_tests)

    add_executable(scraperx_suspended_ladder_tests tests/suspended_ladder_tests.cpp)
    target_link_libraries(scraperx_suspended_ladder_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.suspended_ladder_route COMMAND scraperx_suspended_ladder_tests)
    add_executable(scraperx_cargo_net_tests tests/cargo_net_tests.cpp)
    target_link_libraries(scraperx_cargo_net_tests PRIVATE scraperx_sim)
    add_test(NAME scraperx_sim.cargo_net_ground_route COMMAND scraperx_cargo_net_tests)
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
    add_executable(scraperx_sim_tests tests/simulation_tests.cpp)
    target_link_libraries(scraperx_sim_tests PRIVATE scraperx_sim)
    target_compile_options(scraperx_sim_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    )
    add_test(NAME scraperx_sim.athletic_traversal COMMAND scraperx_sim_tests)
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
    )
    target_link_libraries(scraperx_native PRIVATE scraperx_sim godot::cpp)
    target_include_directories(scraperx_native PRIVATE src)
    set_target_properties(scraperx_native PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "scraperx_native"
    )

    if(ANDROID)
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_SOURCE_DIR}/godot/addons/scraperx_native/bin/android/arm64")
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
