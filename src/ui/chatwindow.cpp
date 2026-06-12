#include "chatwindow.h"
#include "memdb/AppState.h"
#include "Helpers/ReqRunner.h"
#include "ui/EventDispatcher.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QFileDialog>
#include <QKeyEvent>
#include <QFontDatabase>
#include <QPixmap>
#include <QFrame>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMenu>
#include <QPainterPath>
#include <QApplication>
#include <QClipboard>
#include <QProcess>
#include <QComboBox>
#include <chrono>
#include <QPainter>
#include <QPainterPath>
#include "home.h"
#include "Helpers/ReqRunner.h"
#include <QPainter>
#include "TokenDetailsWindow.h"
#include <QMouseEvent>
#include <QtConcurrent/QtConcurrent>
#include <QTextDocument>
#include <QRegularExpression>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <windows.h>

// ---------------------------------------------------------------------------
// Helper: fast hash for markdown detection
// ---------------------------------------------------------------------------
static bool HasMarkdownIndicators(const std::string& s)
{
    for (char c : s) {
        if (c == '*' || c == '#' || c == '`' || c == '[' || c == '!' || c == '|' || c == '>' || c == '-' || c == '_') return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helper: strip ChatGPT citation markers like 📎filecite🔖turn22file0🔖3️⃣
// Also strips 【...】 style citations
// ---------------------------------------------------------------------------
static QString StripCitations(const QString& text)
{
    QString result = text;
    // Strip emoji-style citations: 📎filecite🔖...🔖...(digits/emoji)
    // 📎 = U+1F4CE, 🔖 = U+1F516
    static QRegularExpression emojiCite(
        QString(u"\U0001F4CEfilecite\U0001F516[^\U0001F516]*\U0001F516[^\\s]*"));
    result.replace(emojiCite, "");
    // Strip 【...】 style citations (U+3010, U+3011)
    static QRegularExpression bracketCite(
        QString(u"\u3010[^\u3011]*\u3011"));
    result.replace(bracketCite, "");
    return result.trimmed();
}

// ---------------------------------------------------------------------------
// ChatModel implementation
// ---------------------------------------------------------------------------
ChatModel::ChatModel(QObject* parent)
    : QAbstractListModel(parent), m_visibleLimit(50)
{}

void ChatModel::setMessages(const std::vector<ChatMessage>& messages, size_t visibleLimit)
{
    int oldTotal = static_cast<int>(m_messages.size());
    int newTotal = static_cast<int>(messages.size());
    int oldVisible = static_cast<int>(std::min(m_messages.size(), m_visibleLimit));
    int newVisible = static_cast<int>(std::min(messages.size(), visibleLimit));
    int oldStart = oldTotal - oldVisible;
    int newStart = newTotal - newVisible;

    if (oldVisible == 0 || oldStart != newStart)
    {
        beginResetModel();
        m_messages = messages;
        m_visibleLimit = visibleLimit;
        endResetModel();
    }
    else if (newVisible > oldVisible)
    {
        beginInsertRows(QModelIndex(), oldVisible, newVisible - 1);
        m_messages = messages;
        m_visibleLimit = visibleLimit;
        endInsertRows();
    }
    else if (newVisible == oldVisible && newVisible > 0)
    {
        m_messages = messages;
        m_visibleLimit = visibleLimit;
        QModelIndex lastIdx = index(newVisible - 1);
        emit dataChanged(lastIdx, lastIdx);
    }
    else
    {
        beginResetModel();
        m_messages = messages;
        m_visibleLimit = visibleLimit;
        endResetModel();
    }
}

int ChatModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(std::min(m_messages.size(), m_visibleLimit));
}

QVariant ChatModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return QVariant();

    // Map row index to the end of m_messages (sliding window)
    size_t totalCount = m_messages.size();
    size_t visibleCount = std::min(totalCount, m_visibleLimit);
    size_t startIndex = totalCount - visibleCount;
    size_t msgIndex = startIndex + index.row();

    if (role == ChatRoles::MessageDataRole)
    {
        return QVariant::fromValue(m_messages[msgIndex]);
    }
    return QVariant();
}

// ---------------------------------------------------------------------------
// MessageDelegate implementation
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// MessageLayoutCache implementation
// ---------------------------------------------------------------------------
void MessageLayoutCache::invalidateRow(const std::string& messageId)
{
    if (m_layouts.contains(messageId)) {
        m_layouts[messageId].layout.dirty = true;
    }
}

void MessageLayoutCache::invalidateAll()
{
    m_layouts.clear();
}

CachedLayout& MessageLayoutCache::getLayout(const std::string& messageId)
{
    return m_layouts[messageId];
}

