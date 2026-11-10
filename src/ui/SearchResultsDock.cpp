#include "SearchResultsDock.h"
#include "RoundedTreeWidget.h"
#include <QMainWindow>
#include <QPainter>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
enum Role { Start = Qt::UserRole, End, HighlightStart, HighlightLength };
class ResultDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        if (!index.data(HighlightStart).isValid()) return;
        QStyleOptionViewItem styled(option);
        initStyleOption(&styled, index);
        const auto text = styled.text;
        const auto *style = styled.widget->style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &styled, styled.widget) + 1;
        const auto rect = style->subElementRect(QStyle::SE_ItemViewItemText, &styled, styled.widget).adjusted(textMargin, 0, -textMargin, 0);
        QTextLayout layout(text, styled.font);
        QTextCharFormat highlight;
        highlight.setForeground(QColor("#a00000"));
        highlight.setBackground(QColor("#ffe680"));
        layout.setFormats({{index.data(HighlightStart).toInt(), index.data(HighlightLength).toInt(), highlight}});
        layout.beginLayout();
        auto line = layout.createLine();
        line.setLineWidth(1000000);
        layout.endLayout();
        painter->save();
        painter->setClipRect(rect);
        // Let the standard delegate draw text and selection using the directory tree style.
        // Overlay only the matched text so hover, focus and inactive selection remain consistent.
        painter->setPen(Qt::transparent);
        layout.draw(painter, QPointF(rect.left(), rect.top() + (rect.height() - line.height()) / 2));
        painter->restore();
    }
};
}// namespace

SearchResultsDock::SearchResultsDock(QMainWindow *window) : QDockWidget(tr("搜索结果"), window) {
    setObjectName("searchResultsDock");
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);
    tabs_ = new QTabWidget(body);
    tabs_->setObjectName("searchResultTabs");
    tabs_->setTabsClosable(true);
    tabs_->setUsesScrollButtons(true);
    tabs_->setMinimumHeight(120);
    layout->addWidget(tabs_);
    setWidget(body);
    window->addDockWidget(Qt::BottomDockWidgetArea, this);
    hide();
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &SearchResultsDock::closeTab);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        const auto *tree = qobject_cast<QTreeWidget *>(tabs_->widget(index));
        setWindowTitle(tree ? tr("搜索结果 — %1 个匹配").arg(tree->topLevelItem(0)->childCount()) : tr("搜索结果"));
    });
    auto *showResults = new QShortcut(QKeySequence(Qt::Key_F7), window);
    connect(showResults, &QShortcut::activated, this, [this] {
        if (tabs_->count() == 0) return;
        show();
        raise();
        tabs_->currentWidget()->setFocus();
    });
}

void SearchResultsDock::showResults(const QString &query, const QString &file, QTextDocument *document, const QVector<QRegularExpressionMatch> &matches) {
    const auto title = tr("搜索“%1”（%2 个匹配，1 个文档）").arg(query).arg(matches.size());
    auto *results = new RoundedTreeWidget(tabs_);
    results->setObjectName("findResults");
    results->setHeaderHidden(true);
    results->setUniformRowHeights(true);
    results->setTextElideMode(Qt::ElideNone);
    results->setItemDelegate(new ResultDelegate(results));
    auto *fileItem = new QTreeWidgetItem(results, {tr("%1（%2 个匹配）").arg(file).arg(matches.size())});
    documents_.insert(fileItem, document);
    fileItem->setToolTip(0, file);
    connect(results, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        if (!item->data(0, Start).isValid() || item->isDisabled()) return;
        const auto document = documents_.value(item->parent());
        if (document) emit matchActivated(document, item->data(0, Start).toInt(), item->data(0, End).toInt());
    });
    for (const auto &match: matches) {
        const int start = static_cast<int>(match.capturedStart());
        const int end = static_cast<int>(match.capturedEnd());
        const auto block = document->findBlock(start);
        const auto prefix = tr("行 %1：").arg(block.blockNumber() + 1);
        auto *item = new QTreeWidgetItem(fileItem, {prefix + block.text()});
        item->setData(0, Start, start);
        item->setData(0, End, end);
        item->setData(0, HighlightStart, prefix.size() + start - block.position());
        item->setData(0, HighlightLength, qMin(end - start, block.position() + static_cast<int>(block.text().size()) - start));
        item->setToolTip(0, match.captured());
    }
    fileItem->setExpanded(true);
    results->resizeColumnToContents(0);
    if (!watchedDocuments_.contains(document)) {
        watchedDocuments_.insert(document);
        connect(document, &QTextDocument::contentsChanged, this, [this, document] { invalidateDocument(document); });
        connect(document, &QObject::destroyed, this, [this, document] {
            invalidateDocument(document);
            watchedDocuments_.remove(document);
        });
    }
    const int tab = tabs_->addTab(results, tr("%1（%2 个匹配）").arg(query).arg(matches.size()));
    tabs_->setTabToolTip(tab, title + '\n' + file);
    tabs_->setCurrentIndex(tab);
    show();
    raise();
    results->setCurrentItem(fileItem);
    results->setFocus();
}

void SearchResultsDock::invalidateDocument(QTextDocument *document) {
    for (auto it = documents_.cbegin(); it != documents_.cend(); ++it) {
        auto *root = it.key();
        if ((it.value() == document || it.value().isNull()) && !root->isDisabled()) {
            root->setDisabled(true);
            root->setText(0, root->text(0) + tr("（原文已修改或关闭，请重新查找）"));
        }
    }
}

void SearchResultsDock::closeTab(int index) {
    auto *tree = qobject_cast<QTreeWidget *>(tabs_->widget(index));
    if (!tree) return;
    documents_.remove(tree->topLevelItem(0));
    tabs_->removeTab(index);
    delete tree;
}
