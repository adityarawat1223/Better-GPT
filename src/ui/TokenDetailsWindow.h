#pragma once

#include <QDialog>
#include <QString>
#include <string>

class QTextEdit;

class TokenDetailsWindow : public QDialog
{
    Q_OBJECT
public:
    explicit TokenDetailsWindow(const std::string& chatId, QWidget* parent = nullptr);
    ~TokenDetailsWindow();

private:
    void setupUi();
    void loadAndCalculateContext();

    std::string m_chatId;
    QTextEdit* m_textEdit;
};
