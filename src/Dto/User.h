#pragma once
#include <string>
struct User {
	std::string default_model;
	std::string user_name;
	std::string pfp_url;
	std::string email;
	std::int64_t created_at;
	std::string user_id;
	std::int64_t file_upload_limit;
	std::int64_t deep_research_limit;
	std::int64_t paste_text_to_file_limit;
	std::int64_t image_gen_limit;
	std::int64_t deep_reset;
	std::int64_t file_upload_reset;
	std::int64_t txttofilereset;
	std::int64_t image_gen_reset;
};