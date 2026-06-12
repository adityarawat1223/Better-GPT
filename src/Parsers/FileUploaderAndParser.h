#pragma once
#include "include/cef_response_filter.h"
#include <string>
#include <iostream>
#include "Manager/app.h"
#include "Dto/FileRef.h"

class FileUploaderAndParser : public CefResponseFilter {
private:
    std::string raw_data;
    IMPLEMENT_REFCOUNTING(FileUploaderAndParser);

    void File_Uploader();

public:

    bool InitFilter() override;

    CefResponseFilter::FilterStatus Filter(
        void* data_in,
        size_t data_in_size,
        size_t& data_in_read,
        void* data_out,
        size_t data_out_size,
        size_t& data_out_written
    ) override;

    ~FileUploaderAndParser();
};