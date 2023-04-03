#pragma once
#include "FFNA_MapFile.h"
#include "FFNA_ModelFile.h"
#include <ppl.h>
#include <concurrent_queue.h>

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

        auto read_all_thread = std::thread(&DATManager::read_all_files, this);
        read_all_thread.detach();
    }

    std::atomic<InitializationState> m_initialization_state{NotStarted};

    int get_num_files_type_read() { return m_num_types_read; }
    int get_num_files() { return m_dat.getNumFiles(); }

    std::vector<MFTEntry>& get_MFT() { return m_dat.get_MFT(); }

    FFNA_MapFile parse_ffna_map_file(int index);
    FFNA_ModelFile parse_ffna_model_file(int index);
    DatTexture parse_ffna_texture_file(int index);
    std::vector<uint8_t> parse_dds_file(int index);

private:
    std::wstring m_dat_filepath;
    GWDat m_dat;

    std::atomic<int> m_num_types_read{0};
    std::atomic<int> m_num_running_dat_reader_threads{0};

    void read_all_files();

    void read_files_thread(Concurrency::concurrent_queue<int>& file_indices_queue);
};
