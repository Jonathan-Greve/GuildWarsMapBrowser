#pragma once

enum InitializationState
{
    NotStarted,
    Started,
    Completed
};

class DATManager
{
public:
    void Init(std::wstring dat_filepath)
    {
        m_initialization_state = InitializationState::Started;

        m_dat_filepath = dat_filepath;
        m_dat.readDat(m_dat_filepath.c_str());

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
            thread.detach();
        }
    }

    std::atomic<InitializationState> m_initialization_state{NotStarted};

    int get_num_files_type_read() { return m_num_types_read; }
    int get_num_files() { return m_dat.getNumFiles(); }

    std::vector<MFTEntry>& get_MFT() { return m_dat.get_MFT(); }

private:
    std::wstring m_dat_filepath;
    GWDat m_dat;

    // Index the MFTEntry vector for fast lookups.
    std::unordered_map<uint32_t, MFTEntry*> ffna_MFTEntry_LUT;

    std::atomic<int> m_num_types_read{0};

    std::atomic<int> m_num_running_dat_reader_threads{0};
    void read_files_thread(std::vector<int> file_indices)
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
};
