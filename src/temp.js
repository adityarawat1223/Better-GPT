(async () => {

    //---------------------------------------------------
    // VALUES FROM C++
    //---------------------------------------------------

    const CHAT_ID = "__CHAT_ID__";

    const PAYLOAD = __PAYLOAD__;

    const TOKEN = "__TOKEN__";

    const OAI_DEVICE_ID = "__DEVICE_ID__";

    const OAI_SESSION_ID = "__SESSION_ID__";

    const BUILD = "__BUILD__";


    //---------------------------------------------------
    // FINAL RESULT
    //---------------------------------------------------

    let conversationId = "";

    let userMessageId = "";

    let assistantMessageId = "";

    let assistantText = "";


    //---------------------------------------------------
    // STREAM STATE
    //---------------------------------------------------

    const response = await fetch(
        "https://chatgpt.com/backend-api/conversation",
        {
            method: "POST",

            credentials: "include",

            headers:
            {
                "Authorization": TOKEN,

                "Content-Type":
                    "application/json",

                "oai-device-id":
                    OAI_DEVICE_ID,

                "oai-session-id":
                    OAI_SESSION_ID,

                "oai-client-build-number":
                    BUILD
            },

            body:
                JSON.stringify(PAYLOAD)
        }
    );


    //---------------------------------------------------
    // STREAM READER
    //---------------------------------------------------

    const reader =
        response.body.getReader();

    const decoder =
        new TextDecoder();

    let pending = "";


    //---------------------------------------------------
    // MAIN READ LOOP
    //---------------------------------------------------

    while (true)
    {
        const { done, value } =
            await reader.read();

        if (done)
            break;

        pending += decoder.decode(
            value,
            { stream: true }
        );

        //------------------------------------------------
        // PARSE SSE FRAMES
        //------------------------------------------------

        let pos;

        while (
            (pos = pending.indexOf("\n\n")) !== -1
        )
        {
            const frame =
                pending.slice(0, pos);

            pending =
                pending.slice(pos + 2);

            processFrame(frame);
        }
    }


    //---------------------------------------------------
    // PROCESS FRAME
    //---------------------------------------------------

    function processFrame(frame)
    {
        const lines =
            frame.split("\n");

        for (const line of lines)
        {
            //------------------------------------------------
            // ONLY DATA LINES
            //------------------------------------------------

            if (!line.startsWith("data:"))
                continue;

            const data =
                line.substring(5).trim();

            if (!data)
                continue;

            if (data === "[DONE]")
                continue;

            //------------------------------------------------
            // PARSE JSON
            //------------------------------------------------

            let obj;

            try
            {
                obj = JSON.parse(data);
            }
            catch(err)
            {
                continue;
            }

            //------------------------------------------------
            // INPUT MESSAGE
            //------------------------------------------------

            if (
                obj.type === "input_message" &&
                obj.input_message
            )
            {
                conversationId =
                    obj.conversation_id || "";

                userMessageId =
                    obj.input_message.id || "";
            }

            //------------------------------------------------
            // ASSISTANT MESSAGE NODE
            //------------------------------------------------

            if (
                obj.message &&
                obj.message.author &&
                obj.message.author.role === "assistant"
            )
            {
                assistantMessageId =
                    obj.message.id || "";

                //------------------------------------------------
                // FULL PARTS ARRAY
                //------------------------------------------------

                const parts =
                    obj.message
                       ?.content
                       ?.parts;

                if (
                    Array.isArray(parts) &&
                    parts.length > 0
                )
                {
                    assistantText =
                        parts.join("");
                }
            }

            //------------------------------------------------
            // APPEND DELTAS
            //------------------------------------------------

            if (
                obj.o === "append" &&
                typeof obj.v === "string"
            )
            {
                assistantText += obj.v;
            }

            //------------------------------------------------
            // CONTINUATION DELTAS
            //------------------------------------------------

            else if (
                typeof obj.v === "string"
            )
            {
                assistantText += obj.v;
            }
        }
    }


    //---------------------------------------------------
    // NORMALIZED RESULT
    //---------------------------------------------------

    const result =
    {
        chat_id:
            CHAT_ID,

        conversation_id:
            conversationId,

        user_message_id:
            userMessageId,

        assistant_message_id:
            assistantMessageId,

        assistant_text:
            assistantText
    };


    //---------------------------------------------------
    // SEND TO NATIVE
    //---------------------------------------------------

    fetch(
        "app://conversation_done",
        {
            method: "POST",

            body:
                JSON.stringify(result)
        }
    );

})();

