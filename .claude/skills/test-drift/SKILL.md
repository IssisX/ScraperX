---
name: test-drift
description: Fix a native or touch test that fails after a change to physics, contacts, the solid world or body order because its script fed fixed inputs that only worked with the old behavior. Use when a test that passed before such a change now ends a little off its target (an offer never appears, a load lands short, the rider ends somewhere else).
---

# A test that relied on the old physics

The fix goes in the script, never in the bound: the script is made to play as a player would.
The danger is hiding a real bug, so step 1 decides which it is, and step 3 limits what may change.

1. **Drift or a real bug?** Run the group alone:
   `SCRAPERX_ONLY=<group> ./build/host/scraperx_sim_tests`. Drift: the script fed fixed inputs
   (walk for 1.2 s, then press) and now ends near, not at, where it meant to be, because the
   change moved something a little (a slide that used to carry the player is gone; a container
   lands 1.2 m away). A real bug: a player could no longer do it by touch (the offer never comes
   however the player approaches; the machine no longer lifts). A real bug is fixed in the game,
   and the test stays as it is.
2. **Never move a bound or weaken what the test proves:** the same machine, the same end state,
   the same numbers checked.
3. **Close the loop, and only that.** Each tick, act from where things are, as a player would:
   - step toward the target at a gentle stick until the game offers the action (the carry target
     is the object; DRIVE or PICK UP is offered), then stop and act;
   - let go of a hold the player would not keep (`request_release()` while the traversal state is
     not `None`);
   - near an edge, nudge gently rather than push until airborne;
   - bound every loop (ticks or seconds) and fail with the state printed if it runs out.

   Anything more (a teleport, a different route, a skipped step, a longer time limit) means it is
   not drift: go back to step 1.
4. **Rerun** the group alone, then the full native suite when the change reaches the whole world
   (the start file's "No redundant test runs" row).
5. **Commit** the script fix with what the old physics did for it and the test's PASS line.

Done this way after the planted grip (2026-10-20): the derrick steps in to each iron pig until
PICK UP is offered (`55c5189`); the Matron's "no way" run steps off the bridge's rim gently
(`ede2e17`); the crane run lets go of the truck's guard and steps in until DRIVE is offered
(`9f199c5`). The body-order fix gives the same rule
(`03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md`).
