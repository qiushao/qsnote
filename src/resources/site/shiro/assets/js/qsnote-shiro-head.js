(() => {
    'use strict';
    const html = document.documentElement;
    html.classList.add('js');
    let state = 'system';
    try { state = localStorage.getItem('theme') || state; } catch (_) {}
    if (!['system', 'light', 'dark'].includes(state)) state = 'system';
    html.dataset.themeState = state;
    const dark = state === 'dark' || (state === 'system' && matchMedia('(prefers-color-scheme: dark)').matches);
    html.dataset.theme = dark ? 'dark' : 'light';
    html.style.colorScheme = html.dataset.theme;
})();
