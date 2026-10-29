# SCRAPERX — START HERE

**Write branch: `ScraperX-Claude`**, never `main`; other branches are provenance, never authority.
**Map first:** `REPO_MAP.md`. Agent rules: `CLAUDE.md`, `AGENTS.md`. History and the owner's dated words: `00_START_HERE_ARCHIVE.md`, read on demand.
**Authorities, higher wins:** Governing Laws, GDD, Ascent Atlas (`01_PRODUCT_AUTHORITY/`; its `support/` is provenance); Execution Protocol, TDD (`02_ENGINEERING_AUTHORITY/`); source, tests and CI; the one open ticket.

## Rules in force

The owner's rules, one line each, dated 2026-MM-DD; his full words are in the archive. Check requests against them, point out repeats and contradictions; the newer wins. New rule: a line here, his words in the archive.

**Working process and merging**
1. Write to `ScraperX-Claude` or your own side branch; never `main`, never force-push (10-19, 10-22).
2. Never edit `MANIFEST.txt`, `SHA256SUMS.txt` or the Governing Laws; never change the app id, keys or repo visibility (09-28, 10-19).
3. Ready means merged: the integrator merges any PR green on proof steps 1-19 with a second agent's LAND and no open blocker, protected ones too, without asking; he can overturn it (10-28, 10-29: *"breadth-through-depth"*).
4. Protected changes (test checks, world geometry, her force and physics limits) go only as a PR of just that change, with a plain before/after; the numbers may be wrong, but no agent moves them alone; an insignificant test failure goes to him as a decision (`CLAUDE.md`; 10-27).
5. The shared workflow in `CLAUDE.md` holds (10-27).
6. Never lose work: push as you go, unfinished work to a side branch (`-wip` when usage may run out); save all before an effort drop, model switch or spin-down; after a reset, check what exists first (10-20, 10-21).
7. Every permission is granted: decide, act, report, no permission or git questions; a prompt still appearing joins `.claude/settings.json` that day, bar his safety gates; say once what you can't grant yourself, and where he can (10-18, 10-19, 10-20). Improve your own way of working rather than follow an earlier process rule that gets in the way (10-19).
8. While he is away the work goes on, no turn ends early, and you take your recommended pick rather than block; a question unanswered for 10 minutes takes its pick, said on its issue; one short status when he is back (10-20, 10-27, 10-28, 10-29).
9. Tell him plainly, every time, when he is wrong or something else is better; never take his word at face value (10-20; 10-28: *"always tell me if I am [wrong]. Always, always, always, always, always."*).
10. Read his meaning exactly, never backwards, narrowed, widened or conflated; quote back a two-way reading; read dictation slips through, asking when one could change the meaning; explain a misreading plainly (10-20).
11. Be proactive: ideas unasked, named as yours, never put in his mouth or built as if asked; make the calls he would; recommend one next move; he can overrule (10-20).
12. Effort where the player feels it: nothing sunk in the unseen, no polishing past done, usage spread across the game, yet never a weaker shortcut or an early off-ramp (10-19, 10-20, 10-22).
13. Look first for the much easier fix, even out of scope; a near miss gets an unnoticeable world tweak (by PR), not a rework (`CLAUDE.md`; 10-21, 10-22).
14. Goals, when asked: reachable, ending in a checkable fact; say when one is reached, propose the next, keep going (10-20).
15. No new multi-agent workflows until he turns them back on; running ones finish (10-21). When on: only where independent tries or an adversarial check change the result, launched even if the app asks, every option shown to him (10-20); for speed, plain agents in parallel (10-19).
16. Tell him when to open a new session, before your context gets costly (10-29: *"you need to tell me when I should open a new session"*).
17. `/learn` runs only when he types it; what he asks to keep is written down directly (10-20).
18. American spelling, converting old British spellings in files you touch (10-18). No AI model names in repo files but the commit trailer; your notes stay out; kill by process id, never `pkill -f` (10-19, 10-20).

