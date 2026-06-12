cmake_minimum_required(VERSION 3.16)

project(DESKTOP_GPT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add MSVC-specific flag for proper __cplusplus macro definition
if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.11.1/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(NOMINMAX)
set(CEF_ROOT "${CMAKE_SOURCE_DIR}/tools")

add_subdirectory(${CEF_ROOT})

add_executable(DESKTOP_GPT WIN32
    src/main.cpp
    src/Manager/app.cpp
    src/Helpers/logger.cpp
    src/Parsers/ChatListParser.cpp
    src/memdb/AppState.cpp
    src/Parsers/ChatTextParser.cpp
    src/Helpers/ReqRunner.cpp
    src/Helpers/Sanitizer.cpp
    src/Parsers/FileLinkParser.cpp
    src/Parsers/FileParser.cpp
    src/ui/home.cpp
    src/ui/chatwindow.cpp
    src/ui/TokenDetailsWindow.h
    src/ui/TokenDetailsWindow.cpp
    src/ui/SearchWindow.h
    src/ui/SearchWindow.cpp
    src/ui/EventDispatcher.h
    src/ui/EventDispatcher.cpp
    "src/Dto/Models.h"
    "src/Parsers/ModelsParser.h"
    "src/Parsers/ModelsParser.cpp"
    "src/Dto/User.h"
    "src/Dto/FileRef.h"
    "src/Parsers/FileUploaderAndParser.h"
    "src/Parsers/FileUploaderAndParser.cpp"
    "src/Helpers/FileUploader.cpp"
    "src/Helpers/FileUploader.h"
    "src/Workers/UploadWorker.h"
    "src/Workers/UploadWorker.cpp"
    "src/Helpers/FileFin.h"
    "src/Helpers/Filefin.cpp"
    "src/Helpers/Fileinit.h"
    "src/Helpers/Fileinit.cpp"
    "src/Serializer/MessageSerializer.h"
    "src/Serializer/MessageSerializer.cpp"
    "src/Helpers/ChatTokenGrabber.h"
    "src/Helpers/ChatTokenGrabber.cpp"
    "src/Parsers/ChatTokenParser.h"
    "src/Parsers/ChatTokenParser.cpp"
    "src/Workers/TokenWorker.h"
    "src/Workers/TokenWorker.cpp"
    "src/Workers/SearchWorker.h"
    "src/Workers/SearchWorker.cpp"
    "src/Scripts/TokenScript.h"
    "src/Scripts/TokenScript.cpp"
    "src/Scripts/MessageScript.h"
    "src/Scripts/MessageScript.cpp"
    "src/Scripts/ChatExecScript.h"
    "src/Scripts/ChatExecScript.cpp"
    "src/Scripts/FileScripts.h"
    "src/Scripts/FileScripts.cpp"
    "src/Scripts/MeScript.h"
    "src/Scripts/MeScript.cpp"
    "src/Parsers/MeParser.h"
    "src/Parsers/MeParser.cpp"
    "src/Scripts/LibraryScript.h"
    "src/Scripts/LibraryScript.cpp"
    "src/Parsers/LibraryParser.h"
    "src/Parsers/LibraryParser.cpp"
    "src/Dto/Windows.h"
 "src/Parsers/InitParser.cpp" "src/Parsers/InitParser.h" "src/Scripts/InitScript.h" "src/Scripts/InitScript.cpp" "src/Scripts/ModelScript.h" "src/Scripts/ModelScript.cpp" "src/Scripts/TestScript.h" "src/Scripts/TestScript.cpp" "src/Scripts/RenameScript.h" "src/Scripts/RenameScript.cpp" "src/Scripts/ChatDeleteScript.h" "src/Scripts/ChatDeleteScript.cpp" "src/Scripts/ChatListFetchScript.h" "src/Scripts/ChatListFetchScript.cpp")

target_include_directories(DESKTOP_GPT PRIVATE
    src
    ${CEF_ROOT}
    Libraries
)

target_link_libraries(DESKTOP_GPT PRIVATE
    libcef_dll_wrapper
    "${CEF_ROOT}/Release/libcef.lib"
    winhttp
    dwmapi
    user32
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if (WIN32)
    add_custom_command(TARGET DESKTOP_GPT POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Release"
        "$<TARGET_FILE_DIR:DESKTOP_GPT>"
    )

    add_custom_command(TARGET DESKTOP_GPT POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Resources"
        "$<TARGET_FILE_DIR:DESKTOP_GPT>"
    )

    add_custom_command(TARGET DESKTOP_GPT POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:DESKTOP_GPT>/fonts"
    )

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS "C:/Qt/6.11.1/msvc2022_64/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET DESKTOP_GPT POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:DESKTOP_GPT>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()