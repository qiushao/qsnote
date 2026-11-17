// LayerArch 1.0: docs/layer-arch-syntax-spec.md.
(() => {
    'use strict';
    const colors = {blue: '#B5D4F4', mint: '#E1F5EE', purple: '#CECBF6', orange: '#FAEEDA',
        slate: '#D3DCE8', gray: '#F1EFE8', green: '#C8E6C9', pink: '#F5DCE6', yellow: '#FFF1BF'};
    const font = 'system-ui, sans-serif';
    const context = document.createElement('canvas').getContext('2d');
    const gap = 16;
    let serial = 0;
    const escape = text => String(text).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    const unescape = text => text.replace(/\\([|@\\])/g, '$1');
    function fail(line, code, message) {
        throw new Error(`layer-arch ${code} (line ${line}): ${message}`);
    }
    function attributes(text, line) {
        const attrs = {};
        let match;
        while ((match = /(?:^|\s)@([A-Za-z][\w-]*)(?:\(([^()]*)\)|([^\s()]*))$/.exec(text))) {
            const [, name, value, suffix] = match;
            if (Object.hasOwn(attrs, name)) fail(line, 'E008', `Duplicate @${name}`);
            if (!['color', 'fill', 'border', 'cols', 'stack', 'span', 'label'].includes(name)) {
                fail(line, 'E008', `Unsupported @${name}; use @color, @fill, @border, @cols, @stack, @span or @label`);
            }
            if (name === 'stack') {
                if (value !== undefined || suffix) fail(line, 'E008', 'Use @stack without arguments');
                attrs.stack = true;
            } else {
                if (value === undefined || suffix) fail(line, 'E008', `Use @${name}(value)`);
                if (name === 'color' && !Object.hasOwn(colors, value) && !/^#[\da-fA-F]{6}$/.test(value)) {
                    fail(line, 'E004', 'Color must be a predefined name or #RRGGBB');
                }
                const enums = {fill: ['soft', 'solid', 'none'], border: ['solid', 'dashed', 'none'], label: ['show', 'hide']};
                if (enums[name] && !enums[name].includes(value)) fail(line, 'E008', `Invalid @${name}(${value})`);
                if (name === 'cols' || name === 'span') {
                    if (!/^[1-9]\d*$/.test(value) || !Number.isSafeInteger(Number(value))) fail(line, 'E006', `@${name} requires a positive integer`);
                    attrs[name] = Number(value);
                } else attrs[name] = value;
            }
            text = text.slice(0, match.index).trimEnd();
        }
        if (/(?:^|\s)@[A-Za-z][\w-]*\((?:\S*|[^()]*)$/.test(text)) fail(line, 'E008', 'Malformed trailing attribute; use @name(value)');
        if (attrs.stack && attrs.cols !== undefined) fail(line, 'E005', '@stack and @cols cannot be combined');
        return {text: text.trim(), attrs};
    }
    function element(kind, text, line) {
        const parsed = attributes(text, line);
        let title = parsed.text, subtitle = '';
        if (kind === 'node') {
            for (let i = 0; i < title.length; i++) {
                if (title[i] === '\\' && /[|@\\]/.test(title[i + 1] || '')) { i++; continue; }
                if (title[i] !== '|') continue;
                subtitle = title.slice(i + 1).trim();
                title = title.slice(0, i).trim();
                break;
            }
        }
        if (!title) fail(line, 'E009', 'Missing title');
        return {kind, title: unescape(title), subtitle: unescape(subtitle), attrs: parsed.attrs, children: [], line};
    }
    function parse(source) {
        const diagram = {title: '', desc: '', theme: 'azure', attrs: {}, children: []};
        const metadata = new Set();
        let block = null, row = null, stack = [], connector = null;
        for (const [index, raw] of source.replace(/\r\n?/g, '\n').split('\n').entries()) {
            const line = index + 1, text = raw.trim();
            if (!text) continue;
            const indent = raw.match(/^\s*/)[0];
            if (indent.includes('\t') || /[^ ]/.test(indent)) fail(line, 'E003', 'Indent with two spaces per list level, without tabs');
            let match;
            if (text.startsWith('@')) {
                if (diagram.children.length || indent.length) fail(line, 'E008', 'Metadata must precede the first # block');
                if ((match = /^@(title|desc|theme)\s+(.+)$/.exec(text))) {
                    const [, name, value] = match;
                    if (metadata.has(name)) fail(line, 'E008', `Duplicate @${name}`);
                    if (name === 'theme' && !['azure', 'light', 'dark'].includes(value)) fail(line, 'E008', 'Theme must be azure, light or dark');
                    metadata.add(name);
                    diagram[name] = value;
                } else {
                    const parsed = attributes(text, line);
                    if (parsed.text || Object.keys(parsed.attrs).length !== 1 || !parsed.attrs.color) fail(line, 'E008', 'Only @color is a global style attribute');
                    if (metadata.has('color')) fail(line, 'E008', 'Duplicate global @color');
                    metadata.add('color');
                    diagram.attrs.color = parsed.attrs.color;
                }
                continue;
            }
            if (text === '-->') {
                if (indent.length || !block || connector) fail(line, 'E007', '--> must be between two adjacent # blocks');
                connector = {kind: 'connector', line};
                diagram.children.push(connector);
                block = null;
                row = null;
                stack = [];
                continue;
            }
            if ((match = /^(#{1,2})(?:\s+(.*))?$/.exec(text))) {
                if (indent.length) fail(line, 'E003', 'Headings must not be indented');
                const node = element(match[1].length === 1 ? 'block' : 'row', match[2] || '', line);
                if (node.kind === 'block') {
                    diagram.children.push(node);
                    block = node;
                    row = null;
                    connector = null;
                } else {
                    if (!block) fail(line, 'E001', '## requires a preceding # block');
                    if (block.children.some(child => child.kind === 'node')) fail(line, 'E002', 'Move direct list items into a ## row; do not mix - and ##');
                    block.children.push(node);
                    row = node;
                }
                stack = [];
                continue;
            }
            if ((match = /^-(?:\s+(.*))?$/.exec(text))) {
                if (!block) fail(line, connector ? 'E007' : 'E001', 'List items require a # block; after --> start a new # block');
                const depth = indent.length / 2;
                if (!Number.isInteger(depth) || depth > stack.length) fail(line, 'E003', 'Each child list must be indented by exactly two more spaces');
                const parent = depth ? stack[depth - 1] : (row || block);
                const node = element('node', match[1] || '', line);
                parent.children.push(node);
                stack.length = depth;
                stack.push(node);
                continue;
            }
            fail(line, /^[.=-]+>/.test(text) ? 'E007' : 'E008', 'Expected #, ##, - or a standalone -->; legacy syntax is not supported');
        }
        if (connector) fail(connector.line, 'E007', '--> requires a following # block');
        if (!diagram.children.length) fail(1, 'E009', 'Expected a # block with at least one component');
        function validate(node, parent) {
            if (node.kind === 'connector') return;
            const a = node.attrs, group = node.kind === 'block' && node.children[0]?.kind === 'row';
            if (node.kind !== 'node' && !node.children.length) fail(node.line, 'E009', 'Block or row requires at least one child');
            const allowed = ['color', 'fill'];
            if (node.kind === 'row' || (node.kind === 'node' && node.children.length)) allowed.push('cols', 'stack', 'label');
            if (node.kind === 'block') allowed.push('label');
            if (group || node.kind === 'node') allowed.push('border');
            if (node.kind === 'node') allowed.push('span');
            for (const name of Object.keys(a)) {
                if (!allowed.includes(name)) fail(node.line, 'E008', `@${name} is not valid on this ${node.kind}`);
            }
            if (a.span !== undefined && (!parent.attrs.cols || a.span > parent.attrs.cols)) {
                fail(node.line, 'E006', '@span requires an explicit @cols on the direct parent and cannot exceed it');
            }
            node.children.forEach(child => validate(child, node));
        }
        diagram.children.forEach(node => validate(node, diagram));
        return diagram;
    }

    function mix(base, background, ratio) {
        const rgb = hex => [1, 3, 5].map(offset => parseInt(hex.slice(offset, offset + 2), 16));
        const a = rgb(base), b = rgb(background);
        return '#' + a.map((value, i) => Math.round(value * ratio + b[i] * (1 - ratio)).toString(16).padStart(2, '0')).join('');
    }
    function ink(background) {
        const channels = [1, 3, 5].map(offset => {
            const value = parseInt(background.slice(offset, offset + 2), 16) / 255;
            return value <= .04045 ? value / 12.92 : ((value + .055) / 1.055) ** 2.4;
        });
        const luminance = channels[0] * .2126 + channels[1] * .7152 + channels[2] * .0722;
        return luminance > .179 ? '#000000' : '#ffffff';
    }
    function wrap(text, width, size, weight) {
        if (!text) return [];
        if (context) context.font = `${weight} ${size}px ${font}`;
        const measure = value => context ? context.measureText(value).width : Array.from(value).reduce((sum, ch) => sum + (ch.codePointAt(0) > 255 ? size : size * .6), 0);
        const lines = [];
        let line = '';
        // Wrap words first; split long words and CJK runs when necessary, without shrinking the font.
        for (const token of text.match(/\s+|[^\s]+/gu) || []) {
            if (measure(line + token) <= width) { line += token; continue; }
            if (line.trim()) lines.push(line.trimEnd());
            line = '';
            for (const ch of token.trimStart()) {
                if (line && measure(line + ch) > width) { lines.push(line); line = ''; }
                line += ch;
            }
        }
        if (line.trim()) lines.push(line.trimEnd());
        return lines;
    }
    function container(node) {
        return node.kind === 'node' && node.children.length > 0 || node.kind === 'block' && node.children[0]?.kind === 'row';
    }
    function columns(node) {
        return node.attrs.cols || (node.attrs.stack ? 1 : node.children.length);
    }
    function minimumWidth(node) {
        const padding = container(node) ? 16 : 0;
        let width = 112;
        if (node.children.length) {
            const units = node.kind === 'block' && container(node) ? 1 : columns(node);
            const unitWidth = Math.max(...node.children.map(child => (minimumWidth(child) + gap) / (child.attrs.span || 1))) - gap;
            width = Math.max(width, units * (unitWidth + gap) - gap + 2 * padding);
        }
        return width;
    }
    function layout(node, width, inherited, background, dark) {
        const grouped = container(node), card = node.kind === 'node';
        const color = node.attrs.color || inherited.color, fill = node.attrs.fill || inherited.fill;
        let base = colors[color] || color;
        if (dark && Object.hasOwn(colors, color)) base = mix(base, '#101827', .32);
        const surface = grouped && !card ? mix(base, background, .35) :
            card ? (fill === 'none' ? background : fill === 'soft' ? mix(base, background, .35) : base) : background;
        const padding = grouped || card ? 16 : 0;
        const visible = card && !grouped || (node.attrs.label ? node.attrs.label === 'show' : grouped);
        const title = visible ? wrap(node.title, width - 2 * Math.max(padding, 4), 14, 600) : [];
        const subtitle = visible ? wrap(node.subtitle, width - 2 * Math.max(padding, 4), 12, 400) : [];
        const textHeight = title.length * 20 + subtitle.length * 18;
        const children = [];
        let y = padding + textHeight + (textHeight && node.children.length ? 10 : 0);
        const cols = node.kind === 'block' && grouped ? 1 : columns(node);
        const unit = (width - 2 * padding - gap * (cols - 1)) / cols;
        let column = 0, rowHeight = 0;
        for (const child of node.children) {
            const span = child.attrs.span || 1;
            if (column + span > cols) { y += rowHeight + gap; rowHeight = 0; column = 0; }
            const box = layout(child, unit * span + gap * (span - 1), {color, fill}, surface, dark);
            children.push({box, x: padding + column * (unit + gap), y});
            column += span;
            rowHeight = Math.max(rowHeight, box.height);
        }
        // Stretch siblings to their grid row's height after measuring their contents.
        const rowHeights = new Map();
        for (const child of children) rowHeights.set(child.y, Math.max(rowHeights.get(child.y) || 0, child.box.height));
        for (const child of children) child.box.height = rowHeights.get(child.y);
        const height = Math.max(card ? 52 : 0, y + rowHeight + padding);
        return {node, width, height, padding, title, subtitle, children, surface, ink: ink(surface),
            fill: card && fill === 'none' ? 'none' : surface, border: node.attrs.border || 'solid', grouped, card};
    }
    function render(source, availableWidth) {
        const diagram = parse(source);
        const background = {azure: '#fbfbf8', light: '#ffffff', dark: '#101827'}[diagram.theme];
        const blocks = diagram.children.filter(node => node.kind === 'block');
        const width = Math.max(Number.isFinite(availableWidth) && availableWidth > 0 ? availableWidth : 680,
            ...blocks.map(node => minimumWidth(node) + 48));
        const uid = `layer-arch-${++serial}`, parts = [];
        const rounded = value => Math.round(value * 100) / 100;
        function draw(box, x, y) {
            const {node, padding} = box;
            parts.push(`<g class="${node.kind === 'node' ? 'node' : node.kind === 'block' ? 'layerbox' : 'layer-row'}" aria-label="${escape(node.title)}">`);
            if (box.card || box.grouped) {
                parts.push(`<rect x="${rounded(x)}" y="${rounded(y)}" width="${rounded(box.width)}" height="${rounded(box.height)}" rx="8" fill="${box.fill}" stroke="${box.border === 'none' ? 'none' : mix(box.surface, box.ink, .55)}"${box.border === 'dashed' ? ' stroke-dasharray="5 4"' : ''}/>`);
            }
            const textHeight = box.title.length * 20 + box.subtitle.length * 18;
            let textY = y + (box.card && !box.grouped ? (box.height - textHeight) / 2 : padding);
            for (const [lines, size, lineHeight, weight] of [[box.title, 14, 20, 600], [box.subtitle, 12, 18, 400]]) {
                for (const line of lines) {
                    parts.push(`<text x="${rounded(x + (box.card && !box.grouped ? box.width / 2 : Math.max(padding, 4)))}" y="${rounded(textY + lineHeight / 2)}" text-anchor="${box.card && !box.grouped ? 'middle' : 'start'}" dominant-baseline="central" font-size="${size}" font-weight="${weight}" fill="${box.ink}">${escape(line)}</text>`);
                    textY += lineHeight;
                }
            }
            box.children.forEach(child => draw(child.box, x + child.x, y + child.y));
            parts.push('</g>');
        }
        let y = 24, connected = false, previous = false;
        for (const node of diagram.children) {
            if (node.kind === 'connector') { connected = true; continue; }
            if (previous) {
                if (connected) parts.push(`<line x1="${width / 2}" y1="${y}" x2="${width / 2}" y2="${y + 30}" stroke="${ink(background)}" stroke-width="1.5" marker-end="url(#${uid}-arrow)"/>`);
                y += connected ? 32 : gap;
            }
            const box = layout(node, width - 48, {color: diagram.attrs.color || 'blue', fill: 'solid'}, background, diagram.theme === 'dark');
            draw(box, 24, y);
            y += box.height;
            previous = true;
            connected = false;
        }
        const height = Math.ceil(y + 24);
        return `<svg xmlns="http://www.w3.org/2000/svg" class="layer-arch" viewBox="0 0 ${width} ${height}" width="${width}" height="${height}" style="max-width:none" font-family="${font}" role="img" aria-labelledby="${uid}-title" aria-describedby="${uid}-desc"><title id="${uid}-title">${escape(diagram.title || 'LayerArch')}</title><desc id="${uid}-desc">${escape(diagram.desc)}</desc><defs><marker id="${uid}-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="M2 1L8 5L2 9" fill="none" stroke="${ink(background)}" stroke-width="1.5"/></marker></defs><rect width="${width}" height="${height}" fill="${background}"/>${parts.join('')}</svg>`;
    }
    window.qsnoteLayerArch = (host, source, availableWidth) => {
        host.innerHTML = render(source, availableWidth);
    };
})();
