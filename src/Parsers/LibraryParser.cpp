#include "LibraryParser.h"

bool LibraryParser::InitFilter() {
    return true;
}

CefResponseFilter::FilterStatus  LibraryParser::Filter(
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


void LibraryParser::Parse_Library() {


    try {

        json init = json::parse(raw_data);

        if (init.is_array()) {
            std::vector<FileRef>files;
            for (auto &ele : init) {

                FileRef fileref;

                if (ele.contains("id") && ele["id"].is_string() && ele.contains("file_id") && ele["file_id"].is_string()
                    && ele.contains("file_name") && ele["file_name"].is_string() && ele.contains("mime_type") &&
                    ele["mime_type"].is_string() && ele.contains("file_size_bytes")
                    )
                {
                    std::string file_id = ele["file_id"].get<std::string>();
                    std::string mime_type = ele["mime_type"].get<std::string>();
                    std::string lib_file_id = ele["id"].get<std::string>();
                    uint64_t size = ele["file_size_bytes"].get<uint64_t>();
                    std::string filename = ele["file_name"].get<std::string>();

                    fileref.lib_file_id = lib_file_id;
                    fileref.filename = filename;
                    fileref.id = file_id;
                    fileref.size_bytes = size;
                    fileref.mime_type = mime_type;

                    files.push_back(fileref);
                }
            }

            if (!files.empty()) {

                AppState::SetLibrary(files);
            }
        }


    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
};

LibraryParser::~LibraryParser() {
    Parse_Library();
}