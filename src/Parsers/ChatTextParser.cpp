#include <string>
#include "Helpers/json.hpp"
#include "memdb/AppState.h"
#include <vector>
#include "Dto/ChatText.h"
#include  <iostream>
#include "ChatTextParser.h"
#include <fstream>
#include "Helpers/Sanitizer.h"

using json = nlohmann::json;

bool ChatTextParser::InitFilter() {
	return true;
}

CefResponseFilter::FilterStatus  ChatTextParser::Filter(
	void* data_in,
	size_t data_in_size,
	size_t& data_in_read,
	void* data_out,
	size_t data_out_size,
	size_t& data_out_written) {

	if (!data_in || data_in_size == 0) {
		data_in_read = 0;
		data_out_written = 0;
		return RESPONSE_FILTER_DONE;
	}

	size_t tocopy = std::min(data_in_size, data_out_size);
	const char* raw_bytes = static_cast<const char*>(data_in);
	raw_data.append(raw_bytes, tocopy);
	memcpy(data_out, data_in, tocopy);
	data_in_read = tocopy;
	data_out_written = tocopy;

	return RESPONSE_FILTER_NEED_MORE_DATA;
}

size_t CalculateTokens(const std::string& text) {
    return text.length() / 4;
}

void ChatTextParser::Chat_Text_Parser()
{
    json initjson = json::parse(raw_data);
    std::vector<ChatMessage> temp;
    std::string chat_id;

    if (initjson.contains("conversation_id") &&
        initjson["conversation_id"].is_string())
    {
        chat_id = initjson["conversation_id"].get<std::string>();
    }
    else { return; }

    if (!initjson.contains("mapping")) return;
    
    // Extract server-assigned title and update AppState directly
    if (initjson.contains("title") && initjson["title"].is_string()) {
        std::string title = initjson["title"].get<std::string>();
        if (!title.empty()) {
            AppState::RenameChatItem(chat_id, title);
        }
    }

    if (!initjson.contains("current_node") ||
        !initjson["current_node"].is_string()) return;

    std::vector<std::string> ordered_nodes;
    std::string current_node =
        initjson["current_node"].get<std::string>();

    while (!current_node.empty()) {
        ordered_nodes.push_back(current_node);
        if (!initjson["mapping"].contains(current_node)) break;
        const auto& node = initjson["mapping"][current_node];
        if (!node.contains("parent") || node["parent"].is_null()) break;
        if (!node["parent"].is_string()) break;
        current_node = node["parent"].get<std::string>();
    }

    AppState::Set_Parent(chat_id, ordered_nodes.front());
    std::reverse(ordered_nodes.begin(), ordered_nodes.end());

    long long total_tokens = 0;

    for (const auto& node_id : ordered_nodes)
    {
        const auto& node = initjson["mapping"][node_id];
        if (!node.contains("message")) continue;

        const auto& msg = node["message"];
        if (msg.is_null()) continue;


        if (msg.contains("metadata") &&
            msg["metadata"].contains("is_visually_hidden_from_conversation") &&
            msg["metadata"]["is_visually_hidden_from_conversation"].is_boolean() &&
            msg["metadata"]["is_visually_hidden_from_conversation"].get<bool>())
        {
            continue;
        }

        if (!msg.contains("author") ||
            !msg["author"].contains("role") ||
            !msg["author"]["role"].is_string()) continue;

        std::string role = msg["author"]["role"].get<std::string>();

        if (!msg.contains("content")) continue;
        const auto& content = msg["content"];

        std::string content_type = "";
        if (content.contains("content_type") &&
            content["content_type"].is_string())
            content_type = content["content_type"].get<std::string>();

        if (content_type == "model_editable_context") continue;

        // Extract text early to calculate total tokens (including system, tool, and thoughts)
        std::string extracted_text;

        if (content.contains("parts") && content["parts"].is_array()) {
            for (const auto& part : content["parts"]) {
                if (part.is_string())
                    extracted_text += part.get<std::string>();
                else if (part.is_object() && part.contains("text") && part["text"].is_string())
                    extracted_text += part["text"].get<std::string>();
            }
        }
        else if (content.contains("text") && content["text"].is_string()) {
            extracted_text = content["text"].get<std::string>();
        }
        else if (content.contains("result") && content["result"].is_string()) {
            extracted_text = content["result"].get<std::string>();
        }
        else if (content_type == "reasoning_recap" && content.contains("content") && content["content"].is_string()) {
            extracted_text = content["content"].get<std::string>();
        }

        total_tokens += CalculateTokens(extracted_text); // Simple token estimation

        // Hide system and tool messages from UI but keep their token weight
        bool is_sys_tool = (role == "system" || role == "tool");

        ChatMessage chattext;
        chattext.is_system_or_tool = is_sys_tool;
        chattext.raw_content = extracted_text;
        chattext.user = (role == "user");
        chattext.message_id = node_id;

        if (msg.contains("create_time") && msg["create_time"].is_number())
            chattext.timestamp = (uint64_t)msg["create_time"].get<double>();

        if (content_type == "reasoning_recap" || content_type == "thoughts") {
            chattext.thinking = extracted_text;
            if (!chattext.thinking.empty())
                temp.push_back(chattext);
            continue;
        }

        if (!extracted_text.empty())
            Santizer(chattext, extracted_text);

        if (msg.contains("metadata") &&
            msg["metadata"].contains("content_references") &&
            msg["metadata"]["content_references"].is_array())
        {
            for (const auto& ref :
                msg["metadata"]["content_references"])
            {
                if (ref.contains("type") &&
                    ref["type"].is_string())
                {
                    std::string ref_type =
                        ref["type"].get<std::string>();

                    if (ref_type == "followup_a" ||
                        ref_type == "conversation_context_citation")
                        continue;
                }

                if (ref.contains("url") &&
                    ref["url"].is_string())
                {
                    std::string url =
                        ref["url"].get<std::string>();
                    if (!url.empty() &&
                        url.find("utm_source=chatgpt") ==
                        std::string::npos)
                        chattext.urls.push_back(url);
                }

                if (ref.contains("items") &&
                    ref["items"].is_array())
                {
                    for (const auto& item : ref["items"]) {
                        if (item.contains("url") &&
                            item["url"].is_string())
                        {
                            std::string url =
                                item["url"].get<std::string>();
                            if (!url.empty() &&
                                url.find("utm_source=chatgpt") ==
                                std::string::npos)
                                chattext.urls.push_back(url);
                        }
                    }
                }
            }
        }

        if (msg.contains("metadata") &&
            msg["metadata"].contains("attachments") &&
            msg["metadata"]["attachments"].is_array())
        {
            for (const auto& file :
                msg["metadata"]["attachments"])
            {
                FileRef ref;
                if (file.contains("id") && file["id"].is_string())
                    ref.id = file["id"].get<std::string>();
                if (file.contains("name") && file["name"].is_string())
                    ref.filename = file["name"].get<std::string>();
                if (file.contains("mime_type") &&
                    file["mime_type"].is_string())
                    ref.mime_type =
                    file["mime_type"].get<std::string>();
                if (file.contains("size") && file["size"].is_number())
                    ref.size_bytes = file["size"].get<uint64_t>();
                if (file.contains("width") && file["width"].is_number())
                    ref.width = file["width"].get<int>();
                if (file.contains("height") &&
                    file["height"].is_number())
                    ref.height = file["height"].get<int>();
                chattext.assets.push_back(ref);
            }
        }

        if (chattext.blocks.empty() &&
            chattext.assets.empty() &&
            chattext.thinking.empty() &&
            chattext.urls.empty())
            continue;

        temp.push_back(chattext);
    }

    AppState::UpdateChatTokens(chat_id, total_tokens);

    if (!temp.empty())
        AppState::AddChatsToMap(chat_id, temp);
}

ChatTextParser::~ChatTextParser() {
    try {
        Chat_Text_Parser();
    }
    catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
    }
}