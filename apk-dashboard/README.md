# ScraperX APKs delivery contract

The dashboard's four download buttons use stable `apk-latest` release URLs.
One optional Release API request enriches the existing UI with metadata. The
dashboard does not query Actions or infer a `syncing` state. Keep this subsystem
out of unrelated game changes.

`.github/workflows/apk-dashboard.yml` selects APK artifacts from successful
branch runs, checks package and label, signs them, and publishes the release.
The manifest records source commits, artifact IDs, hashes, and signing identity.
The published signing certificate must stay unchanged for in-place Android
updates. A green game build alone does not prove successful publication.

## Checks without redundant publication

Scheduled checks remain enabled. For each model, the publisher compares the
selected successful source commit and artifact ID with the last published
manifest. It also checks the configured signing certificate, package identity,
and the release asset's actual size and GitHub SHA-256 digest. Matching APKs
are left untouched: no APK download, signing, or upload.

When all four match, the job exits without editing the release, manifest, or
timestamps. When one changes, only that APK is uploaded; the other models keep
their assets, publication times, and provenance. A missing or mismatched asset
is republished. If no usable new artifact exists, existing published provenance
is retained. Checks are serialized without cancelling an active publication.

Run the publisher regression fixture with:

```sh
python3 apk-dashboard/test-publisher.py
```

It executes the actual workflow shell with fixture GitHub/Android command
boundaries and real certificate extraction. It checks publication decisions;
live GitHub runs establish actual publication behavior.

Live verification on October 2, 2026: publisher run `37034221476` passed all
18 regression checks and published the newer ChatGPT source `83822fca6b5f`.
Gemini, Claude, and Grok were skipped; their release asset IDs, SHA-256 digests,
and publication timestamps remained identical to the preceding release.

## Signing setup and recovery

Run `setup-stable-signing-termux.sh` with Bash. First-time setup retains the
keystore and password under `~/.local/share/scraperx-dashboard-signing/` with
restricted permissions. Back up both securely. Re-running uses that same key.
If signing is already provisioned but the local backup is absent, setup refuses
to generate or overwrite secrets. API/provenance lookup errors also stop setup.

To restore a backup matching the published certificate:

```sh
bash apk-dashboard/setup-stable-signing-termux.sh --restore /path/original.p12 /path/password-file
```

The script checks the certificate before updating either secret and dispatches
the publisher after restoration. Do not put passwords in arguments or chat.
The previous setup script generated a new identity on every invocation and
deleted its temporary key. If that original private key has no backup, it
cannot be recovered from a signed APK or read back from GitHub secrets. A new
certificate then requires an explicitly approved migration and reinstall of
the previously installed model APKs; bypassing verification is not a repair.

## Observed incident: October 2, 2026

ChatGPT run `36922232894` successfully produced the artifact for commit
`3d664e4abfcc5c0acbdb7537c72734248f6b4632`. Publisher run `36976238096`
rejected signing secret certificate
`91184379cf4aad862b2436f403230f3ba163a8a9cdc5dc7c32dffef2a517402a`
because the release uses
`22d547086ddee2df27eba7db9a54df2f4e1996c764d6b709ae91f9463bbb2cce`.
The release therefore still contained ChatGPT source `4996a6e8cca0`.

The setup and preflight repairs prevent silent replacement and catch mismatches
before APK processing. They do not recover the original key or resolve the current
secret mismatch by themselves. Cory explicitly approved a one-time reinstall
migration to the existing `911843...` signing secret on October 2, 2026.
Publisher run `37007234977` completed successfully and published all four APKs,
including ChatGPT source `3d664e4abfcc`. The temporary exact-certificate migration
exception has been removed; strict signer continuity is restored against the
new published identity. Previously installed model APKs require uninstall and
reinstall once. Dashboard UI is unchanged.

Run the isolated signing-setup regression checks with:

```sh
python3 apk-dashboard/test-signing-setup.py
```

These tests use real OpenSSL and keytool certificate operations, the actual
publisher preflight shell, and a fixture GitHub CLI. They verify local setup and
certificate rejection, not live secret writes or publication.
