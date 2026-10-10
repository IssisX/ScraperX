# Latest available APK and development-source gap

Observed on 2026-10-10 against `IssisX/ScraperX`, branch `ChatGPT`, development HEAD `89e21f14ff6f63dc24efbdbea91170dd1d924075`. This record distinguishes available/downloaded APKs from development code. The installed phone package was not observed.

## Latest available ChatGPT APK

The current [public release](https://github.com/IssisX/ScraperX/releases/tag/apk-latest) still maps ChatGPT to executable source **`0e31e2229f26bbed773b575ac5a44783e36d0af6`**, [successful run37864437741](https://github.com/IssisX/ScraperX/actions/runs/37864437741), artifact **11588888895**. That run completed successfully at **2026-10-09T01:16:59Z**. No newer completed green ChatGPT build was found in the recent completed-build list.

| Available package | Version code | APK SHA-256 | Evidence |
|---|---:|---|---|
| Dashboard `ScraperX-ChatGPT.apk`; latest root Downloads copy `ScraperX-ChatGPT (10).apk` | **213671847** | `1337101b0792dbde5f19c96b57a0cdb28012b6d8fc02d7d394cd297db57aceb6` | Current public release manifest and asset digest; independently computed local file hash matches exactly. |
| Previously delivered direct artifact copy `ScraperX-ChatGPT-0e31e22.apk` | **213671807** | `0587e3d6935591320fcdb8a98c89ab4c238e3f9086ef3448a6dbcb174657bcd9` | [Saved exact-source delivery receipt](../../supplied-machines-2026-10-08/ci-0e31/receipt.json); local file hash still matches. |

These are two package revisions of the **same executable source**, not two gameplay-source versions. Public metadata records asset623497962, updated **2026-10-09T01:17:39Z**, and package `com.cory.scraperx.chatgpt`. The latest matching Downloads file has mtime **2026-10-09T11:21:49.091207Z**; mtime establishes file ordering, not installation. The public manifest was generated **2026-10-09T20:40:30Z**. Its `source_is_current_head=true` is a historical snapshot and must not be treated as the live development HEAD.

The user's report of using the most updated version is consistent with the newest available APK. The newest local Downloads copy is exactly the newest public asset. No evidence here identifies the installed package, contradicts the user's report or closes the drift bug.

## What that executable source contains

Source0e31 still computes neutral beam lateral motion from `-beam.offset * kBalanceCentering`, capped by `kBalanceCenteringMaxMps=0.4`. It predates both:

- `e3c6168a720b94d3fa42c4e67df682747e3c8642` (2026-10-09T04:49:49Z): removes automatic centring and derives fine lateral correction from deliberate input; neutral input brakes.
- `30a60c75478f78e2fb396e25e922698b9829870f` (2026-10-09T17:33:33Z): adds bounded reciprocal contact-force compensation for gravity tangent to the support incline.

Neither commit is an ancestor of0e31. Current-development beam/contact probes therefore do not reproduce the delivered executable boundary and cannot reject the phone report by inference. This source gap is verified; it does not establish the cause of every reported beam or rail drift.

## Current delivery boundary

Eight completed ChatGPT builds after0e31 failed: **37920897135,37969393230,37972296060,37975266277,37990803171,37995212805,38001791173,38007285774**. Successful dashboard-publisher runs do not mean those gameplay builds passed.

At observation, [run38060174319](https://github.com/IssisX/ScraperX/actions/runs/38060174319) for `89e21f14ff6f63dc24efbdbea91170dd1d924075` was **in progress**, created **2026-10-10T14:34:53Z**, with **zero artifacts**. There was no new APK to download or deliver. Root owns full green exact-source CI and matching APK delivery.

## Retained input-release evidence

[Copied receipt](input-release/receipt.json) and [actual trace](input-release/run.log) retain the already-run short headless viewport joystick test. Ordinary release and canceled release immediately cleared touch movement and stick ownership. All240 subsequent command frames supplied exact-zero router movement and `native.set_move_input` arguments. No connected pad existed (`active_pad=-1`, cached axes zero); no device state was fabricated. Godot normalized canceled events to `pressed=false`, so the third cancel case repeated that actual event shape.

This is input-path evidence at the receipt's exact source/native hashes, not a phone observation or beam-contact acceptance. No maintained UI/native changes or new experiments were made for this delivery mapping. The existing receipt/log were copied unchanged; no APK was downloaded.

## Actual read-only evidence commands

```sh
gh run list --repo IssisX/ScraperX --branch ChatGPT --workflow wo000-delivery-spine.yml --status completed --limit 12 --json databaseId,headSha,createdAt,updatedAt,status,conclusion,displayTitle
gh api repos/IssisX/ScraperX/releases/tags/apk-latest --jq '{tag_name,published_at,updated_at,body,assets:[.assets[]|{name,id,created_at,updated_at,size,digest,browser_download_url}]}'
gh api repos/IssisX/ScraperX/actions/runs/38060174319 --jq '{id,head_sha,status,conclusion,created_at,updated_at}'
gh api repos/IssisX/ScraperX/actions/runs/38060174319/artifacts --jq '{total_count,artifacts:[.artifacts[]|{id,name,created_at,expired}]}'
```

Local hashes were computed with Python `hashlib.sha256(path.read_bytes()).hexdigest()` for the four newest root-level `ScraperX-ChatGPT*.apk` files, ordered by mtime. No installed-package query was run. Source ancestry and the beam/contact snippets were read with `git show` and `git merge-base --is-ancestor`.
