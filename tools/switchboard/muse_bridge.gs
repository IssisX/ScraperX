/**
 * ScraperX Muse bridge (design v3 step B, job J-031). Google Apps Script on the owner's account.
 *
 * Both directions, every five minutes:
 *   OUT  The switchboard asks Muse by posting a comment that starts `CLAUDE_TO_MUSE:` on an issue and
 *        labeling the issue `wake:muse`. This script mails each such comment it has not sent yet to
 *        Muse, then removes the label.
 *   IN   Mail whose subject starts `MUSE_TO_CLAUDE:` (from an allowed sender, proven by more than the
 *        forgeable From line: see authentic_) is posted as a comment:
 *        on #N when the subject names `#N`, else on the job issue `J-NNN` the subject names, else on
 *        the board #340. Its first line is `**Muse (via email bridge): <subject>**`, so a subject like
 *        `MUSE_TO_CLAUDE: review of #100 @ 1a2b3c4d: LAND` is a verdict the switch reads.
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
 */

var DEFAULT_REPO = 'IssisX/ScraperX';
var BOARD_ISSUE = 340;
var WAKE_LABEL = 'wake:muse';
var DONE_LABEL = 'muse-bridged';
var BOT_LOGIN = 'github-actions[bot]';
var MAX_BODY = 60000;
var MAX_REMEMBERED = 500;
var MAX_THREADS = 20;

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
    outbound_();
    inbound_();
  } finally {
    lock.releaseLock();
  }
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
function outbound_() {
  var cfg = props_();
  var sent = remembered_(cfg, 'sent_comment_ids');
  var issues = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues?state=open&per_page=50&labels=' + encodeURIComponent(WAKE_LABEL));
  issues.forEach(function (issue) {
    var comments = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues/' + issue.number + '/comments?per_page=100');
    comments.forEach(function (c) {
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
  });
}

/** IN: MUSE_TO_CLAUDE mail becomes a comment on the issue or PR it names. */
function inbound_() {
  var cfg = props_();
  var done = remembered_(cfg, 'posted_message_ids');
  var doneLabel = GmailApp.getUserLabelByName(DONE_LABEL) || GmailApp.createLabel(DONE_LABEL);
  var threads = GmailApp.search('subject:"MUSE_TO_CLAUDE" newer_than:2d', 0, MAX_THREADS);
  var jobs = null;
  threads.forEach(function (thread) {
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
      var job = rest.match(/\bJ-\d{3,}\b/);
      if (num) {
        target = Number(num[1]);
      } else if (job) {
        if (jobs === null) jobs = gh_(cfg, 'GET', 'repos/' + cfg.repo + '/issues?state=open&labels=job&per_page=100');
        jobs.forEach(function (it) { if (!target && it.title.indexOf(job[0] + ' ') === 0) target = it.number; });
      }
      if (!target) target = BOARD_ISSUE;
      var when = Utilities.formatDate(m.getDate(), 'America/Chicago', 'h:mm a') + ' Central';
      var text = stripQuoted_(m.getPlainBody() || '').slice(0, MAX_BODY);
      gh_(cfg, 'POST', 'repos/' + cfg.repo + '/issues/' + target + '/comments',
        { body: '**Muse (via email bridge): ' + rest + '**\n\n' + text + '\n\n_Mailed ' + when + '._' });
      remember_(cfg, 'posted_message_ids', done, m.getId());
    });
    thread.addLabel(doneLabel);
  });
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
