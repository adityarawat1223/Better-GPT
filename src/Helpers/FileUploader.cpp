#include "FileUploader.h"
#include "Helpers/json.hpp"
#include "memdb/AppState.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

#include <windows.h>
#include <winhttp.h>
#include "Helpers/ReqRunner.h"
#pragma comment(lib, "winhttp.lib")
#define STB_IMAGE_IMPLEMENTATION
using json = nlohmann::json;
namespace fs = std::filesystem;
#include "stb_image.h"
#include <chrono>
#include <random>

static bool WinHttpPutStreamed(
    const std::string& url,
    const fs::path& filepath,
    const std::string& mime_type)
{
    std::error_code ec;

    uintmax_t total_size =
        fs::file_size(filepath, ec);

    if (ec)
    {
        std::cerr
            << "WinHTTP: failed to get file size"
            << std::endl;

        return false;
    }

    std::wstring wurl(url.begin(), url.end());

    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);

    wchar_t host[256] = {};
    wchar_t path[2048] = {};

    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;

    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(
        wurl.c_str(),
        0,
        0,
        &uc))
    {
        std::cerr
            << "WinHTTP: failed to parse URL"
            << std::endl;

        return false;
    }

    HINTERNET session =
        WinHttpOpen(
            L"NativeUploader/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

    if (!session)
        return false;

    HINTERNET connect =
        WinHttpConnect(
            session,
            host,
            uc.nPort,
            0);

    if (!connect)
    {
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD flags =
        (uc.nScheme == INTERNET_SCHEME_HTTPS)
        ? WINHTTP_FLAG_SECURE
        : 0;

    HINTERNET request =
        WinHttpOpenRequest(
            connect,
            L"PUT",
            path,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags | WINHTTP_FLAG_BYPASS_PROXY_CACHE);

    if (!request)
    {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    std::wstring mime_w(
        mime_type.begin(),
        mime_type.end());

    std::wstring headers =
        L"Content-Type: " +
        mime_w +
        L"\r\n"
        L"x-ms-blob-type: BlockBlob\r\n";

    WinHttpAddRequestHeaders(
        request,
        headers.c_str(),
        (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(
        request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        (DWORD)total_size,
        0))
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        return false;
    }

    std::ifstream file(
        filepath,
        std::ios::binary);

    if (!file.is_open())
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        return false;
    }

    const size_t CHUNK = 64 * 1024;

    std::vector<char> buffer(CHUNK);

    bool ok = true;

    while (
        file.read(buffer.data(), buffer.size()) ||
        file.gcount() > 0)
    {
        DWORD bytesRead =
            (DWORD)file.gcount();

        DWORD bytesWritten = 0;

        if (!WinHttpWriteData(
            request,
            buffer.data(),
            bytesRead,
            &bytesWritten))
        {
            std::cerr
                << "WinHTTP: stream write failed"
                << std::endl;

            ok = false;
            break;
        }
    }

    file.close();

    if (ok)
    {
        if (!WinHttpReceiveResponse(
            request,
            nullptr))
        {
            std::cerr
                << "WinHTTP: failed receiving response"
                << std::endl;

            ok = false;
        }
        else
        {
            DWORD status = 0;
            DWORD status_size = sizeof(status);

            WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &status_size,
                WINHTTP_NO_HEADER_INDEX);

            if (status != 200 &&
                status != 201)
            {
                std::cerr
                    << "WinHTTP: server returned "
                    << status
                    << std::endl;

                ok = false;
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    return ok;
}

static std::unordered_map<std::string, std::string> MimeTypes =
{
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".webp", "image/webp"},
    {".txt",  "text/plain"},
    {".json", "application/json"},
    {".pdf",  "application/pdf"},
    {".csv",  "text/csv"},
    {".cpp",  "text/plain"},
    {".h",    "text/plain"},
    {".py",   "text/plain"},
    {".java", "text/plain"},
    {".zip",  "application/zip"}
};
void FileUploader(FileRef fileref)
{
    try
    {
       
        fs::path src(fileref.source_path);

        if (!fs::exists(src))
        {
            std::cerr
                << "[Uploader] Source file missing"
                << std::endl;

            AppState::Update_Asset_Status(
                UploadStatus::Failed,
                fileref.chat_id,
                fileref.local_id);

            return;
        }

     

        std::string extension =
            src.extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        std::string stem =
            src.stem().string();

        auto now =
            std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

        std::random_device rd;

        fileref.local_id =
            stem + "_" +
            std::to_string(now) + "_" +
            std::to_string(rd()) +
            extension;

        fileref.filename =
            src.filename().string();

        fileref.size_bytes =
            fs::file_size(src);

        fileref.mime_type =
            (MimeTypes.find(extension)
                != MimeTypes.end())
            ?
            MimeTypes[extension]
            :
            "application/octet-stream";

        int width = 0;
        int height = 0;
        int channels = 0;

        bool isImage =
            stbi_info(
                fileref.source_path.c_str(),
                &width,
                &height,
                &channels);

        if (isImage)
        {
            fileref.width = width;
            fileref.height = height;
        }

        fileref.uploadstatus =
            UploadStatus::Queued;

     
        AppState::Add_File_Asset(
            fileref,
            fileref.chat_id,
            fileref.local_id);

        
        std::cout
            << "[Uploader] Requesting upload init..."
            << std::endl;

        ReqRunner::FileUploaderInit(
            fileref);

   

        const int TIMEOUT_MS = 30000;
        const int POLL_INTERVAL_MS = 20;

        int elapsed_ms = 0;

        while (elapsed_ms < TIMEOUT_MS)
        {
            UploadStatus uploadstatus = AppState::Get_Asset_Status(fileref.chat_id, fileref.local_id);
            if (uploadstatus == UploadStatus::Processing)
            {
                fileref = AppState::Get_File_Asset(fileref.chat_id,fileref.local_id);

                break;
            }

            if (uploadstatus
                == UploadStatus::Failed)
            {
                std::cerr
                    << "[Uploader] Upload init failed"
                    << std::endl;

                return;
            }

            Sleep(POLL_INTERVAL_MS);

            elapsed_ms +=
                POLL_INTERVAL_MS;
        }


        if (fileref.upload_url.empty() || fileref.id.empty())
        {
            std::cerr << fileref.upload_url << std::endl;
            std::cerr << fileref.id << std::endl;
            std::cerr
                << "[Uploader] Upload init timeout"
                << std::endl;

            AppState::Update_Asset_Status(
                UploadStatus::Failed,
                fileref.chat_id,
                fileref.local_id);

            return;
        }


        AppState::Update_Asset_Status(
            UploadStatus::Uploading,
            fileref.chat_id,
            fileref.local_id);

        std::cout
            << "[Uploader] Uploading: "
            << fileref.filename
            << std::endl;

        bool uploaded =
            WinHttpPutStreamed(
                fileref.upload_url,
                src,
                fileref.mime_type);

        if (!uploaded)
        {
            std::cerr
                << "[Uploader] Upload failed"
                << std::endl;

            AppState::Update_Asset_Status(
                UploadStatus::Failed,
                fileref.chat_id,
                fileref.local_id);

            return;
        }


        AppState::Update_Asset_Status(
            UploadStatus::Processing,
            fileref.chat_id,
            fileref.local_id);

        std::cout
            << "[Uploader] Awaiting GPT processing..."
            << std::endl;

        ReqRunner::FileUploadFin(
            fileref.id,
            fileref.filename,
            fileref.chat_id,
            AppState::Get_Parent(
                fileref.id),
            fileref.local_id);



        elapsed_ms = 0;

        while (elapsed_ms < TIMEOUT_MS)
        {
            UploadStatus uploadstatus = AppState::Get_Asset_Status(fileref.chat_id, fileref.local_id);

            if (uploadstatus
                == UploadStatus::Ready)
            {
                fileref = AppState::Get_File_Asset(fileref.chat_id, fileref.local_id);

                break;
            }

            if (uploadstatus
                == UploadStatus::Failed)
            {
                std::cerr
                    << "[Uploader] GPT processing failed"
                    << std::endl;

                return;
            }

            Sleep(POLL_INTERVAL_MS);

            elapsed_ms +=
                POLL_INTERVAL_MS;
        }


        if (fileref.uploadstatus
            != UploadStatus::Ready)
        {
            std::cerr
                << "[Uploader] GPT processing timeout"
                << std::endl;

            AppState::Update_Asset_Status(
                UploadStatus::Failed,
                fileref.chat_id,
                fileref.local_id);

            return;
        }

     
        fs::path cache_dir = AppState::GetUserDir() / "cache";

        fs::create_directories(cache_dir);

        fs::path cached_file =
            cache_dir / (fileref.lib_file_id + extension);

        std::cout
            << "[Uploader] Caching file..."
            << std::endl;

        std::error_code ec;

        for (int i = 0; i < 10; ++i)
        {
            ec.clear();

            fs::copy_file(
                src,
                cached_file,
                fs::copy_options::overwrite_existing,
                ec);

            if (!ec)
                break;

            Sleep(50);
        }

        if (ec)
        {
            std::cerr
                << "[Uploader] Cache copy failed: "
                << ec.message()
                << std::endl;
            AppState::Update_Asset_Status(
                UploadStatus::Failed,
                fileref.chat_id,
                fileref.local_id);
            return;
        }
        AppState::Update_Asset_Status(
            UploadStatus::Complete,
            fileref.chat_id,
            fileref.local_id);
        std::cout
            << "[Uploader] Upload transaction complete"
            << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "FileUploader exception: "
            << e.what()
            << std::endl;
    }
}
