#include "AppState.h"
#include <iostream>
#include "ui/EventDispatcher.h"
void AppState::SetTokenInfo(const TokenInfo& info) {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    m_token_info = info;
}
std::string AppState::GetTokenInfo() {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    return m_token_info.bearertoken;
}
void AppState::UpdateBearerToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    m_token_info.bearertoken = token;
}
void AppState::UpdateBrowserInstance(CefRefPtr<CefBrowser> browser) {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    m_token_info.mainBrowser = browser;
}
void AppState::UpdateHiddenBrowser(CefRefPtr<CefBrowser> browser) {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    m_token_info.HiddenBrowser = browser;
}
void AppState::SetChatList(std::vector<ChatItem> fresh_list) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        // Build set of incoming IDs to prune stale chats that were deleted on the server.
        // We keep any chat that is currently open (open_chats) even if not in the fresh list,
        // so in-flight conversations don't disappear from the map.
        std::unordered_set<std::string> incoming_ids;
        incoming_ids.reserve(fresh_list.size());
        for (const auto& item : fresh_list)
            incoming_ids.insert(item.id);
        // Erase chats that are gone from the server and not currently open
        for (auto it = m_chat_map.begin(); it != m_chat_map.end(); ) {
            if (incoming_ids.find(it->first) == incoming_ids.end() &&
                open_chats.find(it->first) == open_chats.end()) {
                it = m_chat_map.erase(it);
            }
            else {
                ++it;
            }
        }
        // Merge: preserve runtime state (tokens, inputbox, streaming)
        for (auto& item : fresh_list) {
            auto it = m_chat_map.find(item.id);
            if (it != m_chat_map.end()) {
                item.current_tokens = it->second.current_tokens;
                item.inputbox = std::move(it->second.inputbox);
                item.isstreaming = it->second.isstreaming;
            }
            m_chat_map[item.id] = std::move(item);
        }
    }
    emit EventDispatcher::instance()->chatListUpdated();
}
void AppState::AppendChatList(const std::vector<ChatItem>& list) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        for (auto item : list) {
            auto it = m_chat_map.find(item.id);
            if (it != m_chat_map.end()) {
                item.current_tokens = it->second.current_tokens;
                item.inputbox = std::move(it->second.inputbox);
                item.isstreaming = it->second.isstreaming;
            }
            m_chat_map[item.id] = std::move(item);
        }
    }
    emit EventDispatcher::instance()->chatListUpdated();
}
std::vector<ChatItem> AppState::GetChatListCopy() {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    std::vector<ChatItem> list;
    list.reserve(m_chat_map.size());
    for (const auto& pair : m_chat_map) {
        list.push_back(pair.second);
    }
    std::sort(list.begin(), list.end(), [](const ChatItem& a, const ChatItem& b) {
        return a.raw_update_time > b.raw_update_time;
        });
    return list;
}
void AppState::AppendChatItem(const ChatItem& item) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        m_chat_map[item.id] = item;
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->chatListUpdated();
}
void AppState::ClearChatList() {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        m_chat_map.clear();
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->chatListUpdated();
}
CefRefPtr<CefBrowser> AppState::GetMainBrowser() {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    return  m_token_info.mainBrowser;
}
CefRefPtr<CefBrowser> AppState::GetHiddenBrowser() {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    return  m_token_info.HiddenBrowser;
}
bool AppState::HasMoreChats() {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    return AppState::m_has_more_chats;
}
void AppState::SetHasMoreChats(bool hasMore) {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    AppState::m_has_more_chats = hasMore;
}
void AppState::AddChatsToMap(const std::string& ChatId, std::vector<ChatMessage>& Txtarry) {
    {
        std::lock_guard<std::mutex> lock(m_messages_mutex);
        ChatIdToText[ChatId] = std::move(Txtarry);
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->chatMessageUpdated(ChatId);
}
std::vector<ChatMessage> AppState::GetChatsFromMap(const std::string& ChatId) {
    std::lock_guard<std::mutex> lock(m_messages_mutex);
    auto it = ChatIdToText.find(ChatId);
    if (it != ChatIdToText.end()) {
        return it->second;
    }
    return {};
}

void AppState::AppendChatMessage(const std::string& ChatId, ChatMessage&& message) {
    UpsertChatMessage(ChatId, std::move(message));
}

void AppState::UpdateChatMessageById(const std::string& ChatId, ChatMessage&& message) {
    UpsertChatMessage(ChatId, std::move(message));
}

bool AppState::UpsertChatMessage(const std::string& ChatId, ChatMessage&& message) {
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(m_messages_mutex);
        auto& messages = ChatIdToText[ChatId];

        for (auto& existing : messages) {
            if (existing.message_id == message.message_id) {
                existing = std::move(message);
                updated = true;
                break;
            }
        }

        if (!updated) {
            messages.push_back(std::move(message));
        }
    }

    emit EventDispatcher::instance()->chatMessageUpdated(ChatId);
    return updated;
}

