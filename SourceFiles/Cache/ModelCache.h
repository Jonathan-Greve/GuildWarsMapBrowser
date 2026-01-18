#pragma once

#include "FileCache.h"
#include "../Animation/AnimationClip.h"
#include "../Animation/Skeleton.h"
#include "../Parsers/BB9AnimationParser.h"
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include <functional>

namespace GW::Cache {

/**
 * @brief Cached animated model with geometry and animation data.
 */
struct CachedAnimatedModel
{
    uint32_t fileId = 0;
    std::shared_ptr<Animation::AnimationClip> animationClip;
    std::shared_ptr<Animation::Skeleton> skeleton;

    // Model hashes for matching
    uint32_t modelHash0 = 0;
    uint32_t modelHash1 = 0;

    bool IsValid() const
    {
        return animationClip && animationClip->IsValid();
    }

    bool HasSkeleton() const
    {
        return skeleton && skeleton->IsValid();
    }
};

/**
 * @brief Cache for parsed model and animation data.
 *
 * Uses weak pointers to allow automatic cleanup when models are no longer referenced.
 * Integrates with FileCache for raw data loading.
 */
class ModelCache
{
public:
    /**
     * @brief Creates a model cache with an associated file cache.
     *
     * @param fileCache File cache for loading raw data.
     */
    explicit ModelCache(std::shared_ptr<FileCache> fileCache = nullptr)
        : m_fileCache(fileCache)
    {
    }

    /**
     * @brief Sets the file cache for loading raw data.
     */
    void SetFileCache(std::shared_ptr<FileCache> fileCache)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileCache = fileCache;
    }

    /**
     * @brief Gets an animated model by file ID, loading and parsing if necessary.
     *
     * @param fileId File ID of the animation file.
     * @return Shared pointer to the cached model, or nullptr on failure.
     */
    std::shared_ptr<CachedAnimatedModel> GetAnimatedModel(uint32_t fileId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if already cached
        auto it = m_animatedModels.find(fileId);
        if (it != m_animatedModels.end())
        {
            auto cached = it->second.lock();
            if (cached)
            {
                return cached;
            }
            // Weak pointer expired, remove from cache
            m_animatedModels.erase(it);
        }

        // Load and parse
        auto model = LoadAnimatedModel(fileId);
        if (model)
        {
            m_animatedModels[fileId] = model;
        }

        return model;
    }

    /**
     * @brief Gets an animation clip by file ID.
     *
     * @param fileId File ID of the animation file.
     * @return Shared pointer to the animation clip, or nullptr on failure.
     */
    std::shared_ptr<Animation::AnimationClip> GetAnimationClip(uint32_t fileId)
    {
        auto model = GetAnimatedModel(fileId);
        return model ? model->animationClip : nullptr;
    }

    /**
     * @brief Gets a skeleton by file ID.
     *
     * @param fileId File ID of the animation file.
     * @return Shared pointer to the skeleton, or nullptr on failure.
     */
    std::shared_ptr<Animation::Skeleton> GetSkeleton(uint32_t fileId)
    {
        auto model = GetAnimatedModel(fileId);
        return model ? model->skeleton : nullptr;
    }

    /**
     * @brief Checks if a model is cached.
     *
     * @param fileId File ID to check.
     * @return true if the model is cached and valid.
     */
    bool IsCached(uint32_t fileId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_animatedModels.find(fileId);
        if (it != m_animatedModels.end())
        {
            return !it->second.expired();
        }
        return false;
    }

    /**
     * @brief Removes a model from the cache.
     *
     * @param fileId File ID to remove.
     * @return true if the model was removed.
     */
    bool Remove(uint32_t fileId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_animatedModels.erase(fileId) > 0;
    }

    /**
     * @brief Clears all cached models.
     */
    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_animatedModels.clear();
    }

    /**
     * @brief Cleans up expired weak pointers.
     */
    void CleanupExpired()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto it = m_animatedModels.begin(); it != m_animatedModels.end();)
        {
            if (it->second.expired())
            {
                it = m_animatedModels.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    /**
     * @brief Gets the number of cached models (including expired).
     */
    size_t GetCachedCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_animatedModels.size();
    }

    /**
     * @brief Gets the number of valid (non-expired) cached models.
     */
    size_t GetValidCachedCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = 0;
        for (const auto& [id, weakPtr] : m_animatedModels)
        {
            if (!weakPtr.expired())
            {
                count++;
            }
        }
        return count;
    }

private:
    std::shared_ptr<CachedAnimatedModel> LoadAnimatedModel(uint32_t fileId)
    {
        if (!m_fileCache)
        {
            return nullptr;
        }

        // Get raw file data
        auto fileData = m_fileCache->GetFile(fileId);
        if (!fileData || fileData->empty())
        {
            return nullptr;
        }

        // Parse animation
        auto clipOpt = Parsers::ParseAnimationFromFile(fileData->data(), fileData->size());
        if (!clipOpt)
        {
            return nullptr;
        }

        auto model = std::make_shared<CachedAnimatedModel>();
        model->fileId = fileId;
        model->animationClip = std::make_shared<Animation::AnimationClip>(std::move(*clipOpt));
        model->modelHash0 = model->animationClip->modelHash0;
        model->modelHash1 = model->animationClip->modelHash1;

        // Create skeleton from animation clip
        model->skeleton = std::make_shared<Animation::Skeleton>(
            Parsers::BB9AnimationParser::CreateSkeleton(*model->animationClip));

        return model;
    }

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<FileCache> m_fileCache;
    std::unordered_map<uint32_t, std::weak_ptr<CachedAnimatedModel>> m_animatedModels;
};

/**
 * @brief Global cache manager for easy access.
 *
 * Provides static access to file and model caches.
 */
class CacheManager
{
public:
    static CacheManager& Instance()
    {
        static CacheManager instance;
        return instance;
    }

    FileCache& GetFileCache() { return m_fileCache; }
    ModelCache& GetModelCache() { return m_modelCache; }

    /**
     * @brief Initializes the cache system with a file loader.
     */
    void Initialize(FileCache::FileLoader fileLoader, size_t maxMemoryMB = 512)
    {
        m_fileCache.SetMaxMemory(maxMemoryMB * 1024 * 1024);
        m_fileCache.SetFileLoader(std::move(fileLoader));
        m_modelCache.SetFileCache(std::make_shared<FileCache>(m_fileCache));
    }

    /**
     * @brief Clears all caches.
     */
    void ClearAll()
    {
        m_modelCache.Clear();
        m_fileCache.Clear();
    }

private:
    CacheManager() = default;
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    FileCache m_fileCache;
    ModelCache m_modelCache;
};

} // namespace GW::Cache
