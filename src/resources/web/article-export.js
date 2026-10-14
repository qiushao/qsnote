'use strict';
window.qsnoteExport = {
    prepare(text) {
        const root = qsnoteArticle.markdown(text, true);
        document.getElementById('content').replaceChildren(root);
        return [...root.querySelectorAll('pre > code')]
            .filter(code => /language-plantuml/i.test(code.className)).map(code => code.textContent);
    },
    async finish(plantUml, mathCss) {
        try {
            const root = document.querySelector('.qsnote-article');
            window.exportStage = "图表";
            await qsnoteArticle.graphs(root, {strict: true, plantUml: async source => ({data: plantUml[source]})});
            // Freeze generated graphics for readers without JavaScript or SVG support.
            for (const node of root.querySelectorAll('eq,eqn,.diagram svg')) {
                window.exportStage = node.tagName;
                let svg, width, height;
                const math = node.tagName.toLowerCase() !== 'svg';
                const alt = math ? node.querySelector('annotation')?.textContent || '' :
                    (node.id.startsWith('mermaid') ? 'Mermaid' : '图表') + ' 图表';
                if (math) {
                    const host = document.createElement('div');
                    host.style.cssText = 'display:inline-block;width:max-content;white-space:nowrap;padding:2px;font-size:16px;color:#24292f';
                    host.innerHTML = node.innerHTML;
                    const style = document.createElement('style');
                    style.textContent = mathCss + '\n.katex-display{margin:0}';
                    host.prepend(style);
                    document.body.append(host);
                    host.getBoundingClientRect();
                    window.exportStage = '公式字体';
                    await Promise.all([...document.fonts].filter(font => font.family.includes('KaTeX') && font.status === 'loading')
                        .map(font => font.loaded));
                    width = Math.ceil(host.getBoundingClientRect().width);
                    height = Math.ceil(host.getBoundingClientRect().height);
                    svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
                    const foreign = document.createElementNS(svg.namespaceURI, 'foreignObject');
                    foreign.setAttribute('width', width);
                    foreign.setAttribute('height', height);
                    foreign.append(host);
                    svg.append(foreign);
                } else {
                    svg = node.cloneNode(true);
                    width = Math.ceil(node.viewBox.baseVal.width || node.width.baseVal.value);
                    height = Math.ceil(node.viewBox.baseVal.height || node.height.baseVal.value);
                    if (!svg.viewBox.baseVal.width) svg.setAttribute('viewBox', `0 0 ${width} ${height}`);
                    // WaveDrom installs its stylesheet in the document rather than the SVG.
                    document.querySelectorAll('head style').forEach(style => svg.prepend(style.cloneNode(true)));
                }
                if (!(width > 0 && height > 0)) throw new Error('图片尺寸无效');
                svg.setAttribute('width', width);
                svg.setAttribute('height', height);
                window.exportStage = '图片：' + alt;
                const image = new Image();
                await new Promise((resolve, reject) => {
                    image.onload = resolve;
                    image.onerror = () => reject(new Error('无法转换图片'));
                    image.src = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(new XMLSerializer().serializeToString(svg));
                });
                const canvas = document.createElement('canvas');
                canvas.width = width * 2;
                canvas.height = height * 2;
                canvas.getContext('2d').drawImage(image, 0, 0, canvas.width, canvas.height);
                const output = new Image(width, height);
                output.style.width = `${width}px`;
                output.src = canvas.toDataURL('image/png');
                output.alt = alt;
                if (math) output.className = node.tagName.toLowerCase() === 'eq' ? 'math-inline' : 'math-display';
                node.replaceWith(output);
            }
            root.querySelectorAll('.code-toolbar > .toolbar').forEach(toolbar => toolbar.remove());
            root.querySelectorAll('input[type=checkbox]').forEach(input => input.disabled = true);
            root.querySelectorAll('[data-source-line]').forEach(node => {
                node.removeAttribute('data-source-line');
                node.classList.remove('source-line');
                if (!node.className) node.removeAttribute('class');
            });
            window.exportResult = {html: new XMLSerializer().serializeToString(root)};
        } catch (error) {
            window.exportResult = {error: String(error.message || error)};
        }
        document.title = 'article-export-ready';
    }
};
