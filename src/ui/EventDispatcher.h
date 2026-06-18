#pragma once
#include <QObject>
#include <string>
class EventDispatcher : public QObject
{
    Q_OBJECT
public:
    static EventDispatcher* instance()
    {
        static EventDispatcher inst;
        return &inst;
    }
signals:
    void chatListUpdated();
    void chatMessageUpdated(const std::string& chatId);
   

    void chatIdSwapped(const std::string& oldId, const std::string& newId);
    void assetsUpdated(const std::string& chatId, const std::string& assetId);
    void searchResultFound(const QString& chatId, const QString& messageId, const QString& snippet, quint64 timestamp, int searchId);
    void jumpToMessageRequested(const std::string& chatId, const std::string& messageId);
    void fileDownloadComplete(const std::string& fileName);
    void libraryUpdated();
private:
    EventDispatcher() = default;
    ~EventDispatcher() = default;
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;
};