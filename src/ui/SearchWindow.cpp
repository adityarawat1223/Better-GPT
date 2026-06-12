#include "SearchWindow.h"
#include "memdb/AppState.h"
#include "ui/EventDispatcher.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

SearchWindow* SearchWindow::s_instance = nullptr;

void SearchWindow::ShowWindow(QWidget* parent)
{
    if (!s_instance) {
        s_instance = new SearchWindow(parent);
    }
    s_instance->show();
    s_instance->raise();
    s_instance->activateWindow();
}

SearchWindow::SearchWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    populateChatList();

    connect(EventDispatcher::instance(), &EventDispatcher::searchResultFound, this, &SearchWindow::onSearchResult, Qt::QueuedConnection);
    setAttribute(Qt::WA_DeleteOnClose);
}

SearchWindow::~SearchWindow()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void SearchWindow::setupUi()
{
    setWindowTitle("Universal Chat Search");
    resize(600, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Search bar row
    QHBoxLayout* searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Enter search text...");
    m_searchBtn = new QPushButton("Search", this);
    searchRow->addWidget(m_searchEdit);
    searchRow->addWidget(m_searchBtn);
    mainLayout->addLayout(searchRow);

    // Chats selection
    mainLayout->addWidget(new QLabel("Select open chats to search:", this));
    m_chatCheckList = new QListWidget(this);
    m_chatCheckList->setMaximumHeight(150);
    mainLayout->addWidget(m_chatCheckList);

    // Results list
    mainLayout->addWidget(new QLabel("Search Results:", this));
    m_resultsList = new QListWidget(this);
    m_resultsList->setWordWrap(true);
    mainLayout->addWidget(m_resultsList);

    connect(m_searchBtn, &QPushButton::clicked, this, &SearchWindow::onSearchClicked);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchWindow::onSearchClicked);
}

void SearchWindow::populateChatList()
{
    m_chatCheckList->clear();
    std::set<std::string> openChats = AppState::GetOpenChats();
    std::vector<ChatItem> allChats = AppState::GetChatListCopy();

    for (const std::string& chatId : openChats)
    {
        QString title = "Unknown Chat";
        for (const auto& chat : allChats) {
            if (chat.id == chatId) {
                title = QString::fromStdString(chat.title);
                break;
            }
        }

        QListWidgetItem* item = new QListWidgetItem(title, m_chatCheckList);
        item->setData(Qt::UserRole, QString::fromStdString(chatId));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked); // Checked by default
    }
}

void SearchWindow::onSearchClicked()
{
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) return;

    std::vector<std::string> selectedChatIds;
    for (int i = 0; i < m_chatCheckList->count(); ++i) {
        QListWidgetItem* item = m_chatCheckList->item(i);
        if (item->checkState() == Qt::Checked) {
            QByteArray utf8Id = item->data(Qt::UserRole).toString().toUtf8();
            selectedChatIds.push_back(std::string(utf8Id.constData(), utf8Id.length()));
        }
    }

    if (selectedChatIds.empty()) {
        QMessageBox::warning(this, "No Chats Selected", "Please select at least one open chat to search.");
        return;
    }

    m_resultsList->clear();
    
    // Submit background job
    QByteArray utf8Query = query.toUtf8();
    std::string queryStr(utf8Query.constData(), utf8Query.length());
    AppState::Submit_Search_Job(queryStr, selectedChatIds);
    m_currentSearchId = AppState::GetActiveSearchId();
}

void SearchWindow::onSearchResult(const QString& chatId, const QString& messageId, const QString& snippet, quint64 timestamp, int searchId)
{
    // Ignore results from previous searches
    if (searchId != m_currentSearchId) return;

    QListWidgetItem* item = new QListWidgetItem(m_resultsList);
    item->setText(snippet);
    item->setData(Qt::UserRole, chatId);
    item->setData(Qt::UserRole + 1, messageId);
}
