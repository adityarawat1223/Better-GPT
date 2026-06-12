#include "FileScripts.h"
#include "Helpers/JsEscaper.h"



void FileScripts::FileUploadFin(
    const std::string& file_id,
    const std::string& filename,
    const std::string& chat_id,
    const std::string& origination_message_id,
    const std::string& local_id) {

    std::string token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string build_num = AppState::GetHeaders("OAI-Client-Build-Number");
    
    json payload = {
        { "file_id", file_id },
        { "use_case", "multimodal" },
        { "index_for_retrieval", false },
        { "file_name", filename },
        { "library_persistence_mode", "opportunistic" },
        { "entry_surface", "chat_composer" },
        {
            "metadata",
            {
                { "store_in_library", true },
                { "is_temporary_chat", false },
                { "library_eligibility_reason", "eligible" },
                { "is_project_thread", false },
                {
                    "library_file_info",
                    {
                        {
                            "origination_message_id",
                            origination_message_id
                        },
                        {
                            "origination_thread_id",
                            chat_id
                        }
                    }
                }
            }
        }
    };

    std::string payload_str =
        payload.dump();

    std::string js = R"((async () => {
        try {
            const rawPayload = `)" + EscapeForJs(payload_str) + R"(`;
            const c_chat_id  = `)" + EscapeForJs(chat_id) + R"(`;
            const c_file_id  = `)" + EscapeForJs(file_id) + R"(`;
            const c_local_id = `)" + EscapeForJs(local_id) + R"(`;

            const authToken  = `)" + EscapeForJs(token) + R"(`;
            const oaiIs      = `)" + EscapeForJs(oai_is) + R"(`;
            const sessId     = `)" + EscapeForJs(session_id) + R"(`;
            const devId      = `)" + EscapeForJs(device_id) + R"(`;
            const bldNum     = `)" + EscapeForJs(build_num) + R"(`;

            const parsedPayload = JSON.parse(rawPayload);
            
            const res = await fetch(
                "https://chatgpt.com/backend-api/files/process_upload_stream",
                {
                    method: "POST",
                    headers: {
                        "Authorization": authToken,
                        "x-oai-is": oaiIs,
                        "oai-session-id": sessId,
                        "oai-device-id": devId,
                        "oai-client-build-number": bldNum,
                        "oai-language": "en-US",
                        "Content-Type": "application/json"
                    },
                    credentials: "include",
                    body: JSON.stringify(parsedPayload)
                }
            );

            const reader = res.body.getReader();
            const decoder = new TextDecoder();
            let pending = "";
            let lib_file_id = "";
            let lib_file_name = "";
            let mime_type = "";

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                
                pending += decoder.decode(value, { stream: true });
                let pos;
                while ((pos = pending.indexOf("\n")) !== -1) {
                    const line = pending.slice(0, pos).trim();
                    pending = pending.slice(pos + 1);
                    if (!line) continue;
                    
                    let obj;
                    try { obj = JSON.parse(line); } catch { continue; }
                    
                    if (obj.event === "file.processing.completed" &&
                        obj.extra &&
                        obj.extra.metadata_object_id)
                    {
                        lib_file_id  = obj.extra.metadata_object_id;
                        lib_file_name = obj.extra.library_file_name || "";
                        mime_type    = obj.extra.mime_type || "";
                    }
                }
            }

            await fetch("https://chatgpt.com/file-processed", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    chat_id:     c_chat_id,
                    lib_file_id: lib_file_id,
                    local_id:    c_local_id,
                    file_id:     c_file_id
                })
            });
            
        } catch(err) {
            console.error("PROCESS_UPLOAD_STREAM EXECUTION ERROR:", err);
        }
    })();)";

 
    auto browser =
        AppState::GetHiddenBrowser();

    browser
        ->GetMainFrame()
        ->ExecuteJavaScript(
            js,
            "https://chatgpt.com",
            0);
};


