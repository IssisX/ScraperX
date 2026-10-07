    const repo = 'IssisX/ScraperX';
    const tag = 'apk-latest';
    const models = [
      { label: 'ChatGPT', branch: 'ChatGPT', file: 'ScraperX-ChatGPT.apk' },
      { label: 'Gemini', branch: 'Gemini', file: 'ScraperX-Gemini.apk' },
      { label: 'Claude', branch: 'ScraperX-Claude', file: 'ScraperX-Claude.apk' },
      { label: 'Grok', branch: 'ScraperX-Grok', file: 'ScraperX-Grok.apk' }
    ];

    function esc(value) {
      return String(value ?? '').replace(/[&<>"']/g, c => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;'
      }[c]));
    }

    function bytes(n) {
      if (!Number.isFinite(n)) return '—';
      return (n / 1048576).toFixed(1) + ' MB';
    }

    function ago(iso) {
      if (!iso) return '—';
      const seconds = Math.max(0, (Date.now() - new Date(iso).getTime()) / 1000);
      if (seconds < 90) return 'just now';
      if (seconds < 3600) return Math.floor(seconds / 60) + ' min ago';
      if (seconds < 86400) return Math.floor(seconds / 3600) + ' hr ago';
      return Math.floor(seconds / 86400) + ' d ago';
    }

    function stableDownload(file) {
      return 'https://github.com/' + repo + '/releases/download/' + tag + '/' + encodeURIComponent(file);
    }

    function card(model) {
      return '<section class="card" data-model="' + esc(model.label) + '" id="card-' + esc(model.label) + '">' +
        '<div class="topline"><div class="model">' + esc(model.label) + '</div><div class="status ok">ready</div></div>' +
        '<div class="meta">' +
          '<div>Branch<b>' + esc(model.branch) + '</b></div>' +
          '<div>APK<b class="size">—</b></div>' +
          '<div>Build<b class="sha">—</b></div>' +
          '<div>Updated<b class="built">—</b></div>' +
        '</div>' +
        '<a class="download" href="' + stableDownload(model.file) + '">Download ' + esc(model.file) + '</a>' +
        '<a class="details" target="_blank" rel="noopener">Open successful build ↗</a>' +
      '</section>';
    }

    document.getElementById('cards').innerHTML = models.map(card).join('');

    const savedKey = 'scraperx-release-v3';
    let lastRelease = null;
    let checking = false;

    function renderRelease(release) {
      const assets = new Map((release.assets || []).map(a => [a.name, a]));
      let provenance = null;
      try {
        const embedded = String(release.body || '').match(/<!-- scraperx-manifest\n([\s\S]*?)\n-->/);
        if (embedded) provenance = JSON.parse(embedded[1]);
      } catch (_) {}
      function publishedSourcePrefix(label) {
        const model = models.find(m => m.label === label);
        const build = provenance?.builds?.[label];
        const asset = assets.get(model.file);
        if (build) {
          return build.file === model.file && asset?.size === build.bytes &&
            asset?.digest === 'sha256:' + build.apk_sha256 ? build.source_sha : '';
        }
        const row = String(release.body || '').split('\n')
          .find(line => line.trimStart().startsWith('| ' + label + ' |'));
        return row?.match(/[0-9a-f]{12,40}/i)?.[0]?.toLowerCase() || '';
      }
      models.forEach(model => {
        const root = document.getElementById('card-' + model.label);
        const status = root.querySelector('.status');
        const download = root.querySelector('.download');
        const details = root.querySelector('.details');
        const asset = assets.get(model.file);
        const source = publishedSourcePrefix(model.label);
        if (asset && asset.state === 'uploaded') {
          status.textContent = 'ready';
          status.className = 'status ok';
          root.querySelector('.size').textContent = bytes(asset.size);
          root.querySelector('.sha').textContent = source.slice(0, 12) || '—';
          root.querySelector('.built').textContent = ago(asset.updated_at);
          const url = new URL(stableDownload(model.file));
          url.searchParams.set('asset', asset.id);
          download.href = url.href;
          download.removeAttribute('aria-disabled');
          details.href = source ? 'https://github.com/' + repo + '/commit/' + source :
            'https://github.com/' + repo + '/releases/tag/' + tag;
          details.textContent = source ? 'Open published source ↗' : 'Open release ↗';
        } else {
          status.textContent = 'unavailable';
          status.className = 'status bad';
          for (const name of ['size', 'sha', 'built']) root.querySelector('.' + name).textContent = '—';
          download.removeAttribute('href');
          download.setAttribute('aria-disabled', 'true');
          details.href = 'https://github.com/' + repo + '/releases/tag/' + tag;
          details.textContent = 'Open release ↗';
        }
      });
    }

    try {
      const saved = JSON.parse(localStorage.getItem(savedKey));
      if (saved?.release?.tag_name === tag && Array.isArray(saved.release.assets)) {
        lastRelease = saved.release;
        renderRelease(lastRelease);
      }
    } catch (_) {}

    async function load() {
      if (checking) return;
      checking = true;
      const button = document.getElementById('refresh');
      button.disabled = true;
      document.getElementById('checked').textContent = 'Checking GitHub…';

      try {
        const releaseResponse = await fetch(
          'https://api.github.com/repos/' + repo + '/releases/tags/' + tag + '?check=' + Date.now(),
          { headers: { 'Accept': 'application/vnd.github+json' }, cache: 'no-store', signal: AbortSignal.timeout(15000) }
        );
        if (!releaseResponse.ok) throw new Error('metadata unavailable');

        const release = await releaseResponse.json();
        if (release.tag_name !== tag || !Array.isArray(release.assets)) throw new Error('invalid release');
        renderRelease(release);
        lastRelease = release;
        try { localStorage.setItem(savedKey, JSON.stringify({release, checkedAt: Date.now()})); } catch (_) {}

        document.getElementById('checked').textContent =
          'Checked ' + new Date().toLocaleTimeString([], {hour:'numeric', minute:'2-digit'});
      } catch (_) {
        document.getElementById('checked').textContent =
          lastRelease ? (navigator.onLine ? 'Could not check · showing saved builds' : 'Offline · showing saved builds') :
            'Could not check · tap Refresh to retry';
        if (lastRelease) document.querySelectorAll('.status.ok').forEach(status => {
          status.textContent = 'saved';
          status.className = 'status sync';
        });
      } finally {
        button.disabled = false;
        checking = false;
      }
    }

    document.getElementById('refresh').addEventListener('click', load);
    load();
    setInterval(() => { if (!document.hidden) load(); }, 2 * 60 * 1000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) load(); });
    window.addEventListener('online', load);
    window.addEventListener('pageshow', event => { if (event.persisted) load(); });
    document.getElementById('cards').addEventListener('click', event => {
      const link = event.target.closest('.download[href]');
      if (link) {
        const url = new URL(link.href);
        url.searchParams.set('download', Date.now());
        link.href = url.href;
      }
    });

    let installPrompt = null;
    const install = document.getElementById('install');
    window.addEventListener('beforeinstallprompt', event => {
      event.preventDefault();
      installPrompt = event;
      install.hidden = false;
    });
    install.addEventListener('click', async () => {
      if (!installPrompt) return;
      await installPrompt.prompt();
      await installPrompt.userChoice;
      installPrompt = null;
      install.hidden = true;
    });
    window.addEventListener('appinstalled', () => { install.hidden = true; });

    if ('serviceWorker' in navigator && /^https?:$/.test(location.protocol)) {
      const wasControlled = Boolean(navigator.serviceWorker.controller);
      let reloading = false;
      navigator.serviceWorker.addEventListener('controllerchange', () => {
        if (wasControlled && !reloading) { reloading = true; location.reload(); }
      });
      navigator.serviceWorker.register('./sw.js', {updateViaCache:'none'})
        .then(registration => {
          registration.update().catch(() => {});
          window.addEventListener('online', () => registration.update().catch(() => {}));
        }).catch(() => {});
    }

