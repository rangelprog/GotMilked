/**
 * Enhanced Scene Management System - Usage Guide
 * 
 * This document demonstrates how to use the improved scene management system
 * for organizing and managing GameObjects in your game.
 */

// ============================================================================
// 1. BASIC SCENE CREATION AND SETUP
// ============================================================================

/*
#include "gm/scene/SceneManager.hpp"
#include "gm/scene/GameObject.hpp"

// Create and load a scene
auto& sceneManager = gm::SceneManager::Instance();
auto gameScene = sceneManager.CreateScene("MainGame");
sceneManager.SetActiveScene("MainGame");

// In your initialization
sceneManager.InitActiveScene();
*/

// ============================================================================
// 2. SPAWNING AND MANAGING GAME OBJECTS
// ============================================================================

/*
// Get the active scene
auto scene = sceneManager.GetActiveScene();

// Create a new GameObject
auto player = scene->CreateGameObject("Player");
player->SetLayer(0);  // Layer 0 for characters

// Create and immediately initialize
auto enemy = scene->SpawnGameObject("Enemy");
enemy->SetLayer(1);   // Layer 1 for enemies

// Add components to GameObject
auto transformComponent = player->AddComponent<TransformComponent>();
auto meshComponent = player->AddComponent<MeshComponent>();
auto healthComponent = player->AddComponent<HealthComponent>();

// Tag GameObjects for easy lookup
scene->TagGameObject(player, "player");
scene->TagGameObject(enemy, "enemy");
*/

// ============================================================================
// 3. QUERYING GAME OBJECTS
// ============================================================================

/*
// Find by name (fast)
auto player = scene->FindGameObjectByName("Player");

// Find by tag (efficient for groups)
auto allEnemies = scene->FindGameObjectsByTag("enemy");
for (auto& enemy : allEnemies) {
    // Process each enemy
    enemy->SetActive(false);
}

// Find by layer
auto layerZeroObjects = scene->GetAllGameObjects();
std::vector<std::shared_ptr<GameObject>> charactersLayer;
for (auto& obj : layerZeroObjects) {
    if (obj->GetLayer() == 0) {
        charactersLayer.push_back(obj);
    }
}

// Check if tag exists
auto allEnemies = scene->FindGameObjectsByTag("enemy");
if (!allEnemies.empty()) {
    printf("Found %zu enemies\n", allEnemies.size());
}
*/

// ============================================================================
// 4. OBJECT LIFECYCLE
// ============================================================================

/*
// Activate/deactivate without destroying
auto enemy = scene->FindGameObjectByName("Enemy");
enemy->SetActive(false);  // Won't update or render

// Mark for destruction (removed at end of frame)
enemy->Destroy();

// The scene automatically cleans up destroyed objects

// Scene pause functionality
auto scene = sceneManager.GetActiveScene();
scene->SetPaused(true);   // Stops all updates
scene->SetPaused(false);  // Resume updates
*/

// ============================================================================
// 5. MULTI-SCENE MANAGEMENT
// ============================================================================

/*
auto& sceneManager = gm::SceneManager::Instance();

// Create multiple scenes
auto mainScene = sceneManager.CreateScene("Main");
auto menuScene = sceneManager.CreateScene("Menu");
auto levelScene = sceneManager.CreateScene("Level1");

// Switch scenes
sceneManager.SetActiveScene("Menu");
// ... menu logic ...
sceneManager.SetActiveScene("Level1");

// Check if scene exists
if (sceneManager.HasScene("Level2")) {
    sceneManager.SetActiveScene("Level2");
} else {
    auto level2 = sceneManager.CreateScene("Level2");
    sceneManager.SetActiveScene("Level2");
}

// Unload scene when done
sceneManager.UnloadScene("Menu");

// Clean shutdown
sceneManager.Shutdown();
*/

// ============================================================================
// 6. COMPONENT ACCESS FROM SCENE OBJECTS
// ============================================================================

/*
auto scene = sceneManager.GetActiveScene();
auto player = scene->FindGameObjectByName("Player");

// Get component
auto health = player->GetComponent<HealthComponent>();
if (health) {
    health->TakeDamage(10);
}

// Iterate all components
for (auto& component : player->GetComponents()) {
    component->Update(deltaTime);
}
*/

