#pragma once
#include <string>
#include <Helpers//json.hpp>

enum class UploadStatus
{
    Queued,
    Uploading,
    Processing,
    Success,
    Ready,
    Complete,
    Failed
};
struct FileRef
{
    std::string id;
    std::string mime_type;
    std::string filename;
    uint64_t size_bytes = 0;
    int width = 0;
    int height = 0;
    UploadStatus uploadstatus;
    std::string local_id;
    std::string lib_file_id;
    std::string upload_url;
    std::string source_path;
    std::string chat_id;

};
NLOHMANN_JSON_SERIALIZE_ENUM(
    UploadStatus,
    {
        {UploadStatus::Queued, "Queued"},
        {UploadStatus::Uploading, "Uploading"},
        {UploadStatus::Success, "Success"},
        {UploadStatus::Processing, "Processing"},
        {UploadStatus::Ready , "Ready"},
        {UploadStatus::Complete , "Complete"},
        {UploadStatus::Failed, "Failed"}
    }
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    FileRef,
    id,
    filename,
    size_bytes,
    mime_type,
    width,
    height,
    uploadstatus,
    local_id,
    lib_file_id,
    upload_url,
    source_path,
    chat_id
)