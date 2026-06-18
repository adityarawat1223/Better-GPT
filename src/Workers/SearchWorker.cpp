#include "SearchWorker.h"
#include "memdb/AppState.h"
#include "ui/EventDispatcher.h"
#include <QString>
#include <QRegularExpression>
#include <iostream>

void SearchWorker()
{
    while (true)
    {
        SearchJob job;
        bool ok = AppState::Pop_Search_Job(job);
        if (!ok) continue;

        QString query = QString::fromStdString(job.query);
        if (query.trimmed().isEmpty()) {
            continue;
        }

        QRegularExpression regex(QRegularExpression::escape(query), QRegularExpression::CaseInsensitiveOption);

        for (const std::string& chatId : job.chatIds)
        {
            if (AppState::GetActiveSearchId() != job.searchId) {
                break;
            }

            std::vector<ChatMessage> messages = AppState::GetChatsFromMap(chatId);
            if (messages.empty()) continue;
            
            for (const ChatMessage& msg : messages)
            {
                if (AppState::GetActiveSearchId() != job.searchId) {
                    break;
                }

                if (msg.is_system_or_tool) continue;

                QString content = QString::fromStdString(msg.raw_content);
                QRegularExpressionMatch match = regex.match(content);
                
                if (match.hasMatch()) {
                    int matchStart = match.capturedStart();
                    int matchEnd = match.capturedEnd();
                    
                    int snippetStart = std::max(0, matchStart - 40);
                    int snippetLength = std::min<int>(static_cast<int>(content.length()) - snippetStart, 120);
                    
                    QString snippet = content.mid(snippetStart, snippetLength);
                    
                    if (snippetStart > 0) snippet.prepend("...");
                    if (snippetStart + snippetLength < content.length()) snippet.append("...");
                    
                    snippet.replace('\n', ' ');

                    emit EventDispatcher::instance()->searchResultFound(
                        QString::fromStdString(chatId), 
                        QString::fromStdString(msg.message_id), 
                        snippet, 
                        static_cast<quint64>(msg.timestamp), 
                        job.searchId
                    );
                }
            }
        }
    }
}
