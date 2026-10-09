/* ============================================================
 * MDNote 原型 —— 三列式 Markdown 笔记软件
 * 左：笔记本管理（普通 / 静态站点 / 电子书）
 * 中：Tab 多开 + 编辑/预览切换 + Markdown 工具栏
 * 右：自动生成大纲
 * ============================================================ */

marked.setOptions({ breaks: true, gfm: true });

/* ---------------- 基础工具 ---------------- */
const nid = () => 'id-' + Math.random().toString(36).slice(2, 10);
const today = () => new Date().toISOString().slice(0, 10);
const $ = s => document.querySelector(s);
const esc = s => String(s ?? '').replace(/[&<>"']/g, c =>
  ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const selOpts = (pairs, cur) => pairs.map(([v, l]) =>
  `<option value="${v}" ${v === cur ? 'selected' : ''}>${l}</option>`).join('');

/* ---------------- 数据模型 ----------------
 * 普通笔记本：目录树节点 { id, name, type:'dir'|'file', content?, children?, order }
 * （order 即「元数据文件」记录的排序信息，原型中随节点写入 localStorage）
 * 静态站点：config（Gridea 式站点配置）+ posts[]
 * 电子书：chapters[]（对应 SUMMARY.md 的层级结构）
 */
const fileNode = (name, content, extra = {}) => ({ id: nid(), name, type: 'file', content, order: 0, ...extra });
const dirNode = (name, children = []) => ({ id: nid(), name, type: 'dir', children, order: 0 });
const postNode = (title, extra = {}) => ({
  id: nid(), title, slug: 'post-' + Date.now().toString(36) + Math.floor(Math.random() * 99),
  date: today(), category: '', tags: [], content: `# ${title}\n\n`, ...extra,
});
const chapterNode = (title, file, extra = {}) => ({ id: nid(), title, file, children: [], content: `# ${title}\n\n`, ...extra });

const defaultSiteConfig = title => ({
  title, description: '', author: '', domain: 'https://example.com',
  theme: 'minimal', footer: 'Powered by MDNote',
  menu: [{ name: '首页', url: '/' }, { name: '归档', url: '/archives' }, { name: '关于', url: '/about' }],
  about: '# 关于\n',
  comment: { platform: 'none', owner: '', repo: '', clientId: '', clientSecret: '' },
  links: [],
  deploy: { platform: 'github-pages', repo: '', branch: 'gh-pages', token: '' },
});

function defaultData() {
  return [
    {
      id: nid(), name: '个人笔记', type: 'normal',
      tree: [
        fileNode('收件箱.md', '# 收件箱\n\n随手记录的想法。\n\n## 待整理\n\n- 原型设计要点\n- 周报素材\n'),
        dirNode('技术', [
          fileNode('JavaScript 笔记.md', '# JavaScript 笔记\n\n## 闭包\n\n函数捕获其词法作用域。\n\n```js\nconst counter = () => { let n = 0; return () => ++n; };\n```\n\n## 原型链\n\n对象通过 `__proto__` 链接。\n'),
        ]),
        fileNode('读书记录.md', '# 读书记录\n\n| 书名 | 状态 |\n| --- | --- |\n| 《重构》 | 在读 |\n\n> 任何傻瓜都能写出计算机能理解的代码。\n'),
      ],
    },
    {
      id: nid(), name: '我的博客', type: 'site',
      config: {
        ...defaultSiteConfig('我的博客'),
        description: '记录技术与生活', author: 'qiushao',
        about: '# 关于我\n\n一名开发者，喜欢写作与开源。',
        links: [{ name: 'MDNote', url: 'https://example.com' }],
      },
      posts: [
        postNode('Hello World', { slug: 'hello-world', category: '随笔', tags: ['开始'],
          content: '# Hello World\n\n这是站点的第一篇文章。\n\n```js\nconsole.log("hello");\n```\n' }),
        postNode('Markdown 写作指南', { slug: 'markdown-guide', category: '技术', tags: ['markdown'],
          content: '# Markdown 写作指南\n\n## 表格\n\n| 语法 | 效果 |\n| --- | --- |\n| `**b**` | 粗体 |\n\n> 保持简洁。\n' }),
      ],
    },
    {
      id: nid(), name: '前端入门手册', type: 'ebook',
      chapters: [
        chapterNode('简介', 'README.md', { content: '# 前端入门手册\n\n面向初学者的前端电子书。\n' }),
        chapterNode('第 1 章 HTML 基础', 'chapter-1.md', {
          content: '# 第 1 章 HTML 基础\n\nHTML 是页面的骨架。\n',
          children: [chapterNode('1.1 常用标签', 'chapter-1/1-1-tags.md', { content: '# 1.1 常用标签\n\n## 文本标签\n\n`p` `h1`-`h6` `span`\n' })],
        }),
        chapterNode('第 2 章 CSS 基础', 'chapter-2.md', { content: '# 第 2 章 CSS 基础\n\n## 盒模型\n\ncontent → padding → border → margin。\n' }),
      ],
    },
  ];
}

/* ---------------- 状态 ---------------- */
const NB_TYPE_LABEL = { normal: '普通笔记本', site: '静态站点', ebook: '电子书' };
const STORE_KEY = 'mdnote-data-v2';

const state = {
  notebooks: load() || defaultData(),
  activeNotebookId: null,
  tabs: [],            // { id, notebookId, noteId, title, dirty }
  activeTabId: null,
  mode: 'edit',        // edit | preview
  collapsedDirs: new Set(),
  searchQuery: '',
  siteGroupBy: 'category',  // category | tag | date
};
state.activeNotebookId = state.notebooks[0]?.id;

function save() {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(state.notebooks)); } catch (e) {}
}
function load() {
  try {
    const data = JSON.parse(localStorage.getItem(STORE_KEY));
    return Array.isArray(data) && data.length ? data : null;
  } catch (e) { return null; }
}

/* ---------------- 数据访问 ---------------- */
const activeNotebook = () => state.notebooks.find(n => n.id === state.activeNotebookId);

function findInTree(tree, id, parent = null) {
  for (const node of tree) {
    if (node.id === id) return { node, parent };
    if (node.type === 'dir') {
      const r = findInTree(node.children, id, node);
      if (r) return r;
    }
  }
  return null;
}

function walkChapters(list, fn) {
  list.forEach(c => { fn(c); walkChapters(c.children, fn); });
}

/** 统一按 id 查找笔记（三种笔记本结构不同） */
function findNote(nb, noteId) {
  if (!nb) return null;
  if (nb.type === 'normal') return findInTree(nb.tree, noteId)?.node || null;
  if (nb.type === 'site') return nb.posts.find(p => p.id === noteId) || null;
  let found = null;
  walkChapters(nb.chapters, c => { if (c.id === noteId) found = c; });
  return found;
}

const noteTitle = (nb, note) => !note ? '?' : nb.type === 'normal' ? note.name : note.title;

