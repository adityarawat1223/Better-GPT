#include <string>
#include <iostream>
#include <algorithm>
#include <limits>
#include <vector>
#include "include/cef_request_context.h"
#include "include/cef_app.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_request_handler.h"
#include "include/cef_cookie.h"
#include "Dto/token.h"
#include "logger.h"
#include "Parsers/ChatListParser.h"
#include "memdb/AppState.h"
#include "ChatTokenGrabber.h"
#include "ReqRunner.h"

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

CefRefPtr<CefResponseFilter> Logger::Handler::GetResourceResponseFilter(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response) {
    std::string url = request->GetURL().ToString();
    if (url.find("/backend-api/conversations?") != std::string::npos) {
        return new ChatListParser(&completed, url);
    }
    return nullptr;
};

CefRefPtr<CefResourceRequestHandler> Logger::Handler::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling)
{
    if (!AppState::GetMainBrowser()) return nullptr;
    if (browser->GetIdentifier() != AppState::GetMainBrowser()->GetIdentifier())
        return nullptr;

    return this;
}
void Logger::Handler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
 

    if (AppState::GetMainBrowser()) {
        browser->GetHost()->CloseBrowser(true);
        return;
    }

    AppState::UpdateBrowserInstance(browser);
}

bool Logger::Handler::OnBeforeBrowse(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) {
    if (!AppState::GetMainBrowser()) return false;
    if (browser->GetIdentifier() != AppState::GetMainBrowser()->GetIdentifier())
        return false;
    std::string url = request->GetURL().ToString();
    if (url.find("chatgpt.com") != std::string::npos ||
        url.find("openai.com") != std::string::npos ||
        url.find("auth.openai.com") != std::string::npos ||
        url.find("accounts.google.com") != std::string::npos ||
        url.find("appleid.apple.com") != std::string::npos ||
        url.find("login.live.com") != std::string::npos)
    {
        return false;
    }

    return true; 
}

bool Logger::Handler::OnBeforePopup(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id,
    const CefString& target_url, const CefString& target_frame_name,
    CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture,
    const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue>& extra_info, bool* no_javascript_access) {
    browser->GetMainFrame()->LoadURL(target_url);
    return true;
   
}
void Logger::Handler::OnLoadEnd(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int httpStatusCode) {

    if (!frame->IsMain()) return;
    std::string js = R"(

(() => {
    if (window.__sentinel_hooked__)
        return;

    window.__sentinel_hooked__ = true;
    console.log("[HOOK] Installing sentinel hook");
    const originalFetch =
        window.fetch;
    window.fetch =
        async (...args) => {
        try {

           const [resource, options] =
                args;

            const url =
                typeof resource === "string"
                    ? resource
                    : resource?.url;

            if (
                url &&
                url.includes(
                    "/backend-api/sentinel/chat-requirements/finalize"))
            {
                console.log(
                    "[HOOK] Sentinel finalize intercepted");

                let body = null;

                // normal fetch(url, { body })
                if (options?.body)
                {
                    body = options.body;
                }

                // fetch(Request)
                else if (resource instanceof Request)
                {
                    try {

                        const cloned =
                            resource.clone();

                        body =
                            await cloned.text();

                    } catch(err) {

                        console.error(
                            "[HOOK] Failed reading Request body",
                            err);
                    }
                }

                if (body)
                {
                    console.log(
                        "[HOOK] Sending token-sync");

                    fetch(
                        "https://chatgpt.com/token-sync",
                        {
                            method: "POST",

                            headers:
                            {
                                "Content-Type":
                                    "application/json"
                            },

                            body
                        });
                }
                else
                {
                    console.log(
                        "[HOOK] No body found");
                }
            }

        }
        catch(err)
        {
            console.error(
                "[HOOK ERROR]",
                err);
        }

        return originalFetch(...args);
    };

    console.log(
        "[HOOK] Installed successfully");

})();

)";

    frame->ExecuteJavaScript(
        js,
        frame->GetURL(),
        0);

}

cef_return_value_t Logger::Handler::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) {

    std::string url = request->GetURL().ToString();
    if (url.find("openai.com") == std::string::npos &&
        url.find("chatgpt.com") == std::string::npos) {
        return RV_CONTINUE;
    }

    CefRequest::HeaderMap headers;
    request->GetHeaderMap(headers);

 
    auto it = headers.find("Authorization");
    if (it != headers.end()) {
        std::string bearer = it->second;
        if (AppState::GetTokenInfo() != bearer) {
            AppState::UpdateBearerToken(bearer);
        }
    }

    if (url.find("/backend-api/") != std::string::npos) {
        std::vector<std::string> keys = {
     "X-OAI-IS",
     "OAI-Session-Id",
     "OAI-Device-Id",
     "OAI-Client-Build-Number",
     "OAI-Client-Version",
     "OAI-Language"
        };
        for (auto& key : keys) {
            auto it = headers.find(key);
            if (it != headers.end()) {
                std::string temp = it->second;
                AppState::UpdateHeaders(key, temp);
            }
        }

    }
    if (url.find("/token-sync") != std::string::npos) {
        CefRefPtr<CefPostData> post = request->GetPostData();
        if (post) {
            std::string body;
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

            if (!body.empty()) {
                ChatTokenGrabber(body);
                if (completed.fetch_add(1) + 1 == 2) {
                    if (AppState::GetMainBrowser()) {
                        CefRefPtr<CefBrowser> browser = AppState::GetMainBrowser();
                        AppState::UpdateBrowserInstance(nullptr);
                        CefPostTask(TID_UI, new CloseBrowserTask(browser));
                    }
                }
            }

        }
        return RV_CANCEL;
    }


    return RV_CONTINUE;
}

void Logger::log_window() {
    if (AppState::GetMainBrowser()){
        return;
    }

    CefWindowInfo info;
    info.SetAsPopup(nullptr, "Login");
    CefBrowserSettings browser_settings;

    CefBrowserHost::CreateBrowser(
        info,
        new Logger::Handler(),
        "https://chatgpt.com",
        browser_settings,
        nullptr,
        nullptr
    );
}