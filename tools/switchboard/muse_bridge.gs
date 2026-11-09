/**
 * ScraperX Muse bridge (design v3 step B, job J-031). Google Apps Script on the owner's account.
 *
 * Both directions, every five minutes:
 *   OUT  The switchboard asks Muse by posting a comment that starts `CLAUDE_TO_MUSE:` on an issue and
 *        labeling the issue `wake:muse`. This script mails each such comment it has not sent yet to
 *        Muse, then removes the label. A labeled PR is skipped untouched (the token cannot clear it).
 *   IN   Mail whose subject starts `MUSE_TO_CLAUDE:` (from an allowed sender, proven by more than the
 *        forgeable From line: see authentic_) is posted as a comment:
 *        on #N when the subject names `#N` and #N is an issue, else on the job issue `J-NNN` the subject
 *        names (or, for a PR #N, the job issue whose title names #N), else on the board #340. Never on a
 *        PR: this token cannot comment there. Its first line is `**Muse (via email bridge): <subject>**`,
 *        so a subject like `MUSE_TO_CLAUDE: J-042 review of #100 @ 1a2b3c4d: LAND` is a verdict the
 *        switch reads for #100 wherever it lands (switch.py bridged_verdicts).
 *
 *        Inbound reads from a stored mark (`inbound_mark`, the end of the last window fully posted), less
 *        INBOUND_OVERLAP_MIN, never from a fixed `newer_than`, so an outage of any length up to
 *        INBOUND_FIRST_DAYS loses no mail (J-072: `newer_than:2d` and a 20-thread cap dropped every
 *        MUSE_TO_CLAUDE mail of 11-07 19:20Z to 11-09 14:44Z). A message is posted only when its id is not
 *        in `posted_message_ids` and its date is inside the window, so a message is posted once.
 *   CLOCK (J-072) Every CLOCK_MIN minutes it dispatches switchboard.yml on trunk with dry_run=false: the
 *        reliable clock GitHub's own `schedule` is not (it ran 26 times in 4 days).
 *   WATCH (J-072) When no full non-dry sweep (run-name SWEEP_TITLE) has succeeded for DOWN_MIN minutes,
 *        it posts one `Claude (integrator): SWITCHBOARD DOWN:` comment on the board and mails this
 *        account (subject `Run failed: Switchboard ...`), once per incident; a SWITCHBOARD UP line ends it.
 *
 * Setup (once): Editor > Services (+) > add "Gmail API" (identifier Gmail); Project Settings > Script
 * properties > add GITHUB_TOKEN (and ACTIONS_TOKEN for the clock and the watchdog); then run `setup`
 * and allow. Keep the project unshared: anyone with edit access can read the script properties.
 * Script properties:
 *   GITHUB_TOKEN  required. A fine-grained token, repository IssisX/ScraperX only, with
 *                 Issues: Read and write only (Metadata: Read comes with it). No Pull requests
 *                 permission: it would let the token submit reviews, which the switch reads. It never
 *                 gets Actions permission either: it is never sent to an `actions/` path (gh_).
 *   ACTIONS_TOKEN optional; unset, the clock and the watchdog are off (logged each tick). A separate
 *                 fine-grained token, repository IssisX/ScraperX only, with Actions: Read and write only.
 *                 It is sent only on the `actions/` paths: the dispatch and the run list.
 *   HC_BRIDGE_URL, HC_CLOCK_URL  optional Healthchecks.io ping URLs (https://hc-ping.com/<uuid>):
 *                 pinged after a tick whose outbound and inbound both worked, and after an accepted
 *                 dispatch. Unset, nothing is pinged. Treated as secrets: never logged.
 *   REPO          optional, default IssisX/ScraperX
 *   MUSE_TO       optional, where Muse reads mail; default this account's own address
 *   MUSE_FROM     optional; only this account's own address can ever post (see authentic_), so a
 *                 list without it posts nothing
 * The tokens are read from the properties only, sent only to api.github.com, and never logged.
 *
 * Health (bridge-health): OUT and IN run apart, so one failing never stops the other, and one issue or
 * mail thread failing never stops the rest. Each direction keeps its last success and last error in the
 * script property `health_out` / `health_in`. When one has failed for HEALTH_ALARM_MIN minutes, the
 * bridge posts ONE comment on the board starting `Claude (integrator): BRIDGE DOWN:` with the error's
 * first 200 characters (the token cut out), and none again until that direction has worked once.
 */