/* ---------------- DOM 引用 ---------------- */
const el = {
  nbCurrent: $('#nb-current'), nbList: $('#nb-list'),
  leftBody: $('#left-body'),
  tabBar: $('#tab-bar'), toolbar: $('#toolbar'),
  editor: $('#editor'), preview: $('#preview'),
  siteConfigMask: $('#site-config-mask'), siteConfigBody: $('#site-config-body'),
  empty: $('#empty-state'),
  outline: $('#outline-body'), status: $('#status-text'),
  exportBtn: $('#export-btn'), previewBtn: $('#preview-site-btn'), siteConfigBtn: $('#site-config-btn'),
  previewOverlay: $('#preview-overlay'), previewFrame: $('#preview-frame'),
  searchInput: $('#search-input'), postMetaBar: $('#post-meta-bar'),
};

const svg = d => `<svg viewBox="0 0 16 16" width="14" height="14"><path fill="currentColor" d="${d}"/></svg>`;
const ICON = {
  file: svg('M3 1h7l3 3v11H3V1zm6 1H4v12h8V5H9V2z'),
  dir: svg('M1 3h5l2 2h7v8H1V3z'),
  book: svg('M2 1h11a1 1 0 011 1v12a1 1 0 01-1 1H2V1zm2 2v2h8V3H4z'),
  post: svg('M3 1h10v14H3V1zm2 3h6v1.5H5V4zm0 3h6v1.5H5V7zm0 3h4v1.5H5V10z'),
};

/* ============================================================
 * 左栏渲染
 * ============================================================ */
function renderNotebookSelect() {
  const nb = activeNotebook();
  el.nbCurrent.querySelector('.nb-name').textContent = nb?.name || '无笔记本';
  el.nbList.innerHTML = state.notebooks.map(n => `
    <div class="nb-item ${n.id === state.activeNotebookId ? 'active' : ''}" data-nb="${n.id}">
      <span class="nb-item-name">${esc(n.name)}</span>
      <span class="nb-type">${NB_TYPE_LABEL[n.type]}</span>
    </div>`).join('');
  el.exportBtn.classList.toggle('hidden', !nb || nb.type === 'normal');
  el.exportBtn.textContent = nb?.type === 'site' ? '导出站点' : nb?.type === 'ebook' ? '导出电子书' : '导出';
  el.previewBtn.classList.toggle('hidden', nb?.type !== 'site');
  el.siteConfigBtn.classList.toggle('hidden', nb?.type !== 'site');
}

function renderLeft() {
  renderNotebookSelect();
  const nb = activeNotebook();
  if (!nb) { el.leftBody.innerHTML = ''; return; }
  if (state.searchQuery) { renderSearchResults(nb, state.searchQuery); return; }
  if (nb.type === 'normal') renderNormalTree(nb);
  else if (nb.type === 'site') renderSitePanel(nb);
  else renderEbookPanel(nb);
}

/* ----- 全文搜索（当前笔记本，匹配标题 + 正文） ----- */
function highlightText(raw, q) {
  const idx = raw.toLowerCase().indexOf(q.toLowerCase());
  if (idx < 0) return esc(raw);
  return esc(raw.slice(0, idx)) + '<mark>' + esc(raw.slice(idx, idx + q.length)) + '</mark>' + esc(raw.slice(idx + q.length));
}

function makeSnippet(content, q) {
  const text = (content || '').replace(/\s+/g, ' ').trim();
  const idx = text.toLowerCase().indexOf(q.toLowerCase());
  if (idx < 0) return esc(text.slice(0, 60));
  const start = Math.max(0, idx - 24);
  const end = Math.min(text.length, idx + q.length + 40);
  return (start > 0 ? '…' : '') + esc(text.slice(start, idx)) +
    '<mark>' + esc(text.slice(idx, idx + q.length)) + '</mark>' +
    esc(text.slice(idx + q.length, end)) + (end < text.length ? '…' : '');
}

function searchNotes(nb, q) {
  const lq = q.toLowerCase();
  const results = [];
  const push = (id, name, content, kind) => {
    if (!name.toLowerCase().includes(lq) && !(content || '').toLowerCase().includes(lq)) return;
    results.push({ id, name, kind, snippet: makeSnippet(content, q) });
  };
  if (nb.type === 'normal') {
    (function walk(nodes) {
      nodes.forEach(n => n.type === 'file' ? push(n.id, n.name, n.content, 'file') : walk(n.children));
    })(nb.tree);
  } else if (nb.type === 'site') {
    nb.posts.forEach(p => push(p.id, p.title, p.content, 'post'));
  } else {
    walkChapters(nb.chapters, c => push(c.id, c.title, c.content, 'chapter'));
  }
  return results;
}

function renderSearchResults(nb, q) {
  const results = searchNotes(nb, q);
  const iconOf = { file: ICON.file, post: ICON.post, chapter: ICON.book };
  el.leftBody.innerHTML = `<div class="search-count">“${esc(q)}” 共 ${results.length} 个结果</div>` +
    (results.map(r => `
      <div class="search-hit">
        <div class="tree-row" data-id="${r.id}" data-kind="${r.kind}">
          <span class="node-icon">${iconOf[r.kind]}</span>
          <span class="row-name">${highlightText(r.name, q)}</span>
        </div>
        ${r.snippet ? `<div class="search-snippet">${r.snippet}</div>` : ''}
      </div>`).join('') || '<div class="outline-empty">无匹配结果</div>');
}

/* ----- 普通笔记本：目录树（拖拽排序写回 order 元数据） ----- */
function sortNodes(nodes) {
  return [...nodes].sort((a, b) => (a.order - b.order) || a.name.localeCompare(b.name, 'zh'));
}

const treeRow = ({ id, kind, icon, name, active, twisty, extra = '' }) => `
  <div class="tree-row ${active ? 'active' : ''}" data-id="${id}" data-kind="${kind}" ${extra}>
    <span class="twisty">${twisty || ''}</span>
    <span class="node-icon">${icon}</span>
    <span class="row-name">${name}</span>
  </div>`;

function renderNormalTree(nb) {
  const build = (nodes) => sortNodes(nodes).map(node => {
    if (node.type === 'dir') {
      const collapsed = state.collapsedDirs.has(node.id);
      return `<div class="tree-node">
        <div class="tree-row" data-id="${node.id}" data-kind="dir" draggable="true">
          <span class="twisty" data-action="toggle-dir" data-id="${node.id}">${collapsed ? '▶' : '▼'}</span>
          <span class="node-icon">${ICON.dir}</span>
          <span class="row-name">${esc(node.name)}</span>
        </div>
        ${collapsed ? '' : `<div class="tree-children">${build(node.children)}</div>`}
      </div>`;
    }
    return treeRow({ id: node.id, kind: 'file', icon: ICON.file, name: esc(node.name),
      active: isActiveNote(nb.id, node.id), extra: 'draggable="true"' });
  }).join('');
  el.leftBody.innerHTML = build(nb.tree) || '<div class="outline-empty">暂无内容，右键新建</div>';
}

