#include "ChatExecScript.h"
#include "Helpers/JsEscaper.h"

void ChatExecScript(const std::string &ChatId) {

    std::string token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string build_num = AppState::GetHeaders("OAI-Client-Build-Number");

    std::string js = R"((async () => {
        try {
            const chatId   = `)" + EscapeForJs(ChatId) + R"(`;
            const authToken = `)" + EscapeForJs(token) + R"(`;
            const oaiIs    = `)" + EscapeForJs(oai_is) + R"(`;
            const sessId   = `)" + EscapeForJs(session_id) + R"(`;
            const devId    = `)" + EscapeForJs(device_id) + R"(`;
            const bldNum   = `)" + EscapeForJs(build_num) + R"(`;

            const response = await fetch(`https://chatgpt.com/backend-api/conversation/${chatId}`, {
                method: 'GET',
                headers: {
                    'Authorization': authToken,
                    'x-oai-is': oaiIs,
                    'oai-session-id': sessId,
                    'oai-device-id': devId,
                    'oai-client-build-number': bldNum,
                    'oai-language': 'en-US',
                    'Accept': '*/*',
                    'Referer': `https://chatgpt.com/c/${chatId}`
                },
                credentials: 'include'
            });

            if (!response.ok) {
                fetch('https://chatgpt.com/status-sync', {
                    method: 'POST',
                    body: JSON.stringify({ chatId: ` + chatId + `, status: 5, error: 'HTTP ' + response.status })
                }).catch(e => {});
            } else {
                fetch('https://chatgpt.com/status-sync', {
                    method: 'POST',
                    body: JSON.stringify({ chatId: ` + chatId + `, status: 2 /* ResRecieved */ })
                }).catch(e => {});
            }

            const text = await response.text();
            console.log(chatId);

        } catch (err) {
            fetch('https://chatgpt.com/status-sync', {
                method: 'POST',
                body: JSON.stringify({ chatId: ` + chatId + `, status: 5, error: err.toString() })
            }).catch(e => {});
        }
    })();)";

    auto browser = AppState::GetHiddenBrowser();
    browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
}
