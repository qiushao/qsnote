'use strict';
window.qsnotePlot = (host, source, availableWidth, interactive) => {
    // Accept the separator comma when copying a configuration from an example list.
    const options = JSON5.parse(source.replace(/,\s*$/, ''));
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
    svg.setAttribute('viewBox', '0 0 ' + options.width + ' ' + options.height);
    svg.style.cssText = 'background:white;color:black;max-width:100%;height:auto';
    // disableZoom prevents scale changes, but D3 still consumes wheel events.
    if (options.disableZoom) svg.querySelector('.zoom-and-drag').style.pointerEvents = 'none';
    // Keep the background when the SVG is rasterised outside this document.
    const background = document.createElementNS(svg.namespaceURI, 'rect');
    background.setAttribute('width', '100%');
    background.setAttribute('height', '100%');
    background.setAttribute('fill', 'white');
    svg.prepend(background);
    return svg;
};
