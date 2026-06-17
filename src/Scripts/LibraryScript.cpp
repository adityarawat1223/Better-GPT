#include "LibraryScript.h"
#include "Helpers/JsEscaper.h"

void LibraryScript() {

    std::string token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string build_num = AppState::GetHeaders("OAI-Client-Build-Number");


    std::string js = R"((async () => {
    try {
        const authToken = `)" + EscapeForJs(token) + R"(`;
        const oaiIs    = `)" + EscapeForJs(oai_is) + R"(`;
        const sessId   = `)" + EscapeForJs(session_id) + R"(`;
        const devId    = `)" + EscapeForJs(device_id) + R"(`;
        const bldNum   = `)" + EscapeForJs(build_num) + R"(`;

        const payload = {
            limit: 50,
            cursor: null
        };

        const response = await fetch('https://chatgpt.com/backend-api/files/library', {
            method: 'POST',
            headers: {
                'Authorization': authToken,
                'x-oai-is': oaiIs,
                'oai-session-id': sessId,
                'oai-device-id': devId,
                'oai-client-build-number': bldNum,
                'oai-language': 'en-US',
                'Content-Type': 'application/json',
                'Accept': '*/*',
                'Referer': 'https://chatgpt.com/'
            },
            credentials: 'include',
            body: JSON.stringify(payload)
        });

        console.log('[Library Info] Status:', response.status);

        const text = await response.text();
        console.log('[Library Info] Body:', text);
        

    } catch (err) {
        console.error('[Library Info]', err);
    }
})();)";

    auto browser = AppState::GetHiddenBrowser();
    browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
};