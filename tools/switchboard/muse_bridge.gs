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
 * Setup (once): Editor > Services (+) > add "Gmail API" (identifier Gmail); Project Settings > Script
 * properties > add GITHUB_TOKEN; then run `setup` and allow. Keep the project unshared: anyone with
 * edit access can read the script properties.
 * Script properties:
 *   GITHUB_TOKEN  required. A fine-grained token, repository IssisX/ScraperX only, with
 *                 Issues: Read and write only (Metadata: Read comes with it). No Pull requests
 *                 permission: it would let the token submit reviews, which the switch reads.
 *   REPO          optional, default IssisX/ScraperX
 *   MUSE_TO       optional, where Muse reads mail; default this account's own address
 *   MUSE_FROM     optional; only this account's own address can ever post (see authentic_), so a
 *                 list without it posts nothing
 * The token is read from the properties only, sent only to api.github.com, and never logged.
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
var MAX_THREADS = 20;
var MAX_COMMENT_PAGES = 10;     // pages of 100 comments read per wake:muse issue
var COMMENT_DAYS = 7;           // only comments this new are mailed: an id aged out of the remembered list is never re-sent
var HEALTH_ALARM_MIN = 30;      // a direction failing this long posts BRIDGE DOWN on the board, once
var ALARM_HEADER = 'Claude (integrator): BRIDGE DOWN:';
var DIRECTIONS = { out: 'outbound (wake:muse comments mailed to Muse)', in: 'inbound (MUSE_TO_CLAUDE mail posted to GitHub)' };

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
    run_(cfg, 'out', outbound_);  // each direction alone: an outbound throw never stops inbound (11-07 to 11-09)
    run_(cfg, 'in', inbound_);
  } finally {
    lock.releaseLock();
  }
}

/** One direction, caught: its success or failure goes in health_<dir>, and a long failure posts one alarm. */
function run_(cfg, dir, fn) {
  try {
    fn(cfg);
  } catch (e) {
    failed_(cfg.store, cfg, dir, e);
    return;
  }
  var h = health_(cfg.store, dir);
  h.lastOk = new Date().toISOString();
  h.failingSince = null;
  h.alarmed = false;
  cfg.store.setProperty('health_' + dir, JSON.stringify(h));
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
  if (cfg && cfg.token) text = text.split(cfg.token).join('[token]');
  text = text.replace(/(gh[pousr]_|github_pat_)[A-Za-z0-9_]+/g, '[token]');
  text = text.replace(/[\r\n\t]+/g, ' ').replace(/[@`<>*]/g, ' ').replace(/:(?= ?(?:land|fix|note|block))/gi, ' -');
  return text.replace(/\s+/g, ' ').trim().slice(0, 200);
}

function props_() {
  var p = PropertiesService.getScriptProperties();
  var token = p.getProperty('GITHUB_TOKEN');
  if (!token) throw new Error('Muse bridge: set the GITHUB_TOKEN script property first');
  var me = Session.getEffectiveUser().getEmail();
  return {
    store: p,
    token: token,
    me: me.trim().toLowerCase(),
    repo: p.getProperty('REPO') || DEFAULT_REPO,
    to: p.getProperty('MUSE_TO') || me,
    from: (p.getProperty('MUSE_FROM') || me).split(',').map(function (s) { return s.trim().toLowerCase(); })
  };
}

function gh_(cfg, method, path, body) {
  var opts = {
    method: method,
    muteHttpExceptions: true,
    headers: {
      Authorization: 'Bearer ' + cfg.token,
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

/** IN: MUSE_TO_CLAUDE mail becomes a comment on the issue or PR it names. */
function inbound_(cfg) {
  var done = remembered_(cfg, 'posted_message_ids');
  var doneLabel = GmailApp.getUserLabelByName(DONE_LABEL) || GmailApp.createLabel(DONE_LABEL);
  var threads = GmailApp.search('subject:"MUSE_TO_CLAUDE" newer_than:2d', 0, MAX_THREADS);
  var jobs = null;
  var errors = [];
  threads.forEach(function (thread) {
    try {  // one thread's failure never stops the others; posted messages are remembered, so a retry posts no twin
      inboundThread_(cfg, thread, done, doneLabel, function () {
        if (jobs === null) jobs = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues?state=open&labels=job&per_page=100');
        return jobs;
      });
    } catch (e) {
      errors.push('thread ' + thread.getId() + ': ' + ((e && e.message) || e));
    }
  });
  if (errors.length) throw new Error(errors.length + ' of ' + threads.length + ' MUSE_TO_CLAUDE thread(s) failed; ' + errors.join(' | '));
}

function inboundThread_(cfg, thread, done, doneLabel, jobList) {
  thread.getMessages().forEach(function (m) {
    if (done.indexOf(m.getId()) >= 0) return;
    var subject = m.getSubject() || '';
    var at = subject.indexOf('MUSE_TO_CLAUDE:');
    if (at < 0) return;
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
    var when = Utilities.formatDate(m.getDate(), 'America/Chicago', 'h:mm a') + ' Central';
    var text = stripQuoted_(m.getPlainBody() || '').slice(0, MAX_BODY);
    gh_(cfg, 'POST', 'repos/' + cfg.repo + '/issues/' + target + '/comments',
      { body: '**Muse (via email bridge): ' + rest + '**\n\n' + text + '\n\n_Mailed ' + when + '._' });
    remember_(cfg, 'posted_message_ids', done, m.getId());
  });
  thread.addLabel(doneLabel);
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
