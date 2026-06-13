#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "include/cef_browser.h"
#include "Dto/chatitem.h"
#include "Dto/token.h"
#include "Dto/ChatText.h"
#include <unordered_map>
#include <set>
#include "Dto/Windows.h"
#include "Dto/Models.h"
#include "Dto/User.h"
#include "Dto/fileRef.h"
#include <queue>
#include <condition_variable>
#include <unordered_set>
#include <filesystem>
#include <cstdlib>
#include <optional>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

struct SearchJob {
    int searchId;
    std::string query;
    std::vector<std::string> chatIds;
};

class AppState {
private:
    static inline std::mutex m_auth_mutex;
    static inline std::mutex m_chat_list_mutex;
    static inline std::mutex m_messages_mutex;
    static inline std::mutex m_models_mutex;
    static inline std::mutex m_user_mutex;
    static inline std::mutex m_inputs_mutex;
    static inline std::mutex m_queue_mutex;
    static inline std::mutex m_search_mutex;
    static inline std::mutex m_library_mutex;
    static inline std::mutex m_status_mutex;
    static inline std::mutex m_windows_mutex;

    static inline std::unordered_map<std::string, ChatItem> m_chat_map;
    static inline TokenInfo m_token_info;
    static inline std::unordered_map<std::string, std::string> headers;
    static inline std::unordered_map<std::string, std::vector<ChatMessage>> ChatIdToText;
    static inline std::set<std::string> open_chats;
    static inline std::set<Models> ModelsInfo;
    static inline User user;
    static inline std::unordered_map<std::string, InputBox> ChatInputs;
    static inline std::unordered_map<std::string, std::string> Chat_Parent_Id;
    static inline std::queue<FileRef> UploadQueue;
    static inline std::condition_variable UploadCV;
    static inline std::vector<FileRef> library;
    static inline std::unordered_map<std::string, std::unique_ptr<WindowState>> Opened_Window;
    
    struct ChatStatus {
        Status status;
        std::string error_txt;
    };
    static inline std::unordered_map<std::string, ChatStatus> m_chat_status_map;
    static inline std::unordered_map<std::string, long long> Models_Token;
    static inline bool m_has_more_chats = false;
    static inline std::atomic<int> active_search_id{0};
    static inline std::optional<SearchJob> CurrentSearchJob;
    static inline std::condition_variable SearchCV;

public:
    AppState() = delete;

    static void SetTokenInfo(const TokenInfo& info);
    static std::string GetTokenInfo();
    static void UpdateBearerToken(const std::string& token);
    static void UpdateBrowserInstance(CefRefPtr<CefBrowser> browser);
    static void UpdateHiddenBrowser(CefRefPtr<CefBrowser> browser);
    static void SetChatList(std::vector<ChatItem> fresh_list);
    static void AppendChatList(const std::vector<ChatItem>& list);
    static std::vector<ChatItem> GetChatListCopy();
    static void AppendChatItem(const ChatItem& item);
    static void ClearChatList();
    static CefRefPtr<CefBrowser> GetMainBrowser();
    static CefRefPtr<CefBrowser> GetHiddenBrowser();
    static bool HasMoreChats();
    static void SetHasMoreChats(bool hasMore);
    static void AddChatsToMap(const std::string& ChatId, std::vector<ChatMessage>& Txtarry);
    static std::vector<ChatMessage> GetChatsFromMap(const std::string& ChatId);
    static void AppendChatMessage(const std::string& ChatId, ChatMessage&& message);
    static void UpdateChatMessageById(const std::string& ChatId, ChatMessage&& message);
    static bool UpsertChatMessage(const std::string& ChatId, ChatMessage&& message);
    static bool InsertChatMessageIfMissing(const std::string& ChatId, ChatMessage&& message);
    static std::set<std::string> GetOpenChats();
    static void CloseChat(const std::string& ChatId);
    static void AddChat(const std::string& ChatId);
    static bool HasChatData(const std::string& ChatId);
    static bool HasChatEntry(const std::string& ChatId);
    static void UpdateHeaders(const std::string& key, std::string& value);
    static std::string GetHeaders(const std::string& key);
    static std::set<Models> GetModels();
    static void AddModels(const Models& md);
    static void Set_Default_Model(std::string& model);
    static std::string Get_Default_Model();
    static void Add_File_Asset(const FileRef& fileref, const std::string& ChatId, const std::string& local_id);
    static void Remove_File_Asset(const FileRef& fileref, const std::string& ChatId, const std::string& local_id);
    static void Clear_Input_Assets(const std::string& ChatId);
    static void Add_Job(const FileRef& body);
    static bool Pop_Job(FileRef& out);
    static void Submit_Search_Job(const std::string& query, const std::vector<std::string>& chatIds);
    static bool Pop_Search_Job(SearchJob& out);
    static int GetActiveSearchId();
    static bool Update_Asset_Status(UploadStatus uploadstatus, const std::string& ChatId, const std::string& local_id);
    static void Set_Parent(const std::string& ChatId, const std::string& Pid);
    static std::string Get_Parent(const std::string& ChatId);
    static void Update_Model(const std::string& model, const std::string& chat_id);
    static UploadStatus Get_Asset_Status(const std::string& ChatId, const std::string& Local_id);
    static bool Update_Upload_Url(const std::string& ChatId, const std::string& local_id, const std::string& url, const std::string& file_id);
    static bool Update_File_Processed(const std::string& ChatId, const std::string& local_id, const std::string& lib_file_id, const std::string& file_id);
    static FileRef Get_File_Asset(const std::string& ChatId, const std::string& local_id);
    static InputBox Get_Input_Box(const std::string& ChatId);
    static void Add_Input(const std::string& input, const std::string& ChatId);
    static void Update_User_Info(std::string& profurl, std::string& name, std::int64_t& created_at, std::string& email, std::string& user_id);
    static std::string Get_User_ID();
    static std::filesystem::path GetUserDir();
    static void SetLibrary(std::vector<FileRef>& new_lib);
    static std::vector<FileRef> GetLibrary();
    static void RemoveLibraryItem(const std::string& fileId);
    static User Get_User();
    static void Update_User_Feature_Limit(const std::string& featurename, int remaining, int64_t reset_time);
    static size_t GetChatCount();
    static void RenameChatItem(const std::string& chatId, const std::string& newTitle);
    static std::string GetChatTitle(const std::string& chatId);
    static void DeleteChatItem(const std::string& chatId);
    static void SwapChatId(const std::string& oldId, const std::string& newId);
    static void UpdateChatStatus(const std::string& chatId, Status status, const std::string& error_txt = "");
    static bool GetChatStatus(const std::string& chatId, Status& outStatus, std::string& outError);
    static void Add_Window(std::unique_ptr<WindowState> ws, const std::string& chatId);
    static void Remove_Window(const std::string& ChatId);
    static WindowState* Get_Window(const std::string& chatId);
    static void Model_Token_Info(const std::string& slug, const long long& max_tokens);
    static void UpdateChatTokens(const std::string& chatId, long long tokens);
    static long long GetChatTokens(const std::string& chatId);
    static long long GetModelMaxTokens(const std::string& slug);
    static void Update_Mode(const std::string& chat_id, Modes mode);
};