var DEFAULT_REPO = 'IssisX/ScraperX';
var BOARD_ISSUE = 340;
var WAKE_LABEL = 'wake:muse';
var DONE_LABEL = 'muse-bridged';
var BOT_LOGIN = 'github-actions[bot]';
var MAX_BODY = 60000;
var MAX_REMEMBERED = 500;
var MAX_SEARCH = 100;            // threads one inbound search returns; a full answer halves the window instead
var MAX_NARROW = 6;              // halvings of the window before inbound gives up loudly (14 days / 64 = 5 h)
var MAX_POSTS = 25;              // messages inbound handles per tick; the rest wait for the next tick (the mark stops there)
var INBOUND_FIRST_DAYS = 14;     // the furthest back inbound ever reads (the first run, or a mark this old)
var INBOUND_EPOCH = Date.UTC(2026, 10, 3);  // the bridge went live 11-03 (J-031); older mail was read by hand
var INBOUND_OVERLAP_MIN = 60;    // each search starts this long before the mark, for mail Gmail indexed late
var MAX_COMMENT_PAGES = 10;     // pages of 100 comments read per wake:muse issue
var COMMENT_DAYS = 7;           // only comments this new are mailed: an id aged out of the remembered list is never re-sent
var HEALTH_ALARM_MIN = 30;      // a direction failing this long posts BRIDGE DOWN on the board, once
var ALARM_HEADER = 'Claude (integrator): BRIDGE DOWN:';
var DIRECTIONS = { out: 'outbound (wake:muse comments mailed to Muse)', in: 'inbound (MUSE_TO_CLAUDE mail posted to GitHub)',
  clock: 'the clock (dispatches the switchboard every 10 min with ACTIONS_TOKEN)',
  watch: 'the switchboard watchdog (reads its runs with ACTIONS_TOKEN)' };
var TRUNK = 'ScraperX-Claude';
var WORKFLOW = 'switchboard.yml';
var CLOCK_MIN = 10;              // the dispatch cadence; ticks are 5 min apart, so a dispatch goes at the first tick 9+ min on
var SWEEP_TITLE = 'Switchboard sweep';  // switchboard.yml's run-name for a full non-dry sweep, and only for one
var DOWN_MIN = 30;               // no successful sweep this long: SWITCHBOARD DOWN
var DOWN_HEADER = 'Claude (integrator): SWITCHBOARD DOWN:';
var UP_HEADER = 'Claude (integrator): SWITCHBOARD UP:';
var DOWN_SUBJECT = 'Run failed: Switchboard';  // the integrator's mail search already matches this

function setup() {
  ScriptApp.getProjectTriggers().forEach(function (t) {
    if (t.getHandlerFunction() === 'tick') ScriptApp.deleteTrigger(t);
  });
  ScriptApp.newTrigger('tick').timeBased().everyMinutes(5).create();
  tick();
}

function tick() {
  var lock = LockService.getScriptLock();
  if (!lock.tryLock(10000)) return;  // the previous tick is still running
  try {
    var cfg;
    try {
      cfg = props_();
    } catch (e) {
      // No token or no account: nothing can be read or posted, so only the properties and the log keep it.
      var store = PropertiesService.getScriptProperties();
      failed_(store, null, 'out', e);
      failed_(store, null, 'in', e);
      return;
    }
    run_(cfg, 'clock', clock_);   // first: mail work never holds the clock back
    var outOk = run_(cfg, 'out', outbound_);  // each direction alone: an outbound throw never stops inbound (11-07 to 11-09)
    var inOk = run_(cfg, 'in', inbound_);
    run_(cfg, 'watch', watchdog_);
    if (outOk && inOk) ping_(cfg, 'HC_BRIDGE_URL');
  } finally {
    lock.releaseLock();
  }
}

