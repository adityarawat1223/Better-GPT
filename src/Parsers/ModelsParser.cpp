#include "ModelsParser.h"
#include "Helpers/json.hpp"
using json = nlohmann::json;
#include <iostream>
#include "Dto/Models.h"
#include "memdb/AppState.h"
bool ModelsParser::InitFilter() {
	return true;
}

CefResponseFilter::FilterStatus  ModelsParser::Filter(
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


void ModelsParser::Get_Models() {

    try {

        json init = json::parse(raw_data);

        if (init.contains("models") && init["models"].is_array()) {

            for (auto& ele : init["models"]) {

                if (ele.contains("slug") && ele.contains("max_tokens")) {
                    
                    std::string slug = ele["slug"].get<std::string>();
                    long long max_tokens = ele["max_tokens"].get<long long>();
                    AppState::Model_Token_Info(slug, max_tokens);
                }
            }
        }


        if (init.contains("versions") &&
            init["versions"].is_array())
        {
            for (auto& ele : init["versions"])
            {
                if (!ele.contains("enabled") ||
                    !ele["enabled"].is_boolean() ||
                    !ele["enabled"].get<bool>())
                {
                    continue;
                }

                Models md;

                if (ele.contains("display_text_full") &&
                    ele["display_text_full"].is_string())
                {
                    md.name =
                        ele["display_text_full"]
                        .get<std::string>();
                }

                if (ele.contains("slugs") &&
                    ele["slugs"].is_array())
                {
                    for (auto& slug : ele["slugs"])
                    {
                        if (slug.is_string())
                        {
                            md.slugs.push_back(
                                slug.get<std::string>());
                        }
                    }
                }
                AppState::AddModels(md);
            }
        }

        if (init.contains("default_model_slug") && init["default_model_slug"].is_string()) {
            std::string m =
                init["default_model_slug"]
                .get<std::string>();
                if (m.empty()) {
                    return;
                }
            AppState::Set_Default_Model(m);
        }

    }
    catch (const std::exception& e)
    {
        std::cout << e.what();
    }
}

ModelsParser::~ModelsParser() {
    Get_Models();
}