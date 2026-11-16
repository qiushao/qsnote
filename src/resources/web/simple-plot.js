(() => {
    'use strict';
    const constants = {pi: Math.PI, e: Math.E, tau: 2 * Math.PI, phi: (1 + Math.sqrt(5)) / 2};
    const mod = (a, b) => ((a % b) + b) % b;
    const functions = {
        ...Object.fromEntries(['sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'sinh', 'cosh', 'tanh',
            'asinh', 'acosh', 'atanh', 'sqrt', 'cbrt', 'abs', 'exp', 'log2', 'log10',
            'floor', 'ceil', 'round', 'sign'].map(name => [name, [Math[name], 1, 1]])),
        ln: [Math.log, 1, 1], log: [Math.log10, 1, 1],
        sec: [x => 1 / Math.cos(x), 1, 1], csc: [x => 1 / Math.sin(x), 1, 1],
        cot: [x => 1 / Math.tan(x), 1, 1],
        min: [Math.min, 2, 8], max: [Math.max, 2, 8],
        pow: [Math.pow, 2, 2], atan2: [Math.atan2, 2, 2], mod: [mod, 2, 2]
    };
    const operators = {
        '+': [10, (a, b) => a + b], '-': [10, (a, b) => a - b],
        '*': [20, (a, b) => a * b], '/': [20, (a, b) => a / b],
        '%': [20, mod], '^': [30, Math.pow]
    };

    // Pratt parsing keeps authored expressions separate from JavaScript execution.
    function compile(source) {
        if (source.length > 500) throw new Error('Expression exceeds 500 characters');
        const text = source.replace(/[×·]/g, '*').replace(/÷/g, '/').replace(/[−–]/g, '-')
            .replace(/π/g, 'pi').replace(/²/g, '^2').replace(/³/g, '^3').replace(/\*\*/g, '^');
        const tokens = [];
        const pattern = /\s+|(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|[a-zA-Z_][a-zA-Z_0-9]*|[+*/%^(),-]/y;
        let offset = 0;
        while (offset < text.length) {
            pattern.lastIndex = offset;
            const match = pattern.exec(text);
            if (!match) throw new Error(`Unexpected character at ${offset + 1}`);
            offset = pattern.lastIndex;
            if (!/^\s/.test(match[0])) tokens.push(match[0].toLowerCase());
        }
        if (tokens.length > 400) throw new Error('Expression is too complex');
        let index = 0, usesX = false;
        function expect(token) {
            if (tokens[index++] !== token) throw new Error(`Expected ${token}`);
        }
        function prefix() {
            const token = tokens[index++];
            if (token === undefined) throw new Error('Incomplete expression');
            if (/^[\d.]/.test(token)) {
                const value = Number(token);
                return () => value;
            }
            if (token === '+' || token === '-') {
                const value = expression(25);
                return token === '-' ? x => -value(x) : value;
            }
            if (token === '(') {
                const value = expression(0);
                expect(')');
                return value;
            }
            if (token === 'x') { usesX = true; return x => x; }
            if (Object.hasOwn(constants, token)) return () => constants[token];
            if (!Object.hasOwn(functions, token)) throw new Error(`Unknown symbol: ${token}`);
            const [fn, min, max] = functions[token];
            expect('(');
            const args = [expression(0)];
            while (tokens[index] === ',') { index++; args.push(expression(0)); }
            expect(')');
            if (args.length < min || args.length > max) throw new Error(`Invalid argument count: ${token}`);
            return x => fn(...args.map(arg => arg(x)));
        }
        function expression(min) {
            let left = prefix();
            while (index < tokens.length) {
                const token = tokens[index];
                const implicit = token === '(' || /^[\da-z_.]/.test(token);
                const operator = implicit ? '*' : token;
                const spec = operators[operator];
                if (!spec || spec[0] <= min) break;
                if (!implicit) index++;
                const right = expression(operator === '^' ? spec[0] - 1 : spec[0]);
                const previous = left;
                left = x => spec[1](previous(x), right(x));
            }
            return left;
        }
        const fn = expression(0);
        if (index !== tokens.length) throw new Error(`Unexpected token: ${tokens[index]}`);
        return {fn, usesX};
    }

    function range(source) {
        if (source.toLowerCase() === 'auto') return null;
        const parts = source.split(/\.\.|,/);
        if (parts.length !== 2) throw new Error('Expected range: a..b');
        const values = parts.map(part => {
            const value = compile(part);
            if (value.usesX) throw new Error('Range endpoints must be constants');
            return value.fn(0);
        });
        if (!values.every(Number.isFinite) || values[0] >= values[1] || !Number.isFinite(values[1] - values[0])) {
            throw new Error('Expected finite, increasing range endpoints');
        }
        return values;
    }

    window.qsnoteSimplePlotOptions = source => {
        const options = {grid: true, height: 300, disableZoom: true, data: []};
        const colors = ['#22d3ee', '#f471b5', '#a3e635', '#fbbf24', '#a78bfa', '#fb923c', '#60a5fa', '#f87171'];
        let xRange = null, yRange = null;
        source.split('\n').forEach((line, index) => {
            const text = line.trim();
            if (!text || text.startsWith('//')) return;
            try {
                const option = /^(title|x|y|height|xlabel|ylabel)\s*:\s*(.*)$/i.exec(text);
                if (option) {
                    const key = option[1].toLowerCase(), value = option[2].trim();
                    if (key === 'title') options.title = value.slice(0, 60);
                    else if (key === 'xlabel') options.xLabel = value.slice(0, 60);
                    else if (key === 'ylabel') options.yLabel = value.slice(0, 60);
                    else if (key === 'x') xRange = range(value);
                    else if (key === 'y') yRange = range(value);
                    else {
                        const height = Number(value);
                        if (!Number.isFinite(height) || height <= 0) throw new Error('Invalid height');
                        options.height = Math.max(200, Math.min(560, Math.round(height)));
                    }
                    return;
                }
                const hash = text.indexOf('#');
                const expression = (hash < 0 ? text : text.slice(0, hash)).trim();
                if (!expression) return;
                if (options.data.length === 8) throw new Error('At most 8 curves are supported');
                const {fn} = compile(expression);
                const label = hash < 0 ? expression : text.slice(hash + 1).trim() || expression;
                options.data.push({fn: scope => fn(scope.x), title: label, color: colors[options.data.length],
                    graphType: 'qsnote-plot', sampler: 'builtIn'});
            } catch (error) {
                throw new Error(`plot: line ${index + 1}: ${error.message}`);
            }
        });
        if (!options.data.length) throw new Error('plot: expected at least one function');
        xRange = xRange || [-8, 8];
        if (!yRange) {
            const values = [];
            for (const curve of options.data) {
                for (let i = 0; i <= 240; i++) {
                    const y = curve.fn({x: xRange[0] + (xRange[1] - xRange[0]) * i / 240});
                    if (Number.isFinite(y)) values.push(y);
                }
            }
            values.sort((a, b) => a - b);
            let low = values.length ? values[Math.floor(0.04 * (values.length - 1))] : -5;
            let high = values.length ? values[Math.floor(0.96 * (values.length - 1))] : 5;
            if (high - low < 1e-9) { low -= 1; high += 1; }
            const span = high - low;
            if (low > 0 && low < span * 0.7) low = 0;
            if (high < 0 && -high < span * 0.7) high = 0;
            const padding = (high - low) * 0.14;
            yRange = [low - padding, high + padding];
            if (!yRange.every(Number.isFinite) || !(yRange[1] > yRange[0]) || !Number.isFinite(yRange[1] - yRange[0])) {
                throw new Error('plot: cannot determine y range; specify y: a..b');
            }
        }
        options.xAxis = {domain: xRange};
        options.yAxis = {domain: yRange};
        return options;
    };

    // Sample each view afresh, breaking paths at undefined values and asymptotes.
    // Reuse function-plot's axes and interactions; keep the output SVG for exports.
    functionPlot.globals.graphTypes['qsnote-plot'] = chart => selection => selection.each(function(curve) {
        const {xScale, yScale, width, height} = chart.meta;
        const [xmin, xmax] = xScale.domain(), [ymin, ymax] = yScale.domain();
        const count = Math.max(64, Math.min(4000, Math.round(width * 2)));
        let path = '', previousY = null, previousPixel = null;
        for (let i = 0; i <= count; i++) {
            const x = xmin + (xmax - xmin) * i / count, y = curve.fn({x});
            if (!Number.isFinite(y)) { previousY = null; continue; }
            const pixel = yScale(y), jump = Math.abs(pixel - previousPixel);
            const split = previousY === null || jump > height * 8
                || (jump > height * 3 && previousY * y < 0
                    && Math.max(Math.abs(y), Math.abs(previousY)) > (ymax - ymin) * 2);
            path += `${split ? 'M' : 'L'}${xScale(x)},${Math.max(-height * 4, Math.min(height * 5, pixel))}`;
            previousY = y;
            previousPixel = pixel;
        }
        const element = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        element.setAttribute('class', 'line');
        element.setAttribute('d', path);
        element.setAttribute('fill', 'none');
        element.setAttribute('stroke', curve.color);
        element.setAttribute('stroke-width', '2.5');
        element.setAttribute('stroke-linecap', 'round');
        element.setAttribute('stroke-linejoin', 'round');
        this.replaceChildren(element);
    });
})();