/** One step, caught: its success or failure goes in health_<dir>, and a long failure posts one alarm. True if it worked. */
function run_(cfg, dir, fn) {
  try {
    fn(cfg);
  } catch (e) {
    failed_(cfg.store, cfg, dir, e);
    return false;
  }
  var h = health_(cfg.store, dir);
  h.lastOk = new Date().toISOString();
  h.failingSince = null;
  h.alarmed = false;
  cfg.store.setProperty('health_' + dir, JSON.stringify(h));
  return true;
}

function health_(store, dir) {
  try {
    return JSON.parse(store.getProperty('health_' + dir) || '{}');
  } catch (e) {
    return {};
  }
}

/** Records a failure; when the direction has failed HEALTH_ALARM_MIN minutes and no alarm is out, posts one. */
function failed_(store, cfg, dir, err) {
  var h = health_(store, dir);
  var now = new Date();
  h.lastErr = clean_(cfg, err);
  h.lastErrAt = now.toISOString();
  if (!h.failingSince) h.failingSince = h.lastErrAt;
  console.error('Muse bridge ' + dir + ' failed: ' + h.lastErr);
  if (cfg && !h.alarmed && now - new Date(h.failingSince) >= HEALTH_ALARM_MIN * 60000) {
    try {
      var since = Utilities.formatDate(new Date(h.failingSince), 'America/Chicago', 'MMM d h:mm a') + ' Central';
      var last = h.lastOk ? Utilities.formatDate(new Date(h.lastOk), 'America/Chicago', 'MMM d h:mm a') + ' Central' : 'never recorded';
      gh_(cfg, 'POST', 'repos/' + cfg.repo + '/issues/' + BOARD_ISSUE + '/comments', { body:
        ALARM_HEADER + ' ' + DIRECTIONS[dir] + ' has failed since ' + since + ' (last success ' + last + ').\n\n' +
        'Last error: ' + h.lastErr + '\n\nThe full error is in the bridge\'s Apps Script Executions log (Cory only). ' +
        'One alarm per outage: the bridge posts none again until this direction has worked once.' });
      h.alarmed = true;
    } catch (e2) {
      console.error('Muse bridge: the BRIDGE DOWN alarm could not be posted: ' + clean_(cfg, e2));
    }
  }
  store.setProperty('health_' + dir, JSON.stringify(h));
}

/**
 * An error's first 200 characters, safe to store and post: the token cut out (it is never in an error
 * we build, but a service's message could echo a header), no mention, markup or verdict trigger
 * (a `: FIX` would wake the switchboard), one line.
 */
