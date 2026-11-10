// Smoke tests of the Muse bridge (muse_bridge.gs) under fake Apps Script services, J-072.
// Run: node tools/switchboard/test_bridge_smoke.js (test_bridge.py runs it in the switchboard's unit tests when
// node is on the path; the ubuntu-24.04 runner has it). No network: GitHub, Gmail and the clock are fakes.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SRC = fs.readFileSync(path.join(__dirname, 'muse_bridge.gs'), 'utf8');
const REPO = 'IssisX/ScraperX';
const BOT = 'github-actions[bot]';
const ME = 'cory@example.com';

function bridge(world) {
  const props = Object.assign({ GITHUB_TOKEN: 'ghp_fake', ACTIONS_TOKEN: 'ghp_actions' }, world.props || {});
  const calls = [];
  const mails = [];
  const ctx = {
    console: { log() {}, warn() {}, error() {} },
    JSON, Date, Math, String, Number, Object, Array, RegExp, Error, encodeURIComponent,
    PropertiesService: { getScriptProperties: () => ({
      getProperty: (k) => (k in props ? props[k] : null),
      setProperty: (k, v) => { props[k] = String(v); } }) },
    Session: { getEffectiveUser: () => ({ getEmail: () => ME }) },
    LockService: { getScriptLock: () => ({ tryLock: () => true, releaseLock() {} }) },
    Utilities: { formatDate: (d) => new Date(d).toISOString() },
    MailApp: { sendEmail: (to, subject, body) => mails.push({ to, subject, body, via: 'MailApp' }) },
    GmailApp: {
      sendEmail: (to, subject, body) => { mails.push({ to, subject, body }); if (world.onMail) world.onMail(subject); },
      search: () => world.threads || [],
      getUserLabelByName: () => ({}),
      createLabel: () => ({})
    },
    Gmail: { Users: { Messages: { get: () => ({ labelIds: ['SENT'] }) } } },
    UrlFetchApp: { fetch: (url, opts) => {
      const p = url.replace('https://api.github.com/', '');
      const method = opts.method;
      const body = opts.payload ? JSON.parse(opts.payload) : undefined;
      calls.push({ method, path: p, body, auth: opts.headers && opts.headers.Authorization });
      const got = world.github(method, p, body);
      const code = got === undefined ? 404 : (got && got.__code) || 200;
      const text = got === null || got === undefined ? '' : JSON.stringify(got);
      return { getResponseCode: () => code, getContentText: () => text };
    } }
  };
  vm.createContext(ctx);
  vm.runInContext(SRC, ctx);
  return { ctx, calls, mails, props, cfg: () => ctx.props_() };
}

function comment(id, body, at) {
  return { id, body, user: { login: BOT }, created_at: at || new Date().toISOString() };
}

const results = [];
function test(name, fn) {
  try {
    fn();
    results.push(`PASS ${name}`);
  } catch (e) {
    results.push(`FAIL ${name}: ${e && e.message}`);
  }
}
function assert(ok, what) { if (!ok) throw new Error(what); }

test('a CLAUDE_TO_MUSE posted while the bridge mails is mailed by the next tick (the label race)', () => {
  const comments = [comment(1, 'CLAUDE_TO_MUSE: J-001 first')];
  let labeled = true;
  const world = {
    onMail: (subject) => {   // the switch posts C2 mid-tick; its label POST is a no-op while the label is on
      if (subject.indexOf('first') >= 0) comments.push(comment(2, 'CLAUDE_TO_MUSE: J-001 second'));
    },
    github: (method, p, body) => {
      if (method === 'GET' && p.indexOf('/issues?state=open') >= 0) return labeled ? [{ number: 340 }] : [];
      if (method === 'GET' && /issues\/340\/comments/.test(p)) return comments.slice();
      if (method === 'DELETE' && /labels\/wake%3Amuse$/.test(p)) { labeled = false; return null; }
      if (method === 'POST' && /issues\/340\/labels$/.test(p)) { labeled = true; return []; }
      return undefined;
    }
  };
  const b = bridge(world);
  b.ctx.outbound_(b.cfg());
  b.ctx.outbound_(b.cfg());
  const subjects = b.mails.map((m) => m.subject);
  assert(subjects.length === 2 && subjects[1].indexOf('second') >= 0, 'mailed: ' + JSON.stringify(subjects));
  assert(!labeled, 'the label is off once both are mailed');
});

