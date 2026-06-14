#include "ReqRunner.h"
#include "Parsers/ChatTextParser.h"
#include "Parsers/ChatListParser.h"
#include "memdb/AppState.h"
#include "Helpers/Sanitizer.h"
#include <iostream>
#include "Parsers/FileLinkParser.h"
#include "Parsers/FileParser.h"
#include "Helpers/json.hpp"
#include  "Parsers/LibraryParser.h"
#include "Dto/fileRef.h"
#include "Parsers/FileParser.h"
#include "Parsers/ChatTextParser.h"
#include "Parsers/ChatTokenParser.h"
#include "Parsers/MeParser.h"
#include "Parsers/LibraryParser.h"
#include "Parsers/FileLinkParser.h"
#include "Parsers/ModelsParser.h"
#include "Parsers/InitParser.h"
#include "FileUploader.h"
#include "FileFin.h"
#include "ChatTokenGrabber.h"
#include "Parsers/ChatTokenParser.h"
#include "Parsers/MeParser.h"
#include "Scripts/FileScripts.h"
#include "Scripts/TokenScript.h"
#include "Scripts/MessageScript.h"
#include "Scripts/ChatExecScript.h"
#include "Scripts/MeScript.h"
#include "Scripts/LibraryScript.h"
#include "Scripts/ChatListFetchScript.h"
#include "Scripts/RenameScript.h"
#include "Scripts/ChatDeleteScript.h"
#include "Scripts/InitScript.h"
using json = nlohmann::json;
#include "Fileinit.h"
#include "Serializer/MessageSerializer.h"
#include "Scripts/ModelScript.h"
#include <regex>


static std::string GetBody(CefRefPtr<CefRequest> request) {
    std::string body;
    if (!request) return body;

    CefRefPtr<CefPostData> post = request->GetPostData();
    if (!post) return body;

    CefPostData::ElementVector elements;
    post->GetElements(elements);
    for (auto& el : elements) {
        if (el->GetType() != PDE_TYPE_BYTES) continue;
        size_t sz = el->GetBytesCount();
        if (sz == 0) continue;

        std::string buf(sz, '\0');
        el->GetBytes(sz, buf.data());
        body += buf;
    }
    return body;
}

static std::string extractIdFromUrl(const std::string& url) {
    std::regex id_regex("id=([^&]*)");
    std::smatch match;

    if (std::regex_search(url, match, id_regex)) {

        return match[1].str();
    }
    
    return ""; 
};

static std::string getExtensionFromMime(const std::string& mimeType) {
    size_t slash_pos = mimeType.find('/');
    if (slash_pos == std::string::npos || slash_pos == mimeType.length() - 1) {
        return ""; 
    }
    
    return mimeType.substr(slash_pos + 1);
};



ReqRunner::ReqRunner() {}


CefRefPtr<CefResponseFilter> ReqRunner::GetResourceResponseFilter(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response) {

    std::string url = request->GetURL().ToString();

    if (url.find("/backend-api/conversation/init") != std::string::npos) {
        return new InitParser();
    }

    if (url.find("/backend-api/conversation/") != std::string::npos)
        return new ChatTextParser();

    if (url.find("/backend-api/conversations?") != std::string::npos) {
        return new ChatListParser(nullptr, url);
    }

    if (url.find("https://chatgpt.com/backend-api/files/download/file") != std::string::npos) {
        return new FileLinkParser();
    }

    if (url.find("https://chatgpt.com/backend-api/sentinel/chat-requirements/finalize") != std::string::npos) {
        return new ChatTokenParser();
    }

    if (url.find("https://chatgpt.com/backend-api/me") != std::string::npos) {
        return new MeParser();
    }

    if (url.find(
        "https://chatgpt.com/backend-api/estuary/content?id")
        != std::string::npos)
    {

        std::string mimetype = getExtensionFromMime(response->GetHeaderByName("Content-type"));
        std::string fileid = extractIdFromUrl(url);
       
        std::string filename = fileid + "." + mimetype;

         if(fileid.empty() || mimetype.empty()){
            filename = "temp.bin";
        }

        
        return new FileParser(filename);
    }
    if (url.find("https://cdn.auth0.com/avatars/") != std::string::npos) {
        std::string user_id = AppState::Get_User_ID();

        if (!user_id.empty()) {
            std::cout << "here avatar" << std::endl;
            return new FileParser(user_id);
        }
    };

    if (url.find("https://chatgpt.com/backend-api/files/library") != std::string::npos) {
        return new LibraryParser();
    }

    if (url.find("https://chatgpt.com/backend-api/models") != std::string::npos) {
        return new ModelsParser();
    }

    return nullptr;
}

