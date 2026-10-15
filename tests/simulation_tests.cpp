#include "sim_checks.hpp"
#include "sim_routes.hpp"

int main() {
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "ascent") {
        run_ascent();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "pitman") {
        using scraperx::sim::InitialSpawn;
        using scraperx::sim::Simulation;
        Simulation floor(InitialSpawn::CasingGallery);
        require(floor.advance_frame(0.4).accepted, "pitman settle must be accepted");
        require(climb_pitman(floor), "the pitman over the gap north of the gallery");
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "wheel") {
        using scraperx::sim::InitialSpawn;
        using scraperx::sim::Simulation;
        Simulation floor(InitialSpawn::PitmanDeck);
        require(floor.advance_frame(0.4).accepted, "wheel settle must be accepted");
        require(climb_wheel_ladder(floor), "up the wheel's ladder and back down, while it is still held");
        require(climb_wheel(floor), "onto the wheel, then up to the next deck");
        Simulation missed(InitialSpawn::PitmanDeck);
        require(missed.advance_frame(0.4).accepted, "wheel miss settle must be accepted");
        require(miss_wheel(missed), "thrown from the pad, then up the ladder");
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "brace") {
        using scraperx::sim::InitialSpawn;
        using scraperx::sim::Simulation;
        Simulation floor(InitialSpawn::WheelDeck);
        require(floor.advance_frame(0.4).accepted, "brace settle must be accepted");
        require(climb_brace(floor), "up the brace bay onto the next deck");
        Simulation missed(InitialSpawn::BracePerch);
        require(missed.advance_frame(0.4).accepted, "brace miss settle must be accepted");
        require(miss_brace(missed), "off the perch, then back up the shelf ladder");
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "girder") {
        run_girder();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "skin") {
        run_service_skin();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007") {
        run_wet_isolation();
        run_wet_band();
        run_wet_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007-route") {
        run_wet_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007-band") {
        run_wet_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008") {
        run_plate_shop();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-band") {
        run_plate_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-route") {
        run_plate_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-wreck") {
        run_plate_wreckage();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009") {
        run_facade_crane();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-band") {
        run_crane_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-route") {
        run_crane_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-wreck") {
        run_crane_wreckage();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "stack") {
        run_stack();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "ascent") {
        run_ascent();
        return EXIT_SUCCESS;
    }

    run_proving_ground();
    run_campaign();
    run_stack();
    run_wet_isolation();
    run_wet_band();
    run_wet_route();
    run_plate_shop();
    run_plate_band();
    run_plate_route();
    run_plate_wreckage();
    run_facade_crane();
    run_crane_band();
    run_crane_route();
    run_crane_wreckage();
    run_ascent();
    run_service_skin();
    run_girder();

    return EXIT_SUCCESS;
}
