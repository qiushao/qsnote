#ifndef QSNOTE_MARKDOWNEDITOR_H
#define QSNOTE_MARKDOWNEDITOR_H

#include <vtextedit/vmarkdowneditor.h>

class QMenu;

class MarkdownEditor : public vte::VMarkdownEditor {
    Q_OBJECT
public:
    explicit MarkdownEditor(QWidget *parent = nullptr);
    void type(vte::TypeAction action, const QVariant &data = {});
    void showFindReplace();
    void showSettings();
    void reloadSettings();
    QMenu *createEditorMenu(QWidget *parent);

signals:
    void settingsChanged();

private:
    MarkdownEditor(const QSharedPointer<vte::MarkdownEditorConfig> &config, QWidget *parent);
    void insertImage(quint64 requestId = 0, const QString &selectedText = {});
    void saveSetting(const QString &key, const QVariant &value);
    QSharedPointer<vte::MarkdownEditorConfig> config_;
    bool applyingSettings_ = false;
};

#endif
