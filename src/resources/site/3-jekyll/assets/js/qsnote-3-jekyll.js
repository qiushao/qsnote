/* Adapt 3-Jekyll's sidebar filters and menu to QSNote's static pages. */
(() => {
  const sidebar = document.getElementById('sidebar');
  const menu = document.getElementById('menu');
  const input = document.getElementById('search-input');
  const filters = Array.from(document.querySelectorAll('.sidebar-tag'));
  const entries = Array.from(document.querySelectorAll('#toc .toc-link')).map(link => ({
    link, tags: JSON.parse(link.dataset.tags), text: link.textContent.toLocaleLowerCase()
  }));
  let selected = null;
  let searchLoaded = false;
  const narrow = window.matchMedia('(max-width: 1280px)');
  const setOpen = open => {
    sidebar.classList.toggle('open', open);
    menu.classList.toggle('open', open);
    menu.setAttribute('aria-expanded', String(open));
    sidebar.inert = narrow.matches && !open;
  };
  const filter = () => {
    const query = input.value.trim().toLocaleLowerCase();
    let visible = 0;
    for (const entry of entries) {
      entry.link.hidden = (selected !== null && !entry.tags.includes(selected)) ||
        !(entry.text + ' ' + entry.tags.join(' ').toLocaleLowerCase()).includes(query);
      if (!entry.link.hidden) ++visible;
    }
    document.getElementById('search-empty').hidden = visible !== 0;
  };
  menu.addEventListener('click', () => setOpen(!sidebar.classList.contains('open')));
  narrow.addEventListener('change', () => setOpen(false));
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && sidebar.classList.contains('open')) {
      setOpen(false);
      menu.focus();
    }
  });
  for (const button of filters) button.addEventListener('click', () => {
    selected = button.dataset.all ? null : button.dataset.filter;
    for (const item of filters) {
      item.classList.toggle('active', item === button);
      item.setAttribute('aria-pressed', String(item === button));
    }
    filter();
  });
  input.addEventListener('input', async () => {
    filter();
    if (searchLoaded) return;
    searchLoaded = true;
    try {
      const response = await fetch(document.body.dataset.root + 'search.json');
      if (!response.ok) throw new Error('search');
      const posts = new Map((await response.json()).map(post => [post.url, post]));
      for (const entry of entries) {
        const post = posts.get(entry.link.getAttribute('href'));
        if (post) entry.text = (post.title + ' ' + post.text).toLocaleLowerCase();
      }
      filter();
    } catch {
      // Title and tag filtering also work without a fetchable search index.
      searchLoaded = false;
    }
  });
  setOpen(false);
  filter();
})();
