#include "DwnldScreen.h"
#include "memdb/AppState.h"
#include "Helpers/ReqRunner.h"
#include "ui/EventDispatcher.h"
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>

DwnldScreen* DwnldScreen::d_instance = nullptr;

void DwnldScreen::ShowWindow(QWidget * parent) {

    if(d_instance == nullptr){
        d_instance = new DwnldScreen(parent);
    }
    ReqRunner::FetchLibrary();
            QString cachePath = QString::fromStdString((AppState::GetUserDir() / "cache").string());

     QDesktopServices::openUrl(QUrl::fromLocalFile(cachePath));

    d_instance->show();
    d_instance->raise();
    d_instance->activateWindow();
}


DwnldScreen::DwnldScreen(QWidget* parent) : QDialog(parent)
{
    setupUi();
    setAttribute(Qt::WA_DeleteOnClose);

    m_statusLabel->setText("Fetching library items...");
    m_progressBar->setMaximum(0);
    m_progressBar->setValue(0);

    connect(EventDispatcher::instance(), &EventDispatcher::libraryUpdated, this, &DwnldScreen::onLibraryUpdated, Qt::QueuedConnection);
    connect(EventDispatcher::instance(), &EventDispatcher::fileDownloadComplete, this, &DwnldScreen::StatusUpdate, Qt::QueuedConnection);
}

void DwnldScreen::setupUi()
{
    setWindowTitle("Library Sync");
    resize(400, 150);
    setStyleSheet("QDialog { background-color: #313338; }");

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Initializing download...", this);
    m_statusLabel->setStyleSheet("color: #dbdee1; font-size: 14px; font-weight: 500;");
    m_statusLabel->setAlignment(Qt::AlignCenter);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            background-color: #1e1f22;
            border-radius: 6px;
            height: 12px;
            text-align: center;
            color: transparent;
        }
        QProgressBar::chunk {
            background-color: #00c0a3;
            border-radius: 6px;
        }
    )");

    layout->addSpacing(20);
    layout->addWidget(m_statusLabel);
    layout->addSpacing(10);
    layout->addWidget(m_progressBar);
    layout->addSpacing(20);
}

void DwnldScreen::startNextDownload()
{
    if (m_currentIndex < m_queue.size()) {
        const FileRef& file = m_queue[m_currentIndex];
        m_statusLabel->setText(QString("Downloading file %1 of %2:\n%3")
            .arg(m_currentIndex + 1)
            .arg(m_queue.size())
            .arg(QString::fromStdString(file.filename)));
        
        ReqRunner::FileExecution(file.id);
    } else {
        m_statusLabel->setText("Download Complete!");
        
        
        close();
    }
}

void DwnldScreen::StatusUpdate(const std::string& fileName)
{
    m_currentIndex++;
    m_progressBar->setValue(m_currentIndex);
    
    startNextDownload();
}

void DwnldScreen::onLibraryUpdated()
{
    disconnect(EventDispatcher::instance(), &EventDispatcher::libraryUpdated, this, &DwnldScreen::onLibraryUpdated);

    m_queue = AppState::GetLibrary();
    
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(), [](const FileRef& f) { return f.id.empty(); }), m_queue.end());

    m_currentIndex = 0;

    if (m_queue.empty()) {
        m_statusLabel->setText("No files found in library.");
        m_progressBar->setMaximum(1);
        m_progressBar->setValue(1);
        return;
    }

    m_progressBar->setMaximum(m_queue.size());
    m_progressBar->setValue(0);

    startNextDownload();
}

DwnldScreen::~DwnldScreen(){
    if (d_instance == this) {
        d_instance = nullptr;
    }

}