cef_return_value_t ReqRunner::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback) {
    std::string url = request->GetURL().ToString();

    if (url.find("/upload-sync") != std::string::npos)
    {
        std::string body = std::move(GetBody(request));
           if (!body.empty()) {
                FileInit(body);
           }
        
        return RV_CANCEL;
    }

    if (url.find("/stream-sync") != std::string::npos)
    {
        std::string body = std::move(GetBody(request));
        if (!body.empty()) {
            MessageStreamSync(body);
        }
        return RV_CANCEL;
    }
   
    if (url.find("/rename-sync") != std::string::npos)
    {
        std::string body = std::move(GetBody(request));
        if (!body.empty()) {
            RenameSync(body);
        }
        return RV_CANCEL;
    }

    if (url.find("/delete-sync") != std::string::npos)
    {
        std::string body = std::move(GetBody(request));
        if (!body.empty()) {
            DeleteSync(body);
        }
        return RV_CANCEL;
    }
   
    if (url.find("file-processed") != std::string::npos)
    {
        std::string body = std::move(GetBody(request));
        if (!body.empty()) {
            Filefin(body);
        }
        return RV_CANCEL;
     
 }

    auto type = request->GetResourceType();
    switch (type) {
    case RT_IMAGE:
    case RT_FONT_RESOURCE:
    case RT_MEDIA:
    case RT_STYLESHEET:
    case RT_SUB_RESOURCE:
    case RT_SCRIPT:
        return RV_CANCEL;
    default:
        return RV_CONTINUE;
    }
}

void ReqRunner::OnResourceLoadComplete(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response,
    URLRequestStatus status,
    int64_t received_content_length)
{

}
void ReqRunner::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    if (!AppState::GetHiddenBrowser()) {
        AppState::UpdateHiddenBrowser(browser);
    }
    else {
        browser->GetHost()->CloseBrowser(true);
    }
}
bool ReqRunner::OnBeforePopup(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int popup_id,
    const CefString& target_url,
    const CefString& target_frame_name,
    CefLifeSpanHandler::WindowOpenDisposition target_disposition,
    bool user_gesture,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue>& extra_info,
    bool* no_javascript_access) {
    return true; 
}


void ReqRunner::OnLoadEnd(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int httpStatusCode) {

    if (!frame->IsMain()) return;   

      GetToken();
      GetMe();
      InitScript();
      ModelScript();
    std::lock_guard<std::mutex> lock(QueueMutex);
    while (!PendingChats.empty()) {
        std::string id = PendingChats.front();
        PendingChats.pop();
        ChatTextExecution(id);
    }
}
void ReqRunner::ChatTextExecution(const std::string& ChatId) {
    if (AppState::HasChatData(ChatId)) return;

    auto browser = AppState::GetHiddenBrowser();
    if (!browser) {
        std::lock_guard<std::mutex> lock(QueueMutex);
        PendingChats.push(ChatId);
        CreateBrowser();
        return;
    }

    ChatExecScript(ChatId);
}
void ReqRunner::FileExecution(const std::string& FileId) {


    FileScripts::FileExec(FileId);
 
}

void ReqRunner::FileDownloader(std::string url) {
        
    FileScripts::FileDownload(url);
  
};

bool ReqRunner::OnConsoleMessage(
    CefRefPtr<CefBrowser> browser,
    cef_log_severity_t level,
    const CefString& message,
    const CefString& source,
    int line)
{
    // std::cout << message.ToString() << std::endl;
    return false;
}
void ReqRunner::CreateBrowser() {
    if (AppState::GetHiddenBrowser()) return;

    CefWindowInfo info;
    info.SetAsWindowless(nullptr);

    CefBrowserSettings settings;
    settings.windowless_frame_rate = 1;

    CefBrowserHost::CreateBrowser(
        info,
        new ReqRunner(),
        "https://chatgpt.com",
        settings,
        nullptr,
        nullptr);
}

void ReqRunner::FileUploaderInit(const FileRef& fileref) 
{
    FileScripts::FileUploaderInit(fileref);
 }

void ReqRunner::FileUploadFin
(
    const std::string& file_id,
    const std::string& filename,
    const std::string& chat_id,
    const std::string& origination_message_id,
    const std::string& local_id
)

{
  
    FileScripts::FileUploadFin(file_id, filename, chat_id, origination_message_id, local_id);
    std::cout << "firing upload request to process stream" << std::endl;
    
}   


void ReqRunner::Send_Message(const std::string& chatid, const std::string input) {

    AppState::Add_Input(input, chatid);
    InputBox inputbox = AppState::Get_Input_Box(chatid);

    std::string out;
    MessageSerializer(inputbox, out, chatid);
    MessageScript(out, chatid);
}

void ReqRunner::GetToken() {
    
    TokenScript();
}

void ReqRunner::GetMe() {
    MeScript();
}

void ReqRunner::PfpDownload(const std::string &url) {
    // std::cout << url << std::endl;
    FileScripts::NoCredFileDownload(url);
}


