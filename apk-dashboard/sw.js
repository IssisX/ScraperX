// Cache the dashboard shell only. GitHub metadata and APKs always use the network.
const CACHE = 'scraperx-shell-2026-10-07-v3';
const ROOT = new URL('./', self.location.href);
const SHELL = ['index.html', 'app.js', 'manifest.webmanifest', 'icon.svg', 'icon-192.png', 'icon-512.png'];
self.addEventListener('install', event => {
  event.waitUntil(caches.open(CACHE).then(cache => cache.addAll(SHELL.map(file => new URL(file, ROOT).href)))
    .then(() => self.skipWaiting()));
});
self.addEventListener('activate', event => {
  event.waitUntil(caches.keys().then(keys => Promise.all(keys
    .filter(key => key.startsWith('scraperx-shell-') && key !== CACHE).map(key => caches.delete(key))))
    .then(() => self.clients.claim()));
});
self.addEventListener('fetch', event => {
  const request = event.request;
  const url = new URL(request.url);
  if (request.method !== 'GET' || url.origin !== ROOT.origin || !url.pathname.startsWith(ROOT.pathname)) return;
  const navigation = request.mode === 'navigate';
  const file = url.pathname.slice(ROOT.pathname.length);
  if (!navigation && !SHELL.includes(file)) return;
  event.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const key = navigation ? new URL('index.html', ROOT).href : new URL(file, ROOT).href;
    try {
      const response = await fetch(request, {cache:'no-store'});
      if (!response.ok || response.redirected) throw new Error('Dashboard unavailable');
      await cache.put(key, response.clone());
      return response;
    } catch (_) {
      const saved = await cache.match(key);
      return saved || new Response('ScraperX APKs is offline. Reconnect and reopen the dashboard.',
        {status:503, headers:{'Content-Type':'text/plain; charset=utf-8'}});
    }
  })());
});