test('mail naming a closed issue is posted on the board, not the closed issue', () => {
  const msg = {
    getId: () => 'm1', getDate: () => new Date(Date.now() - 60000), getSubject: () => 'MUSE_TO_CLAUDE: #107 resend',
    getFrom: () => ME, getPlainBody: () => 'Please resend.'
  };
  const world = {
    threads: [{ getId: () => 't1', getMessages: () => [msg], addLabel() {} }],
    github: (method, p) => {
      if (method === 'GET' && p === `repos/${REPO}/issues/107`) return { number: 107, state: 'closed' };
      if (method === 'GET' && p.indexOf('/issues?state=open&labels=job') >= 0) return [];
      if (method === 'POST' && /issues\/\d+\/comments$/.test(p)) return { id: 9 };
      return undefined;
    }
  };
  const b = bridge(world);
  b.ctx.inbound_(b.cfg());
  const posts = b.calls.filter((c) => c.method === 'POST').map((c) => c.path);
  assert(posts.length === 1 && posts[0] === `repos/${REPO}/issues/340/comments`, 'posted to: ' + posts);
});

test('mail naming an open issue still goes there', () => {
  const msg = {
    getId: () => 'm2', getDate: () => new Date(Date.now() - 60000), getSubject: () => 'MUSE_TO_CLAUDE: #350 done',
    getFrom: () => ME, getPlainBody: () => 'Done.'
  };
  const world = {
    threads: [{ getId: () => 't2', getMessages: () => [msg], addLabel() {} }],
    github: (method, p) => {
      if (method === 'GET' && p === `repos/${REPO}/issues/350`) return { number: 350, state: 'open' };
      if (method === 'POST' && /issues\/\d+\/comments$/.test(p)) return { id: 9 };
      return undefined;
    }
  };
  const b = bridge(world);
  b.ctx.inbound_(b.cfg());
  const posts = b.calls.filter((c) => c.method === 'POST').map((c) => c.path);
  assert(posts.length === 1 && posts[0] === `repos/${REPO}/issues/350/comments`, 'posted to: ' + posts);
});

test('the clock sends BRIDGE_VERSION with each dispatch', () => {
  const b = bridge({ github: (method, p) => (method === 'POST' && /dispatches$/.test(p) ? null : undefined) });
  b.ctx.clock_(b.cfg());
  const d = b.calls.find((c) => /dispatches$/.test(c.path));
  assert(/^[0-9a-f]{12}$/.test(String(b.ctx.BRIDGE_VERSION)), 'BRIDGE_VERSION: ' + b.ctx.BRIDGE_VERSION);
  assert(d && d.body.inputs.bridge_version === b.ctx.BRIDGE_VERSION && d.body.inputs.dry_run === 'false',
    'dispatch: ' + JSON.stringify(d && d.body));
});

function runsWorld(okRuns, newest) {
  return (method, p) => {
    if (method === 'GET' && /actions\/workflows\/switchboard\.yml\/runs/.test(p)) {
      return { workflow_runs: p.indexOf('status=success') >= 0 ? okRuns : newest };
    }
    if (method === 'POST' && /issues\/340\/comments$/.test(p)) return { id: 1 };
    return undefined;
  };
}

test('a successful sweep the clock named with its version counts as alive', () => {
  const now = new Date().toISOString();
  const ok = [{ display_title: 'Switchboard sweep (bridge 0123456789ab)', status: 'completed', conclusion: 'success',
    updated_at: now, created_at: now, html_url: 'u' }];
  const b = bridge({ github: runsWorld(ok, ok) });
  b.ctx.watchdog_(b.cfg());
  assert(!b.calls.some((c) => c.method === 'POST'), 'no alarm');
});

