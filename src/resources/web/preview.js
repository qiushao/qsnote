'use strict';

(() => {
    const content = document.getElementById('content');
    let bridge;
    let revision = -1;
    let requestedRevision = -1;
    let lineCount = 1;
    let pending = null;
    let rendering = false;
    let requestedLine = null;
    let ignoreScroll = false;
    let graphId = 0;
    let viz = new Viz();
    let metadata = '';
    let collapsedSections = new Set();
    const folding = new HeadingFolding(content);
    function foldedHeadings() {
        return new Set([...content.querySelectorAll('.vx-heading-fold-toggle[aria-expanded="false"]')]
            .map(button => button.parentElement.id));
    }
    function decorateHeadings(collapsed = new Set()) {
        folding.refresh();
        content.querySelectorAll('.vx-heading-fold-toggle').forEach(button => {
            button.tabIndex = 0;
            if (collapsed.has(button.parentElement.id)) {
                folding.setExpanded(button, document.getElementById(button.getAttribute('aria-controls')), false);
            }
        });
    }
    function expandAll(expanded = true) {
        content.querySelectorAll('.vx-heading-fold-toggle').forEach(button => {
            folding.setExpanded(button, document.getElementById(button.getAttribute('aria-controls')), expanded);
        });
    }
    window.qsnotePreview = {
        expandAll,
        setHeadingFolding(enabled) {
            collapsedSections.clear();
            folding.setEnabled(enabled);
            decorateHeadings();
        }
    };
    const md = markdownit({html: true, linkify: true, maxNesting: 500});
    md.use(markdownitTaskLists, {enabled: true})
        .use(markdownitSub).use(markdownitSup).use(markdownitEmoji)
        .use(markdownitFootnote).use(window['markdown-it-imsize.js'])
        .use(texmath, {delimitersList: ['dollars', 'raw']})
        .use(markdownitInjectLinenumbers).use(markdownItAnchor)
        .use(markdownItTocDoneRight).use(markdownitImplicitFigure, {figcaption: true})
        .use(markdownitMark).use(markdownitFrontMatter, text => { metadata = text; });
    texmath.render = (text, display) => katex.renderToString(text, {
        displayMode: display, throwOnError: false, trust: false
    });
    // VTextEdit distinguishes display-style typesetting from block placement.
    md.renderer.rules.math_dollars_inline_double = (tokens, index) =>
        `<eq class="tex-to-render">${texmath.render(tokens[index].content, true)}</eq>`;
    md.use(markdownitContainer, 'alert', {
        validate: text => /^alert-\w+$/.test(text.trim()),
        render: (tokens, index) => tokens[index].nesting === 1
            ? `<aside class="${tokens[index].info.trim()}">` : '</aside>\n'
    });
    // Filter authored HTML only. Library-generated math retains its MathML and
    // SVG, and the XSS filter keeps its default URL and CSS validation.
    const whiteList = filterXSS.getDefaultWhiteList();
    for (const tag of Object.keys(whiteList)) whiteList[tag] = [...whiteList[tag], 'class', 'id'];
    whiteList.input = ['type', 'checked', 'disabled', 'class'];
    for (const name of ['html_inline', 'html_block']) {
        md.renderer.rules[name] = (tokens, index) => filterXSS(tokens[index].content, {whiteList});
    }
    mermaid.initialize({startOnLoad: false, securityLevel: 'strict', suppressErrorRendering: true});

    function points() {
        const result = [{line: 0, y: 0}];
        content.querySelectorAll('[data-source-line]').forEach(node => {
            const line = Number(node.dataset.sourceLine);
            const y = node.getBoundingClientRect().top + scrollY;
            if (Number.isFinite(line) && node.getClientRects().length) result.push({line, y});
        });
        result.sort((a, b) => a.line - b.line || a.y - b.y);
        const unique = [];
        for (const point of result) {
            if (!unique.length || point.line > unique[unique.length - 1].line) unique.push(point);
        }
        unique.push({line: lineCount, y: document.documentElement.scrollHeight});
        return unique;
    }
    function currentLine() {
        const map = points();
        for (let i = 1; i < map.length; ++i) {
            if (map[i].y > scrollY) {
                const a = map[i - 1], b = map[i];
                return Math.floor(a.line + (b.line - a.line) * Math.max(0, scrollY - a.y) / Math.max(1, b.y - a.y));
            }
        }
        return lineCount - 1;
    }
    function scrollToLine(line, reveal = true) {
        if (reveal) {
            const nodes = [...content.querySelectorAll('[data-source-line]')]
                .sort((a, b) => Number(a.dataset.sourceLine) - Number(b.dataset.sourceLine));
            let target = nodes[0];
            for (const node of nodes) {
                if (Number(node.dataset.sourceLine) > line) break;
                target = node;
            }
            if (target) folding.expandHiddenAncestors(target);
        }
        const map = points();
        for (let i = 1; i < map.length; ++i) {
            if (map[i].line >= line) {
                const a = map[i - 1], b = map[i];
                ignoreScroll = true;
                window.scrollTo(0, a.y + (b.y - a.y) * (line - a.line) / Math.max(1, b.line - a.line));
                requestAnimationFrame(() => requestAnimationFrame(() => { ignoreScroll = false; }));
                return;
            }
        }
    }
    async function graphs(root, generation) {
        let waveIndex = 0;
        for (const code of root.querySelectorAll('pre > code')) {
            if (generation !== requestedRevision) return;
            const language = [...code.classList].find(name => name.startsWith('language-'))?.slice(9) || '';
            const graph = document.createElement('div');
            graph.className = 'diagram';
            graph.id = `diagram-${++graphId}`;
            if (code.dataset.sourceLine !== undefined) graph.dataset.sourceLine = code.dataset.sourceLine;
            const source = code.textContent;
            const parent = code.parentElement;
            try {
                if (language === 'mermaid') {
                    const result = await mermaid.render(`mermaid-${graphId}`, source);
                    if (generation !== requestedRevision) return;
                    graph.innerHTML = result.svg;
                } else if (['dot', 'graphviz', 'neato', 'twopi', 'circo', 'fdp', 'sfdp', 'osage', 'patchwork'].includes(language)) {
                    try {
                        graph.appendChild(await viz.renderSVGElement(source, {engine: language === 'graphviz' ? 'dot' : language}));
                    } catch (error) {
                        viz = new Viz();
                        throw error;
                    }
                } else if (['flow', 'flowchart', 'wave', 'wavedrom'].includes(language)) {
                    parent.after(graph);
                    if (language === 'flow' || language === 'flowchart') {
                        flowchart.parse(source).drawSVG(graph.id);
                    } else {
                        // WaveDrom's style initializer requires index zero.
                        graph.id = `wave-${waveIndex}`;
                        WaveDrom.RenderWaveForm(waveIndex++, JSON.parse(source), 'wave-');
                        graph.id = `diagram-${graphId}`;
                    }
                } else {
                    if (Prism.languages[language]) Prism.highlightElement(code);
                    continue;
                }
                if (generation !== requestedRevision) { graph.remove(); return; }
                parent.replaceWith(graph);
            } catch (error) {
                graph.remove();
                const message = document.createElement('p');
                message.className = 'render-error';
                message.textContent = `图表渲染失败：${error.message || error}`;
                parent.after(message);
            }
        }
    }
    async function renderLatest() {
        if (rendering) return;
        rendering = true;
        try {
            while (pending) {
                const request = pending;
                pending = null;
                const oldLine = request.reset ? 0 : currentLine();
                if (request.reset) collapsedSections.clear();
                else if (content.querySelector('.vx-heading-fold-toggle')) collapsedSections = foldedHeadings();
                metadata = '';
                const root = document.createElement('article');
                root.innerHTML = md.render(request.text);
                if (metadata) {
                    const details = document.createElement('details');
                    const summary = document.createElement('summary');
                    summary.textContent = '文档元数据';
                    const pre = document.createElement('pre');
                    pre.textContent = metadata;
                    details.append(summary, pre);
                    root.prepend(details);
                }
                ignoreScroll = true;
                content.replaceChildren(root);
                lineCount = request.text.split('\n').length;
                await graphs(root, request.revision);
                if (request.revision !== requestedRevision) continue;
                decorateHeadings(collapsedSections);
                revision = request.revision;
                scrollToLine(requestedLine ?? oldLine, requestedLine !== null);
                requestedLine = null;
                bridge.rendered(revision);
            }
        } finally {
            rendering = false;
        }
    }
    let scrollTimer;
    window.addEventListener('scroll', () => {
        if (ignoreScroll || rendering) return;
        clearTimeout(scrollTimer);
        scrollTimer = setTimeout(() => bridge.sourceLine(revision, currentLine()), 80);
    }, {passive: true});
    content.addEventListener('change', event => {
        if (!event.target.matches('input.task-list-item-checkbox')) return;
        const line = event.target.closest('[data-source-line]');
        if (line) bridge.taskChanged(revision, Number(line.dataset.sourceLine), event.target.checked);
    });
    content.addEventListener('click', event => {
        const anchor = event.target.closest('a[href^="#"]');
        if (anchor) {
            let target;
            try { target = document.getElementById(decodeURIComponent(anchor.hash.slice(1))); } catch (_) { return; }
            if (target) {
                event.preventDefault();
                folding.expandHiddenAncestors(target);
                target.scrollIntoView();
            }
            return;
        }
        const image = event.target.closest('img');
        if (!image) return;
        event.preventDefault();
        const dialog = document.createElement('dialog');
        dialog.className = 'image-viewer';
        dialog.append(image.cloneNode());
        document.body.append(dialog);
        dialog.addEventListener('click', () => dialog.close());
        dialog.addEventListener('close', () => dialog.remove());
        dialog.showModal();
    });
    new QWebChannel(qt.webChannelTransport, channel => {
        bridge = channel.objects.previewBridge;
        bridge.documentChanged.connect((text, generation, reset) => {
            requestedRevision = generation;
            if (reset) requestedLine = 0;
            pending = {text, revision: generation, reset: reset || !!pending?.reset};
            renderLatest();
        });
        bridge.scrollRequested.connect((generation, line) => {
            if (generation !== requestedRevision) return;
            if (rendering) requestedLine = line;
            else scrollToLine(line);
        });
        bridge.ready();
    });
})();
