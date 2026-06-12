#pragma once
#include <string>
#include "memdb/AppState.h"
#include <iostream>
#include "Helpers/json.hpp"

using json = nlohmann::json;
class FileScripts {
public:
	void static  FileExec(const std::string& FileId);
	void static FileDownload(const std::string& url);
	void static  FileUploaderInit(const FileRef &fileref);
	void static  FileUploadFin(
		const std::string& file_id,
		const std::string& filename,
		const std::string& chat_id,
		const std::string& origination_message_id,
		const std::string& local_id
	);
	void static NoCredFileDownload(const std::string& url);

};