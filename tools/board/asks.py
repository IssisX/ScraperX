#!/usr/bin/env python3
"""Every cross-agent ask with no answer yet, so none is left waiting.

This file is the one judge of asks (J-072): the ledger (tools/switchboard/ledger.py read_feed) and the reconciler
(reconciler.py stream_asks, acks_by_reply) both call read() and answer_rule(), so the two cannot disagree on what
an ask is or on what answers it.

An ask is a comment on an open issue or PR that addresses another agent: a line that opens `To <Name>` (after
quote, list and emphasis marks `> - + * _`, and at most one signature such as `Claude (integrator):` or the
bridge's `**Muse (via email bridge):`), or `<Author> → <Name>` or `cc <Name>` anywhere. `To <Name>` inside a
sentence addresses no one ("both To Grok asks above are done", 6746481113 on #340, sat 19.7 h as an ask to
Grok). A post that names an ask from its addressee (that ask's comment id, URL or O-id, or a quote of it) is that
ask's answer, not a new ask ("To ChatGPT (from Claude, integrator), answering 6752607408 and 6752574323: ..." on
#350 was sent to ChatGPT as an ask). Only a naming reply: a post merely addressed back may be a new ask, and
every mail of Muse's is addressed to Claude (her "number ruling owed", 6746962497 on #312, must stay an ask).
In the oldest day of the comments read, a post that names anything is not listed at all (ASK_CONTEXT): the ask
it may be answering can be out of reach.

Who wrote a post is read from its first line (every agent posts as one login). A post that opens with an address
is signed right after it: `To Claude: Grok Bot, 2:56 PM ...`, `To Claude (from ChatGPT): ...`.

An ask counts as answered only by a later post that addresses it (J-072: Muse's "please resend" 6746462436 on
#340 dropped out when Claude posted about Grok's asks there a minute later). From the addressee:
  - a post anywhere that names the ask itself (its comment id or URL, or the reconciler's id for it, O-xxxxxxxx)
    or quotes it (a `> ` line of its text),
  - a reply on the same thread addressed back to the asker (`To <Asker>`, `→ <Asker>`, `cc <Asker>`; a
    `CLAUDE_TO_MUSE:` post is addressed to Muse, and a bridge post, Muse's mail, to Claude),
  - a reply on another thread naming this one (`#N`) and addressed back to the asker,
  - a verdict (`**<Name> (review of #N @ ...)**`) on a PR the ask names,
  - on a job's own issue or PR (never the board): its next post there addressed to no one but the asker. It
    answers every ask to it on that thread (Grok's one audit post on #322 answered Claude's three asks there),
  - on the board or a thread that names no job: such a post only when its first line names the asker, it names no
    other post or job, and the addressee owes one ask there, not several.
From anyone on the team: a line `/ack O-xxxxxxxx` with the ask's id settles it (the asker withdraws it so).
A post whose text opens with `/ack` (after the bridge's header line) is an ack, never an ask.

An ask to Claude (the integrator) that asks no question (no `?` outside links and code) and says no BLOCKED is
mostly a status line from Muse's mail (11-11: 13 such "asks" sat ESCALATED on #384). Besides the answers above
(Claude's post on a job's thread addressed to no one else, or a bridge record of Claude's mail, `CLAUDE_TO_MUSE:`,
addressed back to Muse), it is closed by:
  - a later post by the asker on the same thread or job (the job id in both openings) that is itself no ask,
  - a PR it names (or its job's PR) merged or closed after it (`closed`, read by the caller from the issue events).
A Claude post there addressed to someone else still answers nothing ("rule on the grip cap", the #360 replay).
An ask to Claude with a question, or that says BLOCKED, stays until one of the answers above answers it.
Any other later post, on the thread or not, is not an answer; the switchboard's own posts never are. Muse answers
by mail; the bridge posts those as `**Muse (via email bridge)...`, which counts as Muse.

Muse never reads GitHub: an ask to Muse is listed with RELAY until the
integrator mails it (`CLAUDE_TO_MUSE:`), then stays listed until Muse answers.
Grok's runs only find comments that START with `To Grok:`; an ask to Grok
buried inside a longer post is listed with REPOST.

Reads GitHub through `gh api`; changes nothing.
Usage: python3 tools/board/asks.py [--repo IssisX/ScraperX] [--hours 48 | --since UNIX_TIME]
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import subprocess
import sys

AGENTS = ("Muse", "ChatGPT", "Grok", "Gemini", "Codex", "Builder", "PE", "Claude", "Switchboard", "Cory")
TARGETS = "Muse|ChatGPT|Grok|Builder|PE|Claude"
SIGNERS = AGENTS + ("Principal Engineer", "FB Bot")   # the names a first line is signed with (author)
_SIGNER = "|".join(re.escape(n) for n in sorted(SIGNERS, key=len, reverse=True))
_MARKS = r"[ \t>*_+\-]*"   # quote, list and emphasis marks before a line's first word
# `To <Name>` where a line opens with it (after the marks and at most one signature: `Claude (integrator):`,
# `**Muse (via email bridge):`); `→ <Name>` and `cc <Name>` anywhere, as before. One group: the name.
# After the signature a job or thread reference may stand before the address: the bridge posts Muse's mail subject
# after its header, and a subject opens with its job (`**Muse (via email bridge): J-044 To Claude: ...`).
ASK = re.compile(r"(?:^" + _MARKS + r"(?:(?:" + _SIGNER + r")(?:[ \t]+Bot)?(?:[ \t]*\([^()\n]*\))?[ \t]*:" + _MARKS
                 + r"(?:(?:J-\d+[a-z]?|#\d+)[ \t]+)?"
                 + r")?To|(?:^|[\s*>(])(?:→|cc))\s+(?:the\s+)?(" + TARGETS + r")\b", re.MULTILINE)
OPENS = re.compile(r"^" + _MARKS + r"To\s+(?:the\s+)?(?:" + TARGETS + r")\b")   # a first line that opens with an address
# ... and the signature right after it: `(from ChatGPT)`, `: Grok Bot, ...`, `: (Claude, integrator) ...`
SIGNED = re.compile(r"[ \t]*(?:\((?:from[ \t]+)?|:[ \t*_]*\(?(?:from[ \t]+)?)(" + _SIGNER + r")\b(?!['’]s)")
VERDICT = re.compile(r"^\*\*(" + TARGETS + r")\b[^\n]*?review of #(\d+)", re.MULTILINE)
PR_REF = re.compile(r"#(\d+)")
MAX_PAGES = 10  # bounded: at most 1000 open issues read
MAX_COMMENT_PAGES = 40  # bounded: at most 4000 comments read; a full last page is reported, never silent (11-14)
NOISE = ("<!-- linear-linkback", "You have reached your Codex")
LEDGER_MARK = "<!-- switchboard:ledger "   # in the switchboard ledger's notices (tools/switchboard/ledger.py)
BRIDGE_POST = "**Muse (via email bridge)"   # the first line of every inbound bridge post: Muse writing to Claude
QUOTE = re.compile(r"^>\s?(.+)$", re.MULTILINE)
MIN_QUOTE = 12   # characters of quoted text that must appear in the ask for a quote to name it
SWITCHBOARD_LOGIN = "github-actions[bot]"   # the switchboard's own login: its posts neither ask nor answer
# A post that names an ask is read as its answer only while that ask is in the stream too. So in the stream's
# oldest day a post that names anything (a comment id or URL, an O-id, a quoted line) is not listed as an ask: the
# ask it may be answering can be out of reach. (Without this "To ChatGPT (from Claude), answering 6752607408 ..."
# would turn into an ask to ChatGPT the hour ChatGPT's own ask left the stream.) An answer that comes more than
# this after the ask it names can still be read as an ask in that gap.
ASK_CONTEXT = dt.timedelta(days=1)
INTEGRATOR = "Claude"   # the holder whose status asks close loosely (see the module docstring)
PR_CLOSED = "its PR closed"   # the rule read() gives an ask closed by a PR it names merging or closing
BLOCKED = re.compile(r"\bBLOCKED\b")   # the team's "BLOCKED: what, needs whom" is a real ask, question mark or not
UNQUOTED = re.compile(r"https?://\S+|`[^`\n]*`")   # a `?` in a link or in code is no question
MAY_NAME = re.compile(r"(?<!\d)\d{9,11}(?!\d)|(?<![\w-])O-[0-9a-f]{8}\b|^>\s?\S", re.MULTILINE)
JOB_ID = re.compile(r"\bJ-\d{3,}[a-z]?\b")


def gh(path: str) -> object:
    """GET one GitHub API path; raises with the path on failure."""
    out = subprocess.run(["gh", "api", path], capture_output=True, text=True, timeout=60)
    if out.returncode != 0:
        raise RuntimeError(f"gh api {path} failed ({out.returncode}): {out.stderr.strip()[:300]}")
    return json.loads(out.stdout)


def author(body: str) -> str:
    """The agent who wrote a comment, from its first line (every agent posts as the same login). "noise" is
    nobody's post: read() never takes it for an ask and answer_rule() never for anyone's reply (the reconciler's
    thread_replies drops it too: no agent is named noise)."""
    text = body.strip()
    if text.startswith(NOISE) or text[:6].lower() == "@codex":
        return "noise"   # a `@codex ...` comment is a command to Codex (the switchboard's, or typed by hand): nobody's post
    if text.startswith("<!-- codex"):
        return "Codex"
    if text.startswith("CLAUDE_TO_MUSE:"):   # the integrator's relay to Muse, posted by it or by the switchboard
        return "Claude"
    first = text.split("\n", 1)[0]
    opens = OPENS.match(first)
    if opens:   # `To Claude: Grok Bot, ...`, `To Claude (from ChatGPT): ...`: signed right after the address, or not
        signed = SIGNED.match(first[opens.end():opens.end() + 160])   # at all (J-072: Grok Bot's read as ChatGPT's)
        head = signed.group(1) if signed else ""
    else:
        head = re.split(r"→|\bTo\b|\bcc\b", first[:160])[0]
    best, pos = "", len(head) + 1
    for name in AGENTS + ("Principal Engineer", "FB Bot"):
        m = re.search(r"\b" + re.escape(name) + r"\b", head)
        if m and m.start() < pos:
            best, pos = name, m.start()
    best = {"Principal Engineer": "PE", "FB Bot": "Muse"}.get(best, best)
    if not best:
        # Claude always ends with its footer, Muse comes through the bridge and Grok and the
        # switchboard name themselves; an unsigned post is ChatGPT's (its file asks it to sign).
        best = "Claude" if "Generated by [Claude Code]" in body else "ChatGPT"
    return best


def addressees(body: str) -> set[str]:
    """Who a post is addressed to: its `To <Name>` / `→ <Name>` / `cc <Name>` lines, Muse for a `CLAUDE_TO_MUSE:`
    post and Claude for a bridge post (Muse's mail is always to Claude)."""
    text = body.lstrip()
    out = {m.group(1) for m in ASK.finditer(body[:4000])}
    if text.startswith("CLAUDE_TO_MUSE:"):
        out.add("Muse")
    if text.startswith(BRIDGE_POST) or "MUSE_TO_CLAUDE:" in text.split("\n", 1)[0]:
        out.add("Claude")
    return out


def acks_only(body: str) -> bool:
    """A post whose text opens with `/ack` (after the bridge's header line, for Muse's mail): an ack, never an ask
    ("/ack O-f70eab45 To Claude: J-009 #377 Codex FIX in work", 6787162569 on #116)."""
    text = body.lstrip()
    if text.startswith(BRIDGE_POST):
        text = text.split("\n", 1)[1].lstrip() if "\n" in text else ""
    return text[:4].lower() == "/ack"


def asks_anyone(body: str) -> bool:
    """Is this post an ask to anyone (a `To <Name>`, `→ <Name>` or `cc <Name>` line, and not an ack)?"""
    return not acks_only(body) and bool(ASK.search(body[:4000]))


def jobs_named(body: str) -> set[str]:
    """The job ids in a post's opening (Muse's mail subjects open with their job: `J-059 #343 FIX pushed`)."""
    return set(JOB_ID.findall(body[:300]))


def status_ask(ask: dict, holder: str, same) -> bool:
    """An ask to the integrator that asks no question and says no BLOCKED: the loose closers apply (module docstring).
    An ask whose text is not known is never one."""
    body = UNQUOTED.sub("", (ask.get("body") or "")[:4000])
    return holder == same(INTEGRATOR) and bool(body.strip()) and "?" not in body and not BLOCKED.search(body)


def names_ask(reply: dict, ask: dict) -> bool:
    """Does `reply` name the ask itself: its comment id, its URL, the reconciler's id for it (`oid`: O-xxxxxxxx), or
    a quote (a `> ` line) of its text?"""
    body = reply["body"][:4000]
    cid = str(ask.get("id") or "")
    if cid and cid != "None" and re.search(rf"(?<!\d){re.escape(cid)}(?!\d)", body):
        return True
    if ask.get("oid") and re.search(rf"(?<![\w-]){re.escape(ask['oid'])}\b", body):
        return True
    if ask.get("url") and ask["url"] in body:
        return True
    flat = " ".join(ask["body"].split())
    for m in QUOTE.finditer(body):
        q = " ".join(m.group(1).split())
        if len(q) >= MIN_QUOTE and q in flat:
            return True
    return False


def elsewhere(body: str, ask: dict, known) -> bool:
    """A board post that answers named other asks, not this one: it names other posts' comment ids ("to Muse, on
    comments 6709306608 and 6710071066"; `known` is every comment id read), or names one job (J-NNN) in its opening
    when this ask names others ("Answering Muse's J-044c ask" is not the answer to her ask about J-048). A post
    naming several jobs ("the eight asks the bridge just posted here") is a batch answer and keeps them all."""
    ids = {x for x in re.findall(r"(?<!\d)(\d{9,11})(?!\d)", body[:4000]) if x in known}
    if ids - {str(ask.get("id"))}:
        return True
    jobs, asked = set(JOB_ID.findall(body[:300])), set(JOB_ID.findall(ask["body"][:4000]))
    return len(jobs) == 1 and bool(asked) and not jobs & asked


def answer_rule(reply: dict, ask: dict, target: str, refs: set[int], job_thread: bool = False, owed: int = 1,
                known=frozenset(), who=None) -> str:
    """The rule by which `reply` (posted after `ask`) answers it, or "" when it does not. The one judge: the ledger
    and the reconciler both ask here (the rules are in the module docstring).
    `reply` has `a` (its author), `n` (its thread), `body`, `verdicts` and, when known, `login`; `ask` has `a` (the
    asker), `n`, `body` and, when known, `id`, `url` and `oid`. `job_thread`: the ask sits on a job's own issue or
    PR. `owed`: how many asks the addressee owes on the ask's thread. `who` maps a name to its agent (PE and
    Builder are one), the identity when not given."""
    same = who or (lambda name: name)
    holder = same(target)
    if reply["a"] == "noise":
        return ""   # nobody's post (author): never anyone's reply
    if any(same(v) == holder and r in refs for v, r in reply["verdicts"]):
        return "verdict on a PR it names"
    if reply.get("login") == SWITCHBOARD_LOGIN:
        return ""   # its notices carry the ask's id and "/ack O-..." themselves
    oid = ask.get("oid")
    if oid and re.search(rf"^[ \t]*/ack\b[^\n]*(?<![\w-]){re.escape(oid)}\b", reply["body"], re.MULTILINE | re.IGNORECASE):
        return "/ack"   # the command, from any team post: the asker withdraws its own ask so
    loose = status_ask(ask, holder, same)
    asker = same(ask["a"]) if ask.get("a") else ""
    if same(reply["a"]) != holder:
        if loose and asker and same(reply["a"]) == asker and not asks_anyone(reply["body"]) \
                and (reply["n"] == ask["n"] or jobs_named(reply["body"]) & jobs_named(ask["body"])):
            return "a later post by the asker, no ask"   # a status ask the asker followed with a non-ask (an ack)
        return ""
    if names_ask(reply, ask):
        return "reply naming it"
    to = {same(x) for x in addressees(reply["body"])}
    here = reply["n"] == ask["n"]
    if asker and asker in to and (here or re.search(rf"#{ask['n']}\b", reply["body"][:2000])):
        return "reply to the asker"
    if not here or not asker or to - {asker}:
        return ""   # elsewhere, or addressed to someone else
    if job_thread:
        return "reply on its thread"   # a job's own issue or PR: its next post there answers
    first = reply["body"].lstrip().split("\n", 1)[0][:200]
    if owed <= 1 and re.search(rf"\b{re.escape(ask['a'])}\b", first) and not elsewhere(reply["body"], ask, known):
        return "reply naming the asker"   # the board: only when it names the asker, and not other asks
    return ""


def answers(reply: dict, ask: dict, target: str, refs: set[int], **kw) -> bool:
    """Is `reply` (posted after `ask`) the target's answer to it? (answer_rule, as a yes or no.)"""
    return bool(answer_rule(reply, ask, target, refs, **kw))


def read(comments: list[dict], open_threads: set[int], start: dt.datetime, job_threads=frozenset(), who=None,
         oid=None, examined: set | None = None, closed=None) -> list[dict]:
    """Every ask posted after `start` (how far back the stream reaches) on an open thread, oldest first, answered or
    not: the comment plus `target`,
    `a` (its author), `verdicts`, `refs`, `answer` (the rule that answered it, "" while it is open) and `by` (the
    answering comment's id). Each comment needs `n` (its thread), `t` (created, aware), `url` and `body`, and may
    carry `id` (its comment id, which a reply can name) and `login` (the switchboard's own posts neither ask nor
    answer). `job_threads`: the threads that are a job's own issue or PR. `oid(comment, target)` gives the
    reconciler's id for that ask, when the caller knows it. `examined`, when given, takes the id of every comment
    read as a possible ask (so a caller can tell "not an ask" from "not read"). Pure: the caller reads GitHub. The
    ledger's own notices are never asks, and a post that names an ask from its addressee is that ask's answer, not
    a new ask to that addressee; in the stream's oldest day (ASK_CONTEXT) a post that names anything is not listed,
    since the ask it may answer can be out of the stream's reach, and a post that opens with `/ack` never is.
    `closed`, when given, maps a PR number to (when it merged or closed, its job id or ""): a status ask to the
    integrator (status_ask) that names that PR, or names its job, and came before it is answered PR_CLOSED."""
    same = who or (lambda name: name)
    comments = sorted(comments, key=lambda c: c["t"])
    for c in comments:
        c["a"] = author(c["body"])
        c["verdicts"] = {(m.group(1), int(m.group(2))) for m in VERDICT.finditer(c["body"][:400])}
    known = {str(c["id"]) for c in comments if c.get("id") is not None}
    live: list[dict] = []   # the asks no post has answered yet
    out: list[dict] = []
    for c in comments:
        if c.get("login") == SWITCHBOARD_LOGIN:
            continue
        owed: dict[tuple[int, str], int] = {}
        for a in live:
            owed[(a["n"], same(a["target"]))] = owed.get((a["n"], same(a["target"])), 0) + 1
        hit = []
        for a in live:
            rule = answer_rule(c, a, a["target"], a["refs"], a["n"] in job_threads, owed[(a["n"], same(a["target"]))],
                               known, who)
            if rule:
                a["answer"], a["by"] = rule, c.get("id")
                hit.append(a)
        if hit:
            live = [a for a in live if not a["answer"]]
        if c["n"] not in open_threads or c["a"] == "noise" or c["t"] <= start or LEDGER_MARK in c["body"]:
            continue   # the API's since is by update time: an older post edited later is not a new ask
        if c["t"] <= start + ASK_CONTEXT and MAY_NAME.search(c["body"][:4000]):
            continue   # it may be the answer to an ask the stream no longer reaches: not judged, not listed
        if examined is not None and c.get("id") is not None:
            examined.add(str(c["id"]))
        if acks_only(c["body"]):
            continue   # an ack, never an ask
        answered = {same(a["a"]) for a in hit   # whose asks to its author this post names: it is their answer
                    if a["answer"] == "reply naming it" and same(a["target"]) == same(c["a"])}
        refs = {int(r) for r in PR_REF.findall(c["body"][:600])} | {c["n"]}
        for target in sorted({m.group(1) for m in ASK.finditer(c["body"][:4000])} - {c["a"]}):
            if same(target) in answered:
                continue   # its answer to them, not a new ask
            ask = dict(c, target=target, refs=refs, answer="", by=None)
            if oid is not None:
                ask["oid"] = oid(c, target)
            live.append(ask)
            out.append(ask)
    for a in live if closed else ():
        jobs = jobs_named(a["body"])
        if status_ask(a, same(a["target"]), same) and any(
                t > a["t"] and (n in a["refs"] or (job and job in jobs)) for n, (t, job) in closed.items()):
            a["answer"] = PR_CLOSED
    return out


def unanswered(comments: list[dict], open_threads: set[int], start: dt.datetime, **kw) -> list[dict]:
    """The asks with no answer yet, one dict per (comment, target): read()'s, the open ones. The keywords are
    read()'s (`job_threads`, `who`, `oid`)."""
    return [a for a in read(comments, open_threads, start, **kw) if not a["answer"]]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default="IssisX/ScraperX")
    ap.add_argument("--hours", type=float, default=48.0)
    ap.add_argument("--since", type=int, default=None,
                    help="list only asks posted after this Unix time (the integrator's read-through mark); overrides --hours")
    args = ap.parse_args()
    now = dt.datetime.now(dt.timezone.utc)
    start = (dt.datetime.fromtimestamp(args.since, dt.timezone.utc) if args.since is not None
             else now - dt.timedelta(hours=args.hours))
    since = start.strftime("%Y-%m-%dT%H:%M:%SZ")

    open_threads: set[int] = set()
    job_threads: set[int] = set()   # a job's own issue or PR: its title names the job
    for page in range(1, MAX_PAGES + 1):
        items = gh(f"repos/{args.repo}/issues?state=open&per_page=100&page={page}")
        open_threads.update(int(i["number"]) for i in items)
        job_threads.update(int(i["number"]) for i in items if JOB_ID.search(i.get("title") or ""))
        if len(items) < 100:
            break

    comments = []
    truncated = False
    for page in range(1, MAX_COMMENT_PAGES + 1):
        batch = gh(f"repos/{args.repo}/issues/comments?since={since}&per_page=100&page={page}")
        truncated = len(batch) == 100 and page == MAX_COMMENT_PAGES
        for c in batch:
            comments.append({
                "id": str(c.get("id")),
                "n": int(c["issue_url"].rsplit("/", 1)[1]),
                "t": dt.datetime.fromisoformat(c["created_at"].replace("Z", "+00:00")),
                "url": c["html_url"],
                "body": c["body"] or "",
                "login": (c.get("user") or {}).get("login") or "",
            })
        if len(batch) < 100:
            break
    open_asks = []
    for c in unanswered(comments, open_threads, start, job_threads=job_threads):
        target = c["target"]
        flag = ""
        if target == "Muse":
            flag = "RELAY?"
        elif target == "Grok" and not c["body"].lstrip().startswith("To Grok:"):
            flag = "REPOST"
        age = (now - c["t"]).total_seconds() / 3600
        line = re.sub(r"\s+", " ", c["body"])[:110]
        open_asks.append((target, age, f"{target:<8} {flag:<7} {age:5.1f}h  #{c['n']:<4} from {c['a']:<8} {c['url']}\n         {line}"))

    for _, _, text in sorted(open_asks, key=lambda x: (x[0], -x[1])):
        print(text)
    print(f"OPEN ASKS: {len(open_asks)} (posted after {since}, open threads only)")
    if truncated:   # 11-14: a 7-day read hit the old 1000-comment cap and dropped every recent ask, silently
        print(f"TRUNCATED: read stopped at {MAX_COMMENT_PAGES * 100} comments; newer asks are missing. Use a shorter --hours.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
