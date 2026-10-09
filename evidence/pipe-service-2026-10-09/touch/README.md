Private ordinary viewport-touch driver for the actual normal-world 440.25→462.25 pipe-service crossing. Root owns the native bridge, build and production geometry. One supported 440 m roof start is followed by ordinary viewport events through production InputRouter/main; no subsequent relocation, velocity/force write, support grant, fixture or production input edit.

The quick case passed with exit 0 at native tick 4751, stable at (-19.49887, 463.15, -142.0169), support 51 and firm footing. Walking alone stopped at X=-15.85 on support 1939. One fresh supported Jump cleared the whole obstacle crest of 452.03 m with soles at 452.2044 m, then landed on the real receiving floor and continued to the 462 m exit.

The connected miss-retry case passed with exit 0 at native tick 9085, stable at (-19.49919, 463.15, -142.0124), support 51, firm footing and zero velocity. It first used the quick crossing, walked around the shelter end and deliberately missed the actual toe. Support loss at tick 3808 was followed by ordinary air braking at tick 3832, a sampled 24-tick response (0.267 simulation seconds). Actual 440 m roof contact at tick 3935 passed the unchanged >8 m/s impact check. It then reapproached normally, retried through the full sheltered lane, verified blocked standing under its 1.45 m roof and permitted standing after physical exit, and continued to the stable 462 m west roof. Gravity remained 1, deaths and accepted traversals remained zero, and hands remained free. A separate sheltered-only touch case was not run; its path and checks were exercised in this connected retry.

Modes match native `quick`, `sheltered`, `miss-retry`. Quick tries ordinary walking into the pipe, backtracks normally, then uses one supported Jump with earned speed and real airborne crest clearance/contact. The native test separately asserts the legacy-vault counter. Touch checks actual traversal state, hands and unchanged accepted-traversal count because the bridge does not expose that counter.

Preparation reuses the already imported production assets/cache from the earlier inspection harness and copies small scripts, the actual main.tscn, extension metadata and supplied bridge. It requires a fresh private target. The complete tested project remains in `godot`; the preparation helper does not create a new shared build or copy a large asset cache.

```sh
python prepare_runtime.py --bridge /absolute/root-built/libscraperx_native.so --target /absolute/fresh/private/godot
sh run_touch.sh quick quick /absolute/private/godot
sh run_touch.sh miss-retry miss-retry /absolute/private/godot
```

Exact runs are preserved in `command-quick.txt` and `command-miss-retry.txt`. Their `*-log`, exit code and receipt files contain actual results and source identity; the filenames are `quick.log`, `quick-exit-code.txt`, `quick-receipt.json`, `miss-retry.log`, `miss-retry-exit-code.txt` and `miss-retry-receipt.json`. Helpers as used are `pipe_driver-quick-as-used.gd`, `pipe_driver-miss-retry-as-used.gd`, `run_touch-quick-as-used.sh`, `run_touch-miss-retry-as-used.sh`, `prepare_runtime-quick-as-used.py` and `prepare_runtime-miss-retry-as-used.py`. The complete appended test driver is `godot/presentation/ui/ui_test_driver.gd`. `godot-prepared-source-manifest.json` records preparation identities; receipts record every tested runtime script, scene, project, extension and bridge hash.

Native bridge SHA256: `f00513db1b85470bba3a959da4d42349549a6b6cf2687917422ecee545e17163`. Production InputRouter SHA256: `39f78d39c3b010a4a4109d4e559c6dfddafedee9438555504981596c1ac76cc0`.

Headless scripted viewport input establishes these connected paths and input delivery under the tested runtime. Handset feel, human accessibility, visual/audio quality, phone performance, CI and APK packaging remain outside this evidence. Older north-transfer projects and receipts are preserved.
