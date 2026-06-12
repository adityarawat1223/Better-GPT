#include "ChatListParser.h"
#include "include/cef_task.h"
#include "Manager/app.h"
#include <iostream>
#include <fstream>
#include <cerrno>
#include <cstring>
#include "Helpers/json.hpp"
using json = nlohmann::json;
#include <filesystem> 
#include "Dto/chatitem.h"
#include "memdb/AppState.h"
namespace fs = std::filesystem;
#include "Helpers/ReqRunner.h"

 #include <sstream>
#include <iomanip>

static std::string FormatTimestamp(const std::string & iso)
{
    std::tm tm = {};

    std::istringstream ss(iso.substr(0, 19));
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    char buffer[64];
    std::strftime(buffer, sizeof(buffer),
        "%b %d, %Y • %I:%M %p",
        &tm);

    return buffer;
}


class CloseBrowserTask : public CefTask {
public:
    explicit CloseBrowserTask(CefRefPtr<CefBrowser> b) : browser_(b) {}
    void Execute() override {
        browser_->GetHost()->CloseBrowser(true);
        CefQuitMessageLoop();
        ReqRunner::CreateBrowser();

    }
private:
    CefRefPtr<CefBrowser> browser_;
    IMPLEMENT_REFCOUNTING(CloseBrowserTask);
};



bool ChatListParser::InitFilter() {
    return true;
}

CefResponseFilter::FilterStatus  ChatListParser::Filter(
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
void ChatListParser::chatlist_json_writer() {
    if (raw_data.empty()) return;

    try {
        json payload = json::parse(raw_data);
        if (!payload.contains("items") || !payload["items"].is_array()) return;

        std::vector<ChatItem> chat_list;

        for (const auto& item : payload["items"]) {
            if (!item.contains("id") || !item["id"].is_string()) continue;

            ChatItem ctitem;
            ctitem.id = item["id"].get<std::string>();
            ctitem.title = item.value("title", "Untitled Chat");
            ctitem.create_time = FormatTimestamp( item.value("create_time", ""));
            ctitem.update_time = FormatTimestamp(item.value("update_time", ""));
            ctitem.raw_update_time = item.value("update_time", "");
            ctitem.is_archived = item.value("is_archived", false);
            ctitem.is_temporary_chat = item.value("is_temporary_chat", false);

            chat_list.push_back(ctitem);
        }

        std::sort(chat_list.begin(), chat_list.end(), [](const ChatItem& a, const ChatItem& b) {
            return a.raw_update_time > b.raw_update_time;
        });

        AppState::SetHasMoreChats(chat_list.size() >= 20);

        // Determine if this is a paginated fetch by checking the offset in the URL
        bool isPagination = false;
        if (m_url.find("offset=") != std::string::npos) {
            size_t pos = m_url.find("offset=");
            std::string offsetStr = m_url.substr(pos + 7);
            int offsetVal = std::atoi(offsetStr.c_str());
            if (offsetVal > 0) {
                isPagination = true;
            }
        }

        if (isPagination) {
            AppState::AppendChatList(chat_list);
        } else {
            AppState::SetChatList(chat_list);
        }

    }
    catch (const std::exception& e) {
        std::cerr << "ChatListParser error: " << e.what() << std::endl;
    }
}
ChatListParser::~ChatListParser() {
  

    chatlist_json_writer();
        if (AppState::GetMainBrowser() && completed->fetch_add(1) + 1 == 2) {
        CefRefPtr<CefBrowser> browser = AppState::GetMainBrowser();
        AppState::UpdateBrowserInstance(nullptr);
        CefPostTask(TID_UI, new CloseBrowserTask(browser));
    }
}