// Adapt Yilia's slider to QSNote's local search index without Hexo or remote scripts.
(() => {
  const container = document.querySelector('#container');
  const panel = document.querySelector('.tools-col');
  const input = panel.querySelector('.search-ipt');
  const results = panel.querySelector('.search-ul');
  const triggers = document.querySelectorAll('.search-trigger');
  let posts;
  let opener;

  function search() {
    const query = input.value.trim().toLowerCase();
    results.replaceChildren();
    for (const post of posts) {
      const tags = post.tags.map(tag => tag.name);
      const matches = query.startsWith('#')
        ? tags.some(tag => tag.toLowerCase().includes(query.slice(1)))
        : (post.title + ' ' + post.text).toLowerCase().includes(query);
      if (!matches) continue;
      const item = document.createElement('li');
      item.className = 'search-li';
      const title = document.createElement('a');
      title.className = 'search-title';
      title.href = container.dataset.root + post.path;
      title.textContent = post.title;
      const date = document.createElement('p');
      date.className = 'search-time';
      date.textContent = post.date.slice(0, 10);
      const labels = document.createElement('p');
      labels.className = 'search-tag';
      labels.textContent = tags.map(tag => '#' + tag).join(' ');
      item.append(title, date, labels);
      results.append(item);
    }
    if (!results.children.length) results.textContent = '没有找到相关文章';
  }

  async function toggle(open) {
    container.classList.toggle('show', open);
    for (const element of document.querySelectorAll('.left-col, .mid-col, .tools-col')) {
      element.classList.toggle('show', open);
      element.classList.toggle('hide', !open);
    }
    panel.setAttribute('aria-hidden', String(!open));
    triggers.forEach(trigger => trigger.setAttribute('aria-expanded', String(open)));
    if (!open) { opener?.focus(); return; }
    input.focus();
    if (!posts) {
      results.textContent = '正在加载…';
      try {
        const response = await fetch(container.dataset.root + 'content.json');
        if (!response.ok) throw new Error(response.status);
        posts = (await response.json()).posts;
      } catch {
        results.textContent = '搜索索引加载失败，请关闭后重试';
        return;
      }
    }
    search();
  }
  triggers.forEach(trigger => trigger.addEventListener('click', event => {
    event.preventDefault();
    opener = trigger;
    toggle(!container.classList.contains('show'));
  }));
  panel.querySelector('.search-close').addEventListener('click', event => { event.preventDefault(); toggle(false); });
  container.addEventListener('click', event => {
    if (container.classList.contains('show') && !event.target.closest('.tools-col, .search-trigger')) toggle(false);
  });
  document.addEventListener('keydown', event => { if (event.key === 'Escape' && container.classList.contains('show')) toggle(false); });
  input.addEventListener('input', () => { if (posts) search(); });
  const top = document.querySelector('#js-jump-container');
  const updateTop = () => { top.hidden = Math.max(container.scrollTop, window.scrollY) < 500; };
  container.addEventListener('scroll', updateTop, { passive: true });
  window.addEventListener('scroll', updateTop, { passive: true });
  top.addEventListener('click', event => {
    event.preventDefault();
    container.scrollTo({ top: 0, behavior: 'smooth' });
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });
})();
