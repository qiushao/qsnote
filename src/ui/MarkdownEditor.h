#ifndef QSNOTE_MARKDOWNEDITOR_H
#define QSNOTE_MARKDOWNEDITOR_H

#include <vtextedit/vmarkdowneditor.h>
#include <functional>

class QMenu;
class InplacePreview;

class MarkdownEditor : public vte::VMarkdownEditor {
    Q_OBJECT
public:
    explicit MarkdownEditor(QWidget *parent = nullptr);
    void type(vte::TypeAction action, const QVariant &data = {});
    void showFindReplace();
    void showSettings();
    QWidget *createSettingsPage(QWidget *parent, std::function<void()> &apply);
    void reloadSettings();
    void setSplitMode(bool enabled);
    void zoom(int delta) override;
    QMenu *createEditorMenu(QWidget *parent);

signals:
    void settingsChanged();
    void imageDirectoryRequested(QString *directory);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    MarkdownEditor(const QSharedPointer<vte::MarkdownEditorConfig> &config, QWidget *parent);
    void insertImage(quint64 requestId = 0, const QString &selectedText = {});
    QString imageDirectory();
    void saveSetting(const QString &key, const QVariant &value);
    QSharedPointer<vte::MarkdownEditorConfig> config_;
    InplacePreview *inplacePreview_ = nullptr;
    bool applyingSettings_ = false;
    bool splitMode_ = false;
};

#endif
