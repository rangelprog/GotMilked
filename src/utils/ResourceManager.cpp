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
#include <future>
#include <mutex>
#include <sstream>
#include <utility>

namespace gm {

namespace {

std::mutex g_registryMutex;
std::shared_ptr<ResourceManager::Registry> g_rootRegistry;
thread_local std::shared_ptr<ResourceManager::Registry> t_scopedRegistry;

std::shared_ptr<ResourceManager::Registry> GetActiveRegistry() {
    if (t_scopedRegistry) {
        return t_scopedRegistry;
    }
    std::lock_guard lock(g_registryMutex);
    if (!g_rootRegistry) {
        g_rootRegistry = std::make_shared<ResourceManager::Registry>();
    }
    return g_rootRegistry;
}

} // namespace

template <typename T>
struct ResourceTraits;

template <>
struct ResourceTraits<Shader> {
    static constexpr const char* Name = "shader";
};

template <>
struct ResourceTraits<Texture> {
    static constexpr const char* Name = "texture";
};

template <>
struct ResourceTraits<Mesh> {
    static constexpr const char* Name = "mesh";
};

template <>
struct ResourceTraits<animation::Skeleton> {
    static constexpr const char* Name = "skeleton";
};

template <>
struct ResourceTraits<animation::AnimationClip> {
    static constexpr const char* Name = "animation";
};

template <>
struct ResourceTraits<animation::SkinnedMeshAsset> {
    static constexpr const char* Name = "skinned_mesh";
};

ResourceManager::Registry::Registry() = default;

ResourceManager::ScopedRegistry::ScopedRegistry(std::shared_ptr<Registry> registry)
    : m_previous(t_scopedRegistry) {
    t_scopedRegistry = std::move(registry);
}

ResourceManager::ScopedRegistry::~ScopedRegistry() {
    t_scopedRegistry = m_previous;
}

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
    {
        std::unique_lock lock(m_notificationMutex);
        m_notificationCallbacks.clear();
    }
    m_nextNotificationId.store(1, std::memory_order_relaxed);
}

template <typename T, typename Loader, typename SwapHook>
ResourceManager::ResourceHandle<T> ResourceManager::Registry::LoadOrStoreResource(
    const std::string& guid,
    Loader&& loader,
    bool reload,
    SwapHook&& swapHook) {
    using Traits = ResourceTraits<T>;
    if (guid.empty()) {
        throw core::ResourceError(Traits::Name, guid, "Descriptor GUID is empty");
    }

    auto slot = GetOrCreateSlot<T>(guid);
    if (!reload) {
        std::shared_lock slotLock(slot->mutex);
        if (slot->resource) {
            return ResourceHandle<T>(guid, slot, weak_from_this());
        }
    }

    auto resource = loader();
    std::shared_ptr<T> previous;
    {
        std::unique_lock slotLock(slot->mutex);
        if (!reload && slot->resource) {
            return ResourceHandle<T>(guid, slot, weak_from_this());
        }
        previous = slot->resource;
        slot->resource = resource;
    }

    if constexpr (!std::is_same_v<std::decay_t<SwapHook>, std::nullptr_t>) {
        if (swapHook) {
            swapHook(previous, resource);
        }
    } else {
        (void)previous;
    }

    NotifyAsync(Traits::Name, guid, reload, true);
    return ResourceHandle<T>(guid, slot, weak_from_this());
}

ResourceManager::ShaderHandle ResourceManager::Registry::LoadShader(const ShaderDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<Shader> {
        auto shader = std::make_shared<Shader>();
        if (!shader->loadFromFiles(descriptor.vertexPath, descriptor.fragmentPath)) {
            std::ostringstream oss;
            oss << "Failed to load shader (" << descriptor.vertexPath << ", " << descriptor.fragmentPath << ")";
            throw core::ResourceError("shader", descriptor.guid, oss.str());
        }
        return shader;
    };

    auto hook = [](const std::shared_ptr<Shader>& previous, const std::shared_ptr<Shader>& current) {
        if (previous) {
            RenderStateCache::InvalidateShader(previous->Id());
        }
        if (current) {
            RenderStateCache::InvalidateShader(current->Id());
        }
    };

    auto handle = LoadOrStoreResource<Shader>(descriptor.guid, loader, false, hook);
    core::Logger::Info("[ResourceManager] Loaded shader '{}' ({}, {})",
                       descriptor.guid, descriptor.vertexPath, descriptor.fragmentPath);
    return handle;
}