bool AppState::InsertChatMessageIfMissing(const std::string& ChatId, ChatMessage&& message) {
    {
        std::lock_guard<std::mutex> lock(m_messages_mutex);
        auto& messages = ChatIdToText[ChatId];

        for (const auto& existing : messages) {
            if (existing.message_id == message.message_id) {
                return false;
            }
        }

        messages.push_back(std::move(message));
    }

    emit EventDispatcher::instance()->chatMessageUpdated(ChatId);
    return true;
}

std::set<std::string> AppState::GetOpenChats() {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    return open_chats;
}
void AppState::CloseChat(const std::string& ChatId) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        open_chats.erase(ChatId);
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->chatListUpdated();
}
void AppState::AddChat(const std::string& ChatId) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        open_chats.insert(ChatId);
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->chatListUpdated();
}
bool AppState::HasChatData(const std::string& ChatId) {
    // Returns true only if the chat has at least one message loaded.
    std::lock_guard<std::mutex> lock(m_messages_mutex);
    auto it = ChatIdToText.find(ChatId);
    return it != ChatIdToText.end() && !it->second.empty();
}
bool AppState::HasChatEntry(const std::string& ChatId) {
    // Returns true if the chat key exists in the map (even if messages are empty).
    // Use this to check if a chat was registered (e.g. temp new chat).
    std::lock_guard<std::mutex> lock(m_messages_mutex);
    return ChatIdToText.find(ChatId) != ChatIdToText.end();
}
void AppState::UpdateHeaders(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    headers[key] = std::move(value);
}
std::string AppState::GetHeaders(const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_auth_mutex);
    auto it = headers.find(key);
    if (it != headers.end())
    {
        return it->second;
    }
    return "";
}
std::set<Models> AppState::GetModels() {
    std::lock_guard<std::mutex> lock(m_models_mutex);
    return ModelsInfo;
}
void AppState::AddModels(const Models& md) {
    std::lock_guard<std::mutex> lock(m_models_mutex);
    ModelsInfo.insert(md);
};
void AppState::Set_Default_Model(std::string& model) {
    std::lock_guard<std::mutex> lock(m_user_mutex);
    user.default_model = std::move(model);
}
std::string AppState::Get_Default_Model() {
    std::lock_guard<std::mutex> lock(m_user_mutex);
    return user.default_model;
};
void AppState::Add_File_Asset(const FileRef& fileref, const std::string& ChatId, const std::string& local_id) {
    {
        std::lock_guard<std::mutex> lock(m_inputs_mutex);
        ChatInputs[ChatId].assets[local_id] = (fileref);
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->assetsUpdated(ChatId, local_id);
}
void AppState::Remove_File_Asset(const FileRef& fileref, const std::string& ChatId, const std::string& local_id) {
    {
        std::lock_guard<std::mutex> lock(m_inputs_mutex);
        ChatInputs[ChatId].assets.erase(local_id);
    }
    emit EventDispatcher::instance()->assetsUpdated(ChatId, local_id);
}
void AppState::Clear_Input_Assets(const std::string& ChatId) {
    {
        std::lock_guard<std::mutex> lock(m_inputs_mutex);
        ChatInputs[ChatId].assets.clear();
    }
    emit EventDispatcher::instance()->assetsUpdated(ChatId, "");
}


void AppState::Add_Job(const FileRef& body)
{
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        UploadQueue.push(body);
    }
    UploadCV.notify_one();
}
bool AppState::Pop_Job(FileRef& out)
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    UploadCV.wait(lock, []()
        {
            return !UploadQueue.empty();
        });
    out = std::move(UploadQueue.front());
    UploadQueue.pop();
    return true;
}
void AppState::Submit_Search_Job(const std::string& query, const std::vector<std::string>& chatIds)
{
    std::lock_guard<std::mutex> lock(m_search_mutex);
    SearchJob job;
    job.searchId = ++active_search_id;
    job.query = query;
    job.chatIds = chatIds;
    CurrentSearchJob = job;
    SearchCV.notify_one();
}
bool AppState::Pop_Search_Job(SearchJob& out)
{
    std::unique_lock<std::mutex> lock(m_search_mutex);
    SearchCV.wait(lock, []() { return CurrentSearchJob.has_value(); });
    out = *CurrentSearchJob;
    CurrentSearchJob.reset();
    return true;
}
int AppState::GetActiveSearchId()
{
    return active_search_id.load();
}
bool AppState::Update_Asset_Status(UploadStatus uploadstatus, const std::string& ChatId, const std::string& local_id) {
    {
        std::lock_guard<std::mutex> lock(m_inputs_mutex);
        auto chatIt = ChatInputs.find(ChatId);
        if (chatIt == ChatInputs.end()) return false;

        auto assetIt = chatIt->second.assets.find(local_id);
        if (assetIt == chatIt->second.assets.end()) return false;

        assetIt->second.uploadstatus = uploadstatus;
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->assetsUpdated(ChatId, local_id);
    return true;
}



void AppState::Set_Parent(const std::string& ChatId, const std::string& Pid) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    Chat_Parent_Id[ChatId] = Pid;
}
std::string AppState::Get_Parent(const std::string& ChatId) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    // Use find() — operator[] would silently insert an empty entry
    auto it = Chat_Parent_Id.find(ChatId);
    return it != Chat_Parent_Id.end() ? it->second : "";
}


