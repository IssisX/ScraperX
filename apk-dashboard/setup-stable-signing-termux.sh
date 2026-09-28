#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

REPO="IssisX/ScraperX"
ALIAS="scraperx-dashboard"

if command -v pkg >/dev/null 2>&1; then
  command -v gh >/dev/null 2>&1 || pkg install -y gh
  command -v openssl >/dev/null 2>&1 || pkg install -y openssl
  command -v base64 >/dev/null 2>&1 || pkg install -y coreutils
fi

for cmd in gh openssl base64; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing $cmd. Install GitHub CLI, OpenSSL, and coreutils, then rerun." >&2
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

private_key="$tmp/private-key.pem"
certificate="$tmp/certificate.pem"

openssl req -x509 -newkey rsa:3072 -sha256 -nodes \
  -keyout "$private_key" \
  -out "$certificate" \
  -days 10000 \
  -subj "/CN=ScraperX Development APK Dashboard/O=ScraperX/C=US"

openssl pkcs12 -export \
  -out "$keystore" \
  -inkey "$private_key" \
  -in "$certificate" \
  -name "$ALIAS" \
  -passout "pass:$password"

base64 < "$keystore" | tr -d '\n' | \
  gh secret set SCRAPERX_APK_KEYSTORE_B64 --repo "$REPO"
printf '%s' "$password" | \
  gh secret set SCRAPERX_APK_KEYSTORE_PASSWORD --repo "$REPO"

fingerprint="$(openssl x509 -in "$certificate" -noout -fingerprint -sha256 | \
  cut -d= -f2 | tr -d ':' | tr '[:upper:]' '[:lower:]')"

echo
echo "Stable ScraperX dashboard signing key installed."
echo "Signer SHA-256: $fingerprint"
echo "Triggering dashboard republish..."
gh workflow run "ScraperX APK Dashboard Publisher" --repo "$REPO"
echo
echo "Done. The first stable-signed install for each model may require uninstalling its old ephemeral-signed copy once. Every later dashboard build will update in place."
