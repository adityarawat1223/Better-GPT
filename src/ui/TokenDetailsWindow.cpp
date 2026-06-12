#include "TokenDetailsWindow.h"
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QScrollBar>
#include "memdb/AppState.h"
#include <QStringList>

TokenDetailsWindow::TokenDetailsWindow(const std::string& chatId, QWidget* parent)
    : QDialog(parent), m_chatId(chatId)
{
    setupUi();
    loadAndCalculateContext();
}

TokenDetailsWindow::~TokenDetailsWindow()
{
}

void TokenDetailsWindow::setupUi()
{
    setWindowTitle("Context Window Details");
    setMinimumSize(800, 600);
    setStyleSheet("background-color: #1a1b1e; color: #ececed;");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* infoLabel = new QLabel("<b>This is the raw context retained by the model.</b><br/>"
                                   "It includes both visible messages and hidden system/tool instructions "
                                   "that fit within the model's token limit.", this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("font-size: 14px; margin-bottom: 10px;");
    mainLayout->addWidget(infoLabel);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet(
        "QTextEdit { background-color: #0c0d0f; border: 1px solid #282c35; border-radius: 4px; padding: 10px; font-family: monospace; font-size: 13px; }"
        "QScrollBar:vertical { background: transparent; width: 10px; }"
        "QScrollBar::handle:vertical { background: #3b3f4c; border-radius: 5px; }"
    );
    mainLayout->addWidget(m_textEdit);
}

void TokenDetailsWindow::loadAndCalculateContext()
{
    std::string slug = AppState::Get_Input_Box(m_chatId).model;
    if (slug.empty()) {
        slug = AppState::Get_Default_Model();
    }
    long long max_tokens = AppState::GetModelMaxTokens(slug);
    long long max_chars = max_tokens * 4;

    std::vector<ChatMessage> messages = AppState::GetChatsFromMap(m_chatId);
    if (messages.empty()) {
        m_textEdit->setPlainText("No context available.");
        return;
    }

    QStringList lines;
    long long current_chars = 0;

    // Traverse in reverse order to capture the "last X words" that the model remembers
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        const auto& msg = *it;
        if (msg.raw_content.empty()) continue;

        long long msg_len = msg.raw_content.length();

        if (current_chars + msg_len > max_chars) {
            // Cut off remaining part. If it's the oldest message that fits partially, 
            // the model only sees the end of it.
            long long remaining = max_chars - current_chars;
            if (remaining > 0) {
                std::string partial = msg.raw_content.substr(msg_len - remaining);
                QString roleStr = msg.is_system_or_tool ? "[System/Tool]" : (msg.user ? "[User]" : "[Assistant]");
                lines.append(roleStr + ":\n" + QString::fromStdString(partial) + "\n----------------------------------------\n");
            }
            break;
        } else {
            current_chars += msg_len;
            QString roleStr = msg.is_system_or_tool ? "[System/Tool]" : (msg.user ? "[User]" : "[Assistant]");
            lines.append(roleStr + ":\n" + QString::fromStdString(msg.raw_content) + "\n----------------------------------------\n");
        }
    }

    if (lines.isEmpty()) {
        m_textEdit->setPlainText("No context available.");
    } else {
        // Reverse the list since we appended in reverse order
        std::reverse(lines.begin(), lines.end());
        m_textEdit->setPlainText(lines.join(""));
        
        // Scroll to the bottom so the user sees the most recent interaction
        QScrollBar* bar = m_textEdit->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}
