#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include "include/cef_response_filter.h"
#include "Helpers/json.hpp"
#include "memdb/AppState.h"
using json = nlohmann::json;

class LibraryParser : public CefResponseFilter
{
public:


    bool InitFilter() override;

    FilterStatus Filter(
        void* data_in,
        size_t data_in_size,
        size_t& data_in_read,
        void* data_out,
        size_t data_out_size,
        size_t& data_out_written) override;

    void Parse_Library();

    ~LibraryParser();

private:
    std::string raw_data;

    IMPLEMENT_REFCOUNTING(LibraryParser);
};