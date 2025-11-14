#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/AnimationPose.hpp"
#include "gm/animation/AnimationPoseEvaluator.hpp"
#include "gm/animation/Skeleton.hpp"
#include "gm/animation/SkinnedMeshAsset.hpp"
#include "gm/scene/AnimatorComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/PrefabLibrary.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SkinnedMeshComponent.hpp"
#include "SceneSerializerExtensions.hpp"
#include "GameResources.hpp"
#include "gm/utils/ResourceManager.hpp"

#include <filesystem>
#include <stdexcept>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {

std::filesystem::path CowModelsDir() {
    return std::filesystem::path(GM_ASSETS_DIR) / "models" / "cow";
}

void EnsureSerializersRegistered() {
    static bool registered = [] {
        gm::SceneSerializerExtensions::RegisterSerializers();
        return true;
    }();
    (void)registered;
}

class GlfwContext {
public:
    GlfwContext() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_window = glfwCreateWindow(64, 64, "AnimationPipelineTests", nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwMakeContextCurrent(m_window);
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
            throw std::runtime_error("Failed to load GLAD");
        }
    }

    ~GlfwContext() {
        if (m_window) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
    }

private:
    GLFWwindow* m_window = nullptr;
};

struct ResourceManagerGuard {
    ResourceManagerGuard() { gm::ResourceManager::Init(); }
    ~ResourceManagerGuard() { gm::ResourceManager::Cleanup(); }
};

} // namespace

TEST_CASE("Cow animation assets load for regression coverage", "[animation][importer]") {
    const auto modelsDir = CowModelsDir();

    const auto skeleton = gm::animation::Skeleton::FromFile((modelsDir / "Cow.gmskel").string());
    REQUIRE_FALSE(skeleton.bones.empty());
    REQUIRE(skeleton.bones.front().parentIndex == -1);

    const auto skinnedMesh = gm::animation::SkinnedMeshAsset::FromFile((modelsDir / "Cow.gmskin").string());
    REQUIRE_FALSE(skinnedMesh.vertices.empty());
    REQUIRE_FALSE(skinnedMesh.indices.empty());
    REQUIRE(skinnedMesh.boneNames.size() == skeleton.bones.size());

    const auto idleClip = gm::animation::AnimationClip::FromFile((modelsDir / "Cow_idle.gmanim").string());
    REQUIRE(idleClip.duration > 0.0);
    REQUIRE(idleClip.channels.size() > 0);
    REQUIRE(idleClip.ticksPerSecond > 0.0);
}