function clean_(cfg, err) {
  var text = String((err && err.message) || err);
  ((cfg && cfg.secrets) || []).forEach(function (s) { text = text.split(s).join('[secret]'); });
  text = text.replace(/https:\/\/hc-ping\.com\/\S*/g, '[ping url]');
  text = text.replace(/(gh[pousr]_|github_pat_)[A-Za-z0-9_]+/g, '[token]');
  text = text.replace(/[\r\n\t]+/g, ' ').replace(/[@`<>*]/g, ' ').replace(/:(?= ?(?:land|fix|note|block))/gi, ' -');
  return text.replace(/\s+/g, ' ').trim().slice(0, 200);
}

function props_() {
  var p = PropertiesService.getScriptProperties();
  var token = p.getProperty('GITHUB_TOKEN');
  if (!token) throw new Error('Muse bridge: set the GITHUB_TOKEN script property first');
  var actionsToken = p.getProperty('ACTIONS_TOKEN') || '';
  var me = Session.getEffectiveUser().getEmail();
  return {
    store: p,
    token: token,
    actionsToken: actionsToken,
    secrets: [token, actionsToken, p.getProperty('HC_BRIDGE_URL'), p.getProperty('HC_CLOCK_URL')].filter(function (s) { return s; }),
    me: me.trim().toLowerCase(),
    repo: p.getProperty('REPO') || DEFAULT_REPO,
    to: p.getProperty('MUSE_TO') || me,
    from: (p.getProperty('MUSE_FROM') || me).split(',').map(function (s) { return s.trim().toLowerCase(); })
  };
}

/**
 * One GitHub call. The token is picked by the path and nothing else: an `actions/` path (the dispatch, the run
 * list) carries ACTIONS_TOKEN, every other path the Issues-only GITHUB_TOKEN. So neither token is ever sent
 * where its permissions do not reach, and the Issues token never needs Actions.
 */
function gh_(cfg, method, path, body) {
  var actions = /^repos\/[^/]+\/[^/]+\/actions\//.test(path);
  var token = actions ? cfg.actionsToken : cfg.token;
  if (!token) throw new Error('Muse bridge: ' + (actions ? 'ACTIONS_TOKEN' : 'GITHUB_TOKEN') + ' is not set');
  var opts = {
    method: method,
    muteHttpExceptions: true,
    headers: {
      Authorization: 'Bearer ' + token,
      Accept: 'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28'
    }
  };
  if (body !== undefined) {
    opts.contentType = 'application/json; charset=utf-8';
    opts.payload = JSON.stringify(body);
  }
  var r = UrlFetchApp.fetch('https://api.github.com/' + path, opts);
  var code = r.getResponseCode();
  if (code === 404 && method === 'DELETE') return null;  // the label was already gone
  if (code < 200 || code >= 300) {
    throw new Error('GitHub ' + method + ' ' + path + ' answered HTTP ' + code + ': ' + r.getContentText().slice(0, 200));
  }
  var text = r.getContentText('UTF-8');
  return text ? JSON.parse(text) : null;
}

function remembered_(cfg, key) {
  try {
    return JSON.parse(cfg.store.getProperty(key) || '[]');
  } catch (e) {
    return [];
  }
}

function remember_(cfg, key, list, id) {
  list.push(String(id));
  cfg.store.setProperty(key, JSON.stringify(list.slice(-MAX_REMEMBERED)));
}

/** OUT: switchboard comments on `wake:muse` issues become mail to Muse. */
function outbound_(cfg) {
  var sent = remembered_(cfg, 'sent_comment_ids');
  var issues = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues?state=open&per_page=50&labels=' + encodeURIComponent(WAKE_LABEL));
  var since = new Date(Date.now() - COMMENT_DAYS * 86400000).toISOString();
  var errors = [];
  issues.forEach(function (issue) {
    // A PR is never touched: this Issues-only token can neither remove a PR's label nor comment there, and the
    // 403 on PR #343's label stopped the whole bridge (11-07 to 11-09). The switch never labels a PR now.
    if (issue.pull_request) return;
    try {  // one issue's failure never stops the others; its label stays on, so the next tick retries it
      issueComments_(cfg, issue.number, since).forEach(function (c) {
        var body = c.body || '';
        if (c.user.login !== BOT_LOGIN || body.indexOf('CLAUDE_TO_MUSE:') !== 0) return;
        if (sent.indexOf(String(c.id)) >= 0) return;
        var subject = body.split('\n')[0].replace(/[\r\n]/g, ' ').slice(0, 200);
        GmailApp.sendEmail(cfg.to, subject, body + '\n\n' + c.html_url +
          '\n\nReply by email with the subject `MUSE_TO_CLAUDE: <J-NNN or #N> ...`. For a verdict, end the subject with' +
          ' `review of #N @ <sha>: LAND` (or FIX with command:, input:, expected:, observed: lines in the body).');
        remember_(cfg, 'sent_comment_ids', sent, c.id);
      });
      gh_(cfg, 'DELETE', 'repos/' + cfg.repo + '/issues/' + issue.number + '/labels/' + encodeURIComponent(WAKE_LABEL));
    } catch (e) {
      errors.push('#' + issue.number + ': ' + ((e && e.message) || e));
    }
  });
  if (errors.length) throw new Error(errors.length + ' of ' + issues.length + ' wake:muse issue(s) failed; ' + errors.join(' | '));
}

