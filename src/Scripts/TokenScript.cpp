#include "TokenScript.h"
#include "Helpers/JsEscaper.h"
#include <iostream>
void TokenScript() {

    auto browser = AppState::GetHiddenBrowser();
    if (!browser || !browser->GetMainFrame()) {
        std::cout << "[Master Clock Error] Single hidden browser frame reference is invalid!" << std::endl;
        return;
    }

    std::string bearer_token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string build_number = AppState::GetHeaders("OAI-Client-Build-Number");

    std::string prepare_token = AppState::GetHeaders("openai-sentinel-chat-requirements-prepare-token");
    std::string proof_token = AppState::GetHeaders("openai-sentinel-proof-token");
    std::string turnstile_token = AppState::GetHeaders("openai-sentinel-turnstile-token");

    std::string js_payload = R"(
        (async () => {
            try {
                const prep = `)" + EscapeForJs(prepare_token) + R"(`;
                const pow  = `)" + EscapeForJs(proof_token) + R"(`;
                const turn = `)" + EscapeForJs(turnstile_token) + R"(`;

                const authToken = `)" + EscapeForJs(bearer_token) + R"(`;
                const oaiIs     = `)" + EscapeForJs(oai_is) + R"(`;
                const sessId    = `)" + EscapeForJs(session_id) + R"(`;
                const devId     = `)" + EscapeForJs(device_id) + R"(`;
                const bldNum    = `)" + EscapeForJs(build_number) + R"(`;

                const payloadBody = {
                    "prepare_token": prep,
                    "proofofwork": pow,
                    "turnstile": turn
                };

                const response = await fetch('https://chatgpt.com/backend-api/sentinel/chat-requirements/finalize', {
                    method: 'POST',
                    headers: {
                        'Authorization': authToken,
                        'X-OAI-IS': oaiIs,
                        'OAI-Session-Id': sessId,
                        'OAI-Device-Id': devId,
                        'OAI-Client-Build-Number': bldNum,
                        'Content-Type': 'application/json'
                    },
                    credentials: "include",
                    body: JSON.stringify(payloadBody)
                });

                console.log("[Token Sync Status]:", response.status);
            } catch (err) {
                console.error('[ReqRunner Payload Refresh Error]', err);
            }
        })();
    )";
    std::cout << "[ReqRunner] Dispatching validated finalize envelope..." << std::endl;
    browser->GetMainFrame()->ExecuteJavaScript(js_payload, "https://chatgpt.com", 0);
}