void ReqRunner::FetchLibrary() {
    LibraryScript();
}

void ReqRunner::MessageStreamSync(const std::string& body)
{
    try
    {
        json j = json::parse(body);

        if (!j.contains("conversation_id"))
            return;

        const std::string chat_id =
            j["conversation_id"].get<std::string>();

        bool is_new_chat = !AppState::HasChatEntry(chat_id);
        if (is_new_chat) {
            std::string temp_id = j.contains("temp_id") && j["temp_id"].is_string() ? j["temp_id"].get<std::string>() : "";
            if (!temp_id.empty() && temp_id != chat_id) {
                AppState::SwapChatId(temp_id, chat_id);
                std::cout << "[MessageStreamSync] Swapped temp chat " << temp_id << " to real chat " << chat_id << std::endl;
            }
            else {
                std::vector<ChatMessage> empty;
                AppState::AddChatsToMap(chat_id, empty);
                AppState::AddChat(chat_id);
                std::cout << "[MessageStreamSync] New chat detected: " << chat_id << std::endl;
            }
           
        }

        if (j.contains("final_message_id") && j["final_message_id"].is_string()) {
            const std::string final_id =
                j["final_message_id"].get<std::string>();

            if (!final_id.empty()) {
                AppState::Set_Parent(chat_id, final_id);
            }
        }

        bool has_user_msg = j.contains("user_message") && j["user_message"].is_object();
        bool has_assistant_msg = j.contains("assistant_message") && j["assistant_message"].is_object();

        std::string user_msg_id, user_content;
        std::string assistant_msg_id, assistant_content, assistant_thinking;

        if (has_user_msg) {
            const auto& userObj = j["user_message"];
            user_msg_id = userObj.contains("id") && userObj["id"].is_string() ? userObj["id"].get<std::string>() : "";
            user_content = userObj.contains("content") && userObj["content"].is_string() ? userObj["content"].get<std::string>() : "";
        }

        if (has_assistant_msg) {
            const auto& aiObj = j["assistant_message"];
            assistant_msg_id = aiObj.contains("id") && aiObj["id"].is_string() ? aiObj["id"].get<std::string>() : "";
            assistant_content = aiObj.contains("content") && aiObj["content"].is_string() ? aiObj["content"].get<std::string>() : "";
            assistant_thinking = aiObj.contains("thinking") && aiObj["thinking"].is_string() ? aiObj["thinking"].get<std::string>() : "";
        }


        if (has_user_msg && !user_msg_id.empty())
        {
            ChatMessage msg;
            msg.user = true;
            msg.message_id = user_msg_id;
            msg.timestamp = (uint64_t)time(nullptr);

            InputBox inbox = AppState::Get_Input_Box(chat_id);
            for (const auto& pair : inbox.assets) {
                msg.assets.push_back(pair.second);
            }

            Santizer(msg, user_content);

            if (AppState::InsertChatMessageIfMissing(chat_id, std::move(msg))) {
                AppState::Clear_Input_Assets(chat_id);
            }
        }

      
        if (has_assistant_msg && !assistant_msg_id.empty())
        {
            bool has_visible_content = !assistant_content.empty() || !assistant_thinking.empty();

            if (!has_visible_content)
                return;

            ChatMessage msg;
            msg.user = false;
            msg.message_id = assistant_msg_id;
            msg.timestamp = (uint64_t)time(nullptr);
            msg.thinking = assistant_thinking;

            Santizer(msg, assistant_content);

            AppState::UpsertChatMessage(chat_id, std::move(msg));
        }

        if (j.contains("is_complete") && j["is_complete"].is_boolean()) {
            if (j["is_complete"].get<bool>()) {
                ReqRunner::FetchChatList(0);
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[MessageStreamSync Error] "
            << e.what()
            << std::endl;
    }
}

void ReqRunner::RenameSync(const std::string& body) {
    try {
        json data = json::parse(body);
        if (data.is_object() && data.contains("chatId") && data.contains("newTitle")) {
            std::string chatId = data["chatId"];
            std::string newTitle = data["newTitle"];
            AppState::RenameChatItem(chatId, newTitle);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[RenameSync Error] " << e.what() << std::endl;
    }
}

void ReqRunner::DeleteSync(const std::string& body) {
    try {
        json data = json::parse(body);
        if (data.is_object() && data.contains("chatId") && data.contains("success") && data["success"] == true) {
            std::string chatId = data["chatId"];
            AppState::DeleteChatItem(chatId);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[DeleteSync Error] " << e.what() << std::endl;
    }
}

void ReqRunner::FetchChatList(int offset) {
    ChatListFetchScript(offset);
}

void ReqRunner::RenameChat(const std::string& chatId, const std::string& newName) {
    RenameScript(chatId, newName);
}

void ReqRunner::DeleteChat(const std::string& chatId) {
    ChatDeleteScript(chatId);
}