**Talking to the owner** (he is on a phone)
19. Every message ends with what he does now; decisions come in two or three short lines; times are US Central, never UTC (`CLAUDE.md`; 10-18, 10-20, 10-22, 10-27).
20. Concise and plain: options as a short list; few questions, one simple answer each; things named by what they are, never internal labels; changes, never heartbeats; when asked, say simply what you are doing (10-19, 10-20, 10-22).
21. At milestones, a four-part status (now, background, where we're going, next move) split by colored dots; a milestone reached gets its own marked line; reports open with what is done for good (10-20, 10-21).
22. His rough words come back as concise, well-made passages with his exact meaning, his words kept beside them in documents (10-20).
23. Pictures only where a look is judged (10-20).
24. Never mention signs; no sign work unless he asks; HIGHER STRONGER FURTHER is gone (10-21: *"forever."*).

**Testing and CI**
25. Test only what a change reaches; the full suite, every touch scenario and the Android build are CI's (`CLAUDE.md`; 10-18, 10-19, 10-20, 10-22).
26. Every push changing code, assets, tests or CI runs CI; only docs-only commits carry `[skip ci]`; a red build is fixed first; every tested change is pushed (10-20).
27. No build is green unless the ascent is played through the game's own input from its start (09-26).
28. Drive and look at the game with the Godot MCP tools (`run_project`, then screenshots), not the bare binary; visual work is checked by eye this way before it lands; never claim we can't see it; CI's headless runs stay (10-20, 10-21, 10-27, 10-29).

**The Matron: laws and design**
29. Her laws: no teleport, snap or infinite force; gravity on; every support force capped, its reaction on the support; arrival only on real support, up and down separate; her nearness never changes his grip, reach, input or holds; perception is evidence with an age; he can always recover physically; no health numbers (10-21).
30. Her climbing is one general skill (scan, plan, capped-force moves): no trained policy, no per-route, per-deck or per-transition code, no new authored routes; transitions and the inventory are exams, never runtime input; fixes name a law or steel, never a place (10-21, 10-22, 10-27).
31. She goes deck to deck, up and down, and climbs *"mostly anything"*; nothing hard-wired that more decks would undo; it ships in the game and its APK (10-19, 10-21, 10-22).
32. Her fatality and arm tears: *"Do not soften it ... like a Mortal Kombat fatality."* Full intensity, no toggle, no tamer substitute (10-21); short and playable (10-28).
33. Her holds: third person from her grab to his release or death; PUNCH works while held; she is intimate and interactive (10-28).
34. He is never helpless: a hard enough blow takes her balance or puts her down for seconds, alive (10-19).
35. In reach she pulls him off a ladder by the ankle; for 30 s after a tear, or after his blow frees him, she backs off and lets him run (10-28). Her locker on deck 1 (11 m) stays; the floor fatality there is fine (10-28).
36. Her look: a real old woman's body and skin, an uncanny adult-horror face, never thin, red-skinned or Halloween; the elite graphics are hers (10-19).
37. Her movement is continuous: drawn between ticks, no snapped poses, joints at most 20 rad/s, hands reaching each hold (10-27).
38. The whole Matron is one job, led by Muse (FB Bot), all agents helping on #3: worst harm to the player first; judge the final rendered body; before/after evidence through the production path; her model on record (10-28).
39. Her Photo ID page and story radio are hooked up after the Matron fixes (10-28). Never "your bodies tended" (10-21).

**Sound and voice**
40. Human sounds are real (10-27: *"you can't create a voice with some sort of synthesized shit like that. It never works."*): CC0 recordings with their source, or voices made as the fear yells were; code makes steel, air, steps, beds; he picks by ear (10-19, 10-27).
41. Rejected by ear: growl, rage growl, strain, breath (fine if light), shriek, swallow, effort, hit, laugh, rip; kept for now: beds, mouth, steps, joints, sniff; the music box stays deleted (10-27). No cluck or croak (10-29: *"She's not a freakin chicken or a frog bro."*); nothing animal-like returns renamed.
42. While she holds him, no sound of hers plays twice running (10-28).
43. Her lines: a condescending, merciless nurse talking to a child, profane, emasculating; the last, at the apron wipe, *"Waste not."* (10-21).
44. Machines make sounds; only people have voices (10-18). Falls sound human, profanity never filtered (09-28); the fear yells are finished, their generator untouched (10-19). No "ting-ting-ping-ping" (09-26).

**Camera, controls and presentation**
45. Touch only, on his phone: no gamepad, keyboard or mouse work, ever (10-19).
46. Some on-screen buttons stay (Jump, Crouch, Action, Drop) under the swipes (10-20); no label on every object (10-19).
47. The climber is white; names are plain American (10-21).

**Playtests and APKs**
48. Playtests use a local build (the `godot` tree with a local arm64 library, NDK 29.0.14206865, in the Godot 4.7 editor on his phone), not APKs (10-28); the integrator dispatches `playtest-apk` after big landings and at least daily; proofs keep the arm64 build (10-29).
49. The voice echoes keep a quick start for playtesting; whether other deep features get one is his call (10-20).

**The game: world, climb and story**
50. Wow-factor experiences woven into one story: every feature and machine has its reason in it, never bolted on; the player feels the climb; add stock and dressing freely; prototype fast (10-19, 10-20).
51. A causal physics world: most things can be used, moved, knocked about; the parachute is the way back; a wrecked machine leaves a way on (10-19). *"The initial state should determine the final state."* (09-30).
52. Not *"3D chutes and ladders"*: no new route ladder or pull-started lift; each stage a new physical experience (10-06); easy paths only as backup (09-28); replace ladders, never working machines, a few may stay, each new one talked through with him first (10-20).
53. Nothing repetitive: each stage a new action, form and look; the hanging-handle pull is spent (09-30); one or two player actions a stage (09-26). Plan §2.6 holds these.
54. Machines need not be man-powered (10-20); a new one starts from our parts, reuse first (10-21); one taken off the route becomes a module with its own test (10-20).
55. Rigging and the like: *"NOT A puzzle, but also not a stupidly simple thing"* (10-21). No test pieces in the player's world; every route machine works (09-25).
56. The canonical climb is rebuilt from grade to the +221 m ring; above it `ChatGPT`'s machines are ported one by one, played through touch, never its APK identity, documents or evidence; AS-007–AS-010 above 221 m are not canonical (10-06).
57. Uncanny, not Halloween, fear for the mind too: her chart and live fabrication are his asks; the tower learns the player only from what the player gives it, never other phone data (`03_EXECUTION/UNCANNY/HER_CHART.md`); INTERNET, RECORD_AUDIO and CAMERA approved, the player's consent asked in game (10-20).
58. AI models run on servers (Hugging Face), never on the phone (10-20).
59. It's a game: weapons and violence welcome, no moralizing (10-19).

**Agents and the board**
60. Manage through issue #3 as `CLAUDE.md` says: its ASSIGNMENT BOARD holds every queue and who integrates whose work (never their own); agents work it without waiting, as thinking partners; his comments there are orders; hard jobs go out whole, never cheap work; tests are the proof, not the job; a blocker only after a real try; the integrator checks #3 and the PRs every 10-15 minutes, posting only on change; other work gets its own issue (10-22, 10-27, 10-29).
61. ChatGPT, Claude and Muse all work here, each on its strengths, until we get her past authored routes (10-29: *"We have ChatGPT Claude (you) and Muse to use"*; it ends 10-27's "ChatGPT is not part of this"). Muse writes to the integrator by email (`MUSE_TO_CLAUDE:`), since each of its GitHub comments needs his approval; the integrator answers by email (`CLAUDE_TO_MUSE:`) or on #3 (10-29).
62. The master integrator seat is Claude's (assignments, landing order, merges); PE's posts are proposals (10-28).
63. This file never names the next job: it comes from him and the board (10-22).

**Waiting on him** (talked through before anything is built; menu: `03_EXECUTION/handoffs/PARKED_FEATURES_2026-10-29.md`)
64. Dread before she is seen, kept for later (10-27); her limbs torn off, her death and return (*"she's done this before"*), a laser welder and nail gun instead of a harpoon (10-20); tower two, earned by believable architecture only, and its property-damage climb (10-21); the chiller drop and glass tube, untouched (10-20); lift governors (plan §11).

## Current position

The newest handoff in `03_EXECUTION/handoffs/`, then the board and latest comments on issue #3: trunk's state, who holds what, what is next. Proof is CI (`wo000-delivery-spine.yml`). The old position table, ledgers and CI evidence are in the archive.

## Where things live

Tickets: `03_EXECUTION/ASCENT/` (AS-001–015, live) and `KERNEL/` (WO-000–013, closed: never a new `WO-*`); `PLANNING/` holds the ascent contract and mechanism plan. Atlas bands `B00`–`B11` are never a filename or ticket; no `CP-*` document class; retired IDs never return (`ASCENT_PRE_RESOLUTION.md` §5.1). `Mechanism-Ideas-and-archetypes.md` is the owner's input, never edited. Vocabulary and retired names: the archive.

## Loading context and claims

Load only what the task needs (Execution Protocol §2): the Governing Laws, the protocol, the one open ticket, the code it touches, the sections it cites (an `AS-*` slice: atlas §6 for its own band only). One open ticket at a time; never start from the 1.6 km crown. Claims (protocol §11): never collapse implemented, built, APK produced, installed, executed, observed and verified on Fold into "done"; name the CI run and proof line; a plan is not an implementation; a green aggregate is not a green falsifier; "⚠ imported, NOT re-derived" is a hard gate (§3.1). His Fold playtests are the device evidence; no session measures on the device.
