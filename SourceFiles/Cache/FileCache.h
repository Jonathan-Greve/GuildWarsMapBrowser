#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <list>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>

namespace GW::Cache {

/**
 * @brief Entry in the file cache.
 */
struct FileCacheEntry
{
    std::shared_ptr<std::vector<uint8_t>> data;
    uint32_t fileId;
    size_t size;
    std::chrono::steady_clock::time_point lastAccess;
    uint32_t accessCount;

    FileCacheEntry()
        : fileId(0), size(0), accessCount(0)
    {
        lastAccess = std::chrono::steady_clock::now();
    }

    FileCacheEntry(uint32_t id, std::shared_ptr<std::vector<uint8_t>> fileData)
        : data(fileData)
        , fileId(id)
        , size(fileData ? fileData->size() : 0)
        , accessCount(1)
    {
        lastAccess = std::chrono::steady_clock::now();
    }

    void Touch()
    {
        lastAccess = std::chrono::steady_clock::now();
        accessCount++;
    }
};

/**
 * @brief LRU cache for raw file data from DAT files.
 *
 * Features:
 * - Thread-safe access with mutex locking
 * - LRU eviction when memory limit is reached
 * - Configurable maximum memory usage
 * - File loading via callback (to integrate with DATManager)
 */
class FileCache
{
public:
    /**
     * @brief Callback type for loading file data.
     *
     * @param fileId File ID to load.
     * @return Shared pointer to file data, or nullptr on failure.
     */
    using FileLoader = std::function<std::shared_ptr<std::vector<uint8_t>>(uint32_t fileId)>;

    FileCache(size_t maxMemory = 512 * 1024 * 1024)  // Default 512 MB
        : m_maxMemory(maxMemory)
        , m_currentMemory(0)
    {
    }

    /**
     * @brief Sets the file loader callback.
     *
     * @param loader Callback that loads file data from the DAT file.
     */
    void SetFileLoader(FileLoader loader)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileLoader = std::move(loader);
    }

    /**
     * @brief Sets the maximum memory usage.
     *
     * @param bytes Maximum memory in bytes.
     */
    void SetMaxMemory(size_t bytes)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxMemory = bytes;
        EvictToLimit();
    }

    /**
     * @brief Gets the maximum memory setting.
     */
    size_t GetMaxMemory() const { return m_maxMemory; }

    /**
     * @brief Gets the current memory usage.
     */
    size_t GetCurrentMemory() const { return m_currentMemory; }

    /**
     * @brief Gets the number of cached files.
     */
    size_t GetCachedCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_cache.size();
    }

    /**
     * @brief Gets a file from cache, loading it if necessary.
     *
     * @param fileId File ID to retrieve.
     * @return Shared pointer to file data, or nullptr on failure.
     */
    std::shared_ptr<std::vector<uint8_t>> GetFile(uint32_t fileId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if already cached
        auto it = m_cache.find(fileId);
        if (it != m_cache.end())
        {
            // Move to front of LRU list
            m_lruList.splice(m_lruList.begin(), m_lruList, it->second.lruIterator);
            it->second.entry.Touch();
            return it->second.entry.data;
        }

        // Not cached, try to load
        if (!m_fileLoader)
        {
            return nullptr;
        }

        auto data = m_fileLoader(fileId);
        if (!data || data->empty())
        {
            return nullptr;
        }

        // Add to cache
        AddToCache(fileId, data);
        return data;
    }

    /**
     * @brief Checks if a file is in the cache.
     *
     * @param fileId File ID to check.
     * @return true if the file is cached.
     */
    bool IsCached(uint32_t fileId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_cache.find(fileId) != m_cache.end();
    }

    /**
     * @brief Preloads multiple files into the cache.
     *
     * @param fileIds Vector of file IDs to preload.
     * @param progressCallback Optional callback for progress updates.
     */
    void PreloadFiles(const std::vector<uint32_t>& fileIds,
                      std::function<void(size_t current, size_t total)> progressCallback = nullptr)
    {
        for (size_t i = 0; i < fileIds.size(); i++)
        {
            GetFile(fileIds[i]);

            if (progressCallback)
            {
                progressCallback(i + 1, fileIds.size());
            }
        }
    }

    /**
     * @brief Removes a file from the cache.
     *
     * @param fileId File ID to remove.
     * @return true if the file was removed.
     */
    bool Remove(uint32_t fileId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache.find(fileId);
        if (it == m_cache.end())
        {
            return false;
        }

        m_currentMemory -= it->second.entry.size;
        m_lruList.erase(it->second.lruIterator);
        m_cache.erase(it);

        return true;
    }

    /**
     * @brief Clears all cached files.
     */
    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
        m_lruList.clear();
        m_currentMemory = 0;
    }

    /**
     * @brief Gets cache statistics.
     */
    struct Stats
    {
        size_t totalFiles;
        size_t totalMemory;
        size_t maxMemory;
        uint64_t totalHits;
        uint64_t totalMisses;
    };

    Stats GetStats() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return {
            m_cache.size(),
            m_currentMemory,
            m_maxMemory,
            m_totalHits,
            m_totalMisses
        };
    }

private:
    struct CacheItem
    {
        FileCacheEntry entry;
        std::list<uint32_t>::iterator lruIterator;
    };

    void AddToCache(uint32_t fileId, std::shared_ptr<std::vector<uint8_t>> data)
    {
        // Evict if necessary to make room
        size_t dataSize = data->size();
        while (m_currentMemory + dataSize > m_maxMemory && !m_cache.empty())
        {
            EvictLRU();
        }

        // Add to LRU list (front = most recently used)
        m_lruList.push_front(fileId);

        // Add to cache
        CacheItem item;
        item.entry = FileCacheEntry(fileId, data);
        item.lruIterator = m_lruList.begin();
        m_cache[fileId] = std::move(item);

        m_currentMemory += dataSize;
    }

    void EvictLRU()
    {
        if (m_lruList.empty())
        {
            return;
        }

        // Remove least recently used (back of list)
        uint32_t fileId = m_lruList.back();
        m_lruList.pop_back();

        auto it = m_cache.find(fileId);
        if (it != m_cache.end())
        {
            m_currentMemory -= it->second.entry.size;
            m_cache.erase(it);
        }
    }

    void EvictToLimit()
    {
        while (m_currentMemory > m_maxMemory && !m_cache.empty())
        {
            EvictLRU();
        }
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<uint32_t, CacheItem> m_cache;
    std::list<uint32_t> m_lruList;  // Front = most recently used

    FileLoader m_fileLoader;

    size_t m_maxMemory;
    size_t m_currentMemory;
    uint64_t m_totalHits = 0;
    uint64_t m_totalMisses = 0;
};

} // namespace GW::Cache
