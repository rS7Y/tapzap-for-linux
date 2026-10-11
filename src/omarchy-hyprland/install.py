#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install, update, or remove the per-user TAP ZAP Omarchy add-on."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parent
RELEASES_API = "https://api.github.com/repos/rS7Y/tapzap-for-linux/releases?per_page=30"
BEGIN = "-- BEGIN TAP ZAP Omarchy outside-click integration"
END = "-- END TAP ZAP Omarchy outside-click integration"


def quote_desktop(value):
    if "\n" in value or "\r" in value:
        raise ValueError("A desktop-entry path cannot contain a newline")
    return (
        '"'
        + value.replace("\\", "\\\\\\\\")
        .replace('"', '\\\\"')
        .replace("`", "\\\\`")
        .replace("$", "\\\\$")
        .replace("%", "%%")
        + '"'
    )


def paths(prefix=None):
    if prefix:
        base = Path(prefix).resolve()
        return base / "data", base / "config", base / "bin"
    return (
        Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")),
        Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")),
        Path.home() / ".local/bin",
    )


def checked_write(path, data, app_root=None, owned=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_text() == data:
            return
        if not app_root or (not owned and str(app_root) not in path.read_text()):
            raise RuntimeError(f"Refusing to overwrite a different file: {path}")
        backup(path, app_root)
    path.write_text(data)


def backup(path, app_root):
    folder = app_root / "backups"
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, folder / f"{time.time_ns()}-{path.name}")


def hyprland_integration(config, app_root, remove=False):
    path = config / "hypr/hyprland.lua"
    if not path.is_file():
        return
    original = path.read_text()
    text = original
    if BEGIN in text or END in text:
        if text.count(BEGIN) != 1 or text.count(END) != 1 or text.index(END) < text.index(BEGIN):
            raise RuntimeError(f"Please check the existing TAP ZAP block in {path}")
        start = text.index(BEGIN)
        stop = text.index(END) + len(END)
        if text[stop : stop + 1] == "\n":
            stop += 1
        text = text[:start] + text[stop:]
    if not remove:
        # Resolve XDG paths once when installing; keep the stable current/ link.
        lua_path = json.dumps(str(app_root / "current/outside-click.lua"))
        block = f'{BEGIN}\ndo\n  local path = {lua_path}\n  local file = io.open(path, "r")\n  if file then file:close(); dofile(path) end\nend\n{END}\n'
        text = text.rstrip("\n") + "\n\n" + block
    else:
        text = text.rstrip("\n") + "\n"
    if text != original:
        backup(path, app_root)
        path.write_text(text)


