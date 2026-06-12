#include "InitParser.h"

#include <iostream>
#include  "Helpers/ReqRunner.h"

bool InitParser::InitFilter() {
    return true;
}

CefResponseFilter::FilterStatus  InitParser::Filter(
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
static const std::int64_t to_epoch_seconds(const std::string& ts) {
    std::istringstream in(ts);

    std::chrono::sys_time<std::chrono::microseconds> tp;
    in >> std::chrono::parse("%FT%T%Ez", tp);

    return std::chrono::duration_cast<std::chrono::seconds>(
        tp.time_since_epoch()
    ).count();
}

void InitParser::GetLimits() {


    try {
        json init = json::parse(raw_data);
        std::cout << "Prinint Raw-data" + raw_data << std::endl;

        if (init.contains("limits_progress") && init["limits_progress"].is_array()) {

            for (auto& ele : init["limits_progress"]) {

                if (!ele.contains("feature_name") || !ele.contains("remaining") || !ele.contains("reset_after")) {
                    return;
                }

                std::string featurename = ele["feature_name"].get<std::string>(), reset_after = ele["reset_after"].get<std::string>();
                int64_t remaining = ele["remaining"].get<int64_t>();
                if (featurename.empty() || reset_after.empty()) {
                    return;
                }
                int64_t reset_time = 0;
                try {
                    reset_time = to_epoch_seconds(reset_after);
                } catch (...) {
                    std::cout << "Failed to parse time: " << reset_after << std::endl;
                }
                std::cout << featurename + " " << reset_time << std::endl;

                AppState::Update_User_Feature_Limit(featurename, remaining, reset_time);
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
    } catch (...) {
        // Ignore JSON parsing errors
    }
}
InitParser::~InitParser() {
    GetLimits();
}