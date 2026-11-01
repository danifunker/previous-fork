#!/bin/sh
#
# setup-macos-signing.sh - store the macOS signing and notarization
# secrets used by .github/workflows/release.yml in GitHub repositories.
#
# Usage:
#   scripts/setup-macos-signing.sh --p12 DeveloperID.p12 [options]
#
# Options:
#   --p12 FILE         Developer ID Application certificate with its private
#                      key, exported from Keychain Access as .p12 (required)
#   --repo OWNER/NAME  repository to configure (repeatable; default: the
#                      repository of the current directory)
#   --env-file FILE    read values from a file with KEY=value lines (see below)
#   --skip-check       don't verify the notarization login with Apple
#   --dry-run          check everything, but don't store any secrets
#   -h, --help         show this help
#
# Values are taken from (first match wins): the environment, --env-file,
# then a silent prompt. Names:
#   MACOS_CERTIFICATE_PWD     password of the .p12 file
#   MACOS_NOTARIZE_APPLE_ID   Apple ID used for notarization
#   MACOS_NOTARIZE_PASSWORD   app-specific password for that Apple ID
#                             (appleid.apple.com > Sign-In and Security)
#   MACOS_TEAM_ID             optional, read from the certificate if unset
#
# The secrets stored are MACOS_CERTIFICATE (the .p12, base64),
# MACOS_CERTIFICATE_PWD, MACOS_NOTARIZE_APPLE_ID, MACOS_NOTARIZE_PASSWORD
# and MACOS_TEAM_ID. Values are passed to gh on stdin, never printed.
#
# Requires: gh (logged in), openssl; xcrun for the notarization check.

set -eu

die() { echo "error: $*" >&2; exit 1; }
usage() { awk 'NR > 2 && /^#/ { sub(/^# ?/, ""); print; next } NR > 2 { exit }' "$0"; exit "${1:-0}"; }

P12= ENVFILE= CHECK=1 DRYRUN=0 REPOS=
while [ $# -gt 0 ]; do
	case $1 in
		--p12)        P12=${2:?}; shift 2 ;;
		--repo)       REPOS="$REPOS ${2:?}"; shift 2 ;;
		--env-file)   ENVFILE=${2:?}; shift 2 ;;
		--skip-check) CHECK=0; shift ;;
		--dry-run)    DRYRUN=1; shift ;;
		-h|--help)    usage 0 ;;
		*)            echo "unknown option: $1" >&2; usage 1 ;;
	esac
done

[ -n "$P12" ] || die "--p12 is required (see --help)"
[ -f "$P12" ] || die "certificate file not found: $P12"
command -v gh >/dev/null || die "gh (GitHub CLI) is not installed"
command -v openssl >/dev/null || die "openssl is not installed"
gh auth status >/dev/null 2>&1 || die "gh is not logged in (run: gh auth login)"

if [ -z "$REPOS" ]; then
	REPOS=$(gh repo view --json nameWithOwner --jq .nameWithOwner 2>/dev/null) \
		|| die "not in a GitHub repository; use --repo OWNER/NAME"
fi

# value NAME: environment, then --env-file, else empty
value() {
	eval "v=\${$1:-}"
	if [ -z "$v" ] && [ -n "$ENVFILE" ]; then
		[ -f "$ENVFILE" ] || die "env file not found: $ENVFILE"
		v=$(sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$ENVFILE" | tail -n 1 |
		    sed -e 's/^"\(.*\)"$/\1/' -e "s/^'\(.*\)'$/\1/")
	fi
	printf '%s' "$v"
}

# ask NAME PROMPT: value() or a silent prompt
ask() {
	v=$(value "$1")
	if [ -z "$v" ]; then
		[ -t 0 ] || die "$1 is not set and there is no terminal to ask"
		printf '%s: ' "$2" >&2
		stty -echo; IFS= read -r v; stty echo; echo >&2
	fi
	[ -n "$v" ] || die "$1 must not be empty"
	printf '%s' "$v"
}

CERT_PWD=$(ask MACOS_CERTIFICATE_PWD "Password of $(basename "$P12")")

# The certificate must open with that password and be a Developer ID
# Application certificate. OpenSSL 3 needs -legacy for older .p12 exports.
SUBJECT=
for legacy in "" "-legacy"; do
	SUBJECT=$(printf '%s' "$CERT_PWD" | openssl pkcs12 $legacy -in "$P12" -passin stdin \
	          -nokeys -clcerts 2>/dev/null | openssl x509 -noout -subject 2>/dev/null) && break
done
[ -n "$SUBJECT" ] || die "can't open $P12 (wrong password, or not a .p12 file)"
case $SUBJECT in
	*"Developer ID Application"*) ;;
	*) die "not a Developer ID Application certificate: $SUBJECT" ;;
esac
CERT_TEAM=$(printf '%s\n' "$SUBJECT" | sed -n 's/.*OU *= *\([A-Z0-9]\{10\}\).*/\1/p')
echo "certificate: $(printf '%s\n' "$SUBJECT" | sed -n 's/.*CN *= *\([^,\/]*\).*/\1/p')"

TEAM_ID=$(value MACOS_TEAM_ID)
TEAM_ID=${TEAM_ID:-$CERT_TEAM}
[ -n "$TEAM_ID" ] || TEAM_ID=$(ask MACOS_TEAM_ID "Apple team ID")
[ -z "$CERT_TEAM" ] || [ "$TEAM_ID" = "$CERT_TEAM" ] ||
	die "MACOS_TEAM_ID ($TEAM_ID) doesn't match the certificate's team ($CERT_TEAM)"
echo "team ID: $TEAM_ID"

APPLE_ID=$(ask MACOS_NOTARIZE_APPLE_ID "Apple ID for notarization")
NOTARIZE_PWD=$(ask MACOS_NOTARIZE_PASSWORD "App-specific password for $APPLE_ID")

if [ $CHECK = 1 ]; then
	if command -v xcrun >/dev/null && xcrun --find notarytool >/dev/null 2>&1; then
		echo "checking the notarization login with Apple..."
		xcrun notarytool history --apple-id "$APPLE_ID" --password "$NOTARIZE_PWD" \
			--team-id "$TEAM_ID" >/dev/null 2>&1 ||
			die "Apple rejected the notarization login (Apple ID, app-specific password or team ID)"
		echo "notarization login OK"
	else
		echo "note: xcrun notarytool not available, notarization login not checked"
	fi
fi

if [ $DRYRUN = 1 ]; then
	echo "dry run: everything checks out, no secrets stored"
	exit 0
fi

for repo in $REPOS; do
	echo "storing secrets in $repo"
	base64 < "$P12" | tr -d '\n' | gh secret set MACOS_CERTIFICATE --repo "$repo"
	printf '%s' "$CERT_PWD"     | gh secret set MACOS_CERTIFICATE_PWD   --repo "$repo"
	printf '%s' "$APPLE_ID"     | gh secret set MACOS_NOTARIZE_APPLE_ID --repo "$repo"
	printf '%s' "$NOTARIZE_PWD" | gh secret set MACOS_NOTARIZE_PASSWORD --repo "$repo"
	printf '%s' "$TEAM_ID"      | gh secret set MACOS_TEAM_ID           --repo "$repo"
	gh secret list --repo "$repo" | grep '^MACOS_' || true
done
echo "done. The next release build signs and notarizes the macOS app."
