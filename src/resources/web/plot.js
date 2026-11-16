'use strict';
let qsnotePlotSerial = 0;
window.qsnotePlot = (host, source, availableWidth, interactive, language = 'function-plot') => {
    // Accept the separator comma when copying a configuration from an example list.
    const options = language === 'plot' ? qsnoteSimplePlotOptions(source) : JSON5.parse(source.replace(/,\s*$/, ''));
    if (!options || Array.isArray(options) || typeof options !== 'object'
        || !Array.isArray(options.data) || !options.data.length) {
        throw new Error('function-plot: expected an object with a non-empty data array');
    }
    for (const key of ['width', 'height']) {
        if (options[key] !== undefined && (!Number.isFinite(options[key]) || options[key] <= 0)) {
            throw new Error('function-plot: invalid ' + key);
        }
    }
    for (const key of ['target', 'id', 'plugins', 'xDomain', 'yDomain']) delete options[key];
    options.target = host;
    options.width = options.width || Math.max(1, availableWidth);
    options.height = options.height || 400;
    options.disableZoom = !interactive || options.disableZoom !== false;
    functionPlot(options);
    const svg = host.querySelector('svg');
    let height = options.height;
    if (language === 'plot') {
        const id = `qsnote-plot-${++qsnotePlotSerial}`;
        svg.id = id;
        const create = (tag, attributes) => {
            const element = document.createElementNS(svg.namespaceURI, tag);
            for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, value);
            return element;
        };
        const defs = create('defs', {});
        const gradient = create('linearGradient', {id: id + '-background', x1: '0', y1: '0', x2: '0', y2: '1'});
        gradient.append(create('stop', {offset: '0', 'stop-color': '#0d1430'}),
            create('stop', {offset: '1', 'stop-color': '#080d1f'}));
        // User-space bounds also give horizontal constant functions a visible glow.
        const glow = create('filter', {id: id + '-glow', filterUnits: 'userSpaceOnUse',
            x: -16, y: -16, width: options.width + 32, height: options.height + 32,
            'color-interpolation-filters': 'sRGB'});
        glow.append(create('feGaussianBlur', {stdDeviation: 4, result: 'blur'}));
        const merge = create('feMerge', {});
        merge.append(create('feMergeNode', {in: 'blur'}), create('feMergeNode', {in: 'SourceGraphic'}));
        glow.append(merge);
        defs.append(gradient, glow);
        svg.prepend(defs);
        // Embed styling so redraws and standalone rasterization use the same palette.
        const style = create('style', {});
        style.textContent = `
            #${id} { font-family: system-ui, sans-serif; }
            #${id} .axis text { fill: #93a0cc; font-size: 11px; }
            #${id} .axis path, #${id} .axis line { stroke: #7896f0; opacity: .22; }
            #${id} .origin { stroke: #8fa3e8; opacity: .65; }
            #${id} .axis-label { fill: #93a0cc; font-size: 13px; }
            #${id} .title { fill: #e9edff; font-size: 16px; font-weight: 600; }
            #${id} .tip text { font: 12px system-ui, sans-serif; }
            #${id} .graph { filter: url(#${id}-glow); }
        `;
        svg.prepend(style);
        // Include labels in the SVG so they survive inline previews and raster exports.
        let x = 16, y = options.height + 10;
        for (const curve of options.data) {
            const label = create('text', {fill: '#e9edff', 'font-size': 11, 'font-family': 'monospace'});
            label.textContent = curve.title;
            svg.append(label);
            const width = Math.min(options.width - 32, label.getComputedTextLength() + 34);
            if (x > 16 && x + width > options.width - 16) { x = 16; y += 32; }
            const chip = create('g', {});
            chip.append(create('rect', {x, y, width, height: 24, rx: 12,
                fill: curve.color, 'fill-opacity': .08, stroke: curve.color, 'stroke-opacity': .4}),
            create('circle', {cx: x + 12, cy: y + 12, r: 3.5, fill: curve.color}));
            label.setAttribute('x', x + 23);
            label.setAttribute('y', y + 16);
            // Keep long expressions inside their chip at narrow preview widths.
            if (label.getComputedTextLength() > width - 34) {
                label.setAttribute('textLength', Math.max(1, width - 34));
                label.setAttribute('lengthAdjust', 'spacingAndGlyphs');
            }
            chip.append(label);
            svg.append(chip);
            x += width + 8;
        }
        height = y + 38;
        svg.setAttribute('height', height);
        const background = create('rect', {x: .5, y: .5, width: options.width - 1, height: height - 1,
            rx: 14, fill: `url(#${id}-background)`, stroke: '#233158'});
        svg.prepend(background);
    }
    svg.setAttribute('viewBox', '0 0 ' + options.width + ' ' + height);
    svg.style.cssText = language === 'plot' ? 'color:#93a0cc;max-width:100%;height:auto' : 'background:white;color:black;max-width:100%;height:auto';
    // disableZoom prevents scale changes, but D3 still consumes wheel events.
    if (options.disableZoom) svg.querySelector('.zoom-and-drag').style.pointerEvents = 'none';
    // Keep the background when the SVG is rasterised outside this document.
    if (language !== 'plot') {
        const background = document.createElementNS(svg.namespaceURI, 'rect');
        background.setAttribute('width', '100%');
        background.setAttribute('height', '100%');
        background.setAttribute('fill', 'white');
        svg.prepend(background);
    }
    return svg;
};
