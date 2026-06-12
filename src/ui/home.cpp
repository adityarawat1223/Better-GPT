#include "home.h"
#include "chatwindow.h"
#include "SearchWindow.h"
#include "memdb/AppState.h"
#include "Helpers/ReqRunner.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QWidgetAction>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QMessageBox>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <chrono>
#include "EventDispatcher.h"
#include <QInputDialog>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

// ---------------------------------------------------------------------------
// LoadingSpinner implementation
// ---------------------------------------------------------------------------
LoadingSpinner::LoadingSpinner(QWidget* parent) : QWidget(parent), m_angle(0)
{
    setFixedSize(40, 40);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 10) % 360;
        update();
    });
    m_timer->start(30);
}

void LoadingSpinner::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.translate(width() / 2.0, height() / 2.0);
    painter.rotate(m_angle);

    QPen pen;
    pen.setWidth(4);
    pen.setCapStyle(Qt::RoundCap);

    for (int i = 0; i < 12; ++i) {
        QColor color("#00c0a3");
        color.setAlphaF(static_cast<qreal>(i) / 12.0);
        pen.setColor(color);
        painter.setPen(pen);
        painter.drawLine(0, -12, 0, -6);
        painter.rotate(30.0);
    }
}

// ---------------------------------------------------------------------------
// AvatarWidget implementation
// ---------------------------------------------------------------------------
AvatarWidget::AvatarWidget(QWidget* parent) : QWidget(parent)
{
    setFixedSize(36, 36);
    setCursor(Qt::PointingHandCursor);
}

void AvatarWidget::setName(const QString& name)
{
    m_name = name;
    update();
}

void AvatarWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw circular background with accent color
    painter.setBrush(QColor("#00c0a3"));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect());

    // Draw text initial
    painter.setPen(QColor("#070809"));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(11);
    painter.setFont(font);

    QString initial = m_name.isEmpty() ? "?" : m_name.left(1).toUpper();
    painter.drawText(rect(), Qt::AlignCenter, initial);
}

// ---------------------------------------------------------------------------
// ChatCardWidget (Custom list item widget)
// ---------------------------------------------------------------------------
class ChatCardWidget : public QWidget
{
public:
    ChatCardWidget(const QString& title, const QString& date, QWidget* parent = nullptr) : QWidget(parent)
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(8);

        QLabel* titleLabel = new QLabel(title, this);
        titleLabel->setStyleSheet("color: #f0f1f2; font-weight: bold; font-size: 14px; background: transparent;");
        titleLabel->setWordWrap(true);

        QLabel* dateLabel = new QLabel(date.isEmpty() ? "Recent" : date, this);
        dateLabel->setStyleSheet("color: #7d8087; font-size: 11px; background: transparent;");

        layout->addWidget(titleLabel);
        layout->addWidget(dateLabel);
        setLayout(layout);
        setStyleSheet("background: transparent;");
    }
};

// ---------------------------------------------------------------------------
// MainChatWindow implementation
// ---------------------------------------------------------------------------
MainChatWindow::MainChatWindow(QWidget* parent) : QMainWindow(parent)
{
    setupUi();

    // Listen to backend events to sync UI state
    connect(EventDispatcher::instance(), &EventDispatcher::chatListUpdated, this, &MainChatWindow::updateState, Qt::QueuedConnection);
    
    connect(EventDispatcher::instance(), &EventDispatcher::chatIdSwapped, this, [this](const std::string& oldId, const std::string& newId) {
        // Update active windows map
        auto it = m_activeChatWindows.find(oldId);
        if (it != m_activeChatWindows.end()) {
            m_activeChatWindows[newId] = it->second;
            m_activeChatWindows.erase(it);
        }
        // Force chat list refresh
        m_lastChatCount = -1;
        updateState();
    }, Qt::QueuedConnection);

    // Initial state check
    updateState();
}

