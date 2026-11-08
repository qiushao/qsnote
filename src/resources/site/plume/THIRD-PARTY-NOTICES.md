# Plume for QSNote

Upstream: https://github.com/pengzhanbo/vuepress-theme-plume
Version: 1.0.0-rc.214
Commit: 0e9b5fdbc1ca7cb3c72b06f57e1387657926ac93
License: MIT (see LICENSE).

- `assets/css/vars.css`: upstream `theme/src/client/styles/vars.css`, unchanged.
- `assets/css/components.css`: scoped styles from `Posts/VPPostItem.vue` and
  `Posts/VPPostsProfile.vue`; Vue `:deep()` wrappers removed for static HTML.
- `assets/css/icons.css`: selected static icons from upstream `styles/icons.css`;
  sun/moon icons retain the Lucide/Feather licenses in `assets/ICON-LICENSE`.
  Search, menu and chevron paths are local static controls. The GitHub mark is
  the Simple Icons GitHub mark (CC0).
- `templates/card.html` and `profile.html`: static adaptation of those components.
- `templates/shell.html`, `page.html`, `assets/css/qsnote-plume.css`: QSNote adaptation
  of `Nav/VPNavBar.vue`, `Nav/VPSwitchAppearance.vue`, `VPSwitch.vue`,
  `Posts/VPPosts.vue`, `VPPostList.vue`, `VPPostsAside.vue`, `VPPostsNav.vue`,
  `VPPostsExtract.vue`, `VPPagination.vue`, `VPDoc.vue`, `VPSidebar.vue` and
  `VPDocAsideOutline.vue`. The reset follows `styles/normalize.css`.
  Article excerpts follow the upstream explicit `<!-- more -->` marker;
  reading time and deterministic tag colors are lightweight QSNote equivalents.
- `assets/nav-icons/*.svg`: QSNote navigation icons. Settings reuses the
  application icon; GitHub reuses the Simple Icons mark (CC0).
- `assets/js/plume.js`: QSNote implementation using native HTML controls, without Vue.

Updating: check out the pinned upstream commit (or deliberately select and record
its successor), copy vars.css and extract the two component style blocks above,
remove :deep() wrappers, then review template and adapter differences. Run the
optional QSNOTE_BUILD_SITE_TESTS tests and inspect blog/document pages at desktop
and mobile widths, both color schemes and without JavaScript. No Node.js build
or VuePress plugins are required by QSNote or its generated sites.
