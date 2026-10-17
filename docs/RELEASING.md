# Publishing Linux releases

Omarchy and the Mint Cinnamon X11 candidate have source-matched public binary
releases. Their release processes and test scopes are separate. Other backend
snapshots retain their experimental status and older unmatched website ZIPs.

## Omarchy

1. Update the version in the root and packaged plugin manifests and in
   `src/omarchy-hyprland/package.sh`. Update the changelog and setup guide.
2. Run `make check` and review the complete diff. Commit to a task branch and
   verify its GitHub source/native-build checks.
3. On Linux x86_64 with the documented dependencies, build from a clean checkout
   of that exact commit:

   ```sh
   bash src/omarchy-hyprland/package.sh
   ```

4. The package runs engine/protocol, QML and isolated installer tests. Use an
   unlocked Omarchy desktop with `WAYLAND_DISPLAY` set to cover both native QML
   entry points. A skipped QML check is not a pass for a native release.
5. Verify the archive/checksum pair from its download directory using
   `sha256sum --check`. Install the archive on the test desktop; exercise Settings,
   all clock pickers, save/validation, a real scheduled start, manual hold, OFF,
   Quit and restart. Preserve and restore the operator's preferences. Record
   tested versions and remaining limits in [Validation](VALIDATION.md).
6. Fast-forward reviewed source into `main`. Tag that same commit
   `omarchy-v<VERSION>`. Create a GitHub release with the archive and adjacent
   checksum; include the exact source SHA, build command, dependencies, test
   results and experimental hardware limits in its notes.
7. Download the public assets again, compare hashes, and run the documented
   setup/update path. Do not replace a published archive silently; publish a
   new version for a changed payload.
8. If updating the root `install-omarchy.sh` bootstrap, pin `installer_commit`
   to the full, published source commit and `installer_sha256` to that commit's
   `src/omarchy-hyprland/install.py` SHA-256. Never fetch executable bootstrap
   code from a moving branch. Run the bootstrap tests and verify its `--help`
   path against the real download before publishing the shortcut change.

The repository-root manifest is for Omarchy's Git manager. The package includes
the same UI plus the native engine. Test both layouts and keep their plugin IDs
and versions identical. `tapzap uninstall` preserves a Git-managed checkout and
its edits; Omarchy's own Remove command deletes that checkout separately.

Marketplace submissions use the
[official submission guide](https://github.com/omacom/omarchy-plugin-marketplace/blob/main/SUBMISSION.md).
Mark the native engine as manual setup. Submission and automated checks are
separate from maintainer approval and marketplace publication.

## Mint Cinnamon X11 candidate

Mint 0.1.0 uses the exact archive built from `5e4d0b8` inside Mint 22.3
Cinnamon 6.6.4. See the [Mint guide](MINT.md) and [validation record](VALIDATION.md).
Its tag identifies the tested build commit; later documentation commits do
not change the archive's recorded source revision.

For a future candidate, build on Mint from a clean exact revision with
`make build VARIANT=x11`, run `make check`, and record compiler/libc versions.
Exercise the GUI, tray, warmth/red/dimming, OFF/Quit restoration and reopening
in the intended Mint session; run the disposable-desktop popover regression.
State clearly whether evidence is virtual or physical.

Include both native binaries, adjacent assets, exact source archive, licenses,
component notices, build metadata and reviewed test evidence. Hash every
payload file, then hash the completed archive. Create a separate
`mint-v<VERSION>` GitHub **prerelease** at the tested source commit and keep
the Omarchy release's latest status unchanged. Publish the same archive bytes
on the website, update both choosers and guides, and verify public download
hashes again. Do not offer the September terminal installer for this layout.
Never silently replace a published payload; a changed payload needs a new version.