/** An issue's comments updated since `since`, every page up to MAX_COMMENT_PAGES (the switch's own are page 2+ on a long thread). */
function issueComments_(cfg, number, since) {
  var out = [];
  for (var page = 1; page <= MAX_COMMENT_PAGES; page++) {
    var got = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues/' + number + '/comments?per_page=100&page=' + page +
      '&since=' + encodeURIComponent(since)) || [];
    out = out.concat(got);
    if (got.length < 100) break;
  }
  return out;
}

/**
 * CLOCK: dispatches a real (non-dry) switchboard sweep on trunk every CLOCK_MIN minutes, with ACTIONS_TOKEN.
 * GitHub answers 204 when it accepts; only then is the time kept and HC_CLOCK_URL pinged. A failed dispatch
 * throws, so health_clock records it and a long failure posts the alarm; the next tick tries again.
 */
function clock_(cfg) {
  if (!cfg.actionsToken) {
    console.warn('Muse bridge: ACTIONS_TOKEN is not set, so the clock is off');
    return;
  }
  var last = Number(cfg.store.getProperty('clock_last')) || 0;
  if (Date.now() - last < (CLOCK_MIN - 1) * 60000) return;
  gh_(cfg, 'POST', 'repos/' + cfg.repo + '/actions/workflows/' + WORKFLOW + '/dispatches',
    { ref: TRUNK, inputs: { dry_run: 'false' } });
  cfg.store.setProperty('clock_last', String(Date.now()));
  ping_(cfg, 'HC_CLOCK_URL');
}

/**
 * WATCH: alive means a full non-dry sweep (run-name SWEEP_TITLE; a dry run, a digest or an event run is named
 * otherwise) finished with success in the last DOWN_MIN minutes. It reads at most every CLOCK_MIN minutes, like
 * the clock: one small read (successful dispatched runs, the clock's and the proof's); scheduled ones only when
 * those show none. Down: one SWITCHBOARD DOWN comment on the board and one mail to this account per incident;
 * back: one SWITCHBOARD UP line.
 */
