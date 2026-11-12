#include "FindReplaceDialog.h"
#include "SearchResultsDock.h"
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QTabBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QVBoxLayout>

FindReplaceDialog::FindReplaceDialog(QWidget *editor, QTextDocument *document, std::function<QTextCursor()> cursor, std::function<void(const QTextCursor &)> select)
    : QDialog(editor), editor_(editor), document_(document), cursor_(std::move(cursor)), select_(std::move(select)) {
    setObjectName("findReplace");
    setWindowTitle(tr("查找和替换"));
    setWindowFlag(Qt::Tool);
    resize(640, 360);
    auto *layout = new QVBoxLayout(this);
    tabs_ = new QTabBar(this);
    tabs_->addTab(tr("查找"));
    tabs_->addTab(tr("替换"));
    layout->addWidget(tabs_);
    auto *fields = new QFormLayout;
    query_ = new QComboBox(this);
    replacement_ = new QComboBox(this);
    query_->setEditable(true);
    replacement_->setEditable(true);
    query_->setObjectName("findText");
    replacement_->setObjectName("replaceText");
    fields->addRow(tr("查找内容"), query_);
    fields->addRow(tr("替换为"), replacement_);
    layout->addLayout(fields);
    auto *optionsLayout = new QHBoxLayout;
    case_ = new QCheckBox(tr("区分大小写"), this);
    whole_ = new QCheckBox(tr("全词匹配"), this);
    wrap_ = new QCheckBox(tr("循环查找"), this);
    selectionOnly_ = new QCheckBox(tr("仅在选区内"), this);
    case_->setObjectName("matchCase");
    whole_->setObjectName("wholeWord");
    wrap_->setObjectName("wrapAround");
    selectionOnly_->setObjectName("inSelection");
    wrap_->setChecked(true);
    for (auto *option: {case_, whole_, wrap_, selectionOnly_}) optionsLayout->addWidget(option);
    layout->addLayout(optionsLayout);
    auto *modes = new QHBoxLayout;
    mode_ = new QComboBox(this);
    mode_->setObjectName("searchMode");
    mode_->addItems({tr("普通"), tr("扩展（\\n、\\r、\\t、\\0、\\\\）"), tr("正则表达式")});
    modes->addWidget(new QLabel(tr("搜索模式"), this));
    modes->addWidget(mode_, 1);
    dot_ = new QCheckBox(tr(". 匹配换行"), this);
    dot_->setObjectName("dotMatchesNewline");
    dot_->setEnabled(false);
    modes->addWidget(dot_);
    layout->addLayout(modes);
    auto *hint = new QLabel(tr("正则替换支持 $1、\\1、${name} 捕获组；扩展模式支持转义字符。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *actions = new QHBoxLayout;
    const QStringList labels = {tr("上一个"), tr("下一个"), tr("计数"), tr("在当前文档中查找全部"), tr("替换"), tr("全部替换")};
    QList<QPushButton *> buttons;
    for (int index = 0; index < labels.size(); ++index) {
        auto *button = new QPushButton(labels[index], this);
        button->setObjectName(QStringLiteral("findAction%1").arg(index));
        button->setAutoDefault(false);
        actions->addWidget(button);
        buttons.append(button);
    }
    connect(buttons[0], &QPushButton::clicked, this, [this] { find(true); });
    connect(buttons[1], &QPushButton::clicked, this, [this] { find(false); });
    connect(buttons[2], &QPushButton::clicked, this, [this] {
        if (collect()) status_->setText(tr("%1 个匹配").arg(matches_.size()));
    });
    connect(buttons[3], &QPushButton::clicked, this, &FindReplaceDialog::showMatches);
    connect(buttons[4], &QPushButton::clicked, this, [this] { replace(false); });
    connect(buttons[5], &QPushButton::clicked, this, [this] { replace(true); });
    connect(query_->lineEdit(), &QLineEdit::returnPressed, this, [this] { find(false); });
    connect(replacement_->lineEdit(), &QLineEdit::returnPressed, this, [this] { replace(false); });
    connect(tabs_, &QTabBar::currentChanged, this, [this, fields, buttons](int index) {
        fields->setRowVisible(replacement_, index == 1);
        buttons[4]->setVisible(index == 1);
        buttons[5]->setVisible(index == 1);
    });
    fields->setRowVisible(replacement_, false);
    buttons[4]->hide();
    buttons[5]->hide();
    layout->addLayout(actions);
    status_ = new QLabel(this);
    status_->setObjectName("findResult");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    connect(query_, &QComboBox::editTextChanged, this, &FindReplaceDialog::invalidate);
    for (auto *option: {case_, whole_, selectionOnly_, dot_}) connect(option, &QCheckBox::toggled, this, &FindReplaceDialog::invalidate);
    connect(mode_, &QComboBox::currentIndexChanged, this, [this](int index) {
        dot_->setEnabled(index == 2);
        invalidate();
    });
    connect(document_, &QTextDocument::contentsChanged, this, &FindReplaceDialog::invalidate);
    for (const bool replace: {false, true}) {
        auto *shortcut = new QShortcut(replace ? QKeySequence::Replace : QKeySequence::Find, this);
        connect(shortcut, &QShortcut::activated, this, [this, replace] { present(replace); });
    }
    for (auto *parent: {editor_, static_cast<QWidget *>(this)}) {
        for (const bool backward: {false, true}) {
            auto *shortcut = new QShortcut(backward ? QKeySequence(Qt::SHIFT | Qt::Key_F3) : QKeySequence(Qt::Key_F3), parent);
            shortcut->setContext(Qt::WidgetWithChildrenShortcut);
            connect(shortcut, &QShortcut::activated, this, [this, backward] { find(backward); });
        }
    }
    const auto history = QSettings().value("search/history").toStringList();
    for (const auto &entry : history) query_->addItem(entry);
    editor_->installEventFilter(this);
}

template<typename Editor> void FindReplaceDialog::openEditor(Editor *editor, bool replace) {
    auto *dialog = editor->template findChild<FindReplaceDialog *>(QString(), Qt::FindDirectChildrenOnly);
    if (!dialog) dialog = new FindReplaceDialog(editor, editor->document(), [editor] { return editor->textCursor(); }, [editor](const QTextCursor &cursor) {
        editor->setTextCursor(cursor);
        editor->ensureCursorVisible();
    });
    dialog->present(replace);
}

void FindReplaceDialog::openFor(QPlainTextEdit *editor, bool replace) { openEditor(editor, replace); }
void FindReplaceDialog::openFor(QTextEdit *editor, bool replace) { openEditor(editor, replace); }

void FindReplaceDialog::present(bool replace) {
    const auto cursor = cursor_();
    if (!isVisible()) {
        scope_ = cursor;
        selectionOnly_->setChecked(false);
        selectionOnly_->setEnabled(cursor.hasSelection());
        if (cursor.hasSelection() && !cursor.selectedText().contains(QChar::ParagraphSeparator)) query_->setEditText(cursor.selectedText());
    }
    tabs_->setCurrentIndex(replace ? 1 : 0);
    show();
    raise();
    activateWindow();
    query_->setFocus();
    query_->lineEdit()->selectAll();
}

bool FindReplaceDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == editor_ && event->type() == QEvent::Hide) hide();
    return QDialog::eventFilter(watched, event);
}