TEST_CASE("AnimationPoseEvaluator blends layered clips", "[animation][pose]") {
    gm::animation::Skeleton skeleton;
    skeleton.name = "TestSkeleton";
    skeleton.bones.push_back({"root", -1, glm::mat4(1.0f)});
    skeleton.bones.push_back({"child", 0, glm::mat4(1.0f)});

    auto makeChannel = [](int boneIndex, const glm::vec3& a, const glm::vec3& b) {
        gm::animation::AnimationClip::Channel channel;
        channel.boneIndex = boneIndex;
        channel.translationKeys.push_back({0.0, a});
        channel.translationKeys.push_back({1.0, b});
        channel.rotationKeys.push_back({0.0, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});
        channel.scaleKeys.push_back({0.0, glm::vec3(1.0f)});
        return channel;
    };

    gm::animation::AnimationClip clipA;
    clipA.name = "ClipA";
    clipA.duration = 1.0;
    clipA.ticksPerSecond = 1.0;
    clipA.channels.push_back(makeChannel(1, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

    gm::animation::AnimationClip clipB;
    clipB.name = "ClipB";
    clipB.duration = 1.0;
    clipB.ticksPerSecond = 1.0;
    clipB.channels.push_back(makeChannel(1, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 3.0f)));

    gm::animation::AnimationPoseEvaluator evaluator(skeleton);
    gm::animation::AnimationPose pose(skeleton.bones.size());

    std::vector<gm::animation::AnimationLayer> layers = {
        {&clipA, 0.5, 0.3f},
        {&clipB, 0.5, 0.7f}
    };

    evaluator.EvaluateLayers(layers, pose);
    const auto& childTransform = pose.LocalTransform(1);

    // Weighted average at half-way point: ClipA (0.5,0,0), ClipB (0,0,1.5)
    REQUIRE(childTransform.translation.x == Catch::Approx(0.15f));
    REQUIRE(childTransform.translation.z == Catch::Approx(1.05f));
    REQUIRE(childTransform.translation.y == Catch::Approx(0.0f));
}

TEST_CASE("AnimatorComponent builds skinning palette from clips", "[animation][gpu]") {
    gm::animation::Skeleton skeleton;
    skeleton.name = "TestSkeleton";
    skeleton.bones.push_back({"root", -1, glm::mat4(1.0f)});
    skeleton.bones.push_back({"child", 0, glm::mat4(1.0f)});

    auto clip = std::make_shared<gm::animation::AnimationClip>();
    clip->name = "MoveX";
    clip->duration = 1.0;
    clip->ticksPerSecond = 1.0;

    gm::animation::AnimationClip::Channel channel;
    channel.boneIndex = 1;
    channel.translationKeys.push_back({0.0, glm::vec3(0.0f)});
    channel.translationKeys.push_back({1.0, glm::vec3(2.0f, 0.0f, 0.0f)});
    channel.rotationKeys.push_back({0.0, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});
    channel.scaleKeys.push_back({0.0, glm::vec3(1.0f)});
    clip->channels.push_back(channel);

    gm::scene::AnimatorComponent animator;
    gm::GameObject owner("AnimatorTest");
    owner.EnsureTransform();
    animator.SetOwner(&owner);
    animator.Init();

    auto skeletonPtr = std::make_shared<gm::animation::Skeleton>(skeleton);
    animator.SetSkeleton(std::move(skeletonPtr), "testSkeleton");
    animator.SetClip("Base", clip, "clip_guid");
    animator.Play("Base", true);

    animator.Update(0.5f);

    std::vector<glm::mat4> palette;
    REQUIRE(animator.GetSkinningPalette(palette));
    REQUIRE(palette.size() == skeleton.bones.size());

    const glm::vec3 childTranslation = glm::vec3(palette[1][3]);
    REQUIRE(childTranslation.x == Catch::Approx(1.0f));
    REQUIRE(childTranslation.y == Catch::Approx(0.0f));
    REQUIRE(childTranslation.z == Catch::Approx(0.0f));
}

TEST_CASE("Skinned material loads with shader override", "[animation][resources]") {
    GlfwContext glContext;
    ResourceManagerGuard guard;
    GameResources resources;
    REQUIRE(resources.Load(std::filesystem::path(GM_ASSETS_DIR)));
    auto material = resources.GetMaterial("cow_mat0");
    REQUIRE(material);

    auto overrideGuid = resources.GetMaterialShaderOverride("cow_mat0");
    REQUIRE(overrideGuid.has_value());
    REQUIRE(*overrideGuid == "shader::simple_skinned");

    resources.Release();
}

TEST_CASE("Cow prefab instantiates skinned mesh and animator", "[animation][prefab]") {
    EnsureSerializersRegistered();

    gm::scene::PrefabLibrary library;
    std::filesystem::path prefabDir = std::filesystem::path(GM_ASSETS_DIR) / "prefabs";
    REQUIRE(library.LoadDirectory(prefabDir));

    gm::Scene scene("PrefabTest");
    auto instances = library.Instantiate("Cow", scene);
    REQUIRE_FALSE(instances.empty());

    auto cow = instances.front();
    REQUIRE(cow);

    auto skinned = cow->GetComponent<gm::scene::SkinnedMeshComponent>();
    REQUIRE(skinned);
    REQUIRE(skinned->MeshGuid() == "5921fbb494a68f0b");
    REQUIRE(skinned->MaterialGuid() == "cow_mat0");
    REQUIRE(skinned->ShaderGuid() == "shader::simple_skinned");

    auto animator = cow->GetComponent<gm::scene::AnimatorComponent>();
    REQUIRE(animator);
    REQUIRE(animator->SkeletonGuid() == "58f8fdb494838d5d");
    auto layers = animator->GetLayerSnapshots();
    REQUIRE(layers.size() >= 1);
    REQUIRE(layers.front().clipGuid == "d5ad897412497a46");
}

