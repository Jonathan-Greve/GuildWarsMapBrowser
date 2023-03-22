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

void DATManager::read_all_files()
{
    const auto num_files = m_dat.getNumFiles();

    // Get the number of available threads
    const auto num_threads = std::thread::hardware_concurrency();
    m_num_running_dat_reader_threads = num_threads;

    // Split the file indices into n chunks
    int chunk_size = num_files / num_threads;
    int remainder = num_files % num_threads;

    std::vector<std::vector<int>> chunks(num_threads);

    int current_index = 0;
    for (int i = 0; i < num_threads; i++)
    {
        int chunk_end = current_index + chunk_size;
        if (i < remainder)
        {
            chunk_end++;
        }

        chunks[i].resize(chunk_end - current_index);
        for (int j = current_index; j < chunk_end; j++)
        {
            chunks[i][j - current_index] = j;
        }
        current_index = chunk_end;
    }

    // Start the file reading threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(&DATManager::read_files_thread, this, std::ref(chunks[i]));
    }

    // Wait for all threads to finish
    for (auto& thread : threads)
    {
        thread.join();
    }
}

void DATManager::read_files_thread(std::vector<int> file_indices)
{
    HANDLE file_handle = m_dat.get_dat_filehandle(m_dat_filepath.c_str());
    unsigned char* data;
    for (const auto index : file_indices)
    {
        data = m_dat.readFile(file_handle, index, false);
        delete[] data;

        auto _ = m_num_types_read.fetch_add(1, std::memory_order_relaxed);
    }
    CloseHandle(file_handle);

    if (m_num_running_dat_reader_threads == 1)
    {
        m_initialization_state = InitializationState::Completed;
    }

    auto _ = m_num_running_dat_reader_threads.fetch_sub(1, std::memory_order_relaxed);
}
