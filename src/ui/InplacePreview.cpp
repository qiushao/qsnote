#include "InplacePreview.h"
#include "MarkdownEditor.h"
#include "core/DiagramRuntime.h"
#include "core/MathStylesheet.h"
#include "core/PlantUmlRenderer.h"
#include "core/WebTranslations.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <vtextedit/markdownhighlighter.h>
#include <vtextedit/theme.h>
#include <vtextedit/vtextedit.h>

namespace {

QString graphLanguage(const QString &info) {
    const auto language = info.section(QRegularExpression("\\s+"), 0, 0).toLower();
    static const QStringList languages = {"mermaid", "flow", "flowchart", "dot", "graphviz", "neato", "twopi", "circo", "fdp", "sfdp", "osage", "patchwork", "wave", "wavedrom", "plantuml", "puml", "mathjax"};
    return languages.contains(language) ? language : QString();
}
}// namespace

InplacePreview::InplacePreview(MarkdownEditor *editor) : QObject(editor), editor_(editor) {
    graphWidth_ = graphWidth();
    editor_->getTextEdit()->viewport()->installEventFilter(this);
    renderTimer_.setSingleShot(true);
    renderTimer_.setInterval(150);
    publishTimer_.setSingleShot(true);
    publishTimer_.setInterval(40);
    connect(&renderTimer_, &QTimer::timeout, this, &InplacePreview::renderNext);
    connect(&publishTimer_, &QTimer::timeout, this, &InplacePreview::publish);
    connect(editor_->document(), &QTextDocument::contentsChange, this, [this](int, int removed, int added) {
        if (removed || added) invalidate();
    });
    connect(editor_->getHighlighter(), &vte::MarkdownHighlighter::previewElementsUpdated, this,
            [this](quint64 revision, const QVector<QSharedPointer<const vte::Preview>> &previews) {
                if (revision_ == revision) return;
                revision_ = revision;
                update(previews);
            });
    auto *manager = editor_->getPreviewMgr();
    connect(manager, &vte::PreviewMgr::requestUpdateCodeBlocks, this, &InplacePreview::refresh);
    connect(manager, &vte::PreviewMgr::requestUpdateMathBlocks, this, &InplacePreview::refresh);
    connect(editor_, &MarkdownEditor::externalCodeBlockHighlightRequested, this,
            [this](int index, quint64 timeStamp, const QString &text) {
                ensurePage();
                if (ready_) emit highlightRequested(index, timeStamp, text);
            });
}

int InplacePreview::graphWidth() const {
    // Reserve scrollbar space even when hidden, so preview layout cannot
    // alternate between two widths as the scrollbar appears and disappears.
    const auto *edit = editor_->getTextEdit();
    return qMax(1, edit->contentsRect().width() - edit->verticalScrollBar()->sizeHint().width()
            - static_cast<int>(2 * editor_->document()->documentMargin()) - 16);
}

bool InplacePreview::eventFilter(QObject *watched, QEvent *event) {
    if (watched == editor_->getTextEdit()->viewport() && event->type() == QEvent::Resize) {
        const int width = graphWidth();
        if (width != graphWidth_) {
            graphWidth_ = width;
            QTimer::singleShot(0, this, &InplacePreview::refresh);
        }
    }
    return QObject::eventFilter(watched, event);
}

void InplacePreview::invalidate() {
    ++generation_;
    revision_ = 0;
    requests_.clear();
    next_ = 0;
    renderTimer_.stop();
    publishTimer_.stop();
}

void InplacePreview::refresh() {
    invalidate();
    QSettings settings;
    QVector<vte::PreviewElementType> types;
    if (settings.value("editor/inplacePreview", true).toBool()) {
        if (settings.value("editor/previewMath", true).toBool()) types.append(vte::PreviewElementType::Math);
        if (settings.value("editor/previewGraphs", true).toBool() || settings.value("editor/previewMath", true).toBool()) types.append(vte::PreviewElementType::Code);
    }
    editor_->getHighlighter()->setPreviewSnapshotTypes(types);
    // Defer out of config and layout callbacks.
    QTimer::singleShot(0, editor_->getHighlighter(), &vte::MarkdownHighlighter::updateHighlight);
}

