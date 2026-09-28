#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

REPO="IssisX/ScraperX"
ALIAS="scraperx-dashboard"

if command -v pkg >/dev/null 2>&1; then
  command -v gh >/dev/null 2>&1 || pkg install -y gh
  command -v keytool >/dev/null 2>&1 || pkg install -y openjdk-17
fi

for cmd in gh keytool base64; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing $cmd. Install GitHub CLI and OpenJDK, then rerun." >&2
    exit 1
  fi
done

if ! gh auth status --hostname github.com >/dev/null 2>&1; then
  echo "GitHub CLI needs one-time authorization."
  gh auth login --hostname github.com --git-protocol https --web
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
keystore="$tmp/scraperx-dashboard.p12"
password="$(od -An -N24 -tx1 /dev/urandom | tr -d ' \n')"

keytool -genkeypair -noprompt \
  -storetype PKCS12 \
  -keystore "$keystore" \
  -storepass "$password" \
  -keypass "$password" \
  -alias "$ALIAS" \
  -keyalg RSA \
  -keysize 3072 \
  -validity 10000 \
  -dname "CN=ScraperX Development APK Dashboard,O=ScraperX,C=US"

base64 < "$keystore" | tr -d '\n' | \
  gh secret set SCRAPERX_APK_KEYSTORE_B64 --repo "$REPO"
printf '%s' "$password" | \
  gh secret set SCRAPERX_APK_KEYSTORE_PASSWORD --repo "$REPO"

fingerprint="$(keytool -list -v \
  -storetype PKCS12 \
  -keystore "$keystore" \
  -storepass "$password" \
  -alias "$ALIAS" 2>/dev/null | \
  awk -F': ' '/SHA256:/ {gsub(":","",$2); print tolower($2); exit}')"

echo
echo "Stable ScraperX dashboard signing key installed."
echo "Signer SHA-256: $fingerprint"
echo "Triggering dashboard republish..."
gh workflow run "ScraperX APK Dashboard Publisher" --repo "$REPO"
echo
echo "Done. The first stable-signed install for each model may require uninstalling its old ephemeral-signed copy once. Every later dashboard build will update in place."
