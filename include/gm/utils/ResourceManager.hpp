#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <type_traits>

namespace gm {

class Shader;
class Texture;
class Mesh;

class ResourceManager {
public:
    class Registry;

    template <typename T>
    struct ResourceSlot {
        mutable std::shared_mutex mutex;
        std::shared_ptr<T> resource;
        std::atomic<std::uint32_t> refCount{0};
    };

    template <typename T>
    class ResourceHandle {
    public:
        ResourceHandle() = default;
        ResourceHandle(const ResourceHandle& other);
        ResourceHandle& operator=(const ResourceHandle& other);
        ResourceHandle(ResourceHandle&& other) noexcept;
        ResourceHandle& operator=(ResourceHandle&& other) noexcept;
        ~ResourceHandle();

        const std::string& Guid() const { return m_guid; }
        bool IsValid() const { return !m_guid.empty(); }
        bool IsLoaded() const;
        std::shared_ptr<T> Lock() const;
        void Reset();

        explicit operator bool() const { return IsValid(); }

    private:
        friend class ResourceManager;
        friend class Registry;

        using SlotPtr = std::shared_ptr<ResourceSlot<T>>;

        ResourceHandle(std::string guid, SlotPtr slot, std::weak_ptr<Registry> registry);

        void Acquire();
        void Release();

        std::string m_guid;
        SlotPtr m_slot;
        std::weak_ptr<Registry> m_registry;
    };

    struct ShaderDescriptor {
        std::string guid;
        std::string vertexPath;
        std::string fragmentPath;
    };

    struct TextureDescriptor {
        std::string guid;
        std::string path;
        bool generateMipmaps = true;
        bool srgb = true;
        bool flipY = true;
    };

    struct MeshDescriptor {
        std::string guid;
        std::string path;
    };

    using ShaderHandle = ResourceHandle<Shader>;
    using TextureHandle = ResourceHandle<Texture>;
    using MeshHandle = ResourceHandle<Mesh>;

    class Registry : public std::enable_shared_from_this<Registry> {
    public:
        Registry();

        void Reset();

        ShaderHandle LoadShader(const ShaderDescriptor& descriptor);
        ShaderHandle ReloadShader(const ShaderDescriptor& descriptor);
        std::shared_ptr<Shader> GetShader(const std::string& guid) const;
        bool HasShader(const std::string& guid) const;

        TextureHandle LoadTexture(const TextureDescriptor& descriptor);
        TextureHandle ReloadTexture(const TextureDescriptor& descriptor);
        std::shared_ptr<Texture> GetTexture(const std::string& guid) const;
        bool HasTexture(const std::string& guid) const;

        MeshHandle LoadMesh(const MeshDescriptor& descriptor);
        MeshHandle ReloadMesh(const MeshDescriptor& descriptor);
        std::shared_ptr<Mesh> GetMesh(const std::string& guid) const;
        bool HasMesh(const std::string& guid) const;

    private:
        template <typename>
        friend class ResourceHandle;

        template <typename T>
        using SlotPtr = std::shared_ptr<ResourceSlot<T>>;

        template <typename T>
        struct CacheStore {
            mutable std::shared_mutex mutex;
            std::unordered_map<std::string, SlotPtr<T>> slots;
        };

        template <typename T>
        CacheStore<T>& Cache();

        template <typename T>
        const CacheStore<T>& Cache() const;

        template <typename T>
        SlotPtr<T> FindSlot(const std::string& guid) const;

        template <typename T>
        SlotPtr<T> GetOrCreateSlot(const std::string& guid);

        template <typename T>
        void RemoveSlotIfUnused(const std::string& guid, const SlotPtr<T>& slot);

        template <typename T>
        void IncrementRef(const std::string& guid, const SlotPtr<T>& slot);

        template <typename T>
        void DecrementRef(const std::string& guid, const SlotPtr<T>& slot);

        CacheStore<Shader> m_shaderCache;
        CacheStore<Texture> m_textureCache;
        CacheStore<Mesh> m_meshCache;
    };

    static void Init(std::shared_ptr<Registry> registry = nullptr);
    static void SetRegistry(std::shared_ptr<Registry> registry);
    static std::shared_ptr<Registry> GetRegistry();
    static void Cleanup();