normal :-
{"action":"next","messages":
[{"id":"38191ef6-d8cb-4731-967c-60b45005e7ba",
"author":{"role":"user"},"create_time":1779710564.644,
"content":{"content_type":"text","parts":["How do you even efficiently parse sse events"]},
"metadata":{"selected_github_repos":[],"selected_all_github_repos":false,"serialization_metadata":{"custom_symbol_offsets":[]}}}],
"conversation_id":"68f879fa-d08c-8322-b39c-11dae109be18","parent_message_id":"2cfbf519-f95d-4af5-800c-52784dbdadc8","model":"auto",
"client_prepare_state":"success","timezone_offset_min":-330,"timezone":"Asia/Calcutta",
"conversation_mode":{"kind":"primary_assistant"},"enable_message_followups":true,
"system_hints":[],"supports_buffering":true,"supported_encodings":["v1"],"client_contextual_info":{"is_dark_mode":true,"time_since_loaded":120,"page_height":746,"page_width":1528,"pixel_ratio":1.25,"screen_height":864,"screen_width":1536,"app_name":"chatgpt.com"},
"paragen_cot_summary_display_override":"allow","force_parallel_switch":"auto"}


thinking:-

{"action":"next","messages":[{"id":"3680bede-7586-42d8-8832-b1600fc82cb1",
"author":{"role":"user"},"create_time":1779718908.02,
"content":{"content_type":"text","parts":["how to generead uuid v4 in cpp "]},
"metadata":{"selected_github_repos":[],"selected_all_github_repos":false,"system_hints":["reason"],
"serialization_metadata":{"custom_symbol_offsets":[]}}}],
"conversation_id":"68f879fa-d08c-8322-b39c-11dae109be18","parent_message_id":"c99d4d69-0a8b-4c08-b962-e3e2adc08a2c",
"model":"auto","client_prepare_state":"success","timezone_offset_min":-330,
"timezone":"Asia/Calcutta","conversation_mode":{"kind":"primary_assistant"},
"enable_message_followups":true,"system_hints":["reason"],
"supports_buffering":true,"supported_encodings":["v1"],
"client_contextual_info":{"is_dark_mode":true,"time_since_loaded":8463,"page_height":746,"page_width":1528,"pixel_ratio":1.25,"screen_height":864,"screen_width":1536,"app_name":"chatgpt.com"},
"paragen_cot_summary_display_override":"allow","force_parallel_switch":"auto"}

web search:-

{"action":"next","messages":[{"id":"f1459b7c-395a-42a6-8e84-5fbd1d0b7e17",
"author":{"role":"user"},"create_time":1779719102.777,"content":{"content_type":"text",
"parts":["any good library for this whihc can be crossplatform ? "]},
"metadata":{"selected_github_repos":[],"selected_all_github_repos":false,"system_hints":["search"],
"serialization_metadata":{"custom_symbol_offsets":[]}}}],"conversation_id":"68f879fa-d08c-8322-b39c-11dae109be18",
"parent_message_id":"57e5037c-a9e1-4674-b21b-28c1ddde0f38","model":"auto","client_prepare_state":"success",
"timezone_offset_min":-330,"timezone":"Asia/Calcutta","conversation_mode":{"kind":"primary_assistant"},
"enable_message_followups":true,"system_hints":[],
"supports_buffering":true,"supported_encodings":["v1"],"force_use_search":true,
"client_reported_search_source":"conversation_composer_web_icon","client_contextual_info":{"is_dark_mode":true,"time_since_loaded":8658,"page_height":746,"page_width":1528,"pixel_ratio":1.25,"screen_height":864,"screen_width":1536,"app_name":"chatgpt.com"},
"paragen_cot_summary_display_override":"allow","force_parallel_switch":"auto"}

images + file  :-

