#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "include/cef_response_filter.h"

class ModelsParser : public CefResponseFilter
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

    void Get_Models();

    ~ModelsParser();

private:
    std::string raw_data;

    IMPLEMENT_REFCOUNTING(ModelsParser);
};