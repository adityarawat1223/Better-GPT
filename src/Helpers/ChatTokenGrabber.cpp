#include "ChatTokenGrabber.h"
#include "Helpers/json.hpp"
#include "memdb/AppState.h"
#include <iostream>
#include <string>


using json = nlohmann::json;


void ChatTokenGrabber(const std::string& raw_data) {

	json j = json::parse(raw_data);
		if (j.contains("prepare_token") && j["prepare_token"].is_string()) {
		std::string temp = j["prepare_token"].get<std::string>();
		std::string key = "openai-sentinel-chat-requirements-prepare-token";
		AppState::UpdateHeaders(key, temp);

	}
	if (j.contains("proofofwork") && j["proofofwork"].is_string()) {
		std::string temp = j["proofofwork"].get<std::string>();
		std::string key = "openai-sentinel-proof-token";
		AppState::UpdateHeaders(key, temp);

	}
	if (j.contains("turnstile") && j["turnstile"].is_string()) {
		std::string temp = j["turnstile"].get<std::string>();
		std::string key = "openai-sentinel-turnstile-token";
		AppState::UpdateHeaders(key, temp);
	}
}