void FileScripts::FileUploaderInit(const  FileRef& fileref) {

    json j = {
        { "file_name", fileref.filename },
        { "mime_type", fileref.mime_type },
        { "file_size", fileref.size_bytes},
        { "store_in_library" , true},
        { "timezone_offset_min", -330},
        { "use_case" ,"multimodal" },
        { "library_persistence_mode", "opportunistic"}
    };

    json fulldto = fileref;
    std::string payload_str = j.dump();
    std::string dto_str = fulldto.dump();

    std::string token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string build_num = AppState::GetHeaders("OAI-Client-Build-Number");

    std::string js = R"((async () => {
        try {
            console.log("START");
            
            // Pass dump strings into template literals and parse them natively inside V8
            const payload = JSON.parse(`)" + EscapeForJs(payload_str) + R"(`);
            const dto     = JSON.parse(`)" + EscapeForJs(dto_str) + R"(`);
            
            const authToken = `)" + EscapeForJs(token) + R"(`;
            const oaiIs     = `)" + EscapeForJs(oai_is) + R"(`;
            const sessId    = `)" + EscapeForJs(session_id) + R"(`;
            const devId     = `)" + EscapeForJs(device_id) + R"(`;
            const bldNum    = `)" + EscapeForJs(build_num) + R"(`;

            console.log("BEFORE FIRST FETCH");
            const res = await fetch("https://chatgpt.com/backend-api/files", {
                method: "POST",
                headers: {
                    "Authorization": authToken,
                    "x-oai-is": oaiIs,
                    "oai-session-id": sessId,
                    "oai-device-id": devId,
                    "oai-client-build-number": bldNum,
                    "oai-language": "en-US",
                    "Content-Type": "application/json"
                },
                credentials: "include",
                body: JSON.stringify(payload)
            });
            
            const txt = await res.text();
            
            console.log("BEFORE SECOND FETCH");
            const res2 = await fetch("https://chatgpt.com/upload-sync", {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify({
                    request: dto,
                    response: JSON.parse(txt) 
                })
            });
            
            console.log("SECOND FETCH DONE, STATUS:", res2.status);
        } catch(err) {
            console.error("ERROR:", err);
        }
    })();)";

    std::cout << "[FileUploaderInit] Dispatching structural file staging payload..." << std::endl;

    auto browser = AppState::GetHiddenBrowser();
     browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
    
}


void FileScripts::FileExec(const std::string& FileId) {

    auto browser = AppState::GetHiddenBrowser();
    if (!browser || !browser->GetMainFrame()) {
        std::cout << "[FileExecution Error] Hidden browser frame reference is invalid!" << std::endl;
        return;
    }

    std::string token = AppState::GetTokenInfo();
    std::string oai_is = AppState::GetHeaders("X-OAI-IS");
    std::string session_id = AppState::GetHeaders("OAI-Session-Id");
    std::string device_id = AppState::GetHeaders("OAI-Device-Id");
    std::string build_num = AppState::GetHeaders("OAI-Client-Build-Number");

    std::string js = R"((async () => {
        try {
            const fileId   = `)" + EscapeForJs(FileId) + R"(`;
            const authToken = `)" + EscapeForJs(token) + R"(`;
            const oaiIs    = `)" + EscapeForJs(oai_is) + R"(`;
            const sessId   = `)" + EscapeForJs(session_id) + R"(`;
            const devId    = `)" + EscapeForJs(device_id) + R"(`;
            const bldNum   = `)" + EscapeForJs(build_num) + R"(`;

            const url = `https://chatgpt.com/backend-api/files/download/${fileId}?post_id=&inline=false`;

            const response = await fetch(url, {
                method: 'GET',
                headers: {
                    'Authorization': authToken,
                    'x-oai-is': oaiIs,
                    'oai-session-id': sessId,
                    'oai-device-id': devId,
                    'oai-client-build-number': bldNum,
                    'oai-language': 'en-US',
                    'Accept': '*/*'
                },
                credentials: 'include'
            });

            console.log("[FILE DOWNLOAD INIT] Status:", response.status);
        } catch (err) {
            console.error("[FILE DOWNLOAD ERROR]", err);
        }
    })();)";

    browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
}


void FileScripts::FileDownload(const std::string& url) {

    auto browser = AppState::GetHiddenBrowser();

    std::string js =
        "fetch('" + url + "',{"
        "method:'GET',"
        "credentials:'include'"
        "});";

    browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
}


void FileScripts::NoCredFileDownload(const std::string& url) {

    auto browser = AppState::GetHiddenBrowser();

    std::string js =
        "fetch('" + url + "',{"
        "method:'GET'"
        "});";
    browser->GetMainFrame()->ExecuteJavaScript(js, "https://chatgpt.com", 0);
}