void MessageLayoutCache::ensureFileCached(const QString& absolutePath, const std::string& messageId)
{
    if (m_fileStatus.contains(absolutePath) || m_pendingFiles.contains(absolutePath)) {
        return;
    }

    m_pendingFiles.insert(absolutePath);

    // Capture copies of everything we need — no raw 'this' on worker thread
    QtConcurrent::run([this, absolutePath, messageId]() {
        bool exists = std::filesystem::exists(absolutePath.toUtf8().constData());
        QImage loadedImage;  // QImage is safe on worker threads; QPixmap is NOT

        if (exists) {
            bool is_image = absolutePath.endsWith(".png", Qt::CaseInsensitive) ||
                absolutePath.endsWith(".jpg", Qt::CaseInsensitive) ||
                absolutePath.endsWith(".jpeg", Qt::CaseInsensitive) ||
                absolutePath.endsWith(".webp", Qt::CaseInsensitive) ||
                absolutePath.endsWith(".gif", Qt::CaseInsensitive);

            if (is_image) {
                if (loadedImage.load(absolutePath)) {
                    if (!loadedImage.isNull() && loadedImage.width() > 0 && loadedImage.height() > 0) {
                        loadedImage = loadedImage.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                }
            }
        }

        // Convert QImage -> QPixmap on the main/GUI thread only
        QMetaObject::invokeMethod(qApp, [this, absolutePath, messageId, exists, loadedImage]() {
            m_pendingFiles.remove(absolutePath);
            m_fileStatus[absolutePath] = exists;

            if (!loadedImage.isNull()) {
                m_thumbnails[absolutePath] = QPixmap::fromImage(loadedImage);
            }

            invalidateRow(messageId);
            if (onLayoutInvalidated) {
                onLayoutInvalidated(messageId);
            }
            }, Qt::QueuedConnection);
        });
}

bool MessageLayoutCache::isFileCached(const QString& absolutePath)
{
    return m_fileStatus.value(absolutePath, false);
}

QPixmap MessageLayoutCache::getThumbnail(const QString& absolutePath)
{
    return m_thumbnails.value(absolutePath, QPixmap());
}

// ---------------------------------------------------------------------------
// MessageLayoutBuilder implementation
// ---------------------------------------------------------------------------
void MessageLayoutBuilder::build(MessageLayout& layout, const ChatMessage& msg, int availableWidth, MessageLayoutCache* cache)
{
    layout.blocks.clear();
    layout.clickRegions.clear();
    layout.layoutWidth = availableWidth;
    layout.bubbleWidth = 0;

    int currentY = 0;
    int maxBlockWidth = 0;
    int maxBubbleArea = std::max(200, availableWidth - 40);  // 20px margin each side
    int maxWidth = std::min(maxBubbleArea, 980) - 28;

    // Process assets
    bool hasImages = false;
    int imgX = 0;
    int maxImgHeight = 0;

    for (const auto& asset : msg.assets) {
        std::filesystem::path cache_path = AppState::GetUserDir() / "cache" / asset.filename;
        QString pathStr = QString::fromStdString(cache_path.string()).replace("\\", "/");

        cache->ensureFileCached(pathStr, msg.message_id);
        bool cached = cache->isFileCached(pathStr);

        bool is_image = asset.mime_type.find("image") != std::string::npos ||
            asset.filename.find(".png") != std::string::npos ||
            asset.filename.find(".jpg") != std::string::npos ||
            asset.filename.find(".jpeg") != std::string::npos ||
            asset.filename.find(".webp") != std::string::npos ||
            asset.filename.find(".gif") != std::string::npos;

        if (is_image && cached) {
            hasImages = true;
            QPixmap thumb = cache->getThumbnail(pathStr);
            int tw = thumb.isNull() ? 160 : thumb.width();
            int th = thumb.isNull() ? 120 : thumb.height();

            LayoutBlock b;
            b.type = BlockType::Image;
            b.thumbnail = thumb;
            b.rect = QRect(imgX, currentY, tw, th);
            layout.blocks.push_back(b);

            imgX += tw + 8;
            maxImgHeight = std::max(maxImgHeight, th);
        }
        else {
            LayoutBlock b;
            b.type = BlockType::File;
            b.action = QString("action:file:%1:%2").arg(QString::fromStdString(asset.id), pathStr);
            b.cached = cached;
            QString labelText = cached ? QString::fromStdString(asset.filename)
                : QString("%1 (%2 KB) - Download").arg(QString::fromStdString(asset.filename), QString::number(asset.size_bytes / 1024.0, 'f', 1));

            b.text = QStaticText(labelText);
            b.text.setTextWidth(maxWidth - 40);
            int fileTextWidth = std::min((int)b.text.size().width() + 44, maxWidth);
            b.rect = QRect(0, currentY, fileTextWidth, 36);
            layout.blocks.push_back(b);

            maxBlockWidth = std::max(maxBlockWidth, fileTextWidth);

            layout.clickRegions.append({ b.rect, b.action });

            currentY += 42;
        }
    }

    if (hasImages) {
        currentY += maxImgHeight + 8;
        maxBlockWidth = std::max(maxBlockWidth, imgX);
    }

    if (!msg.thinking.empty()) {
        LayoutBlock b;
        b.type = BlockType::Thinking;
        b.text = QStaticText(QString::fromStdString(msg.thinking));

        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        b.text.setTextOption(textOption);
        b.text.setTextFormat(Qt::PlainText);

        b.text.setTextWidth(maxWidth - 20); // account for padding

        int h = b.text.size().height() + 20; // 10px padding top/bottom
        b.rect = QRect(0, currentY, maxWidth, h);
        layout.blocks.push_back(b);
        currentY += h + 8;
        maxBlockWidth = std::max(maxBlockWidth, (int)b.text.size().width() + 20);
    }

    // Process Blocks
    for (const auto& block : msg.blocks) {
        LayoutBlock b;
        b.type = block.type;

        if (block.type == BlockType::Separator) {
            b.rect = QRect(0, currentY + 4, maxWidth, 1);
            layout.blocks.push_back(b);
            currentY += 9;
            continue;
        }

        if (block.type == BlockType::Code) {
            QString codeHtml = QString("<pre style=\"font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; color: #dcdcdc; margin: 0;\">%1</pre>")
                .arg(QString::fromStdString(block.content).toHtmlEscaped());
            b.text = QStaticText(codeHtml);

            QTextOption textOption;
            textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            b.text.setTextOption(textOption);
            b.text.setTextFormat(Qt::RichText);

            b.text.setTextWidth(maxWidth - 20);
            int h = b.text.size().height() + 38; // 26px header + 12px padding
            b.rect = QRect(0, currentY, maxWidth, h);

            int blockIdx = &block - &msg.blocks[0];
            b.action = QString("action:copycode:%1").arg(blockIdx);
            layout.clickRegions.append({ QRect(maxWidth - 60, currentY + 4, 50, 18), b.action });

            maxBlockWidth = std::max(maxBlockWidth, (int)b.text.size().width() + 20);
            currentY += h + 8;
        }
        else {
            // Fast path: plain text if no markdown indicators
            if (!HasMarkdownIndicators(block.content)) {
                b.text = QStaticText(StripCitations(QString::fromStdString(block.content)));

                QTextOption textOption;
                textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
                b.text.setTextOption(textOption);
                b.text.setTextFormat(Qt::PlainText);

                b.text.setTextWidth(maxWidth);
                int h = b.text.size().height();
                b.rect = QRect(0, currentY, maxWidth, h);
                maxBlockWidth = std::max(maxBlockWidth, std::min((int)b.text.size().width(), maxWidth));
                currentY += h + 8;
            }
            else {
                // Markdown path: escape raw HTML entities first to prevent rendering glitches
                QString rawContent = StripCitations(QString::fromStdString(block.content));
                rawContent.replace("&", "&amp;");
                rawContent.replace("<", "&lt;");
                rawContent.replace(">", "&gt;");
                // Undo escaping for markdown syntax that uses > (blockquotes)
                rawContent.replace(QRegularExpression("^&gt;", QRegularExpression::MultilineOption), ">");

                QTextDocument tempDoc;
                tempDoc.setDefaultStyleSheet("p, li { color: #ececed; font-family: 'Segoe UI'; font-size: 14px; margin-top: 4px; margin-bottom: 4px; } "
                    "h1, h2, h3, h4, h5, h6 { color: #00c0a3; font-family: 'Segoe UI'; margin-top: 10px; margin-bottom: 6px; } "
                    "a { color: #00c0a3; text-decoration: underline; } "
                    "strong { color: #ffffff; font-weight: bold; }");
                tempDoc.setMarkdown(rawContent);

                b.text = QStaticText(tempDoc.toHtml());

                QTextOption textOption;
                textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
                b.text.setTextOption(textOption);
                b.text.setTextFormat(Qt::RichText);

                b.text.setTextWidth(maxWidth);
                int h = b.text.size().height();
                b.rect = QRect(0, currentY, maxWidth, h);
                // Clamp reported width to maxWidth — QStaticText RichText can over-report
                maxBlockWidth = std::max(maxBlockWidth, std::min((int)b.text.size().width(), maxWidth));
                currentY += h + 8;
            }
        }
        layout.blocks.push_back(b);
    }

    // Clamp bubble so it never exceeds the viewport
    layout.bubbleWidth = std::min(maxBlockWidth + 28, maxBubbleArea);
    layout.bubbleHeight = currentY + 28;
    layout.totalSize = QSize(availableWidth, layout.bubbleHeight + 32);
    layout.dirty = false;
}

// ---------------------------------------------------------------------------
// MessageDelegate implementation
// ---------------------------------------------------------------------------
MessageDelegate::MessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

uint64_t MessageDelegate::ComputeMessageHash(const ChatMessage& msg)
{
    uint64_t h = std::hash<bool>{}(msg.user);
    h ^= std::hash<size_t>{}(msg.blocks.size()) * 0x9e3779b97f4a7c15ULL;
    h ^= std::hash<size_t>{}(msg.assets.size()) * 0x6c62272e07bb0142ULL;
    h ^= std::hash<std::string>{}(msg.thinking) * 0xbf58476d1ce4e5b9ULL;
    if (!msg.blocks.empty())
        h ^= std::hash<std::string>{}(msg.blocks.back().content) * 0x94d049bb133111ebULL;
    if (!msg.assets.empty())
        h ^= std::hash<std::string>{}(msg.assets.back().id) * 0xff51afd7ed558ccdULL;
    return h;
}

void MessageDelegate::invalidateAll() const { m_cache.invalidateAll(); }
void MessageDelegate::invalidateRow(const std::string& messageId) const { m_cache.invalidateRow(messageId); }

void MessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QVariant dataVar = index.data(ChatRoles::MessageDataRole);
    if (!dataVar.canConvert<ChatMessage>()) {
        painter->restore();
        return;
    }
    ChatMessage msg = dataVar.value<ChatMessage>();
    CachedLayout& cached = m_cache.getLayout(msg.message_id);

    if (cached.layout.dirty) {
        painter->restore();
        return;
    }

    const MessageLayout& layout = cached.layout;

    painter->save();

    // Draw bubble background
    QRect bubbleRect = QRect(option.rect.x() + (msg.user ? option.rect.width() - layout.bubbleWidth - 10 : 10),
        option.rect.y() + 5,
        layout.bubbleWidth, layout.bubbleHeight);

    QPainterPath path;
    path.addRoundedRect(bubbleRect, 12, 12);
    QColor bgColor = msg.user ? QColor("#123e34") : QColor("#16171a");
    QColor borderColor = msg.user ? QColor("#1a5649") : QColor("#26272b");

    painter->fillPath(path, bgColor);
    painter->setPen(QPen(borderColor, 1));
    painter->drawPath(path);

    QPoint origin = bubbleRect.topLeft();

    // Draw header
    if (!msg.user) {
        painter->setPen(QColor("#00c0a3"));
        painter->setFont(QFont("Segoe UI", 8, QFont::Bold));
        painter->drawText(origin.x(), origin.y() - 4, "GPT");
    }

    // Draw blocks
    painter->translate(origin.x() + 14, origin.y() + 14);

    for (auto& block : layout.blocks) {
        switch (block.type) {
        case BlockType::Text:
        case BlockType::Math:
            painter->setPen(QColor("#ececed"));
            painter->drawStaticText(block.rect.topLeft(), block.text);
            break;

        case BlockType::Thinking:
            painter->setBrush(QColor("#090a0c"));
            painter->setPen(QColor("#555861"));
            painter->drawRoundedRect(block.rect, 4, 4);
            painter->setPen(QColor("#a0a1a5"));
            painter->drawStaticText(block.rect.topLeft() + QPoint(10, 10), block.text);
            break;

        case BlockType::Code:
        {
            // Background
            painter->setBrush(QColor("#1e1e1e"));
            painter->setPen(QColor("#333333"));
            painter->drawRoundedRect(block.rect, 6, 6);

            // Header
            QPainterPath headerPath;
            headerPath.addRoundedRect(QRectF(block.rect.x(), block.rect.y(), block.rect.width(), 26), 6, 6);
            headerPath.addRect(block.rect.x(), block.rect.y() + 10, block.rect.width(), 16);
            painter->fillPath(headerPath.simplified(), QColor("#2d2d2d"));

            // Copy text
            painter->setPen(QColor("#aaaaaa"));
            painter->setFont(QFont("Segoe UI", 9));
            painter->drawText(QRect(block.rect.right() - 60, block.rect.y() + 4, 50, 18), Qt::AlignRight | Qt::AlignVCenter, "Copy");

            // Code Text
            painter->drawStaticText(block.rect.topLeft() + QPoint(10, 32), block.text);
            break;
        }

        case BlockType::Image:
            if (!block.thumbnail.isNull()) {
                painter->drawPixmap(block.rect.topLeft(), block.thumbnail);
            }
            else {
                painter->setBrush(QColor("#2a2b30"));
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(block.rect, 4, 4);

                painter->setPen(QColor("#555"));
                painter->setFont(QFont("Segoe UI", 10));
                painter->drawText(block.rect, Qt::AlignCenter, "Loading...");
            }
            break;

        case BlockType::File:
        {
            painter->setBrush(QColor("#2a2b30"));
            painter->setPen(block.cached ? QColor("#00c0a3") : QColor("#555555"));
            painter->drawRoundedRect(block.rect, 6, 6);

            painter->setPen(block.cached ? QColor("#ececed") : QColor("#888888"));
            painter->drawStaticText(block.rect.topLeft() + QPoint(36, 10), block.text);

            // Draw a generic file icon representation
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(block.cached ? QColor("#00c0a3") : QColor("#555555"), 1.5));
            QRect iconRect(block.rect.x() + 12, block.rect.y() + 8, 14, 18);
            painter->drawRect(iconRect);
            painter->drawLine(iconRect.x(), iconRect.y() + 4, iconRect.right(), iconRect.y() + 4);
            break;
        }

        case BlockType::Separator:
            painter->setPen(QColor("#282a30"));
            painter->drawLine(block.rect.left(), block.rect.center().y(),
                block.rect.right(), block.rect.center().y());
            break;
        }
    }

    painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QVariant dataVar = index.data(ChatRoles::MessageDataRole);
    if (!dataVar.canConvert<ChatMessage>()) {
        return QSize();
    }
    ChatMessage msg = dataVar.value<ChatMessage>();

    int width = option.rect.width();
    if (width <= 0) width = 800; // fallback

    uint64_t currentHash = ComputeMessageHash(msg);
    CachedLayout& cached = m_cache.getLayout(msg.message_id);

    if (cached.layout.dirty || cached.contentHash != currentHash || cached.availableWidth != width)
    {
        MessageLayoutBuilder::build(cached.layout, msg, width, &m_cache);
        cached.contentHash = currentHash;
        cached.availableWidth = width;
        cached.layout.dirty = false;
    }

    return cached.layout.totalSize;
}

