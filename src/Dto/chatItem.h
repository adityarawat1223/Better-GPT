#pragma once

#include <string>
#include<vector>
#include "FileRef.h"
#include <set>

enum class Modes {
    normal,
    reason,
    search,
    create_image,
    deep_research,
};

enum class Status {
    NotOpened,
    ReqSent,
    ResRecieved,
    Parsing,
    Cached,
    Error
};
struct InputBox {

    std::string content;
    std::unordered_map<
        std::string,
        FileRef
    > assets;
    Modes mode = Modes::normal;
    std::string model;
};

struct ChatItem {
    std::string id;
    std::string title;
    std::string create_time;
    std::string update_time;
    std::string raw_update_time;
    bool is_archived;
    bool is_temporary_chat = false;
    InputBox inputbox;
    bool isstreaming = false;
    long long current_tokens = 0;
};

inline std::vector<ChatItem>ChatList;