#pragma once
#include "include/cef_response_filter.h"
#include <string>
#include <iostream>
#include "Manager/app.h"
#include "Dto/token.h"
#include <atomic>
class ChatListParser : public CefResponseFilter {
private:
    std::string raw_data;
    IMPLEMENT_REFCOUNTING(ChatListParser);

    void chatlist_json_writer();

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
    std::atomic<int>* completed;
    std::string m_url;
public:
    ChatListParser() : completed(nullptr) {}
    ChatListParser(std::atomic<int>* completed_ptr, const std::string& url = "") : completed(completed_ptr), m_url(url) {}

    ~ChatListParser();
};