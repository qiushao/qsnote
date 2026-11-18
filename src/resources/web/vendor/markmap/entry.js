import { parse } from '@markmap/parse';
import { createCanvasMeasurer, renderTreeAsync, resolveRenderOptions, toSVGString } from '@markmap/render';
import { defaultGestures, Markmap, resolveViewOptions } from '@markmap/view';

export async function snapshot(host, source) {
    const { root, frontmatter } = parse(source);
    const { options } = resolveRenderOptions({ measurer: createCanvasMeasurer() }, frontmatter.markmap);
    const result = await renderTreeAsync(root, options);
    host.innerHTML = toSVGString(result);
}

export function preview(host, source) {
    const { root, frontmatter } = parse(source);
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.style.cssText = 'display:block;width:100%;height:320px';
    host.append(svg);
    const { options } = resolveViewOptions({}, frontmatter.markmap);
    return Markmap.create(svg, root, {
        ...options,
        zoom: false,
        gestures(map) {
            // Leave touch scrolling and pinching to the article, while keeping mouse dragging.
            map.zoomBehavior.touchable(false);
            return defaultGestures(map);
        },
    });
}