TextSearch::Options FindReplaceDialog::options() const {
    return {static_cast<TextSearch::Mode>(mode_->currentIndex()), case_->isChecked(), whole_->isChecked(), dot_->isEnabled() && dot_->isChecked()};
}

void FindReplaceDialog::remember(QComboBox *field) {
    const auto text = field->currentText();
    if (!text.isEmpty() && field->findText(text) < 0) field->addItem(text);
}

void FindReplaceDialog::saveHistory() {
    QStringList history;
    history.reserve(query_->count());
    for (int index = 0; index < query_->count(); ++index) history.append(query_->itemText(index));
    while (history.size() > 100) history.removeLast();
    QSettings().setValue("search/history", history);
}

void FindReplaceDialog::invalidate() {
    matches_.clear();
    status_->clear();
    lastStart_ = lastEnd_ = -1;
}

bool FindReplaceDialog::collect() {
    matches_.clear();
    const auto query = query_->currentText();
    if (query.isEmpty()) {
        status_->setText(tr("请输入查找内容"));
        return false;
    }
    const auto regex = TextSearch::expression(query, options());
    if (!regex.isValid()) {
        status_->setText(tr("正则表达式无效：%1").arg(regex.errorString()));
        return false;
    }
    remember(query_);
    saveHistory();
    const auto text = document_->toPlainText();
    const int start = selectionOnly_->isChecked() ? scope_.selectionStart() : 0;
    const auto end = selectionOnly_->isChecked() ? scope_.selectionEnd() : text.size();
    auto iterator = regex.globalMatch(text, start);
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        if (match.capturedStart() > end) break;
        if (match.capturedEnd() <= end) matches_.append(match);
    }
    return true;
}