// ============================================================================
// 7. TAG-BASED GAMEPLAY PATTERNS
// ============================================================================

/*
// Damage all enemies
auto allEnemies = scene->FindGameObjectsByTag("enemy");
for (auto& enemy : allEnemies) {
    auto health = enemy->GetComponent<HealthComponent>();
    if (health) {
        health->TakeDamage(5);
    }
}

// Find interactive objects
auto interactables = scene->FindGameObjectsByTag("interactable");
for (auto& obj : interactables) {
    auto interactor = obj->GetComponent<InteractableComponent>();
    if (interactor && IsPlayerNear(obj->GetPosition())) {
        interactor->OnPlayerNear();
    }
}

// Collect all projectiles
auto projectiles = scene->FindGameObjectsByTag("projectile");
for (auto& proj : projectiles) {
    if (proj->IsDestroyed()) {
        // Already marked for deletion, scene will clean it up
        continue;
    }
}
*/

// ============================================================================
// 8. SCENE UPDATE IN GAME LOOP
// ============================================================================

/*
// In your main game loop:

float deltaTime = 0.016f; // ~60 FPS

while (!glfwWindowShouldClose(window)) {
    // ... input, timing, etc ...

    // Update scene (updates all active GameObjects)
    sceneManager.UpdateActiveScene(deltaTime);

    // Render
    auto scene = sceneManager.GetActiveScene();
    if (scene) {
        scene->Draw(shader, camera, fbw, fbh, fov);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
}

// Cleanup
sceneManager.Shutdown();
*/

// ============================================================================
// 9. PRACTICAL EXAMPLE: ENEMY SPAWNER
// ============================================================================

/*
class EnemySpawner {
private:
    std::string sceneName;
    float spawnTimer = 0.0f;
    float spawnInterval = 2.0f;
    int maxEnemies = 10;

public:
    void Update(float deltaTime) {
        auto scene = gm::SceneManager::Instance().GetScene(sceneName);
        if (!scene) return;

        // Check enemy count
        auto enemies = scene->FindGameObjectsByTag("enemy");
        if (enemies.size() >= maxEnemies) return;

        // Spawn timer
        spawnTimer += deltaTime;
        if (spawnTimer >= spawnInterval) {
            SpawnEnemy(scene);
            spawnTimer = 0.0f;
        }

        // Clean up dead enemies
        for (auto& enemy : enemies) {
            auto health = enemy->GetComponent<HealthComponent>();
            if (health && health->IsDead()) {
                scene->DestroyGameObject(enemy);
            }
        }
    }

    void SpawnEnemy(std::shared_ptr<gm::Scene> scene) {
        static int enemyCount = 0;
        auto enemyName = "Enemy_" + std::to_string(enemyCount++);
        
        auto enemy = scene->SpawnGameObject(enemyName);
        enemy->SetLayer(1);  // Enemy layer
        scene->TagGameObject(enemy, "enemy");
        
        // Setup components
        auto health = enemy->AddComponent<HealthComponent>();
        health->SetMaxHealth(100);
        
        auto ai = enemy->AddComponent<AIComponent>();
        ai->SetBehavior(AIBehavior::Patrol);
    }
};
*/

// ============================================================================
// 10. SCENE COMPOSITION PATTERN
// ============================================================================

/*
class GameLevel {
private:
    std::string levelName;

public:
    void Load() {
        auto& sceneManager = gm::SceneManager::Instance();
        auto scene = sceneManager.LoadScene(levelName);
        
        // Spawn player
        auto player = scene->SpawnGameObject("Player");
        scene->TagGameObject(player, "player");
        // ... setup player ...

        // Spawn enemies
        for (int i = 0; i < 5; ++i) {
            auto enemy = scene->SpawnGameObject("Enemy_" + std::to_string(i));
            scene->TagGameObject(enemy, "enemy");
            // ... setup enemy ...
        }

        // Spawn collectibles
        for (int i = 0; i < 10; ++i) {
            auto collectible = scene->SpawnGameObject("Coin_" + std::to_string(i));
            scene->TagGameObject(collectible, "collectible");
            // ... setup collectible ...
        }

        sceneManager.InitActiveScene();
    }

    void Unload() {
        auto& sceneManager = gm::SceneManager::Instance();
        sceneManager.UnloadScene(levelName);
    }
};
*/