bool MessageDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
    const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QVariant dataVar = index.data(ChatRoles::MessageDataRole);
        if (dataVar.canConvert<ChatMessage>())
        {
            ChatMessage msg = dataVar.value<ChatMessage>();
            CachedLayout& cached = m_cache.getLayout(msg.message_id);

            int startX = option.rect.x() + (msg.user ? option.rect.width() - cached.layout.bubbleWidth - 10 : 10) + 14;
            int startY = option.rect.y() + 5 + 14;

            for (const auto& region : cached.layout.clickRegions) {
                QRect absoluteRect(region.rect.x() + startX, region.rect.y() + startY, region.rect.width(), region.rect.height());
                if (absoluteRect.contains(me->pos())) {
                    if (region.action.startsWith("action:copycode:")) {
                        int blockIdx = region.action.mid(16).toInt();
                        if (blockIdx >= 0 && blockIdx < msg.blocks.size()) {
                            QGuiApplication::clipboard()->setText(QString::fromStdString(msg.blocks[blockIdx].content));
                        }
                    }
                    else {
                        handleAction(region.action);
                    }
                    return true;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void MessageDelegate::handleAction(const QString& action) const
{
    if (action.startsWith("action:file:"))
    {
        QStringList parts = action.split(":");
        if (parts.size() >= 4) {
            std::string assetId = parts[2].toUtf8().constData();
            QString path = parts.mid(3).join(":"); // in case path has C:/ (which contains :)

            if (std::filesystem::exists(path.toUtf8().constData())) {
                ShellExecuteA(0, 0, path.toUtf8().constData(), 0, 0, SW_SHOW);
            }
            else {
                ReqRunner::FileExecution(assetId);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ChatWindow implementation
// ---------------------------------------------------------------------------
ChatWindow::ChatWindow(const std::string& chatId, QWidget* parent)
    : QWidget(parent, Qt::Window), m_chatId(chatId)
{
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(150); // Reduced from 33ms to 150ms — chat doesn't need 30 FPS
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        std::vector<ChatMessage> messages = AppState::GetChatsFromMap(m_chatId);
        if (messages.empty()) return;
        std::vector<ChatMessage> filteredMessages;
        filteredMessages.reserve(messages.size());
        for (const auto& msg : messages) {
            if (!msg.is_system_or_tool) {
                filteredMessages.push_back(msg);
            }
        }

        bool wasEmpty = chatModel->rowCount() == 0;

        QScrollBar* bar = listView->verticalScrollBar();
        bool isAtBottom = bar->value() >= bar->maximum() - 10;

        chatModel->setMessages(filteredMessages, m_visibleLimit);

        // If we just loaded messages for the first time, switch away from loading screen
        if (wasEmpty && chatModel->rowCount() > 0) {
            m_stackedWidget->setCurrentIndex(0); // Chat View
            updateStatus();  // Refresh header with real status
        }

        if (wasEmpty || isAtBottom) {
            // Defer scroll — layout hasn't updated yet after model reset
            QMetaObject::invokeMethod(this, [this]() {
                QScrollBar* sb = listView->verticalScrollBar();
                sb->setValue(sb->maximum());
                }, Qt::QueuedConnection);
        }
        });

    setupUi();

    // Event filter on QTextEdit for Enter key sending
    inputEdit->installEventFilter(this);

    // Connect vertical scrollbar for infinite scrolling virtualization
    connect(listView->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatWindow::onScrollValueChanged);

    // Event driven updates from backend
    connect(EventDispatcher::instance(), &EventDispatcher::assetsUpdated, this, [this](const std::string& chatId, const std::string& assetId) {
        if (m_chatId == chatId) {
            updateAttachmentsUI();
        }
        });

    connect(EventDispatcher::instance(), &EventDispatcher::chatMessageUpdated, this, [this](const std::string& id) {
        if (id == m_chatId) updateMessages();
        });
    connect(EventDispatcher::instance(), &EventDispatcher::chatListUpdated, this, &ChatWindow::updateStatus);

    updateStatus();
    connect(EventDispatcher::instance(), &EventDispatcher::assetsUpdated, this, [this](const std::string& id, const std::string&) {
        if (id == m_chatId) updateMessages();
        });
    connect(EventDispatcher::instance(), &EventDispatcher::chatIdSwapped, this, [this](const std::string& oldId, const std::string& newId) {
        if (m_chatId == oldId) {
            m_chatId = newId;
            updateMessages();
            updateStatus();
        }
        });

    // Initial check
    updateMessages();
}

ChatWindow::~ChatWindow()
{}

void ChatWindow::closeEvent(QCloseEvent* event)
{
    AppState::CloseChat(m_chatId);
    QWidget::closeEvent(event);
}

bool ChatWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == inputEdit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier))
            {
                onSendClicked();
                return true;
            }
        }
    }
    else if (obj == m_statusLabel && event->type() == QEvent::MouseButtonPress) {
        onTokenClicked();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void ChatWindow::setupUi()
{
    std::string titleStr = m_chatId.substr(0, 12);
    auto chats = AppState::GetChatListCopy();
    for (const auto& chat : chats) {
        if (chat.id == m_chatId) {
            if (!chat.title.empty()) {
                titleStr = chat.title;
            }
            break;
        }
    }
    setWindowTitle(QString("Chat - %1").arg(QString::fromStdString(titleStr)));
    resize(1050, 800);
    setStyleSheet("background-color: #0c0d0f;");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Header Area
    QWidget* header = new QWidget(this);
    header->setFixedHeight(50);
    header->setStyleSheet("background-color: #08090a; border-bottom: 1px solid #1a1b1e;");

    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    QWidget* dot = new QWidget(header);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet("background-color: #00c0a3; border-radius: 4px;");
    headerLayout->addWidget(dot);

    QLabel* titleLabel = new QLabel(QString::fromStdString(titleStr), header);
    titleLabel->setStyleSheet("color: #ececed; font-weight: bold; font-size: 14px; background: transparent;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_statusLabel = new QLabel("", header);
    m_statusLabel->setStyleSheet("color: #00c0a3; font-size: 12px; background: transparent;");
    m_statusLabel->setCursor(Qt::PointingHandCursor);
    m_statusLabel->installEventFilter(this);
    headerLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(header);

    // 2. Virtualized List View Area
    m_stackedWidget = new QStackedWidget(this);

    listView = new QListView(m_stackedWidget);
    listView->setResizeMode(QListView::Adjust);
    listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setSelectionMode(QAbstractItemView::NoSelection);
    listView->setFocusPolicy(Qt::NoFocus);
    listView->setStyleSheet("background-color: transparent; border: none; outline: none;");

    chatModel = new ChatModel(this);
    messageDelegate = new MessageDelegate(this);

    messageDelegate->setOnLayoutInvalidated([this](const std::string&) {
        listView->viewport()->update();
        });

    listView->setModel(chatModel);
    listView->setItemDelegate(messageDelegate);
    m_stackedWidget->addWidget(listView);

    // Loading View (Page 1)
    m_loadingWidget = new QWidget(m_stackedWidget);
    QVBoxLayout* loadingLayout = new QVBoxLayout(m_loadingWidget);
    loadingLayout->setAlignment(Qt::AlignCenter);
    loadingLayout->setSpacing(16);

    LoadingSpinner* spinner = new LoadingSpinner(m_loadingWidget);
    m_loadingLabel = new QLabel("Fetching conversation...", m_loadingWidget);
    m_loadingLabel->setStyleSheet("color: #7d8087; font-size: 14px; font-weight: 500;");

    loadingLayout->addWidget(spinner, 0, Qt::AlignCenter);
    loadingLayout->addWidget(m_loadingLabel, 0, Qt::AlignCenter);
    m_stackedWidget->addWidget(m_loadingWidget);

    // Error View (Page 2)
    m_errorWidget = new QWidget(m_stackedWidget);
    QVBoxLayout* errorLayout = new QVBoxLayout(m_errorWidget);
    errorLayout->setAlignment(Qt::AlignCenter);
    errorLayout->setSpacing(16);

    m_errorLabel = new QLabel("An error occurred.", m_errorWidget);
    m_errorLabel->setStyleSheet("color: #d9534f; font-size: 14px; font-weight: 500; text-align: center;");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignCenter);

    QPushButton* retryBtn = new QPushButton("Retry", m_errorWidget);
    retryBtn->setFixedSize(100, 36);
    retryBtn->setCursor(Qt::PointingHandCursor);
    retryBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #10a37f; color: white; border-radius: 4px; font-size: 14px; font-weight: 500;
        }
        QPushButton:hover { background-color: #1a7f64; }
    )");
    connect(retryBtn, &QPushButton::clicked, this, &ChatWindow::onRetryClicked);

    errorLayout->addWidget(m_errorLabel, 0, Qt::AlignCenter);
    errorLayout->addWidget(retryBtn, 0, Qt::AlignCenter);
    m_stackedWidget->addWidget(m_errorWidget);

    mainLayout->addWidget(m_stackedWidget, 1);

    // Divider Line
    QFrame* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    divider->setStyleSheet("background-color: #1a1b1e; min-height: 1px; max-height: 1px; border: none;");
    mainLayout->addWidget(divider);

    // 3. Bottom Input Bar
    QWidget* inputArea = new QWidget(this);
    inputArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    inputArea->setStyleSheet("background-color: #08090a;");

    QVBoxLayout* inputAreaLayout = new QVBoxLayout(inputArea);
    inputAreaLayout->setContentsMargins(16, 10, 16, 10);
    inputAreaLayout->setSpacing(6);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    QPushButton* attachBtn = new QPushButton("ATTACH", inputArea);
    attachBtn->setCursor(Qt::PointingHandCursor);
    connect(attachBtn, &QPushButton::clicked, this, &ChatWindow::onAttachClicked);
    btnRow->addWidget(attachBtn);

    m_modeDropdown = new QComboBox(inputArea);
    m_modeDropdown->setStyleSheet("QComboBox { background-color: #1a1b1e; color: #ececed; border: 1px solid #282c35; border-radius: 4px; padding: 4px; }");

    auto formatLimit = [](int64_t limit, int64_t reset_epoch) {
        if (limit < 0) return QString("");
        int64_t current = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t diff = reset_epoch - current;
        if (diff <= 0) return QString(" (%1 left)").arg(limit);

        QString timeStr;
        if (diff >= 3600) timeStr = QString("%1h").arg(diff / 3600);
        else if (diff >= 60) timeStr = QString("%1m").arg(diff / 60);
        else timeStr = QString("%1s").arg(diff);

        return QString(" (%1 left, renews in %2)").arg(limit).arg(timeStr);
        };

    User u = AppState::Get_User();

    m_modeDropdown->addItem("Normal", static_cast<int>(Modes::normal));
    m_modeDropdown->addItem("Create Image" + formatLimit(u.image_gen_limit, u.image_gen_reset), static_cast<int>(Modes::create_image));
    m_modeDropdown->addItem("Search", static_cast<int>(Modes::search));
    m_modeDropdown->addItem("Deep Research" + formatLimit(u.deep_research_limit, u.deep_reset), static_cast<int>(Modes::deep_research));
    m_modeDropdown->addItem("Thinking", static_cast<int>(Modes::reason));

    Modes currentMode = AppState::Get_Input_Box(m_chatId).mode;
    int modeIdx = m_modeDropdown->findData(static_cast<int>(currentMode));
    if (modeIdx >= 0) m_modeDropdown->setCurrentIndex(modeIdx);

    connect(m_modeDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Modes mode = static_cast<Modes>(m_modeDropdown->itemData(index).toInt());
        AppState::Update_Mode(m_chatId, mode);
        });

    btnRow->addWidget(m_modeDropdown);

    m_modelSelector = new QComboBox(inputArea);
    m_modelSelector->setStyleSheet("QComboBox { background-color: #1a1b1e; color: #ececed; border: 1px solid #282c35; border-radius: 4px; padding: 4px; }");

    std::set<Models> models = AppState::GetModels();
    for (const auto& model : models) {
        if (!model.slugs.empty()) {
            m_modelSelector->addItem(QString::fromStdString(model.name), QString::fromStdString(model.slugs.front()));
        }
    }

    std::string currentModel = AppState::Get_Input_Box(m_chatId).model;
    if (currentModel.empty()) currentModel = AppState::Get_Default_Model();
    int idx = m_modelSelector->findData(QString::fromStdString(currentModel));
    if (idx >= 0) m_modelSelector->setCurrentIndex(idx);

    connect(m_modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        onModelSelected(m_modelSelector->itemData(index).toString());
        });

    btnRow->addWidget(m_modelSelector);

    btnRow->addStretch();
    inputAreaLayout->addLayout(btnRow);

    m_attachmentsContainer = new QWidget(inputArea);
    m_attachmentsLayout = new QHBoxLayout(m_attachmentsContainer);
    m_attachmentsLayout->setContentsMargins(0, 0, 0, 0);
    m_attachmentsLayout->setSpacing(8);
    m_attachmentsContainer->hide();
    inputAreaLayout->addWidget(m_attachmentsContainer);

    QHBoxLayout* inputRow = new QHBoxLayout();
    inputRow->setSpacing(10);

    inputEdit = new QTextEdit(inputArea);
    inputEdit->setPlaceholderText("Type your message...");
    inputEdit->setFixedHeight(54);

    connect(inputEdit->document(), &QTextDocument::contentsChanged, this, [this]() {
        int newHeight = static_cast<int>(inputEdit->document()->size().height()) + 12; // padding
        if (newHeight < 54) newHeight = 54;
        if (newHeight > 300) newHeight = 300;

        if (inputEdit->height() != newHeight) {
            inputEdit->setFixedHeight(newHeight);
        }
        });

    inputRow->addWidget(inputEdit, 1);

    m_sendBtn = new QPushButton(">>", inputArea);
    m_sendBtn->setObjectName("sendButton");
    m_sendBtn->setFixedSize(54, 54);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet("QPushButton { color: #ffffff; background-color: #2a2e38; border-radius: 8px; font-weight: bold; } QPushButton:hover { background-color: #3b404d; } QPushButton:disabled { color: #666666; background-color: #1a1b1e; }");
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);

    connect(inputEdit, &QTextEdit::textChanged, this, &ChatWindow::updateSendButtonState);

    inputRow->addWidget(m_sendBtn);

    inputAreaLayout->addLayout(inputRow);
    mainLayout->addWidget(inputArea);

    updateSendButtonState();
}

