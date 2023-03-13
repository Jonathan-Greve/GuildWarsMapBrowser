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

        // Start the file reading thread
        std::thread fileThread(&DATManager::read_files_thread, this);
        fileThread.detach();
    }

    std::atomic<InitializationState> m_initialization_state{NotStarted};

private:
    std::wstring m_dat_filepath;
    GWDat m_dat;

    std::atomic<int> m_num_types_read{0};

    void read_files_thread()
    {
        unsigned char* data;
        for (unsigned int i = 0; i < m_dat.getNumFiles(); ++i)
        {
            data = m_dat.readFile(i, false);
            delete[] data;
            increment(m_num_types_read);
        }

        assert(m_num_types_read == m_dat.getNumFiles());
        m_initialization_state = InitializationState::Completed;
    }

    void increment(std::atomic<int>& value) { value.fetch_add(1, std::memory_order_relaxed); }
};
