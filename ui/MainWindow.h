#pragma once
#include <QMainWindow>
#include <QImage>
#include <QUrl>
#include <QMap>
class MarkdownManager;
class ImageInsertJob;
class MarkdownEditor;
class MarkdownDocumentView;
class DocumentSidebar;
class OutlineSidebar;
class KnowledgeGraphWidget;
class QLabel;
class QTimer;
class QAction;
class QPushButton;
class QSplitter;
class QStackedWidget;
class DocumentFindDialog;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &dataPath, QWidget *parent = nullptr);
    bool openDocument(const QString &path);
    void setRelationsVisible(bool visible);
    void setSourceMode(bool sourceMode);
    void openKnowledgeTree();
protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void applyResponsiveLayout();
    void normalizeSidebarWidth();
    void setColumnWidths(int documents, int outline);
    void sizeRelationPanel();
    bool saveCurrent();
    bool saveAs();
    bool mayLeaveDocument();
    bool mayCloseLibrary();
    void createDocument();
    void createDocumentInDirectory(const QString &parentPath);
    void createDocumentAtPath(const QString &path);
    void importDocument();
    void renameDocument();
    void renameDocument(const QString &path);
    void deleteDocument();
    void deleteDocument(const QString &path);
    void createDirectory(const QString &parentPath);
    void renameDirectory(const QString &path);
    void deleteDirectory(const QString &path);
    void movePath(const QString &path, bool directory, const QString &destinationDirectory);
    void moveDocuments(const QStringList &paths, const QString &destinationDirectory);
    void openFileManager(const QString &path);
    void openDocumentLink(const QUrl &url);
    void finishRemoval(bool currentRemoved);
    void refreshDocuments();
    void refreshView();
    void refreshOutline();
    void refreshGraph();
    void showDuplicateDocuments();
    void reloadCurrent();
    void renderDocument(bool force = false);
    void undoRedo(bool redo);
    void updateEditorStatus();
    void insertImageFiles(const QList<QUrl> &urls);
    void insertClipboardImage(const QImage &image);
    void exportDocument();
    void insertImageReference(const QString &reference, const QString &alt);
    void showError(const QString &error);
    void openMarkdownSettings();
    void openImageSettings();
    ImageInsertJob *prepareImageInsertion();
    void applyMarkdownSettings();
    QString askDocumentPath(const QString &title, const QString &initial = {});
    MarkdownManager *m_manager;
    MarkdownEditor *m_editor;
    MarkdownDocumentView *m_document;
    DocumentFindDialog *m_finder = nullptr;
    QStackedWidget *m_views;
    DocumentSidebar *m_documents;
    OutlineSidebar *m_outline;
    KnowledgeGraphWidget *m_graph;
    QWidget *m_relationPanel;
    QSplitter *m_vertical = nullptr, *m_horizontal = nullptr;
    QLabel *m_title, *m_notice;
    QPushButton *m_reload;
    QTimer *m_parseTimer;
    QAction *m_relationAction;
    QAction *m_outlineAction;
    QAction *m_documentAction, *m_sourceAction;
    QString m_current;
    QString m_duplicateNoticeSignature;
    bool m_loading = false, m_busy = false;
    struct RenderedEditCaret { int before, after; };
    QMap<int, RenderedEditCaret> m_renderedEditCarets;
    bool m_syncingDocument = false, m_restoringHistory = false;
    int m_pendingDocumentCaret = -1;
    bool m_exporting = false;
    enum class LayoutMode { Landscape, Portrait, NarrowPortrait };
    LayoutMode m_layoutMode = LayoutMode::Landscape;
    bool m_layoutInitialized = false, m_applyingLayout = false;
    bool m_portraitOutlineVisible = true;
    int m_landscapeDocumentWidth = 200, m_portraitOutlineWidth = 215;
};