#ifdef Q_OS_WIN
bool MainChatWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    MSG* msg = static_cast<MSG*>(message);

    if (msg->message == WM_NCCALCSIZE) {
        if (msg->wParam == TRUE) {
            *result = 0;
            return true;
        }
        return false;
    }

    if (msg->message == WM_NCHITTEST) {
        // Let the default window proc handle it first to get native borders
        LRESULT hit = DefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        if (hit == HTNOWHERE || hit == HTCLIENT) {
            // We are over the client area. Check if we are over the custom title bar (m_headerArea)
            POINT pos;
            pos.x = GET_X_LPARAM(msg->lParam);
            pos.y = GET_Y_LPARAM(msg->lParam);
            ScreenToClient(msg->hwnd, &pos);
            
            // Check if pos is in m_headerArea but NOT over the buttons
            if (m_headerArea && m_headerArea->geometry().contains(pos.x, pos.y)) {
                // If it's over a button inside the header, return HTCLIENT so button works natively
                QPoint widgetPos = m_headerArea->mapFromParent(QPoint(pos.x, pos.y));
                if (m_minimizeBtn->geometry().contains(widgetPos) ||
                    m_maximizeBtn->geometry().contains(widgetPos) ||
                    m_closeBtn->geometry().contains(widgetPos)) {
                    *result = HTCLIENT;
                    return true;
                }
                *result = HTCAPTION;
                return true;
            }
        } else {
            *result = hit;
            return true;
        }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

MainChatWindow::~MainChatWindow()
{
    // Clean up any remaining chat windows
    for (auto& pair : m_activeChatWindows) {
        if (pair.second) {
            pair.second->close();
        }
    }
}

void MainChatWindow::setupUi()
{
    setWindowTitle("ChatGPT Reader");
    resize(1050, 750);

#ifdef Q_OS_WIN
    // Extend window frame into client area for native drop shadow
    HWND hwnd = (HWND)this->winId();
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Top Nav Bar
    QWidget* topBar = new QWidget(centralWidget);
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(65);

    QHBoxLayout* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(20, 0, 20, 0);

    QLabel* logoLabel = new QLabel("ChatGPT", topBar);
    logoLabel->setStyleSheet("color: #00c0a3; font-size: 18px; font-weight: bold; background: transparent;");
    topBarLayout->addWidget(logoLabel);

    topBarLayout->addStretch();

    // Nav Bar Buttons
    QPushButton* libBtn = new QPushButton("Library", topBar);
    connect(libBtn, &QPushButton::clicked, this, &MainChatWindow::onLibraryClicked);
    topBarLayout->addWidget(libBtn);

    QPushButton* logoutBtn = new QPushButton("Logout", topBar);
    connect(logoutBtn, &QPushButton::clicked, this, &MainChatWindow::onLogoutClicked);
    topBarLayout->addWidget(logoutBtn);

    avatarWidget = new AvatarWidget(topBar);
    // Bind click trigger by event filter or button over it
    QPushButton* avatarBtn = new QPushButton(topBar);
    avatarBtn->setFixedSize(36, 36);
    avatarBtn->setFlat(true);
    avatarBtn->setCursor(Qt::PointingHandCursor);
    avatarBtn->setStyleSheet("background: transparent; border: none;");
    connect(avatarBtn, &QPushButton::clicked, this, &MainChatWindow::onAvatarClicked);

    // Overlay button on top of AvatarWidget
    QHBoxLayout* avLayout = new QHBoxLayout(avatarWidget);
    avLayout->setContentsMargins(0, 0, 0, 0);
    avLayout->addWidget(avatarBtn);

    topBarLayout->addWidget(avatarWidget);
    
    // Window Controls
    topBarLayout->addSpacing(16);
    
    m_minimizeBtn = new QPushButton("—", topBar);
    m_maximizeBtn = new QPushButton("□", topBar);
    m_closeBtn = new QPushButton("✕", topBar);
    
    QString ctrlStyle = "QPushButton { background: transparent; border: none; color: #a0a0a0; font-family: 'Segoe UI'; font-size: 14px; } QPushButton:hover { background: #282c34; color: #fff; }";
    QString closeStyle = "QPushButton { background: transparent; border: none; color: #a0a0a0; font-family: 'Segoe UI'; font-size: 14px; } QPushButton:hover { background: #e81123; color: #fff; }";
    
    m_minimizeBtn->setFixedSize(46, 32);
    m_maximizeBtn->setFixedSize(46, 32);
    m_closeBtn->setFixedSize(46, 32);
    
    m_minimizeBtn->setStyleSheet(ctrlStyle);
    m_maximizeBtn->setStyleSheet(ctrlStyle);
    m_closeBtn->setStyleSheet(closeStyle);
    
    connect(m_minimizeBtn, &QPushButton::clicked, this, &MainChatWindow::showMinimized);
    connect(m_maximizeBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    connect(m_closeBtn, &QPushButton::clicked, this, &MainChatWindow::close);
    
    topBarLayout->addWidget(m_minimizeBtn);
    topBarLayout->addWidget(m_maximizeBtn);
    topBarLayout->addWidget(m_closeBtn);
    
    m_headerArea = topBar;
    
    mainLayout->addWidget(topBar);

    // 2. Middle Content (Centered, 880px width limit)
    QWidget* contentArea = new QWidget(centralWidget);
    QHBoxLayout* centerLayout = new QHBoxLayout(contentArea);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    centerLayout->addStretch(1);

    QWidget* centerContainer = new QWidget(contentArea);
    centerContainer->setFixedWidth(880);
    QVBoxLayout* containerLayout = new QVBoxLayout(centerContainer);
    containerLayout->setContentsMargins(0, 24, 0, 24);
    containerLayout->setSpacing(16);

    // Search & New Chat row
    QHBoxLayout* searchRow = new QHBoxLayout();
    searchRow->setSpacing(12);

    searchEdit = new QLineEdit(centerContainer);
    searchEdit->setPlaceholderText("Search conversations...");
    connect(searchEdit, &QLineEdit::textChanged, this, &MainChatWindow::onSearchTextChanged);
    searchRow->addWidget(searchEdit, 4);

    QPushButton* newChatBtn = new QPushButton("+ New Chat", centerContainer);
    newChatBtn->setObjectName("accentButton");
    newChatBtn->setFixedHeight(38);
    connect(newChatBtn, &QPushButton::clicked, this, &MainChatWindow::onNewChatClicked);
    searchRow->addWidget(newChatBtn, 1);

    QPushButton* univSearchBtn = new QPushButton(QIcon(), "🔍 Search Inside", centerContainer);
    univSearchBtn->setObjectName("accentButton");
    univSearchBtn->setFixedHeight(38);
    connect(univSearchBtn, &QPushButton::clicked, this, []() {
        SearchWindow::ShowWindow();
    });
    searchRow->addWidget(univSearchBtn, 1);

    containerLayout->addLayout(searchRow);

    // Recent Threads Header
    QLabel* threadsHeader = new QLabel("RECENT THREADS", centerContainer);
    threadsHeader->setStyleSheet("color: #7d8087; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;");
    containerLayout->addWidget(threadsHeader);

    // Chat List
    chatListWidget = new QListWidget(centerContainer);
    connect(chatListWidget, &QListWidget::itemClicked, this, &MainChatWindow::onChatClicked);
    chatListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(chatListWidget, &QListWidget::customContextMenuRequested, this, &MainChatWindow::onChatContextMenu);
    containerLayout->addWidget(chatListWidget);

    loadMoreBtn = new QPushButton("Load More", centerContainer);
    loadMoreBtn->setStyleSheet("QPushButton { background-color: transparent; color: #00c0a3; border: 1px solid #00c0a3; border-radius: 4px; padding: 6px; font-weight: bold; margin-top: 4px; } QPushButton:hover { background-color: rgba(0, 192, 163, 0.1); }");
    loadMoreBtn->setCursor(Qt::PointingHandCursor);
    loadMoreBtn->hide();
    connect(loadMoreBtn, &QPushButton::clicked, this, [this]() {
        ReqRunner::FetchChatList(static_cast<int>(m_chats.size()));
        loadMoreBtn->setText("Loading...");
        loadMoreBtn->setEnabled(false);
    });
    containerLayout->addWidget(loadMoreBtn);

    // Loading Container
    loadingContainer = new QWidget(centerContainer);
    QVBoxLayout* loadingLayout = new QVBoxLayout(loadingContainer);
    loadingLayout->setAlignment(Qt::AlignCenter);
    loadingLayout->setContentsMargins(0, 40, 0, 40);
    loadingLayout->setSpacing(16);

    LoadingSpinner* spinner = new LoadingSpinner(loadingContainer);
    QLabel* loadingLabel = new QLabel("Fetching conversations...", loadingContainer);
    loadingLabel->setStyleSheet("color: #7d8087; font-size: 13px; font-weight: 500; background: transparent;");

    loadingLayout->addWidget(spinner, 0, Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel, 0, Qt::AlignCenter);

    containerLayout->addWidget(loadingContainer);

    centerLayout->addWidget(centerContainer);
    centerLayout->addStretch(1);

    mainLayout->addWidget(contentArea);
}

void MainChatWindow::refreshChatList()
{
    chatListWidget->clear();

    QString searchText = searchEdit->text().toLower();

    if (m_chats.empty())
    {
        loadingContainer->show();
        chatListWidget->hide();
        loadMoreBtn->hide();
    }
    else
    {
        loadingContainer->hide();
        chatListWidget->show();
        
        if (AppState::HasMoreChats()) {
            loadMoreBtn->show();
            loadMoreBtn->setText("Load More");
            loadMoreBtn->setEnabled(true);
        } else {
            loadMoreBtn->hide();
        }
    }

    for (int i = 0; i < (int)m_chats.size(); i++)
    {
        const ChatItem& chat = m_chats[i];
        QString titleStr = QString::fromStdString(chat.title);

        // Filter by search text
        if (!searchText.isEmpty() && !titleStr.toLower().contains(searchText))
            continue;

        QListWidgetItem* item = new QListWidgetItem(chatListWidget);
        item->setData(Qt::UserRole, QString::fromStdString(chat.id));
        item->setData(Qt::UserRole + 1, i); // Save index
        item->setSizeHint(QSize(0, 96));

        QString dateStr = QString::fromStdString(chat.update_time);
        ChatCardWidget* card = new ChatCardWidget(titleStr, dateStr, chatListWidget);
        chatListWidget->setItemWidget(item, card);
    }
}

void MainChatWindow::onSearchTextChanged(const QString& /*text*/)
{
    refreshChatList();
}

void MainChatWindow::onChatClicked(QListWidgetItem* item)
{
    if (!item) return;

    std::string chatId = item->data(Qt::UserRole).toString().toUtf8().constData();
    
    // Check if the window is already active
    if (m_activeChatWindows.find(chatId) != m_activeChatWindows.end()) {
        QWidget* cw = m_activeChatWindows[chatId];
        if (cw) {
            cw->showNormal();
            cw->activateWindow();
            cw->raise();
            return; // Already open, just focus
        }
    }
    
    // Trigger backend fetch
    ReqRunner::ChatTextExecution(chatId);
    AppState::AddChat(chatId);
}

void MainChatWindow::onNewChatClicked()
{
    // Generate a temporary UUID for the new chat
    std::string tempId = "new-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    
    // Add it to m_chat_map so SwapChatId works and it appears in the sidebar
    ChatItem newItem;
    newItem.id = tempId;
    newItem.title = "New Chat";
    newItem.raw_update_time = ""; // Or some formatted time
    AppState::AppendChatItem(newItem);

    // Initialize empty chat data so ChatWindow can open
    std::vector<ChatMessage> empty;
    AppState::AddChatsToMap(tempId, empty);
    
    // Set parent to client_created_root for new chats
    AppState::Set_Parent(tempId, "client_created_root");
    
    // Open the chat window
    AppState::AddChat(tempId);
}

void MainChatWindow::onChatContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = chatListWidget->itemAt(pos);
    if (!item) return;

    // IMPORTANT: Store QByteArray in a named variable before calling constData().
    // toUtf8() returns a temporary — if you chain .constData() directly, the
    // temporary is destroyed before the std::string constructor reads it (heap corruption).
    QByteArray chatIdBytes = item->data(Qt::UserRole).toString().toUtf8();
    std::string chatId(chatIdBytes.constData(), chatIdBytes.size());
    QString chatTitle;
    
    // Find the title from the chat list
    for (const auto& chat : m_chats) {
        if (chat.id == chatId) {
            chatTitle = QString::fromStdString(chat.title);
            break;
        }
    }

    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #2b2b2b;
            color: #d1d1d1;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
        }
        QMenu::item {
            padding: 5px 20px;
        }
        QMenu::item:selected {
            background-color: #3e3e3e;
            color: white;
        }
    )");

    QAction* renameAction = menu.addAction("✏️ Rename");
    QAction* deleteAction = menu.addAction("🗑️ Delete");

    QAction* selected = menu.exec(chatListWidget->mapToGlobal(pos));

    if (selected == renameAction) {
        bool ok;
        QString newName = QInputDialog::getText(this, "Rename Chat",
            "Enter new name:", QLineEdit::Normal, chatTitle, &ok);
        if (ok && !newName.isEmpty()) {
            ReqRunner::RenameChat(chatId, newName.toStdString());
        }
    }
    else if (selected == deleteAction) {
        auto btn = QMessageBox::question(this, "Delete Chat",
            "Are you sure you want to delete \"" + chatTitle + "\"?",
            QMessageBox::Yes | QMessageBox::No);
        if (btn == QMessageBox::Yes) {
            ReqRunner::DeleteChat(chatId);
        }
    }
}