/* ----- 静态站点：分组文章树 ----- */
function renderSitePanel(nb) {
  const groupBy = state.siteGroupBy;
  const groups = new Map();
  const addTo = (key, p) => {
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(p);
  };
  nb.posts.forEach(p => {
    if (groupBy === 'category') addTo(p.category || '未分类', p);
    else if (groupBy === 'tag') (p.tags?.length ? p.tags : ['无标签']).forEach(t => addTo(t, p));
    else addTo(p.date ? p.date.slice(0, 7) : '无日期', p);
  });
  const keys = [...groups.keys()].sort((a, b) =>
    groupBy === 'date' ? b.localeCompare(a) : a.localeCompare(b, 'zh'));

  const postRow = p => `
        <div class="tree-row ${isActiveNote(nb.id, p.id) ? 'active' : ''}" data-id="${p.id}" data-kind="post">
          <span class="twisty"></span>
          <span class="node-icon">${ICON.post}</span>
          <span class="row-name">${esc(p.title)}</span>
          ${groupBy !== 'category' && p.category ? `<span class="post-cat">${esc(p.category)}</span>` : ''}
        </div>`;

  el.leftBody.innerHTML = `
    <div class="site-filters">
      <select data-groupby title="文章分组方式">
        ${selOpts([['category', '按分类'], ['tag', '按标签'], ['date', '按日期']], groupBy)}
      </select>
    </div>
    ${keys.map(k => {
      const gid = `grp:${groupBy}:${k}`;
      const collapsed = state.collapsedDirs.has(gid);
      const posts = sortNodes(groups.get(k));
      return `<div class="tree-node">
        <div class="tree-row" data-kind="group" data-id="${esc(gid)}">
          <span class="twisty" data-action="toggle-dir" data-id="${esc(gid)}">${collapsed ? '▶' : '▼'}</span>
          <span class="node-icon">${ICON.dir}</span>
          <span class="row-name">${esc(k)}（${posts.length}）</span>
        </div>
        ${collapsed ? '' : `<div class="tree-children">${posts.map(postRow).join('')}</div>`}
      </div>`;
    }).join('') || '<div class="outline-empty">暂无文章，右键新建</div>'}`;
}

/* ----- 电子书：SUMMARY.md 章节树 ----- */
function renderEbookPanel(nb) {
  const build = (list, depth) => list.map(c => `
    <div class="tree-node">
      <div class="tree-row ${isActiveNote(nb.id, c.id) ? 'active' : ''}" data-id="${c.id}" data-kind="chapter" style="padding-left:${6 + depth * 14}px">
        <span class="node-icon">${ICON.book}</span>
        <span class="row-name">${esc(c.title)}</span>
      </div>
      ${build(c.children, depth + 1)}
    </div>`).join('');
  el.leftBody.innerHTML = build(nb.chapters, 0) || '<div class="outline-empty">暂无章节，右键添加</div>';
}

/* ============================================================
 * Tab 与编辑区
 * ============================================================ */
const activeTab = () => state.tabs.find(t => t.id === state.activeTabId) || null;

function activeNote() {
  const t = activeTab();
  return t ? findNote(state.notebooks.find(n => n.id === t.notebookId), t.noteId) : null;
}

function isActiveNote(notebookId, noteId) {
  const t = activeTab();
  return !!t && t.notebookId === notebookId && t.noteId === noteId;
}

function openNote(notebookId, noteId) {
  let tab = state.tabs.find(t => t.notebookId === notebookId && t.noteId === noteId);
  if (!tab) {
    const nb = state.notebooks.find(n => n.id === notebookId);
    tab = { id: nid(), notebookId, noteId, title: noteTitle(nb, findNote(nb, noteId)), dirty: false };
    state.tabs.push(tab);
  }
  state.activeTabId = tab.id;
  state.mode = 'edit';
  renderAll();
}

function openSiteConfig(notebookId) {
  const nb = state.notebooks.find(n => n.id === notebookId);
  if (!nb) return;
  renderSiteConfigDialog(nb);
  el.siteConfigMask.classList.remove('hidden');
}
const closeSiteConfig = () => el.siteConfigMask.classList.add('hidden');

function renderTabs() {
  el.tabBar.innerHTML = state.tabs.map(t => `
    <div class="tab ${t.id === state.activeTabId ? 'active' : ''}" data-tab="${t.id}">
      ${t.dirty ? '<span class="dirty-dot"></span>' : ''}
      <span class="tab-title">${esc(t.title)}</span>
      <button class="tab-close" data-close="${t.id}" title="关闭">×</button>
    </div>`).join('');
}

function renderEditor() {
  const t = activeTab();
  const nbOfTab = t ? state.notebooks.find(n => n.id === t.notebookId) : null;
  const isSitePost = !!t && nbOfTab?.type === 'site';

  el.empty.classList.toggle('hidden', !!t);
  el.editor.classList.toggle('hidden', !t || state.mode !== 'edit');
  el.preview.classList.toggle('hidden', !t || state.mode !== 'preview');
  el.toolbar.classList.toggle('hidden', !t);
  el.postMetaBar.classList.toggle('hidden', !isSitePost);

  if (isSitePost) renderPostMetaBar(nbOfTab, t);
  else delete el.postMetaBar.dataset.noteId;

  if (t) {
    const note = activeNote();
    if (note && el.editor.dataset.noteId !== t.noteId) {
      el.editor.value = note.content;
      el.editor.dataset.noteId = t.noteId;
    }
    if (state.mode === 'preview') renderPreview();
    updateToolbarMode();
    updateStatus();
  } else {
    delete el.editor.dataset.noteId;
    el.status.textContent = '';
  }
}

function renderPreview() {
  const note = activeNote();
  if (!note) return;
  el.preview.innerHTML = marked.parse(note.content || '');
  el.preview.querySelectorAll('pre code').forEach(b => hljs.highlightElement(b));
}

function updateStatus() {
  const note = activeNote();
  if (!note) return;
  const text = note.content || '';
  el.status.textContent = `${text.length} 字 · ${text.split('\n').length} 行 · ${state.mode === 'edit' ? '编辑中' : '预览'}`;
}

/* ---------------- 站点文章元数据条（分类 / 标签 / 日期 / Slug） ---------------- */
function renderPostMetaBar(nb, tab) {
  if (el.postMetaBar.dataset.noteId === tab.noteId) return;
  const p = findNote(nb, tab.noteId);
  if (!p) return;
  el.postMetaBar.dataset.noteId = tab.noteId;
  el.postMetaBar.innerHTML = `
    <label>分类 <input data-pm="category" value="${esc(p.category || '')}" placeholder="未分类"></label>
    <label>标签 <input data-pm="tags" value="${esc((p.tags || []).join(', '))}" placeholder="逗号分隔"></label>
    <label>日期 <input data-pm="date" type="date" value="${esc(p.date || '')}"></label>
    <label>Slug <input data-pm="slug" value="${esc(p.slug || '')}"></label>`;
}

el.postMetaBar.addEventListener('input', e => {
  const key = e.target.dataset.pm;
  if (!key) return;
  const p = activeNote();
  if (!p) return;
  if (key === 'tags') p.tags = e.target.value.split(/[,，]/).map(s => s.trim()).filter(Boolean);
  else p[key] = e.target.value;
  save();
  renderLeft(); // 刷新分组与文章计数
});

