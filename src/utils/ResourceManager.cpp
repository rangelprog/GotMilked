#include "gm/utils/ResourceManager.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/RenderStateCache.hpp"
#include "gm/animation/Skeleton.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/SkinnedMeshAsset.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/core/Error.hpp"
#include "gm/core/Logger.hpp"

#include <exception>
#include <mutex>
#include <sstream>

namespace gm {

namespace {

std::mutex g_registryMutex;
std::shared_ptr<ResourceManager::Registry> g_registry;

std::shared_ptr<ResourceManager::Registry> EnsureRegistry() {
    std::lock_guard lock(g_registryMutex);
    if (!g_registry) {
        g_registry = std::make_shared<ResourceManager::Registry>();
    }
    return g_registry;
}

} // namespace

ResourceManager::Registry::Registry() = default;

void ResourceManager::Registry::Reset() {
    {
        std::unique_lock lock(m_shaderCache.mutex);
        m_shaderCache.slots.clear();
    }
    {
        std::unique_lock lock(m_textureCache.mutex);
        m_textureCache.slots.clear();
    }
    {
        std::unique_lock lock(m_meshCache.mutex);
        m_meshCache.slots.clear();
    }
    {
        std::unique_lock lock(m_skeletonCache.mutex);
        m_skeletonCache.slots.clear();
    }
    {
        std::unique_lock lock(m_animationClipCache.mutex);
        m_animationClipCache.slots.clear();
    }
    {
        std::unique_lock lock(m_skinnedMeshCache.mutex);
        m_skinnedMeshCache.slots.clear();
    }
}

ResourceManager::ShaderHandle ResourceManager::Registry::LoadShader(const ShaderDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("shader", descriptor.guid, "Shader descriptor GUID is empty");
    }

    if (auto slot = FindSlot<Shader>(descriptor.guid)) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return ShaderHandle(descriptor.guid, slot, weak_from_this());
        }
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(descriptor.vertexPath, descriptor.fragmentPath)) {
        std::ostringstream oss;
        oss << "Failed to load shader (" << descriptor.vertexPath << ", " << descriptor.fragmentPath << ")";
        throw core::ResourceError("shader", descriptor.guid, oss.str());
    }

    auto slot = GetOrCreateSlot<Shader>(descriptor.guid);
    {
        std::unique_lock slotLock(slot->mutex);
        slot->resource = shader;
    }

    core::Logger::Info("[ResourceManager] Loaded shader '{}' ({}, {})",
                       descriptor.guid, descriptor.vertexPath, descriptor.fragmentPath);
    return ShaderHandle(descriptor.guid, slot, weak_from_this());
}

ResourceManager::ShaderHandle ResourceManager::Registry::ReloadShader(const ShaderDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("shader", descriptor.guid, "Shader descriptor GUID is empty");
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(descriptor.vertexPath, descriptor.fragmentPath)) {
        std::ostringstream oss;
        oss << "Failed to reload shader (" << descriptor.vertexPath << ", " << descriptor.fragmentPath << ")";
        throw core::ResourceError("shader", descriptor.guid, oss.str());
    }

    auto slot = GetOrCreateSlot<Shader>(descriptor.guid);
    std::shared_ptr<Shader> previous;
    {
        std::unique_lock slotLock(slot->mutex);
        previous = slot->resource;
        slot->resource = shader;
    }

    if (previous) {
        RenderStateCache::InvalidateShader(previous->Id());
    }
    RenderStateCache::InvalidateShader(shader->Id());

    core::Logger::Info("[ResourceManager] Reloaded shader '{}' ({}, {})",
                       descriptor.guid, descriptor.vertexPath, descriptor.fragmentPath);
    return ShaderHandle(descriptor.guid, slot, weak_from_this());
}

std::shared_ptr<Shader> ResourceManager::Registry::GetShader(const std::string& guid) const {
    if (auto slot = FindSlot<Shader>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return slot->resource;
    }
    return nullptr;
}

