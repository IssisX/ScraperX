# Native-contact slide friction sound

Sliding now has a continuous friction sound instead of simply silencing
footsteps. A generic cloth/sole texture is the fallback; an actual steel
support tag selects a sheet-metal resonance. One dedicated Effects player
changes gain and pitch with native support-relative sliding speed. The
existing six one-shot Effects voices remain unchanged.

`slide_friction.gd` reads the native grounded/support identity, slide flag,
slide count, speed, tick and death count. It never changes physics. Lost
contact, no support, zero slip, mute, invalid input, death, restart and
rewind stop the player. The voice is pausable even though menu audio stays
live; the audio owner also clears its history while the tree is paused.

The two loop recipes are appended to SoundBank. The generator state is
restored afterward, retaining the old sound recipes and variant sequence.
The loops use existing seeded synthesis helpers and add no downloaded
asset, license, bus or native dependency.

## Tuning

`slide_friction.gd` owns the presentation values: full feedback at 8 m/s,
silence at or below 0.3 m/s, gain from -26 to -13 dB and pitch from 0.85 to
1.15. These are audio choices, not physical limits. The native slide owner
and all movement force/work/contact laws are unchanged.

## Actual execution

Prototype and exact receipts:
`/data/data/com.termux/files/usr/tmp/scraperx-slide-friction-20261018/`.
The coherent native bridge is the accepted, unchanged e014c22 graph.

`unit-receipt.json` contains the exact headless command, exit 0 and log:

```text
SLIDE_FRICTION_AUDIO PASS checks=48 failures=0 native_contacts_exercised=0 heard_audio=0
```

It exercises actual loop buffers/player selection, bounded speed response,
same-tick reuse, surface changes, loss of support, mute, lifecycle changes,
invalid data and one-player reuse. Dummy does not retire stopped WAV
playbacks through a real mixer; its ObjectDB warning remains in the log.

Exposed Godot MCP tools ran the real shipping Main/native controls path:

```text
THUMB_SPRINT velocity=(0.0, 0.0, -8.000001) grounded=true sprint=true
SCRAPERX_THUMB_FLOW_INPUT PASS checks=295 native=1
```

The added checks observe actual touch sprint-to-slide driving the real
Effects loop, measured native speed, actual pause stopping it, and restart
to quiet footing. The rest of the existing controls chain also completes.
`mcp-receipt.json` owns the calls/output; the runtime stopped afterward.
Host 3D was disabled for this interaction check and the driver was Dummy.
Rendered candidate CI, phone feel and heard sound are separate evidence.
