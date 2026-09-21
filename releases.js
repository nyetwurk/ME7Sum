(function () {
  var REPO = "nyetwurk/ME7Sum";
  var API = "https://api.github.com/repos/" + REPO + "/releases?per_page=100";
  var CACHE_KEY = "me7sum-releases-v2";
  var CACHE_MS = 10 * 60 * 1000;
  var LATEST = "https://github.com/" + REPO + "/releases/latest";

  function platformOf(name) {
    var n = String(name || "").toLowerCase();
    if (n.indexOf("win") !== -1) {
      return { tag: "Windows", order: 0, blurb: "me7sum.exe and ME7Check.exe." };
    }
    if (n.indexOf("macos") !== -1 || n.indexOf("darwin") !== -1) {
      return { tag: "macOS", order: 1, blurb: "Native me7sum binary." };
    }
    if (n.indexOf("ubuntu") !== -1 || n.indexOf("linux") !== -1) {
      return { tag: "Linux", order: 2, blurb: "Ubuntu me7sum build." };
    }
    return { tag: "Download", order: 9, blurb: "" };
  }

  function formatSize(bytes) {
    var n = Number(bytes) || 0;
    if (n < 1024) return n + " B";
    if (n < 1024 * 1024) return (n / 1024).toFixed(n < 10 * 1024 ? 1 : 0) + " KB";
    return (n / (1024 * 1024)).toFixed(1) + " MB";
  }

  function formatDate(iso) {
    if (!iso) return "";
    return String(iso).slice(0, 10);
  }

  function sortAssets(assets) {
    return (assets || []).slice().sort(function (a, b) {
      var pa = platformOf(a.name).order;
      var pb = platformOf(b.name).order;
      if (pa !== pb) return pa - pb;
      return String(a.name).localeCompare(String(b.name));
    });
  }

  function isRc(rel) {
    var tag = String(rel && rel.tag_name || "").toLowerCase();
    return !!(rel && (rel.prerelease || tag.indexOf("-rc") !== -1));
  }

  function publishedReleases(releases) {
    return (releases || []).filter(function (rel) {
      return rel && !rel.draft && !isRc(rel);
    });
  }

  function latestStable(releases) {
    return publishedReleases(releases)[0] || null;
  }

  function el(tag, className, text) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (text != null) node.textContent = text;
    return node;
  }

  function assetCard(asset) {
    var plat = platformOf(asset.name);
    var card = el("a", "card");
    card.href = asset.browser_download_url;
    var top = el("div", "card-top");
    top.appendChild(el("span", "tag", plat.tag));
    top.appendChild(el("span", "", formatSize(asset.size)));
    card.appendChild(top);
    card.appendChild(el("h3", "", asset.name));
    if (plat.blurb) card.appendChild(el("p", "", plat.blurb));
    return card;
  }

  function setText(selector, text) {
    document.querySelectorAll(selector).forEach(function (node) {
      node.textContent = text;
    });
  }

  function renderLatest(release) {
    var root = document.getElementById("release-assets");
    if (!root || !release) return;
    var assets = sortAssets(release.assets);
    root.replaceChildren();
    if (!assets.length) {
      root.appendChild(el("p", "note", "No downloadable assets on this release yet."));
      return;
    }
    assets.forEach(function (asset) {
      root.appendChild(assetCard(asset));
    });
    setText("[data-release-tag]", release.tag_name);
    setText("[data-release-meta]", release.tag_name + " · " + formatDate(release.published_at));
    document.querySelectorAll("[data-release-button]").forEach(function (btn) {
      btn.textContent = "Download " + release.tag_name;
      btn.href = LATEST;
    });
  }

  function renderArchive(releases) {
    var root = document.getElementById("release-archive");
    if (!root) return;
    var list = publishedReleases(releases);
    root.replaceChildren();
    if (!list.length) {
      root.appendChild(el("p", "note", "No GitHub releases found."));
      return;
    }
    list.forEach(function (release) {
      var block = el("article", "release");
      var head = el("div", "section-head");
      var title = el("h2", "");
      var link = el("a", "", release.tag_name);
      link.href = release.html_url;
      title.appendChild(link);
      head.appendChild(title);
      head.appendChild(el(
        "p",
        "",
        formatDate(release.published_at) + " · " + (release.assets || []).length + " files"
      ));
      block.appendChild(head);
      var grid = el("div", "grid");
      sortAssets(release.assets).forEach(function (asset) {
        grid.appendChild(assetCard(asset));
      });
      block.appendChild(grid);
      root.appendChild(block);
    });
  }

  function showError(message) {
    ["release-assets", "release-archive"].forEach(function (id) {
      var root = document.getElementById(id);
      if (!root) return;
      root.replaceChildren();
      var note = el("p", "note", message + " ");
      var link = el("a", "", "Open GitHub Releases");
      link.href = LATEST;
      note.appendChild(link);
      root.appendChild(note);
    });
  }

  function readCache() {
    try {
      var raw = sessionStorage.getItem(CACHE_KEY);
      if (!raw) return null;
      var box = JSON.parse(raw);
      if (!box || Date.now() - box.at > CACHE_MS || !Array.isArray(box.releases)) return null;
      return box.releases;
    } catch (err) {
      return null;
    }
  }

  function writeCache(releases) {
    try {
      sessionStorage.setItem(CACHE_KEY, JSON.stringify({ at: Date.now(), releases: releases }));
    } catch (err) {
      /* ignore quota / private mode */
    }
  }

  function apply(releases) {
    renderLatest(latestStable(releases));
    renderArchive(releases);
  }

  var cached = readCache();
  if (cached) apply(cached);

  fetch(API, {
    headers: { Accept: "application/vnd.github+json" }
  }).then(function (res) {
    if (!res.ok) throw new Error("GitHub API " + res.status);
    return res.json();
  }).then(function (releases) {
    if (!Array.isArray(releases)) throw new Error("Unexpected GitHub API payload");
    writeCache(releases);
    apply(releases);
  }).catch(function () {
    if (!cached) {
      showError("Could not load releases from GitHub.");
    }
  });
})();