void InplacePreview::update(const QVector<QSharedPointer<const vte::Preview>> &previews) {
    ++generation_;
    const auto previousImages = activeImages_;
    activeImages_.clear();
    requests_.clear();
    next_ = 0;
    QSettings settings;
    if (!settings.value("editor/inplacePreview", true).toBool()) {
        publish();
        return;
    }
    const bool mathEnabled = settings.value("editor/previewMath", true).toBool();
    const bool graphsEnabled = settings.value("editor/previewGraphs", true).toBool();
    const auto fontSize = editor_->editorFontPointSize();
    const auto color = QColor(editor_->theme()->editorStyle(vte::Theme::Text).m_textColor).name();
    const auto scale = editor_->devicePixelRatioF();
    for (const auto &preview: previews) {
        Request request;
        request.preview = preview;
        if (preview->type() == vte::PreviewElementType::Math && mathEnabled) {
            const auto math = preview.staticCast<const vte::MathPreview>();
            request.language = "math";
            request.text = math->expression();
            request.display = math->isDisplayMath();
        } else if (preview->type() == vte::PreviewElementType::Code) {
            const auto code = preview.staticCast<const vte::CodePreview>();
            request.language = graphLanguage(code->language());
            if (request.language.isEmpty() || (request.language == "mathjax" ? !mathEnabled : !graphsEnabled)) continue;
            request.text = code->code();
            request.display = true;
        } else {
            continue;
        }
        const QJsonObject identity{{"language", request.language}, {"text", request.text}, {"display", request.display}, {"font", fontSize}, {"baseFont", editor_->baseEditorFontPointSize()}, {"dpi", editor_->logicalDpiY()}, {"color", color}, {"scale", scale}, {"graphWidth", graphWidth()}};
        request.key = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
        requests_.append(request);
        const auto previous = previousImages.constFind(request.key);
        const auto *image = previous != previousImages.cend() ? &previous.value() : cache_.object(request.key);
        if (image) {
            activeImages_.insert(request.key, *image);
            attach(static_cast<int>(requests_.size()) - 1, *image);
        }
    }
    publishTimer_.start();
    renderTimer_.start();
}

void InplacePreview::ensurePage() {
    if (page_) return;
    page_ = new QWebEnginePage(this);
    installWebTranslations(page_);
    page_->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    auto *channel = new QWebChannel(page_);
    channel->registerObject("inplaceBridge", this);
    page_->setWebChannel(channel);
    page_->load(QUrl("qrc:/web/inplace-preview.html"));
}

void InplacePreview::ready(const QVariantMap &styles) {
    MarkdownEditor::ExternalCodeBlockHighlightStyles formats;
    for (auto it = styles.cbegin(); it != styles.cend(); ++it) {
        const auto properties = it.value().toMap();
        QTextCharFormat format;
        if (properties.contains("color")) format.setForeground(QColor(properties.value("color").toString()));
        if (properties.contains("bold")) format.setFontWeight(properties.value("bold").toBool() ? QFont::Bold : QFont::Normal);
        if (properties.contains("italic")) format.setFontItalic(properties.value("italic").toBool());
        formats.insert(it.key(), format);
    }
    MarkdownEditor::setExternalCodeBlockHighlihgtStyles(formats);
    ready_ = true;
    mathError_.clear();
    const auto css = embeddedMathStylesheet(&mathError_);
    if (!css.isEmpty()) emit stylesheetReady(css);
    editor_->getHighlighter()->updateHighlight();
    renderNext();
}

void InplacePreview::highlighted(int index, quint64 timeStamp, const QString &html) {
    editor_->handleExternalCodeBlockHighlightData(index, timeStamp, html);
}

void InplacePreview::renderNext() {
    if (busy_ || renderTimer_.isActive()) return;
    while (next_ < requests_.size() && requests_[next_].item) ++next_;
    if (next_ >= requests_.size()) {
        publish();
        // A fresh highlighter publication applies preview-driven source folding.
        QTimer::singleShot(0, editor_->getHighlighter(), &vte::MarkdownHighlighter::updateHighlight);
        return;
    }
    const auto &request = requests_[next_];
    if (request.language == "sfdp") {
        busy_ = true;
        const auto executable = DiagramRuntime::graphviz();
        if (executable.isEmpty()) {
            rendered(generation_, next_, {}, 0, 0, QCoreApplication::translate("InplacePreview", "未找到 Graphviz，请重新安装完整应用包，或安装 Graphviz。"));
            return;
        }
        auto *process = new QProcess(this);
        const auto complete = [this, process, generation = generation_, index = next_](const QByteArray &png, const QString &error) {
            const auto image = QImage::fromData(png);
            rendered(generation, index, QString::fromLatin1(png.toBase64()), image.width(), image.height(), error);
            process->deleteLater();
        };
        connect(process, &QProcess::errorOccurred, this, [process, complete](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) complete({}, QCoreApplication::translate("InplacePreview", "无法启动 Graphviz：") + process->errorString());
        });
        connect(process, &QProcess::finished, this, [process, complete](int exitCode, QProcess::ExitStatus status) {
            if (status == QProcess::NormalExit && exitCode == 0) complete(process->readAllStandardOutput(), {});
            else
                complete({}, QCoreApplication::translate("InplacePreview", "Graphviz 渲染失败：") + QString::fromUtf8(process->readAllStandardError()).right(2000));
        });
        connect(process, &QProcess::started, this, [process, text = request.text] {
            process->write(text.toUtf8());
            process->closeWriteChannel();
        });
        QTimer::singleShot(30000, process, [process] { process->kill(); });
        process->setProcessEnvironment(DiagramRuntime::environment());
        process->start(executable, {"-Ksfdp", "-Tpng"});
        return;
    }
    if (request.language == "plantuml" || request.language == "puml") {
        busy_ = true;
        auto *renderer = new PlantUmlRenderer(this);
        connect(renderer, &PlantUmlRenderer::finished, this,
                [this, renderer, generation = generation_, index = next_](const QByteArray &png, const QString &error) {
                    const auto image = QImage::fromData(png);
                    rendered(generation, index, QString::fromLatin1(png.toBase64()), image.width(), image.height(), error);
                    renderer->deleteLater();
                });
        renderer->render(request.text);
        return;
    }
    ensurePage();
    if (!ready_) return;
    if ((request.language == "math" || request.language == "mathjax") && !mathError_.isEmpty()) {
        rendered(generation_, next_, {}, 0, 0, mathError_);
        return;
    }
    busy_ = true;
    const QJsonObject data{{"generation", generation_}, {"index", next_}, {"language", request.language}, {"text", request.text}, {"display", request.display}, {"fontSize", editor_->editorFontPointSize() * editor_->logicalDpiY() / 72.0}, {"zoom", static_cast<double>(editor_->editorFontPointSize()) / editor_->baseEditorFontPointSize()}, {"color", QColor(editor_->theme()->editorStyle(vte::Theme::Text).m_textColor).name()}, {"scale", editor_->devicePixelRatioF()}, {"graphWidth", graphWidth()}};
    emit renderRequested(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
}