test('an open incident does not hide a new kind of outage', () => {
  const old = new Date(Date.now() - 3 * 3600000).toISOString();
  const failed = [{ display_title: 'Switchboard sweep', status: 'completed', conclusion: 'failure',
    updated_at: old, created_at: old, html_url: 'u' }];
  const b = bridge({
    github: runsWorld([], failed),
    props: { watch_switchboard: JSON.stringify({ down: true, kind: 'failing', since: old }) }
  });
  b.ctx.watchdog_(b.cfg());   // the newest sweep is 3 h old: no sweep starts at all now, a new incident
  const posts = b.calls.filter((c) => c.method === 'POST');
  assert(posts.length === 1 && posts[0].body.body.indexOf('A new incident') >= 0, 'posts: ' + JSON.stringify(posts));
  assert(JSON.parse(b.props.watch_switchboard).kind === 'missing', 'state: ' + b.props.watch_switchboard);
  b.props.watch_last = '0';
  b.ctx.watchdog_(b.cfg());   // the same incident again: no second alarm
  assert(b.calls.filter((c) => c.method === 'POST').length === 1, 'one alarm per incident');
});

// J-072: the board is the open issue labeled `board`, looked up at run time. #107 to #340 (11-09) needed an edit
// and a paste of this script, and until then it posted on the closed board.
const BOARDS = `repos/${REPO}/issues?labels=board&state=open&per_page=100`;

function mail(id, subject) {
  return { getId: () => id, getDate: () => new Date(Date.now() - 60000), getSubject: () => subject,
    getFrom: () => ME, getPlainBody: () => 'Body.' };
}

// GitHub with `boards` as the answer to the board lookup (a function is called each time), no job issue, #107 closed
function boardWorld(boards, ids, extra) {
  return Object.assign({
    threads: (ids || ['m1']).map((id) => ({ getId: () => 't' + id, getMessages: () => [mail(id, 'MUSE_TO_CLAUDE: #107 resend')], addLabel() {} })),
    github: (method, p) => {
      if (method === 'GET' && p === BOARDS) return typeof boards === 'function' ? boards() : boards;
      if (method === 'GET' && p === `repos/${REPO}/issues/107`) return { number: 107, state: 'closed' };
      if (method === 'GET' && p.indexOf('/issues?state=open&labels=job') >= 0) return [];
      if (method === 'POST' && /issues\/\d+\/comments$/.test(p)) return { id: 9 };
      return undefined;
    }
  }, extra || {});
}
const posted = (b) => b.calls.filter((c) => c.method === 'POST').map((c) => c.path);
const lookups = (b) => b.calls.filter((c) => c.path === BOARDS);
const minutesAgo = (m) => Date.now() - m * 60000;

test('after the board rolls over, mail for the board goes on the new open issue labeled board', () => {
  const b = bridge(boardWorld([{ number: 400 }]));   // #340 closed at 100 comments; #400 is open and labeled
  b.ctx.inbound_(b.cfg());
  assert(posted(b).length === 1 && posted(b)[0] === `repos/${REPO}/issues/400/comments`, 'posted to: ' + posted(b));
  assert(JSON.parse(b.props.board).n === 400, 'kept: ' + b.props.board);
  assert(lookups(b).length === 1 && lookups(b)[0].auth === 'Bearer ghp_fake', 'the lookup carries the Issues token only');
  assert(!b.calls.some((c) => c.auth !== 'Bearer ghp_fake'), 'no call here carries the Actions token');
});

test('the board is looked up once a tick, then not again for BOARD_CACHE_MIN minutes', () => {
  const b = bridge(boardWorld([{ number: 400 }], ['m1', 'm2', 'm3']));
  b.ctx.inbound_(b.cfg());           // three mails for the board in one tick
  assert(posted(b).length === 3 && lookups(b).length === 1, 'lookups in one tick: ' + lookups(b).length);
  b.props.posted_message_ids = '[]';
  b.props.inbound_mark = '';
  b.ctx.inbound_(b.cfg());           // the next tick, minutes later: the kept board, no read
  assert(posted(b).length === 6 && lookups(b).length === 1, 'lookups in the next tick: ' + lookups(b).length);
  b.props.board = JSON.stringify({ n: 400, at: minutesAgo(b.ctx.BOARD_CACHE_MIN + 1) });
  b.ctx.board_(b.cfg());
  assert(lookups(b).length === 2, 'looked up again once the kept one is old');
});