function watchdog_(cfg) {
  if (!cfg.actionsToken) {
    console.warn('Muse bridge: ACTIONS_TOKEN is not set, so the switchboard watchdog is off');
    return;
  }
  var now = Date.now();
  if (now - (Number(cfg.store.getProperty('watch_last')) || 0) < (CLOCK_MIN - 1) * 60000) return;
  var state = {};
  try { state = JSON.parse(cfg.store.getProperty('watch_switchboard') || '{}'); } catch (e) { state = {}; }
  var fresh = function (r) { return r && now - Date.parse(r.updated_at) < DOWN_MIN * 60000; };
  var ok = newestOk_(sweeps_(cfg, 'workflow_dispatch', true));
  if (!fresh(ok)) {
    var sched = newestOk_(sweeps_(cfg, 'schedule', true));
    if (sched && (!ok || Date.parse(sched.updated_at) > Date.parse(ok.updated_at))) ok = sched;
  }
  cfg.store.setProperty('watch_last', String(now));
  var board = 'repos/' + cfg.repo + '/issues/' + BOARD_ISSUE + '/comments';
  if (fresh(ok)) {
    if (state.down) {  // a failed post throws and leaves the incident open, so the UP line is retried
      gh_(cfg, 'POST', board, { body: UP_HEADER + ' a non-dry sweep succeeded at ' + central_(ok.updated_at) +
        ' (' + ok.html_url + '). The SWITCHBOARD DOWN of ' + central_(state.since) + ' is over.' });
      cfg.store.setProperty('watch_switchboard', '{}');
    }
    return;
  }
  if (state.down) return;  // one alarm per incident
  var newest = sweeps_(cfg, 'workflow_dispatch', false)[0];  // the alarm alone pays this read: the newest sweep run, any outcome
  var text = DOWN_HEADER + ' no full non-dry sweep has succeeded in ' + DOWN_MIN + ' min (' +
    (ok ? 'the last at ' + central_(ok.updated_at) : 'none in the runs read') + ').\n\n' +
    (newest ? 'Newest sweep run: started ' + central_(newest.created_at) + ', ' + newest.status +
      (newest.conclusion ? ' / ' + newest.conclusion : '') + ', ' + newest.html_url + '\n\n' : '') +
    'The bridge dispatches one every ' + CLOCK_MIN + ' min; a red or missing run is in the Actions tab. ' +
    'One alarm per incident; a SWITCHBOARD UP line follows when a sweep succeeds.';
  var errors = [];
  var told = false;
  try { gh_(cfg, 'POST', board, { body: text }); told = true; } catch (e) { errors.push('board post: ' + ((e && e.message) || e)); }
  try {
    MailApp.sendEmail(cfg.me, DOWN_SUBJECT + ': no successful sweep in ' + DOWN_MIN + ' min', text);
    told = true;
  } catch (e) {
    errors.push('mail: ' + ((e && e.message) || e));
  }
  if (told) cfg.store.setProperty('watch_switchboard', JSON.stringify({ down: true, since: new Date(now).toISOString() }));
  if (errors.length) throw new Error('SWITCHBOARD DOWN alarm: ' + errors.join(' | '));
}

/**
 * The newest 5 switchboard runs on trunk for one event (successful ones only when `okOnly`), newest first, kept
 * only when named as a full non-dry sweep. Five keeps the answer small (a page of 30 runs is about 430 KB).
 */
function sweeps_(cfg, event, okOnly) {
  var got = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/actions/workflows/' + WORKFLOW + '/runs?branch=' + TRUNK +
    '&event=' + event + (okOnly ? '&status=success' : '') + '&per_page=5&exclude_pull_requests=true') || {};
  return (got.workflow_runs || []).filter(function (r) { return r.display_title === SWEEP_TITLE; });
}

function newestOk_(runs) {
  var best = null;
  runs.forEach(function (r) {
    if (r.status === 'completed' && r.conclusion === 'success' &&
        (!best || Date.parse(r.updated_at) > Date.parse(best.updated_at))) best = r;
  });
  return best;
}

function central_(stamp) {
  return Utilities.formatDate(new Date(stamp), 'America/Chicago', 'MMM d h:mm a') + ' Central';
}

/**
 * A Healthchecks.io heartbeat: GET the URL in script property `name`, if set. It never throws and never logs the
 * URL (it is a secret: anyone holding it can ping the check).
 */
function ping_(cfg, name) {
  var url = cfg.store.getProperty(name);
  if (!url) return;
  if (!/^https:\/\//.test(url)) {
    console.warn('Muse bridge: ' + name + ' is not an https URL, so it was not pinged');
    return;
  }
  try {
    var code = UrlFetchApp.fetch(url, { method: 'get', muteHttpExceptions: true }).getResponseCode();
    if (code !== 200) console.warn('Muse bridge: the ' + name + ' ping answered HTTP ' + code);
  } catch (e) {
    console.warn('Muse bridge: the ' + name + ' ping failed: ' + clean_(cfg, e));
  }
}

/**
 * IN: MUSE_TO_CLAUDE mail becomes a comment on the issue or PR it names, oldest first. The window runs from the
 * stored mark (less INBOUND_OVERLAP_MIN) to now, never further back than INBOUND_FIRST_DAYS; a window with
 * MAX_SEARCH threads or more is halved until it fits. A message is handled once: only when it is dated inside
 * the window AND its id is not in posted_message_ids (every message in the overlap was remembered when it was
 * handled, and far fewer than MAX_REMEMBERED fit in one overlap). The mark moves to the window's end, or to the
 * first message left over (failed, or past MAX_POSTS this tick), so nothing older than the mark is skipped.
 */
