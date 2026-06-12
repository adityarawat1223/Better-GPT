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

        // We use a regex for case-insensitive matching and to extract a snippet
        QRegularExpression regex(QRegularExpression::escape(query), QRegularExpression::CaseInsensitiveOption);

        for (const std::string& chatId : job.chatIds)
        {
            // Abort if a new search was started
            if (AppState::GetActiveSearchId() != job.searchId) {
                break;
            }

            std::vector<ChatMessage> messages = AppState::GetChatsFromMap(chatId);
            if (messages.empty()) continue;
            
            for (const ChatMessage& msg : messages)
            {
                // Re-check abort
                if (AppState::GetActiveSearchId() != job.searchId) {
                    break;
                }

                if (msg.is_system_or_tool) continue;

                QString content = QString::fromStdString(msg.raw_content);
                QRegularExpressionMatch match = regex.match(content);
                
                if (match.hasMatch()) {
                    // Extract snippet (e.g. 40 chars before and 80 after)
                    int matchStart = match.capturedStart();
                    int matchEnd = match.capturedEnd();
                    
                    int snippetStart = std::max(0, matchStart - 40);
                    int snippetLength = std::min<int>(static_cast<int>(content.length()) - snippetStart, 120);
                    
                    QString snippet = content.mid(snippetStart, snippetLength);
                    
                    // Add ellipsis if truncated
                    if (snippetStart > 0) snippet.prepend("...");
                    if (snippetStart + snippetLength < content.length()) snippet.append("...");
                    
                    // Clean up newlines for display
                    snippet.replace('\n', ' ');

                    // Emit via EventDispatcher
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