    static ShaderHandle LoadShader(const ShaderDescriptor& descriptor);
    static ShaderHandle ReloadShader(const ShaderDescriptor& descriptor);
    static std::shared_ptr<Shader> GetShader(const std::string& guid);
    static bool HasShader(const std::string& guid);

    static TextureHandle LoadTexture(const TextureDescriptor& descriptor);
    static TextureHandle ReloadTexture(const TextureDescriptor& descriptor);
    static std::shared_ptr<Texture> GetTexture(const std::string& guid);
    static bool HasTexture(const std::string& guid);

    static MeshHandle LoadMesh(const MeshDescriptor& descriptor);
    static MeshHandle ReloadMesh(const MeshDescriptor& descriptor);
    static std::shared_ptr<Mesh> GetMesh(const std::string& guid);
    static bool HasMesh(const std::string& guid);

private:
    ResourceManager() = delete;
};

// ----- ResourceHandle Implementation -----

template <typename T>
ResourceManager::ResourceHandle<T>::ResourceHandle(std::string guid,
                                                   SlotPtr slot,
                                                   std::weak_ptr<Registry> registry)
    : m_guid(std::move(guid))
    , m_slot(std::move(slot))
    , m_registry(std::move(registry)) {
    Acquire();
}

template <typename T>
ResourceManager::ResourceHandle<T>::ResourceHandle(const ResourceHandle& other)
    : m_guid(other.m_guid)
    , m_slot(other.m_slot)
    , m_registry(other.m_registry) {
    Acquire();
}

template <typename T>
ResourceManager::ResourceHandle<T>& ResourceManager::ResourceHandle<T>::operator=(const ResourceHandle& other) {
    if (this != &other) {
        Release();
        m_guid = other.m_guid;
        m_slot = other.m_slot;
        m_registry = other.m_registry;
        Acquire();
    }
    return *this;
}

template <typename T>
ResourceManager::ResourceHandle<T>::ResourceHandle(ResourceHandle&& other) noexcept
    : m_guid(std::move(other.m_guid))
    , m_slot(std::move(other.m_slot))
    , m_registry(std::move(other.m_registry)) {
    other.m_guid.clear();
}

template <typename T>
ResourceManager::ResourceHandle<T>& ResourceManager::ResourceHandle<T>::operator=(ResourceHandle&& other) noexcept {
    if (this != &other) {
        Release();
        m_guid = std::move(other.m_guid);
        m_slot = std::move(other.m_slot);
        m_registry = std::move(other.m_registry);
        other.m_guid.clear();
    }
    return *this;
}

template <typename T>
ResourceManager::ResourceHandle<T>::~ResourceHandle() {
    Release();
}

template <typename T>
bool ResourceManager::ResourceHandle<T>::IsLoaded() const {
    auto slot = m_slot;
    if (!slot) {
        return false;
    }
    std::shared_lock lock(slot->mutex);
    return static_cast<bool>(slot->resource);
}

template <typename T>
std::shared_ptr<T> ResourceManager::ResourceHandle<T>::Lock() const {
    auto slot = m_slot;
    if (!slot) {
        return nullptr;
    }
    std::shared_lock lock(slot->mutex);
    return slot->resource;
}

template <typename T>
void ResourceManager::ResourceHandle<T>::Reset() {
    Release();
}

template <typename T>
void ResourceManager::ResourceHandle<T>::Acquire() {
    if (m_guid.empty() || !m_slot) {
        return;
    }
    if (auto registry = m_registry.lock()) {
        if constexpr (std::is_same_v<T, Shader>) {
            registry->IncrementRef<Shader>(m_guid, m_slot);
        } else if constexpr (std::is_same_v<T, Texture>) {
            registry->IncrementRef<Texture>(m_guid, m_slot);
        } else if constexpr (std::is_same_v<T, Mesh>) {
            registry->IncrementRef<Mesh>(m_guid, m_slot);
        }
    } else {
        m_slot->refCount.fetch_add(1, std::memory_order_relaxed);
    }
}

template <typename T>
void ResourceManager::ResourceHandle<T>::Release() {
    if (m_guid.empty() || !m_slot) {
        return;
    }
    if (auto registry = m_registry.lock()) {
        if constexpr (std::is_same_v<T, Shader>) {
            registry->DecrementRef<Shader>(m_guid, m_slot);
        } else if constexpr (std::is_same_v<T, Texture>) {
            registry->DecrementRef<Texture>(m_guid, m_slot);
        } else if constexpr (std::is_same_v<T, Mesh>) {
            registry->DecrementRef<Mesh>(m_guid, m_slot);
        }
    } else {
        m_slot->refCount.fetch_sub(1, std::memory_order_relaxed);
    }
    m_guid.clear();
    m_slot.reset();
    m_registry.reset();
}

// ----- Registry Template Helpers -----

template <typename T>
typename ResourceManager::Registry::template CacheStore<T>&
ResourceManager::Registry::Cache() {
    if constexpr (std::is_same_v<T, Shader>) {
        return m_shaderCache;
    } else if constexpr (std::is_same_v<T, Texture>) {
        return m_textureCache;
    } else {
        static_assert(std::is_same_v<T, Mesh>, "Unsupported resource type");
        return m_meshCache;
    }
}

template <typename T>
const typename ResourceManager::Registry::template CacheStore<T>&
ResourceManager::Registry::Cache() const {
    if constexpr (std::is_same_v<T, Shader>) {
        return m_shaderCache;
    } else if constexpr (std::is_same_v<T, Texture>) {
        return m_textureCache;
    } else {
        static_assert(std::is_same_v<T, Mesh>, "Unsupported resource type");
        return m_meshCache;
    }
}

template <typename T>
typename ResourceManager::Registry::template SlotPtr<T>
ResourceManager::Registry::FindSlot(const std::string& guid) const {
    const auto& cache = Cache<T>();
    std::shared_lock lock(cache.mutex);
    if (auto it = cache.slots.find(guid); it != cache.slots.end()) {
        return it->second;
    }
    return nullptr;
}

template <typename T>
typename ResourceManager::Registry::template SlotPtr<T>
ResourceManager::Registry::GetOrCreateSlot(const std::string& guid) {
    {
        const auto& cacheConst = Cache<T>();
        std::shared_lock sharedLock(cacheConst.mutex);
        if (auto it = cacheConst.slots.find(guid); it != cacheConst.slots.end()) {
            return it->second;
        }
    }

    auto& cache = Cache<T>();
    std::unique_lock lock(cache.mutex);
    auto it = cache.slots.find(guid);
    if (it != cache.slots.end()) {
        return it->second;
    }

    auto slot = std::make_shared<ResourceSlot<T>>();
    slot->refCount.store(0, std::memory_order_relaxed);
    cache.slots.emplace(guid, slot);
    return slot;
}

template <typename T>
void ResourceManager::Registry::RemoveSlotIfUnused(const std::string& guid,
                                                   const SlotPtr<T>& slot) {
    if (!slot) {
        return;
    }

    if (slot->refCount.load(std::memory_order_acquire) != 0) {
        return;
    }

    // slot.use_count() accounts for this function's reference and the registry's map
    if (slot.use_count() > 2) {
        return;
    }

    auto& cache = Cache<T>();
    std::unique_lock lock(cache.mutex);
    auto it = cache.slots.find(guid);
    if (it == cache.slots.end() || it->second != slot) {
        return;
    }

    if (slot->refCount.load(std::memory_order_acquire) == 0 &&
        slot.use_count() <= 2) {
        cache.slots.erase(it);
    }
}

template <typename T>
void ResourceManager::Registry::IncrementRef(const std::string&,
                                             const SlotPtr<T>& slot) {
    if (!slot) {
        return;
    }
    slot->refCount.fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
void ResourceManager::Registry::DecrementRef(const std::string& guid,
                                             const SlotPtr<T>& slot) {
    if (!slot) {
        return;
    }
    const auto previous = slot->refCount.fetch_sub(1, std::memory_order_acq_rel);
    if (previous <= 1) {
        RemoveSlotIfUnused<T>(guid, slot);
    }
}

} // namespace gm