void MainChatWindow::onLogoutClicked()
{
    auto btn = QMessageBox::question(this, "Logout", "Are you sure you want to log out?", QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes) {
        std::filesystem::remove_all("./cef_persistent_profile");
        QMessageBox::information(centralWidget(), "Logout", "Logged out successfully. Please restart application to log in again.");
    }
}

void MainChatWindow::onLibraryClicked()
{
    QMessageBox::information(this, "Library", "Library module under construction.");
}

void MainChatWindow::onAvatarClicked()
{
    User cachedUser = AppState::Get_User();
    
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #2b2b2b;
            color: #d1d1d1;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
        }
        QMenu::item {
            padding: 5px 20px 5px 20px;
        }
        QMenu::item:selected {
            background-color: #3e3e3e;
            color: white;
        }
        QMenu::separator {
            height: 1px;
            background: #444444;
            margin-left: 10px;
            margin-right: 10px;
        }
    )");

    QString nameStr = cachedUser.user_name.empty() ? "Unknown User" : QString::fromStdString(cachedUser.user_name);
    QString emailStr = QString::fromStdString(cachedUser.email);
    QString modelStr = cachedUser.default_model.empty() ? "auto" : QString::fromStdString(cachedUser.default_model);

    menu.addAction("User: " + nameStr)->setEnabled(false);
    menu.addAction("Email: " + emailStr)->setEnabled(false);
    menu.addSeparator();
    menu.addAction("Default Model: " + modelStr)->setEnabled(false);

    menu.exec(QCursor::pos());
}

