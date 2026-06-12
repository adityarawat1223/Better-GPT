#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

#include "include/cef_response_filter.h"

class FileParser : public CefResponseFilter
{
public:

    explicit FileParser(const std::string& name);

    bool InitFilter() override;

    FilterStatus Filter(
        void* data_in,
        size_t data_in_size,
        size_t& data_in_read,
        void* data_out,
        size_t data_out_size,
        size_t& data_out_written) override;

    void SaveFile();

    ~FileParser();

private:

    std::string file_name;

    std::ofstream out_file;


    IMPLEMENT_REFCOUNTING(FileParser);
};