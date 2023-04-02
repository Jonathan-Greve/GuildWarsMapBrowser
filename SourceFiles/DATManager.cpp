#include "pch.h"
#include "DATManager.h"

FFNA_MapFile DATManager::parse_ffna_map_file(int index)
{
    MFTEntry* mft_entry = m_dat.get_MFT_entry_ptr(index);
    if (! mft_entry)
        throw "mft_entry not found.";

    // Get decompressed file data
    HANDLE file_handle = m_dat.get_dat_filehandle(m_dat_filepath.c_str());
    auto data = m_dat.readFile(file_handle, index, true);
    std::span<unsigned char> file_data(data, mft_entry->uncompressedSize);
    CloseHandle(file_handle);

    FFNA_MapFile ffna_map_file(0, file_data);
    delete[] data;

    return ffna_map_file;
}

FFNA_ModelFile DATManager::parse_ffna_model_file(int index)
{
    MFTEntry* mft_entry = m_dat.get_MFT_entry_ptr(index);
    if (! mft_entry)
        throw "mft_entry not found.";

    // Get decompressed file data
    HANDLE file_handle = m_dat.get_dat_filehandle(m_dat_filepath.c_str());
    auto data = m_dat.readFile(file_handle, index, true);
    std::span<unsigned char> file_data(data, mft_entry->uncompressedSize);
    CloseHandle(file_handle);

    FFNA_ModelFile ffna_model_file(0, file_data);
    delete[] data;

    return ffna_model_file;
}

std::vector<RGBA> DATManager::parse_ffna_texture_file(int index)
{
    MFTEntry* mft_entry = m_dat.get_MFT_entry_ptr(index);
    if (! mft_entry)
        throw "mft_entry not found.";

    // Get decompressed file data
    HANDLE file_handle = m_dat.get_dat_filehandle(m_dat_filepath.c_str());
    auto data = m_dat.readFile(file_handle, index, true);
    std::span<unsigned char> file_data(data, mft_entry->uncompressedSize);
    CloseHandle(file_handle);

    // Process texture data
    auto rgba_data = ProcessImageFile(data, mft_entry->uncompressedSize);

    delete[] data;

    return rgba_data;
}

void DATManager::read_all_files()
{
    const auto num_files = m_dat.getNumFiles();

    // Get the number of available threads
    const auto num_threads = std::thread::hardware_concurrency();

    // Fill the concurrent queue with file indices
    Concurrency::concurrent_queue<int> file_indices_queue;
    for (int i = 0; i < num_files; ++i)
    {
        file_indices_queue.push(i);
    }

    // Start the file reading threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(&DATManager::read_files_thread, this, std::ref(file_indices_queue));
    }

    // Wait for all threads to finish
    for (auto& thread : threads)
    {
        thread.join();
    }
}

void DATManager::read_files_thread(Concurrency::concurrent_queue<int>& file_indices_queue)
{
    HANDLE file_handle = m_dat.get_dat_filehandle(m_dat_filepath.c_str());
    unsigned char* data;
    int index;

    while (file_indices_queue.try_pop(index))
    {
        data = m_dat.readFile(file_handle, index, false);
        delete[] data;

        auto _ = m_num_types_read.fetch_add(1, std::memory_order_relaxed);
    }

    CloseHandle(file_handle);

    auto remaining_threads = m_num_running_dat_reader_threads.fetch_sub(1, std::memory_order_relaxed);

    if (file_indices_queue.empty())
    {
        m_initialization_state = InitializationState::Completed;
    }
}