def verify_package(package):
    manifest = json.loads((package / "SHA256SUMS.json").read_text())
    actual = {str(p.relative_to(package)) for p in package.rglob("*") if p.is_file()}
    if actual != set(manifest) | {"SHA256SUMS.json"}:
        raise RuntimeError("Archive verification failed: unexpected or missing files")
    for relative, expected in manifest.items():
        p = (package / relative).resolve()
        if not p.is_relative_to(package) or hashlib.sha256(p.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Archive verification failed: {relative}")


def install(prefix=None, package=ROOT):
    if platform.system() != "Linux" and not prefix:
        raise RuntimeError(
            "Install TAP ZAP on Linux; --prefix is available for isolated staging tests"
        )
    if platform.machine() not in ("x86_64", "AMD64") and not prefix:
        raise RuntimeError("This archive contains x86_64 binaries")
    package = package.resolve()
    verify_package(package)
    data, config, bin_dir = paths(prefix)
    version = hashlib.sha256((package / "SHA256SUMS.json").read_bytes()).hexdigest()[:16]
    app_root = data / "tapzap-omarchy-trial"
    release = app_root / "releases" / version
    files = {
        p.relative_to(package / "payload"): p
        for p in (package / "payload").rglob("*")
        if p.is_file()
    }
    for name in ("install.py", "BUILD-INFO.json"):
        files[Path("setup") / name] = package / name
    files[Path("LICENSE")] = package / "LICENSE"
    for name in ("integration", "THIRD-PARTY-NOTICES", "docs"):
        for p in (package / name).rglob("*"):
            if p.is_file():
                relative = p.relative_to(package)
                files[Path("setup") / relative if name == "integration" else relative] = p
    if release.exists():
        actual = {p.relative_to(release) for p in release.rglob("*") if p.is_file()}
        if actual != set(files) or any(
            (release / relative).is_symlink()
            or (release / relative).read_bytes() != source.read_bytes()
            for relative, source in files.items()
        ):
            raise RuntimeError(f"Existing add-on release differs: {release}")
    else:
        release.parent.mkdir(parents=True, exist_ok=True)
        staging = release.parent / f".install-{os.getpid()}"
        try:
            for relative, source in files.items():
                target = staging / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
            staging.rename(release)
        finally:
            if staging.exists():
                shutil.rmtree(staging)
    current = app_root / "current"
    if current.exists() and not current.is_symlink():
        raise RuntimeError(f"Refusing to replace {current}")
    launcher = bin_dir / "tapzap-omarchy"
    if launcher.exists() or launcher.is_symlink():
        if not launcher.is_symlink() or os.readlink(launcher) != str(current / "tapzap-omarchy"):
            raise RuntimeError(f"Another installation owns {launcher}")
    executable = str(current / "tapzap-omarchy")
    desktop = (package / "integration/tapzap-omarchy.desktop").read_text()
    desktop = desktop.replace("Exec=tapzap-omarchy ", f"Exec={quote_desktop(executable)} ")
    desktop = desktop.replace(
        "Icon=tapzap-omarchy", f"Icon={data}/icons/hicolor/scalable/apps/tapzap-omarchy.svg"
    )
    desktop_path = data / "applications/tapzap-omarchy.desktop"
    owned_desktop = desktop_path.is_file() and str(app_root) in desktop_path.read_text()
    checked_write(
        data / "icons/hicolor/scalable/apps/tapzap-omarchy.svg",
        (package / "integration/tapzap-omarchy.svg").read_text(),
        app_root,
        owned=owned_desktop,
    )
    checked_write(desktop_path, desktop, app_root)
    hyprland_integration(config, app_root)
    if not prefix and current.exists():
        subprocess.run([str(current / "bin/tapzap"), "quit"], timeout=8, capture_output=True)
    pending = app_root / f".current-{os.getpid()}"
    pending.symlink_to(release)
    pending.replace(current)
    bin_dir.mkdir(parents=True, exist_ok=True)
    if not launcher.is_symlink():
        launcher.symlink_to(current / "tapzap-omarchy")
    short = bin_dir / "tapzap"
    if not short.exists() and not short.is_symlink():
        short.symlink_to(current / "tapzap-omarchy")
    elif not short.is_symlink() or os.readlink(short) != str(current / "tapzap-omarchy"):
        print("An existing 'tapzap' command was preserved. Use tapzap-omarchy instead.")
    print("Installed TAP ZAP. Open it from the application launcher or run tapzap.")
    print("First use starts OFF. Start at login is optional in Settings.")
    print("Update: tapzap update    Remove: tapzap uninstall")
    reload_hyprland(prefix)


def reload_hyprland(prefix):
    if not prefix and os.environ.get("HYPRLAND_INSTANCE_SIGNATURE") and shutil.which("hyprctl"):
        subprocess.run(["hyprctl", "reload"], timeout=8, capture_output=True, check=True)


def uninstall(prefix=None):
    data, config, bin_dir = paths(prefix)
    app_root = data / "tapzap-omarchy-trial"
    exe = app_root / "current/tapzap-omarchy"
    if not prefix and exe.exists():
        subprocess.run([str(exe), "quit"], timeout=8, check=False, capture_output=True)
    for name in ("tapzap", "tapzap-omarchy"):
        launcher = bin_dir / name
        if launcher.is_symlink() and os.readlink(launcher) == str(exe):
            launcher.unlink()
    desktop = data / "applications/tapzap-omarchy.desktop"
    if desktop.exists() and str(app_root) in desktop.read_text():
        desktop.unlink()
    login = config / "autostart/tapzap-omarchy.desktop"
    if login.exists() and str(app_root) in login.read_text():
        login.unlink()
    icon = data / "icons/hicolor/scalable/apps/tapzap-omarchy.svg"
    if (
        icon.exists()
        and icon.read_bytes() == (ROOT / "integration/tapzap-omarchy.svg").read_bytes()
    ):
        icon.unlink()
    hyprland_integration(config, app_root, remove=True)
    reload_hyprland(prefix)
    print("Removed TAP ZAP commands, launcher, tray integration and login entry.")
    print("Preferences and previous versions remain available for reinstall or rollback.")


def install_archive(archive, prefix=None):
    archive = Path(archive).resolve()
    expected = Path(str(archive) + ".sha256").read_text().split()[0]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise RuntimeError("Archive verification failed: download checksum mismatch")
    with tempfile.TemporaryDirectory(prefix="tapzap-install-") as temporary:
        folder = Path(temporary)
        with tarfile.open(archive) as tar:
            members = tar.getmembers()
            if sum(m.size for m in members) > 256 * 1024 * 1024:
                raise RuntimeError("Archive verification failed: package is too large")
            for member in members:
                path = Path(member.name)
                if (
                    path.is_absolute()
                    or ".." in path.parts
                    or not (member.isfile() or member.isdir())
                ):
                    raise RuntimeError("Archive verification failed: unsafe package entry")
            tar.extractall(folder)
        packages = list(folder.iterdir())
        if len(packages) != 1 or not packages[0].is_dir():
            raise RuntimeError("Archive verification failed: expected one package directory")
        install(prefix, packages[0])


def download(url):
    if not url.startswith("https://"):
        raise RuntimeError("Downloads require HTTPS")
    request = urllib.request.Request(url, headers={"User-Agent": "TapZap-Omarchy-Installer"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


def install_download(prefix=None):
    releases = json.loads(download(RELEASES_API))
    for release in releases:
        if release.get("draft") or not release.get("tag_name", "").startswith("omarchy-v"):
            continue
        assets = {asset["name"]: asset["browser_download_url"] for asset in release["assets"]}
        archives = [
            name
            for name in assets
            if name.startswith("tapzap-omarchy-")
            and name.endswith("-linux-x86_64.tar.gz")
            and name + ".sha256" in assets
        ]
        if not archives:
            continue
        name = sorted(archives)[-1]
        print(f"Downloading TAP ZAP {release['tag_name']}…", flush=True)
        with tempfile.TemporaryDirectory(prefix="tapzap-download-") as temporary:
            archive = Path(temporary) / name
            archive.write_bytes(download(assets[name]))
            Path(str(archive) + ".sha256").write_bytes(download(assets[name + ".sha256"]))
            install_archive(archive, prefix)
        return
    raise RuntimeError(
        "No Omarchy installer release is published yet. Use a locally built archive."
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--prefix",
        help="Stage integration files in an isolated directory instead of installing for this user",
    )
    parser.add_argument("--uninstall", action="store_true")
    parser.add_argument("--version", action="store_true")
    source = parser.add_mutually_exclusive_group()
    source.add_argument(
        "--archive", type=Path, help="Install or update from a verified local archive"
    )
    source.add_argument(
        "--download", action="store_true", help="Install the latest Omarchy release"
    )
    args = parser.parse_args()
    try:
        if args.version:
            info = json.loads((ROOT / "BUILD-INFO.json").read_text())
            print(f"TAP ZAP {info.get('version', 'experimental')} ({info['source_revision'][:12]})")
        elif args.uninstall:
            uninstall(args.prefix)
        elif args.archive:
            install_archive(args.archive, args.prefix)
        elif args.download:
            install_download(args.prefix)
        else:
            install(args.prefix)
    except (
        OSError,
        ValueError,
        RuntimeError,
        tarfile.TarError,
        urllib.error.URLError,
        subprocess.SubprocessError,
    ) as error:
        print(f"TAP ZAP setup: {error}", file=sys.stderr)
        sys.exit(1)
