#include "FileFin.h"
#include "Helpers/json.hpp"
#include "memdb/AppState.h"
#include <iostream>
#include <string>


using json = nlohmann::json;


 void Filefin(const std::string& raw_data) {

    std::string Chatid ,local_id ,file_id;
	try {
		json init = json::parse(raw_data);

        if (init.contains("lib_file_id") && init["lib_file_id"].is_string()) {

            std::string lib_id = init["lib_file_id"].get<std::string>();
            if (!init.contains("chat_id") || !init["chat_id"].is_string()) {
                return;
            }


            if (!init.contains("local_id") || !init["local_id"].is_string()) {
                return;
            }
            
            if (!init.contains("file_id") || !init["file_id"].is_string()) {
                return;
            }


            Chatid = init["chat_id"].get<std::string>();
            local_id = init["local_id"].get<std::string>();
            file_id = init["file_id"].get<std::string>();
            AppState::Update_Asset_Status(UploadStatus::Ready, Chatid, local_id);
            AppState::Update_Upload_Url(Chatid, local_id, lib_id, file_id);
        }

        else {

            if (Chatid.empty() || local_id.empty())
                return;
            else
                AppState::Update_Asset_Status(UploadStatus::Failed, Chatid, local_id);
        }
	}
    catch (const std::exception& e)
    {
        std::cerr
            << "FileFin exception: "
            << e.what()
            << std::endl;
    }
}