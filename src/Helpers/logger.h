#pragma once
#include <string>
#include <vector>
#include "include/cef_request_context.h"
#include "include/cef_app.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_request_handler.h"
#include "include/cef_cookie.h"
#include "../Dto/token.h"
#include "../Parsers/ChatListParser.h"
#include "include/cef_load_handler.h"

class Logger {
private:
    class Handler :
        public CefClient,
        public CefRequestHandler,
        public CefResourceRequestHandler,
        public CefLifeSpanHandler,
        public CefLoadHandler

    {
    private:
        IMPLEMENT_REFCOUNTING(Handler);
       

    public:
        std::atomic<int> completed{ 0 };

        CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
        CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

        CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request,
            bool is_navigation,
            bool is_download,
            const CefString& request_initiator,
            bool& disable_default_handling) override;

        CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request,
            CefRefPtr<CefResponse> response) override;

        void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
        void OnLoadEnd(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            int httpStatusCode) override;
        bool OnBeforeBrowse(
            CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) override;

        bool OnBeforePopup(
            CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id,
            const CefString& target_url, const CefString& target_frame_name,
            CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture,
            const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
            CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
            CefRefPtr<CefDictionaryValue>& extra_info, bool* no_javascript_access) override;

        cef_return_value_t OnBeforeResourceLoad(
            CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override;
    };

public:

    void log_window();
};