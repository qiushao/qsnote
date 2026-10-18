# 3-Jekyll upstream assets

Source: https://github.com/P233/3-Jekyll
Author: P233 (Peiwen Lu)
Revision: 45ca7c4578d498890c8ef078ff13c189a12df114

`assets/css/main.css` is compiled from upstream `css/main.sass` and `_sass/`.
The IcoMoon font files and default avatar are copied unchanged from upstream.
The templates adapt upstream `_layouts/default.html`, `_layouts/post.html`,
and `_includes/sidebar.html`. The QSNote script replaces the Jekyll/Liquid,
jQuery and PJAX integration with static links, native menu/filter handling,
and the existing local search index.

The upstream repository at this revision does not include a theme LICENSE
or an explicit license grant for its bundled fonts/avatar. No license is
inferred here. The upstream default footer's CC BY-NC-SA statement refers
to site content; it is not treated as a license for the theme source.
The upstream HTML compression template and third-party JavaScript libraries
are not included in this port.