void InplacePreview::rendered(int generation, int index, const QString &png, int width, int height, const QString &error) {
    busy_ = false;
    if (generation == generation_ && index < requests_.size()) {
        Image image{QPixmap(), QSize(width, height)};
        image.pixmap.loadFromData(QByteArray::fromBase64(png.toLatin1()), "PNG");
        if (!image.pixmap.isNull() && width > 0 && height > 0) {
            // Native PNGs have no editor zoom applied yet.
            const auto &language = requests_[index].language;
            if (language == "plantuml" || language == "puml" || language == "sfdp") {
                image.size *= static_cast<double>(editor_->editorFontPointSize()) / editor_->baseEditorFontPointSize();
            }
            attach(index, image);
            activeImages_.insert(requests_[index].key, image);
            cache_.insert(requests_[index].key, new Image(image), qMax(1, static_cast<int>(image.pixmap.width() * static_cast<qint64>(image.pixmap.height()) * 4 / 1024)));
        } else if (!error.isEmpty()) {
            qWarning().noquote() << QCoreApplication::translate("InplacePreview", "就地预览渲染失败（%1）：").arg(requests_[index].language) << error;
        }
        next_ = index + 1;
        publishTimer_.start();
    }
    QTimer::singleShot(0, this, &InplacePreview::renderNext);
}

void InplacePreview::attach(int index, const Image &image) {
    auto &request = requests_[index];
    const auto &preview = request.preview;
    const auto block = editor_->document()->findBlock(preview->endPos() - 1);
    if (!block.isValid()) return;
    auto item = QSharedPointer<vte::PreviewItem>::create();
    item->m_blockNumber = block.blockNumber();
    item->m_blockPos = block.position();
    item->m_isBlockwise = preview->placement() == vte::PreviewPlacement::BlockAfterSource;
    item->m_startPos = qMax(preview->startPos(), block.position());
    item->m_endPos = preview->endPos();
    item->m_padding = vte::PreviewMgr::calculateBlockMargin(block, qRound(editor_->getTextEdit()->tabStopDistance()));
    item->m_name = "inplace-" + request.key;
    item->m_image = image.pixmap;
    item->m_logicalSize = image.size;
    if (preview->type() == vte::PreviewElementType::Code && request.language != "mathjax") {
        const int width = qMax(1, graphWidth() - item->m_padding);
        if (item->m_logicalSize.width() > width) {
            item->m_logicalSize.scale(width, item->m_logicalSize.height(), Qt::KeepAspectRatio);
        }
        item->m_backgroundColor = qRgb(255, 255, 255);
    }
    request.item = item;
}

void InplacePreview::publish() {
    publishTimer_.stop();
    QVector<QSharedPointer<vte::PreviewItem>> math, graphs;
    for (const auto &request: requests_) {
        if (!request.item) continue;
        if (request.preview->type() == vte::PreviewElementType::Math) math.append(request.item);
        else
            graphs.append(request.item);
    }
    editor_->getPreviewMgr()->updateMathBlocks(math);
    editor_->getPreviewMgr()->updateCodeBlocks(graphs);
}
