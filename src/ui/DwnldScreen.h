#pragma once


#include <QDialog>
#include <QString>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <QLabel>
#include <QProgressBar>
#include "Dto/FileRef.h"


class DwnldScreen : public QDialog {
    Q_OBJECT
    public :
    static void ShowWindow(QWidget* parent = nullptr);
    ~DwnldScreen();

    private slots :
    void StatusUpdate(const std::string& fileName);
    void startNextDownload();
    void onLibraryUpdated();

    private:
    explicit DwnldScreen(QWidget* parent = nullptr);
    static DwnldScreen* d_instance;

    void setupUi();

    std::vector<FileRef> m_queue;
    int m_currentIndex = 0;

    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    
};
