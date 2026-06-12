#include "Fileinit.h"
#include <iostream>
#include "Helpers/json.hpp"
#include "memdb/AppState.h"

using json = nlohmann::json;

void FileInit(const std::string& raw_data) {
    json payload = json::parse(raw_data);

    if (!payload.contains("request") || !payload.contains("response"))
    {
        std::cerr << "[FileInit] Error: Invalid payload structure" << std::endl;
        return;
    }

    json req = payload["request"];
    json resp = payload["response"];

    if (!resp.contains("status") || resp["status"] != "success")
    {
        std::cerr << "[FileInit] Error: Upload initialization flag is not success" << std::endl;
        return;
    }

    std::string upload_url = resp.value("upload_url", "");
    std::string file_id = resp.value("file_id", "");

    std::string chat_id = req.value("chat_id", ""); 
    std::string local_id = req.value("local_id", "");
    std::string filename = req.value("filename", "");
    std::string mime_type = req.value("mime_type", "application/octet-stream");

    if (upload_url.empty() || file_id.empty() || local_id.empty() || chat_id.empty())
    {
        std::cerr << "[FileInit] Error: Critical field extraction failed (empty string detected)" << std::endl;
        return;
    }

    AppState::Update_Upload_Url(chat_id, local_id, upload_url, file_id);

    AppState::Update_Asset_Status(UploadStatus::Processing, chat_id, local_id);
}