bool ResourceManager::Registry::HasShader(const std::string& guid) const {
    if (auto slot = FindSlot<Shader>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return static_cast<bool>(slot->resource);
    }
    return false;
}

ResourceManager::TextureHandle ResourceManager::Registry::LoadTexture(const TextureDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("texture", descriptor.guid, "Texture descriptor GUID is empty");
    }

    if (auto slot = FindSlot<Texture>(descriptor.guid)) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return TextureHandle(descriptor.guid, slot, weak_from_this());
        }
    }

    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrThrow(descriptor.path));
        auto slot = GetOrCreateSlot<Texture>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = texture;
        }
        core::Logger::Info("[ResourceManager] Loaded texture '{}' ({})",
                           descriptor.guid, descriptor.path);
        return TextureHandle(descriptor.guid, slot, weak_from_this());
    } catch (const core::GraphicsError& err) {
        throw core::ResourceError("texture", descriptor.guid, std::string(err.what()));
    } catch (const std::exception& ex) {
        throw core::ResourceError("texture", descriptor.guid, ex.what());
    }
}

ResourceManager::TextureHandle ResourceManager::Registry::ReloadTexture(const TextureDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("texture", descriptor.guid, "Texture descriptor GUID is empty");
    }

    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrThrow(descriptor.path));
        auto slot = GetOrCreateSlot<Texture>(descriptor.guid);
        std::shared_ptr<Texture> previous;
        {
            std::unique_lock slotLock(slot->mutex);
            previous = slot->resource;
            slot->resource = texture;
        }
        if (previous) {
            RenderStateCache::InvalidateTexture(previous->id());
        }
        RenderStateCache::InvalidateTexture(texture->id());
        core::Logger::Info("[ResourceManager] Reloaded texture '{}' ({})",
                           descriptor.guid, descriptor.path);
        return TextureHandle(descriptor.guid, slot, weak_from_this());
    } catch (const core::GraphicsError& err) {
        throw core::ResourceError("texture", descriptor.guid, std::string(err.what()));
    } catch (const std::exception& ex) {
        throw core::ResourceError("texture", descriptor.guid, ex.what());
    }
}

std::shared_ptr<Texture> ResourceManager::Registry::GetTexture(const std::string& guid) const {
    if (auto slot = FindSlot<Texture>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return slot->resource;
    }
    return nullptr;
}

bool ResourceManager::Registry::HasTexture(const std::string& guid) const {
    if (auto slot = FindSlot<Texture>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return static_cast<bool>(slot->resource);
    }
    return false;
}

ResourceManager::MeshHandle ResourceManager::Registry::LoadMesh(const MeshDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("mesh", descriptor.guid, "Mesh descriptor GUID is empty");
    }

    if (auto slot = FindSlot<Mesh>(descriptor.guid)) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return MeshHandle(descriptor.guid, slot, weak_from_this());
        }
    }

    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(descriptor.path));
        auto slot = GetOrCreateSlot<Mesh>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = mesh;
        }
        core::Logger::Info("[ResourceManager] Loaded mesh '{}' ({})",
                           descriptor.guid, descriptor.path);
        return MeshHandle(descriptor.guid, slot, weak_from_this());
    } catch (const core::ResourceError&) {
        throw;
    } catch (const std::exception& ex) {
        throw core::ResourceError("mesh", descriptor.guid, ex.what());
    }
}

ResourceManager::MeshHandle ResourceManager::Registry::ReloadMesh(const MeshDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("mesh", descriptor.guid, "Mesh descriptor GUID is empty");
    }

    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(descriptor.path));
        auto slot = GetOrCreateSlot<Mesh>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = mesh;
        }
        core::Logger::Info("[ResourceManager] Reloaded mesh '{}' ({})",
                           descriptor.guid, descriptor.path);
        return MeshHandle(descriptor.guid, slot, weak_from_this());
    } catch (const core::ResourceError&) {
        throw;
    } catch (const std::exception& ex) {
        throw core::ResourceError("mesh", descriptor.guid, ex.what());
    }
}

