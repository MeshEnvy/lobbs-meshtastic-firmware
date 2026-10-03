#!/usr/bin/env bash
# LoBBS source release tags (no binaries). Dry-run by default; pass --create for an annotated tag.
#
# Tag shape:
#   lobbs-{lobbsSemVer}.{lobbsSha7}-meshtastic-{mtSemVer}.{mtSha7}
#
# Example:
#   lobbs-1.3.0.f18d6d6-meshtastic-2.7.26.54e0d8d

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CREATE=0
COMMIT=HEAD
LOBBS_VERSION=""
MT_PIN_SHA="${MT_PIN_SHA:-54e0d8d}"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --create              Create an annotated git tag (default: print tag only)
  --commit REF          Tag this commit (default: HEAD)
  --lobbs-version SEM   LoBBS semver when version.properties has no [LOBBS] section
  --mt-pin-sha SHA7     Meshtastic upstream pin (default: ${MT_PIN_SHA} or MT_PIN_SHA env)

Environment:
  MT_PIN_SHA            Same as --mt-pin-sha

Examples:
  $(basename "$0") --commit f18d6d6b0 --lobbs-version 1.3.0
  $(basename "$0") --create --commit HEAD
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --create) CREATE=1; shift ;;
    --commit) COMMIT="$2"; shift 2 ;;
    --lobbs-version) LOBBS_VERSION="$2"; shift 2 ;;
    --mt-pin-sha) MT_PIN_SHA="$2"; shift 2 ;;
    -h | --help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

if ! git rev-parse --verify "${COMMIT}^{commit}" >/dev/null 2>&1; then
  echo "Not a commit: ${COMMIT}" >&2
  exit 1
fi

LOBBS_SHA7="$(git rev-parse --short=7 "${COMMIT}")"

props="$(git show "${COMMIT}:version.properties" 2>/dev/null || true)"
if [[ -z "$props" ]]; then
  echo "No version.properties at ${COMMIT}" >&2
  exit 1
fi

read_props_section() {
  local section="$1"
  awk -v sec="$section" '
    $0 ~ ("^\\[" sec "\\]") { in_sec=1; next }
    /^\[/ { in_sec=0 }
    in_sec && /^major[[:space:]]*=/ { gsub(/[^0-9]/, "", $0); major=$0 }
    in_sec && /^minor[[:space:]]*=/ { gsub(/[^0-9]/, "", $0); minor=$0 }
    in_sec && /^build[[:space:]]*=/ { gsub(/[^0-9]/, "", $0); build=$0 }
    END {
      if (major == "" || minor == "" || build == "") exit 1
      printf "%s.%s.%s", major, minor, build
    }
  ' <<<"$props"
}

MT_SEMVER="$(read_props_section VERSION)" || {
  echo "Could not parse [VERSION] in version.properties at ${COMMIT}" >&2
  exit 1
}

if [[ -z "$LOBBS_VERSION" ]]; then
  LOBBS_VERSION="$(read_props_section LOBBS 2>/dev/null)" || true
fi

if [[ -z "$LOBBS_VERSION" ]]; then
  echo "No [LOBBS] in version.properties at ${COMMIT}; pass --lobbs-version" >&2
  exit 1
fi

TAG="lobbs-${LOBBS_VERSION}.${LOBBS_SHA7}-meshtastic-${MT_SEMVER}.${MT_PIN_SHA}"

if [[ "$CREATE" -eq 0 ]]; then
  echo "$TAG"
  echo "(dry run; use --create to tag ${COMMIT})" >&2
  exit 0
fi

if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
  echo "Tag already exists: ${TAG}" >&2
  exit 1
fi

MSG="LoBBS ${LOBBS_VERSION} source tag at ${LOBBS_SHA7} (Meshtastic ${MT_SEMVER}, pin ${MT_PIN_SHA})"
git tag -a "$TAG" -m "$MSG" "$COMMIT"
echo "Created annotated tag ${TAG} on $(git rev-parse --short "$COMMIT")"
echo "Push with: git push origin ${TAG}"
