# Clear operator controls — 2026-10-17

Owner screenshot Screenshot_20261017_163524_ScraperX-ChatGPT.png showed the
forklift selector across the middle of the windshield and a tall instrument
panel behind those controls. Installed source is unknown; those positions
match the published layout. Its21FPS indicator does not establish a cause.

Forklift controls now form lower thumb groups: left steering/accelerator/brake,
F/N/R above it, park/fine-control beside it; right lift and tilt pairs. Tool and
horn sit at the bottom, with exit in the upper corner. Text-only buttons carry
their label inside the target. The crane uses the same edge grouping. Touch
operator gauges retain all native values and safety warnings in a short upper
two-column strip; repeated instructions and central floating labels are removed.
Keyboard/gamepad behavior, all vehicle axes, coupler actions, cancellation and
native physics are unchanged. No graphical quality setting changed.

[Exact commands and observed output](COMMANDS.json), pinned Godot4.7 under
ARM64 Ubuntu proot, clean published944e0e1 native bridge plus these UI edits:

- android_responsive_controls_test: exit0,576layouts/failures0. Existing safe-area,
  non-overlap, per-button capture/verb/hold, stick and independent look checks
  remain. Operator targets and the instrument strip must also leave the entire
  forward-view band from22% through62% of safe height unobstructed.
- forklift_operator_inputs_test: exit0/PASS, all dedicated axis/input behavior.
- forklift_tool_inputs_test: exit0/PASS, including actual Main/native loading;
  physical exchange remains its separate test.
- touch_capture_test: exit0/PASS, releases cannot leave a held owner behind.

The initial isolated host config lacked a LinuxARM64 extension selector. Adding
that host-only mapping repaired the harness; shipping Android config is unchanged.
Final runs loaded the actual native bridge without script/extension errors.
Local OpenGL is already known broken independently of this game. The candidate
workflow therefore captures the actual rendered Main forklift controls at the
owner screenshot's1696x1456 aspect and retains those PNGs with opening evidence.
That future render is not counted as observed here. APK/phone appearance and
performance remain unverified until matching delivery/playback. Performance is
separate: native prepared-idle solving remains near its90Hz budget; the measured
passive-body lifecycle is under active diagnosis, with no sleep or physics cut
included in this control-layout change.
