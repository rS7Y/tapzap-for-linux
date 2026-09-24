#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")"
trial=0.1.6
out="dist/tapzap-omarchy-$trial-linux-x86_64"
[[ $(uname -m) == x86_64 ]] || { echo 'This trial archive targets x86_64.' >&2; exit 1; }
repo_root="$(git rev-parse --show-toplevel)"
[[ -z $(git -C "$repo_root" status --porcelain --untracked-files=all) ]] || {
  echo 'Refusing to package a dirty source tree; commit or remove generated files first.' >&2
  exit 1
}
[[ ! -e $out ]] || { echo "Archive staging already exists: $out" >&2; exit 1; }
bash build.sh
bash test.sh
source_revision="$(git -C "$repo_root" rev-parse HEAD)"
mkdir -p "$out/payload/bin" "$out/integration"
install -m755 build/tapzap build/tapzap-ctm build/tapzap-tray "$out/payload/bin/"
install -m755 integration/tapzap-omarchy "$out/payload/"
install -m644 integration/outside-click.lua "$out/payload/"
cp -R src/linux/assets "$out/payload/"
install -m644 integration/tapzap-omarchy.desktop integration/tapzap-omarchy.svg "$out/integration/"
install -m755 install.py "$out/"
install -m644 README.md "$out/"
install -m644 "$repo_root/LICENSE" "$out/"
install -m644 "$repo_root/SECURITY.md" "$out/"
cp -R "$repo_root/THIRD-PARTY-NOTICES" "$out/"
mkdir -p "$out/docs"
for doc in BUILDING INSTALLING KNOWN-ISSUES SUPPORT-MATRIX TROUBLESHOOTING VALIDATION; do
  install -m644 "$repo_root/docs/$doc.md" "$out/docs/"
done
SOURCE_REVISION="$source_revision" python3 - "$repo_root" "$out/BUILD-INFO.json" <<'PY'
import hashlib,json,os,sys
from pathlib import Path
root=Path(sys.argv[1]); output=Path(sys.argv[2])
manifest=root/'docs/SOURCE-MANIFEST.json'
info={'source_repository':'https://github.com/rS7Y/tapzap-linux',
      'source_revision':os.environ['SOURCE_REVISION'],
      'source_manifest_sha256':hashlib.sha256(manifest.read_bytes()).hexdigest(),
      'architecture':'x86_64','status':'experimental; not a general Linux release',
      'build_command':'cd src/omarchy-hyprland && bash package.sh',
      'checks':['bash build.sh','bash test.sh','tests/package.py'],
      'license':'GPL-3.0-or-later','open_issues':'docs/KNOWN-ISSUES.md'}
output.write_text(json.dumps(info,indent=2)+'\n')
PY
python3 - "$out" <<'PY'
import hashlib,json,sys
from pathlib import Path
p=Path(sys.argv[1]); files={str(f.relative_to(p)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(p.rglob('*')) if f.is_file()}
(p/'SHA256SUMS.json').write_text(json.dumps(files,indent=2)+'\n')
PY
tar -C dist -czf "$out.tar.gz" "$(basename "$out")"
sha256sum "$out.tar.gz" > "$out.tar.gz.sha256"
cat "$out.tar.gz.sha256"
python3 tests/package.py "$out.tar.gz"