function inbound_(cfg) {
  var done = remembered_(cfg, 'posted_message_ids');
  var now = Date.now();
  var floor = Math.max(now - INBOUND_FIRST_DAYS * 86400000, INBOUND_EPOCH);
  var mark = Number(cfg.store.getProperty('inbound_mark')) || 0;
  if (mark && mark < floor) console.warn('Muse bridge: the inbound mark is older than ' + INBOUND_FIRST_DAYS + ' days; older mail is not read');
  var from = Math.max(mark ? mark - INBOUND_OVERLAP_MIN * 60000 : floor, floor);
  var to = now;
  var threads = inboundSearch_(from, to);
  for (var n = 0; threads.length >= MAX_SEARCH; n++) {
    if (n >= MAX_NARROW) {
      throw new Error(MAX_SEARCH + '+ MUSE_TO_CLAUDE threads from ' + new Date(from).toISOString() + ' to ' +
        new Date(to).toISOString() + '; the mark stays, so nothing is skipped');
    }
    to = from + Math.floor((to - from) / 2);
    threads = inboundSearch_(from, to);
  }
  var items = [];
  threads.forEach(function (thread) {
    thread.getMessages().forEach(function (m) {
      var t = m.getDate().getTime();
      if (t < from || t >= to || done.indexOf(m.getId()) >= 0) return;
      if ((m.getSubject() || '').indexOf('MUSE_TO_CLAUDE:') < 0) return;
      items.push({ m: m, t: t, thread: thread });
    });
  });
  items.sort(function (a, b) { return a.t - b.t; });
  var jobs = null;
  var jobList = function () {
    if (jobs === null) jobs = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues?state=open&labels=job&per_page=100');
    return jobs;
  };
  var next = to;
  var errors = [];
  var handled = {};
  var left = {};
  items.forEach(function (it, i) {
    var id = it.thread.getId();
    if (i >= MAX_POSTS) {  // the rest wait for the next tick
      next = Math.min(next, it.t);
      left[id] = true;
      return;
    }
    try {  // one message's failure never stops the others; handled ones are remembered, so a retry posts no twin
      inboundMessage_(cfg, it.m, done, jobList);
      handled[id] = it.thread;
    } catch (e) {
      errors.push('message ' + it.m.getId() + ': ' + ((e && e.message) || e));
      next = Math.min(next, it.t);
      left[id] = true;
    }
  });
  var doneLabel = null;
  Object.keys(handled).forEach(function (id) {
    if (left[id]) return;
    doneLabel = doneLabel || GmailApp.getUserLabelByName(DONE_LABEL) || GmailApp.createLabel(DONE_LABEL);
    handled[id].addLabel(doneLabel);
  });
  cfg.store.setProperty('inbound_mark', String(next));
  if (errors.length) throw new Error(errors.length + ' of ' + items.length + ' MUSE_TO_CLAUDE message(s) failed; ' + errors.join(' | '));
}

/** MUSE_TO_CLAUDE threads with a message dated from `from` to `to` (ms), newest first, at most MAX_SEARCH. */
function inboundSearch_(from, to) {
  return GmailApp.search('subject:"MUSE_TO_CLAUDE" after:' + Math.floor(from / 1000) + ' before:' + (Math.floor(to / 1000) + 1), 0, MAX_SEARCH);
}

