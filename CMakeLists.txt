cmake_minimum_required(VERSION 3.20)

project(ScraperX VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SCRAPERX_BUILD_BRIDGE "Build the Godot GDExtension bridge" ON)
set(SCRAPERX_GODOT_CPP_COMMIT "507ed9d840c01a3c5b2a39af8bb4000bfac30bf5" CACHE STRING "Pinned godot-cpp commit")

add_library(scraperx_sim STATIC
    src/sim/ground_stage.cpp
    src/sim/water_screw.cpp
)
target_include_directories(scraperx_sim PUBLIC src)
set_target_properties(scraperx_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(scraperx_sim PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(scraperx_sim_tests tests/ground_screw_tests.cpp)
    target_link_libraries(scraperx_sim_tests PRIVATE scraperx_sim)
    target_compile_options(scraperx_sim_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    )
    add_test(NAME scraperx_sim.ground_screw_ascent COMMAND scraperx_sim_tests)
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
        src/bridge/scraperx_screw.cpp
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