std::shared_ptr<Mesh> ResourceManager::Registry::GetMesh(const std::string& guid) const {
    if (auto slot = FindSlot<Mesh>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return slot->resource;
    }
    return nullptr;
}

bool ResourceManager::Registry::HasMesh(const std::string& guid) const {
    if (auto slot = FindSlot<Mesh>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return static_cast<bool>(slot->resource);
    }
    return false;
}

ResourceManager::SkeletonHandle ResourceManager::Registry::LoadSkeleton(const SkeletonDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("skeleton", descriptor.guid, "Skeleton descriptor GUID is empty");
    }

    if (auto slot = FindSlot<animation::Skeleton>(descriptor.guid)) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return SkeletonHandle(descriptor.guid, slot, weak_from_this());
        }
    }

    try {
        auto skeleton = std::make_shared<animation::Skeleton>(animation::Skeleton::FromFile(descriptor.path));
        auto slot = GetOrCreateSlot<animation::Skeleton>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = skeleton;
        }
        core::Logger::Info("[ResourceManager] Loaded skeleton '{}' ({})", descriptor.guid, descriptor.path);
        return SkeletonHandle(descriptor.guid, slot, weak_from_this());
    } catch (const std::exception& ex) {
        throw core::ResourceError("skeleton", descriptor.guid, ex.what());
    }
}

ResourceManager::SkeletonHandle ResourceManager::Registry::ReloadSkeleton(const SkeletonDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("skeleton", descriptor.guid, "Skeleton descriptor GUID is empty");
    }

    try {
        auto skeleton = std::make_shared<animation::Skeleton>(animation::Skeleton::FromFile(descriptor.path));
        auto slot = GetOrCreateSlot<animation::Skeleton>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = skeleton;
        }
        core::Logger::Info("[ResourceManager] Reloaded skeleton '{}' ({})", descriptor.guid, descriptor.path);
        return SkeletonHandle(descriptor.guid, slot, weak_from_this());
    } catch (const std::exception& ex) {
        throw core::ResourceError("skeleton", descriptor.guid, ex.what());
    }
}

std::shared_ptr<animation::Skeleton> ResourceManager::Registry::GetSkeleton(const std::string& guid) const {
    if (auto slot = FindSlot<animation::Skeleton>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return slot->resource;
    }
    return nullptr;
}

bool ResourceManager::Registry::HasSkeleton(const std::string& guid) const {
    if (auto slot = FindSlot<animation::Skeleton>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return static_cast<bool>(slot->resource);
    }
    return false;
}

ResourceManager::AnimationClipHandle ResourceManager::Registry::LoadAnimationClip(const AnimationClipDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("animation", descriptor.guid, "Animation clip descriptor GUID is empty");
    }

    if (auto slot = FindSlot<animation::AnimationClip>(descriptor.guid)) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return AnimationClipHandle(descriptor.guid, slot, weak_from_this());
        }
    }

    try {
        auto clip = std::make_shared<animation::AnimationClip>(animation::AnimationClip::FromFile(descriptor.path));
        auto slot = GetOrCreateSlot<animation::AnimationClip>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = clip;
        }
        core::Logger::Info("[ResourceManager] Loaded animation '{}' ({})", descriptor.guid, descriptor.path);
        return AnimationClipHandle(descriptor.guid, slot, weak_from_this());
    } catch (const std::exception& ex) {
        throw core::ResourceError("animation", descriptor.guid, ex.what());
    }
}

ResourceManager::AnimationClipHandle ResourceManager::Registry::ReloadAnimationClip(const AnimationClipDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("animation", descriptor.guid, "Animation clip descriptor GUID is empty");
    }

    try {
        auto clip = std::make_shared<animation::AnimationClip>(animation::AnimationClip::FromFile(descriptor.path));
        auto slot = GetOrCreateSlot<animation::AnimationClip>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = clip;
        }
        core::Logger::Info("[ResourceManager] Reloaded animation '{}' ({})", descriptor.guid, descriptor.path);
        return AnimationClipHandle(descriptor.guid, slot, weak_from_this());
    } catch (const std::exception& ex) {
        throw core::ResourceError("animation", descriptor.guid, ex.what());
    }
}

