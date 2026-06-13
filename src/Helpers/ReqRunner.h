#pragma once
#include "include/cef_client.h"
#include "include/cef_request_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_render_handler.h"
#include "include/cef_display_handler.h"
#include <string>
#include <queue>
#include <mutex>
#include "Dto/fileRef.h"
#include "Dto/chatItem.h"
#include <atomic>

class ReqRunner :
    public CefClient,
    public CefRequestHandler,
    public CefLifeSpanHandler,
    public CefResourceRequestHandler,
    public CefLoadHandler,
    public CefRenderHandler,
    public CefDisplayHandler

{
public:
    ReqRunner();

    inline static std::queue<std::string> PendingChats;
    inline static std::mutex QueueMutex;
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override
    {
        return this;
    }

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        rect = CefRect(0, 0, 1280, 720);
    }

    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
        const RectList& dirtyRects, const void* buffer,
        int width, int height) override {
    }

    CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response) override;

    cef_return_value_t OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback) override;

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

    bool OnBeforePopup(
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
        bool* no_javascript_access) override;
    
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
        cef_log_severity_t level,
        const CefString& message,
        const CefString& source,
        int line) override;
    void OnLoadEnd(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int httpStatusCode) override;
    void OnResourceLoadComplete(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response,
        URLRequestStatus status,
        int64_t received_content_length) override;
    static void ChatTextExecution(const std::string& chatid);
    static void FetchChatList(int offset);
    static void CreateBrowser();
    static void FileExecution(const std::string& FileId);
    static void RemoveLibraryIPC(const std::string& fid);
    static void StatusSyncIPC(const std::string& chatId, Status status, const std::string& error = "");
    
    static void FileDownloader(std::string url);
    static void FileUploaderInit(const FileRef& fileref);
    static void FileUploadFin(const std::string& file_id,
        const std::string& filename,
        const std::string& chat_id,
        const std::string& origination_message_id, const std::string& local_id);
    static void Send_Message(const std::string& Chat_Id, const std::string Input);
    static void GetToken();
    static void GetMe();
    static void PfpDownload(const std::string & Url);
    static void FetchLibrary();
    static void MessageStreamSync(const std::string& body);
    static void RenameSync(const std::string& body);
    static void DeleteSync(const std::string& body);
    static void RenameChat(const std::string& chatId, const std::string& newName);
    static void DeleteChat(const std::string& chatId);

private:
    IMPLEMENT_REFCOUNTING(ReqRunner);
};