$(function() {
  var root = document.documentElement;
  var giscus = document.querySelector('script[data-src="https://giscus.app/client.js"]');
  function syncGiscusTheme() {
    if (!giscus) return;
    giscus.dataset.theme = root.dataset.theme;
    var frame = document.querySelector('iframe.giscus-frame');
    if (frame) {
      frame.contentWindow.postMessage({ giscus: { setConfig: { theme: root.dataset.theme } } }, 'https://giscus.app');
    }
  }
  if (giscus) {
    syncGiscusTheme();
    document.addEventListener('load', function(event) {
      if (event.target.matches && event.target.matches('iframe.giscus-frame')) syncGiscusTheme();
    }, true);
    giscus.src = giscus.dataset.src;
  }
  var $themeToggle = $('#theme-toggle');
  $themeToggle.prop('hidden', false).attr('aria-checked', String(root.dataset.theme === 'dark')).on('click', function() {
    root.dataset.theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
    $themeToggle.attr('aria-checked', String(root.dataset.theme === 'dark'));
    try { localStorage.setItem('qsnote-pure-theme', root.dataset.theme); } catch (_) {}
    syncGiscusTheme();
  });


  // Toggle both the widgets and the article Catalogue sidebar.
  var $sidebarToggle = $('#sidebar-toggle');
  function setSidebarExpanded(expanded) {
    $('body').toggleClass('no-sidebar', !expanded);
    var label = expanded ? '收起侧栏' : '展开侧栏';
    $sidebarToggle.attr('aria-expanded', String(expanded)).attr('aria-label', label).attr('title', label);
  }
  if ($('#collapseToc').length) {
    $sidebarToggle.attr('aria-controls', 'main-sidebar collapseToc');
  }
  $sidebarToggle.prop('hidden', false).on('click', function() {
    setSidebarExpanded($('body').hasClass('no-sidebar'));
  });
  $('#collapseToc').on('show.bs.collapse', function() {
    setSidebarExpanded(true);
  });

  // bootstrap tooltip
  $('[data-toggle="tooltip"]').tooltip();

  // slimscroll
  if (typeof $.fn.slimScroll != 'undefined') {
    $(".sidebar .slimContent").slimScroll({
      height: $(window).height(),
      color: "rgba(0,0,0,0.15)",
      size: "5px",
      position: 'right',
      // allowPageScroll: true
    });
  }

  $('#collapseToc').on('shown.bs.collapse', function() {
    // do something…
    // slimscroll
    if (typeof $.fn.slimScroll != 'undefined') {
      $(".sidebar .slimContent").slimScroll().on('slimscroll');
    }
  });

  // Follow the current article heading, using the same offset as Shiro.
  var tocContent = document.querySelector('#collapseToc .slimContent');
  if (tocContent) {
    var tocEntries = Array.from(tocContent.querySelectorAll('.toc-link[href^="#"]')).map(function(link) {
      return { link: link, heading: document.getElementById(decodeURIComponent(link.getAttribute('href').slice(1))) };
    }).filter(function(entry) { return entry.heading; });
    var activeEntry = null;
    var tocUpdatePending = false;

    function updateToc() {
      tocUpdatePending = false;
      var current = null;
      tocEntries.forEach(function(entry) {
        if (entry.heading.getBoundingClientRect().top <= 100) current = entry;
      });
      if (current !== activeEntry) {
        if (activeEntry) {
          activeEntry.link.classList.remove('active');
          activeEntry.link.removeAttribute('aria-current');
        }
        activeEntry = current;
        if (activeEntry) {
          activeEntry.link.classList.add('active');
          activeEntry.link.setAttribute('aria-current', 'location');
        }
      }
      if (!activeEntry || !tocContent.getClientRects().length) return;
      var linkRect = activeEntry.link.getBoundingClientRect();
      var contentRect = tocContent.getBoundingClientRect();
      var offset = linkRect.top < contentRect.top ? linkRect.top - contentRect.top :
        Math.max(0, linkRect.bottom - contentRect.bottom);
      if (offset) {
        var scrollTop = tocContent.scrollTop + offset;
        if (typeof $.fn.slimScroll != 'undefined') {
          $(tocContent).slimScroll({ scrollTo: scrollTop + 'px' });
        } else {
          tocContent.scrollTop = scrollTop;
        }
      }
    }

    function scheduleTocUpdate() {
      if (tocUpdatePending) return;
      tocUpdatePending = true;
      window.requestAnimationFrame(updateToc);
    }
    window.addEventListener('scroll', scheduleTocUpdate, { passive: true });
    window.addEventListener('resize', scheduleTocUpdate);
    window.addEventListener('load', scheduleTocUpdate);
    $('#collapseToc').on('shown.bs.collapse', scheduleTocUpdate);
    $sidebarToggle.on('click', scheduleTocUpdate);
    if (document.fonts) document.fonts.ready.then(scheduleTocUpdate);
    scheduleTocUpdate();
  }

  // geopattern 背景生成
  $(".geopattern").each(function() {
    $(this).geopattern($(this).data('pattern-id'));
  });

  // okayNav
  var navigation = $('#nav-main').okayNav({
    swipe_enabled: false, // If true, you'll be able to swipe left/right to open the navigation
  });

  // modal居中
  // $('.modal').on('shown.bs.modal', function(e) {
  //   $(this).show();
  //   var modalDialog = $(this).find(".modal-dialog");
  //    // Applying the top margin on modal dialog to align it vertically center 
  //   modalDialog.css("margin-top", Math.max(0, ($(window).height() - modalDialog.height()) / 2));
  // });

  // sticky
  $('[data-stick-bottom]').keepInView({
    fixed: false,
    parentClass: "has-sticky",
    customClass: "sticky",
    trigger: 'bottom',
    zindex: 42,
    edgeOffset: 0
  });
  
  $('[data-stick-top]').keepInView({
    fixed: true,
    parentClass: "has-sticky",
    customClass: "sticky",
    trigger: 'top',
    zindex: 42,
    edgeOffset: 0
  });

  // menu auto highlight
  var menuHighlight = $("ul.main-nav").hasClass('menu-highlight');
  if (menuHighlight) {
    var currentPathname = location.pathname,
      $menuList = $("ul.main-nav>li"),
      activeIndex = -1;
    for (var i = 0, length = $menuList.length; i < length; i++) {
      var itemHref = $($menuList[i]).find('a').attr('href');
      if (currentPathname.indexOf(itemHref) > -1 ||
        (currentPathname === '/' && (itemHref === '/.' || itemHref === '/' || itemHref === 'index.html' || itemHref === '/index.html'))) {
        activeIndex = i;
      }
      $($menuList[i]).removeClass('active');
    }
    $menuList[activeIndex] && $($menuList[activeIndex]).addClass('active');
  }
});