function inboundMessage_(cfg, m, done, jobList) {
  var subject = m.getSubject() || '';
  var at = subject.indexOf('MUSE_TO_CLAUDE:');
  if (!authentic_(cfg, m)) {
    remember_(cfg, 'posted_message_ids', done, m.getId());  // not provably from an allowed sender: never posted
    console.warn('Muse bridge: skipped message ' + m.getId() + ': sender not allowed or not authenticated');
    return;
  }
  var rest = subject.slice(at + 'MUSE_TO_CLAUDE:'.length).replace(/[*\r\n`]/g, ' ').replace(/\s+/g, ' ').trim();
  var target = 0;
  var num = rest.match(/#(\d{1,6})\b/);
  var job = rest.match(/\bJ-\d{3,}[a-z]?\b/);  // J-044b is its own job, like the switchboard reads it
  var pr = num ? pullOf_(cfg, Number(num[1])) : null;
  var byJob = function (id) {
    jobList().forEach(function (it) { if (!target && it.title.indexOf(id + ' ') === 0) target = it.number; });
  };
  if (num && !pr) {
    target = Number(num[1]);
  } else if (pr) {
    // A PR: its job issue, found the way the switch finds it (the PR's `Job:` line, then a job title naming
    // #N), so the switch reads the verdict there; then the subject's J-NNN.
    var line = String(pr.body || '').match(/^\s*Job:\s*(J-\d{3,}[a-z]?)\b/m);
    if (line) byJob(line[1]);
    var named = new RegExp('#' + num[1] + '\\b');
    jobList().forEach(function (it) { if (!target && named.test(it.title)) target = it.number; });
    if (!target && job) byJob(job[0]);
  } else if (job) {
    byJob(job[0]);
  }
  if (!target) target = BOARD_ISSUE;  // the first line keeps `#N` and `review of #N @ sha`, so the switch still reads it
  var when = Utilities.formatDate(m.getDate(), 'America/Chicago', 'MMM d h:mm a') + ' Central';  // a mail can post days late
  var text = stripQuoted_(m.getPlainBody() || '').slice(0, MAX_BODY);
  gh_(cfg, 'POST', 'repos/' + cfg.repo + '/issues/' + target + '/comments',
    { body: '**Muse (via email bridge): ' + rest + '**\n\n' + text + '\n\n_Mailed ' + when + '._' });
  remember_(cfg, 'posted_message_ids', done, m.getId());
}

/**
 * #number as a pull request (its issue record, body included), or null when it is an issue. This token cannot
 * comment on a PR, so mail about one goes on its job issue or the board. A 403 or 404 on the read counts as a
 * PR (the token may not see it), with no body; any other error throws,
 * so the thread is retried next tick rather than posted in the wrong place.
 */
function pullOf_(cfg, number) {
  try {
    var it = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues/' + number);
    return it && it.pull_request ? it : null;
  } catch (e) {
    if (/HTTP (403|404)\b/.test(String((e && e.message) || e))) return {};
    throw e;
  }
}

/**
 * True only when the mail provably comes from this account itself; anyone can forge the From line.
 * Such mail carries Gmail's SENT label, which a forged From arriving from outside never does. No
 * other sender is accepted: an Authentication-Results header can be prepended by whoever sent the
 * mail, so it proves nothing here.
 */
function authentic_(cfg, m) {
  var addr = senderAddress_(m.getFrom());
  if (addr !== cfg.me || cfg.from.indexOf(addr) < 0) return false;
  if (typeof Gmail === 'undefined') {
    throw new Error('Muse bridge: add the Gmail API service (Editor > Services) so mail can be checked');
  }
  var meta = Gmail.Users.Messages.get('me', m.getId(), { format: 'minimal' });
  return (meta.labelIds || []).indexOf('SENT') >= 0;
}

function senderAddress_(from) {
  var m = String(from).match(/<([^>]+)>/);
  return (m ? m[1] : String(from)).trim().toLowerCase();
}

/** Drops the quoted earlier mail under a reply, so only Muse's new words are posted. */
function stripQuoted_(text) {
  var out = [];
  var lines = text.split(/\r?\n/);
  for (var i = 0; i < lines.length; i++) {
    if (/^On .+ wrote:\s*$/.test(lines[i])) break;
    if (/^>/.test(lines[i])) continue;
    out.push(lines[i]);
  }
  return out.join('\n').trim();
}
