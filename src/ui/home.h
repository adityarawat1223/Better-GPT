#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QListWidgetItem>
#include <QWidget>
#include <QPaintEvent>
#include <vector>
#include <string>
#include <unordered_map>
#include "Dto/chatItem.h"

// Forward declaration of ChatWindow
class ChatWindow;

class LoadingSpinner : public QWidget
{
    Q_OBJECT
public:
    explicit LoadingSpinner(QWidget* parent = nullptr);
    ~LoadingSpinner() override = default;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_angle = 0;
    QTimer* m_timer = nullptr;
};

class AvatarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AvatarWidget(QWidget* parent = nullptr);
    void setName(const QString& name);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_name;
};

class QPushButton;

class MainChatWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainChatWindow(QWidget* parent = nullptr);
    ~MainChatWindow() override;

protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private slots:
    void onSearchTextChanged(const QString& text);
    void onChatClicked(QListWidgetItem* item);
    void onNewChatClicked();
    void onLogoutClicked();
    void onLibraryClicked();
    void onAvatarClicked();
    void onChatContextMenu(const QPoint& pos);
    void updateState();

private:
    void setupUi();
    void refreshChatList();

    QLineEdit* searchEdit;
    QListWidget* chatListWidget;
    QPushButton* loadMoreBtn;
    AvatarWidget* avatarWidget;
    QWidget* loadingContainer;
    
    // Header tracking for native drag
    QWidget* m_headerArea;
    QPushButton* m_minimizeBtn;
    QPushButton* m_maximizeBtn;
    QPushButton* m_closeBtn;

    std::vector<ChatItem> m_chats;
    size_t m_lastChatCount = -1;
    std::string m_lastUsername;
    std::unordered_map<std::string, ChatWindow*> m_activeChatWindows;
};