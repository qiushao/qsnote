'use strict';
document.documentElement.lang = qsnoteTranslations.language;
document.title = qsnoteTranslations.preview;
window.qsnoteArticle = (() => {
    let graphId = 0;
    let viz = new Viz();
    mermaid.initialize({startOnLoad: false, securityLevel: 'strict', suppressErrorRendering: true,
        theme: 'default', htmlLabels: false, flowchart: {htmlLabels: false}});
    function codeLanguage(code) {
        const language = ([...code.classList].find(name => name.startsWith('language-'))?.slice(9) || '').toLowerCase();
        return language === 'puml' ? 'plantuml' : language;
    }
    function markdown(text, strict = false) {
        let metadata = '';
        const md = markdownit({html: true, linkify: true, maxNesting: 500});
        md.linkify.set({fuzzyLink: false});
        md.use(markdownitTaskLists, {enabled: true})
            .use(markdownitSub).use(markdownitSup).use(markdownitEmoji)
            .use(markdownitFootnote).use(window['markdown-it-imsize.js'])
            .use(texmath, {delimitersList: ['dollars', 'raw']})
            .use(markdownitInjectLinenumbers).use(markdownItAnchor, {slugify: text => text.trim().toLowerCase().replace(/\s+/g, '-')})
            .use(markdownItTocDoneRight).use(markdownitImplicitFigure, {figcaption: true})
            .use(markdownitMark).use(markdownitFrontMatter, text => { metadata = text; });
        // A dollar in prose may belong to a shell variable. Only consume a
        // single-dollar pair when its contents can actually be parsed as math.
        const dollarRule = texmath.rules.dollars.inline.find(rule => rule.name === 'math_dollars_inline');
        const parseDollar = texmath.inline(dollarRule);
        md.inline.ruler.at(dollarRule.name, (state, silent) => {
            const start = state.pos;
            if (!parseDollar(state, true)) return false;
            const end = state.pos;
            state.pos = start;
            // The dollar starting ${NAME} is not a closing math delimiter.
            if (/^\{[A-Za-z_][A-Za-z0-9_]*\}/.test(state.src.slice(end))) return false;
            try {
                katex.renderToString(state.src.slice(start + 1, end - 1), {throwOnError: true, strict: 'ignore', trust: false});
            } catch (error) {
                if (error instanceof katex.ParseError) return false;
                throw error;
            }
            return parseDollar(state, silent);
        });
        texmath.render = (text, display) => katex.renderToString(text, {
            displayMode: display, throwOnError: strict, trust: false
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
            md.renderer.rules[name] = (tokens, index) => {
                // Preserve the inert blog excerpt boundary through static export.
                const html = tokens[index].content;
                return html.trim() === '<!-- more -->' ? '<!-- more -->' : filterXSS(html, {whiteList});
            };
        }

        const fence = md.renderer.rules.fence;
        md.renderer.rules.fence = (tokens, index, ...args) => {
            if (tokens[index].info.trim().split(/\s+/)[0].toLowerCase() !== 'mathjax') return fence(tokens, index, ...args);
            const source = tokens[index].content.trim().replace(/^\$\$([\s\S]*)\$\$$/, '$1').replace(/^\\\[([\s\S]*)\\\]$/, '$1');
            return `<div><eqn class="tex-to-render">${texmath.render(source, true)}</eqn></div>`;
        };
        const root = document.createElement('article');
        root.className = 'qsnote-article';
        root.innerHTML = md.render(text);
        // GitHub-style alerts: a blockquote whose first line is [!NOTE] etc.
        // becomes the same alert aside as the "alert-..." container syntax.
        for (const quote of [...root.querySelectorAll('blockquote')]) {
            const paragraph = quote.querySelector(':scope > p');
            const marker = paragraph?.firstChild;
            const match = marker?.nodeType === 3
                && /^\s*\[!(note|tip|important|warning|caution)\][ \t]*(\n|$)/i.exec(marker.textContent);
            if (!match) continue;
            marker.textContent = marker.textContent.slice(match[0].length);
            if (!paragraph.textContent.trim()) paragraph.remove();
            const type = match[1].toLowerCase();
            const title = document.createElement('p');
            title.className = 'alert-title';
            title.textContent = type[0].toUpperCase() + type.slice(1);
            const aside = document.createElement('aside');
            aside.className = `alert-${type}`;
            if (quote.dataset.sourceLine !== undefined) aside.dataset.sourceLine = quote.dataset.sourceLine;
            aside.append(title);
            while (quote.firstChild) aside.append(quote.firstChild);
            quote.replaceWith(aside);
        }
        // Preserve authored image dimensions even when a site theme sets image sizes.
        root.querySelectorAll('img').forEach(image => {
            for (const dimension of ['width', 'height']) {
                const value = image.getAttribute(dimension);
                if (value) image.style[dimension] = /^\d+$/.test(value) ? `${value}px` : value;
            }
        });
        if (metadata) {
            const details = document.createElement('details');
            const summary = document.createElement('summary');
            summary.textContent = qsnoteTranslations.metadata;
            const pre = document.createElement('pre');
            pre.textContent = metadata;
            details.append(summary, pre);
            root.prepend(details);
        }
        return root;
    }
    // The clipboard API needs a secure context, which qrc:// and file:// are not.
    function copyText(text) {
        const area = document.createElement('textarea');
        area.value = text;
        area.readOnly = true;
        area.style.cssText = 'position:fixed;top:0;left:-9999px;opacity:0';
        document.body.append(area);
        area.select();
        const copied = document.execCommand('copy');
        area.remove();
        if (copied || !navigator.clipboard) return Promise.resolve(copied);
        return navigator.clipboard.writeText(text).then(() => true, () => false);
    }
    function decorateCode(code, language) {
        const pre = code.parentElement;
        if (!pre || pre.tagName !== 'PRE' || pre.parentElement?.classList.contains('code-block')) return;
        const figure = document.createElement('figure');
        figure.className = 'code-block';
        if (language) figure.dataset.language = language;
        const copy = document.createElement('button');
        copy.className = 'code-copy';
        copy.type = 'button';
        copy.textContent = qsnoteTranslations.copy;
        copy.addEventListener('click', async () => {
            if (!await copyText(code.textContent)) return;
            copy.textContent = qsnoteTranslations.copied;
            setTimeout(() => { copy.textContent = qsnoteTranslations.copy; }, 1500);
        });
        pre.replaceWith(figure);
        figure.append(copy, pre);
    }
    async function graphs(root, options = {}) {
        let waveIndex = 0;
        for (const code of root.querySelectorAll('pre > code')) {
            if (options.cancelled?.()) return;
            const language = codeLanguage(code);
            const graph = document.createElement('div');
            graph.className = 'diagram';
            graph.id = `diagram-${++graphId}`;
            if (code.dataset.sourceLine !== undefined) graph.dataset.sourceLine = code.dataset.sourceLine;
            const source = code.textContent;
            const parent = code.parentElement;
            try {
                if (language === 'mermaid') {
                    const result = await mermaid.render(`mermaid-${graphId}`, source);
                    if (options.cancelled?.()) return;
                    graph.innerHTML = result.svg;
                } else if (language === 'plantuml') {
                    const result = await options.plantUml(source, graphId);
                    if (options.cancelled?.()) return;
                    if (result.error) throw new Error(result.error);
                    const image = new Image();
                    image.alt = qsnoteTranslations.plantUml;
                    await new Promise((resolve, reject) => {
                        image.onload = resolve;
                        image.onerror = () => reject(new Error(qsnoteTranslations.plantUmlImageError));
                        image.src = `data:image/png;base64,${result.data}`;
                    });
                    graph.appendChild(image);
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
                    decorateCode(code, language);
                    continue;
                }
                if (options.cancelled?.()) { graph.remove(); return; }
                parent.replaceWith(graph);
            } catch (error) {
                if (options.strict) throw new Error(`${language === 'mermaid' ? 'Mermaid' : language} ${qsnoteTranslations.diagramError}${error.message || error}`);
                graph.remove();
                const message = document.createElement('p');
                message.className = 'render-error';
                message.textContent = qsnoteTranslations.diagramError + (error.message || error);
                parent.after(message);
            }
        }
    }
    return {markdown, graphs, codeLanguage};
})();