std::shared_ptr<animation::AnimationClip> ResourceManager::Registry::GetAnimationClip(const std::string& guid) const {
    if (auto slot = FindSlot<animation::AnimationClip>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return slot->resource;
    }
    return nullptr;
}

bool ResourceManager::Registry::HasAnimationClip(const std::string& guid) const {
    if (auto slot = FindSlot<animation::AnimationClip>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return static_cast<bool>(slot->resource);
    }
    return false;
}

ResourceManager::SkinnedMeshHandle ResourceManager::Registry::LoadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("skinned_mesh", descriptor.guid, "Skinned mesh descriptor GUID is empty");
    }

    if (auto slot = FindSlot<animation::SkinnedMeshAsset>(descriptor.guid)) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return SkinnedMeshHandle(descriptor.guid, slot, weak_from_this());
        }
    }

    try {
        auto mesh = std::make_shared<animation::SkinnedMeshAsset>(animation::SkinnedMeshAsset::FromFile(descriptor.path));
        auto slot = GetOrCreateSlot<animation::SkinnedMeshAsset>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = mesh;
        }
        core::Logger::Info("[ResourceManager] Loaded skinned mesh '{}' ({})", descriptor.guid, descriptor.path);
        return SkinnedMeshHandle(descriptor.guid, slot, weak_from_this());
    } catch (const std::exception& ex) {
        throw core::ResourceError("skinned_mesh", descriptor.guid, ex.what());
    }
}

ResourceManager::SkinnedMeshHandle ResourceManager::Registry::ReloadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("skinned_mesh", descriptor.guid, "Skinned mesh descriptor GUID is empty");
    }

    try {
        auto mesh = std::make_shared<animation::SkinnedMeshAsset>(animation::SkinnedMeshAsset::FromFile(descriptor.path));
        auto slot = GetOrCreateSlot<animation::SkinnedMeshAsset>(descriptor.guid);
        {
            std::unique_lock slotLock(slot->mutex);
            slot->resource = mesh;
        }
        core::Logger::Info("[ResourceManager] Reloaded skinned mesh '{}' ({})", descriptor.guid, descriptor.path);
        return SkinnedMeshHandle(descriptor.guid, slot, weak_from_this());
    } catch (const std::exception& ex) {
        throw core::ResourceError("skinned_mesh", descriptor.guid, ex.what());
    }
}

std::shared_ptr<animation::SkinnedMeshAsset> ResourceManager::Registry::GetSkinnedMesh(const std::string& guid) const {
    if (auto slot = FindSlot<animation::SkinnedMeshAsset>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return slot->resource;
    }
    return nullptr;
}

bool ResourceManager::Registry::HasSkinnedMesh(const std::string& guid) const {
    if (auto slot = FindSlot<animation::SkinnedMeshAsset>(guid)) {
        std::shared_lock slotLock(slot->mutex);
        return static_cast<bool>(slot->resource);
    }
    return false;
}

void ResourceManager::Init(std::shared_ptr<Registry> registry) {
    std::lock_guard lock(g_registryMutex);
    if (!registry) {
        registry = std::make_shared<Registry>();
    }
    g_registry = std::move(registry);
    RenderStateCache::Reset();
}

void ResourceManager::SetRegistry(std::shared_ptr<Registry> registry) {
    std::lock_guard lock(g_registryMutex);
    g_registry = std::move(registry);
}

std::shared_ptr<ResourceManager::Registry> ResourceManager::GetRegistry() {
    std::lock_guard lock(g_registryMutex);
    return g_registry;
}