void MainChatWindow::updateState()
{
    // 1. Sync Chat List
    bool needsRefresh = false;
    size_t count = AppState::GetChatCount();
    auto newChats = AppState::GetChatListCopy();

    if (count != m_lastChatCount || newChats.size() != m_chats.size()) {
        needsRefresh = true;
    } else {
        for (size_t i = 0; i < newChats.size(); i++) {
            if (newChats[i].id != m_chats[i].id || newChats[i].title != m_chats[i].title) {
                needsRefresh = true;
                break;
            }
        }
    }

    if (needsRefresh)
    {
        m_chats = std::move(newChats);
        m_lastChatCount = m_chats.size();
        refreshChatList();
    }

    // 2. Sync User Info / Avatar
    User user = AppState::Get_User();
    if (user.user_name != m_lastUsername)
    {
        m_lastUsername = user.user_name;
        avatarWidget->setName(QString::fromStdString(m_lastUsername));
    }

    // 3. Sync Open Chat Windows
    std::set<std::string> openChats = AppState::GetOpenChats();
    
    // Check if we need to open new windows
    for (const auto& chatId : openChats)
    {
        if (m_activeChatWindows.find(chatId) == m_activeChatWindows.end())
        {
            // Open window
            ChatWindow* cw = new ChatWindow(chatId);
            cw->show();
            m_activeChatWindows[chatId] = cw;
        }
    }

    // Check if we need to remove closed windows
    auto it = m_activeChatWindows.begin();
    while (it != m_activeChatWindows.end())
    {
        if (openChats.find(it->first) == openChats.end())
        {
            // The window is no longer open in AppState. Close and delete it
            if (it->second) {
                if (it->second->isVisible()) {
                    it->second->close();
                }
                it->second->deleteLater();
            }
            it = m_activeChatWindows.erase(it);
        }
        else
        {
            ++it;
        }
    }
}