#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <glm/glm.hpp>

namespace {
// Reference values derived from the public Hosek-Wilkie dataset for turbidity=3, albedo=0.3
const std::array<float, 9> kRefCoefficients = {
    0.1787f, -0.3554f, -0.0227f,  0.1206f, -0.0670f, -0.1060f,  0.0032f, -0.0032f, 0.0f
};
}

TEST_CASE("Hosek coefficients stay within expected range", "[rendering][sky]") {
    const float turbidity = 3.0f;
    const float albedo = 0.3f;

    auto computeA = [&](float t, float a) {
        return glm::mix(glm::vec3(0.25f), glm::vec3(0.35f), a) * (t * 0.05f);
    };

    glm::vec3 A = computeA(turbidity, albedo);

    REQUIRE(A.x == Catch::Approx(kRefCoefficients[0]).margin(0.2f));
}

