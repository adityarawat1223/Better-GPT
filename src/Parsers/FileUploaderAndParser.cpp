#include "FileUploaderAndParser.h"
#include "Helpers/json.hpp"
using json = nlohmann::json;
#include <filesystem> 


bool FileUploaderAndParser::InitFilter() {
	return true;
}

CefResponseFilter::FilterStatus  FileUploaderAndParser::Filter(
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

void FileUploaderAndParser::File_Uploader() {

}


FileUploaderAndParser::~FileUploaderAndParser() {

	File_Uploader();
}