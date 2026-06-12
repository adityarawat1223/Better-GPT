#include "UploadWorker.h"
#include "memdb/AppState.h"
#include "Helpers/FileUploader.h"

#include <iostream>

void UploadWorker()
{
    while (true)
    {
        FileRef fileref;

        bool ok =
            AppState::Pop_Job(fileref);

        if (!ok)
            continue;

        try
        {
            FileUploader(fileref);
        }
        catch (const std::exception& e)
        {
            std::cout
                << "Upload worker error: "
                << e.what()
                << std::endl;
        }
    }
}