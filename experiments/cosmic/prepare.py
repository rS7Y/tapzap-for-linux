#!/usr/bin/env python3
"""Fetch byte-pinned sources and apply the experimental compositor patch."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

HERE = Path(__file__).resolve().parent


def unpack(archive, target):
    target.mkdir(parents=True, exist_ok=False)
    with tarfile.open(archive) as bundle:
        for member in bundle:
            relative = Path(*Path(member.name).parts[1:])
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError("Unsafe source archive path")
            destination = target / relative
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                destination.parent.mkdir(parents=True, exist_ok=True)
                with bundle.extractfile(member) as src, destination.open("wb") as dst:
                    shutil.copyfileobj(src, dst)
                destination.chmod(member.mode & 0o777)
            else:
                raise ValueError(f"Unsupported archive member: {member.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--archives", type=Path)
    args = parser.parse_args()
    lock = json.loads((HERE / "sources.json").read_text())
    args.output.mkdir(parents=True, exist_ok=True)
    for name, source in lock.items():
        archive = (args.archives or args.output) / f"{name}-pinned.tar.gz"
        if not archive.exists():
            urllib.request.urlretrieve(source["archive_url"], archive)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest != source["archive_sha256"]:
            raise ValueError(f"Source checksum mismatch for {name}: {digest}")
        unpack(archive, args.output / name)
    cosmic = args.output / "cosmic"
    subprocess.run(["patch", "-p1", "--batch", "-i", str(HERE / "cosmic-comp.patch")],
                   cwd=cosmic, check=True)
    module = cosmic / "src/tapzap"
    module.mkdir()
    shutil.copy2(HERE / "tapzap.rs", module / "mod.rs")
    shutil.copy2(HERE / "control.rs", module / "control.rs")
    print(json.dumps(lock, indent=2))


if __name__ == "__main__":
    main()
