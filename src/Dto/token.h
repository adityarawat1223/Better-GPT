#pragma once
#include <string>
#include "include/cef_browser.h"

struct TokenInfo {
    std::string bearertoken = "no";
    CefRefPtr<CefBrowser> mainBrowser;  
    CefRefPtr<CefBrowser> HiddenBrowser;
};