ResourceManager::ShaderHandle ResourceManager::Registry::ReloadShader(const ShaderDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<Shader> {
        auto shader = std::make_shared<Shader>();
        if (!shader->loadFromFiles(descriptor.vertexPath, descriptor.fragmentPath)) {
            std::ostringstream oss;
            oss << "Failed to reload shader (" << descriptor.vertexPath << ", " << descriptor.fragmentPath << ")";
            throw core::ResourceError("shader", descriptor.guid, oss.str());
        }
        return shader;
    };

    auto hook = [](const std::shared_ptr<Shader>& previous, const std::shared_ptr<Shader>& current) {
        if (previous) {
            RenderStateCache::InvalidateShader(previous->Id());
        }
        if (current) {
            RenderStateCache::InvalidateShader(current->Id());
        }
    };

    auto handle = LoadOrStoreResource<Shader>(descriptor.guid, loader, true, hook);
    core::Logger::Info("[ResourceManager] Reloaded shader '{}' ({}, {})",
                       descriptor.guid, descriptor.vertexPath, descriptor.fragmentPath);
    return handle;
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
    auto loader = [&]() -> std::shared_ptr<Texture> {
        try {
            return std::make_shared<Texture>(Texture::loadOrThrow(descriptor.path));
        } catch (const core::GraphicsError& err) {
            throw core::ResourceError("texture", descriptor.guid, std::string(err.what()));
        } catch (const std::exception& ex) {
            throw core::ResourceError("texture", descriptor.guid, ex.what());
        }
    };

    auto hook = [](const std::shared_ptr<Texture>& previous, const std::shared_ptr<Texture>& current) {
        if (previous) {
            RenderStateCache::InvalidateTexture(previous->id());
        }
        if (current) {
            RenderStateCache::InvalidateTexture(current->id());
        }
    };

    auto handle = LoadOrStoreResource<Texture>(descriptor.guid, loader, false, hook);
    core::Logger::Info("[ResourceManager] Loaded texture '{}' ({})",
                       descriptor.guid, descriptor.path);
    return handle;
}

