/* QSNote adapter: native controls retain navigation without JavaScript. */
(() => {
  const root = document.documentElement;
  root.classList.add('js');
  const preference = matchMedia('(prefers-color-scheme: dark)');
  let saved;
  try { saved = localStorage.getItem('qsnote-plume-theme'); } catch (_) {}
  root.dataset.theme = saved || (preference.matches ? 'dark' : 'light');
  const theme = document.querySelector('#theme-toggle');
  theme.hidden = false;
  theme.setAttribute('aria-checked', String(root.dataset.theme === 'dark'));
  theme.addEventListener('click', () => {
    root.dataset.theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
    theme.setAttribute('aria-checked', String(root.dataset.theme === 'dark'));
    try { localStorage.setItem('qsnote-plume-theme', root.dataset.theme); } catch (_) {}
  });
  const reading = document.querySelector('#reading-toggle');
  reading.hidden = false;
  reading.addEventListener('click', () => {
    const enabled = document.body.classList.toggle('reading-mode');
    const label = enabled ? '退出全屏阅读' : '全屏阅读';
    reading.setAttribute('aria-pressed', String(enabled));
    reading.setAttribute('aria-label', label);
    reading.title = label;
  });
  document.querySelectorAll('.code-copy').forEach(button => {
    button.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(button.closest('.code-block').querySelector('code').textContent);
        button.textContent = '已复制';
      } catch (_) { button.textContent = '请手动复制'; }
      setTimeout(() => { button.textContent = '复制'; }, 1500);
    });
  });
  const menu = document.querySelector('.vp-menu');
  const sidebar = document.querySelector('.vp-sidebar');
  const compact = matchMedia('(max-width: 959px)');
  const updateLayout = () => {
    menu.open = !compact.matches;
    if (sidebar) sidebar.open = !compact.matches;
  };
  compact.addEventListener('change', updateLayout);
  updateLayout();
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape') {
      document.querySelectorAll('.vp-nav-group[open]').forEach(group => { group.open = false; });
      document.querySelector('#search-dialog').close();
      if (compact.matches) menu.open = false;
    }
  });
  document.querySelectorAll('.vp-nav-group').forEach(group => {
    group.addEventListener('focusout', () => setTimeout(() => { if (!group.contains(document.activeElement)) group.open = false; }, 0));
  });
  const dialog = document.querySelector('#search-dialog');
  const toggle = document.querySelector('#search-toggle');
  const input = document.querySelector('#search-input');
  const results = document.querySelector('#search-results');
  const status = document.querySelector('#search-status');
  let entries = [], loading;
  const search = () => {
    results.replaceChildren();
    const terms = input.value.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
    if (!terms.length) { status.textContent = '输入关键词搜索'; return; }
    const matches = entries.filter(entry => terms.every(term => (entry.title + ' ' + entry.text).toLocaleLowerCase().includes(term)));
    status.textContent = `找到 ${matches.length} 个结果`;
    for (const entry of matches.slice(0, 50)) {
      const item = document.createElement('li');
      const link = document.createElement('a');
      link.href = entry.url;
      link.textContent = entry.title;
      const detail = document.createElement('small');
      detail.textContent = (entry.type === 'doc' ? '文档 · ' + entry.collection : '博客') + ' — ' + entry.text.slice(0, 100);
      link.append(detail); item.append(link); results.append(item);
    }
  };
  toggle.hidden = false;
  toggle.addEventListener('click', async () => {
    dialog.showModal(); input.focus();
    status.textContent = '正在加载…';
    try {
      loading ||= fetch(document.body.dataset.root + 'search.json').then(response => { if (!response.ok) throw new Error(); return response.json(); });
      entries = await loading; search();
    } catch (_) { loading = null; status.textContent = '搜索加载失败，请重试。'; }
  });
  input.addEventListener('input', search);
  document.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') { event.preventDefault(); toggle.click(); }
  });
  const profile = document.querySelector('#profile-dialog');
  const profileToggle = document.querySelector('.vp-posts-extract');
  if (profile && profileToggle) {
    profileToggle.hidden = false;
    profileToggle.addEventListener('click', () => { profile.showModal(); profileToggle.setAttribute('aria-expanded', 'true'); });
    profile.addEventListener('close', () => { profileToggle.setAttribute('aria-expanded', 'false'); profileToggle.focus(); });
    profile.addEventListener('click', event => {
      const bounds = profile.getBoundingClientRect();
      if (event.clientY < bounds.top) profile.close();
    });
    matchMedia('(min-width: 768px)').addEventListener('change', event => { if (event.matches) profile.close(); });
  }
  if ('IntersectionObserver' in window) {
    const links = [...document.querySelectorAll('.vp-outline a')];
    const observer = new IntersectionObserver(items => {
      const visible = items.find(item => item.isIntersecting);
      if (!visible) return;
      links.forEach(link => link.classList.toggle('active', decodeURIComponent(link.hash.slice(1)) === visible.target.id));
    }, { rootMargin: '-80px 0px -60% 0px' });
    document.querySelectorAll('.vp-doc h1[id],.vp-doc h2[id],.vp-doc h3[id],.vp-doc h4[id]').forEach(heading => observer.observe(heading));
  }
})();