void ResourceManager::Cleanup() {
    std::lock_guard lock(g_registryMutex);
    if (g_registry) {
        g_registry->Reset();
    }
    g_registry.reset();
    RenderStateCache::Reset();
}

ResourceManager::ShaderHandle ResourceManager::LoadShader(const ShaderDescriptor& descriptor) {
    return EnsureRegistry()->LoadShader(descriptor);
}

ResourceManager::ShaderHandle ResourceManager::ReloadShader(const ShaderDescriptor& descriptor) {
    return EnsureRegistry()->ReloadShader(descriptor);
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& guid) {
    return EnsureRegistry()->GetShader(guid);
}

bool ResourceManager::HasShader(const std::string& guid) {
    return EnsureRegistry()->HasShader(guid);
}

ResourceManager::TextureHandle ResourceManager::LoadTexture(const TextureDescriptor& descriptor) {
    return EnsureRegistry()->LoadTexture(descriptor);
}

ResourceManager::TextureHandle ResourceManager::ReloadTexture(const TextureDescriptor& descriptor) {
    return EnsureRegistry()->ReloadTexture(descriptor);
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& guid) {
    return EnsureRegistry()->GetTexture(guid);
}

bool ResourceManager::HasTexture(const std::string& guid) {
    return EnsureRegistry()->HasTexture(guid);
}

ResourceManager::MeshHandle ResourceManager::LoadMesh(const MeshDescriptor& descriptor) {
    return EnsureRegistry()->LoadMesh(descriptor);
}

ResourceManager::MeshHandle ResourceManager::ReloadMesh(const MeshDescriptor& descriptor) {
    return EnsureRegistry()->ReloadMesh(descriptor);
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(const std::string& guid) {
    return EnsureRegistry()->GetMesh(guid);
}

bool ResourceManager::HasMesh(const std::string& guid) {
    return EnsureRegistry()->HasMesh(guid);
}

ResourceManager::SkeletonHandle ResourceManager::LoadSkeleton(const SkeletonDescriptor& descriptor) {
    return EnsureRegistry()->LoadSkeleton(descriptor);
}

ResourceManager::SkeletonHandle ResourceManager::ReloadSkeleton(const SkeletonDescriptor& descriptor) {
    return EnsureRegistry()->ReloadSkeleton(descriptor);
}

std::shared_ptr<animation::Skeleton> ResourceManager::GetSkeleton(const std::string& guid) {
    return EnsureRegistry()->GetSkeleton(guid);
}

bool ResourceManager::HasSkeleton(const std::string& guid) {
    return EnsureRegistry()->HasSkeleton(guid);
}

ResourceManager::AnimationClipHandle ResourceManager::LoadAnimationClip(const AnimationClipDescriptor& descriptor) {
    return EnsureRegistry()->LoadAnimationClip(descriptor);
}

ResourceManager::AnimationClipHandle ResourceManager::ReloadAnimationClip(const AnimationClipDescriptor& descriptor) {
    return EnsureRegistry()->ReloadAnimationClip(descriptor);
}

std::shared_ptr<animation::AnimationClip> ResourceManager::GetAnimationClip(const std::string& guid) {
    return EnsureRegistry()->GetAnimationClip(guid);
}

bool ResourceManager::HasAnimationClip(const std::string& guid) {
    return EnsureRegistry()->HasAnimationClip(guid);
}

ResourceManager::SkinnedMeshHandle ResourceManager::LoadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    return EnsureRegistry()->LoadSkinnedMesh(descriptor);
}

ResourceManager::SkinnedMeshHandle ResourceManager::ReloadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    return EnsureRegistry()->ReloadSkinnedMesh(descriptor);
}

std::shared_ptr<animation::SkinnedMeshAsset> ResourceManager::GetSkinnedMesh(const std::string& guid) {
    return EnsureRegistry()->GetSkinnedMesh(guid);
}

bool ResourceManager::HasSkinnedMesh(const std::string& guid) {
    return EnsureRegistry()->HasSkinnedMesh(guid);
}

} // namespace gm