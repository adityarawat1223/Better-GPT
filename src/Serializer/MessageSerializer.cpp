#include "MessageSerializer.h"
#include "Helpers/json.hpp"
using json = nlohmann::json;
#include <iostream>
#include <random>
#include <string>
#include "uuid.h"
#include "memdb/AppState.h"
static std::string generate_secure_chat_id() {
	thread_local std::mt19937 engine([]() {
		std::random_device rd;
		return rd();
		}());

	thread_local uuids::uuid_random_generator gen{ engine };

	return uuids::to_string(gen());
}
 void MessageSerializer(
    const InputBox& input,
    std::string& out,
    const std::string chat_id)
{
    json attachments = json::array();
    json parts = json::array();

    std::string content_type = "text";

    std::string message_id =
        generate_secure_chat_id();

    std::string parent_id =
        AppState::Get_Parent(chat_id);

    if (parent_id.empty()) {
        parent_id = "client_created_root";
    }

    std::string model =
        AppState::Get_Default_Model();

    if (model != "auto" &&
        !input.model.empty())
    {
        model = input.model;
    }

    for (auto& ele : input.assets)
    {
        auto& asset = ele.second;

        if (asset.lib_file_id.empty())
        {
            return;
        }

        if (asset.height && asset.width)
        {
            if (content_type == "text")
            {
                content_type =
                    "multimodal_text";
            }

            json obj;

            obj["id"] =
                asset.id;

            obj["library_file_id"] =
                asset.lib_file_id;

            obj["is_big_paste"] =
                false;

            obj["mime_type"] =
                asset.mime_type;

            obj["height"] =
                asset.height;

            obj["width"] =
                asset.width;

            obj["name"] =
                asset.filename;

            obj["size"] =
                asset.size_bytes;

            obj["source"] =
                "library";

            attachments.push_back(obj);

            json part_obj;

            part_obj["asset_pointer"] =
                "sediment://" + asset.id;

            part_obj["size_bytes"] =
                asset.size_bytes;

            part_obj["content_type"] =
                "image_asset_pointer";

            part_obj["height"] =
                asset.height;

            part_obj["width"] =
                asset.width;

            parts.push_back(part_obj);
        }
        else
        {
            json obj;

            obj["id"] =
                asset.id;

            obj["library_file_id"] =
                asset.lib_file_id;

            obj["is_big_paste"] =
                false;

            obj["mime_type"] =
                asset.mime_type;

            obj["name"] =
                asset.filename;

            obj["size"] =
                asset.size_bytes;

            obj["source"] =
                "library";

            attachments.push_back(obj);
        }
    }

    parts.push_back(input.content);

    json message;

    message["id"] =
        message_id;

    message["author"] =
    {
        {"role", "user"}
    };

    message["create_time"] =
        std::time(nullptr);

    message["content"] =
    {
        {"content_type", content_type},
        {"parts", parts}
    };

    message["metadata"] =
    {
        {"attachments", attachments},
        {"selected_github_repos", json::array()},
        {"selected_all_github_repos", false},
        {
            "serialization_metadata",
            {
                {"custom_symbol_offsets", json::array()}
            }
        }
    };

    if (input.mode == Modes::reason)
    {
        message["metadata"]["system_hints"] =
            json::array({ "reason" });
    }

     if (input.mode == Modes::create_image)
    {
        message["metadata"]["system_hints"] =
            json::array({ "picture_v2" });
    }


    json root;

    root["action"] =
        "next";

    root["messages"] =
        json::array({ message });

    if(parent_id != "client_created_root")
       root["conversation_id"] =
        chat_id;

    root["parent_message_id"] =
        parent_id;

    root["model"] =
        model;

    root["client_prepare_state"] =
        "success";

    root["timezone_offset_min"] =
        -330;

    root["timezone"] =
        "Asia/Calcutta";

    root["conversation_mode"] =
    {
        {"kind", "primary_assistant"}
    };

    root["enable_message_followups"] =
        true;

    if(input.mode == Modes::reason || input.mode == Modes::create_image){
        root["system_hints"] =
        input.mode == Modes::reason
        ? json::array({ "reason" })
        : json::array({"picture_v2"});
    }

    else{
    root["system_hints"] = json::array();
    }

    root["supports_buffering"] =
        true;

    root["supported_encodings"] =
        json::array({ "v1" });

    if (input.mode == Modes::search)
    {
        root["force_use_search"] =
            true;

        root["client_reported_search_source"] =
            "conversation_composer_web_icon";
    }

    root["client_contextual_info"] =
    {
        {"is_dark_mode", true},
        {"time_since_loaded", 120},
        {"page_height", 746},
        {"page_width", 1528},
        {"pixel_ratio", 1.25},
        {"screen_height", 864},
        {"screen_width", 1536},
        {"app_name", "chatgpt.com"}
    };

    root["paragen_cot_summary_display_override"] =
        "allow";

    root["force_parallel_switch"] =
        "auto";

    out =
        root.dump();
}