#pragma once
#include <string>

// Forward declaration of Qt Widget
class QWidget;

enum class WindowType
{
    Home,
    Conversation
};

struct WindowState
{
    WindowType type;
    QWidget* widget = nullptr; // Pointer to the Qt window/widget
    std::string chat_id;
    bool focus = false;
    bool open = true;
};