void AppState::Update_Model(const std::string& model, const std::string& chat_id) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    ChatInputs[chat_id].model = model;
    
}
UploadStatus AppState::Get_Asset_Status(const std::string& ChatId, const std::string& Local_id) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    auto it1 = ChatInputs.find(ChatId);
    if (it1 != ChatInputs.end()) {
        auto it2 = it1->second.assets.find(Local_id);
        if (it2 != it1->second.assets.end()) {
            return it2->second.uploadstatus;
        }
    }
    return UploadStatus::Failed;
}


bool AppState::Update_Upload_Url(const std::string& ChatId, const std::string& local_id, const std::string& url, const std::string& file_id) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    auto chatIt = ChatInputs.find(ChatId);
    if (chatIt == ChatInputs.end()) return false;

    auto assetIt = chatIt->second.assets.find(local_id);
    if (assetIt == chatIt->second.assets.end()) return false;

    assetIt->second.upload_url = url;
    assetIt->second.id = file_id;
    return true;
}

bool AppState::Update_File_Processed(const std::string& ChatId, const std::string& local_id, const std::string& lib_file_id, const std::string& file_id) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    auto chatIt = ChatInputs.find(ChatId);
    if (chatIt == ChatInputs.end()) return false;

    auto assetIt = chatIt->second.assets.find(local_id);
    if (assetIt == chatIt->second.assets.end()) return false;

    assetIt->second.lib_file_id = lib_file_id;
    assetIt->second.id = file_id;
    return true;
}


FileRef AppState::Get_File_Asset(const std::string& ChatId, const std::string& local_id) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    auto it1 = ChatInputs.find(ChatId);
    if (it1 != ChatInputs.end()) {
        auto it2 = it1->second.assets.find(local_id);
        if (it2 != it1->second.assets.end()) {
            return it2->second;
        }
    }
    return FileRef(); // Return empty FileRef
}
InputBox AppState::Get_Input_Box(const std::string& ChatId) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    auto it = ChatInputs.find(ChatId);
    return it != ChatInputs.end() ? it->second : InputBox{};
};
void AppState::Add_Input(const std::string& input, const std::string& ChatId) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    ChatInputs[ChatId].content = input;
    
}


