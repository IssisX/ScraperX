#!/usr/bin/env bash
# The Claude branch's APK identity, read back from a built APK.
#
# Every APK this branch produces is ScraperX-Claude: its own launcher label
# and its own application ID, so it installs beside the builds of the other
# AI branches (main, ChatGPT and ScraperX-Grok ship com.cory.scraperx,
# Gemini ships com.cory.scraperx.gemini). The identity is set in
# godot/export_presets.cfg; this check reads it back out of the APK that was
# actually exported. The requirement is permanent for this branch.
#
# Android refuses to install a second app when it shares the first one's
# package name, a content-provider authority, a declared permission (or
# permission group / tree) name, or a sharedUserId. So besides the package
# and the label, every authority and every declared permission must sit
# inside this app's own package namespace, and no shared user id may be set.
#
# Usage: check_apk_identity.sh APK
# aapt2 is taken from $AAPT2, else the newest $ANDROID_HOME/build-tools.
set -euo pipefail

readonly EXPECTED_PACKAGE="com.cory.scraperx.claude"
readonly EXPECTED_LABEL="ScraperX-Claude"

apk="${1:?usage: check_apk_identity.sh APK}"
test -s "${apk}" || { echo "check_apk_identity: no APK at ${apk}" >&2; exit 2; }

aapt2="${AAPT2:-}"
if [[ -z "${aapt2}" ]]; then
  aapt2="$(find "${ANDROID_HOME:?ANDROID_HOME or AAPT2 must be set}/build-tools" \
    -maxdepth 2 -type f -name aapt2 | sort -V | tail -n 1)"
fi
test -x "${aapt2}" || { echo "check_apk_identity: aapt2 not found" >&2; exit 2; }

failures=()

badging="$("${aapt2}" dump badging "${apk}" 2>/dev/null)"
package="$(sed -n "s/^package: name='\([^']*\)'.*/\1/p" <<<"${badging}")"
[[ "${package}" == "${EXPECTED_PACKAGE}" ]] ||
  failures+=("package is '${package}', expected '${EXPECTED_PACKAGE}'")

# The default label and each localized one: the launcher shows whichever
# matches the device's language.
labels="$(sed -n "s/^application-label[^:]*:'\(.*\)'$/\1/p" <<<"${badging}")"
[[ -n "${labels}" ]] || failures+=("no application-label in the APK")
wrong_labels="$(grep -v -x -F "${EXPECTED_LABEL}" <<<"${labels}" | sort -u || true)"
[[ -z "${wrong_labels}" ]] ||
  failures+=("labels other than '${EXPECTED_LABEL}': $(tr '\n' ' ' <<<"${wrong_labels}")")
label_count="$(grep -c . <<<"${labels}" || true)"

# The manifest as element / attribute / string value, one per line.
attributes="$("${aapt2}" dump xmltree --file AndroidManifest.xml "${apk}" 2>/dev/null | awk '
  /^[ ]*E: / { line = $0; sub(/^[ ]*E: /, "", line); split(line, part, " "); element = part[1]; next }
  /^[ ]*A: / {
    line = $0; sub(/^[ ]*A: /, "", line)
    at = index(line, "=\"")
    if (at == 0) next
    key = substr(line, 1, at - 1)
    sub(/\(0x[0-9a-fA-F]+\)$/, "", key)
    count = split(key, piece, ":")
    rest = substr(line, at + 2)
    printf "%s\t%s\t%s\n", element, piece[count], substr(rest, 1, index(rest, "\"") - 1)
  }')"
[[ -n "${attributes}" ]] || failures+=("could not read AndroidManifest.xml")

manifest_package="$(awk -F'\t' '$1 == "manifest" && $2 == "package" { print $3 }' <<<"${attributes}")"
[[ "${manifest_package}" == "${EXPECTED_PACKAGE}" ]] ||
  failures+=("manifest package is '${manifest_package}', expected '${EXPECTED_PACKAGE}'")

shared_user="$(awk -F'\t' '$2 == "sharedUserId" { print $3 }' <<<"${attributes}")"
[[ -z "${shared_user}" ]] || failures+=("sharedUserId '${shared_user}' is set")

authorities="$(awk -F'\t' '$2 == "authorities" { print $3 }' <<<"${attributes}" | tr ';' '\n' | sed '/^$/d')"
while IFS= read -r authority; do
  [[ -z "${authority}" || "${authority}" == "${EXPECTED_PACKAGE}."* ]] ||
    failures+=("provider authority '${authority}' is outside ${EXPECTED_PACKAGE}")
done <<<"${authorities}"

declared="$(awk -F'\t' '($1 == "permission" || $1 == "permission-group" || $1 == "permission-tree") && $2 == "name" { print $3 }' <<<"${attributes}")"
while IFS= read -r permission; do
  [[ -z "${permission}" || "${permission}" == "${EXPECTED_PACKAGE}."* ]] ||
    failures+=("declared permission '${permission}' is outside ${EXPECTED_PACKAGE}")
done <<<"${declared}"

if (( ${#failures[@]} > 0 )); then
  printf 'SCRAPERX_APK_IDENTITY FAIL %s\n' "${apk}"
  printf '  - %s\n' "${failures[@]}"
  exit 1
fi
printf 'SCRAPERX_APK_IDENTITY PASS package=%s label=%s locales=%s authorities=%s declared_permissions=%s shared_user=none apk=%s\n' \
  "${package}" "${EXPECTED_LABEL}" "${label_count}" \
  "$(tr '\n' ',' <<<"${authorities}" | sed 's/,$//')" \
  "$(grep -c . <<<"${declared}" || true)" "$(basename "${apk}")"