test('a tick with nothing for the board never looks it up', () => {
  const now = new Date().toISOString();
  const ok = [{ display_title: 'Switchboard sweep', status: 'completed', conclusion: 'success', updated_at: now,
    created_at: now, html_url: 'u' }];
  const b = bridge({ threads: [], github: (method, p) => {
    if (method === 'GET' && /actions\/workflows\/switchboard\.yml\/runs/.test(p)) return { workflow_runs: ok };
    if (method === 'GET' && p.indexOf('/issues?state=open') >= 0) return [];
    if (method === 'POST' && /dispatches$/.test(p)) return null;
    return undefined;
  } });
  b.ctx.tick();
  assert(b.calls.length > 0 && lookups(b).length === 0, 'calls: ' + b.calls.map((c) => c.path));
});

test('two open boards: the newest; a pull request carrying the label is never the board', () => {
  const b = bridge(boardWorld([{ number: 340 }, { number: 400 }, { number: 999, pull_request: { url: 'x' } }]));
  assert(b.ctx.board_(b.cfg()) === 400, 'board: ' + b.ctx.board_(b.cfg()));
  const onlyPr = bridge(boardWorld([{ number: 999, pull_request: { url: 'x' } }]));
  assert(onlyPr.ctx.board_(onlyPr.cfg()) === 340, 'a PR alone is no board');
});

test('a failed or empty lookup keeps the last board found, else the built-in one, and never throws', () => {
  const kept = { board: JSON.stringify({ n: 400, at: minutesAgo(60) }) };
  [{ __code: 500 }, [], { message: 'not a list' }, [{ number: '401' }, null]].forEach((answer) => {
    const b = bridge(boardWorld(answer, ['m1'], { props: kept }));
    b.ctx.inbound_(b.cfg());
    assert(posted(b)[0] === `repos/${REPO}/issues/400/comments`, JSON.stringify(answer) + ' posted to: ' + posted(b));
    assert(JSON.parse(b.props.board).n === 400, 'the last board is kept');
    const cold = bridge(boardWorld(answer));
    cold.ctx.inbound_(cold.cfg());
    assert(posted(cold)[0] === `repos/${REPO}/issues/340/comments`, JSON.stringify(answer) + ' cold posted to: ' + posted(cold));
  });
  const junk = bridge(boardWorld([], ['m1'], { props: { board: '{not json' } }));
  assert(junk.ctx.board_(junk.cfg()) === 340, 'a broken property is no board');
});

test('the SWITCHBOARD DOWN and BRIDGE DOWN alarms go on the looked-up board', () => {
  const old = new Date(Date.now() - 3 * 3600000).toISOString();
  const failed = [{ display_title: 'Switchboard sweep', status: 'completed', conclusion: 'failure',
    updated_at: old, created_at: old, html_url: 'u' }];
  const runs = runsWorld([], failed);
  const github = (method, p) => {
    if (method === 'GET' && p === BOARDS) return [{ number: 400 }];
    if (method === 'POST' && /issues\/400\/comments$/.test(p)) return { id: 1 };
    return runs(method, p);
  };
  const b = bridge({ github });
  b.ctx.watchdog_(b.cfg());
  assert(posted(b).length === 1 && posted(b)[0] === `repos/${REPO}/issues/400/comments`, 'SWITCHBOARD DOWN on: ' + posted(b));
  const d = bridge({ github, props: { health_out: JSON.stringify({ failingSince: old }) } });
  const cfg = d.cfg();
  d.ctx.failed_(cfg.store, cfg, 'out', new Error('boom'));
  assert(posted(d).length === 1 && posted(d)[0] === `repos/${REPO}/issues/400/comments`, 'BRIDGE DOWN on: ' + posted(d));
  assert(JSON.parse(d.props.health_out).alarmed === true, 'alarmed once');
});

results.forEach((r) => console.log(r));
const failed = results.filter((r) => r.indexOf('FAIL') === 0).length;
console.log(`BRIDGE SMOKE: ${results.length - failed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