void AppState::Update_User_Info(std::string& profurl, std::string& name, std::int64_t& created_at, std::string& email, std::string& user_id) {
    std::lock_guard<std::mutex> lock(m_user_mutex);
    user.pfp_url = std::move(profurl);
    user.user_name = std::move(name);
    user.email = std::move(email);
    user.created_at = std::move(created_at);
    user.user_id = std::move(user_id);
}
std::string AppState::Get_User_ID() {
    std::lock_guard<std::mutex> lock(m_user_mutex);
    return user.user_id;
}
std::filesystem::path AppState::GetUserDir() {
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        std::filesystem::path result(path);
        CoTaskMemFree(path);
        return result / "Desktop-GPT";
    }
    throw std::runtime_error("Failed to get LocalAppData");
#elif defined(__linux__)
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::filesystem::path(xdg) / "Desktop-GPT";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "Desktop-GPT";
    throw std::runtime_error("Failed to determine config directory");
#endif
}
void AppState::SetLibrary(std::vector<FileRef>& new_lib) {
    std::lock_guard<std::mutex> lock(m_library_mutex);
    library = std::move(new_lib);
}
std::vector<FileRef>AppState::GetLibrary() {
    std::lock_guard<std::mutex> lock(m_library_mutex);
    return library;
}
void AppState::RemoveLibraryItem(const std::string& fileId) {
    std::lock_guard<std::mutex> lock(m_library_mutex);
    library.erase(std::remove_if(library.begin(), library.end(), [&](const FileRef& a) { return a.id == fileId; }), library.end());
}
User AppState::Get_User() {
    std::lock_guard<std::mutex> lock(m_user_mutex);
    return user;
}
void AppState::Update_User_Feature_Limit(const std::string& featurename, int remaining, int64_t reset_time) {
    std::lock_guard<std::mutex> lock(m_user_mutex);
    if (featurename == "deep_research") {
        user.deep_research_limit = remaining;
        user.deep_reset = reset_time;
    }
    else if (featurename == "file_upload") {
        user.file_upload_limit = remaining;
        user.file_upload_reset = reset_time;
    }
    else if (featurename == "paste_text_to_file") {
        user.paste_text_to_file_limit = remaining;
        user.txttofilereset = reset_time;
    }
    else if (featurename == "image_gen") {
        user.image_gen_limit = remaining;
        user.image_gen_reset = reset_time;
    }
}
size_t AppState::GetChatCount() {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    return m_chat_map.size();
}
void AppState::RenameChatItem(const std::string& chatId, const std::string& newTitle) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        auto it = m_chat_map.find(chatId);
        if (it != m_chat_map.end()) {
            it->second.title = newTitle;
        }
    }
    emit EventDispatcher::instance()->chatListUpdated();
}
std::string AppState::GetChatTitle(const std::string& chatId) {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    auto it = m_chat_map.find(chatId);
    if (it != m_chat_map.end() && !it->second.title.empty()) {
        return it->second.title;
    }
    return chatId.substr(0, 12);
}
void AppState::DeleteChatItem(const std::string& chatId) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        m_chat_map.erase(chatId);
        open_chats.erase(chatId);
    }
    {
        std::lock_guard<std::mutex> lock(m_messages_mutex);
        ChatIdToText.erase(chatId);
    }
    {
        std::lock_guard<std::mutex> lock(m_inputs_mutex);
        ChatInputs.erase(chatId);
        Chat_Parent_Id.erase(chatId);
    }
    emit EventDispatcher::instance()->chatListUpdated();
}
void AppState::SwapChatId(const std::string& oldId, const std::string& newId) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        if (m_chat_map.find(oldId) != m_chat_map.end()) {
            ChatItem item = m_chat_map[oldId];
            item.id = newId;
            m_chat_map.erase(oldId);
            m_chat_map[newId] = item;
        }
        if (open_chats.find(oldId) != open_chats.end()) {
            open_chats.erase(oldId);
            open_chats.insert(newId);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        if (m_chat_status_map.find(oldId) != m_chat_status_map.end()) {
            m_chat_status_map[newId] = m_chat_status_map[oldId];
            m_chat_status_map.erase(oldId);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_messages_mutex);
        if (ChatIdToText.find(oldId) != ChatIdToText.end()) {
            ChatIdToText[newId] = std::move(ChatIdToText[oldId]);
            ChatIdToText.erase(oldId);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_inputs_mutex);
        if (ChatInputs.find(oldId) != ChatInputs.end()) {
            ChatInputs[newId] = std::move(ChatInputs[oldId]);
            ChatInputs.erase(oldId);
        }
        if (Chat_Parent_Id.find(oldId) != Chat_Parent_Id.end()) {
            Chat_Parent_Id[newId] = Chat_Parent_Id[oldId];
            Chat_Parent_Id.erase(oldId);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_windows_mutex);
        if (Opened_Window.find(oldId) != Opened_Window.end()) {
            Opened_Window[newId] = std::move(Opened_Window[oldId]);
            Opened_Window.erase(oldId);
        }
    }

    emit EventDispatcher::instance()->chatIdSwapped(oldId, newId);
    emit EventDispatcher::instance()->chatListUpdated();
}
void AppState::UpdateChatStatus(const std::string& chatId, Status status, const std::string& error_txt) {
    {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        m_chat_status_map[chatId].status = status;
        m_chat_status_map[chatId].error_txt = error_txt;
    }
    // Emit signal AFTER releasing lock
    emit EventDispatcher::instance()->chatListUpdated();
    emit EventDispatcher::instance()->chatMessageUpdated(chatId);
}


