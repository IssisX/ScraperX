# Story overlay render correction

Base: published9d554b31a2e216895544edd718912b420099e06f. Candidate39500604458 passed all six story gates but failed opening_render_budget at1014draws in crown_to_ground. Existing strict<1000 gate is unchanged.

Root changed only story presenter rendering and its focused test: stable intro/objective text uses UPDATE_ONCE SubViewports at actual canvas pixel density, raster origins aligned to screen pixels. Parent intro opacity and radio accent remain live; fonts, shadows, clipping, copy, layout and input behavior are retained. No scene-quality setting changed.

Executed on Godot4.7.stable.official.5b4e0cb0f:

```
proot-distro login ubuntu --bind /data/data/com.termux/files/usr/tmp:/data/data/com.termux/files/usr/tmp -- /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --headless --path /data/data/com.termux/files/usr/tmp/scraperx-portable-line-20261020/presentation-check/project --script res://tests/story_presenter_test.gd
STORY_PRESENTER PASS checks=86 failures=0 supplied_state=1 input_transparent=1 layouts=5 world_physics=0
exit0
```

Actual direct Godot MCP run_project/run_script on P/story-opening/runtime: isolated story UI before35draws at160x90; after5draws at160x90 and13draws at960x540. Final screenshot P/story-opening/story-cache-final.png rendered and inspected. These are UI-only counts, not full-world draw calls or Android FPS. Fresh MCP process loaded final sources. The prototype clip_text=false trial did not reduce draw calls and was rejected; it was never applied to source.

One exploratory probe incorrectly expected SubViewport.render_target_update_mode to become UPDATE_DISABLED after rendering once. It returned false because Godot’s scene getter retains the requested enum; source https://github.com/godotengine/godot/blob/4.7-stable/scene/main/viewport.cpp SubViewport::get_update_mode returns stored update_mode. That probe is not an acceptance result. The focused test separately disables a cache, presents unchanged copy and verifies no update is requested; changing the objective requests only its cache. The real existing tower render-budget test remains required in the matching candidate.

Broader story progression, optional hosted inference, player photo UI and embodied character remain separate unfinished scope; no APK delivery or phone-performance claim at this write.
