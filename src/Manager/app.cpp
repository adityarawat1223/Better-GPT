#include "app.h"
#include "ui/home.h"
#include "Workers/UploadWorker.h"
#include "Workers/TokenWorker.h"
#include "Workers/SearchWorker.h"
#include "memdb/AppState.h"

#include <QApplication>
#include <QFontDatabase>
#include <QDir>
#include <QStyleFactory>
#include <thread>
#include <filesystem>
#include <iostream>

void App::Cef_Init(CefMainArgs main_args)
{
    CefSettings settings;
    settings.no_sandbox = true;

    std::filesystem::path absolute_cache =
        AppState::GetUserDir() / "cef_persistent_profile";
    std::wstring cache_path = absolute_cache.wstring();

    CefString(&settings.cache_path).FromWString(cache_path);

    settings.persist_session_cookies = true;
    settings.multi_threaded_message_loop = true;
    settings.log_severity = LOGSEVERITY_DISABLE;
    
    CefInitialize(
        main_args,
        settings,
        nullptr,
        nullptr);
}

void App::SetupCEF(CefMainArgs main_args)
{
    Cef_Init(main_args);
}

void App::StartWorkers()
{
    std::thread(UploadWorker).detach();
    std::thread(TokenWorker).detach();
    std::thread(SearchWorker).detach();
}

int App::Run()
{
    int argc = 0;
    char* argv[] = { nullptr };
    QApplication a(argc, argv);
    qRegisterMetaType<std::string>("std::string");

    QString stylesheet = R"(
        /* Global Background & Typography */
        QMainWindow, QDialog, QWidget {
            background-color: #0c0d0f;
            color: #e3e4e6;
            font-family: "Noto Sans", "Segoe UI", sans-serif;
        }

        /* Top Bar styling */
        QWidget#topBar {
            background-color: #070809;
            border-bottom: 1px solid #18191c;
        }

        /* Scroll Area */
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QScrollArea > QWidget > QWidget {
            background-color: transparent;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            border: none;
            background: #111214;
            width: 8px;
            margin: 0px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #232529;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #32353b;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            border: none;
            background: none;
            height: 0px;
        }

        /* Inputs */
        QLineEdit, QTextEdit {
            background-color: #121316;
            border: 1px solid #202226;
            border-radius: 8px;
            color: #f0f1f2;
            padding: 8px 12px;
        }
        QLineEdit:focus, QTextEdit:focus {
            border: 1px solid #00c0a3;
        }

        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            background-color: #121316;
            border: 1px solid #1c1d21;
            border-radius: 8px;
            margin-bottom: 8px;
            padding: 0px;
        }
        QListWidget::item:hover {
            background-color: #18191d;
            border-color: #27292f;
        }
        QListWidget::item:selected {
            background-color: #142838;
            border-color: #1d3c54;
            color: #ffffff;
        }

        QPushButton {
            background-color: #16181d;
            border: 1px solid #282c35;
            border-radius: 6px;
            color: #e3e4e6;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #1f2229;
            border-color: #383e4a;
        }
        QPushButton:pressed {
            background-color: #0f1013;
        }

        QPushButton#accentButton {
            background-color: #00c0a3;
            border: none;
            color: #070809;
            font-weight: bold;
            border-radius: 8px;
            padding: 8px 16px;
        }
        QPushButton#accentButton:hover {
            background-color: #00d9b8;
        }
        QPushButton#accentButton:pressed {
            background-color: #00a68d;
        }

        QPushButton#sendButton {
            background-color: #00c0a3;
            border: none;
            color: #070809;
            font-weight: bold;
            border-radius: 8px;
            font-size: 14px;
        }
        QPushButton#sendButton:hover {
            background-color: #00d9b8;
        }
        QPushButton#sendButton:pressed {
            background-color: #00a68d;
        }
    )";
    a.setStyleSheet(stylesheet);

    QString appDir = QCoreApplication::applicationDirPath();
    QStringList fontPaths = {
        appDir + "/fonts/NotoSans-Regular.ttf",
        "./fonts/NotoSans-Regular.ttf",
        appDir + "/fonts/JetBrainsMonoNL-Regular.ttf",
        "./fonts/JetBrainsMonoNL-Regular.ttf"
    };

    QString loadedFamily = "Segoe UI";
    for (const auto& path : fontPaths) {
        int fontId = QFontDatabase::addApplicationFont(path);
        if (fontId != -1) {
            QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            if (!families.isEmpty() && path.contains("NotoSans")) {
                loadedFamily = families.first();
            }
        }
    }
    a.setFont(QFont(loadedFamily, 10));

    MainChatWindow w;
    w.show();

    return a.exec();
}

void App::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line)
{
    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");
    command_line->AppendSwitch("disable-gpu-rasterization");
    command_line->AppendSwitch("disable-software-rasterizer");
    command_line->AppendSwitch("disable-smooth-scrolling");
    command_line->AppendSwitch("js-flags=--expose-gc --max-semi-space-size=1 --max-old-space-size=128");
    command_line->AppendSwitch("mute-audio");
    command_line->AppendSwitch("disable-audio-output");
    command_line->AppendSwitch("disable-renderer-backgrounding");
    command_line->AppendSwitch("disable-background-timer-throttling");
    command_line->AppendSwitch("disable-backgrounding-occluded-windows");
    command_line->AppendSwitch("disable-extensions");
    command_line->AppendSwitch("disable-plugins");
    command_line->AppendSwitch("disable-printing");
}