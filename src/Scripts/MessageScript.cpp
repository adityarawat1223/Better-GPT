#include "MessageScript.h"
#include "Helpers/JsEscaper.h"
#include <iostream>

void MessageScript(const std::string& out, const std::string& temp_chat_id) {
    std::string token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string oai_session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string oai_device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string oai_client_build_numer = AppState::GetHeaders("OAI-Client-Build-Number");

    std::string open_sentinel_proof_token = AppState::GetHeaders("openai-sentinel-proof-token");
    std::string open_sentinel_chat_req_token = AppState::GetHeaders("openai-sentinel-chat-requirements-token");
    std::string open_sentinel_turnstile_token = AppState::GetHeaders("openai-sentinel-turnstile-token");

    std::string prepare_token = AppState::GetHeaders("openai-sentinel-chat-requirements-prepare-token");

    std::string js = R"((async () => {
    try {
        const rawPayload = `)" + EscapeForJs(out) + R"(`;
        const authToken  = `)" + EscapeForJs(token) + R"(`;
        const oaiIs      = `)" + EscapeForJs(oai_is) + R"(`;
        const sessId     = `)" + EscapeForJs(oai_session_id) + R"(`;
        const devId      = `)" + EscapeForJs(oai_device_id) + R"(`;
        const bldNum     = `)" + EscapeForJs(oai_client_build_numer) + R"(`;

        // Fallback sentinel tokens from C++ side
        let proofTok   = `)" + EscapeForJs(open_sentinel_proof_token) + R"(`;
        let reqTok     = `)" + EscapeForJs(open_sentinel_chat_req_token) + R"(`;
        let turnTok    = `)" + EscapeForJs(open_sentinel_turnstile_token) + R"(`;
        const prepTok  = `)" + EscapeForJs(prepare_token) + R"(`;
        const tempChatId = `)" + EscapeForJs(temp_chat_id) + R"(`;

        // --- Step 1: Refresh sentinel tokens before sending ---
        try {
            const finalizeResp = await fetch('https://chatgpt.com/backend-api/sentinel/chat-requirements/finalize', {
                method: 'POST',
                headers: {
                    'Authorization': authToken,
                    'X-OAI-IS': oaiIs,
                    'OAI-Session-Id': sessId,
                    'OAI-Device-Id': devId,
                    'OAI-Client-Build-Number': bldNum,
                    'Content-Type': 'application/json'
                },
                credentials: 'include',
                body: JSON.stringify({
                    prepare_token: prepTok,
                    proofofwork: proofTok,
                    turnstile: turnTok
                })
            });

            if (finalizeResp.ok) {
                const tokenData = await finalizeResp.json();
                if (tokenData.token) {
                    reqTok = tokenData.token;
                }
                console.log('[MessageScript] Sentinel tokens refreshed');
            } else {
                console.warn('[MessageScript] Token refresh failed:', finalizeResp.status);
            }
        } catch (tokenErr) {
            console.warn('[MessageScript] Token refresh error:', tokenErr);
        }

        const payload = JSON.parse(rawPayload);

        let conversationId = payload.conversation_id || "";
        const userParts = payload.messages && payload.messages[0] && payload.messages[0].content && payload.messages[0].content.parts;
        let userMessageId = payload.messages && payload.messages[0] && payload.messages[0].id || "";
        let userMessageContent = userParts ? userParts.filter(p => typeof p === 'string').join('') : "";

        let assistantMessageId = "";
        let assistantMessageContent = "";
        let assistantMessageThinking = "";
        let finalMessageId = "";

        let userMessageSynced = false;
        let lastSyncPayload = "";
        const sendSync = async (convoId, isComplete) => {
            if (!convoId) return;

            const bodyObj = {
                conversation_id: convoId,
                temp_id: tempChatId,
                is_complete: isComplete,
                final_message_id: finalMessageId,
                user_message: (!userMessageSynced && userMessageId) ? { id: userMessageId, content: userMessageContent } : null,
                assistant_message: assistantMessageId ? { id: assistantMessageId, content: assistantMessageContent, thinking: assistantMessageThinking } : null
            };

            if (userMessageId && !userMessageSynced) {
                userMessageSynced = true;
            }

            const bodyStr = JSON.stringify(bodyObj);
            if (bodyStr === lastSyncPayload && !isComplete) return;
            lastSyncPayload = bodyStr;

            try {
                await fetch("https://chatgpt.com/stream-sync", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: bodyStr
                });
            } catch (err) {
                console.error("[SYNC ERROR]", err);
            }
        };

        if (conversationId && userMessageId) {
            await sendSync(conversationId, false);
        }

        const response = await fetch(
            "https://chatgpt.com/backend-api/conversation",
            {
                method: "POST",
                headers: {
                    "accept": "text/event-stream",
                    "authorization": authToken,
                    "content-type": "application/json",
                    "x-oai-is": oaiIs,
                    "oai-session-id": sessId,
                    "oai-device-id": devId,
                    "oai-client-build-number": bldNum,
                    "oai-language": "en-US",
                    "openai-sentinel-proof-token": proofTok,
                    "openai-sentinel-chat-requirements-token": reqTok,
                    "openai-sentinel-turnstile-token": turnTok
                },
                credentials: "include",
                body: JSON.stringify(payload)
            }
        );

        fetch('https://chatgpt.com/status-sync', {
            method: 'POST',
            body: JSON.stringify({ chatId: tempChatId || conversationId, status: 1 })
        }).catch(err => {});
        if (!response.ok) {
            fetch('https://chatgpt.com/status-sync', {
                method: 'POST',
                body: JSON.stringify({ chatId: tempChatId || conversationId, status: 5, error: 'HTTP ' + response.status })
            }).catch(err => {});
            return;
        }

        const reader = response.body.getReader();
        const decoder = new TextDecoder("utf-8");
        let buffer = "";
        let isCompleted = false;

        const finalizeSync = async () => {
            if (isCompleted) return;
            isCompleted = true;

            if (!finalMessageId && assistantMessageId) {
                finalMessageId = assistantMessageId;
            }

            await sendSync(conversationId, true);
        };

        while (true) {
            const { value, done } = await reader.read();
            if (done) {
                await finalizeSync();
                fetch('https://chatgpt.com/status-sync', {
                    method: 'POST',
                    body: JSON.stringify({ chatId: conversationId, status: 4 /* Cached */ })
                }).catch(e => {});
                break;
            }

            buffer += decoder.decode(value, { stream: true });
            const lines = buffer.split("\n");
            buffer = lines.pop(); // keep partial line

            for (const line of lines) {
                const trimmed = line.trim();
                if (!trimmed) continue;

                if (trimmed.startsWith("data: ")) {
                    const dataStr = trimmed.slice(6);
                    if (dataStr === "[DONE]") {
                        await finalizeSync();
                        break;
                    }

                    try {
                        const data = JSON.parse(dataStr);

                        if (data.conversation_id) {
                            conversationId = data.conversation_id;
                            // Sync user message with newly discovered conversation ID
                            if (!userMessageSynced && userMessageId) {
                                await sendSync(conversationId, false);
                            }
                        }

                        if (data.type === "input_message" && data.input_message) {
                            userMessageId = data.input_message.id;
                            if (data.input_message.content && data.input_message.content.parts) {
                                userMessageContent = data.input_message.content.parts.filter(p => typeof p === 'string').join('');
                            }
                            // Sync user message when we get its proper ID
                            if (!userMessageSynced && conversationId) {
                                await sendSync(conversationId, false);
                            } else if (!userMessageSynced && tempChatId) {
                                // Sync user message immediately using temp ID so UI updates
                                await sendSync(tempChatId, false);
                            }
                        }

                        if (
                            data.type === "message_marker" &&
                            data.marker === "last_token"
                        ) {
                            finalMessageId = data.message_id;
                            await sendSync(conversationId, true);
                            continue;
                        }

                        if (data.message) {
                            const msg = data.message;
                            if (msg.author && msg.author.role === "user") {
                                userMessageId = msg.id;
                                if (msg.content && msg.content.parts) {
                                    userMessageContent = msg.content.parts.filter(p => typeof p === 'string').join('');
                                }
                            } else if (msg.author && msg.author.role === "assistant") {
                                assistantMessageId = msg.id;
                                if (msg.content && msg.content.parts) {
                                    assistantMessageContent = msg.content.parts.filter(p => typeof p === 'string').join('');
                                }
                            }
                        }

                        if (data.p === "" && data.o === "add" && data.v && data.v.message) {
                            const msg = data.v.message;
                            if (msg.author && msg.author.role === "user") {
                                userMessageId = msg.id;
                                if (msg.content && msg.content.parts) {
                                    userMessageContent = msg.content.parts.filter(p => typeof p === 'string').join('');
                                }
                            } else if (msg.author && msg.author.role === "assistant") {
                                assistantMessageId = msg.id;
                                if (msg.content && msg.content.parts) {
                                    assistantMessageContent = msg.content.parts.filter(p => typeof p === 'string').join('');
                                }
                            }
                        }

                        if (data.v && data.v.message) {
                            const msg = data.v.message;
                            if (msg.author && msg.author.role === "user") {
                                userMessageId = msg.id;
                                if (msg.content && msg.content.parts) {
                                    userMessageContent = msg.content.parts.filter(p => typeof p === 'string').join('');
                                }
                            } else if (msg.author && msg.author.role === "assistant") {
                                assistantMessageId = msg.id;
                                if (msg.content && msg.content.parts) {
                                    assistantMessageContent = msg.content.parts.filter(p => typeof p === 'string').join('');
                                }
                            }
                        }

                        if (data.o === "patch" && Array.isArray(data.v)) {
                            let contentChanged = false;

                            for (const op of data.v) {
                                if (op.p && op.p.startsWith("/message/content/parts/")) {
                                    if (op.o === "append") {
                                        if (typeof op.v === "string") {
                                            assistantMessageContent += op.v;
                                            contentChanged = true;
                                        } else if (Array.isArray(op.v)) {
                                            assistantMessageContent += op.v.filter(p => typeof p === 'string').join('');
                                            contentChanged = true;
                                        }
                                    } else if (op.o === "replace") {
                                        if (typeof op.v === "string") {
                                            assistantMessageContent = op.v;
                                            contentChanged = true;
                                        } else if (Array.isArray(op.v)) {
                                            assistantMessageContent = op.v.filter(p => typeof p === 'string').join('');
                                            contentChanged = true;
                                        }
                                    }
                                } else if (op.p === "/message/content/thoughts") {
                                    if (op.o === "append") {
                                        if (typeof op.v === "string") {
                                            assistantMessageThinking += op.v;
                                            contentChanged = true;
                                        } else if (Array.isArray(op.v)) {
                                            for (const item of op.v) {
                                                if (typeof item === "string") {
                                                    assistantMessageThinking += item;
                                                    contentChanged = true;
                                                } else if (item && typeof item === "object") {
                                                    if (Array.isArray(item.chunks)) {
                                                        assistantMessageThinking += item.chunks.join("");
                                                        contentChanged = true;
                                                    } else if (typeof item.content === "string") {
                                                        if (!item.chunks) {
                                                            assistantMessageThinking += item.content;
                                                            contentChanged = true;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else if (op.o === "replace") {
                                        if (typeof op.v === "string") {
                                            assistantMessageThinking = op.v;
                                            contentChanged = true;
                                        } else if (Array.isArray(op.v)) {
                                            let tempThinking = "";
                                            for (const item of op.v) {
                                                if (typeof item === "string") {
                                                    tempThinking += item;
                                                } else if (item && typeof item === "object") {
                                                    if (Array.isArray(item.chunks)) {
                                                        tempThinking += item.chunks.join("");
                                                    } else if (typeof item.content === "string") {
                                                        tempThinking += item.content;
                                                    }
                                                }
                                            }
                                            assistantMessageThinking = tempThinking;
                                            contentChanged = true;
                                        }
                                    }
                                }
                            }

                            if (contentChanged) {
                                await sendSync(conversationId, false);
                            }
                        }

                        if (data.o === "append" && typeof data.v === "string") {
                            assistantMessageContent += data.v;
                            await sendSync(conversationId, false);
                        }

                        if (data.v && typeof data.v === "string" && !data.o) {
                            assistantMessageContent += data.v;
                            await sendSync(conversationId, false);
                        }
                    } catch (e) {
                        // Ignore partial or invalid JSON frames
                    }
                }
            }
        }
    }
    catch (err) {
        fetch('https://chatgpt.com/status-sync', {
            method: 'POST',
            body: JSON.stringify({ chatId: conversationId, status: 5, error: err.toString() })
        }).catch(e => {});
    }
})();)";


    auto browser = AppState::GetHiddenBrowser();
    if (browser && browser->GetMainFrame()) {
        browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
    }
}
