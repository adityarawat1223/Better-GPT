#include "RenameScript.h"


void RenameScript(const std::string &chatid, const std::string &newname) {

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
        const chatid   = `)" + EscapeForJs(chatid) + R"(`;
        const newname  = `)" + EscapeForJs(newname) + R"(`;

        const payload = {
            title: newname
        };

        const response = await fetch(`https://chatgpt.com/backend-api/conversation/${chatid}`, {
            method: 'PATCH',
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

        console.log('[Rename Script] Status:', response.status);

        const text = await response.text();
        console.log('[Rename Script] Body:', text);

        if (response.ok) {
            try {
                const result = JSON.parse(text);
                if (result.success === true) {
                    await fetch('https://chatgpt.com/rename-sync', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ chatId: chatid, newTitle: newname })
                    });
                }
            } catch (e) {}
        }

    } catch (err) {
        console.error('[Rename Script]', err);
    }
})();)";

    auto browser = AppState::GetHiddenBrowser();
    if (browser && browser->GetMainFrame()) {
        browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
    }
}