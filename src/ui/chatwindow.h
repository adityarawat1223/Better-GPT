#pragma once

#include <QWidget>
#include <QListView>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QTimer>
#include <string>
#include <vector>
#include <unordered_set>
#include "Dto/ChatText.h"
#include <QStackedWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <cmath>
#include "home.h"

// Custom data roles
namespace ChatRoles {
    enum Roles {
        MessageDataRole = Qt::UserRole + 1
    };
}

#include <QStaticText>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QSet>
#include <QMap>
#include <unordered_map>
#include <functional>

struct LayoutBlock {
    BlockType type;
    QRect rect;
    QStaticText text;
    QPixmap thumbnail;
    QString action;
    bool cached = false;
};

struct ClickRegion {
    QRect rect;
    QString action;
};

struct MessageLayout {
    std::vector<LayoutBlock> blocks;
    QList<ClickRegion> clickRegions;
    int layoutWidth = 0;
    int bubbleWidth = 0;
    int bubbleHeight = 0;
    QSize totalSize;
    bool dirty = true;
};

struct CachedLayout {
    uint64_t contentHash = 0;
    int availableWidth = 0;
    MessageLayout layout;
};

class MessageLayoutCache {
public:
    CachedLayout& getLayout(const std::string& messageId);
    void invalidateRow(const std::string& messageId);
    void invalidateAll();

    void ensureFileCached(const QString& absolutePath, const std::string& messageId);
    bool isFileCached(const QString& absolutePath);
    QPixmap getThumbnail(const QString& absolutePath);

    std::function<void(const std::string&)> onLayoutInvalidated;

private:
    std::unordered_map<std::string, CachedLayout> m_layouts;
    QMap<QString, bool> m_fileStatus;
    QMap<QString, QPixmap> m_thumbnails;
    QSet<QString> m_pendingFiles;
};

class MessageLayoutBuilder {
public:
    static void build(MessageLayout& layout, const ChatMessage& msg, int availableWidth, MessageLayoutCache* cache);
};


class ChatModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ChatModel(QObject* parent = nullptr);
    void setMessages(const std::vector<ChatMessage>& messages, size_t visibleLimit);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    std::vector<ChatMessage> m_messages;
    size_t m_visibleLimit = 50;
};

class MessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit MessageDelegate(QObject* parent = nullptr);
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void setOnLayoutInvalidated(std::function<void(const std::string&)> cb) {
        m_cache.onLayoutInvalidated = cb;
    }
    void invalidateRow(const std::string& messageId) const;
    void invalidateAll() const;

protected:
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
    static uint64_t ComputeMessageHash(const ChatMessage& msg);
    void handleAction(const QString& action) const;
    mutable MessageLayoutCache m_cache;
};

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(const std::string& chatId, QWidget* parent = nullptr);
    ~ChatWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSendClicked();
    void onAttachClicked();
    void onRemoveAssetClicked(const std::string& assetId);
    void updateAttachmentsUI();
    void updateSendButtonState();
    void onModelSelected(const QString& model);
    void onTokenClicked();
    void updateMessages();
    void onScrollValueChanged(int value);
    void updateStatus();
    void onRetryClicked();

private:
    void setupUi();

    std::string m_chatId;
    QListView* listView;
    ChatModel* chatModel;
    MessageDelegate* messageDelegate;
    QTextEdit* inputEdit;
    QTimer* m_updateTimer;

    QLabel* m_statusLabel;
    QStackedWidget* m_stackedWidget;
    QWidget* m_loadingWidget;
    QLabel* m_loadingLabel;
    QWidget* m_errorWidget;
    QLabel* m_errorLabel;
    QComboBox* m_modeDropdown;
    QComboBox* m_modelSelector;
    QWidget* m_attachmentsContainer;
    QHBoxLayout* m_attachmentsLayout;
    QPushButton* m_sendBtn;

    size_t m_visibleLimit = 50;
};