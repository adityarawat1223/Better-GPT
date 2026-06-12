#include "MeParser.h"
#include <iostream>
#include  "Helpers/ReqRunner.h"
bool MeParser::InitFilter() {
    return true;
}

CefResponseFilter::FilterStatus  MeParser::Filter(
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


void MeParser::GetMyInfo() {


    try {

        json init = json::parse(raw_data);

        if (init.contains("name") && init["name"].is_string() && init.contains("created") &&
            init.contains("picture") && init["picture"].is_string() && init.contains("email") && init["email"].is_string() &&
            init.contains("id") && init["id"].is_string())
        {
            std::string name = init["name"].get<std::string>();
            std::string email = init["email"].get<std::string>();
            std::int64_t created = init["created"].get<std::int64_t>();
            std::string url = init["picture"].get<std::string>();
            std::string id = init["id"].get<std::string>();
            std::string url2 = url;
            AppState::Update_User_Info(url, name, created, email , id);
            ReqRunner::PfpDownload(url2);



        }
      
        

    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
};

MeParser::~MeParser() {
    GetMyInfo();
}