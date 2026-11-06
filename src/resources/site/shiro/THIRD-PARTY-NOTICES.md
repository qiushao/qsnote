# Shiro upstream assets

Source: https://github.com/Acris/hexo-theme-shiro
Version: 1.6.1
Commit: 417329cba818706c989655b55d5c1b6616244637 (main)
License: MIT, Copyright (c) 2025 Acris Liu. See the accompanying LICENSE.

The precompiled `style.min.css`, `toc.min.css`, and `theme-toggle.min.js`,
`mobile-menu.min.js`, `toc.min.js`, `progress.min.js`, `back-to-top.min.js`
are copied without changes from this commit. The HTML templates, inline SVG
icons and seal are adapted from the upstream Nunjucks layouts and UI macros.

QSNote supplies page data, Markdown rendering, comments and search.json.
`qsnote-shiro.css`, `qsnote-shiro-head.js` and `qsnote-shiro.js` adapt article
colors, navigation, heading IDs and local search to QSNote's generated output.
Google Fonts, Pagefind, Hexo plugins, analytics, LightGallery and the font
preloader are not bundled or loaded. Font stacks fall back to local serif
fonts. No Node.js, Hexo or Tailwind installation is required to build QSNote
or generate a site.

For asset updates, check out the pinned upstream commit and copy the listed
precompiled files. To regenerate upstream assets use Node.js >= 24.11.0,
`npm ci` and `npm run build` with that commit's package-lock.json. Keep any
QSNote-specific changes in the adapter files rather than the upstream files.
