(() => {
    'use strict';
    const root = document.getElementById('render');
    let css = '', serial = 0, viz = new Viz();
    mermaid.initialize({startOnLoad: false, securityLevel: 'strict', suppressErrorRendering: true,
        theme: 'default', flowchart: {htmlLabels: false}});

    async function raster(svg, width, height, scale) {
        svg.setAttribute('width', width);
        svg.setAttribute('height', height);
        const image = new Image();
        const xml = new XMLSerializer().serializeToString(svg);
        await new Promise((resolve, reject) => {
            image.onload = resolve;
            image.onerror = () => reject(new Error(qsnoteTranslations.previewImageError));
            image.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(xml)}`;
        });
        const canvas = document.createElement('canvas');
        canvas.width = Math.ceil(width * scale);
        canvas.height = Math.ceil(height * scale);
        canvas.getContext('2d').drawImage(image, 0, 0, canvas.width, canvas.height);
        return canvas.toDataURL('image/png').split(',')[1];
    }

    async function math(request) {
        let text = request.text;
        if (request.language === 'mathjax') {
            // VNote's mathjax fences also accept explicit TeX delimiters.
            text = text.trim().replace(/^\$\$([\s\S]*)\$\$$/, '$1')
                .replace(/^\\\[([\s\S]*)\\\]$/, '$1');
        }
        root.style.cssText = `display:inline-block;width:max-content;white-space:nowrap;padding:2px;font-size:${request.fontSize}px;color:${request.color}`;
        katex.render(text, root, {displayMode: request.display, throwOnError: true, trust: false, output: 'html'});
        root.getBoundingClientRect();
        await Promise.all([...document.fonts].filter(font => font.family.includes('KaTeX') && font.status === 'loading')
            .map(font => font.loaded));
        const width = Math.ceil(root.getBoundingClientRect().width);
        const height = Math.ceil(root.getBoundingClientRect().height);
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        const foreign = document.createElementNS(svg.namespaceURI, 'foreignObject');
        foreign.setAttribute('width', width);
        foreign.setAttribute('height', height);
        const wrapper = document.createElementNS('http://www.w3.org/1999/xhtml', 'div');
        const style = document.createElementNS(wrapper.namespaceURI, 'style');
        style.textContent = css + '\n.katex-display{margin:0}';
        wrapper.append(style, root.cloneNode(true));
        foreign.append(wrapper);
        svg.append(foreign);
        return {png: await raster(svg, width, height, request.scale * 2), width, height};
    }

    async function graph(request) {
        root.style.cssText = `display:block;width:${request.graphWidth}px;padding:0;background:white;color:black`;
        const id = `inplace-${++serial}`;
        if (request.language === 'mermaid') {
            // Render inside the sized host: the hidden page has no viewport width.
            const result = await mermaid.render(id, request.text, root);
            root.innerHTML = result.svg;
        } else if (['flow', 'flowchart'].includes(request.language)) {
            const host = document.createElement('div');
            host.id = id;
            root.append(host);
            flowchart.parse(request.text).drawSVG(id);
        } else if (['wave', 'wavedrom'].includes(request.language)) {
            const host = document.createElement('div');
            host.id = 'wave-0';
            root.append(host);
            WaveDrom.RenderWaveForm(0, JSON.parse(request.text), 'wave-');
        } else {
            try {
                root.append(await viz.renderSVGElement(request.text, {engine: request.language === 'graphviz' ? 'dot' : request.language}));
            } catch (error) {
                viz = new Viz();
                throw error;
            }
        }
        const svg = root.querySelector('svg');
        if (!svg) throw new Error(qsnoteTranslations.missingDiagram);
        const box = svg.viewBox.baseVal;
        const width = box.width || svg.width.baseVal.value;
        const height = box.height || svg.height.baseVal.value;
        if (!(width > 0 && height > 0)) throw new Error(qsnoteTranslations.diagramSizeError);
        if (!box.width) svg.setAttribute('viewBox', `0 0 ${width} ${height}`);
        return {png: await raster(svg, width, height, request.scale * request.zoom * 2),
            width: Math.ceil(width * request.zoom), height: Math.ceil(height * request.zoom)};
    }

    function highlightStyles() {
        const styles = {};
        const sheet = [...document.styleSheets].find(value => value.href && value.href.endsWith('/highlight.css'));
        for (const rule of sheet.cssRules) {
            const format = {};
            if (rule.style.color) {
                format.color = rule.style.color.replace(/rgb\((\d+),\s*(\d+),\s*(\d+)\)/,
                    (_, r, g, b) => '#' + [r, g, b].map(value => Number(value).toString(16).padStart(2, '0')).join(''));
            }
            if (rule.style.fontWeight) format.bold = rule.style.fontWeight === 'bold';
            if (rule.style.fontStyle) format.italic = rule.style.fontStyle === 'italic';
            for (const selector of rule.selectorText.split(',')) {
                const name = selector.trim();
                if (!name.startsWith('.token.')) continue;
                for (const token of name.slice(7).split('.')) {
                    styles[token] = Object.assign(styles[token] || {}, format);
                }
            }
        }
        return styles;
    }

    // Preserve Prism's multi-line grammar while giving the Qt line parser balanced spans.
    function lineHtml(node) {
        if (node.nodeType === Node.TEXT_NODE) return Prism.util.encode(node.textContent);
        const content = [...node.childNodes].map(lineHtml).join('');
        if (node.tagName !== 'SPAN') return content;
        return content.split('\n').map(line => `<span class="${node.className}">${line}</span>`).join('\n');
    }

    new QWebChannel(qt.webChannelTransport, channel => {
        const bridge = channel.objects.inplaceBridge;
        bridge.highlightRequested.connect((index, timeStamp, text) => {
            const match = /^(`{3,}|~{3,})[^\S\n]*(\S+)?[^\n]*\n([\s\S]*)\n\1\s*$/.exec(text);
            let html = '';
            if (match && match[2] && Prism.languages[match[2]]) {
                const container = document.createElement('div');
                container.innerHTML = Prism.highlight(match[3], Prism.languages[match[2]], match[2]);
                html = lineHtml(container);
            }
            bridge.highlighted(index, timeStamp, html);
        });
        bridge.stylesheetReady.connect(value => { css = value; });
        bridge.renderRequested.connect(async json => {
            const request = JSON.parse(json);
            root.replaceChildren();
            try {
                const result = await (['math', 'mathjax'].includes(request.language) ? math(request) : graph(request));
                bridge.rendered(request.generation, request.index, result.png, result.width, result.height, '');
            } catch (error) {
                bridge.rendered(request.generation, request.index, '', 0, 0, String(error.message || error));
            } finally {
                root.replaceChildren();
            }
        });
        bridge.ready(highlightStyles());
    });
})();
