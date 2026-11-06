(() => {
    'use strict';
    // QSNote heading links use URL-encoded fragments; Shiro's TOC expects raw IDs.
    document.querySelectorAll('.toc-link[href^="#"]').forEach(link => {
        link.dataset.target = decodeURIComponent(link.getAttribute('href').slice(1));
    });
    document.querySelectorAll('.nav-desktop-link').forEach(link => {
        if (link.pathname === location.pathname) link.setAttribute('aria-current', 'page');
    });

    const dialog = document.getElementById('shiroSearch');
    const toggle = document.getElementById('searchToggle');
    const input = document.getElementById('searchInput');
    const status = document.getElementById('searchStatus');
    const results = document.getElementById('searchResults');
    let entries;
    let loading;

    async function search() {
        const query = input.value.trim().toLocaleLowerCase();
        results.replaceChildren();
        if (!query) { status.textContent = '输入关键词开始搜索'; return; }
        status.textContent = '正在搜索…';
        try {
            if (!entries) {
                if (!loading) loading = fetch(dialog.dataset.index).then(response => {
                    if (!response.ok) throw new Error('Search index unavailable');
                    return response.json();
                });
                entries = await loading;
            }
            if (input.value.trim().toLocaleLowerCase() !== query || !dialog.open) return;
            const words = query.split(/\s+/);
            const matches = entries.filter(entry => words.every(word => (entry.title + ' ' + entry.text).toLocaleLowerCase().includes(word)));
            status.textContent = matches.length ? `找到 ${matches.length} 个结果` : '没有找到匹配的文章';
            for (const entry of matches.slice(0, 50)) {
                const item = document.createElement('li');
                const link = document.createElement('a');
                link.href = entry.url;
                link.textContent = entry.title;
                const excerpt = document.createElement('p');
                excerpt.textContent = entry.text.slice(0, 160);
                item.append(link, excerpt);
                results.append(item);
            }
        } catch (_) {
            loading = undefined;
            if (input.value.trim().toLocaleLowerCase() === query) status.textContent = '搜索数据加载失败，请重新输入以重试';
        }
    }
    toggle.addEventListener('click', () => {
        dialog.showModal();
        document.documentElement.dataset.modalOpen = 'true';
        toggle.setAttribute('aria-expanded', 'true');
        input.focus();
        search();
    });
    dialog.addEventListener('close', () => {
        delete document.documentElement.dataset.modalOpen;
        toggle.setAttribute('aria-expanded', 'false');
        toggle.focus();
    });
    input.addEventListener('input', search);
    toggle.disabled = false;
})();
