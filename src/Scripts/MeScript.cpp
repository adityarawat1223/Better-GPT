#include "MeScript.h"
#include "Helpers/JsEscaper.h"


void MeScript() {


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

            const response = await fetch(`https://chatgpt.com/backend-api/me`, {
                method: 'GET',
                headers: {
                    'Authorization': authToken,
                    'x-oai-is': oaiIs,
                    'oai-session-id': sessId,
                    'oai-device-id': devId,
                    'oai-client-build-number': bldNum,
                    'oai-language': 'en-US',
                    'Accept': '*/*',
                    'Referer': `https://chatgpt.com/`
                },
                credentials: 'include'
            });

            console.log("[ME info] Status:", response.status);
        } catch (err) {
            console.error("[Me Info]", err);
        }
    })();)";

    auto browser = AppState::GetHiddenBrowser();
    browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
};