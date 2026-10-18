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
        setReaderStyle(options) {
            const line = currentLine();
            const generation = revision;
            const root = document.documentElement;
            root.dataset.theme = options.theme;
            root.style.fontFamily = options.fontFamily ? `${JSON.stringify(options.fontFamily)}, sans-serif` : '';
            root.style.fontSize = `${options.fontSize}px`;
            root.style.lineHeight = options.lineHeight;
            content.style.maxWidth = options.contentWidth ? `${options.contentWidth}px` : '';
            requestAnimationFrame(() => {
                if (generation === revision && generation === requestedRevision) scrollToLine(line, false);
            });
        },
        headings() {
            return [...content.querySelectorAll('h1,h2,h3,h4,h5,h6')].map(node => ({
                title: node.textContent, level: Number(node.tagName.slice(1))
            }));
        },
        jumpToHeading(generation, index) {
            if (generation !== revision || generation !== requestedRevision) return;
            const target = content.querySelectorAll('h1,h2,h3,h4,h5,h6')[index];
            if (!target) return;
            folding.expandHiddenAncestors(target);
            target.scrollIntoView();
        },
        setHeadingFolding(enabled) {
            collapsedSections.clear();
            folding.setEnabled(enabled);
            decorateHeadings();
        }
    };

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
                const root = qsnoteArticle.markdown(request.text);
                if (request.baseUrl) {
                    await Promise.all(Array.from(root.querySelectorAll('img[src]'), async image => {
                        try {
                            const url = new URL(image.getAttribute('src'), request.baseUrl);
                            if (url.protocol === 'file:') {
                                const data = await new Promise(resolve => bridge.localImage(url.href, resolve));
                                if (data) image.src = data;
                                else image.removeAttribute('src');
                            } else image.src = url.href;
                        } catch (_) { /* Keep invalid image links as written. */ }
                    }));
                }
                if (request.revision !== requestedRevision) continue;
                ignoreScroll = true;
                content.replaceChildren(root);
                lineCount = request.text.split('\n').length;
                await qsnoteArticle.graphs(root, {
                    cancelled: () => request.revision !== requestedRevision,
                    plantUml: (source, id) => new Promise(resolve => {
                        const receive = (generation, renderedId, data, error) => {
                            if (generation !== request.revision || renderedId !== id) return;
                            bridge.plantUmlRendered.disconnect(receive);
                            resolve({data, error});
                        };
                        bridge.plantUmlRendered.connect(receive);
                        bridge.renderPlantUml(source, request.revision, id);
                    })
                });
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
        bridge.documentChanged.connect((text, generation, reset, baseUrl) => {
            requestedRevision = generation;
            if (reset) requestedLine = 0;
            pending = {text, revision: generation, reset: reset || !!pending?.reset, baseUrl};
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
