#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

REPO="IssisX/ScraperX"
ALIAS="scraperx-dashboard"

# Re-running setup must never replace an installed application's identity.
# GitHub secrets cannot be read back; keep the original key locally too.
SIGNING_DIR="${SCRAPERX_SIGNING_DIR:-${HOME}/.local/share/scraperx-dashboard-signing}"
umask 077

if command -v pkg >/dev/null 2>&1; then
  command -v gh >/dev/null 2>&1 || pkg install -y gh
  command -v openssl >/dev/null 2>&1 || pkg install -y openssl
  command -v base64 >/dev/null 2>&1 || pkg install -y coreutils
  command -v jq >/dev/null 2>&1 || pkg install -y jq
fi

for cmd in gh openssl base64 jq; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing $cmd. Install GitHub CLI, OpenSSL, jq, and coreutils, then rerun." >&2
    exit 1
  fi
done

if ! gh auth status --hostname github.com >/dev/null 2>&1; then
  echo "GitHub CLI needs one-time authorization."
  gh auth login --hostname github.com --git-protocol https --web
fi

if [[ $# -ne 0 && !( $# -eq 3 && "$1" = "--restore" ) ]]; then
  echo "Usage: $0 [--restore original.p12 password-file]" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# A failed lookup must not be interpreted as permission to create a new key.
gh secret list --repo "$REPO" --json name > "$tmp/secrets.json"
secret_count="$(jq '[.[] | select(.name == "SCRAPERX_APK_KEYSTORE_B64" or .name == "SCRAPERX_APK_KEYSTORE_PASSWORD")] | length' "$tmp/secrets.json")"
published_cert=""
gh api "repos/${REPO}/releases?per_page=100" > "$tmp/releases.json"
if jq -e '.[] | select(.tag_name == "apk-latest")' "$tmp/releases.json" >/dev/null; then
  gh release download apk-latest --repo "$REPO" --pattern manifest.json --dir "$tmp"
  published_cert="$(jq -er '.signing_certificate_sha256 | select(test("^[0-9a-f]{64}$"))' "$tmp/manifest.json")"
fi

if [[ $# -eq 3 ]]; then
  # Restore only a key whose certificate matches the published APK identity.
  # The password file avoids secrets in argv, shell history, or terminal output.
  keystore="$2"
  password_file="$3"
  test -s "$keystore"
  test -s "$password_file"
else
  keystore="$SIGNING_DIR/scraperx-dashboard.p12"
  password_file="$SIGNING_DIR/password"
  if [[ ! -s "$keystore" || ! -s "$password_file" ]]; then
    if [[ "$secret_count" -gt 0 || -n "$published_cert" ]]; then
      echo "Refusing to generate a replacement key: signing is already provisioned." >&2
      echo "Restore the original .p12 and password file with --restore. If the original key is lost, a certificate migration requires an explicit reinstall decision." >&2
      exit 1
    fi
    if [[ -e "$keystore" || -e "$password_file" ]]; then
      echo "Incomplete signing backup at $SIGNING_DIR; refusing to overwrite it." >&2
      exit 1
    fi
    mkdir -p "$SIGNING_DIR"
    chmod 700 "$SIGNING_DIR"
    od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "$tmp/password"
    openssl req -x509 -newkey rsa:3072 -sha256 -nodes \
      -keyout "$tmp/private-key.pem" \
      -out "$tmp/certificate.pem" \
      -days 10000 \
      -subj "/CN=ScraperX Development APK Dashboard/O=ScraperX/C=US"
    openssl pkcs12 -export \
      -out "$tmp/scraperx-dashboard.p12" \
      -inkey "$tmp/private-key.pem" \
      -in "$tmp/certificate.pem" \
      -name "$ALIAS" \
      -passout "file:$tmp/password"
    cp "$tmp/scraperx-dashboard.p12" "$keystore"
    cp "$tmp/password" "$password_file"
  fi
fi

openssl pkcs12 -in "$keystore" -passin "file:$password_file" \
  -clcerts -nokeys -out "$tmp/certificate.pem"
fingerprint="$(openssl x509 -in "$tmp/certificate.pem" -noout -fingerprint -sha256 | \
  cut -d= -f2 | tr -d ':' | tr '[:upper:]' '[:lower:]')"
if [[ -n "$published_cert" && "$fingerprint" != "$published_cert" ]]; then
  echo "Refusing to replace published signer $published_cert with $fingerprint." >&2
  echo "No secrets changed. Use the original signing backup." >&2
  exit 1
fi

base64 < "$keystore" | tr -d '\n' | \
  gh secret set SCRAPERX_APK_KEYSTORE_B64 --repo "$REPO"
gh secret set SCRAPERX_APK_KEYSTORE_PASSWORD --repo "$REPO" < "$password_file"

echo
echo "Stable ScraperX dashboard signing key installed."
echo "Signer SHA-256: $fingerprint"
echo "Keep the keystore and password file together in a secure backup."
echo "Triggering dashboard republish..."
gh workflow run "ScraperX APK Dashboard Publisher" --repo "$REPO"
echo
echo "Done. The first stable-signed install for each model may require uninstalling its old ephemeral-signed copy once. Every later dashboard build will update in place."
