'use strict';
document.documentElement.lang = qsnoteTranslations.language;
document.title = qsnoteTranslations.preview;
window.qsnoteArticle = (() => {
    let graphId = 0;
    let viz = new Viz();
    mermaid.initialize({startOnLoad: false, securityLevel: 'strict', suppressErrorRendering: true,
        theme: 'default', htmlLabels: false, flowchart: {htmlLabels: false}});
    function markdown(text, strict = false) {
        let metadata = '';
        const md = markdownit({html: true, linkify: true, maxNesting: 500});
        md.use(markdownitTaskLists, {enabled: true})
            .use(markdownitSub).use(markdownitSup).use(markdownitEmoji)
            .use(markdownitFootnote).use(window['markdown-it-imsize.js'])
            .use(texmath, {delimitersList: ['dollars', 'raw']})
            .use(markdownitInjectLinenumbers).use(markdownItAnchor, {slugify: text => text.trim().toLowerCase().replace(/\s+/g, '-')})
            .use(markdownItTocDoneRight).use(markdownitImplicitFigure, {figcaption: true})
            .use(markdownitMark).use(markdownitFrontMatter, text => { metadata = text; });
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
            md.renderer.rules[name] = (tokens, index) => filterXSS(tokens[index].content, {whiteList});
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
    async function graphs(root, options = {}) {
        let waveIndex = 0;
        for (const code of root.querySelectorAll('pre > code')) {
            if (options.cancelled?.()) return;
            const language = ([...code.classList].find(name => name.startsWith('language-'))?.slice(9) || '').toLowerCase();
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
    return {markdown, graphs};
})();
