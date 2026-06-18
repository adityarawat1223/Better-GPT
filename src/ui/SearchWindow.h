#pragma once

#include <QDialog>
#include <QString>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class SearchWindow : public QDialog
{
    Q_OBJECT
public:
    static void ShowWindow(QWidget* parent = nullptr);
    ~SearchWindow();

private slots:
    void onSearchClicked();
    void onSearchResult(const QString& chatId, const QString& messageId, const QString& snippet, quint64 timestamp, int searchId);
    void onResultDoubleClicked(QListWidgetItem* item);


private:
    explicit SearchWindow(QWidget* parent = nullptr);
    void setupUi();
    void populateChatList();

    QLineEdit* m_searchEdit;
    QListWidget* m_chatCheckList;
    QPushButton* m_searchBtn;
    QListWidget* m_resultsList;
    int m_currentSearchId = -1;

    static SearchWindow* s_instance;
};
