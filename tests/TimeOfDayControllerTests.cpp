#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "gm/scene/TimeOfDayController.hpp"

TEST_CASE("TimeOfDayController normalizes time and evaluates directions", "[scene][time_of_day]") {
    gm::scene::TimeOfDayController controller;
    gm::scene::CelestialConfig config;
    config.latitudeDeg = 45.0f;
    config.axialTiltDeg = 23.0f;
    config.dayLengthSeconds = 120.0f;
    controller.SetConfig(config);

    controller.SetTimeSeconds(0.0f);
    auto midnight = controller.Evaluate();

    controller.SetTimeSeconds(60.0f);
    auto noon = controller.Evaluate();

    REQUIRE(noon.sunDirection.y > midnight.sunDirection.y);
    REQUIRE(noon.sunIntensity > midnight.sunIntensity);

    controller.Advance(120.0f);
    REQUIRE(controller.GetNormalizedTime() == Catch::Approx(0.5f));
}

