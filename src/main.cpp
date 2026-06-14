#include <windows.h>
#include "Dto/token.h"
#include "Helpers/logger.h"
#include "Manager/app.h"
#include "memdb/AppState.h"

int main(int argc, char* argv[])
{

    try
    {
    std::filesystem::path sentinel = AppState::GetUserDir() / "logout_pending";

    if (std::filesystem::exists(sentinel)) 
    {

        std::filesystem::remove_all(AppState::GetUserDir() / "cef_persistent_profile");
        std::filesystem::remove(sentinel);
    }
    }
    catch(std::exception e){};
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    CefMainArgs main_args(hInstance);
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0)
        return exit_code;

    AllocConsole();

    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    {
        App app;
        app.SetupCEF(main_args);
        app.StartWorkers();
        if (AppState::GetTokenInfo() == "no") {
            Logger logger;
            logger.log_window();
        }

        app.Run();
    }
    CefShutdown();
    return 0;
}