/* ---------------- 站点配置弹窗 ---------------- */
function renderSiteConfigDialog(nb) {
  const c = nb.config;
  // 兼容旧数据
  if (!c.comment) c.comment = { platform: 'none', owner: '', repo: '', clientId: '', clientSecret: '' };
  if (!c.links) c.links = [];
  if (!c.deploy) c.deploy = { platform: 'github-pages', repo: '', branch: 'gh-pages', token: '' };
  const cfgInput = (label, key, val) =>
    `<div class="cfg-field"><label>${label}</label><input data-cfg="${key}" value="${esc(val ?? '')}"></div>`;
  const cfgInput2 = (obj, key, label, val) =>
    `<div class="cfg-field"><label>${label}</label><input data-cfg2="${obj}" data-k="${key}" value="${esc(val ?? '')}"></div>`;

  el.siteConfigBody.innerHTML = `
    <div class="cfg-section">
      <h3>基本信息</h3>
      ${cfgInput('站点标题', 'title', c.title)}
      ${cfgInput('站点描述', 'description', c.description)}
      ${cfgInput('作者', 'author', c.author)}
      ${cfgInput('域名', 'domain', c.domain)}
    </div>
    <div class="cfg-section">
      <h3>评论</h3>
      <div class="cfg-field"><label>评论平台</label>
        <select data-cfg2="comment" data-k="platform">
          ${selOpts([['none', '关闭'], ['gitalk', 'Gitalk'], ['disqus', 'Disqus']], c.comment.platform)}
        </select>
      </div>
      ${cfgInput2('comment', 'owner', 'GitHub 用户', c.comment.owner)}
      ${cfgInput2('comment', 'repo', '评论仓库', c.comment.repo)}
      ${cfgInput2('comment', 'clientId', 'Client ID', c.comment.clientId)}
      ${cfgInput2('comment', 'clientSecret', 'Client Secret', c.comment.clientSecret)}
    </div>
    <div class="cfg-section">
      <h3>友链</h3>
      ${c.links.map((l, i) => `
        <div class="cfg-menu-row">
          <input data-cfg-link="${i}" data-k="name" value="${esc(l.name)}" placeholder="名称">
          <input data-cfg-link="${i}" data-k="url" value="${esc(l.url)}" placeholder="https://">
          <button class="btn" data-action="link-del" data-i="${i}">删除</button>
        </div>`).join('')}
      <button class="btn" data-action="link-add">＋ 添加友链</button>
    </div>
    <div class="cfg-section">
      <h3>主题与页脚</h3>
      <div class="cfg-field"><label>主题</label>
        <select data-cfg="theme">${selOpts(['minimal', 'notes', 'tech'].map(t => [t, t]), c.theme)}</select>
      </div>
      ${cfgInput('页脚文字', 'footer', c.footer)}
    </div>
    <div class="cfg-section">
      <h3>发布配置</h3>
      <div class="cfg-field"><label>发布平台</label>
        <select data-cfg2="deploy" data-k="platform">
          ${selOpts([['github-pages', 'GitHub Pages'], ['coding', 'Coding Pages'], ['gitee', 'Gitee Pages']], c.deploy.platform)}
        </select>
      </div>
      ${cfgInput2('deploy', 'repo', '仓库', c.deploy.repo)}
      ${cfgInput2('deploy', 'branch', '分支', c.deploy.branch)}
      <div class="cfg-field"><label>Token</label><input type="password" data-cfg2="deploy" data-k="token" value="${esc(c.deploy.token)}" placeholder="访问令牌"></div>
      <div class="dim" style="font-size:12px;margin-top:4px">配置随 config.json 导出；使用顶栏「导出站点」生成静态文件后按此配置部署。</div>
    </div>`;
}

/* ============================================================
 * 工具栏
 * ============================================================ */
const TB = [
  { label: 'H1', before: '# ', block: true, ph: '标题' },
  { label: 'H2', before: '## ', block: true, ph: '标题' },
  { label: 'H3', before: '### ', block: true, ph: '标题' },
  'sep',
  { label: 'B', title: '粗体', before: '**', after: '**', ph: '粗体' },
  { label: 'I', title: '斜体', before: '*', after: '*', ph: '斜体' },
  { label: 'S', title: '删除线', before: '~~', after: '~~', ph: '删除线' },
  'sep',
  { label: '链接', before: '[', after: '](https://)', ph: '链接文字' },
  { label: '图片', before: '![', after: '](https://image.png)', ph: '图片描述' },
  'sep',
  { label: '列表', before: '- ', block: true, ph: '列表项' },
  { label: '有序', before: '1. ', block: true, ph: '列表项' },
  { label: '任务', before: '- [ ] ', block: true, ph: '任务项' },
  { label: '引用', before: '> ', block: true, ph: '引用内容' },
  'sep',
  { label: '代码', before: '`', after: '`', ph: 'code', mono: true },
  { label: '代码块', tmpl: '```js\n代码\n```\n', block: true, mono: true },
  { label: '表格', tmpl: '| 列1 | 列2 | 列3 |\n| --- | --- | --- |\n| 内容 | 内容 | 内容 |\n', block: true },
  { label: '分割线', tmpl: '\n---\n', block: true },
];

function renderToolbar() {
  el.toolbar.innerHTML = TB.map((item, i) =>
    item === 'sep' ? '<span class="tb-sep"></span>'
      : `<button class="tb-btn ${item.mono ? 'mono' : ''}" data-tb="${i}" title="${item.title || item.label}">${item.label}</button>`
  ).join('') + `
    <span class="mode-toggle">
      <button data-mode="edit" class="${state.mode === 'edit' ? 'active' : ''}">编辑</button>
      <button data-mode="preview" class="${state.mode === 'preview' ? 'active' : ''}">预览</button>
    </span>`;
}
function updateToolbarMode() {
  el.toolbar.querySelectorAll('.mode-toggle button').forEach(b =>
    b.classList.toggle('active', b.dataset.mode === state.mode));
}

function applyToolbar(i) {
  const item = TB[i];
  if (!item || item === 'sep') return;
  const ta = el.editor;
  const start = ta.selectionStart, end = ta.selectionEnd;
  const val = ta.value;
  const selected = val.slice(start, end);

  let insert, cursorOffset;
  if (item.tmpl !== undefined) {
    insert = (item.block && start > 0 && val[start - 1] !== '\n' ? '\n' : '') + item.tmpl;
    cursorOffset = insert.length;
  } else {
    insert = item.before + (selected || item.ph || '') + (item.after || '');
    if (item.block && start > 0 && val[start - 1] !== '\n') insert = '\n' + insert;
    cursorOffset = selected ? insert.length : item.before.length;
  }
  ta.value = val.slice(0, start) + insert + val.slice(end);
  ta.focus();
  ta.selectionStart = ta.selectionEnd = start + cursorOffset;
  ta.dispatchEvent(new Event('input'));
}

/* ============================================================
 * 大纲
 * ============================================================ */
