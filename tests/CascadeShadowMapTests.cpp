#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "gm/rendering/CascadeShadowMap.hpp"

TEST_CASE("CascadeShadowMap responds to sun elevation bands", "[rendering][shadows]") {
    gm::rendering::CascadeShadowSettings settings;
    settings.cascadeCount = 3;
    gm::rendering::CascadeShadowMap cascades;
    cascades.SetSettings(settings);

    const glm::mat4 identityView = glm::mat4(1.0f);
    const glm::mat4 testProj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 200.0f);

    cascades.Update(identityView, testProj, 0.1f, 200.0f, glm::vec3(0.0f, -1.0f, 0.0f), 60.0f);
    REQUIRE(cascades.CascadeMatrices().size() == 3);
    REQUIRE(cascades.CascadeSplits().size() == 3);
    REQUIRE(cascades.ActiveSplitLambda() == Catch::Approx(0.60f).margin(0.02f));

    cascades.Update(identityView, testProj, 0.1f, 200.0f, glm::vec3(0.0f, -1.0f, 0.0f), -30.0f);
    REQUIRE(cascades.ActiveSplitLambda() == Catch::Approx(0.92f).margin(0.02f));

    const auto& splits = cascades.CascadeSplits();
    REQUIRE(splits[0] > 0.0f);
    REQUIRE(splits[0] < splits[1]);
    REQUIRE(splits[1] < splits[2]);
    REQUIRE(splits[2] <= 1.0f);
}


