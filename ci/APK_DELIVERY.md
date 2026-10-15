# ChatGPT Android builds

Every push to `ChatGPT` starts **ScraperX ChatGPT — downloadable APK candidate**. Open the latest run at https://github.com/IssisX/ScraperX/actions/workflows/chatgpt-apk-candidate.yml?query=branch%3AChatGPT and download its `ScraperX-ChatGPT-candidate-<commit>` artifact. The ZIP contains `ScraperX-ChatGPT.apk`, checksums and exact-source provenance.

A successful candidate proves host/native compilation, Godot project import, Android export, package identity, signing lineage, native-byte identity and 16 KB native/ZIP alignment. It does not establish full gameplay regression success or device play. The separate delivery-spine workflow retains its full native/Godot proof gates; failures remain visible. Candidate artifact names are deliberately excluded from the existing automatic tested-release publisher, so a candidate cannot replace the tested `apk-latest` release.

Both workflows use the vendored stable signer from `.github/actions/stable-apk/UPSTREAM.json`, with 16 KB ZIP alignment before signing. Existing signing secrets and the already public legacy debug certificate retain upgrade lineage. Provenance verifies the rotated certificate at SDK 28 and newer, matching the signing rotation policy.

Local APKs do not appear on GitHub until their source is committed, pushed and the candidate workflow succeeds. Keep build/download evidence separate from gameplay and installed-device evidence.

For this Termux workspace, use the existing repository-scoped approved push command: `git -C /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo push origin ChatGPT`. The generic `git push` spelling triggers a separate prompt rule. Inspect applicable existing allow rules when a routine authorized command is rejected; do not disable safeguards or send the task back to the owner before checking the supported scoped command.