void FindReplaceDialog::selectMatch(int index) {
    if (index < 0 || index >= matches_.size()) return;
    const auto &match = matches_[index];
    QTextCursor cursor(document_);
    lastStart_ = static_cast<int>(match.capturedStart());
    lastEnd_ = static_cast<int>(match.capturedEnd());
    cursor.setPosition(lastStart_);
    cursor.setPosition(lastEnd_, QTextCursor::KeepAnchor);
    select_(cursor);
}

void FindReplaceDialog::find(bool backward) {
    if (!collect()) return;
    const auto cursor = cursor_();
    const int position = backward ? cursor.selectionStart() : cursor.selectionEnd();
    int chosen = -1;
    for (int index = 0; index < matches_.size(); ++index) {
        const auto &match = matches_[index];
        const bool sameEmpty = match.capturedStart() == match.capturedEnd() && match.capturedStart() == position && lastStart_ == position && lastEnd_ == position;
        if (sameEmpty) continue;
        if (backward && match.capturedEnd() <= position) chosen = index;
        if (!backward && match.capturedStart() >= position) {
            chosen = index;
            break;
        }
    }
    const bool wrapped = chosen < 0 && wrap_->isChecked() && !matches_.isEmpty();
    if (wrapped) chosen = backward ? static_cast<int>(matches_.size()) - 1 : 0;
    if (chosen < 0) {
        status_->setText(matches_.isEmpty() ? tr("未找到匹配内容") : tr("已到达搜索范围边界"));
        return;
    }
    selectMatch(chosen);
    status_->setText(tr("第 %1 / %2 个匹配%3").arg(chosen + 1).arg(matches_.size()).arg(wrapped ? tr("（已循环）") : QString()));
}

void FindReplaceDialog::replace(bool all) {
    if (!collect()) return;
    remember(replacement_);
    auto matches = matches_;
    if (!all) {
        const auto cursor = cursor_();
        matches.clear();
        for (const auto &match: matches_) {
            if (match.capturedStart() == cursor.selectionStart() && match.capturedEnd() == cursor.selectionEnd()
                && (cursor.hasSelection() || (lastStart_ == cursor.position() && lastEnd_ == cursor.position()))) {
                matches.append(match);
                break;
            }
        }
        if (matches.isEmpty()) {
            find(false);
            return;
        }
    }
    const int scopeStart = scope_.selectionStart();
    const int scopeEnd = scope_.selectionEnd();
    const int oldLength = document_->characterCount();
    QTextCursor edit(document_);
    edit.beginEditBlock();
    for (auto it = matches.crbegin(); it != matches.crend(); ++it) {
        edit.setPosition(static_cast<int>(it->capturedStart()));
        edit.setPosition(static_cast<int>(it->capturedEnd()), QTextCursor::KeepAnchor);
        edit.insertText(TextSearch::replacement(replacement_->currentText(), options().mode, *it));
    }
    edit.endEditBlock();
    if (selectionOnly_->isChecked()) {
        scope_.setPosition(scopeStart);
        scope_.setPosition(scopeEnd + document_->characterCount() - oldLength, QTextCursor::KeepAnchor);
    }
    if (!all && !matches.isEmpty()) {
        select_(edit);
        // Skip the same empty match when the replacement inserts no characters.
        lastStart_ = lastEnd_ = edit.position();
        find(false);
    }
    status_->setText(tr("已替换 %1 处").arg(matches.size()));
}

void FindReplaceDialog::showMatches() {
    if (!collect()) return;
    auto *results = editor_->window()->findChild<SearchResultsDock *>();
    if (!results) return;
    const auto file = document_->metaInformation(QTextDocument::DocumentTitle);
    results->showResults(query_->currentText(), file.isEmpty() ? tr("当前文档") : file, document_, matches_);
    status_->setText(tr("%1 个匹配").arg(matches_.size()));
    hide();
}
