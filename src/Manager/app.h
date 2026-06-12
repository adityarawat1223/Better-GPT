#pragma once

#include <windows.h>
#include "include/cef_app.h"

class App : public CefApp
{
private:
    void Cef_Init(CefMainArgs main_args);

    IMPLEMENT_REFCOUNTING(App);

public:
    App() = default;

    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override;

    void SetupCEF(CefMainArgs main_args);
    int Run();
    void StartWorkers();
};