void ChatWindow::onSendClicked()
{
    QString text = inputEdit->toPlainText().trimmed();
    if (!text.isEmpty())
    {
        ReqRunner::Send_Message(m_chatId, text.toUtf8().constData());
        inputEdit->clear();
    }
}

void ChatWindow::onAttachClicked()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Select File to Attach",
        "",
        "All Files (*.*);;Images (*.png *.jpg *.jpeg *.webp);;Documents (*.pdf *.txt *.json)"
    );

    if (!filename.isEmpty())
    {
        FileRef fileref;
        fileref.chat_id = m_chatId;
        fileref.source_path = filename.toUtf8().constData();
        AppState::Add_Job(fileref);
    }
}

void ChatWindow::onRemoveAssetClicked(const std::string& assetId)
{
    FileRef dummy;
    AppState::Remove_File_Asset(dummy, m_chatId, assetId);
}

void ChatWindow::updateAttachmentsUI()
{
    if (!m_attachmentsLayout) return;

    // Clear existing pills
    QLayoutItem* item;
    while ((item = m_attachmentsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    auto assets = AppState::Get_Input_Box(m_chatId).assets;
    if (assets.empty()) {
        m_attachmentsContainer->hide();
        return;
    }

    m_attachmentsContainer->show();

    for (const auto& pair : assets) {
        const std::string& local_id = pair.first;
        const FileRef& ref = pair.second;

        QWidget* pill = new QWidget(m_attachmentsContainer);
        pill->setStyleSheet("background-color: #282c35; border-radius: 4px; padding: 4px;");
        QHBoxLayout* pillLayout = new QHBoxLayout(pill);
        pillLayout->setContentsMargins(4, 2, 4, 2);
        pillLayout->setSpacing(4);

        QString statusText = "";
        if (ref.uploadstatus == UploadStatus::Uploading) statusText = " (Uploading...)";
        else if (ref.uploadstatus == UploadStatus::Failed) statusText = " (Failed)";

        QLabel* nameLabel = new QLabel(QString::fromStdString(ref.filename) + statusText, pill);
        nameLabel->setStyleSheet("color: #ececed;");
        pillLayout->addWidget(nameLabel);

        QPushButton* removeBtn = new QPushButton("x", pill);
        removeBtn->setFixedSize(16, 16);
        removeBtn->setStyleSheet("QPushButton { color: #f04444; border: none; background: transparent; font-weight: bold; } QPushButton:hover { color: #ff6666; }");
        removeBtn->setCursor(Qt::PointingHandCursor);

        std::string assetIdCpy = local_id;
        connect(removeBtn, &QPushButton::clicked, this, [this, assetIdCpy]() {
            onRemoveAssetClicked(assetIdCpy);
            });
        pillLayout->addWidget(removeBtn);

        m_attachmentsLayout->addWidget(pill);
    }
    m_attachmentsLayout->addStretch();

    updateSendButtonState();
}

void ChatWindow::updateSendButtonState()
{
    if (!m_sendBtn) return;

    bool isStreaming = false;
    Status status = Status::NotOpened;
    std::string errorText;
    if (AppState::GetChatStatus(m_chatId, status, errorText)) {
        if (status == Status::ReqSent || status == Status::Parsing) {
            isStreaming = true;
        }
    }

    bool isUploading = false;
    bool hasAssets = false;
    auto assets = AppState::Get_Input_Box(m_chatId).assets;
    for (const auto& pair : assets) {
        hasAssets = true;
        if (pair.second.uploadstatus == UploadStatus::Uploading) {
            isUploading = true;
            break;
        }
    }

    bool isEmpty = inputEdit->toPlainText().trimmed().isEmpty() && !hasAssets;

    if (isStreaming || isUploading || isEmpty) {
        m_sendBtn->setDisabled(true);
    }
    else {
        m_sendBtn->setDisabled(false);
    }
}

void ChatWindow::onModelSelected(const QString& model)
{
    AppState::Update_Model(model.toUtf8().constData(), m_chatId);
    updateStatus();
}

void ChatWindow::onTokenClicked()
{
    TokenDetailsWindow* dlg = new TokenDetailsWindow(m_chatId, this);
    dlg->exec();
}

void ChatWindow::updateMessages()
{
    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
    }
}

void ChatWindow::onScrollValueChanged(int value)
{
    if (value == listView->verticalScrollBar()->minimum())
    {
        std::vector<ChatMessage> messages = AppState::GetChatsFromMap(m_chatId);
        if (!messages.empty() && m_visibleLimit < messages.size())
        {
            size_t oldLimit = m_visibleLimit;
            int insertCount = static_cast<int>(std::min<size_t>(50, messages.size() - oldLimit));

            // We need the current viewport width so sizeHint can correctly measure and cache layout
            QStyleOptionViewItem option;
            option.rect.setWidth(listView->viewport()->width());

            int insertedHeight = 0;
            // The items will be inserted at indices [0, insertCount - 1] after the model update,
            // but we can compute their heights by looking at the raw messages array since they aren't in the model yet!
            // Wait, sizeHint requires a valid QModelIndex. If we update the model first, then compute height...

            m_visibleLimit = std::min(m_visibleLimit + 50, messages.size());
            chatModel->setMessages(messages, m_visibleLimit);

            // Now the items are in the model at indices 0 to insertCount-1
            for (int i = 0; i < insertCount; ++i) {
                insertedHeight += messageDelegate->sizeHint(option, chatModel->index(i)).height();
            }

            QMetaObject::invokeMethod(this, [this, insertedHeight]() {
                QScrollBar* bar = listView->verticalScrollBar();
                bar->setValue(bar->value() + insertedHeight);
                }, Qt::QueuedConnection);
        }
    }
}

void ChatWindow::updateStatus()
{
    Status status = Status::NotOpened;
    std::string errorText;

    bool tracked = AppState::GetChatStatus(m_chatId, status, errorText);

    std::string slug = AppState::Get_Input_Box(m_chatId).model;
    if (slug.empty()) {
        slug = AppState::Get_Default_Model();
        AppState::Update_Model(slug, m_chatId);
    }
    long long max_tokens = AppState::GetModelMaxTokens(slug);
    long long current_tokens = AppState::GetChatTokens(m_chatId);
    QString tokenText = QString("Context: %1 / %2 Tokens").arg(current_tokens).arg(max_tokens);

    // Update window title if it changed in AppState
    std::string titleStr = AppState::GetChatTitle(m_chatId);
    setWindowTitle(QString("Chat - %1").arg(QString::fromStdString(titleStr)));

    // --- Always update the header status label ---
    if (!tracked) {
        m_statusLabel->setText(tokenText);
    }
    else {
        if (status == Status::ReqSent) m_statusLabel->setText("Request Sent | " + tokenText);
        else if (status == Status::ResRecieved) m_statusLabel->setText("Response Received | " + tokenText);
        else if (status == Status::Parsing) m_statusLabel->setText("Parsing | " + tokenText);
        else if (status == Status::Error) m_statusLabel->setText("Error | " + tokenText);
        else m_statusLabel->setText("Ready | " + tokenText);
    }

    // --- Determine which view to show ---

    // If we already have messages loaded, always show the chat view
    if (chatModel->rowCount() > 0) {
        m_stackedWidget->setCurrentIndex(0); // Chat View
    }
    // If cached (loaded from disk), show chat view even if empty
    else if (tracked && status == Status::Cached) {
        m_stackedWidget->setCurrentIndex(0);
    }
    // Error state
    else if (tracked && status == Status::Error) {
        m_errorLabel->setText(QString::fromStdString(errorText));
        m_stackedWidget->setCurrentIndex(2); // Error View
    }
    else if (status == Status::ReqSent) {
        m_loadingLabel->setText("Fetching conversation...");
        m_stackedWidget->setCurrentIndex(1);
    }
    else if (status == Status::ResRecieved) {
        m_loadingLabel->setText("Receiving response...");
        m_stackedWidget->setCurrentIndex(1);
    }
    else if (status == Status::Parsing) {
        m_loadingLabel->setText("Parsing data...");
        m_stackedWidget->setCurrentIndex(1);
    }
    else {
        if (tracked) {
            m_loadingLabel->setText("Loading...");
            m_stackedWidget->setCurrentIndex(1);
        }
        else {
            m_stackedWidget->setCurrentIndex(0); // Untracked/New empty chat
        }
    }

    updateSendButtonState();
}

void ChatWindow::onRetryClicked()
{
    // Update UI immediately to avoid empty states
    m_loadingLabel->setText("Retrying...");
    m_stackedWidget->setCurrentIndex(1);

    // Re-trigger the fetch
    ReqRunner::ChatTextExecution(m_chatId);
}