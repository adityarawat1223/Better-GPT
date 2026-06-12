#include "FileParser.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include "memdb/AppState.h"
namespace fs = std::filesystem;

FileParser::FileParser(const std::string& name)
    : file_name(name)
{}

bool FileParser::InitFilter()
{
    try
    {
        auto cache_dir = AppState::GetUserDir() / "cache";

        fs::create_directories(cache_dir);

        std::string full_path =
            (cache_dir / file_name).string();
        out_file.open(
            full_path,
            std::ios::binary);

        if (!out_file.is_open())
        {
            std::cout
                << "Failed to open file: "
                << full_path
                << std::endl;

            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        std::cout
            << e.what()
            << std::endl;

        return false;
    }
}

CefResponseFilter::FilterStatus FileParser::Filter(
    void* data_in,
    size_t data_in_size,
    size_t& data_in_read,
    void* data_out,
    size_t data_out_size,
    size_t& data_out_written)
{
    data_in_read = 0;
    data_out_written = 0;

    if (!data_in || data_in_size == 0)
    {
        return RESPONSE_FILTER_DONE;
    }

    size_t to_copy =
        std::min(data_in_size, data_out_size);

    memcpy(
        data_out,
        data_in,
        to_copy);

    if (out_file.is_open())
    {
        out_file.write(
            static_cast<const char*>(data_in),
            to_copy);
    }

    data_in_read = to_copy;
    data_out_written = to_copy;

    return RESPONSE_FILTER_NEED_MORE_DATA;
}

FileParser::~FileParser()
{
    if (out_file.is_open())
    {
        out_file.flush();
        out_file.close();
    }
}