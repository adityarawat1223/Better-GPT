#include <string>
#include "Helpers/json.hpp"
#include "FileLinkParser.h"
#include "Helpers/reqrunner.h"

using json = nlohmann::json;

bool FileLinkParser::InitFilter() {
    return true;
}

CefResponseFilter::FilterStatus   FileLinkParser::Filter(
    void* data_in,
    size_t data_in_size,
    size_t& data_in_read,
    void* data_out,
    size_t data_out_size,
    size_t& data_out_written) {

    if (!data_in || data_in_size == 0) {
        data_in_read = 0;
        data_out_written = 0;
        return RESPONSE_FILTER_DONE;
    }

    size_t tocopy = std::min(data_in_size, data_out_size);
    const char* raw_bytes = static_cast<const char*>(data_in);
    raw_data.append(raw_bytes, tocopy);
    memcpy(data_out, data_in, tocopy);
    data_in_read = tocopy;
    data_out_written = tocopy;

    return RESPONSE_FILTER_NEED_MORE_DATA;
}


void FileLinkParser::file_link_parser() {

    try {
        
        json init = json::parse(raw_data);

        if (init.contains("download_url") && init["download_url"].is_string()) {
            ReqRunner::FileDownloader(init["download_url"].get<std::string>());
        };
    }
    catch (const std::exception& e) {
        std::cerr << "ChatListParser error: " << e.what() << std::endl;
    }
}

FileLinkParser::~FileLinkParser() {

    file_link_parser();
}