ResourceManager::TextureHandle ResourceManager::Registry::ReloadTexture(const TextureDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<Texture> {
        try {
            return std::make_shared<Texture>(Texture::loadOrThrow(descriptor.path));
        } catch (const core::GraphicsError& err) {
            throw core::ResourceError("texture", descriptor.guid, std::string(err.what()));
        } catch (const std::exception& ex) {
            throw core::ResourceError("texture", descriptor.guid, ex.what());
        }
    };

    auto hook = [](const std::shared_ptr<Texture>& previous, const std::shared_ptr<Texture>& current) {
        if (previous) {
            RenderStateCache::InvalidateTexture(previous->id());
        }
        if (current) {
            RenderStateCache::InvalidateTexture(current->id());
        }
    };

    auto handle = LoadOrStoreResource<Texture>(descriptor.guid, loader, true, hook);
    core::Logger::Info("[ResourceManager] Reloaded texture '{}' ({})",
                       descriptor.guid, descriptor.path);
    return handle;
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
    auto loader = [&]() -> std::shared_ptr<Mesh> {
        try {
            return std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(descriptor.path));
        } catch (const core::ResourceError&) {
            throw;
        } catch (const std::exception& ex) {
            throw core::ResourceError("mesh", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<Mesh>(descriptor.guid, loader, false, nullptr);
    core::Logger::Info("[ResourceManager] Loaded mesh '{}' ({})",
                       descriptor.guid, descriptor.path);
    return handle;
}

ResourceManager::MeshHandle ResourceManager::Registry::ReloadMesh(const MeshDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<Mesh> {
        try {
            return std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(descriptor.path));
        } catch (const core::ResourceError&) {
            throw;
        } catch (const std::exception& ex) {
            throw core::ResourceError("mesh", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<Mesh>(descriptor.guid, loader, true, nullptr);
    core::Logger::Info("[ResourceManager] Reloaded mesh '{}' ({})",
                       descriptor.guid, descriptor.path);
    return handle;
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
    auto loader = [&]() -> std::shared_ptr<animation::Skeleton> {
        try {
            return std::make_shared<animation::Skeleton>(animation::Skeleton::FromFile(descriptor.path));
        } catch (const std::exception& ex) {
            throw core::ResourceError("skeleton", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<animation::Skeleton>(descriptor.guid, loader, false, nullptr);
    core::Logger::Info("[ResourceManager] Loaded skeleton '{}' ({})", descriptor.guid, descriptor.path);
    return handle;
}

ResourceManager::SkeletonHandle ResourceManager::Registry::ReloadSkeleton(const SkeletonDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<animation::Skeleton> {
        try {
            return std::make_shared<animation::Skeleton>(animation::Skeleton::FromFile(descriptor.path));
        } catch (const std::exception& ex) {
            throw core::ResourceError("skeleton", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<animation::Skeleton>(descriptor.guid, loader, true, nullptr);
    core::Logger::Info("[ResourceManager] Reloaded skeleton '{}' ({})", descriptor.guid, descriptor.path);
    return handle;
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
    auto loader = [&]() -> std::shared_ptr<animation::AnimationClip> {
        try {
            return std::make_shared<animation::AnimationClip>(animation::AnimationClip::FromFile(descriptor.path));
        } catch (const std::exception& ex) {
            throw core::ResourceError("animation", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<animation::AnimationClip>(descriptor.guid, loader, false, nullptr);
    core::Logger::Info("[ResourceManager] Loaded animation '{}' ({})", descriptor.guid, descriptor.path);
    return handle;
}

ResourceManager::AnimationClipHandle ResourceManager::Registry::ReloadAnimationClip(const AnimationClipDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<animation::AnimationClip> {
        try {
            return std::make_shared<animation::AnimationClip>(animation::AnimationClip::FromFile(descriptor.path));
        } catch (const std::exception& ex) {
            throw core::ResourceError("animation", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<animation::AnimationClip>(descriptor.guid, loader, true, nullptr);
    core::Logger::Info("[ResourceManager] Reloaded animation '{}' ({})", descriptor.guid, descriptor.path);
    return handle;
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
    auto loader = [&]() -> std::shared_ptr<animation::SkinnedMeshAsset> {
        try {
            return std::make_shared<animation::SkinnedMeshAsset>(animation::SkinnedMeshAsset::FromFile(descriptor.path));
        } catch (const std::exception& ex) {
            throw core::ResourceError("skinned_mesh", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<animation::SkinnedMeshAsset>(descriptor.guid, loader, false, nullptr);
    core::Logger::Info("[ResourceManager] Loaded skinned mesh '{}' ({})", descriptor.guid, descriptor.path);
    return handle;
}

ResourceManager::SkinnedMeshHandle ResourceManager::Registry::ReloadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    auto loader = [&]() -> std::shared_ptr<animation::SkinnedMeshAsset> {
        try {
            return std::make_shared<animation::SkinnedMeshAsset>(animation::SkinnedMeshAsset::FromFile(descriptor.path));
        } catch (const std::exception& ex) {
            throw core::ResourceError("skinned_mesh", descriptor.guid, ex.what());
        }
    };

    auto handle = LoadOrStoreResource<animation::SkinnedMeshAsset>(descriptor.guid, loader, true, nullptr);
    core::Logger::Info("[ResourceManager] Reloaded skinned mesh '{}' ({})", descriptor.guid, descriptor.path);
    return handle;
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

ResourceManager::Registry::NotificationToken
ResourceManager::Registry::RegisterNotificationCallback(NotificationCallback callback) {
    if (!callback) {
        return {};
    }
    const auto id = m_nextNotificationId.fetch_add(1, std::memory_order_relaxed);
    {
        std::unique_lock lock(m_notificationMutex);
        m_notificationCallbacks[id] = std::move(callback);
    }
    return NotificationToken{id};
}

void ResourceManager::Registry::UnregisterNotificationCallback(NotificationToken token) {
    if (!token) {
        return;
    }
    std::unique_lock lock(m_notificationMutex);
    m_notificationCallbacks.erase(token.value);
}

void ResourceManager::Registry::NotifyAsync(std::string_view type,
                                            const std::string& guid,
                                            bool reloaded,
                                            bool success) const {
    std::vector<NotificationCallback> callbacks;
    {
        std::shared_lock lock(m_notificationMutex);
        callbacks.reserve(m_notificationCallbacks.size());
        for (const auto& [id, cb] : m_notificationCallbacks) {
            if (cb) {
                callbacks.push_back(cb);
            }
        }
    }

    if (callbacks.empty()) {
        return;
    }

    ResourceNotification notification{std::string(type), guid, reloaded, success};
    std::async(std::launch::async, [callbacks = std::move(callbacks), notification]() {
        for (const auto& cb : callbacks) {
            if (cb) {
                cb(notification);
            }
        }
    });
}

void ResourceManager::Init(std::shared_ptr<Registry> registry) {
    std::lock_guard lock(g_registryMutex);
    if (!registry) {
        registry = std::make_shared<Registry>();
    }
    g_rootRegistry = std::move(registry);
    RenderStateCache::Reset();
}

void ResourceManager::SetRegistry(std::shared_ptr<Registry> registry) {
    std::lock_guard lock(g_registryMutex);
    g_rootRegistry = std::move(registry);
}

std::shared_ptr<ResourceManager::Registry> ResourceManager::GetRegistry() {
    return GetActiveRegistry();
}

void ResourceManager::Cleanup() {
    std::lock_guard lock(g_registryMutex);
    if (g_rootRegistry) {
        g_rootRegistry->Reset();
    }
    g_rootRegistry.reset();
    RenderStateCache::Reset();
}

ResourceManager::ShaderHandle ResourceManager::LoadShader(const ShaderDescriptor& descriptor) {
    return GetActiveRegistry()->LoadShader(descriptor);
}

ResourceManager::ShaderHandle ResourceManager::ReloadShader(const ShaderDescriptor& descriptor) {
    return GetActiveRegistry()->ReloadShader(descriptor);
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& guid) {
    return GetActiveRegistry()->GetShader(guid);
}

bool ResourceManager::HasShader(const std::string& guid) {
    return GetActiveRegistry()->HasShader(guid);
}

ResourceManager::TextureHandle ResourceManager::LoadTexture(const TextureDescriptor& descriptor) {
    return GetActiveRegistry()->LoadTexture(descriptor);
}

ResourceManager::TextureHandle ResourceManager::ReloadTexture(const TextureDescriptor& descriptor) {
    return GetActiveRegistry()->ReloadTexture(descriptor);
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& guid) {
    return GetActiveRegistry()->GetTexture(guid);
}

bool ResourceManager::HasTexture(const std::string& guid) {
    return GetActiveRegistry()->HasTexture(guid);
}

ResourceManager::MeshHandle ResourceManager::LoadMesh(const MeshDescriptor& descriptor) {
    return GetActiveRegistry()->LoadMesh(descriptor);
}

ResourceManager::MeshHandle ResourceManager::ReloadMesh(const MeshDescriptor& descriptor) {
    return GetActiveRegistry()->ReloadMesh(descriptor);
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(const std::string& guid) {
    return GetActiveRegistry()->GetMesh(guid);
}

bool ResourceManager::HasMesh(const std::string& guid) {
    return GetActiveRegistry()->HasMesh(guid);
}

ResourceManager::SkeletonHandle ResourceManager::LoadSkeleton(const SkeletonDescriptor& descriptor) {
    return GetActiveRegistry()->LoadSkeleton(descriptor);
}

ResourceManager::SkeletonHandle ResourceManager::ReloadSkeleton(const SkeletonDescriptor& descriptor) {
    return GetActiveRegistry()->ReloadSkeleton(descriptor);
}

std::shared_ptr<animation::Skeleton> ResourceManager::GetSkeleton(const std::string& guid) {
    return GetActiveRegistry()->GetSkeleton(guid);
}

bool ResourceManager::HasSkeleton(const std::string& guid) {
    return GetActiveRegistry()->HasSkeleton(guid);
}

ResourceManager::AnimationClipHandle ResourceManager::LoadAnimationClip(const AnimationClipDescriptor& descriptor) {
    return GetActiveRegistry()->LoadAnimationClip(descriptor);
}

ResourceManager::AnimationClipHandle ResourceManager::ReloadAnimationClip(const AnimationClipDescriptor& descriptor) {
    return GetActiveRegistry()->ReloadAnimationClip(descriptor);
}

std::shared_ptr<animation::AnimationClip> ResourceManager::GetAnimationClip(const std::string& guid) {
    return GetActiveRegistry()->GetAnimationClip(guid);
}

bool ResourceManager::HasAnimationClip(const std::string& guid) {
    return GetActiveRegistry()->HasAnimationClip(guid);
}

ResourceManager::SkinnedMeshHandle ResourceManager::LoadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    return GetActiveRegistry()->LoadSkinnedMesh(descriptor);
}

ResourceManager::SkinnedMeshHandle ResourceManager::ReloadSkinnedMesh(const SkinnedMeshDescriptor& descriptor) {
    return GetActiveRegistry()->ReloadSkinnedMesh(descriptor);
}

std::shared_ptr<animation::SkinnedMeshAsset> ResourceManager::GetSkinnedMesh(const std::string& guid) {
    return GetActiveRegistry()->GetSkinnedMesh(guid);
}

bool ResourceManager::HasSkinnedMesh(const std::string& guid) {
    return GetActiveRegistry()->HasSkinnedMesh(guid);
}

ResourceManager::NotificationHandle ResourceManager::RegisterNotificationCallback(NotificationCallback callback) {
    auto registry = GetActiveRegistry();
    auto token = registry->RegisterNotificationCallback(std::move(callback));
    return NotificationHandle{registry, token};
}

void ResourceManager::UnregisterNotificationCallback(NotificationHandle& handle) {
    if (!handle) {
        return;
    }
    if (auto registry = handle.registry.lock()) {
        registry->UnregisterNotificationCallback(handle.token);
    }
    handle.registry.reset();
    handle.token = {};
}

} // namespace gm