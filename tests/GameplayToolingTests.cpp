#include "TestAssetHelpers.hpp"
#include "apps/GotMilked/src/GameResources.hpp"

#include "gm/assets/AssetCatalog.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "SceneSerializerExtensions.hpp"
#include "gameplay/QuestTriggerComponent.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#if GM_DEBUG_TOOLS
#include "DebugMenu.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/save/SaveManager.hpp"
#include <imgui.h>
#endif

namespace {

struct SerializerExtensionsGuard {
    SerializerExtensionsGuard() {
        gm::SceneSerializer::ClearComponentSerializers();
        gm::SceneSerializerExtensions::RegisterSerializers();
    }
    ~SerializerExtensionsGuard() {
        gm::SceneSerializerExtensions::UnregisterSerializers();
        gm::SceneSerializer::ClearComponentSerializers();
    }
};

std::filesystem::path MakeTempDir(const std::string& prefix) {
    auto root = std::filesystem::temp_directory_path();
    std::filesystem::path dir;
    std::size_t counter = 0;
    do {
        dir = root / (prefix + "_" + std::to_string(++counter));
    } while (std::filesystem::exists(dir));
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST_CASE("QuestTriggerComponent survives headless scene serialization", "[scene][gameplay]") {
    SerializerExtensionsGuard guard;

    gm::Scene scene("QuestScene");
    auto questObject = scene.CreateGameObject("QuestNPC");
    auto questComponent = questObject->AddComponent<gm::gameplay::QuestTriggerComponent>();
    REQUIRE(questComponent);
    questComponent->SetQuestId("quest_intro");
    questComponent->SetActivationRadius(7.5f);
    questComponent->SetTriggerOnSceneLoad(true);
    questComponent->SetTriggerOnInteract(false);
    questComponent->SetRepeatable(true);
    questComponent->SetActivationAction("Talk");

    const std::string serialized = gm::SceneSerializer::Serialize(scene);
    gm::Scene restored("QuestSceneRestored");
    REQUIRE(gm::SceneSerializer::Deserialize(restored, serialized));

    auto restoredObject = restored.FindGameObjectByName("QuestNPC");
    REQUIRE(restoredObject);
    auto restoredQuest = restoredObject->GetComponent<gm::gameplay::QuestTriggerComponent>();
    REQUIRE(restoredQuest);
    REQUIRE(restoredQuest->GetQuestId() == "quest_intro");
    REQUIRE(restoredQuest->GetActivationRadius() == Catch::Approx(7.5f));
    REQUIRE(restoredQuest->TriggerOnSceneLoad());
    REQUIRE_FALSE(restoredQuest->TriggerOnInteract());
    REQUIRE(restoredQuest->IsRepeatable());
    REQUIRE(restoredQuest->GetActivationAction() == "Talk");
}

TEST_CASE("AssetCatalog emits reload events for content changes", "[assets][catalog][reload]") {
    const auto bundle = CreateMeshSpinnerTestAssets();
    auto& catalog = gm::assets::AssetCatalog::Instance();
    const auto originalRoot = catalog.GetAssetRoot();

    catalog.SetAssetRoot(bundle.root);
    catalog.Scan();

    std::vector<gm::assets::AssetEvent> events;
    const auto listener = catalog.RegisterListener([&](const gm::assets::AssetEvent& event) {
        events.push_back(event);
    });

    const auto prefabDir = bundle.root / "prefabs";
    std::filesystem::create_directories(prefabDir);
    const auto prefabPath = prefabDir / "test.prefab.json";
    {
        std::ofstream out(prefabPath);
        out << "{ \"name\": \"TestPrefab\" }";
    }

    catalog.Scan();
    REQUIRE(std::any_of(events.begin(), events.end(), [&](const auto& event) {
        return event.type == gm::assets::AssetEventType::Added &&
               event.descriptor.relativePath.find("prefabs/test.prefab.json") != std::string::npos;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    {
        std::ofstream out(prefabPath, std::ios::app);
        out << "\n";
    }

    catalog.Scan();
    REQUIRE(std::any_of(events.begin(), events.end(), [&](const auto& event) {
        return event.type == gm::assets::AssetEventType::Updated &&
               event.descriptor.relativePath.find("prefabs/test.prefab.json") != std::string::npos;
    }));

    std::filesystem::remove(prefabPath);
    catalog.Scan();
    REQUIRE(std::any_of(events.begin(), events.end(), [&](const auto& event) {
        return event.type == gm::assets::AssetEventType::Removed &&
               event.descriptor.relativePath.find("prefabs/test.prefab.json") != std::string::npos;
    }));

    catalog.UnregisterListener(listener);
    if (!originalRoot.empty()) {
        catalog.SetAssetRoot(originalRoot);
        catalog.Scan();
    }
    std::filesystem::remove_all(bundle.root);
}

#if GM_DEBUG_TOOLS
TEST_CASE("DebugMenu can run an ImGui smoke session", "[tooling][imgui]") {
    ImGui::CreateContext();
    [[maybe_unused]] ImGuiIO& io = ImGui::GetIO();
#if defined(ImGuiConfigFlags_ViewportsEnable)
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#elif defined(ImGuiConfigFlags_DockingEnable)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    gm::debug::DebugMenu menu;
    gm::save::SaveManager saveManager(MakeTempDir("gm_save_tests"));
    auto scene = std::make_shared<gm::Scene>("ImGuiScene");
    GameResources resources;

    menu.SetSaveManager(&saveManager);
    menu.SetScene(scene);
    menu.SetGameResources(&resources);
    menu.SetLayoutProfilePath(MakeTempDir("gm_layout_tests") / "layout.json");
    menu.SetPluginManifestPath(MakeTempDir("gm_plugins_tests") / "plugins.json");

    bool visible = true;
    ImGui::NewFrame();
    menu.Render(visible);
    ImGui::Render();
    ImGui::DestroyContext();
}

TEST_CASE("EditableTerrainComponent supports editing flows", "[terrain][editing]") {
    gm::Scene scene("TerrainScene");
    auto terrainObject = scene.CreateGameObject("Terrain");
    auto terrainComponent = terrainObject->AddComponent<gm::debug::EditableTerrainComponent>();
    REQUIRE(terrainComponent);

    const int resolution = 4;
    const float size = 8.0f;
    const float minHeight = -1.0f;
    const float maxHeight = 3.5f;
    std::vector<float> heights(resolution * resolution, 1.0f);

    REQUIRE(terrainComponent->SetHeightData(resolution, size, minHeight, maxHeight, heights));
    REQUIRE(terrainComponent->GetResolution() == resolution);
    REQUIRE(terrainComponent->GetTerrainSize() == Catch::Approx(size));
    REQUIRE(terrainComponent->GetMinHeight() == Catch::Approx(minHeight));
    REQUIRE(terrainComponent->GetMaxHeight() == Catch::Approx(maxHeight));

    terrainComponent->SetPaintLayerCount(2);
    std::vector<float> weights(resolution * resolution, 0.5f);
    terrainComponent->SetPaintLayerData(0, "soil", true, weights);
    terrainComponent->SetPaintLayerData(1, "grass", true, weights);
    terrainComponent->SetActivePaintLayerIndex(1);

    REQUIRE(terrainComponent->GetActivePaintLayerIndex() == 1);
    REQUIRE(terrainComponent->GetPaintLayerCount() == 2);
    REQUIRE(terrainComponent->GetPaintLayerWeights(0).size() == weights.size());
}
#endif // GM_DEBUG_TOOLS