{"action":"next","messages":[{"id":"21caf080-65ac-42f5-b315-ebdab61dc265","author":{"role":"user"},
"create_time":1779931295.727,"content":{"content_type":"multimodal_text","parts":
[{"content_type":"image_asset_pointer","asset_pointer":"sediment://file_00000000240471f5babe999e61475042","size_bytes":0,
"width":512,"height":512},"resume review i need  whatever format you can read "]},
"metadata":{"attachments":[{"id":"file_00000000240471f5babe999e61475042","size":0,"name":"1600(2).png","mime_type":"image/png",
"width":512,"height":512,"source":"library","library_file_id":"libfile_35aac4537a98819187c87290058757c5","is_big_paste":false},
{"id":"file_00000000d7fc720babffb9da2be93a47","size":83641,"name":"Aditya Rawat.pdf","mime_type":"application/pdf",
"source":"library","library_file_id":"libfile_99b30629f6a48191b322463521d2d13b","is_big_paste":false}],
"selected_github_repos":[],"selected_all_github_repos":false,"serialization_metadata":
{"custom_symbol_offsets":[]}}}],"conversation_id":"68f879fa-d08c-8322-b39c-11dae109be18",
"parent_message_id":"46c72c74-8faa-454b-839b-2ceaf17f4959","model":"auto","client_prepare_state":"success",
"timezone_offset_min":-330,"timezone":"Asia/Calcutta",
"conversation_mode":{"kind":"primary_assistant"},
"enable_message_followups":true,"system_hints":[],
"supports_buffering":true,"supported_encodings":["v1"],
"client_contextual_info":{"is_dark_mode":true,
"time_since_loaded":82,"page_height":746,"page_width":1528,"pixel_ratio":1.25,"screen_height":864,"screen_width":1536,
"app_name":"chatgpt.com"},"paragen_cot_summary_display_override":"allow","force_parallel_switch":"auto"}

#include <iostream>
#include <random>
#include <string>
#include "uuid.h"

std::string generate_secure_chat_id() {
    // Corrected: Initialize rd and pass its result directly to the engine
    thread_local std::mt19937 engine([]() {
        std::random_device rd;
        return rd();
    }());
    
    thread_local uuids::uuid_random_generator gen{engine};
    
    return uuids::to_string(gen());
}

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

 //if (url.find("/token-sync") != std::string::npos) {
    //    CefRefPtr<CefPostData> post = request->GetPostData();
    //    if (post) {
    //        std::string body;
    //        CefPostData::ElementVector elements;
    //        post->GetElements(elements);
    //        for (auto& el : elements) {
    //            if (el->GetType() != PDE_TYPE_BYTES) continue;
    //            size_t sz = el->GetBytesCount();
    //            if (sz == 0) continue;
    //            std::string buf(sz, '\0');
    //            el->GetBytes(sz, buf.data());
    //            body += buf;
    //        }

    //        std::cout << "[C++ Sync] Received all 3 raw tokens: " << body << std::endl;
    //        if (!body.empty()) {
    //            std::cout << body << std::endl;
    //            ChatTokenGrabber(body);
    //        }

    //    }
    //    return RV_CANCEL;
    //}


    //    std::string js = R"(
//
//(() => {
//    if (window.__sentinel_hooked__)
//        return;
//
//    window.__sentinel_hooked__ = true;
//    console.log("[HOOK] Installing sentinel hook");
//    const originalFetch =
//        window.fetch;
//    window.fetch =
//        async (...args) => {
//        try {
//
//           const [resource, options] =
//                args;
//
//            const url =
//                typeof resource === "string"
//                    ? resource
//                    : resource?.url;
//
//            if (
//                url &&
//                url.includes(
//                    "/backend-api/sentinel/chat-requirements/finalize"))
//            {
//                console.log(
//                    "[HOOK] Sentinel finalize intercepted");
//
//                let body = null;
//
//                // normal fetch(url, { body })
//                if (options?.body)
//                {
//                    body = options.body;
//                }
//
//                // fetch(Request)
//                else if (resource instanceof Request)
//                {
//                    try {
//
//                        const cloned =
//                            resource.clone();
//
//                        body =
//                            await cloned.text();
//
//                    } catch(err) {
//
//                        console.error(
//                            "[HOOK] Failed reading Request body",
//                            err);
//                    }
//                }
//
//                if (body)
//                {
//                    console.log(
//                        "[HOOK] Sending token-sync");
//
//                    fetch(
//                        "https://chatgpt.com/token-sync",
//                        {
//                            method: "POST",
//
//                            headers:
//                            {
//                                "Content-Type":
//                                    "application/json"
//                            },
//
//                            body
//                        });
//                }
//                else
//                {
//                    console.log(
//                        "[HOOK] No body found");
//                }
//            }
//
//        }
//        catch(err)
//        {
//            console.error(
//                "[HOOK ERROR]",
//                err);
//        }
//
//        return originalFetch(...args);
//    };
//
//    console.log(
//        "[HOOK] Installed successfully");
//
//})();
//
//)";
//
//    frame->ExecuteJavaScript(
//        js,
//        frame->GetURL(),
//        0);