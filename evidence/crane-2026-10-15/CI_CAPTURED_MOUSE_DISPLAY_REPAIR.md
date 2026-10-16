# Actual Main camera check needs a captured-mouse display

Candidate38837066273 at46374e2 completed host and Android native builds, resource import, static/cable render checks, crane input and native-audio checks. Its actual-Main check failed only real mouse freelook. The workflow launched that entire check with --headless, unlike the previously accepted Xvfb rendered Main run.

A focused Godot4.7 same-binary probe requests Input.MOUSE_MODE_CAPTURED (2). Actual headless readback remains0; actual X11/Xvfb readback is2. The existing production router correctly ignores uncaptured mouse motion, so a headless mouse injection cannot prove camera rotation. No production input change, skipped camera assertion or fabricated motion is appropriate.

The two pure input/audio checks remain headless. The unchanged actual-Main check now runs in a real640x480 Xvfb compatibility-renderer window, preserving its native cab, hand, camera, machine commands, pause and exit assertions. It has180s rather than120s to accommodate actual display initialization. The actual camera assertion and all PASS/error gates remain intact. Prior rendered source-v5 actualMain40checksPASS remains historical evidence; the next exact workflow must exercise the corrected invocation before an APK claim.

Focused probe output: MOUSE_CAPTURE display=headless requested=2 actual=0; MOUSE_CAPTURE display=X11 requested=2 actual=2. Private probe script /data/data/com.termux/files/usr/tmp/scraperx-crane-mouse-display-20261016/mouse_probe.gd. Source/runtime failures in current Actions log retained adjacent. Fork exchange remains uncommitted concurrent work and is excluded from this CI repair.