function parseOutline(md) {
  const items = [];
  let inCode = false;
  (md || '').split('\n').forEach((line, idx) => {
    if (/^```/.test(line.trim())) inCode = !inCode;
    if (inCode) return;
    const m = line.match(/^(#{1,6})\s+(.+)/);
    if (m) items.push({ level: m[1].length, text: m[2].trim(), line: idx });
  });
  return items;
}

function renderOutline() {
  const t = activeTab();
  const items = t ? parseOutline(activeNote()?.content) : [];
  if (!t || !items.length) {
    el.outline.innerHTML = `<div class="outline-empty">${t ? '本文暂无标题' : '打开一篇笔记后自动生成大纲'}</div>`;
    return;
  }
  const min = Math.min(...items.map(i => i.level));
  el.outline.innerHTML = items.map((it, i) => `
    <div class="outline-item" data-oi="${i}" data-line="${it.line}"
         style="padding-left:${8 + (it.level - min) * 14}px" title="${esc(it.text)}">
      ${esc(it.text)}
    </div>`).join('');
}

function outlineJump(idx, line) {
  if (state.mode === 'preview') {
    el.preview.querySelectorAll('h1,h2,h3,h4,h5,h6')[idx]?.scrollIntoView({ behavior: 'smooth', block: 'start' });
  } else {
    const ta = el.editor;
    const lines = ta.value.split('\n');
    let pos = 0;
    for (let i = 0; i < line && i < lines.length; i++) pos += lines[i].length + 1;
    ta.focus();
    ta.selectionStart = ta.selectionEnd = pos;
    ta.scrollTop = (line / Math.max(lines.length, 1)) * ta.scrollHeight - 60;
  }
}

/* ============================================================
 * 站点预览与导出
 * ============================================================ */
function download(blob, filename) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = filename;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

const SITE_CSS = `body{font:16px/1.8 -apple-system,"PingFang SC","Microsoft YaHei",sans-serif;color:#24292f;max-width:760px;margin:0 auto;padding:0 20px 60px}
header.site{border-bottom:1px solid #e3e5e8;padding:28px 0 16px;margin-bottom:28px}
header.site h1{margin:0;font-size:26px}header.site p{color:#8a8f98;margin:6px 0 0}
nav.menu{margin-top:12px}nav.menu a{margin-right:18px;color:#3b6fd4;text-decoration:none}
.post-list{list-style:none;padding:0}.post-list li{padding:12px 0;border-bottom:1px dashed #e3e5e8}
.post-list a{color:#24292f;text-decoration:none;font-size:18px}.post-list a:hover{color:#3b6fd4}
.post-list .date{color:#8a8f98;font-size:13px;margin-left:8px}
article h1{border-bottom:1px solid #e3e5e8;padding-bottom:8px}
pre{background:#f6f8fa;padding:12px;border-radius:8px;overflow-x:auto}
code{font-family:Consolas,Menlo,monospace;font-size:.92em}
blockquote{border-left:3px solid #3b6fd4;margin:0;padding:2px 14px;color:#57606a;background:#f6f7f9}
table{border-collapse:collapse}th,td{border:1px solid #e3e5e8;padding:6px 12px}
footer.site{margin-top:48px;padding-top:16px;border-top:1px solid #e3e5e8;color:#8a8f98;font-size:13px}`;

function siteMenuItems(c) {
  const items = [...c.menu];
  if (c.links?.length) items.push({ name: '友链', url: '/links' });
  return items;
}

const linksListHtml = links => `<ul class="post-list">
${links.map(l => `  <li><a href="${esc(l.url)}" target="_blank" rel="noopener">${esc(l.name)}</a></li>`).join('\n')}
</ul>`;

const postListHtml = (posts, linkOf) => `<ul class="post-list">
${posts.map(p => `  <li><a href="${linkOf(p)}">${esc(p.title)}</a><span class="date">${esc(p.date)}</span></li>`).join('\n')}
</ul>`;

function sitePage(nb, title, bodyHtml) {
  const c = nb.config;
  const menu = siteMenuItems(c).map(m =>
    `<a href="${m.url === '/' ? 'index.html' : m.url.replace(/^\//, '') + '.html'}">${esc(m.name)}</a>`).join('\n      ');
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>${esc(title)} - ${esc(c.title)}</title>
<link rel="stylesheet" href="assets/style.css">
</head>
<body>
<header class="site">
  <h1>${esc(c.title)}</h1>
  <p>${esc(c.description)}</p>
  <nav class="menu">
      ${menu}
  </nav>
</header>
<main>
${bodyHtml}
</main>
<footer class="site">${esc(c.footer)} · ${esc(c.author)} · ${esc(c.domain)}</footer>
</body>
</html>`;
}

/* ----- 站点预览：生成自包含 HTML，菜单/文章通过 hash 路由跳转 ----- */
function buildSitePreviewHtml(nb) {
  const c = nb.config;
  const data = {
    title: esc(c.title), description: esc(c.description),
    author: esc(c.author), footer: esc(c.footer),
    menu: siteMenuItems(c).map(m => ({ name: esc(m.name), url: m.url })),
    links: (c.links || []).map(l => ({ name: esc(l.name), url: esc(l.url) })),
    about: marked.parse(c.about || ''),
    posts: sortNodes(nb.posts).map(p => ({
      slug: p.slug, title: esc(p.title), date: esc(p.date), html: marked.parse(p.content || ''),
    })),
  };
  // < 转义为 <，避免数据中的 </script> 破坏文档
  const json = JSON.stringify(data).replace(/</g, '\\u003c');
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>${esc(c.title)}</title>
<style>${SITE_CSS}</style>
</head>
<body>
<script>
var DATA = ${json};
var listHtml = '<ul class="post-list">' + DATA.posts.map(function(p) {
  return '<li><a href="#/post/' + p.slug + '">' + p.title + '</a><span class="date">' + p.date + '</span></li>';
}).join('') + '</ul>';
var menuHtml = DATA.menu.map(function(m) {
  return '<a href="#' + m.url + '">' + m.name + '</a>';
}).join('');
function page(body) {
  return '<header class="site"><h1>' + DATA.title + '</h1><p>' + DATA.description +
    '</p><nav class="menu">' + menuHtml + '</nav></header><main>' + body +
    '</main><footer class="site">' + DATA.footer + ' · ' + DATA.author + '</footer>';
}
function route() {
  var h = location.hash.replace(/^#/, '') || '/';
  var body;
  if (h.indexOf('/post/') === 0) {
    var p = DATA.posts.filter(function(x) { return x.slug === h.slice(6); })[0];
    body = p ? '<article>' + p.html + '</article>' : '<p>文章不存在</p>';
  } else if (h === '/about') {
    body = '<article>' + DATA.about + '</article>';
  } else if (h === '/links') {
    body = '<article><h1>友链</h1><ul class="post-list">' + DATA.links.map(function(l) {
      return '<li><a href="' + l.url + '" target="_blank" rel="noopener">' + l.name + '</a></li>';
    }).join('') + '</ul></article>';
  } else {
    body = listHtml;
  }
  document.body.innerHTML = page(body);
  window.scrollTo(0, 0);
}
window.addEventListener('hashchange', route);
route();
</script>
</body>
</html>`;
}

function openSitePreview(nb) {
  el.previewFrame.srcdoc = buildSitePreviewHtml(nb);
  el.previewOverlay.classList.remove('hidden');
}
function closeSitePreview() {
  el.previewOverlay.classList.add('hidden');
  el.previewFrame.srcdoc = '';
}

async function exportSite(nb) {
  const zip = new JSZip();
  const posts = sortNodes(nb.posts);
  const listHtml = postListHtml(posts, p => `post-${p.slug}.html`);

  zip.file('assets/style.css', SITE_CSS);
  zip.file('index.html', sitePage(nb, '首页', listHtml));
  zip.file('archives.html', sitePage(nb, '归档', listHtml));
  zip.file('about.html', sitePage(nb, '关于', `<article>${marked.parse(nb.config.about || '')}</article>`));
  if (nb.config.links?.length) {
    zip.file('links.html', sitePage(nb, '友链', `<article><h1>友链</h1>${linksListHtml(nb.config.links)}</article>`));
  }
  posts.forEach(p => zip.file(`post-${p.slug}.html`, sitePage(nb, p.title, `<article>${marked.parse(p.content || '')}</article>`)));
  zip.file('config.json', JSON.stringify(nb.config, null, 2));

  download(await zip.generateAsync({ type: 'blob' }), `${nb.name}-站点.zip`);
  flashStatus(`已导出 ${posts.length} 篇文章 + 首页/归档/关于`);
}

async function exportEbook(nb) {
  const zip = new JSZip();
  const flat = [];
  walkChapters(nb.chapters, c => flat.push(c));

  const summaryLines = ['# Summary', ''];
  (function walk(list, depth) {
    list.forEach(c => {
      summaryLines.push(`${'  '.repeat(depth)}* [${c.title}](${c.file})`);
      walk(c.children, depth + 1);
    });
  })(nb.chapters, 0);
  zip.file('SUMMARY.md', summaryLines.join('\n') + '\n');
  flat.forEach(c => zip.file(c.file, c.content || ''));

  const bookBody = flat.map((c, i) =>
    `<section class="chapter"${i ? ' style="page-break-before:always"' : ''}>${marked.parse(c.content || '')}</section>`
  ).join('\n');
  zip.file('book.html', `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>${esc(nb.name)}</title>
<style>${SITE_CSS}</style>
</head>
<body>
<h1 style="text-align:center;margin:60px 0 8px">${esc(nb.name)}</h1>
${bookBody}
</body>
</html>`);

  download(await zip.generateAsync({ type: 'blob' }), `${nb.name}-电子书.zip`);
  flashStatus(`已导出 SUMMARY.md + ${flat.length} 个章节 + book.html`);
}

function flashStatus(text) {
  el.status.textContent = text;
  setTimeout(updateStatus, 4000);
}

/* ============================================================
 * 事件绑定
 * ============================================================ */
function renderAll() {
  renderLeft();
  renderTabs();
  renderToolbar();
  renderEditor();
  renderOutline();
}

/* ----- 左栏事件（委托） ----- */
/* 单击：仅选中（折叠箭头除外）；搜索模式下单击结果直接打开 */
el.leftBody.addEventListener('click', e => {
  const nb = activeNotebook();
  if (!nb) return;

  const twisty = e.target.closest('.twisty[data-action]');
  if (twisty) {
    e.stopPropagation();
    handleLeftAction(nb, twisty.dataset.action, twisty.dataset);
    return;
  }
  const row = e.target.closest('.tree-row');
  if (row) {
    el.leftBody.querySelectorAll('.tree-row.selected').forEach(r => r.classList.remove('selected'));
    row.classList.add('selected');
    if (state.searchQuery && row.dataset.id && row.dataset.kind) openNote(nb.id, row.dataset.id);
  }
});

/* 站点文章分组方式（按分类 / 按标签 / 按日期） */
el.leftBody.addEventListener('change', e => {
  const sel = e.target.closest('[data-groupby]');
  if (!sel) return;
  state.siteGroupBy = sel.value;
  renderLeft();
});

/* 双击：打开笔记；双击文件夹/分组切换折叠 */
el.leftBody.addEventListener('dblclick', e => {
  const nb = activeNotebook();
  if (!nb) return;
  const row = e.target.closest('.tree-row');
  const { kind, id } = row?.dataset || {};
  if (!id) return;
  if (kind === 'dir' || kind === 'group') handleLeftAction(nb, 'toggle-dir', { id });
  else if (kind) openNote(nb.id, id);
});

function handleLeftAction(nb, action, ds) {
  const id = ds.id;
  const targetChildren = () => (id ? findInTree(nb.tree, id)?.node?.children : null) || nb.tree;
  switch (action) {
    case 'toggle-dir':
      state.collapsedDirs.has(id) ? state.collapsedDirs.delete(id) : state.collapsedDirs.add(id);
      break;
    case 'new-file': {
      const name = prompt('笔记名称：', '未命名.md');
      if (!name) return;
      targetChildren().push(fileNode(name.endsWith('.md') ? name : name + '.md', `# ${name.replace(/\.md$/, '')}\n\n`));
      break;
    }
    case 'new-dir': {
      const name = prompt('文件夹名称：', '新建文件夹');
      if (!name) return;
      targetChildren().push(dirNode(name));
      break;
    }
    case 'rename-node': {
      const target = findNote(nb, id);
      if (!target) return;
      const key = nb.type === 'normal' ? 'name' : 'title';
      const name = prompt('重命名：', target[key]);
      if (!name) return;
      target[key] = name;
      state.tabs.forEach(t => { if (t.notebookId === nb.id && t.noteId === id) t.title = name; });
      break;
    }
    case 'delete-node':
      if (!confirm('确认删除？该操作不可恢复。')) return;
      deleteNode(nb, id);
      state.tabs.filter(t => t.notebookId === nb.id && t.noteId === id).forEach(t => closeTab(t.id));
      break;
    case 'new-post': {
      const title = prompt('文章标题：', '新文章');
      if (!title) return;
      nb.posts.push(postNode(title));
      break;
    }
    case 'ch-add': addChapter(nb, null); break;
    case 'ch-add-child': addChapter(nb, id); break;
    case 'ch-move': moveChapter(nb, id, +ds.dir); break;
    case 'ch-indent': indentChapter(nb, id); break;
    case 'ch-outdent': outdentChapter(nb, id); break;
    default: return;
  }
  save();
  renderAll();
}

function deleteNode(nb, id) {
  if (nb.type === 'normal') {
    const r = findInTree(nb.tree, id);
    if (!r) return;
    const arr = r.parent ? r.parent.children : nb.tree;
    arr.splice(arr.findIndex(n => n.id === id), 1);
  } else if (nb.type === 'site') {
    nb.posts = nb.posts.filter(p => p.id !== id);
  } else {
    (function walk(list) {
      const i = list.findIndex(c => c.id === id);
      if (i >= 0) { list.splice(i, 1); return; }
      list.forEach(c => walk(c.children));
    })(nb.chapters);
  }
}

/* ----- 右键上下文菜单（桌面软件交互） ----- */
const ctxMenu = document.createElement('div');
ctxMenu.id = 'ctx-menu';
ctxMenu.className = 'hidden';
document.body.appendChild(ctxMenu);

const hideCtxMenu = () => ctxMenu.classList.add('hidden');

function showCtxMenu(x, y, items) {
  ctxMenu.innerHTML = items.map((it, i) =>
    it === '-' ? '<div class="ctx-sep"></div>'
      : `<div class="ctx-item ${it.danger ? 'danger' : ''} ${it.disabled ? 'disabled' : ''}" data-i="${i}">${it.label}</div>`
  ).join('');
  ctxMenu.classList.remove('hidden');
  // 防止超出窗口
  const rect = ctxMenu.getBoundingClientRect();
  ctxMenu.style.left = Math.min(x, innerWidth - rect.width - 8) + 'px';
  ctxMenu.style.top = Math.min(y, innerHeight - rect.height - 8) + 'px';

  ctxMenu.onclick = e => {
    const item = e.target.closest('.ctx-item');
    if (!item || item.classList.contains('disabled')) return;
    hideCtxMenu();
    items[+item.dataset.i].onClick();
  };
}
document.addEventListener('click', hideCtxMenu);
document.addEventListener('keydown', e => {
  if (e.key === 'Escape') { hideCtxMenu(); closeSitePreview(); closeSiteConfig(); el.nbList.classList.add('hidden'); }
});
window.addEventListener('blur', hideCtxMenu);
el.leftBody.addEventListener('scroll', hideCtxMenu);

el.leftBody.addEventListener('contextmenu', e => {
  e.preventDefault();
  const nb = activeNotebook();
  if (!nb) return;
  const row = e.target.closest('.tree-row');
  const items = buildCtxItems(nb, row, row?.dataset.id);
  if (items.length) showCtxMenu(e.clientX, e.clientY, items);
});

function buildCtxItems(nb, row, id) {
  const act = (action, ds = {}) => () => handleLeftAction(nb, action, ds);
  const renameDel = [
    { label: '重命名', onClick: act('rename-node', { id }) },
    { label: '删除', danger: true, onClick: act('delete-node', { id }) },
  ];
  if (nb.type === 'normal') {
    if (!row) return [
      { label: '新建笔记', onClick: act('new-file', { id: '' }) },
      { label: '新建文件夹', onClick: act('new-dir', { id: '' }) },
    ];
    if (row.dataset.kind === 'dir') return [
      { label: '新建笔记', onClick: act('new-file', { id }) },
      { label: '新建文件夹', onClick: act('new-dir', { id }) },
      '-', ...renameDel,
    ];
    return renameDel;
  }
  if (nb.type === 'site') {
    if (row?.dataset.kind === 'post') return renameDel;
    return [
      { label: '新建文章', onClick: act('new-post') },
      { label: '站点配置', onClick: () => openSiteConfig(nb.id) },
    ];
  }
  // ebook
  if (!row || row.dataset.kind !== 'chapter') return [{ label: '添加章节', onClick: act('ch-add') }];
  const r = findChapterParent(nb, id);
  const isFirst = !r || r.index === 0;
  return [
    { label: '添加子章节', onClick: act('ch-add-child', { id }) },
    '-',
    { label: '上移', disabled: isFirst, onClick: act('ch-move', { id, dir: -1 }) },
    { label: '下移', disabled: !r || r.index === r.list.length - 1, onClick: act('ch-move', { id, dir: 1 }) },
    { label: '缩进为子章节', disabled: isFirst, onClick: act('ch-indent', { id }) },
    { label: '提升层级', disabled: !r || !r.parent, onClick: act('ch-outdent', { id }) },
    '-', ...renameDel,
  ];
}

/* ----- 电子书章节操作 ----- */
function findChapterParent(nb, id) {
  let result = null;
  (function walk(list, parent) {
    for (const c of list) {
      if (c.id === id) { result = { list, parent, index: list.indexOf(c), node: c }; return; }
      walk(c.children, c);
    }
  })(nb.chapters, null);
  return result;
}
function addChapter(nb, parentId) {
  const title = prompt('章节标题：', '新章节');
  if (!title) return;
  const ch = chapterNode(title, `chapter-${Date.now().toString(36)}.md`);
  if (parentId) {
    const p = findNote(nb, parentId);
    if (p) { ch.file = p.file.replace(/\.md$/, '') + '/' + ch.file; p.children.push(ch); }
  } else nb.chapters.push(ch);
}
function moveChapter(nb, id, dir) {
  const r = findChapterParent(nb, id);
  if (!r) return;
  const j = r.index + dir;
  if (j < 0 || j >= r.list.length) return;
  [r.list[r.index], r.list[j]] = [r.list[j], r.list[r.index]];
}
function indentChapter(nb, id) {
  const r = findChapterParent(nb, id);
  if (!r || r.index === 0) return;
  const prev = r.list[r.index - 1];
  r.list.splice(r.index, 1);
  r.node.file = prev.file.replace(/\.md$/, '') + '/' + r.node.file.split('/').pop();
  prev.children.push(r.node);
}
function outdentChapter(nb, id) {
  const r = findChapterParent(nb, id);
  if (!r || !r.parent) return;
  const pr = findChapterParent(nb, r.parent.id);
  r.list.splice(r.index, 1);
  r.node.file = r.node.file.split('/').pop();
  pr.list.splice(pr.index + 1, 0, r.node);
}

/* ----- 拖拽排序（普通笔记本，写回 order 元数据） ----- */
let dragId = null;
el.leftBody.addEventListener('dragstart', e => {
  const row = e.target.closest('.tree-row');
  if (row) dragId = row.dataset.id;
});
el.leftBody.addEventListener('dragover', e => {
  const row = e.target.closest('.tree-row');
  if (!row || !dragId || row.dataset.id === dragId) return;
  e.preventDefault();
  el.leftBody.querySelectorAll('.drag-over').forEach(r => r.classList.remove('drag-over'));
  row.classList.add('drag-over');
});
el.leftBody.addEventListener('dragleave', e => {
  e.target.closest('.tree-row')?.classList.remove('drag-over');
});
el.leftBody.addEventListener('drop', e => {
  const row = e.target.closest('.tree-row');
  el.leftBody.querySelectorAll('.drag-over').forEach(r => r.classList.remove('drag-over'));
  if (!row || !dragId || row.dataset.id === dragId) return;
  e.preventDefault();
  const nb = activeNotebook();
  if (!nb || nb.type !== 'normal') return;

  const src = findInTree(nb.tree, dragId);
  const dst = findInTree(nb.tree, row.dataset.id);
  if (!src || !dst) return;
  // 防止把文件夹拖进自己的子树
  let p = dst.parent;
  while (p) { if (p.id === dragId) return; p = findInTree(nb.tree, p.id)?.parent; }

  const srcArr = src.parent ? src.parent.children : nb.tree;
  srcArr.splice(srcArr.findIndex(n => n.id === dragId), 1);

  if (dst.node.type === 'dir') {
    dst.node.children.push(src.node);
    state.collapsedDirs.delete(dst.node.id);
  } else {
    const dstArr = dst.parent ? dst.parent.children : nb.tree;
    dstArr.splice(dstArr.findIndex(n => n.id === dst.node.id), 0, src.node);
  }
  // 重写同层 order，模拟写入 .metadata.json
  (dst.node.type === 'dir' ? dst.node.children : dst.parent ? dst.parent.children : nb.tree)
    .forEach((n, i) => { n.order = i; });
  dragId = null;
  save();
  renderAll();
});

/* ----- Tab 事件 ----- */
el.tabBar.addEventListener('click', e => {
  const close = e.target.closest('[data-close]');
  if (close) { e.stopPropagation(); closeTab(close.dataset.close); return; }
  const tab = e.target.closest('[data-tab]');
  if (tab) {
    state.activeTabId = tab.dataset.tab;
    state.mode = 'edit';
    renderAll();
  }
});
function closeTab(id) {
  const i = state.tabs.findIndex(t => t.id === id);
  if (i < 0) return;
  state.tabs.splice(i, 1);
  if (state.activeTabId === id) {
    state.activeTabId = state.tabs[Math.min(i, state.tabs.length - 1)]?.id || null;
    state.mode = 'edit';
  }
  renderAll();
}

/* ----- 编辑器 ----- */
let outlineTimer = null;
el.editor.addEventListener('input', () => {
  const t = activeTab();
  const note = activeNote();
  if (!t || !note) return;
  note.content = el.editor.value;
  if (!t.dirty) { t.dirty = true; renderTabs(); }
  updateStatus();
  clearTimeout(outlineTimer);
  outlineTimer = setTimeout(() => { renderOutline(); save(); }, 300);
});

/* ----- 工具栏 ----- */
el.toolbar.addEventListener('click', e => {
  const modeBtn = e.target.closest('[data-mode]');
  if (modeBtn) {
    state.mode = modeBtn.dataset.mode;
    renderEditor();
    renderOutline();
    return;
  }
  const tb = e.target.closest('[data-tb]');
  if (tb) applyToolbar(+tb.dataset.tb);
});

/* ----- 大纲点击 ----- */
el.outline.addEventListener('click', e => {
  const item = e.target.closest('.outline-item');
  if (item) outlineJump(+item.dataset.oi, +item.dataset.line);
});

/* ----- 站点配置弹窗 ----- */
function handlePanelInput(e) {
  const nb = activeNotebook();
  if (!nb) return;
  const { cfg, cfg2, k, cfgLink } = e.target.dataset;
  if (cfg) nb.config[cfg] = e.target.value;
  else if (cfg2 && nb.config[cfg2]) nb.config[cfg2][k] = e.target.value;
  else if (cfgLink !== undefined && nb.config.links) nb.config.links[+cfgLink][k] = e.target.value;
  else return;
  save();
}
el.siteConfigBody.addEventListener('input', handlePanelInput);
el.siteConfigBody.addEventListener('change', handlePanelInput);
el.siteConfigBody.addEventListener('click', e => {
  const btn = e.target.closest('[data-action]');
  if (!btn) return;
  const nb = activeNotebook();
  if (btn.dataset.action === 'link-add') nb.config.links.push({ name: '新友链', url: 'https://' });
  else if (btn.dataset.action === 'link-del') nb.config.links.splice(+btn.dataset.i, 1);
  else return;
  save();
  renderSiteConfigDialog(nb);
});
$('#site-config-close').addEventListener('click', closeSiteConfig);
el.siteConfigMask.addEventListener('click', e => { if (e.target === el.siteConfigMask) closeSiteConfig(); });

/* ----- 顶栏 ----- */
$('#toggle-left').addEventListener('click', () => document.body.classList.toggle('left-collapsed'));
$('#toggle-right').addEventListener('click', () => document.body.classList.toggle('right-collapsed'));
el.exportBtn.addEventListener('click', () => {
  const nb = activeNotebook();
  if (nb?.type === 'site') exportSite(nb);
  else if (nb?.type === 'ebook') exportEbook(nb);
});
el.previewBtn.addEventListener('click', () => {
  const nb = activeNotebook();
  if (nb?.type === 'site') openSitePreview(nb);
});
el.siteConfigBtn.addEventListener('click', () => {
  const nb = activeNotebook();
  if (nb?.type === 'site') openSiteConfig(nb.id);
});
$('#preview-close').addEventListener('click', closeSitePreview);

/* ----- 笔记本切换（自定义下拉） / 新建 ----- */
el.nbCurrent.addEventListener('click', e => {
  e.stopPropagation();
  el.nbList.classList.toggle('hidden');
});
el.nbList.addEventListener('click', e => {
  const item = e.target.closest('[data-nb]');
  if (!item) return;
  state.activeNotebookId = item.dataset.nb;
  el.nbList.classList.add('hidden');
  renderLeft();
});
document.addEventListener('click', e => {
  if (!e.target.closest('#nb-dropdown')) el.nbList.classList.add('hidden');
});

const modal = $('#modal-mask');
$('#new-notebook-btn').addEventListener('click', () => { modal.classList.remove('hidden'); $('#nb-name').focus(); });
$('#modal-cancel').addEventListener('click', () => modal.classList.add('hidden'));
modal.addEventListener('click', e => { if (e.target === modal) modal.classList.add('hidden'); });
$('#modal-ok').addEventListener('click', () => {
  const name = $('#nb-name').value.trim() || '未命名笔记本';
  const type = $('#nb-type').value;
  let nb;
  if (type === 'normal') nb = { id: nid(), name, type, tree: [fileNode('欢迎使用.md', '# 欢迎使用\n\n')] };
  else if (type === 'site') nb = { id: nid(), name, type, config: defaultSiteConfig(name), posts: [] };
  else nb = { id: nid(), name, type, chapters: [chapterNode('简介', 'README.md', { content: `# ${name}\n\n` })] };
  state.notebooks.push(nb);
  state.activeNotebookId = nb.id;
  $('#nb-name').value = '';
  modal.classList.add('hidden');
  save();
  renderAll();
});

/* ----- 全文搜索 ----- */
el.searchInput.addEventListener('input', () => {
  state.searchQuery = el.searchInput.value.trim();
  renderLeft();
});
el.searchInput.addEventListener('keydown', e => {
  if (e.key === 'Escape') {
    el.searchInput.value = '';
    state.searchQuery = '';
    renderLeft();
    el.searchInput.blur();
  }
});

/* ----- 快捷键：Ctrl/Cmd+E 切换编辑/预览，Ctrl/Cmd+F 聚焦搜索 ----- */
document.addEventListener('keydown', e => {
  if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
    e.preventDefault();
    document.body.classList.remove('left-collapsed');
    el.searchInput.focus();
    el.searchInput.select();
    return;
  }
  if ((e.ctrlKey || e.metaKey) && e.key === 'e' && activeTab()) {
    e.preventDefault();
    state.mode = state.mode === 'edit' ? 'preview' : 'edit';
    renderEditor();
    renderOutline();
  }
});

/* ---------------- 启动 ---------------- */
renderAll();
