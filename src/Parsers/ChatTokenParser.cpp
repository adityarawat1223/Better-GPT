#include "ChatTokenParser.h"
#include "Helpers/json.hpp"
#include "memdb/AppState.h"
using json = nlohmann::json;

bool ChatTokenParser::InitFilter() {
    return true;
}

CefResponseFilter::FilterStatus ChatTokenParser::Filter(
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

void ChatTokenParser::token_parser() {
    json j = json::parse(raw_data);

    if (j.contains("token") && j["token"].is_string()) {
        std::string temp = j["token"].get<std::string>();
        std::string key = "openai-sentinel-chat-requirements-token";

        AppState::UpdateHeaders(key, temp);
       
    }
};

ChatTokenParser::~ChatTokenParser() {

    token_parser();
}