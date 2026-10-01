#!/usr/bin/env bash
# Create a signed release tag plus a signed checksum of the source archive.
# Usage: tools/release.sh vX.Y.Z
# Needs: clean tree, git signing configured (gpg or ssh: `git config gpg.format ssh`,
# `git config user.signingkey <key>`), ssh-keygen for the checksum signature.
set -euo pipefail

tag="${1:-}"
[[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "usage: $0 vX.Y.Z" >&2; exit 2; }

cd "$(git rev-parse --show-toplevel)"

[[ -z "$(git status --porcelain --untracked-files=no)" ]] || { echo "working tree not clean" >&2; exit 1; }
git rev-parse -q --verify "refs/tags/$tag" >/dev/null && { echo "tag $tag already exists" >&2; exit 1; }

fw="$(sed -n "s/.*FW_VERSION='\"\(v[0-9.]*\)\"'.*/\1/p" platformio.ini | head -1)"
[[ "$fw" == "$tag" ]] || { echo "FW_VERSION in platformio.ini ($fw) != $tag" >&2; exit 1; }

echo "== native tests"
pio test -e native

echo "== compile check"
pio run -e ld2450_release

echo "== signed tag"
git tag -s "$tag" -m "$tag"
git tag -v "$tag"

out="dist"; mkdir -p "$out"
archive="$out/ld2450-$tag.tar.gz"
git archive --format=tar.gz --prefix="ld2450-$tag/" -o "$archive" "$tag"
( cd "$out" && sha256sum "$(basename "$archive")" > SHA256SUMS )

if [[ "$(git config gpg.format || true)" == "ssh" ]]; then
  key="$(git config user.signingkey)"
  ssh-keygen -Y sign -f "$key" -n file "$out/SHA256SUMS"
  echo "signature: $out/SHA256SUMS.sig"
else
  gpg --armor --detach-sign "$out/SHA256SUMS"
  echo "signature: $out/SHA256SUMS.asc"
fi

echo "Done. Push with: git push origin $tag   (tag protection must be enabled on the remote)"