bool AppState::GetChatStatus(const std::string& chatId, Status& outStatus, std::string& outError) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    auto it = m_chat_status_map.find(chatId);
    if (it != m_chat_status_map.end()) {
        outStatus = it->second.status;
        outError = it->second.error_txt;
        return true;
    }
    return false;
}
void AppState::Add_Window(
    std::unique_ptr<WindowState> ws,
    const std::string& chatId)
{
    std::lock_guard<std::mutex> lock(m_windows_mutex);
    Opened_Window[chatId] = std::move(ws);
}
void AppState::Remove_Window(const std::string& ChatId) {
    std::lock_guard<std::mutex> lock(m_windows_mutex);
    Opened_Window.erase(ChatId);
}
WindowState* AppState::Get_Window(
    const std::string& chatId) {
    std::lock_guard<std::mutex> lock(m_windows_mutex);
    auto it = Opened_Window.find(chatId);
    if (it != Opened_Window.end()) {
        return it->second.get();
    }
    return nullptr;
}
void AppState::Model_Token_Info(const std::string& slug, const long long& max_tokens) {
    std::lock_guard<std::mutex> lock(m_models_mutex);
    Models_Token[slug] = max_tokens;
}
void AppState::UpdateChatTokens(const std::string& chatId, long long tokens) {
    {
        std::lock_guard<std::mutex> lock(m_chat_list_mutex);
        m_chat_map[chatId].current_tokens = tokens;
    }
    emit EventDispatcher::instance()->chatListUpdated();
}
long long AppState::GetChatTokens(const std::string& chatId) {
    std::lock_guard<std::mutex> lock(m_chat_list_mutex);
    auto it = m_chat_map.find(chatId);
    if (it != m_chat_map.end()) {
        return it->second.current_tokens;
    }
    return 0;
}
long long AppState::GetModelMaxTokens(const std::string& slug) {
    std::lock_guard<std::mutex> lock(m_models_mutex);
    auto it = Models_Token.find(slug);
    if (it != Models_Token.end()) {
        return it->second;
    }
    return 128000;
}
void AppState::Update_Mode(const std::string& chat_id, Modes mode) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    ChatInputs[chat_id].mode = mode;
}
